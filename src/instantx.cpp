// Copyright (c) 2014-2017 The Dash Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "activemasternode.h"
#include "instantx.h"
#include "key.h"
#include "validation.h"
#include "masternode-sync.h"
#include "masternodeman.h"
#include "messagesigner.h"
#include "net.h"
#include "protocol.h"
#include "spork.h"
#include "sync.h"
#include "txmempool.h"
#include "util.h"
#include "consensus/validation.h"

#include <boost/algorithm/string/replace.hpp>
#include <boost/thread.hpp>

extern CWallet* pwalletMain;
extern CTxMemPool mempool;

bool fEnableInstantSend = true;
int nInstantSendDepth = DEFAULT_INSTANTSEND_DEPTH;
int nCompleteTXLocks;

CInstantSend instantsend;

// Transaction Locks
//
// step 1) Some node announces intention to lock transaction inputs via "txlreg" message
// step 2) Top COutPointLock::SIGNATURES_TOTAL masternodes per each spent outpoint push "txvote" message
// step 3) Once there are COutPointLock::SIGNATURES_REQUIRED valid "txvote" messages per each spent outpoint
//         for a corresponding "txlreg" message, all outpoints from that tx are treated as locked

//
// CInstantSend
//

void CInstantSend::ProcessMessage(CNode* pfrom, std::string& strCommand, CDataStream& vRecv)
{
    if(fLiteMode) return; // disable all Ebakus specific functionality
    if(!sporkManager.IsSporkActive(SPORK_2_INSTANTSEND_ENABLED)) return;

    // Ignore any InstantSend messages until masternode list is synced
    if(!masternodeSync.IsMasternodeListSynced()) return;

    // NOTE: NetMsgType::TXLOCKREQUEST is handled via ProcessMessage() in main.cpp

    if (strCommand == NetMsgType::TXLOCKVOTE) // InstantSend Transaction Lock Consensus Votes
    {
        if(pfrom->nVersion < MIN_INSTANTSEND_PROTO_VERSION) return;

        CTxLockVote vote;
        vRecv >> vote;

        LOCK(cs_main);
#ifdef ENABLE_WALLET
        if (pwalletMain)
            LOCK(pwalletMain->cs_wallet);
#endif
        LOCK(cs_instantsend);


        H256 nVoteHash = vote.GetHash();

        if(mapTxLockVotes.count(nVoteHash)) return;
        mapTxLockVotes.insert(std::make_pair(nVoteHash, vote));

        ProcessTxLockVote(pfrom, vote);

        return;
    }
}

bool CInstantSend::ProcessTxLockRequest(const CTxLockRequest& txLockRequest)
{
    LOCK2(cs_main, cs_instantsend);

    H256 txHash = txLockRequest.GetHash();

    if(!CreateTxLockCandidate(txLockRequest)) {
        // smth is not right
        LogPrintf("CInstantSend::ProcessTxLockRequest -- CreateTxLockCandidate failed, txid=%s\n", txHash.ToString());
        return false;
    }
    LogPrintf("CInstantSend::ProcessTxLockRequest -- accepted, txid=%s\n", txHash.ToString());

    std::map<H256, CTxLockCandidate>::iterator itLockCandidate = mapTxLockCandidates.find(txHash);
    CTxLockCandidate& txLockCandidate = itLockCandidate->second;
    Vote(txLockCandidate);
    ProcessOrphanTxLockVotes();

    // Masternodes will sometimes propagate votes before the transaction is known to the client.
    // If this just happened - lock inputs, resolve conflicting locks, update transaction status
    // forcing external script notification.
    TryToFinalizeLockCandidate(txLockCandidate);

    return true;
}

bool CInstantSend::CreateTxLockCandidate(const CTxLockRequest& txLockRequest)
{
    // Normally we should require all outpoints to be unspent, but in case we are reprocessing
    // because of a lot of legit orphan votes we should also check already spent outpoints.
    H256 txHash = txLockRequest.GetHash();
    if(!txLockRequest.IsValid(!IsEnoughOrphanVotesForTx(txLockRequest))) return false;

    LOCK(cs_instantsend);

    std::map<H256, CTxLockCandidate>::iterator itLockCandidate = mapTxLockCandidates.find(txHash);
    if(itLockCandidate == mapTxLockCandidates.end()) {
        LogPrintf("CInstantSend::CreateTxLockCandidate -- new, txid=%s\n", txHash.ToString());

        CTxLockCandidate txLockCandidate(txLockRequest);
        // all inputs should already be checked by txLockRequest.IsValid() above, just use them now
        mapTxLockCandidates.insert(std::make_pair(txHash, txLockCandidate));
    } else {
        LogPrint("instantsend", "CInstantSend::CreateTxLockCandidate -- seen, txid=%s\n", txHash.ToString());
    }

    return true;
}

