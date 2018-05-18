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
#include "wallet/wallet.h"
#include "wallet/wallettx.h"

#include "validation.h"
#include "net.h"
#include "scheduler.h"
#include "timedata.h"
#include "utilmoneystr.h"
#include "init.h"
#include "key.h"
#include "keystore.h"

/**
 * Mark old keypool keys as used,
 * and generate all new keys
 */
bool CWallet::NewKeyPool()
{
    LogPrintf("keypool - newkeypool");
    {
        LOCK(cs_wallet);
        CWalletDB walletdb(*dbw);

        for (auto accountPair : mapAccounts)
        {
            if(!accountPair.second->IsHD())
            {
                for(int64_t nIndex : accountPair.second->setKeyPoolInternal)
                    walletdb.ErasePool(this, nIndex);
                for(int64_t nIndex : accountPair.second->setKeyPoolExternal)
                    walletdb.ErasePool(this, nIndex);

                accountPair.second->setKeyPoolInternal.clear();
                accountPair.second->setKeyPoolExternal.clear();
            }
        }

        if (IsLocked())
            return false;

        //Nothing else to do - the shadow thread will take care of the rest.
    }
    return true;
}

//fixme: (FUT)) GULDEN Note for HD this should actually care more about maintaining a gap above the last used address than it should about the size of the pool.
int CWallet::TopUpKeyPool(unsigned int nTargetKeypoolSize, unsigned int nMaxNewAllocations, CAccount* forAccount)
{
    // Return -1 if we fail to allocate any -and- one of the accounts is not HD -and- it is locked.
    unsigned int nNew = 0;
    bool bAnyNonHDAccountsLockedAndRequireKeys = false;

    LOCK(cs_wallet);

    CWalletDB walletdb(*dbw);

    // Top up key pool
    unsigned int nAccountTargetSize;
    if (nTargetKeypoolSize > 0)
        nAccountTargetSize = nTargetKeypoolSize;
    else
        nAccountTargetSize = GetArg("-keypool", 5);

    //Find current unique highest key index across *all* keypools.
    int64_t nIndex = 1;
    for (auto accountPair : mapAccounts)
    {
        for (auto keyChain : { KEYCHAIN_EXTERNAL, KEYCHAIN_CHANGE })
        {
            auto& keyPool = ( keyChain == KEYCHAIN_EXTERNAL ? accountPair.second->setKeyPoolExternal : accountPair.second->setKeyPoolInternal );
            if (!keyPool.empty())
                nIndex = std::max( nIndex, *(--keyPool.end()) + 1 );
        }
    }

    for (auto accountPair : mapAccounts)
    {
        const auto& accountUUID = accountPair.first;
        auto& account = accountPair.second;
        if ( (forAccount == nullptr) || (forAccount->getUUID() == accountUUID) )
        {
            for (auto& keyChain : { KEYCHAIN_EXTERNAL, KEYCHAIN_CHANGE })
            {
                auto& keyPool = ( keyChain == KEYCHAIN_EXTERNAL ? account->setKeyPoolExternal : account->setKeyPoolInternal );
                while (keyPool.size() < nAccountTargetSize)
                {
                    // We can't allocate any keys here if we are a non HD account that is locked - so don't and instead just signal to caller that there is an issue.
                    if (!account->IsHD() && account->IsLocked())
                    {
                        bAnyNonHDAccountsLockedAndRequireKeys = true;
                    }
                    else
                    {
                        if (!walletdb.WritePool( ++nIndex, CKeyPool(GenerateNewKey(*account, keyChain), getUUIDAsString(accountUUID), keyChain ) ) )
                            throw std::runtime_error(std::string(__func__) + ": writing generated key failed");
                        keyPool.insert(nIndex);
                        LogPrintf("keypool [%s:%s] added key %d, size=%u\n", account->getLabel(), (keyChain == KEYCHAIN_CHANGE ? "change" : "external"), nIndex, keyPool.size());

                        // Limit generation for this loop - rest will be generated later
                        ++nNew;
                        if (nMaxNewAllocations != 0 && nNew >= nMaxNewAllocations)
                            return nNew;
                    }
                }
            }
        }
    }
    if (bAnyNonHDAccountsLockedAndRequireKeys && (nNew == 0))
        return -1;
    return nNew;
}

