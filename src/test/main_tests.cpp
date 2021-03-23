// Copyright (c) 2014-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "chainparams.h"
#include "validation/validation.h"
#include "net.h"
#include "unity/signals.h"

#include "test/test.h"

#include <boost/signals2/signal.hpp>
#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(main_tests, TestingSetup)

BOOST_AUTO_TEST_CASE(block_subsidy_test)
{
    const auto chainParams = CreateChainParams(CBaseChainParams::MAIN);
    uint64_t nP4First = chainParams->GetConsensus().pow2Phase4FirstBlockHeight;
    BOOST_CHECK_EQUAL(GetBlockSubsidy(0),             (994'744'000*COIN) + (10*CENT));
    BOOST_CHECK_EQUAL(GetBlockSubsidy(1000),          10*CENT*2);
    BOOST_CHECK_EQUAL(GetBlockSubsidyWitness(1000),   10*CENT);
    BOOST_CHECK_EQUAL(GetBlockSubsidy(99999),        10*CENT*2);
    BOOST_CHECK_EQUAL(GetBlockSubsidyWitness(99999), 10*CENT);
    BOOST_CHECK_EQUAL(GetBlockSubsidy(100000),        (10*CENT));
    BOOST_CHECK_EQUAL(GetBlockSubsidyWitness(100000), (75*CENT/10));
    BOOST_CHECK_EQUAL(GetBlockSubsidy(100001),        (10*CENT));
    BOOST_CHECK_EQUAL(GetBlockSubsidyWitness(100001), (75*CENT/10));
    BOOST_CHECK_EQUAL(GetBlockSubsidy(499999),        (10*CENT));
    BOOST_CHECK_EQUAL(GetBlockSubsidy(500000),        (10*CENT)/2);
    BOOST_CHECK_EQUAL(GetBlockSubsidy(500001),        (10*CENT)/2);
    BOOST_CHECK_EQUAL(GetBlockSubsidy(899999),        (10*CENT)/2);
    BOOST_CHECK_EQUAL(GetBlockSubsidy(900000),        (10*CENT)/2/2);
    BOOST_CHECK_EQUAL(GetBlockSubsidy(900001),        (10*CENT)/2/2);
    BOOST_CHECK_EQUAL(GetBlockSubsidy(1299999),       (10*CENT)/2/2);
    BOOST_CHECK_EQUAL(GetBlockSubsidy(1300000),       (10*CENT)/2/2/2);
    BOOST_CHECK_EQUAL(GetBlockSubsidy(1300001),       (10*CENT)/2/2/2);
    BOOST_CHECK_EQUAL(GetBlockSubsidy(9300001),       0);
    BOOST_CHECK_EQUAL(GetBlockSubsidy(12000000),      0);
    BOOST_CHECK_EQUAL(GetBlockSubsidy(42000000),      0);
    
}


BOOST_AUTO_TEST_CASE(subsidy_limit_test)
{
    const auto chainParams = CreateChainParams(CBaseChainParams::MAIN);
    CAmount nSumTotal = 0;
    CAmount nSumTotalMining = 0;
    CAmount nSumTotalWitness = 0;
    for (int nHeight = 0; nHeight < 9300001; nHeight++)
    {
        CAmount nSubsidy = GetBlockSubsidy(nHeight);
        nSumTotal += nSubsidy;
        BOOST_CHECK(MoneyRange(nSumTotal));
        CAmount nSubsidyWitness = GetBlockSubsidyWitness(nHeight);
        nSumTotalWitness += nSubsidyWitness;
        BOOST_CHECK(MoneyRange(nSumTotalWitness));
    }
    BOOST_CHECK_EQUAL(nSumTotal, 99484399982800000LL);
    BOOST_CHECK_EQUAL(nSumTotalWitness, 6999996000000LL);
}

bool ReturnFalse() { return false; }
bool ReturnTrue() { return true; }

BOOST_AUTO_TEST_CASE(test_combiner_all)
{
    boost::signals2::signal<bool (), BooleanAndAllReturnValues> Test;
    BOOST_CHECK(Test());
    Test.connect(&ReturnFalse);
    BOOST_CHECK(!Test());
    Test.connect(&ReturnTrue);
    BOOST_CHECK(!Test());
    Test.disconnect(&ReturnFalse);
    BOOST_CHECK(Test());
    Test.disconnect(&ReturnTrue);
    BOOST_CHECK(Test());
}
BOOST_AUTO_TEST_SUITE_END()
