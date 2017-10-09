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
        if(mDB->Read(hash, *(std::vector<Bytes> *)&data)) {
            return data;
        }

        return CTrieNode();
    }

    void init() {
        CTrieNode root = CTrieNode();
        SetRoot(RawInsertNode(root));
    }

    void SetRoot(const H256& root)
    {
        mRoot = root;
    }

    bool IsNull() const { return mRoot == NullTrieDBNode; }
    bool IsEmpty() const { return node(mRoot).size() == 1; }

    const H256& root() const
    {
        if (node(mRoot).empty())
            BOOST_THROW_EXCEPTION(BadRoot(mRoot));
        return mRoot;
    }

    CTrieNode MergeAt(CTrieNode const& orig, CNibbleView k, Bytes const& v, bool inLine = false);
    CTrieNode MergeAt(CTrieNode const& orig, H256 const& origHash, CNibbleView k, Bytes const& v, bool inLine = false);
    void MergeAtAux(CTrieNode& out, CTrieNode const& orig, CNibbleView k, Bytes const& v);

    template <typename V>
    bool GetValue(const H256& key, V& value) const {
        return mDB->Read(key, value);
    }

    H256 At(const Bytes& key) const;

    H256 AtAux(const CTrieNode& here, CNibbleView key) const;

    void Insert(Bytes const& key, Bytes const& value);

    template <typename V>
    void InsertValue(Bytes const&key, V const& value);

    bool Contains(const Bytes& key) const { return !At(key).IsNull(); }

    void Remove(const Bytes& key);

    CTrieNode DeleteAt(CTrieNode const& orig, CNibbleView k);
    bool DeleteAtAux(CTrieNode& out, CTrieNode const& orig, CNibbleView k);

private:
    CTrieNode Place(CTrieNode const& orig, CNibbleView k, Bytes const& s);
    CTrieNode Cleve(CTrieNode const& orig, unsigned s);
    CTrieNode Branch(CTrieNode const& orig);
    CTrieNode Graft(CTrieNode const& orig);
    CTrieNode Merge(CTrieNode const& orig, Byte i);

    H256 RawInsertNode(CTrieNode const& v) { auto h = v.GetHash(); RawInsertNode(h, v); return h; }
    void RawInsertNode(H256 const& h, CTrieNode v) { mDB->Write(h, *(std::vector<Bytes> *)&v); }

    void KillNode(H256 const& hash) { mDB->Erase(hash); }
    void KillNode(CTrieNode const& d) { mDB->Erase( d.GetHash() ); }

    Byte UniqueInUse(CTrieNode const& orig, Byte except)
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
H256 CTrieDB<DB>::At(const Bytes& key) const
{
    return AtAux(node(mRoot), key);
}

template <class DB>
H256 CTrieDB<DB>::AtAux(const CTrieNode& here, CNibbleView key) const
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
            return AtAux(node(here[1]), midKey);
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
            return AtAux(node(n), key.mid(1));
    }
}

template <class DB>
CTrieNode CTrieDB<DB>::Place(CTrieNode const& orig, CNibbleView k, Bytes const& s)
{
    KillNode(orig);
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
CTrieNode CTrieDB<DB>::Cleve(CTrieNode const& orig, unsigned s)
{
    KillNode(orig);
    assert(orig.size() == 2);

    auto k = keyOf(orig);
    assert(s && s <= k.size());

    CTrieNode bottom(hexPrefixEncode(k, isLeaf(orig), (int)s), orig[1]);

    CTrieNode top(hexPrefixEncode(k, false, 0, (int)s), RawInsertNode(bottom).AsBytes());

    return top;
}

template <class DB>
CTrieNode CTrieDB<DB>::Branch(CTrieNode const& orig)
{
    assert(orig.size() == 2);
    KillNode(orig);

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
                    r.push_back( RawInsertNode(CTrieNode(hexPrefixEncode(k.mid(1), isLeaf(orig)), orig[1])).AsBytes()  );
                else
                    r.push_back(orig[1]);
            else
                r.push_back(Bytes());
        r.push_back(Bytes());
    }
    return r;
}

template <class DB>
CTrieNode CTrieDB<DB>::Graft(CTrieNode const& orig)
{
    assert(orig.size() == 2);

    CTrieNode s;
    CTrieNode n;

    s = node(orig[1]);
    KillNode(H256(orig[1]));
    n = CTrieNode(s);

    assert(n.size() == 2);

    return CTrieNode(hexPrefixEncode(keyOf(orig), keyOf(n), isLeaf(n)), n[1]);
}

template <class DB>
CTrieNode CTrieDB<DB>::Merge(CTrieNode const& orig, Byte i)
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
void CTrieDB<DB>::MergeAtAux(CTrieNode& out, CTrieNode const& orig, CNibbleView k, Bytes const& v)
{
    CTrieNode r = orig;

    bool isRemovable = false;
    if (!r.IsEmpty()) {
        r = node(orig.GetHash());
        isRemovable = true;
    }

    CTrieNode b = MergeAt(r, k, v, !isRemovable);
    out.push_back(RawInsertNode(b).AsBytes());
}

