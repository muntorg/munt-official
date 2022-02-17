// Copyright (c) 2014-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// File contains modifications by: The Gulden developers
// All modifications:
// Copyright (c) 2017-2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#include "coins.h"
#include "script/standard.h"
#include "uint256.h"
#include "undo.h"
#include "utilstrencodings.h"
#include "test/test.h"
#include "validation/validation.h"
#include "consensus/validation.h"

#include <vector>
#include <map>

#include <boost/test/unit_test.hpp>

int ApplyTxInUndo(CoinUndo &&undo, CCoinsViewCache &view, COutPoint out);
void UpdateCoins(const CTransaction &tx, CCoinsViewCache &inputs, CTxUndo &txundo, uint32_t nHeight, uint32_t nTxIndex);

// fixme: (PHASE5) Add additional tests for new segsig related coin features.

template <typename T>
T &operator<<(T &os, const COutPoint &op)
{
    os << op.ToString();
    return os;
}

namespace
{
    //! equality test
    bool operator==(const Coin &a, const Coin &b)
    {
        // Empty Coin objects are always equal.
        if (a.IsSpent() && b.IsSpent())
            return true;
        return a.fCoinBase == b.fCoinBase &&
               a.nHeight == b.nHeight &&
               a.out == b.out;
    }

    class CCoinsViewCacheTest : public CCoinsViewCache
    {
    public:
        CCoinsViewCacheTest(CCoinsView *_base) : CCoinsViewCache(_base) {}

        void SelfTest() const
        {
            // Manually recompute the dynamic usage of the whole data, and compare it.
            size_t ret = memusage::DynamicUsage(cacheCoins);
            size_t count = 0;
            for (CCoinsMap::iterator it = cacheCoins.begin(); it != cacheCoins.end(); it++)
            {
                ret += it->second.coin.DynamicMemoryUsage();
                ++count;
            }
            BOOST_CHECK_EQUAL(GetCacheSize(), count);
            BOOST_CHECK_EQUAL(DynamicMemoryUsage(), ret);
        }

        CCoinsMap &map() { return cacheCoins; }
        CCoinsRefMap &refmap() { return cacheCoinRefs; }
        size_t &usage() { return cachedCoinsUsage; }
    };

}

using namespace std;

struct IndexTestingSetup : public TestingSetup
{
    struct SimBlock
    {
        int height;
        std::vector<CTransaction> transactions;
        std::vector<CTxUndo> undos;

        void addCoinBaseTx()
        {
            CMutableTransaction tx(CTransaction::CURRENT_VERSION);

            // setup inputs
            tx.vin.resize(1);

            // setup outputs
            tx.vout.resize(1);
            tx.vout[0].nValue = 888;
            tx.vout[0].output.scriptPubKey.assign(InsecureRand32() & 0x3F, 0);

            transactions.push_back(tx);
        }

        void addSpendCoinBaseTxByIndex(int spendBlockHeight, int nIn)
        {
            CMutableTransaction tx(CTransaction::CURRENT_VERSION);

            // setup inputs
            tx.vin.resize(1);
            tx.vin[0] = CTxIn(COutPoint(spendBlockHeight, 0, nIn), CScript(), 0, 0); // index based

            // setup outputs
            tx.vout.resize(2);
            tx.vout[0].nValue = 550;
            tx.vout[0].output.scriptPubKey.assign(InsecureRand32() & 0x3F, 0);

            tx.vout[1].nValue = 550;
            tx.vout[1].output.scriptPubKey.assign(InsecureRand32() & 0x3F, 0);

            transactions.push_back(tx);
        }
    };

    SimBlock createBlockSpendingCoinBaseByIndex(int blockHeight, int coinBaseToSpendHeight)
    {
        SimBlock blk;
        blk.height = blockHeight;
        blk.addCoinBaseTx();
        blk.addSpendCoinBaseTxByIndex(coinBaseToSpendHeight, 0);
        return blk;
    }

    // A simple map to track what we expect the cache stack to represent.
    std::map<COutPoint, Coin> resultHashBased;
    std::map<COutPoint, COutPoint> resultIndexBased;
    uint64_t heightOffset = 100;

    vector<SimBlock> blocks;

    CCoinsViewDB &base = *pcoinsdbview; // Proper coins view db as the base
    CCoinsViewCache coinsView = CCoinsViewCache(&base);

    const int numGenesisTxOutputs = 2;

    IndexTestingSetup()
    {
        addGenesisBlock();
    }

