// Copyright (c) 2017 Harry Kalogirou (harkal@gmail.com)
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef TRIEDB_H
#define TRIEDB_H

#include "crypto/hash.h"
#include "exceptions.h"

#include "nibble.h"
#include "streams.h"

extern const H256 NullTrieDBNode;

template <class DB>
class CTrieDB
{
public:
    CTrieDB(DB* db) :  mRoot(NullTrieDBNode), mDB(db) {}
    ~CTrieDB() {}

    CTrieNode node(Bytes const& b) const {
        H256 hash(b);
        return node(hash);
    }

    CTrieNode node(H256 const& hash) const {
        CTrieNode data;
        mDB->Read(hash, *(std::vector<Bytes> *)&data);
        return data;
    }

    void init() {
        CTrieNode root = CTrieNode();
        setRoot(rawInsertNode(root));
    }

    void setRoot(const H256& root)
    {
        mRoot = root;
    }

    bool isNull() const { return mRoot == NullTrieDBNode; }
    bool isEmpty() const { return node(mRoot).size() == 1; }

    const H256& root() const
    {
        if (node(mRoot).empty())
            BOOST_THROW_EXCEPTION(BadRoot(mRoot));
        return mRoot;
    }

    CTrieNode mergeAt(CTrieNode const& orig, CNibbleView k, Bytes const& v, bool inLine = false);
    CTrieNode mergeAt(CTrieNode const& orig, H256 const& origHash, CNibbleView k, Bytes const& v, bool inLine = false);
    void mergeAtAux(CTrieNode& out, CTrieNode const& orig, CNibbleView k, Bytes const& v);

    H256 at(const Bytes& key) const;

    H256 atAux(const CTrieNode& here, CNibbleView key) const;

    void insert(Bytes const& key, Bytes const& value);

    bool contains(const Bytes& key) const { return !at(key).IsNull(); }

    void remove(const Bytes& key);

    CTrieNode deleteAt(CTrieNode const& orig, CNibbleView k);
    bool deleteAtAux(CTrieNode& out, CTrieNode const& orig, CNibbleView k);

private:
    CTrieNode place(CTrieNode const& orig, CNibbleView k, Bytes const& s);
    CTrieNode cleve(CTrieNode const& orig, unsigned s);
    CTrieNode branch(CTrieNode const& orig);
    CTrieNode graft(CTrieNode const& orig);
    CTrieNode merge(CTrieNode const& orig, Byte i);

    H256 rawInsertNode(CTrieNode const& v) { auto h = v.GetHash(); rawInsertNode(h, v); return h; }
    void rawInsertNode(H256 const& h, CTrieNode v) { mDB->Write(h, *(std::vector<Bytes> *)&v); }

    void killNode(H256 const& hash) { mDB->Erase(hash); }
    void killNode(CTrieNode const& d) { mDB->Erase( d.GetHash() ); }

    Byte uniqueInUse(CTrieNode const& orig, Byte except)
    {
        Byte used = 255;

        for (unsigned i = 0 ; i < 17 ; ++i) {
            if (i != except && orig[i].size())
            {
                if (used == 255)
                    used = (Byte)i;
                else
                    return 255;
            }
        }

        return used;
    }

protected:
    H256 mRoot;
    DB* mDB = nullptr;
};

template <class DB>
H256 CTrieDB<DB>::at(const Bytes& key) const
{
    return atAux(node(mRoot), key);
}

