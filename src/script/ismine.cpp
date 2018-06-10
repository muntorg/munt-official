// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// File contains modifications by: The Gulden developers
// All modifications:
// Copyright (c) 2016-2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#include "ismine.h"

#include "key.h"
#include "keystore.h"
#include "script/script.h"
#include "script/standard.h"
#include "script/sign.h"
#include <wallet/wallet.h>



typedef std::vector<unsigned char> valtype;

unsigned int HaveKeys(const std::vector<valtype>& pubkeys, const CKeyStore& keystore)
{
    unsigned int nResult = 0;
    for(const valtype& pubkey : pubkeys)
    {
        CKeyID keyID = CPubKey(pubkey).GetID();
        if (keystore.HaveKey(keyID))
            ++nResult;
    }
    return nResult;
}

unsigned int HaveKeys(const std::vector<valtype>& pubkeys, const CWallet& wallet)
{
    unsigned int nResult = 0;
    for (const auto& accountPair : wallet.mapAccounts)
        nResult += HaveKeys(pubkeys, *accountPair.second);
    return nResult;
}

isminetype IsMine(const CKeyStore& keystore, const CScript& scriptPubKey, SigVersion sigversion)
{
    bool isInvalid = false;
    return IsMine(keystore, scriptPubKey, isInvalid, sigversion);
}

isminetype IsMine(const CKeyStore& keystore, const CTxDestination& dest, SigVersion sigversion)
{
    bool isInvalid = false;
    return IsMine(keystore, dest, isInvalid, sigversion);
}

isminetype IsMine(const CKeyStore &keystore, const CTxDestination& dest, bool& isInvalid, SigVersion sigversion)
{
    CScript script = GetScriptForDestination(dest);
    return IsMine(keystore, script, isInvalid, sigversion);
}

isminetype IsMine(const CKeyStore &keystore, const CScript& scriptPubKey, bool& isInvalid, SigVersion sigversion)
{
    std::vector<valtype> vSolutions;
    txnouttype whichType;
    if (!Solver(scriptPubKey, whichType, vSolutions)) {
        if (keystore.HaveWatchOnly(scriptPubKey))
            return ISMINE_WATCH_UNSOLVABLE;
        return ISMINE_NO;
    }

    CKeyID keyID;
    switch (whichType)
    {
    case TX_NONSTANDARD:
    case TX_NULL_DATA:
        break;
    case TX_PUBKEY:
        keyID = CPubKey(vSolutions[0]).GetID();
        if (sigversion != SIGVERSION_BASE && vSolutions[0].size() != 33) {
            isInvalid = true;
            return ISMINE_NO;
        }
        if (keystore.HaveKey(keyID))
            return ISMINE_SPENDABLE;
        break;
    case TX_PUBKEYHASH:
        keyID = CKeyID(uint160(vSolutions[0]));
        if (sigversion != SIGVERSION_BASE) {
            CPubKey pubkey;
            if (keystore.GetPubKey(keyID, pubkey) && !pubkey.IsCompressed()) {
                isInvalid = true;
                return ISMINE_NO;
            }
        }
        if (keystore.HaveKey(keyID))
            return ISMINE_SPENDABLE;
        break;
    case TX_SCRIPTHASH:
    {
        CScriptID scriptID = CScriptID(uint160(vSolutions[0]));
        CScript subscript;
        if (keystore.GetCScript(scriptID, subscript)) {
            isminetype ret = IsMine(keystore, subscript, isInvalid);
            if (ret == ISMINE_SPENDABLE || ret == ISMINE_WATCH_SOLVABLE || (ret == ISMINE_NO && isInvalid))
                return ret;
        }
        break;
    }

    case TX_MULTISIG:
    {
        // Only consider transactions "mine" if we own ALL the
        // keys involved. Multi-signature transactions that are
        // partially owned (somebody else has a key that can spend
        // them) enable spend-out-from-under-you attacks, especially
        // in shared-wallet situations.
        std::vector<valtype> keys(vSolutions.begin()+1, vSolutions.begin()+vSolutions.size()-1);
        if (sigversion != SIGVERSION_BASE) {
            for (size_t i = 0; i < keys.size(); i++) {
                if (keys[i].size() != 33) {
                    isInvalid = true;
                    return ISMINE_NO;
                }
            }
        }
        if (HaveKeys(keys, keystore) == keys.size())
            return ISMINE_SPENDABLE;
        break;
    }
    case TX_PUBKEYHASH_POW2WITNESS:
        CKeyID spendingKeyID = CKeyID(uint160(vSolutions[0]));
        CKeyID witnessKeyID = CKeyID(uint160(vSolutions[1]));
        if (sigversion != SIGVERSION_BASE) {
            CPubKey pubkey;
            if (keystore.GetPubKey(spendingKeyID, pubkey) && !pubkey.IsCompressed()) {
                isInvalid = true;
                return ISMINE_NO;
            }
        }
        if (keystore.HaveKey(spendingKeyID))
            return ISMINE_SPENDABLE;
        //fixme: (2.0) Need new ismine type here.?
        if (keystore.HaveKey(witnessKeyID))
            return ISMINE_SPENDABLE;
        break;
    }

    //fixme: (Post-2.1) (WATCHONLY)
    /*if (keystore.HaveWatchOnly(scriptPubKey)) {
        // TODO: This could be optimized some by doing some work after the above solver
        SignatureData sigs;
        return ProduceSignature(DummySignatureCreator(&keystore), scriptPubKey, sigs) ? ISMINE_WATCH_SOLVABLE : ISMINE_WATCH_UNSOLVABLE;
    }*/
    return ISMINE_NO;
}

