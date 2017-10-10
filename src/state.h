#ifndef STATE_H
#define STATE_H

#include <boost/filesystem.hpp>
#include "triedb/triedb.h"
#include "key.h"
#include "account.h"
#include "primitives/transaction.h"
#include "dbwrapper.h"

class CBlock;

class CState
{
public:
    CState() {}

    CState(const CTrieDB<CDBWrapper>& statedb);

    void SetRoot(const H256& root);
    bool IsAddressInUse(const CKeyID& address) const;

    CAccount GetAccount(const CKeyID& address) const;
    bool SetAccount(const CKeyID& address, const CAccount& account);

    void commit();

    void ApplyTransaction(const CTransaction& tx);
    void AdvaceState(const CBlock& block);
private:
    CTrieDB<CDBWrapper> mStateTrie;

    CAccountMap mAccountCache;
};

#endif // STATE_H
