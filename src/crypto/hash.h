// Copyright (c) 2017 Harry Kalogirou (harkal@gmail.com)
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef HASH_H
#define HASH_H

#include <array>
#include <../common.h>

template <unsigned N>
class CHash
{
public:
    CHash() { clear(); }

    void clear() { mData.fill(0); }
private:
    std::array<common::Byte, N> mData;
};

// Define hash types
using H2048 = CHash<256>;
using H1024 = CHash<128>;
using H520  = CHash<65>;
using H512  = CHash<64>;
using H256  = CHash<32>;
using H160  = CHash<20>;
using H128  = CHash<16>;
using H64   = CHash<8>;

#endif // HASH_H
