#ifndef STATE_H
#define STATE_H

#include <boost/filesystem.hpp>
#include "dbwrapper.h"
#include "triedb/triedb.h"
#include "key.h"
#include "account.h"
#include "primitives/block.h"
#include "primitives/transaction.h"

class CState
{
public:
    CState(CTrieDB<CDBWrapper>* statedb);

    void SetRoot(const H256& root);
    bool IsAddressInUse(const CKeyID& address) const;

    CAccount GetAccount(const CKeyID& address) const;

    void commit();

    void ApplyTransaction(const CTransaction& tx);
    void AdvaceState(const CBlock& block);
private:
    CTrieDB<CDBWrapper> *mStateTrie = 0;

    CAccountMap mAccountCache;
};

#endif // STATE_H
