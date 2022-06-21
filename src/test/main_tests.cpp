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
    BOOST_CHECK_EQUAL(GetBlockSubsidy(1).mining,                                                                     COIN * 170000000);
    BOOST_CHECK_EQUAL(GetBlockSubsidy(250000).mining,                                                                COIN * 1000);
    BOOST_CHECK_EQUAL(GetBlockSubsidy(250001).mining,                                                                COIN * 100);
    BOOST_CHECK_EQUAL(GetBlockSubsidy(1030001).total,                                                                COIN * 110);
    BOOST_CHECK_EQUAL(GetBlockSubsidy(nP4First).total,                                                               COIN * 110);
    BOOST_CHECK_EQUAL(GetBlockSubsidy(nP4First+1).total,                                                             COIN * 120);
    BOOST_CHECK_EQUAL(GetBlockSubsidy(1226651).total,                                                                COIN * 120);
    BOOST_CHECK_EQUAL(GetBlockSubsidy(1226652).total,                                                                COIN * 200);
    BOOST_CHECK_EQUAL(GetBlockSubsidy(1226652).total-GetBlockSubsidy(1226652).dev-GetBlockSubsidy(1226652).witness,  COIN * 90);
    BOOST_CHECK_EQUAL(GetBlockSubsidy(1226652).mining,                                                               COIN * 90);
    BOOST_CHECK_EQUAL(GetBlockSubsidy(1226652).dev,                                                                  COIN * 80);
    BOOST_CHECK_EQUAL(GetBlockSubsidy(1226652).witness,                                                              COIN * 30);
    BOOST_CHECK_EQUAL(GetBlockSubsidy(1228003).total,                                                                COIN * 200);
    BOOST_CHECK_EQUAL(GetBlockSubsidy(1228004).total,                                                                COIN * 160);
    BOOST_CHECK_EQUAL(GetBlockSubsidy(1228004).total-GetBlockSubsidy(1228004).dev-GetBlockSubsidy(1228004).witness,  COIN * 50);
    BOOST_CHECK_EQUAL(GetBlockSubsidy(1228004).mining,                                                               COIN * 50);
    BOOST_CHECK_EQUAL(GetBlockSubsidy(1228004).dev,                                                                  COIN * 80);
    BOOST_CHECK_EQUAL(GetBlockSubsidy(1228004).witness,                                                              COIN * 30);
    BOOST_CHECK_EQUAL(GetBlockSubsidy(1400000).mining,                                                               COIN * 50);
    BOOST_CHECK_EQUAL(GetBlockSubsidy(1400000).dev,                                                                  COIN * 80);
    BOOST_CHECK_EQUAL(GetBlockSubsidy(1400000).witness,                                                              COIN * 30);
    BOOST_CHECK_EQUAL(GetBlockSubsidy(1400001).mining,                                                               COIN * 10);
    BOOST_CHECK_EQUAL(GetBlockSubsidy(1400001).witness,                                                              COIN * 15);
    BOOST_CHECK_EQUAL(GetBlockSubsidy(1400001).dev,                                                                  COIN * 65);
    BOOST_CHECK_EQUAL(GetBlockSubsidy(1619997).mining,                                                               COIN * 10);
    BOOST_CHECK_EQUAL(GetBlockSubsidy(1619997).witness,                                                              COIN * 15);
    BOOST_CHECK_EQUAL(GetBlockSubsidy(1619997).dev,                                                                  COIN * 100'000'000);
    BOOST_CHECK_EQUAL(GetBlockSubsidy(1619998).mining,                                                               COIN * 10);
    BOOST_CHECK_EQUAL(GetBlockSubsidy(1619998).witness,                                                              COIN * 15);
    BOOST_CHECK_EQUAL(GetBlockSubsidy(1619998).dev,                                                                  COIN * 0);
    BOOST_CHECK_EQUAL(GetBlockSubsidy(2242500).mining,                                                               COIN * 5);
    BOOST_CHECK_EQUAL(GetBlockSubsidy(2242500).witness,                                                              CENT * 750);
    BOOST_CHECK_EQUAL(GetBlockSubsidy(2242500).dev,                                                                  COIN * 0); 
    BOOST_CHECK_EQUAL(GetBlockSubsidy(2242501).mining,                                                               COIN * 5);
    BOOST_CHECK_EQUAL(GetBlockSubsidy(2242501).witness,                                                              CENT * 750);
    BOOST_CHECK_EQUAL(GetBlockSubsidy(2242501).dev,                                                                  CENT * 0);
    BOOST_CHECK_EQUAL(GetBlockSubsidy(433'009'989).mining,                                                                     0);
    BOOST_CHECK_EQUAL(GetBlockSubsidy(433'009'989).witness,                                                                     0);
    BOOST_CHECK_EQUAL(GetBlockSubsidy(433'009'989).dev,                                                                     0);
}


BOOST_AUTO_TEST_CASE(subsidy_limit_test)
{
    const auto chainParams = CreateChainParams(CBaseChainParams::MAIN);
    CAmount nSum = 0;
    for (uint64_t nHeight = 0; nHeight < Params().GetConsensus().finalSubsidyBlockHeight + 200000; nHeight++)
    {
        CAmount nSubsidy = GetBlockSubsidy(nHeight).total;
        nSum += nSubsidy;
        BOOST_CHECK(MoneyRange(nSum));
    }
    BOOST_CHECK_EQUAL(nSum, 700'000'000*COIN);
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
