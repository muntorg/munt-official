// Copyright (c) 2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "consensus/merkle.h"
#include "test/test_bitcoin.h"
#include "random.h"

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(merkle_tests, TestingSetup)

// Older version of the merkle root computation code, for comparison.
static uint256 BlockBuildMerkleTree(const CBlock& block, bool* fMutated, std::vector<uint256>& vMerkleTree)
{
    vMerkleTree.clear();
    vMerkleTree.reserve(block.vtx.size() * 2 + 16); // Safe upper bound for the number of total nodes.
    for (std::vector<CTransaction>::const_iterator it(block.vtx.begin()); it != block.vtx.end(); ++it)
        vMerkleTree.push_back(it->GetHash());
    int j = 0;
    bool mutated = false;
    for (int nSize = block.vtx.size(); nSize > 1; nSize = (nSize + 1) / 2) {
        for (int i = 0; i < nSize; i += 2) {
            int i2 = std::min(i + 1, nSize - 1);
            if (i2 == i + 1 && i2 + 1 == nSize && vMerkleTree[j + i] == vMerkleTree[j + i2]) {

                mutated = true;
            }
            vMerkleTree.push_back(Hash(vMerkleTree[j + i].begin(), vMerkleTree[j + i].end(),
                                       vMerkleTree[j + i2].begin(), vMerkleTree[j + i2].end()));
        }
        j += nSize;
    }
    if (fMutated) {
        *fMutated = mutated;
    }
    return (vMerkleTree.empty() ? uint256() : vMerkleTree.back());
}

static std::vector<uint256> BlockGetMerkleBranch(const CBlock& block, const std::vector<uint256>& vMerkleTree, int nIndex)
{
    std::vector<uint256> vMerkleBranch;
    int j = 0;
    for (int nSize = block.vtx.size(); nSize > 1; nSize = (nSize + 1) / 2) {
        int i = std::min(nIndex ^ 1, nSize - 1);
        vMerkleBranch.push_back(vMerkleTree[j + i]);
        nIndex >>= 1;
        j += nSize;
    }
    return vMerkleBranch;
}

static inline int ctz(uint32_t i)
{
    if (i == 0)
        return 0;
    int j = 0;
    while (!(i & 1)) {
        j++;
        i >>= 1;
    }
    return j;
}

BOOST_AUTO_TEST_CASE(merkle_test)
{
    for (int i = 0; i < 32; i++) {

        int ntx = (i <= 16) ? i : 17 + (insecure_rand() % 4000);

        for (int mutate = 0; mutate <= 3; mutate++) {
            int duplicate1 = mutate >= 1 ? 1 << ctz(ntx) : 0; // The last how many transactions to duplicate first.
            if (duplicate1 >= ntx)
                break; // Duplication of the entire tree results in a different root (it adds a level).
            int ntx1 = ntx + duplicate1; // The resulting number of transactions after the first duplication.
            int duplicate2 = mutate >= 2 ? 1 << ctz(ntx1) : 0; // Likewise for the second mutation.
            if (duplicate2 >= ntx1)
                break;
            int ntx2 = ntx1 + duplicate2;
            int duplicate3 = mutate >= 3 ? 1 << ctz(ntx2) : 0; // And for the third mutation.
            if (duplicate3 >= ntx2)
                break;
            int ntx3 = ntx2 + duplicate3;

            CBlock block;
            block.vtx.resize(ntx);
            for (int j = 0; j < ntx; j++) {
                CMutableTransaction mtx;
                mtx.nLockTime = j;
                block.vtx[j] = mtx;
            }

            bool unmutatedMutated = false;
            uint256 unmutatedRoot = BlockMerkleRoot(block, &unmutatedMutated);
            BOOST_CHECK(unmutatedMutated == false);

            block.vtx.resize(ntx3);
            for (int j = 0; j < duplicate1; j++) {
                block.vtx[ntx + j] = block.vtx[ntx + j - duplicate1];
            }
            for (int j = 0; j < duplicate2; j++) {
                block.vtx[ntx1 + j] = block.vtx[ntx1 + j - duplicate2];
            }
            for (int j = 0; j < duplicate3; j++) {
                block.vtx[ntx2 + j] = block.vtx[ntx2 + j - duplicate3];
            }

            bool oldMutated = false;
            std::vector<uint256> merkleTree;
            uint256 oldRoot = BlockBuildMerkleTree(block, &oldMutated, merkleTree);

            bool newMutated = false;
            uint256 newRoot = BlockMerkleRoot(block, &newMutated);
            BOOST_CHECK(oldRoot == newRoot);
            BOOST_CHECK(newRoot == unmutatedRoot);
            BOOST_CHECK((newRoot == uint256()) == (ntx == 0));
            BOOST_CHECK(oldMutated == newMutated);
            BOOST_CHECK(newMutated == !!mutate);

            if (mutate == 0) {
                for (int loop = 0; loop < std::min(ntx, 16); loop++) {

                    int mtx = loop;
                    if (ntx > 16) {
                        mtx = insecure_rand() % ntx;
                    }
                    std::vector<uint256> newBranch = BlockMerkleBranch(block, mtx);
                    std::vector<uint256> oldBranch = BlockGetMerkleBranch(block, merkleTree, mtx);
                    BOOST_CHECK(oldBranch == newBranch);
                    BOOST_CHECK(ComputeMerkleRootFromBranch(block.vtx[mtx].GetHash(), newBranch, mtx) == oldRoot);
                }
            }
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()
