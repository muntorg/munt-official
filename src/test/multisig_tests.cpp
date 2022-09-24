// Copyright (c) 2011-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// File contains modifications by: The Centure developers
// All modifications:
// Copyright (c) 2016-2022 The Centure developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the Libre Chain License, see the accompanying
// file COPYING

#include "key.h"
#include "keystore.h"
#include "policy/policy.h"
#include "script/script.h"
#include "script/script_error.h"
#include "script/interpreter.h"
#include "script/sign.h"
#include "script/ismine.h"
#include "uint256.h"
#include "test/test.h"



#include <boost/test/unit_test.hpp>

typedef std::vector<unsigned char> valtype;

BOOST_FIXTURE_TEST_SUITE(multisig_tests, BasicTestingSetup)

void sign_multisig(CScript scriptPubKey, std::vector<CKey> keys, CTransaction transaction, int whichIn, CScript& resultScript, CSegregatedSignatureData& resultSigData)
{
    auto sigversion = transaction.nVersion < CTransaction::SEGSIG_ACTIVATION_VERSION ? SIGVERSION_BASE : SIGVERSION_SEGSIG;
    auto sigtype = SIGHASH_ALL;
    uint256 hash = SignatureHash(scriptPubKey, transaction, whichIn, sigtype, 0, sigversion);

    if (sigversion != SIGVERSION_SEGSIG)
    {
        resultScript << OP_0; // CHECKMULTISIG bug workaround
    }
    for(const CKey &key : keys)
    {
        //Segsig keys must be compressed
        BOOST_CHECK (sigversion != SIGVERSION_SEGSIG || key.IsCompressed());
        
        std::vector<unsigned char> vchSig;
        if (sigversion == SIGVERSION_SEGSIG)
        {
            BOOST_CHECK(key.SignCompact(hash, vchSig));
            vchSig.push_back((unsigned char)sigtype);
            resultSigData.stack.push_back(vchSig);
        }
        else
        {
            BOOST_CHECK(key.Sign(hash, vchSig));
            vchSig.push_back((unsigned char)sigtype);
            resultScript << vchSig;
        }
    }
}

