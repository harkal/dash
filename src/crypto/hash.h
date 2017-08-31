// Copyright (c) 2017 Harry Kalogirou (harkal@gmail.com)
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef HASH_H
#define HASH_H

#include <array>

template <unsigned N>
class CHash
{
public:
    CHash() { clear(); }

    void clear() { mData.fill(0); }
private:
    std::array<unsigned char, N> mData;
};

// Define hash types
using h2048 = CHash<256>;
using h1024 = CHash<128>;
using h520  = CHash<65>;
using h512  = CHash<64>;
using h256  = CHash<32>;
using h160  = CHash<20>;
using h128  = CHash<16>;
using h64   = CHash<8>;

#endif // HASH_H
