// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "ismine.h"

#include "key.h"
#include "keystore.h"
#include "script/script.h"
#include "script/standard.h"
#include "script/sign.h"
#include "wallet/wallet.h"

#include <boost/foreach.hpp>

using namespace std;

typedef vector<unsigned char> valtype;

unsigned int HaveKeys(const vector<valtype>& pubkeys, const CKeyStore& keystore)
{
    unsigned int nResult = 0;
    BOOST_FOREACH(const valtype& pubkey, pubkeys)
    {
        CKeyID keyID = CPubKey(pubkey).GetID();
        if (keystore.HaveKey(keyID))
            ++nResult;
    }
    return nResult;
}

isminetype IsMine(const CKeyStore &keystore, const CTxDestination& dest)
{
    CScript script = GetScriptForDestination(dest);
    return IsMine(keystore, script);
}


isminetype RemoveAddressFromKeypoolIfIsMine(CWallet &wallet, const CTxDestination& dest, uint64_t time)
{
    LOCK(wallet.cs_wallet);
    
    isminetype ret = isminetype::ISMINE_NO;
    for (const auto& accountItem : wallet.mapAccounts)
    {
        for (auto keyChain : { KEYCHAIN_EXTERNAL, KEYCHAIN_CHANGE })
        {
            isminetype temp = ( keyChain == KEYCHAIN_EXTERNAL ? RemoveAddressFromKeypoolIfIsMine(wallet, accountItem.second->externalKeyStore, dest, time) : RemoveAddressFromKeypoolIfIsMine(wallet, accountItem.second->internalKeyStore, dest, time) );
            if (temp > ret)
                ret = temp;
        }
    }
    return ret;
}

isminetype RemoveAddressFromKeypoolIfIsMine(CWallet& wallet, const CKeyStore &keystore, const CTxDestination& dest, uint64_t time)
{
    CScript script = GetScriptForDestination(dest);
    return RemoveAddressFromKeypoolIfIsMine(wallet, keystore, script, time);
}

isminetype IsMine(const CKeyStore &keystore, const CTxOut& txout)
{
    return IsMine(keystore, txout.scriptPubKey);
}

isminetype RemoveAddressFromKeypoolIfIsMine(CWallet &wallet, const CKeyStore& keystore, const CTxOut& txout, uint64_t time)
{
    return RemoveAddressFromKeypoolIfIsMine(wallet, keystore, txout.scriptPubKey, time);
}

isminetype RemoveAddressFromKeypoolIfIsMine(CWallet &wallet, const CTxOut& txout, uint64_t time)
{
    LOCK(wallet.cs_wallet);
    
    isminetype ret = isminetype::ISMINE_NO;
    for (const auto& accountItem : wallet.mapAccounts)
    {
        for (auto keyChain : { KEYCHAIN_EXTERNAL, KEYCHAIN_CHANGE })
        {
            isminetype temp = ( keyChain == KEYCHAIN_EXTERNAL ? RemoveAddressFromKeypoolIfIsMine(wallet, accountItem.second->externalKeyStore, txout, time) : RemoveAddressFromKeypoolIfIsMine(wallet, accountItem.second->internalKeyStore, txout, time) );
            if (temp > ret)
                ret = temp;
        }
    }
    return ret;
}





