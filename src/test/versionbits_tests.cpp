// Copyright (c) 2014-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "chain.h"
#include "random.h"
#include "versionbits.h"
#include "test/test_bitcoin.h"
#include "chainparams.h"
#include "main.h"
#include "consensus/params.h"

#include <boost/test/unit_test.hpp>

/* Define a virtual block time, one block per 10 minutes after Nov 14 2014, 0:55:36am */
int32_t TestTime(int nHeight) { return 1415926536 + 600 * nHeight; }

static const Consensus::Params paramsDummy = Consensus::Params();

class TestConditionChecker : public AbstractThresholdConditionChecker {
private:
    mutable ThresholdConditionCache cache;

public:
    int64_t BeginTime(const Consensus::Params& params) const { return TestTime(10000); }
    int64_t EndTime(const Consensus::Params& params) const { return TestTime(20000); }
    int Period(const Consensus::Params& params) const { return 1000; }
    int Threshold(const Consensus::Params& params) const { return 900; }
    bool Condition(const CBlockIndex* pindex, const Consensus::Params& params) const { return (pindex->nVersion & 0x100); }

    ThresholdState GetStateFor(const CBlockIndex* pindexPrev) const { return AbstractThresholdConditionChecker::GetStateFor(pindexPrev, paramsDummy, cache); }
};

#define CHECKERS 6

class VersionBitsTester {

    std::vector<CBlockIndex*> vpblock;

    TestConditionChecker checker[CHECKERS];

    int num;

public:
    VersionBitsTester()
        : num(0)
    {
    }

    VersionBitsTester& Reset()
    {
        for (unsigned int i = 0; i < vpblock.size(); i++) {
            delete vpblock[i];
        }
        for (unsigned int i = 0; i < CHECKERS; i++) {
            checker[i] = TestConditionChecker();
        }
        vpblock.clear();
        return *this;
    }

    ~VersionBitsTester()
    {
        Reset();
    }

    VersionBitsTester& Mine(unsigned int height, int32_t nTime, int32_t nVersion)
    {
        while (vpblock.size() < height) {
            CBlockIndex* pindex = new CBlockIndex();
            pindex->nHeight = vpblock.size();
            pindex->pprev = vpblock.size() > 0 ? vpblock.back() : NULL;
            pindex->nTime = nTime;
            pindex->nVersion = nVersion;
            pindex->BuildSkip();
            vpblock.push_back(pindex);
        }
        return *this;
    }

    VersionBitsTester& TestDefined()
    {
        for (int i = 0; i < CHECKERS; i++) {
            if ((insecure_rand() & ((1 << i) - 1)) == 0) {
                BOOST_CHECK_MESSAGE(checker[i].GetStateFor(vpblock.empty() ? NULL : vpblock.back()) == THRESHOLD_DEFINED, strprintf("Test %i for DEFINED", num));
            }
        }
        num++;
        return *this;
    }

    VersionBitsTester& TestStarted()
    {
        for (int i = 0; i < CHECKERS; i++) {
            if ((insecure_rand() & ((1 << i) - 1)) == 0) {
                BOOST_CHECK_MESSAGE(checker[i].GetStateFor(vpblock.empty() ? NULL : vpblock.back()) == THRESHOLD_STARTED, strprintf("Test %i for STARTED", num));
            }
        }
        num++;
        return *this;
    }

    VersionBitsTester& TestLockedIn()
    {
        for (int i = 0; i < CHECKERS; i++) {
            if ((insecure_rand() & ((1 << i) - 1)) == 0) {
                BOOST_CHECK_MESSAGE(checker[i].GetStateFor(vpblock.empty() ? NULL : vpblock.back()) == THRESHOLD_LOCKED_IN, strprintf("Test %i for LOCKED_IN", num));
            }
        }
        num++;
        return *this;
    }

    VersionBitsTester& TestActive()
    {
        for (int i = 0; i < CHECKERS; i++) {
            if ((insecure_rand() & ((1 << i) - 1)) == 0) {
                BOOST_CHECK_MESSAGE(checker[i].GetStateFor(vpblock.empty() ? NULL : vpblock.back()) == THRESHOLD_ACTIVE, strprintf("Test %i for ACTIVE", num));
            }
        }
        num++;
        return *this;
    }

    VersionBitsTester& TestFailed()
    {
        for (int i = 0; i < CHECKERS; i++) {
            if ((insecure_rand() & ((1 << i) - 1)) == 0) {
                BOOST_CHECK_MESSAGE(checker[i].GetStateFor(vpblock.empty() ? NULL : vpblock.back()) == THRESHOLD_FAILED, strprintf("Test %i for FAILED", num));
            }
        }
        num++;
        return *this;
    }