void CInstantSend::Vote(CTxLockCandidate& txLockCandidate)
{
    if(!fMasterNode) return;

    LOCK2(cs_main, cs_instantsend);

    H256 txHash = txLockCandidate.GetHash();
    // check if we need to vote on this candidate's outpoints,
    // it's possible that we need to vote for several of them
    std::map<COutPoint, COutPointLock>::iterator itOutpointLock = txLockCandidate.mapOutPointLocks.begin();
    while(itOutpointLock != txLockCandidate.mapOutPointLocks.end()) {

        int nPrevoutHeight = GetUTXOHeight(itOutpointLock->first);
        if(nPrevoutHeight == -1) {
            LogPrint("instantsend", "CInstantSend::Vote -- Failed to find UTXO %s\n", itOutpointLock->first.ToStringShort());
            return;
        }

        int nLockInputHeight = nPrevoutHeight + 4;

        int n = mnodeman.GetMasternodeRank(activeMasternode.vin, nLockInputHeight, MIN_INSTANTSEND_PROTO_VERSION);

        if(n == -1) {
            LogPrint("instantsend", "CInstantSend::Vote -- Can't calculate rank for masternode %s\n", activeMasternode.vin.prevout.ToStringShort());
            ++itOutpointLock;
            continue;
        }

        int nSignaturesTotal = COutPointLock::SIGNATURES_TOTAL;
        if(n > nSignaturesTotal) {
            LogPrint("instantsend", "CInstantSend::Vote -- Masternode not in the top %d (%d)\n", nSignaturesTotal, n);
            ++itOutpointLock;
            continue;
        }

        LogPrint("instantsend", "CInstantSend::Vote -- In the top %d (%d)\n", nSignaturesTotal, n);

        std::map<COutPoint, std::set<H256> >::iterator itVoted = mapVotedOutpoints.find(itOutpointLock->first);

        // Check to see if we already voted for this outpoint,
        // refuse to vote twice or to include the same outpoint in another tx
        bool fAlreadyVoted = false;
        if(itVoted != mapVotedOutpoints.end()) {
            BOOST_FOREACH(const H256& hash, itVoted->second) {
                std::map<H256, CTxLockCandidate>::iterator it2 = mapTxLockCandidates.find(hash);
                if(it2->second.HasMasternodeVoted(itOutpointLock->first, activeMasternode.vin.prevout)) {
                    // we already voted for this outpoint to be included either in the same tx or in a competing one,
                    // skip it anyway
                    fAlreadyVoted = true;
                    LogPrintf("CInstantSend::Vote -- WARNING: We already voted for this outpoint, skipping: txHash=%s, outpoint=%s\n",
                            txHash.ToString(), itOutpointLock->first.ToStringShort());
                    break;
                }
            }
        }
        if(fAlreadyVoted) {
            ++itOutpointLock;
            continue; // skip to the next outpoint
        }

        // we haven't voted for this outpoint yet, let's try to do this now
        CTxLockVote vote(txHash, itOutpointLock->first, activeMasternode.vin.prevout);

        if(!vote.Sign()) {
            LogPrintf("CInstantSend::Vote -- Failed to sign consensus vote\n");
            return;
        }
        if(!vote.CheckSignature()) {
            LogPrintf("CInstantSend::Vote -- Signature invalid\n");
            return;
        }

        // vote constructed sucessfully, let's store and relay it
        H256 nVoteHash = vote.GetHash();
        mapTxLockVotes.insert(std::make_pair(nVoteHash, vote));
        if(itOutpointLock->second.AddVote(vote)) {
            LogPrintf("CInstantSend::Vote -- Vote created successfully, relaying: txHash=%s, outpoint=%s, vote=%s\n",
                    txHash.ToString(), itOutpointLock->first.ToStringShort(), nVoteHash.ToString());

            if(itVoted == mapVotedOutpoints.end()) {
                std::set<H256> setHashes;
                setHashes.insert(txHash);
                mapVotedOutpoints.insert(std::make_pair(itOutpointLock->first, setHashes));
            } else {
                mapVotedOutpoints[itOutpointLock->first].insert(txHash);
                if(mapVotedOutpoints[itOutpointLock->first].size() > 1) {
                    // it's ok to continue, just warn user
                    LogPrintf("CInstantSend::Vote -- WARNING: Vote conflicts with some existing votes: txHash=%s, outpoint=%s, vote=%s\n",
                            txHash.ToString(), itOutpointLock->first.ToStringShort(), nVoteHash.ToString());
                }
            }

            vote.Relay();
        }

        ++itOutpointLock;
    }
}