isminetype IsMine(const CKeyStore &keystore, const CScript& scriptPubKey)
{
    vector<valtype> vSolutions;
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
        if (keystore.HaveKey(keyID))
            return ISMINE_SPENDABLE;
        break;
    case TX_PUBKEYHASH:
    case TX_WITNESS_V0_KEYHASH:
        keyID = CKeyID(uint160(vSolutions[0]));
        if (keystore.HaveKey(keyID))
            return ISMINE_SPENDABLE;
        break;
    case TX_SCRIPTHASH:
    {
        CScriptID scriptID = CScriptID(uint160(vSolutions[0]));
        CScript subscript;
        if (keystore.GetCScript(scriptID, subscript)) {
            isminetype ret = IsMine(keystore, subscript);
            if (ret == ISMINE_SPENDABLE)
                return ret;
        }
        break;
    }
    case TX_WITNESS_V0_SCRIPTHASH:
    {
        uint160 hash;
        CRIPEMD160().Write(&vSolutions[0][0], vSolutions[0].size()).Finalize(hash.begin());
        CScriptID scriptID = CScriptID(hash);
        CScript subscript;
        if (keystore.GetCScript(scriptID, subscript)) {
            isminetype ret = IsMine(keystore, subscript);
            if (ret == ISMINE_SPENDABLE)
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
        vector<valtype> keys(vSolutions.begin()+1, vSolutions.begin()+vSolutions.size()-1);
        if (HaveKeys(keys, keystore) == keys.size())
            return ISMINE_SPENDABLE;
        break;
    }
    }

    if (keystore.HaveWatchOnly(scriptPubKey)) {
        // TODO: This could be optimized some by doing some work after the above solver
        SignatureData sigs;
        return ProduceSignature(DummySignatureCreator(&keystore), scriptPubKey, sigs) ? ISMINE_WATCH_SOLVABLE : ISMINE_WATCH_UNSOLVABLE;
    }
    return ISMINE_NO;
}


isminetype RemoveAddressFromKeypoolIfIsMine(CWallet &wallet, const CScript& scriptPubKey, uint64_t time)
{
    LOCK(wallet.cs_wallet);
    
    isminetype ret = isminetype::ISMINE_NO;
    for (const auto& accountItem : wallet.mapAccounts)
    {
        for (auto keyChain : { KEYCHAIN_EXTERNAL, KEYCHAIN_CHANGE })
        {
            isminetype temp = ( keyChain == KEYCHAIN_EXTERNAL ? RemoveAddressFromKeypoolIfIsMine(wallet, accountItem.second->externalKeyStore, scriptPubKey, time) : RemoveAddressFromKeypoolIfIsMine(wallet, accountItem.second->internalKeyStore, scriptPubKey, time) );
            if (temp > ret)
                ret = temp;
        }
    }
    return ret;
}

isminetype RemoveAddressFromKeypoolIfIsMine(CWallet &wallet, const CKeyStore &keystore, const CScript& scriptPubKey, uint64_t time)
{
    vector<valtype> vSolutions;
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
        if (keystore.HaveKey(keyID))
        {
            wallet.MarkKeyUsed(keyID, time);
            return ISMINE_SPENDABLE;
        }
        break;
    case TX_PUBKEYHASH:
    case TX_WITNESS_V0_KEYHASH:
        keyID = CKeyID(uint160(vSolutions[0]));
        if (keystore.HaveKey(keyID))
        {
            wallet.MarkKeyUsed(keyID, time);
            return ISMINE_SPENDABLE;
        }
        break;
    case TX_SCRIPTHASH:
    {
        CScriptID scriptID = CScriptID(uint160(vSolutions[0]));
        CScript subscript;
        if (keystore.GetCScript(scriptID, subscript)) {
            //fixme: GUlden BIP44
            isminetype ret = IsMine(keystore, subscript);
            if (ret == ISMINE_SPENDABLE)
                return ret;
        }
        break;
    }
    case TX_WITNESS_V0_SCRIPTHASH:
    {
        uint160 hash;
        CRIPEMD160().Write(&vSolutions[0][0], vSolutions[0].size()).Finalize(hash.begin());
        CScriptID scriptID = CScriptID(hash);
        CScript subscript;
        //fixme: GUlden (BIP44)
        if (keystore.GetCScript(scriptID, subscript)) {
            isminetype ret = IsMine(keystore, subscript);
            if (ret == ISMINE_SPENDABLE)
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
        //fixme: GUlden BIP44
        vector<valtype> keys(vSolutions.begin()+1, vSolutions.begin()+vSolutions.size()-1);
        if (HaveKeys(keys, keystore) == keys.size())
            return ISMINE_SPENDABLE;
        break;
    }
    }

    if (keystore.HaveWatchOnly(scriptPubKey)) {
        // TODO: This could be optimized some by doing some work after the above solver
        SignatureData sigs;
        return ProduceSignature(DummySignatureCreator(&keystore), scriptPubKey, sigs) ? ISMINE_WATCH_SOLVABLE : ISMINE_WATCH_UNSOLVABLE;
    }
    return ISMINE_NO;
}