    CBlockIndex* Tip() { return vpblock.size() ? vpblock.back() : NULL; }
};

BOOST_FIXTURE_TEST_SUITE(versionbits_tests, TestingSetup)

BOOST_AUTO_TEST_CASE(versionbits_test)
{
    for (int i = 0; i < 64; i++) {

        VersionBitsTester().TestDefined().Mine(1, TestTime(1), 0x100).TestDefined().Mine(11, TestTime(11), 0x100).TestDefined().Mine(989, TestTime(989), 0x100).TestDefined().Mine(999, TestTime(20000), 0x100).TestDefined().Mine(1000, TestTime(20000), 0x100).TestFailed().Mine(1999, TestTime(30001), 0x100).TestFailed().Mine(2000, TestTime(30002), 0x100).TestFailed().Mine(2001, TestTime(30003), 0x100).TestFailed().Mine(2999, TestTime(30004), 0x100).TestFailed().Mine(3000, TestTime(30005), 0x100).TestFailed()

            .Reset()
            .TestDefined()
            .Mine(1, TestTime(1), 0)
            .TestDefined()
            .Mine(1000, TestTime(10000) - 1, 0x100)
            .TestDefined() // One second more and it would be defined
            .Mine(2000, TestTime(10000), 0x100)
            .TestStarted() // So that's what happens the next period
            .Mine(2051, TestTime(10010), 0)
            .TestStarted() // 51 old blocks
            .Mine(2950, TestTime(10020), 0x100)
            .TestStarted() // 899 new blocks
            .Mine(3000, TestTime(20000), 0)
            .TestFailed() // 50 old blocks (so 899 out of the past 1000)
            .Mine(4000, TestTime(20010), 0x100)
            .TestFailed()

            .Reset()
            .TestDefined()
            .Mine(1, TestTime(1), 0)
            .TestDefined()
            .Mine(1000, TestTime(10000) - 1, 0x101)
            .TestDefined() // One second more and it would be defined
            .Mine(2000, TestTime(10000), 0x101)
            .TestStarted() // So that's what happens the next period
            .Mine(2999, TestTime(30000), 0x100)
            .TestStarted() // 999 new blocks
            .Mine(3000, TestTime(30000), 0x100)
            .TestFailed() // 1 new block (so 1000 out of the past 1000 are new)
            .Mine(3999, TestTime(30001), 0)
            .TestFailed()
            .Mine(4000, TestTime(30002), 0)
            .TestFailed()
            .Mine(14333, TestTime(30003), 0)
            .TestFailed()
            .Mine(24000, TestTime(40000), 0)
            .TestFailed()

            .Reset()
            .TestDefined()
            .Mine(1, TestTime(1), 0)
            .TestDefined()
            .Mine(1000, TestTime(10000) - 1, 0x101)
            .TestDefined() // One second more and it would be defined
            .Mine(2000, TestTime(10000), 0x101)
            .TestStarted() // So that's what happens the next period
            .Mine(2050, TestTime(10010), 0x200)
            .TestStarted() // 50 old blocks
            .Mine(2950, TestTime(10020), 0x100)
            .TestStarted() // 900 new blocks
            .Mine(2999, TestTime(19999), 0x200)
            .TestStarted() // 49 old blocks
            .Mine(3000, TestTime(29999), 0x200)
            .TestLockedIn() // 1 old block (so 900 out of the past 1000)
            .Mine(3999, TestTime(30001), 0)
            .TestLockedIn()
            .Mine(4000, TestTime(30002), 0)
            .TestActive()
            .Mine(14333, TestTime(30003), 0)
            .TestActive()
            .Mine(24000, TestTime(40000), 0)
            .TestActive();
    }

    const Consensus::Params& mainnetParams = Params(CBaseChainParams::MAIN).GetConsensus();
    for (int i = 0; i < (int)Consensus::MAX_VERSION_BITS_DEPLOYMENTS; i++) {
        uint32_t bitmask = VersionBitsMask(mainnetParams, (Consensus::DeploymentPos)i);

        BOOST_CHECK_EQUAL(bitmask & ~(uint32_t)VERSIONBITS_TOP_MASK, bitmask);

        for (int j = i + 1; j < (int)Consensus::MAX_VERSION_BITS_DEPLOYMENTS; j++) {
            if (VersionBitsMask(mainnetParams, (Consensus::DeploymentPos)j) == bitmask) {
                BOOST_CHECK(mainnetParams.vDeployments[j].nStartTime > mainnetParams.vDeployments[i].nTimeout || mainnetParams.vDeployments[i].nStartTime > mainnetParams.vDeployments[j].nTimeout);
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(versionbits_computeblockversion)
{

    const Consensus::Params& mainnetParams = Params(CBaseChainParams::MAIN).GetConsensus();

    int64_t bit = mainnetParams.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit;
    int64_t nStartTime = mainnetParams.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime;
    int64_t nTimeout = mainnetParams.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout;

    assert(nStartTime < nTimeout);

    VersionBitsTester firstChain, secondChain;

    int64_t nTime = nStartTime - 1;

    CBlockIndex* lastBlock = NULL;
    lastBlock = firstChain.Mine(2016, nTime, VERSIONBITS_LAST_OLD_BLOCK_VERSION).Tip();
    BOOST_CHECK_EQUAL(ComputeBlockVersion(lastBlock, mainnetParams) & (1 << bit), 0);

    for (int i = 1; i < 2012; i++) {
        lastBlock = firstChain.Mine(2016 + i, nTime, VERSIONBITS_LAST_OLD_BLOCK_VERSION).Tip();

        BOOST_CHECK_EQUAL(ComputeBlockVersion(lastBlock, mainnetParams) & (1 << bit), 0);
    }

    nTime = nStartTime;
    for (int i = 2012; i <= 2016; i++) {
        lastBlock = firstChain.Mine(2016 + i, nTime, VERSIONBITS_LAST_OLD_BLOCK_VERSION).Tip();
        BOOST_CHECK_EQUAL(ComputeBlockVersion(lastBlock, mainnetParams) & (1 << bit), 0);
    }

    lastBlock = firstChain.Mine(6048, nTime, VERSIONBITS_LAST_OLD_BLOCK_VERSION).Tip();

    BOOST_CHECK((ComputeBlockVersion(lastBlock, mainnetParams) & (1 << bit)) != 0);

    BOOST_CHECK_EQUAL(ComputeBlockVersion(lastBlock, mainnetParams) & VERSIONBITS_TOP_MASK, VERSIONBITS_TOP_BITS);

    nTime += 600;
    int blocksToMine = 4032; // test blocks for up to 2 time periods
    int nHeight = 6048;

    while (nTime < nTimeout && blocksToMine > 0) {
        lastBlock = firstChain.Mine(nHeight + 1, nTime, VERSIONBITS_LAST_OLD_BLOCK_VERSION).Tip();
        BOOST_CHECK((ComputeBlockVersion(lastBlock, mainnetParams) & (1 << bit)) != 0);
        BOOST_CHECK_EQUAL(ComputeBlockVersion(lastBlock, mainnetParams) & VERSIONBITS_TOP_MASK, VERSIONBITS_TOP_BITS);
        blocksToMine--;
        nTime += 600;
        nHeight += 1;
    };

    nTime = nTimeout;

    for (int i = 0; i < 2015; i++) {
        lastBlock = firstChain.Mine(nHeight + 1, nTime, VERSIONBITS_LAST_OLD_BLOCK_VERSION).Tip();
        BOOST_CHECK((ComputeBlockVersion(lastBlock, mainnetParams) & (1 << bit)) != 0);
        nHeight += 1;
    }

    lastBlock = firstChain.Mine(nHeight + 1, nTime, VERSIONBITS_LAST_OLD_BLOCK_VERSION).Tip();
    BOOST_CHECK_EQUAL(ComputeBlockVersion(lastBlock, mainnetParams) & (1 << bit), 0);

    nTime = nStartTime;

    lastBlock = secondChain.Mine(2016, nStartTime, VERSIONBITS_LAST_OLD_BLOCK_VERSION).Tip();
    BOOST_CHECK((ComputeBlockVersion(lastBlock, mainnetParams) & (1 << bit)) != 0);

    lastBlock = secondChain.Mine(4032, nStartTime, VERSIONBITS_TOP_BITS | (1 << bit)).Tip();

    BOOST_CHECK((ComputeBlockVersion(lastBlock, mainnetParams) & (1 << bit)) != 0);

    lastBlock = secondChain.Mine(6047, nStartTime, VERSIONBITS_LAST_OLD_BLOCK_VERSION).Tip();
    BOOST_CHECK((ComputeBlockVersion(lastBlock, mainnetParams) & (1 << bit)) != 0);
    lastBlock = secondChain.Mine(6048, nStartTime, VERSIONBITS_LAST_OLD_BLOCK_VERSION).Tip();
    BOOST_CHECK_EQUAL(ComputeBlockVersion(lastBlock, mainnetParams) & (1 << bit), 0);
}

BOOST_AUTO_TEST_SUITE_END()