//received a consensus vote
bool CInstantSend::ProcessTxLockVote(CNode* pfrom, CTxLockVote& vote)
{
    // cs_main, cs_wallet and cs_instantsend should be already locked
    AssertLockHeld(cs_main);
#ifdef ENABLE_WALLET
    if (pwalletMain)
        AssertLockHeld(pwalletMain->cs_wallet);
#endif
    AssertLockHeld(cs_instantsend);

    H256 txHash = vote.GetTxHash();

    if(!vote.IsValid(pfrom)) {
        // could be because of missing MN
        LogPrint("instantsend", "CInstantSend::ProcessTxLockVote -- Vote is invalid, txid=%s\n", txHash.ToString());
        return false;
    }

    // Masternodes will sometimes propagate votes before the transaction is known to the client,
    // will actually process only after the lock request itself has arrived

    std::map<H256, CTxLockCandidate>::iterator it = mapTxLockCandidates.find(txHash);
    if(it == mapTxLockCandidates.end()) {
        if(!mapTxLockVotesOrphan.count(vote.GetHash())) {
            mapTxLockVotesOrphan[vote.GetHash()] = vote;
            LogPrint("instantsend", "CInstantSend::ProcessTxLockVote -- Orphan vote: txid=%s  masternode=%s new\n",
                    txHash.ToString(), vote.GetMasternodeOutpoint().ToStringShort());
            bool fReprocess = true;
            std::map<H256, CTxLockRequest>::iterator itLockRequest = mapLockRequestAccepted.find(txHash);
            if(itLockRequest == mapLockRequestAccepted.end()) {
                itLockRequest = mapLockRequestRejected.find(txHash);
                if(itLockRequest == mapLockRequestRejected.end()) {
                    // still too early, wait for tx lock request
                    fReprocess = false;
                }
            }
            if(fReprocess && IsEnoughOrphanVotesForTx(itLockRequest->second)) {
                // We have enough votes for corresponding lock to complete,
                // tx lock request should already be received at this stage.
                LogPrint("instantsend", "CInstantSend::ProcessTxLockVote -- Found enough orphan votes, reprocessing Transaction Lock Request: txid=%s\n", txHash.ToString());
                ProcessTxLockRequest(itLockRequest->second);
                return true;
            }
        } else {
            LogPrint("instantsend", "CInstantSend::ProcessTxLockVote -- Orphan vote: txid=%s  masternode=%s seen\n",
                    txHash.ToString(), vote.GetMasternodeOutpoint().ToStringShort());
        }

        // This tracks those messages and allows only the same rate as of the rest of the network
        // TODO: make sure this works good enough for multi-quorum

        int nMasternodeOrphanExpireTime = GetTime() + 60*10; // keep time data for 10 minutes
        if(!mapMasternodeOrphanVotes.count(vote.GetMasternodeOutpoint())) {
            mapMasternodeOrphanVotes[vote.GetMasternodeOutpoint()] = nMasternodeOrphanExpireTime;
        } else {
            int64_t nPrevOrphanVote = mapMasternodeOrphanVotes[vote.GetMasternodeOutpoint()];
            if(nPrevOrphanVote > GetTime() && nPrevOrphanVote > GetAverageMasternodeOrphanVoteTime()) {
                LogPrint("instantsend", "CInstantSend::ProcessTxLockVote -- masternode is spamming orphan Transaction Lock Votes: txid=%s  masternode=%s\n",
                        txHash.ToString(), vote.GetMasternodeOutpoint().ToStringShort());
                // Misbehaving(pfrom->id, 1);
                return false;
            }
            // not spamming, refresh
            mapMasternodeOrphanVotes[vote.GetMasternodeOutpoint()] = nMasternodeOrphanExpireTime;
        }

        return true;
    }

    LogPrint("instantsend", "CInstantSend::ProcessTxLockVote -- Transaction Lock Vote, txid=%s\n", txHash.ToString());

    std::map<COutPoint, std::set<H256> >::iterator it1 = mapVotedOutpoints.find(vote.GetOutpoint());
    if(it1 != mapVotedOutpoints.end()) {
        BOOST_FOREACH(const H256& hash, it1->second) {
            if(hash != txHash) {
                // same outpoint was already voted to be locked by another tx lock request,
                // find out if the same mn voted on this outpoint before
                std::map<H256, CTxLockCandidate>::iterator it2 = mapTxLockCandidates.find(hash);
                if(it2->second.HasMasternodeVoted(vote.GetOutpoint(), vote.GetMasternodeOutpoint())) {
                    // yes, it did, refuse to accept a vote to include the same outpoint in another tx
                    // from the same masternode.
                    // TODO: apply pose ban score to this masternode?
                    // NOTE: if we decide to apply pose ban score here, this vote must be relayed further
                    // to let all other nodes know about this node's misbehaviour and let them apply
                    // pose ban score too.
                    LogPrintf("CInstantSend::ProcessTxLockVote -- masternode sent conflicting votes! %s\n", vote.GetMasternodeOutpoint().ToStringShort());
                    return false;
                }
            }
        }
        // we have votes by other masternodes only (so far), let's continue and see who will win
        it1->second.insert(txHash);
    } else {
        std::set<H256> setHashes;
        setHashes.insert(txHash);
        mapVotedOutpoints.insert(std::make_pair(vote.GetOutpoint(), setHashes));
    }

    CTxLockCandidate& txLockCandidate = it->second;

    if(!txLockCandidate.AddVote(vote)) {
        // this should never happen
        return false;
    }

    int nSignatures = txLockCandidate.CountVotes();
    int nSignaturesMax = txLockCandidate.txLockRequest.GetMaxSignatures();
    LogPrint("instantsend", "CInstantSend::ProcessTxLockVote -- Transaction Lock signatures count: %d/%d, vote hash=%s\n",
            nSignatures, nSignaturesMax, vote.GetHash().ToString());

    TryToFinalizeLockCandidate(txLockCandidate);

    vote.Relay();

    return true;
}

void CInstantSend::ProcessOrphanTxLockVotes()
{
    LOCK(cs_main);
#ifdef ENABLE_WALLET
    if (pwalletMain)
        LOCK(pwalletMain->cs_wallet);
#endif
    LOCK(cs_instantsend);

    std::map<H256, CTxLockVote>::iterator it = mapTxLockVotesOrphan.begin();
    while(it != mapTxLockVotesOrphan.end()) {
        if(ProcessTxLockVote(NULL, it->second)) {
            mapTxLockVotesOrphan.erase(it++);
        } else {
            ++it;
        }
    }
}

bool CInstantSend::IsEnoughOrphanVotesForTx(const CTxLockRequest& txLockRequest)
{

    return true;
}

bool CInstantSend::IsEnoughOrphanVotesForTxAndOutPoint(const H256& txHash, const COutPoint& outpoint)
{
    // Scan orphan votes to check if this outpoint has enough orphan votes to be locked in some tx.
    LOCK2(cs_main, cs_instantsend);
    int nCountVotes = 0;
    std::map<H256, CTxLockVote>::iterator it = mapTxLockVotesOrphan.begin();
    while(it != mapTxLockVotesOrphan.end()) {
        if(it->second.GetTxHash() == txHash && it->second.GetOutpoint() == outpoint) {
            nCountVotes++;
            if(nCountVotes >= COutPointLock::SIGNATURES_REQUIRED) {
                return true;
            }
        }
        ++it;
    }
    return false;
}

