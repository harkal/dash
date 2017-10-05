#include "state.h"

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
    CAccount acc;
    H256 hash = mStateTrie->At(address.AsBytes());

    if(mStateTrie->GetValue(hash, acc)) {
        return acc;
    }

    return CAccount();
}
