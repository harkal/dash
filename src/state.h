#ifndef STATE_H
#define STATE_H

#include <boost/filesystem.hpp>
#include "dbwrapper.h"
#include "triedb/triedb.h"
#include "key.h"
#include "account.h"

class CState
{
public:
    CState(CTrieDB<CDBWrapper>* statedb);

    void SetRoot(const H256& root);
    bool IsAddressInUse(const CKeyID& address) const;

    CAccount GetAccount(const CKeyID& address) const;
private:
    CTrieDB<CDBWrapper> *mStateTrie = 0;
};

#endif // STATE_H