    void addGenesisBlock()
    {
        SimBlock block;
        block.height = heightOffset;

        CMutableTransaction tx(CTransaction::CURRENT_VERSION);
        tx.vin.resize(1);
        tx.vout.resize(numGenesisTxOutputs);
        for (int outputIndex = 0; outputIndex < numGenesisTxOutputs; ++outputIndex)
        {
            tx.vout[outputIndex].nValue = 1000 + 100 * outputIndex;                      // Keep txs unique unless intended to duplicate
            tx.vout[outputIndex].output.scriptPubKey.assign(InsecureRand32() & 0x3F, 0); // Random sizes so we can test memory usage accounting
        }
        assert(CTransaction(tx).IsCoinBase());

        for (uint64_t outputIndex = 0; outputIndex < 3; ++outputIndex)
        {
            // allCoins[COutPoint(tx.GetHash(), outputIndex)] = std::tuple(heightOffset + blockCount - 1, 0, outputIndex);
        }

        // Call UpdateCoins on the top cache
        CTxUndo undo;
        UpdateCoins(tx, coinsView, undo, heightOffset, 0);

        block.transactions.push_back(tx);
        block.undos.push_back(undo);

        blocks.push_back(block);
    }

    // Taken from validation.cpp:DisconnectBlock() stripped most if it, leaving only things that use the CCoinsViewCache
    DisconnectResult SimulatedDisconnectBlock(const std::vector<CTransaction> &vtx, std::vector<CTxUndo> &vtxundo, CCoinsViewCache &view)
    {
        bool fClean = true;

        if (vtxundo.size() + 1 != vtx.size())
        {
            error("DisconnectBlock(): block and undo data inconsistent C");
            return DISCONNECT_FAILED;
        }

        // undo transactions in reverse order
        for (int i = vtx.size() - 1; i >= 0; i--)
        {
            const CTransaction &tx = vtx[i];
            uint256 hash = tx.GetHash();

            // Check that all outputs are available and match the outputs in the block itself
            // exactly.
            for (size_t o = 0; o < tx.vout.size(); o++)
            {
                if (!tx.vout[o].IsUnspendable())
                {
                    COutPoint out(hash, o);
                    CoinUndo coin;

                    LogPrintf("Spend output [%s]\n", out.ToString());

                    view.SpendCoin(out, &coin);
                    if (tx.vout[o] != coin.out)
                    {
                        fClean = false; // transaction output mismatch
                    }
                }
            }

            // restore inputs (not coinbases)
            if (i > 0)
            {
                CTxUndo &txundo = vtxundo[i - 1];
                if (tx.IsPoW2WitnessCoinBase())
                {
                    // NB! 'IsPoW2WitnessCoinBase' already forces vin size to be 2, and the first input NULL, so we don't need to validate the size here
                    if (txundo.vprevout.size() != 1)
                    {
                        error("DisconnectBlock(): transaction and undo data inconsistent A");
                        return DISCONNECT_FAILED;
                    }
                    for (unsigned int j = tx.vin.size(); j-- > 0;)
                    {
                        if (!tx.vin[j].GetPrevOut().IsNull())
                        {
                            const COutPoint &out = tx.vin[j].GetPrevOut();

                            // from unittest
                            CoinUndo undo = txundo.vprevout[j];
                            if (out.isHash)
                            {
                                assert(COutPoint(undo.prevhash, out.n) == out);
                                LogPrintf("Unspend hash input [%s] [%s]\n", out.ToString(), COutPoint((uint64_t)undo.nHeight, undo.nTxIndex, out.n).ToString());
                            }
                            else
                            {
                                LogPrintf("Unspend index input [%s] [%s]\n", COutPoint((uint64_t)undo.nHeight, undo.nTxIndex, out.n).ToString(), COutPoint(undo.prevhash, out.n).ToString());
                            }

                            int res = ApplyTxInUndo(std::move(txundo.vprevout[0]), view, out);
                            if (res == DISCONNECT_FAILED)
                                return DISCONNECT_FAILED;
                            fClean = fClean && res != DISCONNECT_UNCLEAN;
                        }
                    }
                }
                else
                {
                    if (txundo.vprevout.size() != tx.vin.size())
                    {
                        error("DisconnectBlock(): transaction and undo data inconsistent B");
                        return DISCONNECT_FAILED;
                    }
                    for (unsigned int j = tx.vin.size(); j-- > 0;)
                    {
                        const COutPoint &out = tx.vin[j].GetPrevOut();

                        // from unittest
                        CoinUndo undo = txundo.vprevout[j];
                        if (out.isHash)
                        {
                            assert(COutPoint(undo.prevhash, out.n) == out);
                            LogPrintf("Unspend hash input [%s] [%s]\n", out.ToString(), COutPoint((uint64_t)undo.nHeight, undo.nTxIndex, out.n).ToString());
                        }
                        else
                        {
                            LogPrintf("Unspend index input [%s] [%s]\n", COutPoint((uint64_t)undo.nHeight, undo.nTxIndex, out.n).ToString(), COutPoint(undo.prevhash, out.n).ToString());
                        }

                        int res = ApplyTxInUndo(std::move(txundo.vprevout[j]), view, out);
                        if (res == DISCONNECT_FAILED)
                            return DISCONNECT_FAILED;
                        fClean = fClean && res != DISCONNECT_UNCLEAN;
                    }
                }
                // At this point, all of txundo.vprevout should have been moved out.
            }
        }

        return fClean ? DISCONNECT_OK : DISCONNECT_UNCLEAN;
    }

