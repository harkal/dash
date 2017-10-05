#include "state.h"

CState::CState(const boost::filesystem::path& path) : mDB(path, 1 << 22), mState(&mDB)
{

}

void CState::reset()
{
    mState.init();
}

void CState::SetRoot(const H256& root)
{
    mState.SetRoot(root);
}

bool CState::IsAddressInUse(const CPubKey& address) const
{
    return mState.Contains(address);
}
