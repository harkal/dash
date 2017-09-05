// Copyright (c) 2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_MERKLE
#define BITCOIN_MERKLE

#include <stdint.h>
#include <vector>

#include "primitives/transaction.h"
#include "primitives/block.h"

H256 ComputeMerkleRoot(const std::vector<H256>& leaves, bool* mutated = NULL);
std::vector<H256> ComputeMerkleBranch(const std::vector<H256>& leaves, uint32_t position);
H256 ComputeMerkleRootFromBranch(const H256& leaf, const std::vector<H256>& branch, uint32_t position);

/*
 * Compute the Merkle root of the transactions in a block.
 * *mutated is set to true if a duplicated subtree was found.
 */
H256 BlockMerkleRoot(const CBlock& block, bool* mutated = NULL);

/*
 * Compute the Merkle branch for the tree of transactions in a block, for a
 * given position.
 * This can be verified using ComputeMerkleRootFromBranch.
 */
std::vector<H256> BlockMerkleBranch(const CBlock& block, uint32_t position);

#endif