isminetype IsMine(const CKeyStore &keystore, const CTxOut& txout)
{
    switch (txout.GetType())
    {
        case CTxOutType::ScriptLegacyOutput:
            return IsMine(keystore, txout.output.scriptPubKey);
        case CTxOutType::PoW2WitnessOutput:
        {
            if (keystore.HaveKey(txout.output.witnessDetails.spendingKeyID))
                return ISMINE_SPENDABLE;
            //fixme: (2.0) Need new ismine type here.?
            if (keystore.HaveKey(txout.output.witnessDetails.witnessKeyID))
                return ISMINE_SPENDABLE;
            break;
        }
        case CTxOutType::StandardKeyHashOutput:
        {
            if (keystore.HaveKey(txout.output.standardKeyHash.keyID))
                return ISMINE_SPENDABLE;
            break;
        }
    }
    return ISMINE_NO;
}

isminetype RemoveAddressFromKeypoolIfIsMine(CWallet& keystore, const CTxOut& txout, uint64_t time)
{
    switch (txout.GetType())
    {
        case CTxOutType::ScriptLegacyOutput:
            return RemoveAddressFromKeypoolIfIsMine(keystore, txout.output.scriptPubKey, time);
        case CTxOutType::PoW2WitnessOutput:
        {
            bool haveSpendingKey = keystore.HaveKey(txout.output.witnessDetails.spendingKeyID);
            bool haveWitnessKey = keystore.HaveKey(txout.output.witnessDetails.witnessKeyID);

            if (haveSpendingKey)
                keystore.MarkKeyUsed(txout.output.witnessDetails.spendingKeyID, time);
            if (haveWitnessKey)
                keystore.MarkKeyUsed(txout.output.witnessDetails.witnessKeyID, time);

            //fixme: (2.0) Need new ismine type here.?
            if (haveSpendingKey)
                return ISMINE_SPENDABLE;
            if (haveWitnessKey)
                return ISMINE_SPENDABLE;
            break;
        }
        case CTxOutType::StandardKeyHashOutput:
        {
            if (keystore.HaveKey(txout.output.standardKeyHash.keyID))
            {
                keystore.MarkKeyUsed(txout.output.standardKeyHash.keyID, time);
                return ISMINE_SPENDABLE;
            }
            break;
        }
    }
    return ISMINE_NO;
}


















isminetype RemoveAddressFromKeypoolIfIsMine(CWallet& keystore, const CScript& scriptPubKey, uint64_t time, SigVersion sigversion)
{
    bool isInvalid = false;
    return RemoveAddressFromKeypoolIfIsMine(keystore, scriptPubKey, time, isInvalid, sigversion);
}