    // Taken from validation.cpp:ConnectBlock() stripped most if it, leaving only things that use the CCoinsViewCache
    /** Apply the effects of this block (with given index) on the UTXO set represented by coins.
     *  Validity checks that depend on the UTXO set are also done; ConnectBlock()
     *  can fail if those validity checks fail (among other reasons). */
    bool SimulatedConnectBlock(const std::vector<CTransaction> &vtx, std::vector<CTxUndo> &vtxundo, int nHeight, CCoinsViewCache &view)
    {
        // "interesting" because of HaveCoin usage
        bool fEnforceBIP30 = true;
        if (fEnforceBIP30)
        {
            for (const auto &tx : vtx)
            {
                for (size_t o = 0; o < tx.vout.size(); o++)
                {
                    if (view.HaveCoin(COutPoint(tx.GetHash(), o)))
                    {
                        error("ConnectBlock(): tried to overwrite transaction [%s]", tx.GetHash().ToString());
                        return false;
                    }
                }
            }
        }

        int nInputs = 0;
        int64_t nSigOpsCost = 0;

        std::vector<std::pair<uint256, CDiskTxPos>> vPos;

        vtxundo.reserve(vtx.size() - 1);

        std::vector<PrecomputedTransactionData> txdata;

        for (unsigned int txIndex = 0; txIndex < vtx.size(); txIndex++)
        {
            const CTransaction &tx = vtx[txIndex];
            nInputs += tx.vin.size();

            if (!tx.IsCoinBase())
            {
                if (!view.HaveInputs(tx))
                {
                    // fixme: (PHASE5) - Low level fix for problem of conflicting transaction entering mempool and causing miners to be unable to mine (due to selecting invalid transactions for block continuously).
                    // This fix should remain in place, but a follow up fix is needed to try stop the conflicting transaction entering the mempool to begin with - need to hunt the source of this down.
                    // Seems to have something to do with a double (conflicting) witness renewal transaction.
                    mempool.removeRecursive(tx, MemPoolRemovalReason::UNKNOWN);
                    LogPrintf("ConnectBlock: mempool contains transaction with invalid inputs [%s]", tx.GetHash().ToString());
                    error("ConnectBlock: inputs missing/spent");
                    return false;
                }
            }

            CTxUndo undoDummy;
            if (txIndex > 0)
            {
                vtxundo.push_back(CTxUndo());
            }
            UpdateCoins(tx, view, txIndex == 0 ? undoDummy : vtxundo.back(), nHeight, txIndex);
        }

        return true;
    }
};

BOOST_FIXTURE_TEST_SUITE(coinsindex_tests, IndexTestingSetup)

