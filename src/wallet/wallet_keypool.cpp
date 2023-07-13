// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// File contains modifications by: The Centure developers
// All modifications:
// Copyright (c) 2016-2022 The Centure developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the GNU Lesser General Public License v3, see the accompanying
// file COPYING
#include "wallet/wallet.h"
#include "wallet/wallettx.h"

#include "alert.h"

#include "validation/validation.h"
#include "net.h"
#include "scheduler.h"
#include "timedata.h"
#include "util/moneystr.h"
#include "init.h"
#include "key.h"
#include "keystore.h"
#include "witnessutil.h"

/**
 * Mark old keypool keys as used,
 * and generate all new keys
 */
bool CWallet::NewKeyPool()
{
    LogPrintf("keypool - newkeypool\n");
    {
        LOCK(cs_wallet);
        CWalletDB walletdb(*dbw);

        for (const auto& [accountUUID, forAccount] : mapAccounts)
        {
            (unused) accountUUID;
            if(!forAccount->IsHD())
            {
                for(int64_t nIndex : forAccount->setKeyPoolInternal)
                    walletdb.ErasePool(this, nIndex);
                for(int64_t nIndex : forAccount->setKeyPoolExternal)
                    walletdb.ErasePool(this, nIndex);

                forAccount->setKeyPoolInternal.clear();
                forAccount->setKeyPoolExternal.clear();
            }
        }

        if (IsLocked())
            return false;

        //Nothing else to do - the shadow thread will take care of the rest.
    }
    return true;
}

//fixme: (FUT) (ACCOUNTS) MUNT Note for HD this should actually care more about maintaining a gap above the last used address than it should about the size of the pool.
int CWallet::TopUpKeyPool(unsigned int nTargetKeypoolSize, unsigned int nMaxNewAllocations, CAccount* topupForAccount, unsigned int nMinimalKeypoolOverride)
{
    // Return -1 if we fail to allocate any -and- one of the accounts is not HD -and- it is locked.
    uint32_t nNew = 0;
    bool bAnyNonHDAccountsLockedAndRequireKeys = false;

    // enclosed in block to ensure that NotifyKeyPoolToppedUp (at end) is called outside the locks
    {
        LOCK2(cs_main, cs_wallet);

        CWalletDB walletdb(*dbw);

        // Top up key pool
        uint32_t nAccountTargetSize;
        if (nTargetKeypoolSize > 0)
            nAccountTargetSize = nTargetKeypoolSize;
        else
            nAccountTargetSize = GetArg("-keypool", 5);

        //Find current unique highest key index across *all* keypools.
        int64_t nIndex = 1;
        for (const auto& [accountUUID, loopForAccount] : mapAccounts)
        {
            (unused) accountUUID;
            for (auto keyChain : { KEYCHAIN_EXTERNAL, KEYCHAIN_CHANGE })
            {
                auto& keyPool = ( keyChain == KEYCHAIN_EXTERNAL ? loopForAccount->setKeyPoolExternal : loopForAccount->setKeyPoolInternal );
                if (!keyPool.empty())
                    nIndex = std::max( nIndex, *(--keyPool.end()) + 1 );
            }
        }

        for (const auto& [accountUUID, loopForAccount] : mapAccounts)
        {
            if ( (topupForAccount == nullptr) || (topupForAccount->getUUID() == accountUUID) )
            {
                // If account uses a fixed keypool then never generate new keys to add to it.
                // NB! This is one of the few places where IsMinimalKeyPool differed from IsFixedKeyPool - we do still generate new addresses for IsMinimalKeyPool (though we keep the size at 1)
                if (loopForAccount->IsFixedKeyPool())
                    continue;

                uint32_t nFinalAccountTargetSize = nAccountTargetSize;
                if (loopForAccount->IsMinimalKeyPool())
                {
                    nFinalAccountTargetSize = nMinimalKeypoolOverride;
                }

                for (auto& keyChain : { KEYCHAIN_EXTERNAL, KEYCHAIN_CHANGE })
                {
                    auto& keyPool = ( keyChain == KEYCHAIN_EXTERNAL ? loopForAccount->setKeyPoolExternal : loopForAccount->setKeyPoolInternal );
                    while (keyPool.size() < nFinalAccountTargetSize && (nMaxNewAllocations == 0 || nNew < nMaxNewAllocations))
                    {
                        // We can't allocate any keys here if we are a non HD account that is locked - so don't and instead just signal to caller that there is an issue.
                        if (!loopForAccount->IsHD() && loopForAccount->IsLocked())
                        {
                            bAnyNonHDAccountsLockedAndRequireKeys = true;
                            break;
                        }
                        else
                        {
                            if (!walletdb.WritePool( ++nIndex, CKeyPool(GenerateNewKey(*loopForAccount, keyChain), getUUIDAsString(accountUUID), keyChain ) ) )
                                throw std::runtime_error(std::string(__func__) + ": writing generated key failed");
                            keyPool.insert(nIndex);
                            LogPrintf("keypool [%s:%s] added key %d, size=%u\n", loopForAccount->getLabel(), (keyChain == KEYCHAIN_CHANGE ? "change" : "external"), nIndex, keyPool.size());

                            // Limit generation for this loop, keeping count using nNew - rest will be generated later
                            ++nNew;
                        }
                    }
                }
            }
        }
    }
    if (bAnyNonHDAccountsLockedAndRequireKeys && (nNew == 0))
        return -1;

    if (nNew > 0)
        NotifyKeyPoolToppedUp();

    return nNew;
}