BOOST_AUTO_TEST_CASE(multisig_verify)
{
    unsigned int flags = SCRIPT_VERIFY_P2SH | SCRIPT_VERIFY_STRICTENC;

    ScriptError err;
    CKey key[4];
    CAmount amount = 0;
    for (int i = 0; i < 4; i++)
        key[i].MakeNewKey(true);

    CScript a_and_b;
    a_and_b << OP_2 << ToByteVector(key[0].GetPubKey()) << ToByteVector(key[1].GetPubKey()) << OP_2 << OP_CHECKMULTISIG;

    CScript a_or_b;
    a_or_b << OP_1 << ToByteVector(key[0].GetPubKey()) << ToByteVector(key[1].GetPubKey()) << OP_2 << OP_CHECKMULTISIG;

    CScript escrow;
    escrow << OP_2 << ToByteVector(key[0].GetPubKey()) << ToByteVector(key[1].GetPubKey()) << ToByteVector(key[2].GetPubKey()) << OP_3 << OP_CHECKMULTISIG;

    CMutableTransaction txFrom(TEST_DEFAULT_TX_VERSION);  // Funding transaction
    txFrom.vout.resize(3);
    txFrom.vout[0].output.scriptPubKey = a_and_b;
    txFrom.vout[1].output.scriptPubKey = a_or_b;
    txFrom.vout[2].output.scriptPubKey = escrow;

    CMutableTransaction txTo[3] = // Spending transaction
                                { CMutableTransaction(TEST_DEFAULT_TX_VERSION),
                                  CMutableTransaction(TEST_DEFAULT_TX_VERSION),
                                  CMutableTransaction(TEST_DEFAULT_TX_VERSION)
                                };

    for (int i = 0; i < 3; i++)
    {
        txTo[i].vin.resize(1);
        txTo[i].vout.resize(1);
        COutPoint changePrevOut = txTo[i].vin[0].GetPrevOut();
        changePrevOut.n = i;
        changePrevOut.setHash(txFrom.GetHash());
        txTo[i].vin[0].SetPrevOut(changePrevOut);
        txTo[i].vout[0].nValue = 1;
    }

    std::vector<CKey> keys;
    CScript scriptSig;
    CSegregatedSignatureData scriptSigData;

    // Test a AND b:
    keys.assign(1,key[0]);
    keys.push_back(key[1]);
    sign_multisig(a_and_b, keys, txTo[0], 0, scriptSig, scriptSigData);
    BOOST_CHECK(scriptSigData.stack.empty());
    BOOST_CHECK(VerifyScript(scriptSig, a_and_b, &scriptSigData, flags, MutableTransactionSignatureChecker(CKeyID(), CKeyID(), &txTo[0], 0, amount), SCRIPT_V1, &err));
    BOOST_CHECK_MESSAGE(err == SCRIPT_ERR_OK, ScriptErrorString(err));

    for (int i = 0; i < 4; i++)
    {
        scriptSig.clear();
        scriptSigData.stack.clear();
        keys.assign(1,key[i]);
        sign_multisig(a_and_b, keys, txTo[0], 0, scriptSig, scriptSigData);
        BOOST_CHECK(scriptSigData.stack.empty());
        BOOST_CHECK_MESSAGE(!VerifyScript(scriptSig, a_and_b, &scriptSigData, flags, MutableTransactionSignatureChecker(CKeyID(), CKeyID(), &txTo[0], 0, amount), SCRIPT_V1, &err), strprintf("a&b 1: %d", i));
        BOOST_CHECK_MESSAGE(err == SCRIPT_ERR_INVALID_STACK_OPERATION, ScriptErrorString(err));

        scriptSig.clear();
        scriptSigData.stack.clear();
        keys.assign(1,key[1]);
        keys.push_back(key[i]);
        sign_multisig(a_and_b, keys, txTo[0], 0, scriptSig, scriptSigData);
        BOOST_CHECK(scriptSigData.stack.empty());
        BOOST_CHECK_MESSAGE(!VerifyScript(scriptSig, a_and_b, &scriptSigData, flags, MutableTransactionSignatureChecker(CKeyID(), CKeyID(), &txTo[0], 0, amount), SCRIPT_V1, &err), strprintf("a&b 2: %d", i));
        BOOST_CHECK_MESSAGE(err == SCRIPT_ERR_EVAL_FALSE, ScriptErrorString(err));
    }

    // Test a OR b:
    for (int i = 0; i < 4; i++)
    {
        scriptSig.clear();
        scriptSigData.stack.clear();
        keys.assign(1,key[i]);
        sign_multisig(a_or_b, keys, txTo[1], 0, scriptSig, scriptSigData);
        BOOST_CHECK(scriptSigData.stack.empty());
        if (i == 0 || i == 1)
        {
            BOOST_CHECK_MESSAGE(VerifyScript(scriptSig, a_or_b, &scriptSigData, flags, MutableTransactionSignatureChecker(CKeyID(), CKeyID(), &txTo[1], 0, amount), SCRIPT_V1, &err), strprintf("a|b: %d", i));
            BOOST_CHECK_MESSAGE(err == SCRIPT_ERR_OK, ScriptErrorString(err));
        }
        else
        {
            BOOST_CHECK_MESSAGE(!VerifyScript(scriptSig, a_or_b, &scriptSigData, flags, MutableTransactionSignatureChecker(CKeyID(), CKeyID(), &txTo[1], 0, amount), SCRIPT_V1, &err), strprintf("a|b: %d", i));
            BOOST_CHECK_MESSAGE(err == SCRIPT_ERR_EVAL_FALSE, ScriptErrorString(err));
        }
    }
    scriptSig.clear();
    scriptSigData.stack.clear();
    scriptSig << OP_0 << OP_1;
    BOOST_CHECK(!VerifyScript(scriptSig, a_or_b, NULL, flags, MutableTransactionSignatureChecker(CKeyID(), CKeyID(), &txTo[1], 0, amount), SCRIPT_V1, &err));
    BOOST_CHECK_MESSAGE(err == SCRIPT_ERR_SIG_DER, ScriptErrorString(err));


    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            scriptSig.clear();
            scriptSigData.stack.clear();
            keys.assign(1,key[i]);
            keys.push_back(key[j]);
            sign_multisig(escrow, keys, txTo[2], 0, scriptSig, scriptSigData);
            BOOST_CHECK(scriptSigData.stack.empty());
            if (i < j && i < 3 && j < 3)
            {
                BOOST_CHECK_MESSAGE(VerifyScript(scriptSig, escrow, &scriptSigData, flags, MutableTransactionSignatureChecker(CKeyID(), CKeyID(), &txTo[2], 0, amount), SCRIPT_V1, &err), strprintf("escrow 1: %d %d", i, j));
                BOOST_CHECK_MESSAGE(err == SCRIPT_ERR_OK, ScriptErrorString(err));
            }
            else
            {
                BOOST_CHECK_MESSAGE(!VerifyScript(scriptSig, escrow, &scriptSigData, flags, MutableTransactionSignatureChecker(CKeyID(), CKeyID(), &txTo[2], 0, amount), SCRIPT_V1, &err), strprintf("escrow 2: %d %d", i, j));
                BOOST_CHECK_MESSAGE(err == SCRIPT_ERR_EVAL_FALSE, ScriptErrorString(err));
            }
        }
    }
}