BOOST_AUTO_TEST_CASE(coinbasetx_connect_disconnect_slowpath)
{
    bool allowFastPath = false;

    // 2 competing blocks both speding the coinbase tx of block # heightOffset
    SimBlock blk1a = createBlockSpendingCoinBaseByIndex(heightOffset + 1, heightOffset);
    SimBlock blk1b = createBlockSpendingCoinBaseByIndex(heightOffset + 1, heightOffset);

    { // connect block 1a
        CCoinsViewCache view(&coinsView);
        BOOST_CHECK(SimulatedConnectBlock(blk1a.transactions, blk1a.undos, blk1a.height, view));
        BOOST_CHECK(view.Flush(allowFastPath));
    }

    { // disconnect block 1a
        CCoinsViewCache view(&coinsView);
        BOOST_CHECK_EQUAL(SimulatedDisconnectBlock(blk1a.transactions, blk1a.undos, view), DISCONNECT_OK);
        BOOST_CHECK(view.Flush(allowFastPath));
    }

    { // connect block 1b
        CCoinsViewCache view(&coinsView);
        BOOST_CHECK(SimulatedConnectBlock(blk1b.transactions, blk1b.undos, blk1b.height, view));
        BOOST_CHECK(view.Flush(allowFastPath));
    }

    { // disconnect block 1b
        CCoinsViewCache view(&coinsView);
        BOOST_CHECK_EQUAL(SimulatedDisconnectBlock(blk1b.transactions, blk1b.undos, view), DISCONNECT_OK);
        BOOST_CHECK(view.Flush(allowFastPath));
    }

    { // connect block 1a again!
        CCoinsViewCache view(&coinsView);
        BOOST_CHECK(SimulatedConnectBlock(blk1a.transactions, blk1a.undos, blk1a.height, view));
        BOOST_CHECK(view.Flush(allowFastPath));
    }

    Coin coin;
    COutPoint canonicalOut;

    // get coin from tx in blk1a by index
    BOOST_CHECK(coinsView.GetCoin(COutPoint(blk1a.height, 0, 0), coin, &canonicalOut));

    // verify that the tx hash of the canonical outpoint matches the tx hash in blk1a
    BOOST_CHECK(canonicalOut.isHash && canonicalOut.getTransactionHash() == blk1a.transactions[0].GetHash());
}

BOOST_AUTO_TEST_CASE(coinbasetx_connect_disconnect_fastpath)
{
    bool allowFastPath = true;

    // 2 competing blocks both speding the coinbase tx of block # heightOffset
    SimBlock blk1a = createBlockSpendingCoinBaseByIndex(heightOffset + 1, heightOffset);
    SimBlock blk1b = createBlockSpendingCoinBaseByIndex(heightOffset + 1, heightOffset);

    { // connect block 1a
        CCoinsViewCache view(&coinsView);
        BOOST_CHECK(SimulatedConnectBlock(blk1a.transactions, blk1a.undos, blk1a.height, view));
        BOOST_CHECK(view.Flush(allowFastPath));
    }

    { // disconnect block 1a
        CCoinsViewCache view(&coinsView);
        BOOST_CHECK_EQUAL(SimulatedDisconnectBlock(blk1a.transactions, blk1a.undos, view), DISCONNECT_OK);
        BOOST_CHECK(view.Flush(allowFastPath));
    }

    { // connect block 1b
        CCoinsViewCache view(&coinsView);
        BOOST_CHECK(SimulatedConnectBlock(blk1b.transactions, blk1b.undos, blk1b.height, view));
        BOOST_CHECK(view.Flush(allowFastPath));
    }

    { // disconnect block 1b
        CCoinsViewCache view(&coinsView);
        BOOST_CHECK_EQUAL(SimulatedDisconnectBlock(blk1b.transactions, blk1b.undos, view), DISCONNECT_OK);
        BOOST_CHECK(view.Flush(allowFastPath));
    }

    { // connect block 1a again!
        CCoinsViewCache view(&coinsView);
        BOOST_CHECK(SimulatedConnectBlock(blk1a.transactions, blk1a.undos, blk1a.height, view));
        BOOST_CHECK(view.Flush(allowFastPath));
    }

    Coin coin;
    COutPoint canonicalOut;

    // get coin from tx in blk1a by index
    BOOST_CHECK(coinsView.GetCoin(COutPoint(blk1a.height, 0, 0), coin, &canonicalOut));

    // verify that the tx hash of the canonical outpoint matches the tx hash in blk1a
    BOOST_CHECK(canonicalOut.isHash && canonicalOut.getTransactionHash() == blk1a.transactions[0].GetHash());
}

BOOST_AUTO_TEST_CASE(add_duplicate_coins)
{
    CCoinsViewCache view(&coinsView);

    // 2 different (fake) tx hashes
    uint256 hash_a = GetRandHash();
    uint256 hash_b = GetRandHash();
    BOOST_CHECK(hash_a != hash_b);

    auto createCoin = [&]() -> Coin
    {
        CTxOut txOut;
        txOut.nValue = 500;
        txOut.output.scriptPubKey.assign(uint32_t(12345) & 0x3F, 0);
        return Coin(txOut, heightOffset + 1, 3, false, true);
    };

    // attempt to add same coin (same height and input tx index) with different tx hash, this should fail!
    view.AddCoin(COutPoint(hash_a, 0), createCoin(), false);
    BOOST_CHECK_THROW(view.AddCoin(COutPoint(hash_b, 0), createCoin(), false), std::exception);
}

BOOST_AUTO_TEST_SUITE_END()
