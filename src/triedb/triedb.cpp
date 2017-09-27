// Copyright (c) 2017 Harry Kalogirou (harkal@gmail.com)
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "triedb.h"
#include "crypto/keccak256.h"

H256 const NullTrieDBNode = CKeccak256::hash("");
