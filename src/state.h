#ifndef STATE_H
#define STATE_H

#include <boost/filesystem.hpp>
#include "dbwrapper.h"
#include "triedb/triedb.h"
#include "key.h"

class CState
{
public:
    CState(const boost::filesystem::path& path);

    void reset();

    void SetRoot(const H256& root);
    bool IsAddressInUse(const CPubKey& address) const;
private:
    CDBWrapper *mDB = 0;
    CTrieDB<CDBWrapper> *mState = 0;
};

#endif // STATE_H
