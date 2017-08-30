// Copyright (c) 2017 Harry Kalogirou (harkal@gmail.com)
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef CSHA3_H
#define CSHA3_H

#include <stdint.h>
#include <stdlib.h>

#include "crypto/sph_keccak.h"

class CSHA3
{
public:
    static const size_t OUTPUT_SIZE = 32;

    CSHA3();
    CSHA3& Write(const unsigned char* data, size_t len);
    void Finalize(unsigned char hash[OUTPUT_SIZE]);
    CSHA3& Reset();

private:
    sph_keccak256_context *cc;
};

#endif // CSHA3_H