void CWallet::ReserveKeyFromKeyPool(int64_t& nIndex, CKeyPool& keypoolentry, CAccount* forAccount, int64_t keyChain)
{
    nIndex = -1;
    keypoolentry.vchPubKey = CPubKey();
    {
        LOCK(cs_wallet);

        if (!IsLocked())
            TopUpKeyPool(1, 0, forAccount);//Only assign the bare minimum here, let the background thread do the rest.

        auto& keyPool = ( keyChain == KEYCHAIN_EXTERNAL ? forAccount->setKeyPoolExternal : forAccount->setKeyPoolInternal );

        // Get the oldest key
        if(keyPool.empty())
            return;

        CWalletDB walletdb(*dbw);

        nIndex = *(keyPool.begin());
        keyPool.erase(keyPool.begin());
        if (!walletdb.ReadPool(nIndex, keypoolentry))
            throw std::runtime_error(std::string(__func__) + ": read failed");
        if (!forAccount->HaveKey(keypoolentry.vchPubKey.GetID()))
            throw std::runtime_error(std::string(__func__) + ": unknown key in key pool");
        assert(keypoolentry.vchPubKey.IsValid());
        LogPrintf("keypool reserve %d\n", nIndex);
    }
}

void CWallet::KeepKey(int64_t nIndex)
{
    // Remove from key pool
    CWalletDB walletdb(*dbw);
        walletdb.ErasePool(this, nIndex);
    LogPrintf("keypool keep %d\n", nIndex);
}

//fixme: (2.1) - We should handle this MarkKeyUsed case better, have it broadcast an event to all reserve keys or something.
//And then remove the disk check below
void CWallet::ReturnKey(int64_t nIndex, CAccount* forAccount, int64_t keyChain)
{
    // Return to key pool - but only if it hasn't been used in the interim (If MarkKeyUsed has removed it from disk in the meantime then we don't return it)
    CKeyPool keypoolentry;

    CWalletDB walletdb(*dbw);
    if (!walletdb.ReadPool(nIndex, keypoolentry))
    {
        LogPrintf("keypool return - aborted as key already used %d\n", nIndex);
        return;
    }

    {
        LOCK(cs_wallet);
        auto& keyPool = ( keyChain == KEYCHAIN_EXTERNAL ? forAccount->setKeyPoolExternal : forAccount->setKeyPoolInternal );
        keyPool.insert(nIndex);
    }
    LogPrintf("keypool return %d\n", nIndex);
}

bool CWallet::GetKeyFromPool(CPubKey& result, CAccount* forAccount, int64_t keyChain)
{
    CKeyPool keypool;
    {
        LOCK(cs_wallet);
        int64_t nIndex = 0;
        ReserveKeyFromKeyPool(nIndex, keypool, forAccount, keyChain);
        if (nIndex == -1)
        {
            if (IsLocked()) return false;
            result = GenerateNewKey(*forAccount, keyChain);
            return true;
        }
        KeepKey(nIndex);
        result = keypool.vchPubKey;
    }
    return true;
}

int64_t CWallet::GetOldestKeyPoolTime()
{
    LOCK(cs_wallet);


    // load oldest key from keypool, get time and return
    CKeyPool keypoolentry;
    CWalletDB walletdb(*dbw);

    // if the keypool is empty, return <NOW>
    int64_t nTime = GetTime();
    for (const auto& accountItem : mapAccounts)
    {
        for (auto keyChain : { KEYCHAIN_EXTERNAL, KEYCHAIN_CHANGE })
        {
            const auto& keyPool = ( keyChain == KEYCHAIN_EXTERNAL ? accountItem.second->setKeyPoolExternal : accountItem.second->setKeyPoolInternal );
            if(!keyPool.empty())
            {
                int64_t nIndex = *(keyPool.begin());
                if (!walletdb.ReadPool(nIndex, keypoolentry))
                throw std::runtime_error(std::string(__func__) + ": read oldest key in keypool failed");
                assert(keypoolentry.vchPubKey.IsValid());
                nTime = std::min(nTime, keypoolentry.nTime);
            }
        }
    }
    return nTime;
}
