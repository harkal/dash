// Copyright (c) 2017 Harry Kalogirou (harkal@gmail.com)
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "crypto/keccak256.h"
#include "crypto/common.h"

CKeccak256::CKeccak256()
{
    sph_keccak256_init(&cc);
}

CKeccak256& CKeccak256::Write(const Byte* data, size_t len)
{
    sph_keccak256(&cc, data, len);
    return *this;
}

void CKeccak256::Finalize(Byte hash[OUTPUT_SIZE])
{
    sph_keccak256_close(&cc, hash);
}

H256 CKeccak256::Finalize()
{
    Byte hash[OUTPUT_SIZE];

    Finalize(hash);

    H256 ret(hash);

    return ret;
}

CKeccak256& CKeccak256::Reset()
{
    sph_keccak256_init(&cc);
    return *this;
}

