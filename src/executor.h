#ifndef EXECUTOR_H
#define EXECUTOR_H

#include "state.h"
#include "primitives/transaction.h"

class CExecutor
{
public:
    CExecutor(CState &state, CTransaction &tx) : mState(state), mTx(tx) {};

    bool Execute();
private:
    CState mState;
    CTransaction mTx;
};

#endif // EXECUTOR_H
