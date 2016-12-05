// Copyright (c) 2012-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "wallet/wallet.h"

#include <set>
#include <stdint.h>
#include <utility>
#include <vector>

#include "wallet/test/wallet_test_fixture.h"

#include <boost/foreach.hpp>
#include <boost/test/unit_test.hpp>

// how many times to run all the tests to have a chance to catch errors that only show up with particular random shuffles
#define RUN_TESTS 100

// some tests fail 1% of the time due to bad luck.
// we repeat those tests this many times and only complain if all iterations of the test fail
#define RANDOM_REPEATS 5

using namespace std;

typedef set<pair<const CWalletTx*, unsigned int> > CoinSet;

BOOST_FIXTURE_TEST_SUITE(wallet_tests, WalletTestingSetup)

static const CWallet wallet;
static vector<COutput> vCoins;

static void add_coin(const CAmount& nValue, int nAge = 6 * 24, bool fIsFromMe = false, int nInput = 0)
{
    static int nextLockTime = 0;
    CMutableTransaction tx;
    tx.nLockTime = nextLockTime++; // so all transactions get different hashes
    tx.vout.resize(nInput + 1);
    tx.vout[nInput].nValue = nValue;
    if (fIsFromMe) {

        tx.vin.resize(1);
    }
    CWalletTx* wtx = new CWalletTx(&wallet, tx);
    if (fIsFromMe) {
        wtx->fDebitCached = true;
        wtx->nDebitCached = 1;
    }
    COutput output(wtx, nInput, nAge, true, true);
    vCoins.push_back(output);
}

static void empty_wallet(void)
{
    BOOST_FOREACH (COutput output, vCoins)
        delete output.tx;
    vCoins.clear();
}

static bool equal_sets(CoinSet a, CoinSet b)
{
    pair<CoinSet::iterator, CoinSet::iterator> ret = mismatch(a.begin(), a.end(), b.begin());
    return ret.first == a.end() && ret.second == b.end();
}

