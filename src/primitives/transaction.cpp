// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "primitives/transaction.h"
#include "key.h"
#include "hash.h"
#include "tinyformat.h"
#include "utilstrencodings.h"
#include "wallet/wallet.h"

std::string COutPoint::ToString() const
{
    return strprintf("COutPoint(%s, %u)", hash.ToString()/*.substr(0,10)*/, n);
}

std::string COutPoint::ToStringShort() const
{
    return strprintf("%s-%u", hash.ToString().substr(0,64), n);
}

CTxIn::CTxIn(COutPoint prevoutIn, CScript scriptSigIn, uint32_t nSequenceIn)
{
    prevout = prevoutIn;
    scriptSig = scriptSigIn;
    nSequence = nSequenceIn;
}

CTxIn::CTxIn(H256 hashPrevTx, uint32_t nOut, CScript scriptSigIn, uint32_t nSequenceIn)
{
    prevout = COutPoint(hashPrevTx, nOut);
    scriptSig = scriptSigIn;
    nSequence = nSequenceIn;
}

std::string CTxIn::ToString() const
{
    std::string str;
    str += "CTxIn(";
    str += prevout.ToString();
    if (prevout.IsNull())
        str += strprintf(", coinbase %s", HexStr(scriptSig));
    else
        str += strprintf(", scriptSig=%s", HexStr(scriptSig).substr(0, 24));
    if (nSequence != SEQUENCE_FINAL)
        str += strprintf(", nSequence=%u", nSequence);
    str += ")";
    return str;
}

CTxOut::CTxOut(const CAmount& nValueIn, CScript scriptPubKeyIn)
{
    nValue = nValueIn;
    scriptPubKey = scriptPubKeyIn;
    nRounds = -10;
}

H256 CTxOut::GetHash() const
{
    return SerializeHash(*this);
}

std::string CTxOut::ToString() const
{
    return strprintf("CTxOut(nValue=%d.%08d, scriptPubKey=%s)", nValue / COIN, nValue % COIN, HexStr(scriptPubKey).substr(0, 30));
}

CMutableTransaction::CMutableTransaction() : nVersion(CTransaction::CURRENT_VERSION), nLockTime(0), mAmount(0) {}
CMutableTransaction::CMutableTransaction(const CTransaction& tx) :
    nVersion(tx.nVersion),
    mAmount(tx.mAmount),
    mReceiver(tx.mReceiver),
    mData(tx.mData),
    nLockTime(tx.nLockTime)

{}

H256 CMutableTransaction::GetHash() const
{
    return SerializeHash(*this);
}

Bytes CMutableTransaction::GetSignature(const CKey &key) const {
    Bytes signature;
    key.SignCompact(GetHash(), signature);
    return signature;
}

void CMutableTransaction::Sign(const CKey &key)
{
    mSignature = GetSignature(key);
}

bool CMutableTransaction::VerifySignature(const Bytes& vchSig, const CPubKey &senderPubKey) const {
    H256 hash = GetHash();


    CPubKey sigPubKey;

    if(sigPubKey.RecoverCompact(hash, vchSig)) {
        if(sigPubKey.GetID() == senderPubKey.GetID()) {
            return true;
        }
    }

    return false;
}

std::string CMutableTransaction::ToString() const
{
    std::string str;
    str += strprintf("CMutableTransaction(hash=%s, ver=%d, nLockTime=%u)\n",
        GetHash().ToString().substr(0,10),
        nVersion,
        nLockTime);
    return str;
}

void CTransaction::UpdateHash() const
{
    H256 h = SerializeHash(*this);
    *const_cast<H256*>(&hash) = h;
}

CTransaction::CTransaction() : nVersion(CTransaction::CURRENT_VERSION), mAmount(0), nLockTime(0) { }

CTransaction::CTransaction(const CMutableTransaction &tx) :
    nVersion(tx.nVersion),
    mAmount(tx.mAmount),
    mReceiver(tx.mReceiver),
    mData(tx.mData),
    mSignature(tx.mSignature),
    nLockTime(tx.nLockTime)
{
    UpdateHash();
}

Bytes CTransaction::GetSignature(const CKey &key) const {
    Bytes signature;
    key.SignCompact(GetHash(), signature);
    return signature;
}

void CTransaction::Sign(const CKey &key)
{
    *const_cast<Bytes *>(&mSignature) = GetSignature(key);
}

CTransaction& CTransaction::operator=(const CTransaction &tx) {
    *const_cast<int*>(&nVersion) = tx.nVersion;
    *const_cast<unsigned int*>(&nLockTime) = tx.nLockTime;
    *const_cast<H256*>(&hash) = tx.hash;
    return *this;
}

CAmount CTransaction::GetValueOut() const
{
    CAmount nValueOut = 0;

    return nValueOut;
}

double CTransaction::ComputePriority(double dPriorityInputs, unsigned int nTxSize) const
{
    nTxSize = CalculateModifiedSize(nTxSize);
    if (nTxSize == 0) return 0.0;

    return dPriorityInputs / nTxSize;
}

unsigned int CTransaction::CalculateModifiedSize(unsigned int nTxSize) const
{
    return nTxSize;
}

std::string CTransaction::ToString() const
{
    std::string str;
    str += strprintf("CTransaction(hash=%s, ver=%d, nLockTime=%u)\n",
        GetHash().ToString().substr(0,10),
        nVersion,
        nLockTime);
    return str;
}

bool CTransaction::VerifySignature(const Bytes& vchSig, const CPubKey &senderPubKey) const {
    H256 hash = GetHash();

    CPubKey sigPubKey;

    if(sigPubKey.RecoverCompact(hash, vchSig)) {
        if(sigPubKey.GetID() == senderPubKey.GetID()) {
            return true;
        }
    }

    return false;
}

CPubKey CTransaction::GetSenderPubKey() const
{
    H256 hash = GetHash();

    CPubKey sigPubKey;

    if(sigPubKey.RecoverCompact(hash, mSignature)) {
        return sigPubKey;
    }

    return CPubKey();
}