BOOST_AUTO_TEST_CASE(multisig_verify_segsig)
{
    unsigned int flags = SCRIPT_VERIFY_P2SH | SCRIPT_VERIFY_STRICTENC;

    ScriptError err;
    CKey key[4];
    CAmount amount = 0;
    for (int i = 0; i < 4; i++)
        key[i].MakeNewKey(true);

    CScript a_and_b;
    a_and_b << OP_2 << ToByteVector(key[0].GetPubKey()) << ToByteVector(key[1].GetPubKey()) << OP_2 << OP_CHECKMULTISIG;

    CScript a_or_b;
    a_or_b << OP_1 << ToByteVector(key[0].GetPubKey()) << ToByteVector(key[1].GetPubKey()) << OP_2 << OP_CHECKMULTISIG;

    CScript escrow;
    escrow << OP_2 << ToByteVector(key[0].GetPubKey()) << ToByteVector(key[1].GetPubKey()) << ToByteVector(key[2].GetPubKey()) << OP_3 << OP_CHECKMULTISIG;

    CMutableTransaction txFrom(CTransaction::SEGSIG_ACTIVATION_VERSION);  // Funding transaction
    txFrom.vout.resize(3);
    txFrom.vout[0].output.scriptPubKey = a_and_b;
    txFrom.vout[1].output.scriptPubKey = a_or_b;
    txFrom.vout[2].output.scriptPubKey = escrow;

    CMutableTransaction txTo[3] = // Spending transaction
                                { CMutableTransaction(CTransaction::SEGSIG_ACTIVATION_VERSION),
                                  CMutableTransaction(CTransaction::SEGSIG_ACTIVATION_VERSION),
                                  CMutableTransaction(CTransaction::SEGSIG_ACTIVATION_VERSION)
                                };

    for (int i = 0; i < 3; i++)
    {
        txTo[i].vin.resize(1);
        txTo[i].vout.resize(1);
        COutPoint changePrevOut = txTo[i].vin[0].GetPrevOut();
        changePrevOut.n = i;
        changePrevOut.setHash(txFrom.GetHash());
        txTo[i].vin[0].SetPrevOut(changePrevOut);
        txTo[i].vout[0].nValue = 1;
    }

    std::vector<CKey> keys;
    CScript scriptSig;
    CSegregatedSignatureData scriptSigData;

    // Test a AND b:
    keys.assign(1,key[0]);
    keys.push_back(key[1]);
    sign_multisig(a_and_b, keys, txTo[0], 0, scriptSig, scriptSigData);
    BOOST_CHECK(VerifyScript(scriptSig, a_and_b, &scriptSigData, flags, MutableTransactionSignatureChecker(CKeyID(), CKeyID(), &txTo[0], 0, amount), SCRIPT_V2, &err));
    BOOST_CHECK_MESSAGE(err == SCRIPT_ERR_OK, ScriptErrorString(err));

    for (int i = 0; i < 4; i++)
    {
        scriptSig.clear();
        scriptSigData.stack.clear();
        keys.assign(1,key[i]);
        sign_multisig(a_and_b, keys, txTo[0], 0, scriptSig, scriptSigData);
        BOOST_CHECK_MESSAGE(!VerifyScript(scriptSig, a_and_b, &scriptSigData, flags, MutableTransactionSignatureChecker(CKeyID(), CKeyID(), &txTo[0], 0, amount), SCRIPT_V2, &err), strprintf("a&b 1: %d", i));
        BOOST_CHECK_MESSAGE(err == SCRIPT_ERR_INVALID_STACK_OPERATION, ScriptErrorString(err));

        scriptSig.clear();
        scriptSigData.stack.clear();
        keys.assign(1,key[1]);
        keys.push_back(key[i]);
        sign_multisig(a_and_b, keys, txTo[0], 0, scriptSig, scriptSigData);
        BOOST_CHECK_MESSAGE(!VerifyScript(scriptSig, a_and_b, &scriptSigData, flags, MutableTransactionSignatureChecker(CKeyID(), CKeyID(), &txTo[0], 0, amount), SCRIPT_V2, &err), strprintf("a&b 2: %d", i));
        BOOST_CHECK_MESSAGE(err == SCRIPT_ERR_EVAL_FALSE, ScriptErrorString(err));
    }

    // Test a OR b:
    for (int i = 0; i < 4; i++)
    {
        scriptSig.clear();
        scriptSigData.stack.clear();
        keys.assign(1,key[i]);
        sign_multisig(a_or_b, keys, txTo[1], 0, scriptSig, scriptSigData);
        if (i == 0 || i == 1)
        {
            BOOST_CHECK_MESSAGE(VerifyScript(scriptSig, a_or_b, &scriptSigData, flags, MutableTransactionSignatureChecker(CKeyID(), CKeyID(), &txTo[1], 0, amount), SCRIPT_V2, &err), strprintf("a|b: %d", i));
            BOOST_CHECK_MESSAGE(err == SCRIPT_ERR_OK, ScriptErrorString(err));
        }
        else
        {
            BOOST_CHECK_MESSAGE(!VerifyScript(scriptSig, a_or_b, &scriptSigData, flags, MutableTransactionSignatureChecker(CKeyID(), CKeyID(), &txTo[1], 0, amount), SCRIPT_V2, &err), strprintf("a|b: %d", i));
            BOOST_CHECK_MESSAGE(err == SCRIPT_ERR_EVAL_FALSE, ScriptErrorString(err));
        }
    }
    
    scriptSig.clear();
    scriptSigData.stack.clear();
    scriptSig << OP_0 << OP_1;
    BOOST_CHECK(!VerifyScript(scriptSig, a_or_b, &scriptSigData, flags, MutableTransactionSignatureChecker(CKeyID(), CKeyID(), &txTo[1], 0, amount), SCRIPT_V2, &err));
    //fixme: (PHASE5) - See fixme: (PHASE5) (SEGSIG) (DERSIG) in interpreter.cpp for why we don't test this now.
    //BOOST_CHECK_MESSAGE(err == SCRIPT_ERR_SIG_DER, ScriptErrorString(err));

    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            scriptSig.clear();
            scriptSigData.stack.clear();
            keys.assign(1,key[i]);
            keys.push_back(key[j]);
            sign_multisig(escrow, keys, txTo[2], 0, scriptSig, scriptSigData);
            if (i < j && i < 3 && j < 3)
            {
                BOOST_CHECK_MESSAGE(VerifyScript(scriptSig, escrow, &scriptSigData, flags, MutableTransactionSignatureChecker(CKeyID(), CKeyID(), &txTo[2], 0, amount), SCRIPT_V2, &err), strprintf("escrow 1: %d %d", i, j));
                BOOST_CHECK_MESSAGE(err == SCRIPT_ERR_OK, ScriptErrorString(err));
            }
            else
            {
                BOOST_CHECK_MESSAGE(!VerifyScript(scriptSig, escrow, &scriptSigData, flags, MutableTransactionSignatureChecker(CKeyID(), CKeyID(), &txTo[2], 0, amount), SCRIPT_V2, &err), strprintf("escrow 2: %d %d", i, j));
                BOOST_CHECK_MESSAGE(err == SCRIPT_ERR_EVAL_FALSE, ScriptErrorString(err));
            }
        }
    }
}