void CInstantSend::TryToFinalizeLockCandidate(const CTxLockCandidate& txLockCandidate)
{
    LOCK(cs_main);
#ifdef ENABLE_WALLET
    if (pwalletMain)
        LOCK(pwalletMain->cs_wallet);
#endif
    LOCK(cs_instantsend);

    H256 txHash = txLockCandidate.txLockRequest.GetHash();
    if(txLockCandidate.IsAllOutPointsReady() && !IsLockedInstantSendTransaction(txHash)) {
        // we have enough votes now
        LogPrint("instantsend", "CInstantSend::TryToFinalizeLockCandidate -- Transaction Lock is ready to complete, txid=%s\n", txHash.ToString());
        if(ResolveConflicts(txLockCandidate, Params().GetConsensus().nInstantSendKeepLock)) {
            LockTransactionInputs(txLockCandidate);
            UpdateLockedTransaction(txLockCandidate);
        }
    }
}

void CInstantSend::UpdateLockedTransaction(const CTxLockCandidate& txLockCandidate)
{
    // cs_wallet and cs_instantsend should be already locked
#ifdef ENABLE_WALLET
    if (pwalletMain)
        AssertLockHeld(pwalletMain->cs_wallet);
#endif
    AssertLockHeld(cs_instantsend);

    H256 txHash = txLockCandidate.GetHash();

    if(!IsLockedInstantSendTransaction(txHash)) return; // not a locked tx, do not update/notify

#ifdef ENABLE_WALLET
    if(pwalletMain && pwalletMain->UpdatedTransaction(txHash)) {
        // bumping this to update UI
        nCompleteTXLocks++;
        // notify an external script once threshold is reached
        std::string strCmd = GetArg("-instantsendnotify", "");
        if(!strCmd.empty()) {
            boost::replace_all(strCmd, "%s", txHash.GetHex());
            boost::thread t(runCommand, strCmd); // thread runs free
        }
    }
#endif

    GetMainSignals().NotifyTransactionLock(txLockCandidate.txLockRequest);

    LogPrint("instantsend", "CInstantSend::UpdateLockedTransaction -- done, txid=%s\n", txHash.ToString());
}

void CInstantSend::LockTransactionInputs(const CTxLockCandidate& txLockCandidate)
{
    LOCK(cs_instantsend);

    H256 txHash = txLockCandidate.GetHash();

    if(!txLockCandidate.IsAllOutPointsReady()) return;

    std::map<COutPoint, COutPointLock>::const_iterator it = txLockCandidate.mapOutPointLocks.begin();

    while(it != txLockCandidate.mapOutPointLocks.end()) {
        mapLockedOutpoints.insert(std::make_pair(it->first, txHash));
        ++it;
    }
    LogPrint("instantsend", "CInstantSend::LockTransactionInputs -- done, txid=%s\n", txHash.ToString());
}

bool CInstantSend::GetLockedOutPointTxHash(const COutPoint& outpoint, H256& hashRet)
{
    LOCK(cs_instantsend);
    std::map<COutPoint, H256>::iterator it = mapLockedOutpoints.find(outpoint);
    if(it == mapLockedOutpoints.end()) return false;
    hashRet = it->second;
    return true;
}

bool CInstantSend::ResolveConflicts(const CTxLockCandidate& txLockCandidate, int nMaxBlocks)
{
    if(nMaxBlocks < 1) return false;

    LOCK2(cs_main, cs_instantsend);

    H256 txHash = txLockCandidate.GetHash();

    // make sure the lock is ready
    if(!txLockCandidate.IsAllOutPointsReady()) return true; // not an error

    LOCK(mempool.cs); // protect mempool.mapNextTx, mempool.mapTx

    bool fMempoolConflict = false;

    if(fMempoolConflict) {
        std::list<CTransaction> removed;
        // remove every tx conflicting with current Transaction Lock Request
        mempool.removeConflicts(txLockCandidate.txLockRequest, removed);
        // and try to accept it in mempool again
        CValidationState state;
        bool fMissingInputs = false;
        if(!AcceptToMemoryPool(mempool, state, txLockCandidate.txLockRequest, true, &fMissingInputs)) {
            LogPrintf("CInstantSend::ResolveConflicts -- ERROR: Failed to accept completed Transaction Lock to mempool, txid=%s\n", txHash.ToString());
            return false;
        }
        LogPrintf("CInstantSend::ResolveConflicts -- Accepted completed Transaction Lock, txid=%s\n", txHash.ToString());
        return true;
    }
    // No conflicts were found so far, check to see if it was already included in block
    CTransaction txTmp;
    H256 hashBlock;
    if(GetTransaction(txHash, txTmp, Params().GetConsensus(), hashBlock, true) && hashBlock != H256()) {
        LogPrint("instantsend", "CInstantSend::ResolveConflicts -- Done, %s is included in block %s\n", txHash.ToString(), hashBlock.ToString());
        return true;
    }

    LogPrint("instantsend", "CInstantSend::ResolveConflicts -- Done, txid=%s\n", txHash.ToString());

    return true;
}

