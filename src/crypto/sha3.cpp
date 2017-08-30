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
    sph_keccak256_close(&cc, hash);
}

CSHA3& CSHA3::Reset()
{
    sph_keccak256_init(&cc);
    return *this;
}