BOOST_AUTO_TEST_CASE(multisig_IsStandard)
{
    CKey key[4];
    for (int i = 0; i < 4; i++)
        key[i].MakeNewKey(true);

    txnouttype whichType;

    CScript a_and_b;
    a_and_b << OP_2 << ToByteVector(key[0].GetPubKey()) << ToByteVector(key[1].GetPubKey()) << OP_2 << OP_CHECKMULTISIG;
    BOOST_CHECK(::IsStandard(a_and_b, whichType));

    CScript a_or_b;
    a_or_b  << OP_1 << ToByteVector(key[0].GetPubKey()) << ToByteVector(key[1].GetPubKey()) << OP_2 << OP_CHECKMULTISIG;
    BOOST_CHECK(::IsStandard(a_or_b, whichType));

    CScript escrow;
    escrow << OP_2 << ToByteVector(key[0].GetPubKey()) << ToByteVector(key[1].GetPubKey()) << ToByteVector(key[2].GetPubKey()) << OP_3 << OP_CHECKMULTISIG;
    BOOST_CHECK(::IsStandard(escrow, whichType));

    CScript one_of_four;
    one_of_four << OP_1 << ToByteVector(key[0].GetPubKey()) << ToByteVector(key[1].GetPubKey()) << ToByteVector(key[2].GetPubKey()) << ToByteVector(key[3].GetPubKey()) << OP_4 << OP_CHECKMULTISIG;
    BOOST_CHECK(!::IsStandard(one_of_four, whichType));

    CScript malformed[6];
    malformed[0] << OP_3 << ToByteVector(key[0].GetPubKey()) << ToByteVector(key[1].GetPubKey()) << OP_2 << OP_CHECKMULTISIG;
    malformed[1] << OP_2 << ToByteVector(key[0].GetPubKey()) << ToByteVector(key[1].GetPubKey()) << OP_3 << OP_CHECKMULTISIG;
    malformed[2] << OP_0 << ToByteVector(key[0].GetPubKey()) << ToByteVector(key[1].GetPubKey()) << OP_2 << OP_CHECKMULTISIG;
    malformed[3] << OP_1 << ToByteVector(key[0].GetPubKey()) << ToByteVector(key[1].GetPubKey()) << OP_0 << OP_CHECKMULTISIG;
    malformed[4] << OP_1 << ToByteVector(key[0].GetPubKey()) << ToByteVector(key[1].GetPubKey()) << OP_CHECKMULTISIG;
    malformed[5] << OP_1 << ToByteVector(key[0].GetPubKey()) << ToByteVector(key[1].GetPubKey());

    for (int i = 0; i < 6; i++)
        BOOST_CHECK(!::IsStandard(malformed[i], whichType));
}

