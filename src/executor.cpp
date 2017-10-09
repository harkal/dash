#include "executor.h"

bool CExecutor::Execute()
{
    CAccount receiver = mState.GetAccount(mTx.mReceiver);

    CPubKey senderPubKey = mTx.GetSenderPubKey();
    if (!senderPubKey.IsValid()) {
        return false;
    }

    CAccount sender = mState.GetAccount(senderPubKey.GetID());

    // Check if sender has enough balance
    if (sender.GetBalance() < mTx.mAmount)
        return false;

    // Do the actual transfer
    receiver.AddBalance(mTx.mAmount);
    sender.SubBalance(mTx.mAmount);

    mState.SetAccount(mTx.mReceiver, receiver);
    mState.SetAccount(senderPubKey.GetID(), sender);

    return true;
}
