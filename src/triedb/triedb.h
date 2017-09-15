// Copyright (c) 2017 Harry Kalogirou (harkal@gmail.com)
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef TRIEDB_H
#define TRIEDB_H

#include "crypto/hash.h"
#include "exceptions.h"

#include "nibble.h"

extern const H256 EmptyTrieDBNode;

template <class DB>
class CTrieDB
{
public:
    CTrieDB(DB* db) : mDB(db) {}
    ~CTrieDB() {}

    Bytes node(H256 const& hash) const { return mDB->lookup(hash); }

    void setRoot(const H256& root)
    {
        mRoot = root;
    }

    bool isNull() const { return node(mRoot).size() == 0; }
    bool isEmpty() const { return mRoot == EmptyTrieDBNode && node(mRoot).size(); }

    const H256& root() const
    {
        if (node(mRoot).empty())
            BOOST_THROW_EXCEPTION(BadRoot(mRoot));
        return mRoot;
    }

    Bytes at(const Bytes& key) const;

protected:
    H256 mRoot;
    DB* mDB = nullptr;
};

template <class DB>
Bytes CTrieDB<DB>::atAux(const RLP& _here, CNibbleView key) const
{
    if (_here.isEmpty() || _here.isNull())
        // not found.
        return Bytes();
    unsigned itemCount = _here.itemCount();
    assert(_here.isList() && (itemCount == 2 || itemCount == 17));
    if (itemCount == 2)
    {
        auto k = keyOf(_here);
        if (key == k && isLeaf(_here))
            // reached leaf and it's us
            return _here[1].toString();
        else if (key.contains(k) && !isLeaf(_here))
            // not yet at leaf and it might yet be us. onwards...
            return atAux(_here[1].isList() ? _here[1] : RLP(node(_here[1].toHash<h256>())), key.mid(k.size()));
        else
            // not us.
            return Bytes();
    }
    else
    {
        if (key.size() == 0)
            return _here[16].toString();
        auto n = _here[key[0]];
        if (n.isEmpty())
            return Bytes();
        else
            return atAux(n.isList() ? n : RLP(node(n.toHash<h256>())), key.mid(1));
    }
}

#endif // TRIEDB_H
