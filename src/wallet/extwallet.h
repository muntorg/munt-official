// Copyright (c) 2016-2022 The Centure developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the Libre Chain License, see the accompanying
// file COPYING

#ifndef EXT_WALLET_H
#define EXT_WALLET_H

#include "amount.h"
#include "streams.h"
#include "tinyformat.h"
#include "ui_interface.h"
#include "util/strencodings.h"
#include "validation/validationinterface.h"
#include "script/ismine.h"
#include "script/sign.h"
#include "wallet/crypter.h"
#include "wallet/walletdb.h"
#include "wallet/rpcwallet.h"

#include <algorithm>
#include <atomic>
#include <map>
#include <set>
#include <stdexcept>
#include <stdint.h>
#include <string>
#include <utility>
#include <vector>

//Gulden specific includes
#include "wallet/walletdberrors.h"
#include "account.h"

#include <boost/thread.hpp>


void StartShadowPoolManagerThread(boost::thread_group& threadGroup);

//fixme: (FUT) (ACCOUNTS) (LOW) Make configurable option
extern bool fShowChildAccountsSeperately;

std::string accountNameForAddress(const CWallet &wallet, const CTxDestination& dest);
isminetype IsMine(const CWallet &wallet, const CTxDestination& dest);
isminetype IsMine(const CWallet &wallet, const CTxOut& out);
isminetype IsMineWitness(const CWallet &wallet, const CTxOut& out);

/** 
 * A CExtWallet maintains a set of transactions and balances
 * and provides the ability to create new transactions.
 * it contains one or more accounts, which are responsible for creating/allocating/managing keys via their keystore interfaces.
 */
class CExtWallet : public CValidationInterface
{
public:
    CExtWallet() : dbw(new CWalletDBWrapper()){};
    CExtWallet(std::unique_ptr<CWalletDBWrapper> dbw_in) : dbw(std::move(dbw_in)){};
    virtual ~CExtWallet(){};

    //Members that are shared with CWallet.
    mutable RecursiveMutex cs_wallet;
    uint64_t nTimeFirstKey = 0;
    //const std::string strWalletFile;
    std::unique_ptr<CWalletDBWrapper> dbw;

    typedef std::pair<CWalletTx*, CAccountingEntry*> TxPair;
    typedef std::multimap<int64_t, TxPair > TxItems;
    TxItems wtxOrdered;

    /**
     * Lock wallet keys. If any unlock sessions are in progress the lock will be delayed until there are no
     * open unlock sessions.
     * @return succesfully locked wallet. This value seems of little use as it will always return true if locking is delayed.
     */
    bool Lock();

    /**
     * Actual lock operation, unconditionally locking the wallet, ie. not depending on unlock sessions or anything.
     */
    bool LockHard();


    bool UnlockWithMasterKey(const CKeyingMaterial& vMasterKeyIn) const
    {
        LOCK(cs_wallet);
        bool ret = true;
        for (const auto& [accountUUID, forAccount] : mapAccounts)
        {
            bool needsWriteToDisk = false;
            if (!forAccount->Unlock(vMasterKeyIn, needsWriteToDisk))
            {
                LogPrintf("CWallet::UnlockWithMasterKey - Failed to unlock account");
                ret = false;
            }
            if (needsWriteToDisk)
            {
                CWalletDB db(*dbw);
                if (!db.WriteAccount(getUUIDAsString(accountUUID), forAccount))
                {
                    throw std::runtime_error("Writing account failed");
                }
            }
        }
        for (const auto& [seedUUID, forSeed] : mapSeeds)
        {
            (unused) seedUUID;
            if (!forSeed->Unlock(vMasterKeyIn))
            {
                LogPrintf("CWallet::UnlockWithMasterKey - Failed to unlock seed");
                ret = false;
            }
        }

        // Only notify changed locking state if unlocking succeeded
        if (ret)
            NotifyLockingChanged(false);

        return ret;
    }


    virtual bool IsCrypted() const
    {
        for (const auto& [accountUUID, forAccount] : mapAccounts)
        {
            (unused) accountUUID;
            if(forAccount->IsCrypted())
                return true;
        }
        for (const auto& [seedUUID, forSeed] : mapSeeds)
        {
            (unused) seedUUID;
            if (forSeed->IsCrypted())
                return true;
        }
        return false;
    }


