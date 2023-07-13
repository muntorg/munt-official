// Copyright (c) 2011-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// File contains modifications by: The Centure developers
// All modifications:
// Copyright (c) 2017-2022 The Centure developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the GNU Lesser General Public License v3, see the accompanying
// file COPYING

#include "consensus/validation.h"
#include "key.h"
#include "validation/validation.h"
#include "generation/miner.h"
#include "pubkey.h"
#include "txmempool.h"
#include "random.h"
#include "script/standard.h"
#include "test/test.h"
#include "util/time.h"

#ifdef ENABLE_WALLET
#include <wallet/wallet.h>
#endif

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(tx_validationcache_tests)

static bool
ToMemPool(CMutableTransaction& tx)
{
    LOCK(cs_main);

    CValidationState state;
    return AcceptToMemoryPool(mempool, state, MakeTransactionRef(tx), false, NULL, NULL, true, 0);
}

BOOST_FIXTURE_TEST_CASE(tx_mempool_block_doublespend, TestChain100Setup)
{
    // Make sure skipping validation of transctions that were
    // validated going into the memory pool does not allow
    // double-spends in blocks to pass validation when they should not.

    CScript scriptPubKey = CScript() <<  ToByteVector(coinbaseKey.GetPubKey()) << OP_CHECKSIG;
    std::shared_ptr<CReserveKeyOrScript> reservedScript = std::make_shared<CReserveKeyOrScript>(scriptPubKey);

    // Create a double-spend of mature coinbase txn:
    std::vector<CMutableTransaction> spends;
    spends.resize(2, CMutableTransaction(TEST_DEFAULT_TX_VERSION));
    for (int i = 0; i < 2; i++)
    {
        spends[i].nVersion = 1;
        spends[i].vin.resize(1);
        COutPoint changePrevOut = spends[i].vin[0].GetPrevOut();
        changePrevOut.setHash(coinbaseTxns[0].GetHash());
        changePrevOut.n = 0;
        spends[i].vin[0].SetPrevOut(changePrevOut);
        spends[i].vout.resize(1);
        spends[i].vout[0].nValue = 11*CENT;
        spends[i].vout[0].output.scriptPubKey = scriptPubKey;

        // Sign:
        std::vector<unsigned char> vchSig;
        uint256 hash = SignatureHash(scriptPubKey, spends[i], 0, SIGHASH_ALL, 0, SIGVERSION_BASE);
        BOOST_CHECK(coinbaseKey.Sign(hash, vchSig));
        vchSig.push_back((unsigned char)SIGHASH_ALL);
        spends[i].vin[0].scriptSig << vchSig;
    }

    CBlock block;

    // Test 1: block with both of those transactions should be rejected.
    block = CreateAndProcessBlock(spends, reservedScript);
    BOOST_CHECK(chainActive.Tip()->GetBlockHashLegacy() != block.GetHashLegacy());

    // Test 2: ... and should be rejected if spend1 is in the memory pool
    BOOST_CHECK(ToMemPool(spends[0]));
    block = CreateAndProcessBlock(spends, reservedScript);
    BOOST_CHECK(chainActive.Tip()->GetBlockHashLegacy() != block.GetHashLegacy());
    mempool.clear();

    // Test 3: ... and should be rejected if spend2 is in the memory pool
    BOOST_CHECK(ToMemPool(spends[1]));
    block = CreateAndProcessBlock(spends, reservedScript);
    BOOST_CHECK(chainActive.Tip()->GetBlockHashLegacy() != block.GetHashLegacy());
    mempool.clear();

    // Final sanity test: first spend in mempool, second in block, that's OK:
    std::vector<CMutableTransaction> oneSpend;
    oneSpend.push_back(spends[0]);
    BOOST_CHECK(ToMemPool(spends[1]));
    block = CreateAndProcessBlock(oneSpend, reservedScript);
    BOOST_CHECK(chainActive.Tip()->GetBlockHashLegacy() == block.GetHashLegacy());
    // spends[1] should have been removed from the mempool when the
    // block with spends[0] is accepted:
    BOOST_CHECK_EQUAL(mempool.size(), 0U);
}

BOOST_AUTO_TEST_SUITE_END()