isminetype RemoveAddressFromKeypoolIfIsMine(CWallet& keystore, const CScript& scriptPubKey, uint64_t time, bool& isInvalid, SigVersion sigversion)
{
    std::vector<valtype> vSolutions;
    txnouttype whichType;
    if (!Solver(scriptPubKey, whichType, vSolutions)) {
        if (keystore.HaveWatchOnly(scriptPubKey))
            return ISMINE_WATCH_UNSOLVABLE;
        return ISMINE_NO;
    }

    CKeyID keyID;
    switch (whichType)
    {
    case TX_NONSTANDARD:
    case TX_NULL_DATA:
        break;
    case TX_PUBKEY:
        keyID = CPubKey(vSolutions[0]).GetID();
        if (sigversion != SIGVERSION_BASE && vSolutions[0].size() != 33) {
            isInvalid = true;
            return ISMINE_NO;
        }
        if (keystore.HaveKey(keyID))
        {
            keystore.MarkKeyUsed(keyID, time);
            return ISMINE_SPENDABLE;
        }
        break;
    case TX_PUBKEYHASH:
        keyID = CKeyID(uint160(vSolutions[0]));
        if (sigversion != SIGVERSION_BASE) {
            CPubKey pubkey;
            if (keystore.GetPubKey(keyID, pubkey) && !pubkey.IsCompressed()) {
                isInvalid = true;
                return ISMINE_NO;
            }
        }
        if (keystore.HaveKey(keyID))
        {
            keystore.MarkKeyUsed(keyID, time);
            return ISMINE_SPENDABLE;
        }
        break;
    case TX_SCRIPTHASH:
    {
        CScriptID scriptID = CScriptID(uint160(vSolutions[0]));
        CScript subscript;
        if (keystore.GetCScript(scriptID, subscript)) {
            isminetype ret = RemoveAddressFromKeypoolIfIsMine(keystore, subscript, time, isInvalid);
            if (ret == ISMINE_SPENDABLE || ret == ISMINE_WATCH_SOLVABLE || (ret == ISMINE_NO && isInvalid))
                return ret;
        }
        break;
    }

    case TX_MULTISIG:
    {
        // Only consider transactions "mine" if we own ALL the
        // keys involved. Multi-signature transactions that are
        // partially owned (somebody else has a key that can spend
        // them) enable spend-out-from-under-you attacks, especially
        // in shared-wallet situations.
        std::vector<valtype> keys(vSolutions.begin()+1, vSolutions.begin()+vSolutions.size()-1);
        if (sigversion != SIGVERSION_BASE) {
            for (size_t i = 0; i < keys.size(); i++) {
                if (keys[i].size() != 33) {
                    isInvalid = true;
                    return ISMINE_NO;
                }
            }
        }
        if (HaveKeys(keys, keystore) == keys.size())
        {
            for (auto& key : keys)
            {
                keystore.MarkKeyUsed(CPubKey(key).GetID(), time);
            }
            return ISMINE_SPENDABLE;
        }
        break;
    }
    case TX_PUBKEYHASH_POW2WITNESS:
        CKeyID spendingKeyID = CKeyID(uint160(vSolutions[0]));
        CKeyID witnessKeyID = CKeyID(uint160(vSolutions[1]));
        if (sigversion != SIGVERSION_BASE) {
            CPubKey pubkey;
            if (keystore.GetPubKey(spendingKeyID, pubkey) && !pubkey.IsCompressed()) {
                isInvalid = true;
                return ISMINE_NO;
            }
        }

        bool haveSpendingKey = keystore.HaveKey(spendingKeyID);
        bool haveWitnessKey = keystore.HaveKey(witnessKeyID);
        if (haveSpendingKey)
            keystore.MarkKeyUsed(spendingKeyID, time);
        if (haveWitnessKey)
            keystore.MarkKeyUsed(witnessKeyID, time);

        if (haveSpendingKey)
            return ISMINE_SPENDABLE;
        //fixme: (2.0) Need new ismine type here.?
        if (haveWitnessKey)
            return ISMINE_SPENDABLE;
        break;
    }

    /*
     //fixme: (Post-2.1) (WATCHONLY)
      if (keystore.HaveWatchOnly(scriptPubKey)) {
        // TODO: This could be optimized some by doing some work after the above solver
        SignatureData sigs;
        return ProduceSignature(DummySignatureCreator(&keystore), scriptPubKey, sigs) ? ISMINE_WATCH_SOLVABLE : ISMINE_WATCH_UNSOLVABLE;
    }*/
    return ISMINE_NO;
}