BOOST_AUTO_TEST_CASE(multisig_Solver1)
{
    // Tests Solver() that returns lists of keys that are
    // required to satisfy a ScriptPubKey
    //
    // Also tests IsMine() and ExtractDestination()
    //
    // Note: ExtractDestination for the multisignature transactions
    // always returns false for this release, even if you have
    // one key that would satisfy an (a|b) or 2-of-3 keys needed
    // to spend an escrow transaction.
    //
    #ifdef ENABLE_WALLET
    CBasicKeyStore keystore, emptykeystore, partialkeystore;
    CKey key[3];
    CTxDestination keyaddr[3];
    for (int i = 0; i < 3; i++)
    {
        key[i].MakeNewKey(true);
        keystore.AddKey(key[i]);
        keyaddr[i] = key[i].GetPubKey().GetID();
    }
    partialkeystore.AddKey(key[0]);

    {
        std::vector<valtype> solutions;
        txnouttype whichType;
        CScript s;
        s << ToByteVector(key[0].GetPubKey()) << OP_CHECKSIG;
        BOOST_CHECK(Solver(s, whichType, solutions));
        BOOST_CHECK(solutions.size() == 1);
        CTxDestination addr;
        BOOST_CHECK(ExtractDestination(s, addr));
        BOOST_CHECK(addr == keyaddr[0]);
        BOOST_CHECK(IsMine(keystore, s));
        BOOST_CHECK(!IsMine(emptykeystore, s));
    }
    {
        std::vector<valtype> solutions;
        txnouttype whichType;
        CScript s;
        s << OP_DUP << OP_HASH160 << ToByteVector(key[0].GetPubKey().GetID()) << OP_EQUALVERIFY << OP_CHECKSIG;
        BOOST_CHECK(Solver(s, whichType, solutions));
        BOOST_CHECK(solutions.size() == 1);
        CTxDestination addr;
        BOOST_CHECK(ExtractDestination(s, addr));
        BOOST_CHECK(addr == keyaddr[0]);
        BOOST_CHECK(IsMine(keystore, s));
        BOOST_CHECK(!IsMine(emptykeystore, s));
    }
    {
        std::vector<valtype> solutions;
        txnouttype whichType;
        CScript s;
        s << OP_2 << ToByteVector(key[0].GetPubKey()) << ToByteVector(key[1].GetPubKey()) << OP_2 << OP_CHECKMULTISIG;
        BOOST_CHECK(Solver(s, whichType, solutions));
        BOOST_CHECK_EQUAL(solutions.size(), 4U);
        CTxDestination addr;
        BOOST_CHECK(!ExtractDestination(s, addr));
        BOOST_CHECK(IsMine(keystore, s));
        BOOST_CHECK(!IsMine(emptykeystore, s));
        BOOST_CHECK(!IsMine(partialkeystore, s));
    }
    {
        std::vector<valtype> solutions;
        txnouttype whichType;
        CScript s;
        s << OP_1 << ToByteVector(key[0].GetPubKey()) << ToByteVector(key[1].GetPubKey()) << OP_2 << OP_CHECKMULTISIG;
        BOOST_CHECK(Solver(s, whichType, solutions));
        BOOST_CHECK_EQUAL(solutions.size(), 4U);
        std::vector<CTxDestination> addrs;
        int nRequired;
        BOOST_CHECK(ExtractDestinations(s, whichType, addrs, nRequired));
        BOOST_CHECK(addrs[0] == keyaddr[0]);
        BOOST_CHECK(addrs[1] == keyaddr[1]);
        BOOST_CHECK(nRequired == 1);
        BOOST_CHECK(IsMine(keystore, s));
        BOOST_CHECK(!IsMine(emptykeystore, s));
        BOOST_CHECK(!IsMine(partialkeystore, s));
    }
    {
        std::vector<valtype> solutions;
        txnouttype whichType;
        CScript s;
        s << OP_2 << ToByteVector(key[0].GetPubKey()) << ToByteVector(key[1].GetPubKey()) << ToByteVector(key[2].GetPubKey()) << OP_3 << OP_CHECKMULTISIG;
        BOOST_CHECK(Solver(s, whichType, solutions));
        BOOST_CHECK(solutions.size() == 5);
    }
    #endif
}

