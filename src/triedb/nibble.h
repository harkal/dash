// Copyright (c) 2017 Harry Kalogirou (harkal@gmail.com)
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef NIBBLE_H
#define NIBBLE_H

#include "common.h"
#include "crypto/hash.h"
#include "serialize.h"
#include "hash.h"


class CTrieNode : public std::vector<Bytes> {
public:
    CTrieNode(Bytes const& n1, Bytes const& n2) {
        push_back(n1);
        push_back(n2);
    }

    H256 GetHash() {
        std::vector<Bytes> const &tmp = *this;
        return (CHashWriter(SER_GETHASH, 0) << tmp).GetHash();
    }

    bool IsEmpty() const {
        return size() == 0;
    }
};

inline Byte nibble(const Bytes &data, unsigned i)
{
    return (i & 1) ? (data[i / 2] & 15) : (data[i / 2] >> 4);
}

inline unsigned sharedNibbles(const Bytes& first, unsigned beginFirst, unsigned endFirst, const Bytes& second, unsigned beginSecond, unsigned endSecond)
{
    unsigned ret = 0;
    while (beginFirst < endFirst && beginSecond < endSecond && nibble(first, beginFirst) == nibble(second, beginSecond)) {
        ++beginFirst;
        ++beginSecond;
        ++ret;
    }

    return ret;
}

class CNibbleView
{
public:    
    Bytes const& mData;
    unsigned mOffset;

    CNibbleView(Bytes const& data = Bytes(), unsigned offset = 0): mData(data), mOffset(offset) {}
    Byte operator[](unsigned i) const { return nibble(mData, mOffset + i); }
    unsigned size() const { return mData.size() * 2 - mOffset; }
    bool empty() const { return !size(); }
    CNibbleView mid(unsigned i) const { return CNibbleView(mData, mOffset + i); }
    //void clear() { mData.clear(); mOffset = 0; }

    /// @returns true iff k is a prefix of this.
    bool contains(CNibbleView k) const { return shared(k) == k.size(); }
    /// @returns the number of shared nibbles at the beginning of this and k.
    unsigned shared(CNibbleView k) const { return sharedNibbles(mData, mOffset, mOffset + size(), k.mData, k.mOffset, k.mOffset + k.size()); }
    /**
     * @brief Determine if we, a full key, are situated prior to a particular key-prefix.
     * @param _k The prefix.
     * @return true if we are strictly prior to the prefix.
     */
    bool isEarlierThan(CNibbleView k) const
    {
        unsigned i = 0;
        for (; i < k.size() && i < size(); ++i)
            if (operator[](i) < k[i])		// Byte is lower - we're earlier..
                return true;
            else if (operator[](i) > k[i])	// Byte is higher - we're not earlier.
                return false;
        if (i >= k.size())					// Ran past the end of the prefix - we're == for the entire prefix - we're not earlier.
            return false;
        return true;						// Ran out before the prefix had finished - we're earlier.
    }
    bool operator==(CNibbleView k) const { return k.size() == size() && shared(k) == k.size(); }
    bool operator!=(CNibbleView s) const { return !operator==(s); }
};

inline std::ostream& operator<<(std::ostream& o, CNibbleView const& nv)
{
    for (unsigned i = 0; i < nv.size(); ++i)
        o << std::hex << (int)nv[i] << std::dec;
    return o;
}

inline bool isLeaf(CTrieNode const& node)
{
    ///assert(node.count() == 2);
    auto pl = node[0];
    return (pl[0] & 0x20) != 0;
}

inline CNibbleView keyOf(Bytes const& hexpe)
{
    if (!hexpe.size())
        return CNibbleView(hexpe, 0);
    if (hexpe[0] & 0x10)
        return CNibbleView(hexpe, 1);
    else
        return CNibbleView(hexpe, 2);
}

inline CNibbleView keyOf(CTrieNode const& node) {
    return keyOf(node[0]);
}

template <typename T>
inline CNibbleView keyOf(std::pair<Bytes const&,T> p)
{
    return keyOf(p.first);
}

//Byte uniqueInUse(RLP const& _orig, byte except);
Bytes hexPrefixEncode(Bytes const& _hexVector, bool _leaf = false, int _begin = 0, int _end = -1);
Bytes hexPrefixEncode(Bytes const& _data, bool _leaf, int _beginNibble, int _endNibble, unsigned _offset);
Bytes hexPrefixEncode(Bytes const& _d1, unsigned _o1, Bytes const& _d2, unsigned _o2, bool _leaf);

inline Bytes hexPrefixEncode(CNibbleView _s, bool _leaf, int _begin = 0, int _end = -1)
{
    return hexPrefixEncode(_s.mData, _leaf, _begin, _end, _s.mOffset);
}

inline Bytes hexPrefixEncode(CNibbleView _s1, CNibbleView _s2, bool _leaf)
{
    return hexPrefixEncode(_s1.mData, _s1.mOffset, _s2.mData, _s2.mOffset, _leaf);
}

#endif // NIBBLE_H
