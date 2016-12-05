// Copyright (c) 2014-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "coins.h"
#include "random.h"
#include "script/standard.h"
#include "uint256.h"
#include "utilstrencodings.h"
#include "test/test_bitcoin.h"
#include "main.h"
#include "consensus/validation.h"

#include <vector>
#include <map>

#include <boost/test/unit_test.hpp>

namespace {
class CCoinsViewTest : public CCoinsView {
    uint256 hashBestBlock_;
    std::map<uint256, CCoins> map_;

public:
    bool GetCoins(const uint256& txid, CCoins& coins) const
    {
        std::map<uint256, CCoins>::const_iterator it = map_.find(txid);
        if (it == map_.end()) {
            return false;
        }
        coins = it->second;
        if (coins.IsPruned() && insecure_rand() % 2 == 0) {

            return false;
        }
        return true;
    }

    bool HaveCoins(const uint256& txid) const
    {
        CCoins coins;
        return GetCoins(txid, coins);
    }

    uint256 GetBestBlock() const { return hashBestBlock_; }

    bool BatchWrite(CCoinsMap& mapCoins, const uint256& hashBlock)
    {
        for (CCoinsMap::iterator it = mapCoins.begin(); it != mapCoins.end();) {
            if (it->second.flags & CCoinsCacheEntry::DIRTY) {

                map_[it->first] = it->second.coins;
                if (it->second.coins.IsPruned() && insecure_rand() % 3 == 0) {

                    map_.erase(it->first);
                }
            }
            mapCoins.erase(it++);
        }
        if (!hashBlock.IsNull())
            hashBestBlock_ = hashBlock;
        return true;
    }
};

class CCoinsViewCacheTest : public CCoinsViewCache {
public:
    CCoinsViewCacheTest(CCoinsView* base)
        : CCoinsViewCache(base)
    {
    }

    void SelfTest() const
    {

        size_t ret = memusage::DynamicUsage(cacheCoins);
        for (CCoinsMap::iterator it = cacheCoins.begin(); it != cacheCoins.end(); it++) {
            ret += it->second.coins.DynamicMemoryUsage();
        }
        BOOST_CHECK_EQUAL(DynamicMemoryUsage(), ret);
    }
};
}

BOOST_FIXTURE_TEST_SUITE(coins_tests, BasicTestingSetup)

static const unsigned int NUM_SIMULATION_ITERATIONS = 40000;

BOOST_AUTO_TEST_CASE(coins_cache_simulation_test)
{

    bool removed_all_caches = false;
    bool reached_4_caches = false;
    bool added_an_entry = false;
    bool removed_an_entry = false;
    bool updated_an_entry = false;
    bool found_an_entry = false;
    bool missed_an_entry = false;

    std::map<uint256, CCoins> result;

    CCoinsViewTest base; // A CCoinsViewTest at the bottom.
    std::vector<CCoinsViewCacheTest*> stack; // A stack of CCoinsViewCaches on top.
    stack.push_back(new CCoinsViewCacheTest(&base)); // Start with one cache.

    std::vector<uint256> txids;
    txids.resize(NUM_SIMULATION_ITERATIONS / 8);
    for (unsigned int i = 0; i < txids.size(); i++) {
        txids[i] = GetRandHash();
    }

    for (unsigned int i = 0; i < NUM_SIMULATION_ITERATIONS; i++) {

        {
            uint256 txid = txids[insecure_rand() % txids.size()]; // txid we're going to modify in this iteration.
            CCoins& coins = result[txid];
            CCoinsModifier entry = stack.back()->ModifyCoins(txid);
            BOOST_CHECK(coins == *entry);
            if (insecure_rand() % 5 == 0 || coins.IsPruned()) {
                if (coins.IsPruned()) {
                    added_an_entry = true;
                } else {
                    updated_an_entry = true;
                }
                coins.nVersion = insecure_rand();
                coins.vout.resize(1);
                coins.vout[0].nValue = insecure_rand();
                *entry = coins;
            } else {
                coins.Clear();
                entry->Clear();
                removed_an_entry = true;
            }
        }

        if (insecure_rand() % 1000 == 1 || i == NUM_SIMULATION_ITERATIONS - 1) {
            for (std::map<uint256, CCoins>::iterator it = result.begin(); it != result.end(); it++) {
                const CCoins* coins = stack.back()->AccessCoins(it->first);
                if (coins) {
                    BOOST_CHECK(*coins == it->second);
                    found_an_entry = true;
                } else {
                    BOOST_CHECK(it->second.IsPruned());
                    missed_an_entry = true;
                }
            }
            BOOST_FOREACH (const CCoinsViewCacheTest* test, stack) {
                test->SelfTest();
            }
        }

        if (insecure_rand() % 100 == 0) {

            if (stack.size() > 1 && insecure_rand() % 2 == 0) {
                unsigned int flushIndex = insecure_rand() % (stack.size() - 1);
                stack[flushIndex]->Flush();
            }
        }
        if (insecure_rand() % 100 == 0) {

            if (stack.size() > 0 && insecure_rand() % 2 == 0) {

                stack.back()->Flush();
                delete stack.back();
                stack.pop_back();
            }
            if (stack.size() == 0 || (stack.size() < 4 && insecure_rand() % 2)) {

                CCoinsView* tip = &base;
                if (stack.size() > 0) {
                    tip = stack.back();
                } else {
                    removed_all_caches = true;
                }
                stack.push_back(new CCoinsViewCacheTest(tip));
                if (stack.size() == 4) {
                    reached_4_caches = true;
                }
            }
        }
    }

    while (stack.size() > 0) {
        delete stack.back();
        stack.pop_back();
    }

    BOOST_CHECK(removed_all_caches);
    BOOST_CHECK(reached_4_caches);
    BOOST_CHECK(added_an_entry);
    BOOST_CHECK(removed_an_entry);
    BOOST_CHECK(updated_an_entry);
    BOOST_CHECK(found_an_entry);
    BOOST_CHECK(missed_an_entry);
}

