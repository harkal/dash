// Copyright (c) 2017 Harry Kalogirou (harkal@gmail.com)
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef TRIEDB_H
#define TRIEDB_H

#include "crypto/hash.h"
#include "exceptions.h"

#include "nibble.h"
#include "streams.h"

extern const H256 EmptyTrieDBNode;

template <class DB>
class CTrieDB
{
public:
    CTrieDB(DB* db) : mDB(db) {}
    ~CTrieDB() {}

    CTrieNode node(H256 const& hash) const {
        Bytes data = mDB->lookup(hash);
        CDataStream s(data, SER_NETWORK, 0);
        CTrieNode n;
        s >> n;
        return n;
    }

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

    CTrieNode mergeAt(CTrieNode const& orig, H256 const& origHash, CNibbleView k, Bytes const& v, bool inLine);
    void mergeAtAux(CTrieNode& out, CTrieNode const& orig, CNibbleView k, Bytes const& v);

    Bytes at(const Bytes& key) const;

    Bytes atAux(const CTrieNode& here, CNibbleView key) const;

    void insert(Bytes const& key, Bytes const& value);

private:
    CTrieNode place(CTrieNode const& orig, CNibbleView k, Bytes const& s);

    H256 rawInsertNode(CTrieNode const& v) { auto h = v.GetHash(); rawInsertNode(h, v); return h; }
    void rawInsertNode(H256 const& h, CTrieNode v) { mDB->insert(h, v.GetBytes()); }

    void killNode(CTrieNode const& d) { mDB->kill( d.GetHash() ); }
protected:
    H256 mRoot;
    DB* mDB = nullptr;
};

template <class DB>
Bytes CTrieDB<DB>::at(const Bytes& key) const
{
    return atAux(node(mRoot), key);
}

template <class DB>
Bytes CTrieDB<DB>::atAux(const CTrieNode& here, CNibbleView key) const
{
    unsigned listSize = here.size();

    assert(listSize == 0 || listSize == 1 || listSize == 2 || listSize == 17);

    if(listSize == 0) {
        return Bytes();
    }

    if (listSize == 2) {
        auto k = keyOf(here);
        if (key == k && isLeaf(here)) {
            return here[1];
        }
        else if (key.contains(k) && !isLeaf(here)) {
            auto midKey = key.mid(k.size());
            return atAux(node(here[1]), midKey);
        } else {
            return Bytes();
        }
    } else {
        if (key.size() == 0)
            return here[16];

        Bytes n = here[key[0]];
        if (n.size() == 0)
            return Bytes();
        else
            return atAux(node(n), key.mid(1));
    }
}

template <class DB>
CTrieNode CTrieDB<DB>::place(CTrieNode const& orig, CNibbleView k, Bytes const& s)
{
    killNode(orig);
    if (orig.IsEmpty())
        return CTrieNode(hexPrefixEncode(k, true), s);

    if (orig.size() == 2)
        return CTrieNode(orig[0], s);

    auto n = CTrieNode();
    n = orig;
    n[17] = s;

    return n;
}

template <class DB>
void CTrieDB<DB>::mergeAtAux(CTrieNode& out, CTrieNode const& orig, CNibbleView k, Bytes const& v)
{
    CTrieNode r = orig;
    // _orig is always a segment of a node's RLP - removing it alone is pointless. However, if may be a hash, in which case we deref and we know it is removable.
    bool isRemovable = false;
    if (!r.IsEmpty())
    {
        r = node(orig.GetHash());
        isRemovable = true;
    }
    CTrieNode b = mergeAt(r, k, v, !isRemovable);
    out.push_back(rawInsertNode(b).AsBytes());
}

template <class DB>
CTrieNode CTrieDB<DB>::mergeAt(CTrieNode const& orig, H256 const& origHash, CNibbleView k, Bytes const& v, bool inLine)
{
    if(orig.IsEmpty()) {
        return place(orig, k, v);
    }

    unsigned count = orig.size();

    if (count == 2) {
        // pair...
        CNibbleView nk = keyOf(orig);

        // exactly our node - place value in directly.
        if (nk == k && isLeaf(orig))
            return place(orig, k, v);

        // partial key is our key - move down.
        if (k.contains(nk) && !isLeaf(orig)) {
            if (!inLine)
                killNode(orig, origHash);

            CTrieNode s;
            s.push_back(orig[0]);
            mergeAtAux(s, orig[1], k.mid(k.size()), v);
            return s;
        }

        /*
        auto sh = k.shared(nk);
        if (sh) {
            // shared stuff - cleve at disagreement.
            auto cleved = cleve(_orig, sh);
            return mergeAt(RLP(cleved), _k, _v, true);
        } else {
            // nothing shared - branch
            auto branched = branch(_orig);
            return mergeAt(RLP(branched), _k, _v, true);
        }*/
    }
}

template <class DB>
void CTrieDB<DB>::insert(Bytes const& key, Bytes const& value)
{
    /*
    Bytes rootValue = node(mRoot);
    assert(rootValue.size());
    bytes b = mergeAt(RLP(rootValue), m_root, NibbleSlice(_key), _value);

    // mergeAt won't attempt to delete the node if it's less than 32 bytes
    // However, we know it's the root node and thus always hashed.
    // So, if it's less than 32 (and thus should have been deleted but wasn't) then we delete it here.
    if (rootValue.size() < 32)
        forceKillNode(m_root);
    m_root = forceInsertNode(&b);
    */
}

#endif // TRIEDB_H