int64_t CInstantSend::GetAverageMasternodeOrphanVoteTime()
{
    LOCK(cs_instantsend);
    // NOTE: should never actually call this function when mapMasternodeOrphanVotes is empty
    if(mapMasternodeOrphanVotes.empty()) return 0;

    std::map<COutPoint, int64_t>::iterator it = mapMasternodeOrphanVotes.begin();
    int64_t total = 0;

    while(it != mapMasternodeOrphanVotes.end()) {
        total+= it->second;
        ++it;
    }

    return total / mapMasternodeOrphanVotes.size();
}

void CInstantSend::CheckAndRemove()
{
    if(!masternodeSync.IsMasternodeListSynced()) return;

    LOCK(cs_instantsend);

    std::map<H256, CTxLockCandidate>::iterator itLockCandidate = mapTxLockCandidates.begin();

    // remove expired candidates
    while(itLockCandidate != mapTxLockCandidates.end()) {
        CTxLockCandidate &txLockCandidate = itLockCandidate->second;
        H256 txHash = txLockCandidate.GetHash();
        if(txLockCandidate.IsExpired(nCachedBlockHeight)) {
            LogPrintf("CInstantSend::CheckAndRemove -- Removing expired Transaction Lock Candidate: txid=%s\n", txHash.ToString());
            std::map<COutPoint, COutPointLock>::iterator itOutpointLock = txLockCandidate.mapOutPointLocks.begin();
            while(itOutpointLock != txLockCandidate.mapOutPointLocks.end()) {
                mapLockedOutpoints.erase(itOutpointLock->first);
                mapVotedOutpoints.erase(itOutpointLock->first);
                ++itOutpointLock;
            }
            mapLockRequestAccepted.erase(txHash);
            mapLockRequestRejected.erase(txHash);
            mapTxLockCandidates.erase(itLockCandidate++);
        } else {
            ++itLockCandidate;
        }
    }

    // remove expired votes
    std::map<H256, CTxLockVote>::iterator itVote = mapTxLockVotes.begin();
    while(itVote != mapTxLockVotes.end()) {
        if(itVote->second.IsExpired(nCachedBlockHeight)) {
            LogPrint("instantsend", "CInstantSend::CheckAndRemove -- Removing expired vote: txid=%s  masternode=%s\n",
                    itVote->second.GetTxHash().ToString(), itVote->second.GetMasternodeOutpoint().ToStringShort());
            mapTxLockVotes.erase(itVote++);
        } else {
            ++itVote;
        }
    }

    // remove expired orphan votes
    std::map<H256, CTxLockVote>::iterator itOrphanVote = mapTxLockVotesOrphan.begin();
    while(itOrphanVote != mapTxLockVotesOrphan.end()) {
        if(GetTime() - itOrphanVote->second.GetTimeCreated() > ORPHAN_VOTE_SECONDS) {
            LogPrint("instantsend", "CInstantSend::CheckAndRemove -- Removing expired orphan vote: txid=%s  masternode=%s\n",
                    itOrphanVote->second.GetTxHash().ToString(), itOrphanVote->second.GetMasternodeOutpoint().ToStringShort());
            mapTxLockVotes.erase(itOrphanVote->first);
            mapTxLockVotesOrphan.erase(itOrphanVote++);
        } else {
            ++itOrphanVote;
        }
    }

    // remove expired masternode orphan votes (DOS protection)
    std::map<COutPoint, int64_t>::iterator itMasternodeOrphan = mapMasternodeOrphanVotes.begin();
    while(itMasternodeOrphan != mapMasternodeOrphanVotes.end()) {
        if(itMasternodeOrphan->second < GetTime()) {
            LogPrint("instantsend", "CInstantSend::CheckAndRemove -- Removing expired orphan masternode vote: masternode=%s\n",
                    itMasternodeOrphan->first.ToStringShort());
            mapMasternodeOrphanVotes.erase(itMasternodeOrphan++);
        } else {
            ++itMasternodeOrphan;
        }
    }
    LogPrintf("CInstantSend::CheckAndRemove -- %s\n", ToString());
}

bool CInstantSend::AlreadyHave(const H256& hash)
{
    LOCK(cs_instantsend);
    return mapLockRequestAccepted.count(hash) ||
            mapLockRequestRejected.count(hash) ||
            mapTxLockVotes.count(hash);
}

void CInstantSend::AcceptLockRequest(const CTxLockRequest& txLockRequest)
{
    LOCK(cs_instantsend);
    mapLockRequestAccepted.insert(make_pair(txLockRequest.GetHash(), txLockRequest));
}

void CInstantSend::RejectLockRequest(const CTxLockRequest& txLockRequest)
{
    LOCK(cs_instantsend);
    mapLockRequestRejected.insert(make_pair(txLockRequest.GetHash(), txLockRequest));
}

bool CInstantSend::HasTxLockRequest(const H256& txHash)
{
    CTxLockRequest txLockRequestTmp;
    return GetTxLockRequest(txHash, txLockRequestTmp);
}

bool CInstantSend::GetTxLockRequest(const H256& txHash, CTxLockRequest& txLockRequestRet)
{
    LOCK(cs_instantsend);

    std::map<H256, CTxLockCandidate>::iterator it = mapTxLockCandidates.find(txHash);
    if(it == mapTxLockCandidates.end()) return false;
    txLockRequestRet = it->second.txLockRequest;

    return true;
}

bool CInstantSend::GetTxLockVote(const H256& hash, CTxLockVote& txLockVoteRet)
{
    LOCK(cs_instantsend);

    std::map<H256, CTxLockVote>::iterator it = mapTxLockVotes.find(hash);
    if(it == mapTxLockVotes.end()) return false;
    txLockVoteRet = it->second;

    return true;
}

