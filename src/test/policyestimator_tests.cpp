// Copyright (c) 2011-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "policy/policy.h"
#include "policy/fees.h"
#include "txmempool.h"
#include "uint256.h"
#include "util.h"

#include "test/test_bitcoin.h"

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(policyestimator_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(BlockPolicyEstimates)
{
    CTxMemPool mpool(CFeeRate(1000));
    TestMemPoolEntryHelper entry;
    CAmount basefee(2000);
    double basepri = 1e6;
    CAmount deltaFee(100);
    double deltaPri = 5e5;
    std::vector<CAmount> feeV[2];
    std::vector<double> priV[2];

    for (int j = 0; j < 10; j++) {

        feeV[0].push_back(basefee * (j + 1));
        priV[0].push_back(0);

        feeV[1].push_back(CAmount(0));
        priV[1].push_back(basepri * pow(10, j + 1));
    }

    std::vector<uint256> txHashes[10];

    CScript garbage;
    for (unsigned int i = 0; i < 128; i++)
        garbage.push_back('X');
    CMutableTransaction tx;
    std::list<CTransaction> dummyConflicted;
    tx.vin.resize(1);
    tx.vin[0].scriptSig = garbage;
    tx.vout.resize(1);
    tx.vout[0].nValue = 0LL;
    CFeeRate baseRate(basefee, GetVirtualTransactionSize(tx));

    std::vector<CTransaction> block;
    int blocknum = 0;

    while (blocknum < 200) {
        for (int j = 0; j < 10; j++) { // For each fee/pri multiple
            for (int k = 0; k < 5; k++) { // add 4 fee txs for every priority tx
                tx.vin[0].prevout.n = 10000 * blocknum + 100 * j + k; // make transaction unique
                uint256 hash = tx.GetHash();
                mpool.addUnchecked(hash, entry.Fee(feeV[k / 4][j]).Time(GetTime()).Priority(priV[k / 4][j]).Height(blocknum).FromTx(tx, &mpool));
                txHashes[j].push_back(hash);
            }
        }

        for (int h = 0; h <= blocknum % 10; h++) {

            while (txHashes[9 - h].size()) {
                std::shared_ptr<const CTransaction> ptx = mpool.get(txHashes[9 - h].back());
                if (ptx)
                    block.push_back(*ptx);
                txHashes[9 - h].pop_back();
            }
        }
        mpool.removeForBlock(block, ++blocknum, dummyConflicted);
        block.clear();
        if (blocknum == 30) {

            BOOST_CHECK(mpool.estimateFee(1) == CFeeRate(0));
            BOOST_CHECK(mpool.estimateFee(2) == CFeeRate(0));
            BOOST_CHECK(mpool.estimateFee(3) == CFeeRate(0));
            BOOST_CHECK(mpool.estimateFee(4).GetFeePerK() < 8 * baseRate.GetFeePerK() + deltaFee);
            BOOST_CHECK(mpool.estimateFee(4).GetFeePerK() > 8 * baseRate.GetFeePerK() - deltaFee);
            int answerFound;
            BOOST_CHECK(mpool.estimateSmartFee(1, &answerFound) == mpool.estimateFee(4) && answerFound == 4);
            BOOST_CHECK(mpool.estimateSmartFee(3, &answerFound) == mpool.estimateFee(4) && answerFound == 4);
            BOOST_CHECK(mpool.estimateSmartFee(4, &answerFound) == mpool.estimateFee(4) && answerFound == 4);
            BOOST_CHECK(mpool.estimateSmartFee(8, &answerFound) == mpool.estimateFee(8) && answerFound == 8);
        }
    }

    std::vector<CAmount> origFeeEst;
    std::vector<double> origPriEst;

    for (int i = 1; i < 10; i++) {
        origFeeEst.push_back(mpool.estimateFee(i).GetFeePerK());
        origPriEst.push_back(mpool.estimatePriority(i));
        if (i > 1) { // Fee estimates should be monotonically decreasing
            BOOST_CHECK(origFeeEst[i - 1] <= origFeeEst[i - 2]);
            BOOST_CHECK(origPriEst[i - 1] <= origPriEst[i - 2]);
        }
        int mult = 11 - i;
        BOOST_CHECK(origFeeEst[i - 1] < mult * baseRate.GetFeePerK() + deltaFee);
        BOOST_CHECK(origFeeEst[i - 1] > mult * baseRate.GetFeePerK() - deltaFee);
        BOOST_CHECK(origPriEst[i - 1] < pow(10, mult) * basepri + deltaPri);
        BOOST_CHECK(origPriEst[i - 1] > pow(10, mult) * basepri - deltaPri);
    }

    while (blocknum < 250)
        mpool.removeForBlock(block, ++blocknum, dummyConflicted);

    for (int i = 1; i < 10; i++) {
        BOOST_CHECK(mpool.estimateFee(i).GetFeePerK() < origFeeEst[i - 1] + deltaFee);
        BOOST_CHECK(mpool.estimateFee(i).GetFeePerK() > origFeeEst[i - 1] - deltaFee);
        BOOST_CHECK(mpool.estimatePriority(i) < origPriEst[i - 1] + deltaPri);
        BOOST_CHECK(mpool.estimatePriority(i) > origPriEst[i - 1] - deltaPri);
    }

    while (blocknum < 265) {
        for (int j = 0; j < 10; j++) { // For each fee/pri multiple
            for (int k = 0; k < 5; k++) { // add 4 fee txs for every priority tx
                tx.vin[0].prevout.n = 10000 * blocknum + 100 * j + k;
                uint256 hash = tx.GetHash();
                mpool.addUnchecked(hash, entry.Fee(feeV[k / 4][j]).Time(GetTime()).Priority(priV[k / 4][j]).Height(blocknum).FromTx(tx, &mpool));
                txHashes[j].push_back(hash);
            }
        }
        mpool.removeForBlock(block, ++blocknum, dummyConflicted);
    }

    int answerFound;
    for (int i = 1; i < 10; i++) {
        BOOST_CHECK(mpool.estimateFee(i) == CFeeRate(0) || mpool.estimateFee(i).GetFeePerK() > origFeeEst[i - 1] - deltaFee);
        BOOST_CHECK(mpool.estimateSmartFee(i, &answerFound).GetFeePerK() > origFeeEst[answerFound - 1] - deltaFee);
        BOOST_CHECK(mpool.estimatePriority(i) == -1 || mpool.estimatePriority(i) > origPriEst[i - 1] - deltaPri);
        BOOST_CHECK(mpool.estimateSmartPriority(i, &answerFound) > origPriEst[answerFound - 1] - deltaPri);
    }

    for (int j = 0; j < 10; j++) {
        while (txHashes[j].size()) {
            std::shared_ptr<const CTransaction> ptx = mpool.get(txHashes[j].back());
            if (ptx)
                block.push_back(*ptx);
            txHashes[j].pop_back();
        }
    }
    mpool.removeForBlock(block, 265, dummyConflicted);
    block.clear();
    for (int i = 1; i < 10; i++) {
        BOOST_CHECK(mpool.estimateFee(i).GetFeePerK() > origFeeEst[i - 1] - deltaFee);
        BOOST_CHECK(mpool.estimatePriority(i) > origPriEst[i - 1] - deltaPri);
    }

    while (blocknum < 465) {
        for (int j = 0; j < 10; j++) { // For each fee/pri multiple
            for (int k = 0; k < 5; k++) { // add 4 fee txs for every priority tx
                tx.vin[0].prevout.n = 10000 * blocknum + 100 * j + k;
                uint256 hash = tx.GetHash();
                mpool.addUnchecked(hash, entry.Fee(feeV[k / 4][j]).Time(GetTime()).Priority(priV[k / 4][j]).Height(blocknum).FromTx(tx, &mpool));
                std::shared_ptr<const CTransaction> ptx = mpool.get(hash);
                if (ptx)
                    block.push_back(*ptx);
            }
        }
        mpool.removeForBlock(block, ++blocknum, dummyConflicted);
        block.clear();
    }
    for (int i = 1; i < 10; i++) {
        BOOST_CHECK(mpool.estimateFee(i).GetFeePerK() < origFeeEst[i - 1] - deltaFee);
        BOOST_CHECK(mpool.estimatePriority(i) < origPriEst[i - 1] - deltaPri);
    }

    mpool.addUnchecked(tx.GetHash(), entry.Fee(feeV[0][5]).Time(GetTime()).Priority(priV[1][5]).Height(blocknum).FromTx(tx, &mpool));

    mpool.TrimToSize(1);
    BOOST_CHECK(mpool.GetMinFee(1).GetFeePerK() > feeV[0][5]);
    for (int i = 1; i < 10; i++) {
        BOOST_CHECK(mpool.estimateSmartFee(i).GetFeePerK() >= mpool.estimateFee(i).GetFeePerK());
        BOOST_CHECK(mpool.estimateSmartFee(i).GetFeePerK() >= mpool.GetMinFee(1).GetFeePerK());
        BOOST_CHECK(mpool.estimateSmartPriority(i) == INF_PRIORITY);
    }
}

BOOST_AUTO_TEST_SUITE_END()