BOOST_AUTO_TEST_CASE(multisig_Sign)
{
    // Test SignSignature() (and therefore the version of Solver() that signs transactions)
    CBasicKeyStore keystore;
    CKey key[4];
    for (int i = 0; i < 4; i++)
    {
        key[i].MakeNewKey(true);
        keystore.AddKey(key[i]);
    }
    
    std::vector<CKeyStore*> accountsToTry;
    accountsToTry.push_back(&keystore);

    CScript a_and_b;
    a_and_b << OP_2 << ToByteVector(key[0].GetPubKey()) << ToByteVector(key[1].GetPubKey()) << OP_2 << OP_CHECKMULTISIG;

    CScript a_or_b;
    a_or_b  << OP_1 << ToByteVector(key[0].GetPubKey()) << ToByteVector(key[1].GetPubKey()) << OP_2 << OP_CHECKMULTISIG;

    CScript escrow;
    escrow << OP_2 << ToByteVector(key[0].GetPubKey()) << ToByteVector(key[1].GetPubKey()) << ToByteVector(key[2].GetPubKey()) << OP_3 << OP_CHECKMULTISIG;

    CMutableTransaction txFrom(TEST_DEFAULT_TX_VERSION);  // Funding transaction
    txFrom.vout.resize(3);
    txFrom.vout[0].output.scriptPubKey = a_and_b;
    txFrom.vout[1].output.scriptPubKey = a_or_b;
    txFrom.vout[2].output.scriptPubKey = escrow;

    // Spending transaction
    CMutableTransaction txTo[3] = { CMutableTransaction(TEST_DEFAULT_TX_VERSION), CMutableTransaction(TEST_DEFAULT_TX_VERSION), CMutableTransaction(TEST_DEFAULT_TX_VERSION)};
    for (int i = 0; i < 3; i++)
    {
        txTo[i].vin.resize(1);
        txTo[i].vout.resize(1);
        COutPoint changePrevOut = txTo[i].vin[0].GetPrevOut();
        changePrevOut.n = i;
        changePrevOut.setHash(txFrom.GetHash());
        txTo[i].vin[0].SetPrevOut(changePrevOut);
        txTo[i].vout[0].nValue = 1;
    }

    for (int i = 0; i < 3; i++)
    {
        BOOST_CHECK_MESSAGE(SignSignature(accountsToTry, txFrom, txTo[i], 0, SIGHASH_ALL, SignType::Spend), strprintf("SignSignature %d", i));
    }
}