    virtual bool IsLocked() const
    {
        for (const auto& [accountUUID, forAccount] : mapAccounts)
        {
            (unused) accountUUID;
            if(forAccount->IsLocked())
                return true;
        }
        for (const auto& [seedUUID, forSeed] : mapSeeds)
        {
            (unused) seedUUID;
            if (forSeed->IsLocked())
                return true;
        }
        return false;
    }

    virtual bool GetKey(const CKeyID &address, CKey& keyOut) const
    {
        LOCK(cs_wallet);
        for (const auto& [accountUUID, forAccount] : mapAccounts)
        {
            (unused) accountUUID;
            if (forAccount->GetKey(address, keyOut))
                return true;
        }
        return false;
    }


    virtual bool GetPubKey(const CKeyID &address, CPubKey& vchPubKeyOut)
    {
        LOCK(cs_wallet);
        for (const auto& [accountUUID, forAccount] : mapAccounts)
        {
            (unused) accountUUID;
            if (forAccount->GetPubKey(address, vchPubKeyOut))
                return true;
        }
        return false;
    }


    virtual bool HaveWatchOnly(const CScript &dest) const
    {
        LOCK(cs_wallet);
        for (const auto& [accountUUID, forAccount] : mapAccounts)
        {
            (unused) accountUUID;
            if (forAccount->HaveWatchOnly(dest))
                return true;
        }
        return false;
    }

    virtual bool HaveWatchOnly() const
    {
        LOCK(cs_wallet);
        for (const auto& [accountUUID, forAccount] : mapAccounts)
        {
            (unused) accountUUID;
            if (forAccount->HaveWatchOnly())
                return true;
        }
        return false;
    }

    virtual bool HaveCScript(const CScriptID &hash)
    {
        LOCK(cs_wallet);
        for (const auto& [accountUUID, forAccount] : mapAccounts)
        {
            (unused) accountUUID;
            if (forAccount->HaveCScript(hash))
                return true;
        }
        return false;
    }

    virtual bool GetCScript(const CScriptID &hash, CScript& redeemScriptOut)
    {
        LOCK(cs_wallet);
        for (const auto& [accountUUID, forAccount] : mapAccounts)
        {
            (unused) accountUUID;
            if (forAccount->GetCScript(hash, redeemScriptOut))
                return true;
        }
        return false;
    }


    virtual bool HaveKey(const CKeyID &address) const
    {
        LOCK(cs_wallet);
        for (const auto& [accountUUID, forAccount] : mapAccounts)
        {
            (unused) accountUUID;
            if (forAccount->HaveKey(address))
                return true;
        }
        return false;
    }

    virtual bool HaveWatchOnly(const CScript &dest)
    {
        LOCK(cs_wallet);
        for (const auto& [accountUUID, forAccount] : mapAccounts)
        {
            (unused) accountUUID;
            if (forAccount->HaveWatchOnly(dest))
                return true;
        }
        return false;
    }
    virtual bool AddHDKeyPubKey(int64_t HDKeyIndex, const CPubKey &pubkey, CAccount& forAccount, int keyChain);
    virtual bool LoadHDKey(int64_t HDKeyIndex, int64_t keyChain, const CPubKey &pubkey, const std::string& forAccount);

    virtual void MarkKeyUsed(CKeyID keyID, uint64_t usageTime);

    isminetype IsMine(const CKeyStore &keystore, const CTxIn& txin) const;
    virtual void RemoveAddressFromKeypoolIfIsMine(const CTxIn& txin, uint64_t time);
    virtual void RemoveAddressFromKeypoolIfIsMine(const CTxOut& txout, uint64_t time);
    virtual void RemoveAddressFromKeypoolIfIsMine(const CTransaction& tx, uint64_t time);

    void changeAccountName(CAccount* account, const std::string& newName, bool notify=true);
    void addAccount(CAccount* account, const std::string& newName, bool bMakeActive=true);
    void deleteAccount(CWalletDB& walletDB, CAccount* account, bool shouldPurge=false);

    CAccountHD* GenerateNewAccount(std::string strAccount, AccountState state, AccountType subType, bool bMakeActive=true);

    //! Create a new legacy account. Legacy accounts are accounts that generate random keys in a keypool as required instead of generating keys deterministically.
    virtual CAccount* GenerateNewLegacyAccount(std::string strAccount);

