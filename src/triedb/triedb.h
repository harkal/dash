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


private:
    CTrieNode place(CTrieNode const& orig, CNibbleView k, Bytes const& s);
    CTrieNode cleve(CTrieNode const& orig, unsigned s);
    CTrieNode branch(CTrieNode const& orig);

    H256 rawInsertNode(CTrieNode const& v) { auto h = v.GetHash(); rawInsertNode(h, v); return h; }
    void rawInsertNode(H256 const& h, CTrieNode v) { mDB->Write(h, *(std::vector<Bytes> *)&v); }

    void killNode(CTrieNode const& d) { mDB->Erase( d.GetHash() ); }
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
    CTrieNode n = node(m_root);
    CTrieNode b = deleteAt(n, CNibbleView(key));

    if (b.size()) {
        mRoot = forceInsertNode(&b);
    }
}


template <class DB>
CTrieNode CTrieDB<DB>::deleteAt(CTrieNode const& orig, CNibbleView k)
{
    // The caller will make sure that the bytes are inserted properly.
    // - This might mean inserting an entry into m_over
    // We will take care to ensure that (our reference to) _orig is killed.

    // Empty - not found - no change.
    if (_orig.isEmpty())
        return bytes();

    assert(_orig.isList() && (_orig.itemCount() == 2 || _orig.itemCount() == 17));
    if (_orig.itemCount() == 2)
    {
        // pair...
        NibbleSlice k = keyOf(_orig);

        // exactly our node - return null.
        if (k == _k && isLeaf(_orig))
        {
            killNode(_orig);
            return RLPNull;
        }

        // partial key is our key - move down.
        if (_k.contains(k))
        {
            RLPStream s;
            s.appendList(2) << _orig[0];
            if (!deleteAtAux(s, _orig[1], _k.mid(k.size())))
                return bytes();
            killNode(_orig);
            RLP r(s.out());
            if (isTwoItemNode(r[1]))
                return graft(r);
            return s.out();
        }
        else
            // not found - no change.
            return bytes();
    }
    else
    {
        // branch...

        // exactly our node - remove and rejig.
        if (_k.size() == 0 && !_orig[16].isEmpty())
        {
            // Kill the node.
            killNode(_orig);

            byte used = uniqueInUse(_orig, 16);
            if (used != 255)
                if (isTwoItemNode(_orig[used]))
                {
                    auto merged = merge(_orig, used);
                    return graft(RLP(merged));
                }
                else
                    return merge(_orig, used);
            else
            {
                RLPStream r(17);
                for (byte i = 0; i < 16; ++i)
                    r << _orig[i];
                r << "";
                return r.out();
            }
        }
        else
        {
            // not exactly our node - delve to next level at the correct index.
            RLPStream r(17);
            byte n = _k[0];
            for (byte i = 0; i < 17; ++i)
                if (i == n)
                    if (!deleteAtAux(r, _orig[i], _k.mid(1)))	// bomb out if the key didn't turn up.
                        return bytes();
                    else {}
                else
                    r << _orig[i];

            // Kill the node.
            killNode(_orig);

            // check if we ended up leaving the node invalid.
            RLP rlp(r.out());
            byte used = uniqueInUse(rlp, 255);
            if (used == 255)	// no - all ok.
                return r.out();

            // yes; merge
            if (isTwoItemNode(rlp[used]))
            {
                auto merged = merge(rlp, used);
                return graft(RLP(merged));
            }
            else
                return merge(rlp, used);
        }
    }

}

template <class DB>
bool CTrieDB<DB>::deleteAtAux(CTrieNode& out, CTrieNode const& orig, CNibbleView k)
{

    bytes b = orig.IsEmpty() ? orig : deleteAt(orig, k);

    if (!b.size())	// not found - no change.
        return false;

/*	if (_orig.isList())
        killNode(_orig);
    else
        killNode(_orig.toHash<h256>());*/

    out.push_back(rawInsertNode(b));

    return true;
}

#endif // TRIEDB_H