BOOST_AUTO_TEST_CASE(coin_selection_tests)
{
    CoinSet setCoinsRet, setCoinsRet2;
    CAmount nValueRet;

    LOCK(wallet.cs_wallet);

    for (int i = 0; i < RUN_TESTS; i++) {
        empty_wallet();

        BOOST_CHECK(!wallet.SelectCoinsMinConf(1 * CENT, 1, 6, vCoins, setCoinsRet, nValueRet));

        add_coin(1 * CENT, 4); // add a new 1 cent coin

        BOOST_CHECK(!wallet.SelectCoinsMinConf(1 * CENT, 1, 6, vCoins, setCoinsRet, nValueRet));

        BOOST_CHECK(wallet.SelectCoinsMinConf(1 * CENT, 1, 1, vCoins, setCoinsRet, nValueRet));
        BOOST_CHECK_EQUAL(nValueRet, 1 * CENT);

        add_coin(2 * CENT); // add a mature 2 cent coin

        BOOST_CHECK(!wallet.SelectCoinsMinConf(3 * CENT, 1, 6, vCoins, setCoinsRet, nValueRet));

        BOOST_CHECK(wallet.SelectCoinsMinConf(3 * CENT, 1, 1, vCoins, setCoinsRet, nValueRet));
        BOOST_CHECK_EQUAL(nValueRet, 3 * CENT);

        add_coin(5 * CENT); // add a mature 5 cent coin,
        add_coin(10 * CENT, 3, true); // a new 10 cent coin sent from one of our own addresses
        add_coin(20 * CENT); // and a mature 20 cent coin

        BOOST_CHECK(!wallet.SelectCoinsMinConf(38 * CENT, 1, 6, vCoins, setCoinsRet, nValueRet));

        BOOST_CHECK(!wallet.SelectCoinsMinConf(38 * CENT, 6, 6, vCoins, setCoinsRet, nValueRet));

        BOOST_CHECK(wallet.SelectCoinsMinConf(37 * CENT, 1, 6, vCoins, setCoinsRet, nValueRet));
        BOOST_CHECK_EQUAL(nValueRet, 37 * CENT);

        BOOST_CHECK(wallet.SelectCoinsMinConf(38 * CENT, 1, 1, vCoins, setCoinsRet, nValueRet));
        BOOST_CHECK_EQUAL(nValueRet, 38 * CENT);

        BOOST_CHECK(wallet.SelectCoinsMinConf(34 * CENT, 1, 1, vCoins, setCoinsRet, nValueRet));
        BOOST_CHECK_EQUAL(nValueRet, 35 * CENT); // but 35 cents is closest
        BOOST_CHECK_EQUAL(setCoinsRet.size(), 3U); // the best should be 20+10+5.  it's incredibly unlikely the 1 or 2 got included (but possible)

        BOOST_CHECK(wallet.SelectCoinsMinConf(7 * CENT, 1, 1, vCoins, setCoinsRet, nValueRet));
        BOOST_CHECK_EQUAL(nValueRet, 7 * CENT);
        BOOST_CHECK_EQUAL(setCoinsRet.size(), 2U);

        BOOST_CHECK(wallet.SelectCoinsMinConf(8 * CENT, 1, 1, vCoins, setCoinsRet, nValueRet));
        BOOST_CHECK(nValueRet == 8 * CENT);
        BOOST_CHECK_EQUAL(setCoinsRet.size(), 3U);

        BOOST_CHECK(wallet.SelectCoinsMinConf(9 * CENT, 1, 1, vCoins, setCoinsRet, nValueRet));
        BOOST_CHECK_EQUAL(nValueRet, 10 * CENT);
        BOOST_CHECK_EQUAL(setCoinsRet.size(), 1U);

        empty_wallet();

        add_coin(6 * CENT);
        add_coin(7 * CENT);
        add_coin(8 * CENT);
        add_coin(20 * CENT);
        add_coin(30 * CENT); // now we have 6+7+8+20+30 = 71 cents total

        BOOST_CHECK(wallet.SelectCoinsMinConf(71 * CENT, 1, 1, vCoins, setCoinsRet, nValueRet));
        BOOST_CHECK(!wallet.SelectCoinsMinConf(72 * CENT, 1, 1, vCoins, setCoinsRet, nValueRet));

        BOOST_CHECK(wallet.SelectCoinsMinConf(16 * CENT, 1, 1, vCoins, setCoinsRet, nValueRet));
        BOOST_CHECK_EQUAL(nValueRet, 20 * CENT); // we should get 20 in one coin
        BOOST_CHECK_EQUAL(setCoinsRet.size(), 1U);

        add_coin(5 * CENT); // now we have 5+6+7+8+20+30 = 75 cents total

        BOOST_CHECK(wallet.SelectCoinsMinConf(16 * CENT, 1, 1, vCoins, setCoinsRet, nValueRet));
        BOOST_CHECK_EQUAL(nValueRet, 18 * CENT); // we should get 18 in 3 coins
        BOOST_CHECK_EQUAL(setCoinsRet.size(), 3U);

        add_coin(18 * CENT); // now we have 5+6+7+8+18+20+30

        BOOST_CHECK(wallet.SelectCoinsMinConf(16 * CENT, 1, 1, vCoins, setCoinsRet, nValueRet));
        BOOST_CHECK_EQUAL(nValueRet, 18 * CENT); // we should get 18 in 1 coin
        BOOST_CHECK_EQUAL(setCoinsRet.size(), 1U); // because in the event of a tie, the biggest coin wins

        BOOST_CHECK(wallet.SelectCoinsMinConf(11 * CENT, 1, 1, vCoins, setCoinsRet, nValueRet));
        BOOST_CHECK_EQUAL(nValueRet, 11 * CENT);
        BOOST_CHECK_EQUAL(setCoinsRet.size(), 2U);

        add_coin(1 * COIN);
        add_coin(2 * COIN);
        add_coin(3 * COIN);
        add_coin(4 * COIN); // now we have 5+6+7+8+18+20+30+100+200+300+400 = 1094 cents
        BOOST_CHECK(wallet.SelectCoinsMinConf(95 * CENT, 1, 1, vCoins, setCoinsRet, nValueRet));
        BOOST_CHECK_EQUAL(nValueRet, 1 * COIN); // we should get 1 BTC in 1 coin
        BOOST_CHECK_EQUAL(setCoinsRet.size(), 1U);

        BOOST_CHECK(wallet.SelectCoinsMinConf(195 * CENT, 1, 1, vCoins, setCoinsRet, nValueRet));
        BOOST_CHECK_EQUAL(nValueRet, 2 * COIN); // we should get 2 BTC in 1 coin
        BOOST_CHECK_EQUAL(setCoinsRet.size(), 1U);

        empty_wallet();
        add_coin(MIN_CHANGE * 1 / 10);
        add_coin(MIN_CHANGE * 2 / 10);
        add_coin(MIN_CHANGE * 3 / 10);
        add_coin(MIN_CHANGE * 4 / 10);
        add_coin(MIN_CHANGE * 5 / 10);

        BOOST_CHECK(wallet.SelectCoinsMinConf(MIN_CHANGE, 1, 1, vCoins, setCoinsRet, nValueRet));
        BOOST_CHECK_EQUAL(nValueRet, MIN_CHANGE);

        add_coin(1111 * MIN_CHANGE);

        BOOST_CHECK(wallet.SelectCoinsMinConf(1 * MIN_CHANGE, 1, 1, vCoins, setCoinsRet, nValueRet));
        BOOST_CHECK_EQUAL(nValueRet, 1 * MIN_CHANGE); // we should get the exact amount

        add_coin(MIN_CHANGE * 6 / 10);
        add_coin(MIN_CHANGE * 7 / 10);

        BOOST_CHECK(wallet.SelectCoinsMinConf(1 * MIN_CHANGE, 1, 1, vCoins, setCoinsRet, nValueRet));
        BOOST_CHECK_EQUAL(nValueRet, 1 * MIN_CHANGE); // we should get the exact amount

        empty_wallet();
        for (int i = 0; i < 20; i++)
            add_coin(50000 * COIN);

        BOOST_CHECK(wallet.SelectCoinsMinConf(500000 * COIN, 1, 1, vCoins, setCoinsRet, nValueRet));
        BOOST_CHECK_EQUAL(nValueRet, 500000 * COIN); // we should get the exact amount
        BOOST_CHECK_EQUAL(setCoinsRet.size(), 10U); // in ten coins

        empty_wallet();
        add_coin(MIN_CHANGE * 5 / 10);
        add_coin(MIN_CHANGE * 6 / 10);
        add_coin(MIN_CHANGE * 7 / 10);
        add_coin(1111 * MIN_CHANGE);
        BOOST_CHECK(wallet.SelectCoinsMinConf(1 * MIN_CHANGE, 1, 1, vCoins, setCoinsRet, nValueRet));
        BOOST_CHECK_EQUAL(nValueRet, 1111 * MIN_CHANGE); // we get the bigger coin
        BOOST_CHECK_EQUAL(setCoinsRet.size(), 1U);

        empty_wallet();
        add_coin(MIN_CHANGE * 4 / 10);
        add_coin(MIN_CHANGE * 6 / 10);
        add_coin(MIN_CHANGE * 8 / 10);
        add_coin(1111 * MIN_CHANGE);
        BOOST_CHECK(wallet.SelectCoinsMinConf(MIN_CHANGE, 1, 1, vCoins, setCoinsRet, nValueRet));
        BOOST_CHECK_EQUAL(nValueRet, MIN_CHANGE); // we should get the exact amount
        BOOST_CHECK_EQUAL(setCoinsRet.size(), 2U); // in two coins 0.4+0.6

        empty_wallet();
        add_coin(MIN_CHANGE * 5 / 100);
        add_coin(MIN_CHANGE * 1);
        add_coin(MIN_CHANGE * 100);

        BOOST_CHECK(wallet.SelectCoinsMinConf(MIN_CHANGE * 10001 / 100, 1, 1, vCoins, setCoinsRet, nValueRet));
        BOOST_CHECK_EQUAL(nValueRet, MIN_CHANGE * 10105 / 100); // we should get all coins
        BOOST_CHECK_EQUAL(setCoinsRet.size(), 3U);

        BOOST_CHECK(wallet.SelectCoinsMinConf(MIN_CHANGE * 9990 / 100, 1, 1, vCoins, setCoinsRet, nValueRet));
        BOOST_CHECK_EQUAL(nValueRet, 101 * MIN_CHANGE);
        BOOST_CHECK_EQUAL(setCoinsRet.size(), 2U);

        for (CAmount amt = 1500; amt < COIN; amt *= 10) {
            empty_wallet();

            for (uint16_t j = 0; j < 676; j++)
                add_coin(amt);
            BOOST_CHECK(wallet.SelectCoinsMinConf(2000, 1, 1, vCoins, setCoinsRet, nValueRet));
            if (amt - 2000 < MIN_CHANGE) {

                uint16_t returnSize = std::ceil((2000.0 + MIN_CHANGE) / amt);
                CAmount returnValue = amt * returnSize;
                BOOST_CHECK_EQUAL(nValueRet, returnValue);
                BOOST_CHECK_EQUAL(setCoinsRet.size(), returnSize);
            } else {

                BOOST_CHECK_EQUAL(nValueRet, amt);
                BOOST_CHECK_EQUAL(setCoinsRet.size(), 1U);
            }
        }

        {
            empty_wallet();
            for (int i2 = 0; i2 < 100; i2++)
                add_coin(COIN);

            BOOST_CHECK(wallet.SelectCoinsMinConf(50 * COIN, 1, 6, vCoins, setCoinsRet, nValueRet));
            BOOST_CHECK(wallet.SelectCoinsMinConf(50 * COIN, 1, 6, vCoins, setCoinsRet2, nValueRet));
            BOOST_CHECK(!equal_sets(setCoinsRet, setCoinsRet2));

            int fails = 0;
            for (int i = 0; i < RANDOM_REPEATS; i++) {

                BOOST_CHECK(wallet.SelectCoinsMinConf(COIN, 1, 6, vCoins, setCoinsRet, nValueRet));
                BOOST_CHECK(wallet.SelectCoinsMinConf(COIN, 1, 6, vCoins, setCoinsRet2, nValueRet));
                if (equal_sets(setCoinsRet, setCoinsRet2))
                    fails++;
            }
            BOOST_CHECK_NE(fails, RANDOM_REPEATS);

            add_coin(5 * CENT);
            add_coin(10 * CENT);
            add_coin(15 * CENT);
            add_coin(20 * CENT);
            add_coin(25 * CENT);

            fails = 0;
            for (int i = 0; i < RANDOM_REPEATS; i++) {

                BOOST_CHECK(wallet.SelectCoinsMinConf(90 * CENT, 1, 6, vCoins, setCoinsRet, nValueRet));
                BOOST_CHECK(wallet.SelectCoinsMinConf(90 * CENT, 1, 6, vCoins, setCoinsRet2, nValueRet));
                if (equal_sets(setCoinsRet, setCoinsRet2))
                    fails++;
            }
            BOOST_CHECK_NE(fails, RANDOM_REPEATS);
        }
    }
    empty_wallet();
}

BOOST_AUTO_TEST_CASE(ApproximateBestSubset)
{
    CoinSet setCoinsRet;
    CAmount nValueRet;

    LOCK(wallet.cs_wallet);

    empty_wallet();

    for (int i = 0; i < 1000; i++)
        add_coin(1000 * COIN);
    add_coin(3 * COIN);

    BOOST_CHECK(wallet.SelectCoinsMinConf(1003 * COIN, 1, 6, vCoins, setCoinsRet, nValueRet));
    BOOST_CHECK_EQUAL(nValueRet, 1003 * COIN);
    BOOST_CHECK_EQUAL(setCoinsRet.size(), 2U);
}

BOOST_AUTO_TEST_SUITE_END()
