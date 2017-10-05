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