bool CInstantSend::IsInstantSendReadyToLock(const H256& txHash)
{
    if(!fEnableInstantSend || fLargeWorkForkFound || fLargeWorkInvalidChainFound ||
        !sporkManager.IsSporkActive(SPORK_2_INSTANTSEND_ENABLED)) return false;

    LOCK(cs_instantsend);
    // There must be a successfully verified lock request
    // and all outputs must be locked (i.e. have enough signatures)
    std::map<H256, CTxLockCandidate>::iterator it = mapTxLockCandidates.find(txHash);
    return it != mapTxLockCandidates.end() && it->second.IsAllOutPointsReady();
}

bool CInstantSend::IsLockedInstantSendTransaction(const H256& txHash)
{
    if(!fEnableInstantSend || fLargeWorkForkFound || fLargeWorkInvalidChainFound ||
        !sporkManager.IsSporkActive(SPORK_2_INSTANTSEND_ENABLED)) return false;

    LOCK(cs_instantsend);

    // there must be a lock candidate
    std::map<H256, CTxLockCandidate>::iterator itLockCandidate = mapTxLockCandidates.find(txHash);
    if(itLockCandidate == mapTxLockCandidates.end()) return false;

    // which should have outpoints
    if(itLockCandidate->second.mapOutPointLocks.empty()) return false;

    // and all of these outputs must be included in mapLockedOutpoints with correct hash
    std::map<COutPoint, COutPointLock>::iterator itOutpointLock = itLockCandidate->second.mapOutPointLocks.begin();
    while(itOutpointLock != itLockCandidate->second.mapOutPointLocks.end()) {
        H256 hashLocked;
        if(!GetLockedOutPointTxHash(itOutpointLock->first, hashLocked) || hashLocked != txHash) return false;
        ++itOutpointLock;
    }

    return true;
}

int CInstantSend::GetTransactionLockSignatures(const H256& txHash)
{
    if(!fEnableInstantSend) return -1;
    if(fLargeWorkForkFound || fLargeWorkInvalidChainFound) return -2;
    if(!sporkManager.IsSporkActive(SPORK_2_INSTANTSEND_ENABLED)) return -3;

    LOCK(cs_instantsend);

    std::map<H256, CTxLockCandidate>::iterator itLockCandidate = mapTxLockCandidates.find(txHash);
    if(itLockCandidate != mapTxLockCandidates.end()) {
        return itLockCandidate->second.CountVotes();
    }

    return -1;
}

int CInstantSend::GetConfirmations(const H256 &nTXHash)
{
    return IsLockedInstantSendTransaction(nTXHash) ? nInstantSendDepth : 0;
}

bool CInstantSend::IsTxLockRequestTimedOut(const H256& txHash)
{
    if(!fEnableInstantSend) return false;

    LOCK(cs_instantsend);

    std::map<H256, CTxLockCandidate>::iterator itLockCandidate = mapTxLockCandidates.find(txHash);
    if (itLockCandidate != mapTxLockCandidates.end()) {
        return !itLockCandidate->second.IsAllOutPointsReady() &&
                itLockCandidate->second.txLockRequest.IsTimedOut();
    }

    return false;
}

void CInstantSend::Relay(const H256& txHash)
{
    LOCK(cs_instantsend);

    std::map<H256, CTxLockCandidate>::const_iterator itLockCandidate = mapTxLockCandidates.find(txHash);
    if (itLockCandidate != mapTxLockCandidates.end()) {
        itLockCandidate->second.Relay();
    }
}

void CInstantSend::UpdatedBlockTip(const CBlockIndex *pindex)
{
    nCachedBlockHeight = pindex->nHeight;
}

void CInstantSend::SyncTransaction(const CTransaction& tx, const CBlock* pblock)
{
    // Update lock candidates and votes if corresponding tx confirmed
    // or went from confirmed to 0-confirmed or conflicted.

    if (tx.IsCoinBase()) return;

    LOCK2(cs_main, cs_instantsend);

    H256 txHash = tx.GetHash();

    // When tx is 0-confirmed or conflicted, pblock is NULL and nHeightNew should be set to -1
    CBlockIndex* pblockindex = NULL;
    if(pblock) {
        H256 blockHash = pblock->GetHash();
        BlockMap::iterator mi = mapBlockIndex.find(blockHash);
        if(mi == mapBlockIndex.end() || !mi->second) {
            // shouldn't happen
            LogPrint("instantsend", "CTxLockRequest::SyncTransaction -- Failed to find block %s\n", blockHash.ToString());
            return;
        }
        pblockindex = mi->second;
    }
    int nHeightNew = pblockindex ? pblockindex->nHeight : -1;

    LogPrint("instantsend", "CInstantSend::SyncTransaction -- txid=%s nHeightNew=%d\n", txHash.ToString(), nHeightNew);

    // Check lock candidates
    std::map<H256, CTxLockCandidate>::iterator itLockCandidate = mapTxLockCandidates.find(txHash);
    if(itLockCandidate != mapTxLockCandidates.end()) {
        LogPrint("instantsend", "CInstantSend::SyncTransaction -- txid=%s nHeightNew=%d lock candidate updated\n",
                txHash.ToString(), nHeightNew);
        itLockCandidate->second.SetConfirmedHeight(nHeightNew);
        // Loop through outpoint locks
        std::map<COutPoint, COutPointLock>::iterator itOutpointLock = itLockCandidate->second.mapOutPointLocks.begin();
        while(itOutpointLock != itLockCandidate->second.mapOutPointLocks.end()) {
            // Check corresponding lock votes
            std::vector<CTxLockVote> vVotes = itOutpointLock->second.GetVotes();
            std::vector<CTxLockVote>::iterator itVote = vVotes.begin();
            std::map<H256, CTxLockVote>::iterator it;
            while(itVote != vVotes.end()) {
                H256 nVoteHash = itVote->GetHash();
                LogPrint("instantsend", "CInstantSend::SyncTransaction -- txid=%s nHeightNew=%d vote %s updated\n",
                        txHash.ToString(), nHeightNew, nVoteHash.ToString());
                it = mapTxLockVotes.find(nVoteHash);
                if(it != mapTxLockVotes.end()) {
                    it->second.SetConfirmedHeight(nHeightNew);
                }
                ++itVote;
            }
            ++itOutpointLock;
        }
    }

    // check orphan votes
    std::map<H256, CTxLockVote>::iterator itOrphanVote = mapTxLockVotesOrphan.begin();
    while(itOrphanVote != mapTxLockVotesOrphan.end()) {
        if(itOrphanVote->second.GetTxHash() == txHash) {
            LogPrint("instantsend", "CInstantSend::SyncTransaction -- txid=%s nHeightNew=%d vote %s updated\n",
                    txHash.ToString(), nHeightNew, itOrphanVote->first.ToString());
            mapTxLockVotes[itOrphanVote->first].SetConfirmedHeight(nHeightNew);
        }
        ++itOrphanVote;
    }
}