    //! Parse the contents of a gulden "witness key" URL into an vector of  private key / birth date  pairs.
    std::vector<std::pair<CKey, uint64_t>> ParseWitnessKeyURL(SecureString sEncodedPrivWitnessKeysURL);

    //! Import vector of  private key / birth date  pairs into an existing "witness only" account.
    CAccount* CreateWitnessOnlyWitnessAccount(std::string strAccount, std::vector<std::pair<CKey, uint64_t>> privateWitnessKeysWithBirthDates, bool allowRescan=true);

    //! Create a new "witness only" account and import vector of  private key / birth date  pairs into it.
    bool ImportKeysIntoWitnessOnlyWitnessAccount(CAccount* forAccount, std::vector<std::pair<CKey, uint64_t>> privateWitnessKeysWithBirthDates, bool allowRescan=true);

    //! Create a read-only HD account using an encoded public key.
    CAccountHD* CreateReadOnlyAccount(std::string strAccount, SecureString encExtPubKey);

    //! Create an HD account directly from a key and not assosciated with any seed.
    CAccountHD* CreateSeedlessHDAccount(std::string strAccount, CEncodedSecretKeyExt<CExtKey> accountExtKey, AccountState state, AccountType type, bool generateKeys=true);

    void setActiveAccount(CWalletDB& walletdb, CAccount* newActiveAccount);

    //! Find the first account that is not deleted/shadow etc. and set it as active
    void setAnyActiveAccount(CWalletDB& walletdb);

    CAccount* getActiveAccount();
    void setActiveSeed(CWalletDB& walletdb, CHDSeed* newActiveSeed);
    CHDSeed* GenerateHDSeed(CHDSeed::SeedType);
    void DeleteSeed(CWalletDB& walletDB, CHDSeed* deleteSeed, bool shouldPurgeAccounts);
    CHDSeed* ImportHDSeed(SecureString mnemonic, CHDSeed::SeedType type);
    CHDSeed* ImportHDSeedFromPubkey(SecureString pubKeyString);
    CHDSeed* getActiveSeed();

    //! for wallet upgrade
    void ForceRewriteKeys(CAccount& forAccount);

    // for transfer of ownership of unlock sessions to the shadowpool thread, protected by cs_wallet
    unsigned int nUnlockedSessionsOwnedByShadow;

    std::map<boost::uuids::uuid, CHDSeed*> mapSeeds;
    std::map<boost::uuids::uuid, CAccount*> mapAccounts;
    std::map<boost::uuids::uuid, std::string> mapAccountLabels;
    std::map<uint256, CWalletTx> mapWallet;

    CAccount* activeAccount;
    CHDSeed* activeSeed;

    // Account changed (name change)
    boost::signals2::signal<void (CWallet* wallet, CAccount* account)> NotifyAccountNameChanged;

    // Account warning state changed (witness account expired)
    boost::signals2::signal<void (CWallet* wallet, CAccount* account)> NotifyAccountWarningChanged;

    // New account added to the wallet
    boost::signals2::signal<void (CWallet* wallet, CAccount* account)> NotifyAccountAdded;

    // Account marked as deleted.
    boost::signals2::signal<void (CWallet* wallet, CAccount* account)> NotifyAccountDeleted;
    
    // Account properties modified in some way (excludes deletion and name changes)
    boost::signals2::signal<void (CWallet* wallet, CAccount* account)> NotifyAccountModified;

    // Currently active account changed
    boost::signals2::signal<void (CWallet* wallet, CAccount* account)> NotifyActiveAccountChanged;

    // Account compound setting changed
    boost::signals2::signal<void (CWallet* wallet, CAccount* account)> NotifyAccountCompoundingChanged;
    
    // Block generation turned on
    boost::signals2::signal<void (void)> NotifyGenerationStarted;
    
    // Block generation turned off
    boost::signals2::signal<void (void)> NotifyGenerationStopped;
    
    // New generation statistics
    boost::signals2::signal<void (void)> NotifyGenerationStatisticsUpdate;

    /** Locking state changed */
    boost::signals2::signal<void (bool isLocked)> NotifyLockingChanged;

protected:
    // number of active unlocked sessions protected by cs_wallet
    unsigned int nUnlockSessions;

    // automatically lock when nUnlockSessions becomes 0
    mutable bool fAutoLock;
};
#endif
