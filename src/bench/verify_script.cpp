// Copyright (c) 2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "bench.h"
#include "key.h"
#if defined(HAVE_CONSENSUS_LIB)
#include "script/guldenconsensus.h"
#endif
#include "script/script.h"
#include "script/sign.h"
#include "streams.h"

// FIXME: (Bitcoin) Dedup with BuildCreditingTransaction in test/script_tests.cpp.
static CMutableTransaction BuildCreditingTransaction(const CScript& scriptPubKey)
{
    CMutableTransaction txCredit;
    txCredit.nVersion = 1;
    txCredit.nLockTime = 0;
    txCredit.vin.resize(1);
    txCredit.vout.resize(1);
    txCredit.vin[0].prevout.SetNull();
    txCredit.vin[0].scriptSig = CScript() << CScriptNum(0) << CScriptNum(0);
    //fixme: (2.1)
    txCredit.vin[0].nSequence = CTxIn::SEQUENCE_FINAL;
    txCredit.vout[0].scriptPubKey = scriptPubKey;
    txCredit.vout[0].nValue = 1;

    return txCredit;
}

// FIXME: Dedup with BuildSpendingTransaction in test/script_tests.cpp.
static CMutableTransaction BuildSpendingTransaction(const CScript& scriptSig, const CMutableTransaction& txCredit)
{
    CMutableTransaction txSpend;
    txSpend.nVersion = 1;
    txSpend.nLockTime = 0;
    txSpend.vin.resize(1);
    txSpend.vout.resize(1);
    txSpend.vin[0].prevout.hash = txCredit.GetHash();
    txSpend.vin[0].prevout.n = 0;
    txSpend.vin[0].scriptSig = scriptSig;
    //fixme: (2.1)
    txSpend.vin[0].nSequence = CTxIn::SEQUENCE_FINAL;
    txSpend.vout[0].scriptPubKey = CScript();
    txSpend.vout[0].nValue = txCredit.vout[0].nValue;

    return txSpend;
}

// Microbenchmark for verification of a basic P2WPKH script. Can be easily
// modified to measure performance of other types of scripts.
static void VerifyScriptBench(benchmark::State& state)
{
    //fixme: (2.1) - implement if needed
}

//fixme: (2.1) - implement if needed
//BENCHMARK(VerifyScriptBench);