void CWallet::ReserveKeyFromKeyPool(int64_t& nIndex, CKeyPool& keypoolentry, CAccount* forAccount, int64_t keyChain)
{
    nIndex = -1;
    keypoolentry.vchPubKey = CPubKey();
    {
        LOCK2(cs_main, cs_wallet);

        if (!IsLocked())
            TopUpKeyPool(1, 0, forAccount);//Only assign the bare minimum here, let the background thread do the rest.

        auto& keyPool = ( keyChain == KEYCHAIN_EXTERNAL ? forAccount->setKeyPoolExternal : forAccount->setKeyPoolInternal );

        // Get the oldest key
        if(keyPool.empty())
            return;

        CWalletDB walletdb(*dbw);

        nIndex = *(keyPool.begin());
        // If account uses a fixed or minimal keypool then never remove keys from it.
        if (!forAccount->IsFixedKeyPool() && !forAccount->IsMinimalKeyPool())
        {
            keyPool.erase(keyPool.begin());
        }
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

//fixme: (FUT) (ACCOUNTS) - We should handle this MarkKeyUsed case better, have it broadcast an event to all reserve keys or something.
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
        if (nIndex == -1 )
        {
            // If account uses a fixed or minimal keypool then never generate new keys for it here
            // In the case of minimal keypools we do allow generation, but only when explicitely requested by the user
            if (!forAccount->IsFixedKeyPool() && !forAccount->IsMinimalKeyPool())
            {
                if (IsLocked()) return false;
                result = GenerateNewKey(*forAccount, keyChain);
                return true;
            }
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
    for (const auto& [accountUUID, forAccount] : mapAccounts)
    {
        (unused) accountUUID;
        for (auto keyChain : { KEYCHAIN_EXTERNAL, KEYCHAIN_CHANGE })
        {
            const auto& keyPool = ( keyChain == KEYCHAIN_EXTERNAL ? forAccount->setKeyPoolExternal : forAccount->setKeyPoolInternal );
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

void CWallet::importWitnessOnlyAccountFromURL(const SecureString& sKey, const std::string sAccountName)
{
    //fixme: (PHASE5) Handle error here more appropriately etc.
    std::vector<std::pair<CKey, uint64_t>> keysAndBirthDates;
    bool urlError = false;
    try
    {
        keysAndBirthDates = ParseWitnessKeyURL(sKey.c_str());
        if (keysAndBirthDates.empty())
        {
            urlError = true;
        }
    }
    catch(...)
    {
        urlError = true;
    }
    if (urlError)
    {
        std::string strErrorMessage = _("Invalid witness URL");
        LogPrintf(strErrorMessage.c_str());
        CAlert::Notify(strErrorMessage, true, true);
        return;
    }

    CreateWitnessOnlyWitnessAccount(sAccountName, keysAndBirthDates);
}

void CWallet::importPrivKey(const SecureString& sKey, std::string sAccountName)
{
    CEncodedSecretKey vchSecret;
    bool fGood = vchSecret.SetString(sKey.c_str());

    if (fGood)
    {
        LOCK2(cs_main, pactiveWallet->cs_wallet);

        CKey privKey = vchSecret.GetKey();
        if (!privKey.IsValid())
        {
            std::string strErrorMessage = _("Error importing private key") + "\n" + _("Invalid private key.");
            LogPrintf(strErrorMessage.c_str());
            CAlert::Notify(strErrorMessage, true, true);
            return;
        }

        importPrivKey(privKey, sAccountName);
    }
}

void CWallet::importPrivKey(const CKey& privKey, std::string sAccountName)
{
    CPubKey pubkey = privKey.GetPubKey();
    assert(privKey.VerifyPubKey(pubkey));

    CKeyID importKeyID = pubkey.GetID();

    //Don't import an address that is already in wallet.
    if (pactiveWallet->HaveKey(importKeyID))
    {
        std::string strErrorMessage = _("Error importing private key") + "\n" + _("Wallet already contains key.");
        LogPrintf(strErrorMessage.c_str());
        CAlert::Notify(strErrorMessage, true, true);
        return;
    }

    CAccount* newAccount = new CAccount();
    newAccount->m_Type = ImportedPrivateKeyAccount;

    if (IsCrypted())
    {
        if (IsLocked())
        {
            std::string strErrorMessage = _("Error importing private key") + "\n" + ("Wallet locked.");
            LogPrintf(strErrorMessage.c_str());
            CAlert::Notify(strErrorMessage, true, true);
        }

        if (!activeAccount)
        {
            std::string strErrorMessage = _("Error importing private key") + "\n" + ("Unable to obtain encyption key.");
            LogPrintf(strErrorMessage.c_str());
            CAlert::Notify(strErrorMessage, true, true);
        }

        if (!newAccount->Encrypt(activeAccount->vMasterKey))
        {
            std::string strErrorMessage = _("Error importing private key") + "\n" + ("Failed to encrypt new account.");
            LogPrintf(strErrorMessage.c_str());
            CAlert::Notify(strErrorMessage, true, true);
        }
    }

    pactiveWallet->addAccount(newAccount, sAccountName);

    //fixme: (FUT) (ACCOUNTS) - Optionally take a key birth date here.
    if (!importPrivKeyIntoAccount(newAccount, privKey, importKeyID, 1))
    {
        //Error messages aleady handled internally by importPrivKeyIntoAccount.
        //fixme: (FUT) (ACCOUNTS) we should delete the account in this scenario.
        return;
    }

    //fixme: (FUT) (ACCOUNTS) Delete account on failure.
}

bool CWallet::forceKeyIntoKeypool(CAccount* forAccount, const CKey& privKeyToInsert)
{
    if (forAccount->IsHD())
    {
        std::string strError = "forceKeyIntoKeypool called on an HD account. Please contact a developer and let them know what you were doing at the time so that they can look into the issue.";
        LogPrintf(strError.c_str());
        CAlert::Notify(strError, true, true);
        return false;
    }

    LOCK(cs_wallet);
    CWalletDB walletdb(*dbw);

    //Find current unique highest key index across *all* keypools.
    int64_t nIndex = 1;
    for (const auto& [accountUUID, forAccount] : mapAccounts)
    {
        (unused) accountUUID;
        for (auto keyChain : { KEYCHAIN_EXTERNAL, KEYCHAIN_CHANGE })
        {
            auto& keyPool = ( keyChain == KEYCHAIN_EXTERNAL ? forAccount->setKeyPoolExternal : forAccount->setKeyPoolInternal );
            if (!keyPool.empty())
                nIndex = std::max( nIndex, *(--keyPool.end()) + 1 );
        }
    }

    //fixme: (FUT) (ACCOUNTS) Add some key metadata here as well?

    CPubKey pubKeyToInsert = privKeyToInsert.GetPubKey();
    if (!AddKeyPubKey(privKeyToInsert, pubKeyToInsert, *forAccount, KEYCHAIN_EXTERNAL))
    {
        std::string strError = "Failed to add public key, perhaps it already exists in another account.";
        LogPrintf(strError.c_str());
        CAlert::Notify(strError, true, true);
        return false;
    }
    if (!walletdb.WritePool( ++nIndex, CKeyPool(pubKeyToInsert, getUUIDAsString(forAccount->getUUID()), KEYCHAIN_EXTERNAL ) ) )
    {
        std::string strError = "Failed to write key to pool. Please contact a developer and let them know what you were doing at the time so that they can look into the issue.";
        LogPrintf(strError.c_str());
        CAlert::Notify(strError, true, true);
        return false;
    }
    forAccount->setKeyPoolExternal.insert(nIndex);
    LogPrintf("keypool [%s:external] added imported key %d, size=%u\n", forAccount->getLabel(), nIndex, forAccount->setKeyPoolExternal.size());
    return true;
}

bool CWallet::importPrivKeyIntoAccount(CAccount* targetAccount, const CKey& privKey, const CKeyID& importKeyID, uint64_t keyBirthDate, bool allowRescan)
{
    assert(!targetAccount->IsHD());

    pactiveWallet->MarkDirty();
    pactiveWallet->mapKeyMetadata[importKeyID].nCreateTime = 1;

    // Force key into the keypool
    if (!forceKeyIntoKeypool(targetAccount, privKey))
    {
        return false;
    }

    // Whenever a key is imported, we need to scan the whole chain from birth date - do so now
    pactiveWallet->nTimeFirstKey = std::min(pactiveWallet->nTimeFirstKey, keyBirthDate);
    if (allowRescan)
    {
        ResetSPVStartRescanThread();
    }

    return true;
}
