// Copyright (c) 2017 Harry Kalogirou (harkal@gmail.com)
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "crypto/sha3.h"
#include "crypto/common.h"

CSHA3::CSHA3()
{
    sph_keccak256_init(&cc);
}

CSHA3& CSHA3::Write(const unsigned char* data, size_t len)
{
    sph_keccak256(&cc, data, len);
    return *this;
}

void CSHA3::Finalize(unsigned char hash[OUTPUT_SIZE])
{
    uint64_t _h[4];

    sph_keccak256_close(&cc, _h);

    WriteBE64(hash, _h[3]);
    WriteBE64(hash + 8, _h[2]);
    WriteBE64(hash + 16, _h[1]);
    WriteBE64(hash + 24, _h[0]);
}

std::vector<unsigned char> CSHA3::Finalize()
{
    unsigned char hash[OUTPUT_SIZE];
    Finalize(hash);
    std::vector<unsigned char> ret;

    ret.reserve(OUTPUT_SIZE);
    for(int i = 0 ; i < OUTPUT_SIZE ; ++i)
        ret.push_back(hash[i]);

    return ret;
}

CSHA3& CSHA3::Reset()
{
    sph_keccak256_init(&cc);
    return *this;
}