BOOST_AUTO_TEST_CASE(updatecoins_simulation_test)
{
    bool spent_a_duplicate_coinbase = false;

    std::map<uint256, CCoins> result;

    CCoinsViewTest base; // A CCoinsViewTest at the bottom.
    std::vector<CCoinsViewCacheTest*> stack; // A stack of CCoinsViewCaches on top.
    stack.push_back(new CCoinsViewCacheTest(&base)); // Start with one cache.

    std::map<uint256, CAmount> coinbaseids;
    std::set<uint256> alltxids;
    std::set<uint256> duplicateids;

    for (unsigned int i = 0; i < NUM_SIMULATION_ITERATIONS; i++) {
        {
            CMutableTransaction tx;
            tx.vin.resize(1);
            tx.vout.resize(1);
            tx.vout[0].nValue = i; //Keep txs unique unless intended to duplicate
            unsigned int height = insecure_rand();

            if (insecure_rand() % 10 == 0 || coinbaseids.size() < 10) {

                if (insecure_rand() % 10 == 0 && coinbaseids.size()) {
                    std::map<uint256, CAmount>::iterator coinbaseIt = coinbaseids.lower_bound(GetRandHash());
                    if (coinbaseIt == coinbaseids.end()) {
                        coinbaseIt = coinbaseids.begin();
                    }

                    tx.vout[0].nValue = coinbaseIt->second;
                    assert(tx.GetHash() == coinbaseIt->first);
                    duplicateids.insert(coinbaseIt->first);
                } else {
                    coinbaseids[tx.GetHash()] = tx.vout[0].nValue;
                }
                assert(CTransaction(tx).IsCoinBase());
            }

            else {
                uint256 prevouthash;

                std::set<uint256>::iterator txIt = alltxids.lower_bound(GetRandHash());
                if (txIt == alltxids.end()) {
                    txIt = alltxids.begin();
                }
                prevouthash = *txIt;

                tx.vin[0].prevout.hash = prevouthash;
                tx.vin[0].prevout.n = 0;

                CCoins& oldcoins = result[prevouthash];
                oldcoins.Clear();

                alltxids.erase(prevouthash);
                coinbaseids.erase(prevouthash);

                if (duplicateids.count(prevouthash))
                    spent_a_duplicate_coinbase = true;

                assert(!CTransaction(tx).IsCoinBase());
            }

            alltxids.insert(tx.GetHash());

            CCoins& coins = result[tx.GetHash()];
            coins.FromTx(tx, height);

            UpdateCoins(tx, *(stack.back()), height);
        }

        if (insecure_rand() % 1000 == 1 || i == NUM_SIMULATION_ITERATIONS - 1) {
            for (std::map<uint256, CCoins>::iterator it = result.begin(); it != result.end(); it++) {
                const CCoins* coins = stack.back()->AccessCoins(it->first);
                if (coins) {
                    BOOST_CHECK(*coins == it->second);
                } else {
                    BOOST_CHECK(it->second.IsPruned());
                }
            }
        }

        if (insecure_rand() % 100 == 0) {

            if (stack.size() > 1 && insecure_rand() % 2 == 0) {
                unsigned int flushIndex = insecure_rand() % (stack.size() - 1);
                stack[flushIndex]->Flush();
            }
        }
        if (insecure_rand() % 100 == 0) {

            if (stack.size() > 0 && insecure_rand() % 2 == 0) {
                stack.back()->Flush();
                delete stack.back();
                stack.pop_back();
            }
            if (stack.size() == 0 || (stack.size() < 4 && insecure_rand() % 2)) {
                CCoinsView* tip = &base;
                if (stack.size() > 0) {
                    tip = stack.back();
                }
                stack.push_back(new CCoinsViewCacheTest(tip));
            }
        }
    }

    while (stack.size() > 0) {
        delete stack.back();
        stack.pop_back();
    }

    BOOST_CHECK(spent_a_duplicate_coinbase);
}