template <class DB>
H256 CTrieDB<DB>::atAux(const CTrieNode& here, CNibbleView key) const
{
    unsigned listSize = here.size();

    assert(listSize == 0 || listSize == 1 || listSize == 2 || listSize == 17);

    if(listSize == 0) {
        return H256();
    }

    if (listSize == 2) {
        auto k = keyOf(here);
        if (key == k && isLeaf(here)) {
            return H256(here[1]);
        }
        else if (key.contains(k) && !isLeaf(here)) {
            auto midKey = key.mid(k.size());
            return atAux(node(here[1]), midKey);
        } else {
            return H256();
        }
    } else {
        if (key.size() == 0)
            return H256(here[16]);

        Bytes n = here[key[0]];
        if (n.size() == 0)
            return H256();
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
CTrieNode CTrieDB<DB>::cleve(CTrieNode const& orig, unsigned s)
{
    killNode(orig);
    assert(orig.size() == 2);

    auto k = keyOf(orig);
    assert(s && s <= k.size());

    CTrieNode bottom(hexPrefixEncode(k, isLeaf(orig), (int)s), orig[1]);

    CTrieNode top(hexPrefixEncode(k, false, 0, (int)s), rawInsertNode(bottom).AsBytes());

    return top;
}

template <class DB>
CTrieNode CTrieDB<DB>::branch(CTrieNode const& orig)
{
    assert(orig.size() == 2);
    killNode(orig);

    auto k = keyOf(orig);

    CTrieNode r;
    if (k.size() == 0)
    {
        assert(isLeaf(orig));
        for (unsigned i = 0; i < 16; ++i)
            r.push_back(Bytes());
        r.push_back(orig[1]);
    }
    else
    {
        Byte b = k[0];
        for (unsigned i = 0; i < 16; ++i)
            if (i == b)
                if (isLeaf(orig) || k.size() > 1)
                    r.push_back( rawInsertNode(CTrieNode(hexPrefixEncode(k.mid(1), isLeaf(orig)), orig[1])).AsBytes()  );
                else
                    r.push_back(orig[1]);
            else
                r.push_back(Bytes());
        r.push_back(Bytes());
    }
    return r;
}

template <class DB>
CTrieNode CTrieDB<DB>::graft(CTrieNode const& orig)
{
    assert(orig.size() == 2);

    CTrieNode s;
    CTrieNode n;

    // remove second item from the trie after derefrencing it into s & n.
    s = node(orig[1]);
    killNode(H256(orig[1]));
    n = CTrieNode(s);

    assert(n.size() == 2);

    return CTrieNode(hexPrefixEncode(keyOf(orig), keyOf(n), isLeaf(n)), n[1]);
}

template <class DB>
CTrieNode CTrieDB<DB>::merge(CTrieNode const& orig, Byte i)
{
    assert(orig.size() == 17);

    CTrieNode s;
    if (i != 16) {
        assert(orig[i].size());
        Bytes key;
        key.push_back(i);
        s.push_back( hexPrefixEncode(key, false, 1, 2, 0) );
    } else {
        s.push_back( hexPrefixEncode(Bytes(), true) );
    }

    s.push_back( orig[i] );

    return s;
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
CTrieNode CTrieDB<DB>::mergeAt(CTrieNode const& orig, CNibbleView k, Bytes const& v, bool inLine)
{
    return mergeAt(orig, orig.GetHash(), k, v, inLine);
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
                killNode(orig);

            CTrieNode s;
            s.push_back(orig[0]);
            mergeAtAux(s, node(orig[1]), k.mid(k.size()), v);
            return s;
        }

        auto sh = k.shared(nk);

        if (sh) {
            // shared stuff - cleve at disagreement.
            auto cleved = cleve(orig, sh);
            return mergeAt(cleved, k, v, true);
        } else {
            // nothing shared - branch
            auto branched = branch(orig);
            return mergeAt(branched, k, v, true);
        }
    } else {
        // branch...

        // exactly our node - place value.
        if (k.size() == 0)
            return place(orig, k, v);

        // Kill the node.
        if (!inLine)
            killNode(orig);

        // not exactly our node - delve to next level at the correct index.
        Byte n = k[0];

        CTrieNode r;
        for (Byte i = 0; i < 17; ++i) {
            if (i == n)
                mergeAtAux(r, node(orig[i]), k.mid(1), v);
            else
                r.push_back(orig[i]);
        }

        return r;
    }
}

template <class DB>
void CTrieDB<DB>::insert(Bytes const& key, Bytes const& value)
{
    CTrieNode rootValue = node(mRoot);
    CTrieNode b = mergeAt(rootValue, mRoot, CNibbleView(key), value);
    mRoot = rawInsertNode(b);
}

template <class DB>
void CTrieDB<DB>::remove(const Bytes& key)
{
    CTrieNode n = node(mRoot);
    CTrieNode b = deleteAt(n, CNibbleView(key));

    if (b.size()) {
        mRoot = rawInsertNode(b);
    }
}


template <class DB>
CTrieNode CTrieDB<DB>::deleteAt(CTrieNode const& orig, CNibbleView k)
{
    // The caller will make sure that the bytes are inserted properly.
    // - This might mean inserting an entry into m_over
    // We will take care to ensure that (our reference to) _orig is killed.

    // Empty - not found - no change.
    if (orig.IsEmpty())
        return CTrieNode();

    if (orig.size() == 2) {
        // pair...
        CNibbleView nk = keyOf(orig);

        // exactly our node - return null.
        if (nk == k && isLeaf(orig))
        {
            killNode(orig);
            return CTrieNode(); // CHECK IT : returned RLPNull;
        }

        // partial key is our key - move down.
        if (k.contains(nk)) {
            CTrieNode s;
            s.push_back(orig[0]);

            if (!deleteAtAux(s, node(orig[1]), k.mid(nk.size())))
                return CTrieNode();

            killNode(orig);

            CTrieNode r(s);

            if (node(r[1]).size() == 2) {
                return graft(r);
            }

            return s;
        }
        else
            // not found - no change.
            return CTrieNode();
    } else {

        // branch...

        // exactly our node - remove and rejig.
        if (k.size() == 0 && orig[16].size())
        {
            // Kill the node.
            killNode(orig);

            Byte used = uniqueInUse(orig, 16);

            if (used != 255) {
                if (node(orig[used]).size() == 2) {
                    auto merged = merge(orig, used);
                    return graft(merged);
                } else {
                    return merge(orig, used);
                }
            } else {
                CTrieNode r(orig);
                r[17] = Bytes();
                return r;
            }
        }
        else
        {
            // not exactly our node - delve to next level at the correct index.
            CTrieNode r; //17
            Byte n = k[0];

            for (unsigned i = 0 ; i < 17 ; ++i) {
                if (i == n) {
                    if (!deleteAtAux(r, node(orig[i]), k.mid(1))) {	// bomb out if the key didn't turn up.
                        return CTrieNode();
                    } else {

                    }
                } else {
                    r.push_back( orig[i] );
                }
            }

            // Kill the node.
            killNode(orig);

            // check if we ended up leaving the node invalid.
            CTrieNode ret(r);
            Byte used = uniqueInUse(ret, 255);
            if (used == 255)	// no - all ok.
                return r;

            // yes; merge
            if (node(ret[used]).size() == 2) {
                CTrieNode merged = merge(ret, used);
                return graft(merged);
            } else {
                return merge(ret, used);
            }
        }
    }

}

template <class DB>
bool CTrieDB<DB>::deleteAtAux(CTrieNode& out, CTrieNode const& orig, CNibbleView k)
{

    CTrieNode b = orig.IsEmpty() ? orig : deleteAt(orig, k);

    if (!b.size())	// not found - no change.
        return false;

/*	if (_orig.isList())
        killNode(_orig);
    else
        killNode(_orig.toHash<h256>());*/

    out.push_back(rawInsertNode(b).AsBytes());

    return true;
}

#endif // TRIEDB_H