std::string CInstantSend::ToString()
{
    LOCK(cs_instantsend);
    return strprintf("Lock Candidates: %llu, Votes %llu", mapTxLockCandidates.size(), mapTxLockVotes.size());
}

//
// CTxLockRequest
//

bool CTxLockRequest::IsValid(bool fRequireUnspent) const
{
    LOCK(cs_main);
    if(!CheckFinalTx(*this)) {
        LogPrint("instantsend", "CTxLockRequest::IsValid -- Transaction is not final: tx=%s", ToString());
        return false;
    }

    CAmount nValueIn = 0;
    CAmount nValueOut = 0;

    return true;
}

CAmount CTxLockRequest::GetMinFee() const
{
    CAmount nMinFee = MIN_FEE;
    return std::max(nMinFee, CAmount(1 * nMinFee));
}

int CTxLockRequest::GetMaxSignatures() const
{
    return 1 * COutPointLock::SIGNATURES_TOTAL;
}

bool CTxLockRequest::IsTimedOut() const
{
    return GetTime() - nTimeCreated > TIMEOUT_SECONDS;
}

//
// CTxLockVote
//

bool CTxLockVote::IsValid(CNode* pnode) const
{
    if(!mnodeman.Has(CTxIn(outpointMasternode))) {
        LogPrint("instantsend", "CTxLockVote::IsValid -- Unknown masternode %s\n", outpointMasternode.ToStringShort());
        mnodeman.AskForMN(pnode, CTxIn(outpointMasternode));
        return false;
    }

    int nPrevoutHeight = GetUTXOHeight(outpoint);
    if(nPrevoutHeight == -1) {
        LogPrint("instantsend", "CTxLockVote::IsValid -- Failed to find UTXO %s\n", outpoint.ToStringShort());
        // Validating utxo set is not enough, votes can arrive after outpoint was already spent,
        // if lock request was mined. We should process them too to count them later if they are legit.
        CTransaction txOutpointCreated;
        H256 nHashOutpointConfirmed;
        if(!GetTransaction(outpoint.hash, txOutpointCreated, Params().GetConsensus(), nHashOutpointConfirmed, true) || nHashOutpointConfirmed == H256()) {
            LogPrint("instantsend", "CTxLockVote::IsValid -- Failed to find outpoint %s\n", outpoint.ToStringShort());
            return false;
        }
        LOCK(cs_main);
        BlockMap::iterator mi = mapBlockIndex.find(nHashOutpointConfirmed);
        if(mi == mapBlockIndex.end() || !mi->second) {
            // not on this chain?
            LogPrint("instantsend", "CTxLockVote::IsValid -- Failed to find block %s for outpoint %s\n", nHashOutpointConfirmed.ToString(), outpoint.ToStringShort());
            return false;
        }
        nPrevoutHeight = mi->second->nHeight;
    }

    int nLockInputHeight = nPrevoutHeight + 4;

    int n = mnodeman.GetMasternodeRank(CTxIn(outpointMasternode), nLockInputHeight, MIN_INSTANTSEND_PROTO_VERSION);

    if(n == -1) {
        //can be caused by past versions trying to vote with an invalid protocol
        LogPrint("instantsend", "CTxLockVote::IsValid -- Can't calculate rank for masternode %s\n", outpointMasternode.ToStringShort());
        return false;
    }
    LogPrint("instantsend", "CTxLockVote::IsValid -- Masternode %s, rank=%d\n", outpointMasternode.ToStringShort(), n);

    int nSignaturesTotal = COutPointLock::SIGNATURES_TOTAL;
    if(n > nSignaturesTotal) {
        LogPrint("instantsend", "CTxLockVote::IsValid -- Masternode %s is not in the top %d (%d), vote hash=%s\n",
                outpointMasternode.ToStringShort(), nSignaturesTotal, n, GetHash().ToString());
        return false;
    }

    if(!CheckSignature()) {
        LogPrintf("CTxLockVote::IsValid -- Signature invalid\n");
        return false;
    }

    return true;
}