template <class DB>
CTrieNode CTrieDB<DB>::MergeAt(CTrieNode const& orig, CNibbleView k, Bytes const& v, bool inLine)
{
    return MergeAt(orig, orig.GetHash(), k, v, inLine);
}


template <class DB>
CTrieNode CTrieDB<DB>::MergeAt(CTrieNode const& orig, H256 const& origHash, CNibbleView k, Bytes const& v, bool inLine)
{
    if (orig.IsEmpty()) {
        return Place(orig, k, v);
    }

    unsigned count = orig.size();

    if (count == 2) {
        CNibbleView nk = keyOf(orig);

        if (nk == k && isLeaf(orig))
            return Place(orig, k, v);

        if (k.contains(nk) && !isLeaf(orig)) {
            if (!inLine)
                KillNode(orig);

            CTrieNode s;
            s.push_back(orig[0]);
            MergeAtAux(s, node(orig[1]), k.mid(k.size()), v);
            return s;
        }

        auto sh = k.shared(nk);

        if (sh) {
            CTrieNode cleved = Cleve(orig, sh);
            return MergeAt(cleved, k, v, true);
        } else {
            CTrieNode branched = Branch(orig);
            return MergeAt(branched, k, v, true);
        }
    } else {
        if (k.size() == 0)
            return Place(orig, k, v);

        if (!inLine)
            KillNode(orig);

        Byte n = k[0];

        CTrieNode r;
        for (Byte i = 0; i < 17; ++i) {
            if (i == n)
                MergeAtAux(r, node(orig[i]), k.mid(1), v);
            else
                r.push_back(orig[i]);
        }

        return r;
    }
}

template <class DB>
void CTrieDB<DB>::Insert(Bytes const& key, Bytes const& value)
{
    CTrieNode rootValue = node(mRoot);
    CTrieNode b = MergeAt(rootValue, mRoot, CNibbleView(key), value);
    mRoot = RawInsertNode(b);
}

template <class DB>
void CTrieDB<DB>::Remove(const Bytes& key)
{
    CTrieNode n = node(mRoot);
    CTrieNode b = DeleteAt(n, CNibbleView(key));

    if (b.size()) {
        mRoot = RawInsertNode(b);
    }
}

template <class DB>
template <typename V>
void CTrieDB<DB>::InsertValue(Bytes const&key, V const& value)
{
    H256 hash = (CHashWriter(SER_NETWORK, 0) << value).GetHash();
    mDB->Write(hash, value);

    Insert(key, hash.AsBytes());
}

template <class DB>
CTrieNode CTrieDB<DB>::DeleteAt(CTrieNode const& orig, CNibbleView k)
{
    if (orig.IsEmpty())
        return CTrieNode();

    if (orig.size() == 2) {
        CNibbleView nk = keyOf(orig);

        if (nk == k && isLeaf(orig)) {
            KillNode(orig);
            return CTrieNode();
        }

        if (k.contains(nk)) {
            CTrieNode s;
            s.push_back(orig[0]);

            if (!DeleteAtAux(s, node(orig[1]), k.mid(nk.size())))
                return CTrieNode();

            KillNode(orig);

            CTrieNode r(s);

            if (node(r[1]).size() == 2) {
                return Graft(r);
            }

            return s;
        } else {
            return CTrieNode();
        }
    } else {
        if (k.size() == 0 && orig[16].size()) {
            // Kill the node.
            KillNode(orig);

            Byte used = UniqueInUse(orig, 16);

            if (used != 255) {
                if (node(orig[used]).size() == 2) {
                    auto merged = Merge(orig, used);
                    return Graft(merged);
                } else {
                    return Merge(orig, used);
                }
            } else {
                CTrieNode r(orig);
                r[17] = Bytes();
                return r;
            }
        } else {
            CTrieNode r;
            Byte n = k[0];

            for (unsigned i = 0 ; i < 17 ; ++i) {
                if (i == n) {
                    if (!DeleteAtAux(r, node(orig[i]), k.mid(1))) {	// bomb out if the key didn't turn up.
                        return CTrieNode();
                    } else {

                    }
                } else {
                    r.push_back( orig[i] );
                }
            }

            KillNode(orig);

            CTrieNode ret(r);
            Byte used = UniqueInUse(ret, 255);
            if (used == 255)	// no - all ok.
                return r;

            if (node(ret[used]).size() == 2) {
                CTrieNode merged = Merge(ret, used);
                return Graft(merged);
            } else {
                return Merge(ret, used);
            }
        }
    }

}

template <class DB>
bool CTrieDB<DB>::DeleteAtAux(CTrieNode& out, CTrieNode const& orig, CNibbleView k)
{

    CTrieNode b = orig.IsEmpty() ? orig : DeleteAt(orig, k);

    if (!b.size())
        return false;

    out.push_back(RawInsertNode(b).AsBytes());

    return true;
}

#endif // TRIEDB_H
