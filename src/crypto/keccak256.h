// Copyright (c) 2017 Harry Kalogirou (harkal@gmail.com)
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef CKECCAK256_H
#define CKECCAK256_H

#include <stdint.h>
#include <stdlib.h>

#include "crypto/sph_keccak.h"
#include "../common.h"
#include "hash.h"

class CKeccak256
{
public:
    static const size_t OUTPUT_SIZE = 32;

    CKeccak256();
    CKeccak256& Write(const Byte* data, size_t len);
    void Finalize(Byte hash[OUTPUT_SIZE]);
    H256 Finalize();
    CKeccak256& Reset();

    static H256 hash(const char* cstr) {
        return CKeccak256().Write((const Byte*)cstr, strlen(cstr)).Finalize();
    }

private:
    sph_keccak256_context cc;
};

#endif // CKECCAK256_H