H256 CTxLockVote::GetHash() const
{
    CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
    ss << txHash;
    ss << outpoint;
    ss << outpointMasternode;
    return ss.GetHash();
}

bool CTxLockVote::CheckSignature() const
{
    std::string strError;
    std::string strMessage = txHash.ToString() + outpoint.ToStringShort();

    masternode_info_t infoMn = mnodeman.GetMasternodeInfo(CTxIn(outpointMasternode));

    if(!infoMn.fInfoValid) {
        LogPrintf("CTxLockVote::CheckSignature -- Unknown Masternode: masternode=%s\n", outpointMasternode.ToString());
        return false;
    }

    if(!CMessageSigner::VerifyMessage(infoMn.pubKeyMasternode, vchMasternodeSignature, strMessage, strError)) {
        LogPrintf("CTxLockVote::CheckSignature -- VerifyMessage() failed, error: %s\n", strError);
        return false;
    }

    return true;
}

bool CTxLockVote::Sign()
{
    std::string strError;
    std::string strMessage = txHash.ToString() + outpoint.ToStringShort();

    if(!CMessageSigner::SignMessage(strMessage, vchMasternodeSignature, activeMasternode.keyMasternode)) {
        LogPrintf("CTxLockVote::Sign -- SignMessage() failed\n");
        return false;
    }

    if(!CMessageSigner::VerifyMessage(activeMasternode.pubKeyMasternode, vchMasternodeSignature, strMessage, strError)) {
        LogPrintf("CTxLockVote::Sign -- VerifyMessage() failed, error: %s\n", strError);
        return false;
    }

    return true;
}

void CTxLockVote::Relay() const
{
    CInv inv(MSG_TXLOCK_VOTE, GetHash());
    g_connman->RelayInv(inv);
}

bool CTxLockVote::IsExpired(int nHeight) const
{
    // Locks and votes expire nInstantSendKeepLock blocks after the block corresponding tx was included into.
    return (nConfirmedHeight != -1) && (nHeight - nConfirmedHeight > Params().GetConsensus().nInstantSendKeepLock);
}

//
// COutPointLock
//

bool COutPointLock::AddVote(const CTxLockVote& vote)
{
    if(mapMasternodeVotes.count(vote.GetMasternodeOutpoint()))
        return false;
    mapMasternodeVotes.insert(std::make_pair(vote.GetMasternodeOutpoint(), vote));
    return true;
}

std::vector<CTxLockVote> COutPointLock::GetVotes() const
{
    std::vector<CTxLockVote> vRet;
    std::map<COutPoint, CTxLockVote>::const_iterator itVote = mapMasternodeVotes.begin();
    while(itVote != mapMasternodeVotes.end()) {
        vRet.push_back(itVote->second);
        ++itVote;
    }
    return vRet;
}

bool COutPointLock::HasMasternodeVoted(const COutPoint& outpointMasternodeIn) const
{
    return mapMasternodeVotes.count(outpointMasternodeIn);
}

void COutPointLock::Relay() const
{
    std::map<COutPoint, CTxLockVote>::const_iterator itVote = mapMasternodeVotes.begin();
    while(itVote != mapMasternodeVotes.end()) {
        itVote->second.Relay();
        ++itVote;
    }
}

//
// CTxLockCandidate
//

void CTxLockCandidate::AddOutPointLock(const COutPoint& outpoint)
{
    mapOutPointLocks.insert(make_pair(outpoint, COutPointLock(outpoint)));
}


bool CTxLockCandidate::AddVote(const CTxLockVote& vote)
{
    std::map<COutPoint, COutPointLock>::iterator it = mapOutPointLocks.find(vote.GetOutpoint());
    if(it == mapOutPointLocks.end()) return false;
    return it->second.AddVote(vote);
}

bool CTxLockCandidate::IsAllOutPointsReady() const
{
    if(mapOutPointLocks.empty()) return false;

    std::map<COutPoint, COutPointLock>::const_iterator it = mapOutPointLocks.begin();
    while(it != mapOutPointLocks.end()) {
        if(!it->second.IsReady()) return false;
        ++it;
    }
    return true;
}

bool CTxLockCandidate::HasMasternodeVoted(const COutPoint& outpointIn, const COutPoint& outpointMasternodeIn)
{
    std::map<COutPoint, COutPointLock>::iterator it = mapOutPointLocks.find(outpointIn);
    return it !=mapOutPointLocks.end() && it->second.HasMasternodeVoted(outpointMasternodeIn);
}

int CTxLockCandidate::CountVotes() const
{
    // Note: do NOT use vote count to figure out if tx is locked, use IsAllOutPointsReady() instead
    int nCountVotes = 0;
    std::map<COutPoint, COutPointLock>::const_iterator it = mapOutPointLocks.begin();
    while(it != mapOutPointLocks.end()) {
        nCountVotes += it->second.CountVotes();
        ++it;
    }
    return nCountVotes;
}

bool CTxLockCandidate::IsExpired(int nHeight) const
{
    // Locks and votes expire nInstantSendKeepLock blocks after the block corresponding tx was included into.
    return (nConfirmedHeight != -1) && (nHeight - nConfirmedHeight > Params().GetConsensus().nInstantSendKeepLock);
}

void CTxLockCandidate::Relay() const
{
    g_connman->RelayTransaction(txLockRequest);
    std::map<COutPoint, COutPointLock>::const_iterator itOutpointLock = mapOutPointLocks.begin();
    while(itOutpointLock != mapOutPointLocks.end()) {
        itOutpointLock->second.Relay();
        ++itOutpointLock;
    }
}