BOOST_AUTO_TEST_CASE(multisig_Sign_segsig)
{
    // Test SignSignature() (and therefore the version of Solver() that signs transactions)
    CBasicKeyStore keystore;
    CKey key[4];
    for (int i = 0; i < 4; i++)
    {
        key[i].MakeNewKey(true);
        keystore.AddKey(key[i]);
    }
    
    std::vector<CKeyStore*> accountsToTry;
    accountsToTry.push_back(&keystore);

    CScript a_and_b;
    a_and_b << OP_2 << ToByteVector(key[0].GetPubKey()) << ToByteVector(key[1].GetPubKey()) << OP_2 << OP_CHECKMULTISIG;

    CScript a_or_b;
    a_or_b  << OP_1 << ToByteVector(key[0].GetPubKey()) << ToByteVector(key[1].GetPubKey()) << OP_2 << OP_CHECKMULTISIG;

    CScript escrow;
    escrow << OP_2 << ToByteVector(key[0].GetPubKey()) << ToByteVector(key[1].GetPubKey()) << ToByteVector(key[2].GetPubKey()) << OP_3 << OP_CHECKMULTISIG;

    CMutableTransaction txFrom(CTransaction::SEGSIG_ACTIVATION_VERSION);  // Funding transaction
    txFrom.vout.resize(3);
    txFrom.vout[0].output.scriptPubKey = a_and_b;
    txFrom.vout[1].output.scriptPubKey = a_or_b;
    txFrom.vout[2].output.scriptPubKey = escrow;

    // Spending transaction
    CMutableTransaction txTo[3] = { CMutableTransaction(CTransaction::SEGSIG_ACTIVATION_VERSION), CMutableTransaction(CTransaction::SEGSIG_ACTIVATION_VERSION), CMutableTransaction(CTransaction::SEGSIG_ACTIVATION_VERSION)};
    for (int i = 0; i < 3; i++)
    {
        txTo[i].vin.resize(1);
        txTo[i].vout.resize(1);
        COutPoint changePrevOut = txTo[i].vin[0].GetPrevOut();
        changePrevOut.n = i;
        changePrevOut.setHash(txFrom.GetHash());
        txTo[i].vin[0].SetPrevOut(changePrevOut);
        txTo[i].vout[0].nValue = 1;
    }

    for (int i = 0; i < 3; i++)
    {
        BOOST_CHECK_MESSAGE(SignSignature(accountsToTry, txFrom, txTo[i], 0, SIGHASH_ALL, SignType::Spend), strprintf("SignSignature %d", i));
    }
}

BOOST_AUTO_TEST_SUITE_END()