BOOST_AUTO_TEST_CASE(ccoins_serialization)
{

    CDataStream ss1(ParseHex("0104835800816115944e077fe7c803cfa57f29b36bf87c1d358bb85e"), SER_DISK, CLIENT_VERSION);
    CCoins cc1;
    ss1 >> cc1;
    BOOST_CHECK_EQUAL(cc1.nVersion, 1);
    BOOST_CHECK_EQUAL(cc1.fCoinBase, false);
    BOOST_CHECK_EQUAL(cc1.nHeight, 203998);
    BOOST_CHECK_EQUAL(cc1.vout.size(), 2);
    BOOST_CHECK_EQUAL(cc1.IsAvailable(0), false);
    BOOST_CHECK_EQUAL(cc1.IsAvailable(1), true);
    BOOST_CHECK_EQUAL(cc1.vout[1].nValue, 60000000000ULL);
    BOOST_CHECK_EQUAL(HexStr(cc1.vout[1].scriptPubKey), HexStr(GetScriptForDestination(CKeyID(uint160(ParseHex("816115944e077fe7c803cfa57f29b36bf87c1d35"))))));

    CDataStream ss2(ParseHex("0109044086ef97d5790061b01caab50f1b8e9c50a5057eb43c2d9563a4eebbd123008c988f1a4a4de2161e0f50aac7f17e7f9555caa486af3b"), SER_DISK, CLIENT_VERSION);
    CCoins cc2;
    ss2 >> cc2;
    BOOST_CHECK_EQUAL(cc2.nVersion, 1);
    BOOST_CHECK_EQUAL(cc2.fCoinBase, true);
    BOOST_CHECK_EQUAL(cc2.nHeight, 120891);
    BOOST_CHECK_EQUAL(cc2.vout.size(), 17);
    for (int i = 0; i < 17; i++) {
        BOOST_CHECK_EQUAL(cc2.IsAvailable(i), i == 4 || i == 16);
    }
    BOOST_CHECK_EQUAL(cc2.vout[4].nValue, 234925952);
    BOOST_CHECK_EQUAL(HexStr(cc2.vout[4].scriptPubKey), HexStr(GetScriptForDestination(CKeyID(uint160(ParseHex("61b01caab50f1b8e9c50a5057eb43c2d9563a4ee"))))));
    BOOST_CHECK_EQUAL(cc2.vout[16].nValue, 110397);
    BOOST_CHECK_EQUAL(HexStr(cc2.vout[16].scriptPubKey), HexStr(GetScriptForDestination(CKeyID(uint160(ParseHex("8c988f1a4a4de2161e0f50aac7f17e7f9555caa4"))))));

    CDataStream ssx(SER_DISK, CLIENT_VERSION);
    BOOST_CHECK_EQUAL(HexStr(ssx.begin(), ssx.end()), "");

    CDataStream ss3(ParseHex("0002000600"), SER_DISK, CLIENT_VERSION);
    CCoins cc3;
    ss3 >> cc3;
    BOOST_CHECK_EQUAL(cc3.nVersion, 0);
    BOOST_CHECK_EQUAL(cc3.fCoinBase, false);
    BOOST_CHECK_EQUAL(cc3.nHeight, 0);
    BOOST_CHECK_EQUAL(cc3.vout.size(), 1);
    BOOST_CHECK_EQUAL(cc3.IsAvailable(0), true);
    BOOST_CHECK_EQUAL(cc3.vout[0].nValue, 0);
    BOOST_CHECK_EQUAL(cc3.vout[0].scriptPubKey.size(), 0);

    CDataStream ss4(ParseHex("0002000800"), SER_DISK, CLIENT_VERSION);
    try {
        CCoins cc4;
        ss4 >> cc4;
        BOOST_CHECK_MESSAGE(false, "We should have thrown");
    }
    catch (const std::ios_base::failure& e) {
    }

    CDataStream tmp(SER_DISK, CLIENT_VERSION);
    uint64_t x = 3000000000ULL;
    tmp << VARINT(x);
    BOOST_CHECK_EQUAL(HexStr(tmp.begin(), tmp.end()), "8a95c0bb00");
    CDataStream ss5(ParseHex("0002008a95c0bb0000"), SER_DISK, CLIENT_VERSION);
    try {
        CCoins cc5;
        ss5 >> cc5;
        BOOST_CHECK_MESSAGE(false, "We should have thrown");
    }
    catch (const std::ios_base::failure& e) {
    }
}

BOOST_AUTO_TEST_SUITE_END()
