// Copyright (c) 2017 Harry Kalogirou (harkal@gmail.com)
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "nibble.h"

/*
 * Hex-prefix Notation. First nibble has flags: oddness = 2^0 & termination = 2^1
 * NOTE: the "termination marker" and "leaf-node" specifier are completely equivalent.
 * [0,0,1,2,3,4,5]   0x10012345
 * [0,1,2,3,4,5]     0x00012345
 * [1,2,3,4,5]       0x112345
 * [0,0,1,2,3,4]     0x00001234
 * [0,1,2,3,4]       0x101234
 * [1,2,3,4]         0x001234
 * [0,0,1,2,3,4,5,T] 0x30012345
 * [0,0,1,2,3,4,T]   0x20001234
 * [0,1,2,3,4,5,T]   0x20012345
 * [1,2,3,4,5,T]     0x312345
 * [1,2,3,4,T]       0x201234
 */

Bytes hexPrefixEncode(Bytes const& hex, bool leaf, int begin, int end)
{
    unsigned b = begin;
    unsigned e = end < 0 ? hex.size() + 1 + end : end;

    bool odd = ((e - b) % 2) != 0;

    Bytes ret(1, ((leaf ? 2 : 0) | (odd ? 1 : 0)) * 16);

    if (odd) {
        ret[0] |= hex[b];
        ++b;
    }

    for (unsigned i = b; i < e; i += 2) {
        ret.push_back( hex[i] * 16 + hex[i + 1] );
    }

    return ret;
}

Bytes hexPrefixEncode(Bytes const& _data, bool _leaf, int _beginNibble, int _endNibble, unsigned _offset)
{
    unsigned begin = _beginNibble + _offset;
    unsigned end = (_endNibble < 0 ? ((int)(_data.size() * 2 - _offset) + 1) + _endNibble : _endNibble) + _offset;
    bool odd = (end - begin) & 1;

    Bytes ret(1, ((_leaf ? 2 : 0) | (odd ? 1 : 0)) * 16);
    ret.reserve((end - begin) / 2 + 1);

    unsigned d = odd ? 1 : 2;
    for (auto i = begin; i < end; ++i, ++d)
    {
        Byte n = nibble(_data, i);
        if (d & 1)	// odd
            ret.back() |= n;		// or the nibble onto the back
        else
            ret.push_back(n << 4);	// push the nibble on to the back << 4
    }
    return ret;
}

Bytes hexPrefixEncode(Bytes const& _d1, unsigned _o1, Bytes const& _d2, unsigned _o2, bool _leaf)
{
    unsigned begin1 = _o1;
    unsigned end1 = _d1.size() * 2;
    unsigned begin2 = _o2;
    unsigned end2 = _d2.size() * 2;

    bool odd = (end1 - begin1 + end2 - begin2) & 1;

    Bytes ret(1, ((_leaf ? 2 : 0) | (odd ? 1 : 0)) * 16);
    ret.reserve((end1 - begin1 + end2 - begin2) / 2 + 1);

    unsigned d = odd ? 1 : 2;
    for (auto i = begin1; i < end1; ++i, ++d)
    {
        Byte n = nibble(_d1, i);
        if (d & 1)	// odd
            ret.back() |= n;		// or the nibble onto the back
        else
            ret.push_back(n << 4);	// push the nibble on to the back << 4
    }
    for (auto i = begin2; i < end2; ++i, ++d)
    {
        Byte n = nibble(_d2, i);
        if (d & 1)	// odd
            ret.back() |= n;		// or the nibble onto the back
        else
            ret.push_back(n << 4);	// push the nibble on to the back << 4
    }
    return ret;
}
/*
byte uniqueInUse(RLP const& _orig, byte except)
{
    byte used = 255;
    for (unsigned i = 0; i < 17; ++i)
        if (i != except && !_orig[i].isEmpty())
        {
            if (used == 255)
                used = (byte)i;
            else
                return 255;
        }
    return used;
}
*/
