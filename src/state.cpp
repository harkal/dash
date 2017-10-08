#include "state.h"
#include "executor.h"

CState::CState(CTrieDB<CDBWrapper>* statedb) : mStateTrie(statedb)
{

}

void CState::SetRoot(const H256& root)
{
    mStateTrie->SetRoot(root);
}

bool CState::IsAddressInUse(const CKeyID& address) const
{
    return mStateTrie->Contains(address);
}

CAccount CState::GetAccount(const CKeyID& address) const
{
    auto i = mAccountCache.find(address);
    if (i != mAccountCache.end()) {
        return i->second;
    }

    CAccount acc;
    H256 hash = mStateTrie->At(address.AsBytes());

    if(mStateTrie->GetValue(hash, acc)) {
        return acc;
    }

    return CAccount();
}

bool CState::SetAccount(const CAccount& account)
{

}

void CState::ApplyTransaction(const CTransaction& tx)
{
    CExecutor executor(*this, tx);

    if (executor.Execute()) {

    }
}

void CState::AdvaceState(const CBlock& block)
{
    for(auto tx : block.vtx) {
        ApplyTransaction(tx);
    }
}

void CState::commit()
{
    for ( auto const& i : mAccountCache ) {

        mStateTrie->Insert(i.first, i.second);
    }
}
