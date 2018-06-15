// Copyright (c) 2016-2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#ifndef GULDEN_WALLET_GULDEN_H
#define GULDEN_WALLET_GULDEN_H

#include "amount.h"
#include "streams.h"
#include "tinyformat.h"
#include "ui_interface.h"
#include "utilstrencodings.h"
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

//fixme: (Post-2.1) Make configurable option
extern bool fShowChildAccountsSeperately;

std::string accountNameForAddress(const CWallet &wallet, const CTxDestination& dest);
isminetype IsMine(const CWallet &wallet, const CTxDestination& dest);
isminetype IsMine(const CWallet &wallet, const CTxOut& out);

/** 
 * A CGuldenWallet maintains a set of transactions and balances
 * and provides the ability to create new transactions.
 * it contains one or more accounts, which are responsible for creating/allocating/managing keys via their keystore interfaces.
 */
class CGuldenWallet : public CValidationInterface
{
public:
    CGuldenWallet() : dbw(new CWalletDBWrapper()){};
    CGuldenWallet(std::unique_ptr<CWalletDBWrapper> dbw_in) : dbw(std::move(dbw_in)){};
    virtual ~CGuldenWallet(){};

    //Members that are shared with CWallet.
    mutable CCriticalSection cs_wallet;
    uint64_t nTimeFirstKey = 0;
    //const std::string strWalletFile;
    std::unique_ptr<CWalletDBWrapper> dbw;


    virtual bool Lock() const
    {
        LOCK(cs_wallet);
        if (delayLock || wantDelayLock)
        {
            didDelayLock = true;
            return true;
        }

        bool ret = true;
        for (auto accountPair : mapAccounts)
        {
            if (!accountPair.second->Lock())
                ret = false;
        }
        for (auto seedPair : mapSeeds)
        {
            if (!seedPair.second->Lock())
                ret = false;
        }
        return ret;
    }

    virtual bool Unlock(const CKeyingMaterial& vMasterKeyIn) const
    {
        LOCK(cs_wallet);
        bool ret = true;
        for (auto accountPair : mapAccounts)
        {
            bool needsWriteToDisk = false;
            if (!accountPair.second->Unlock(vMasterKeyIn, needsWriteToDisk))
                ret = false;
            if (needsWriteToDisk)
            {
                CWalletDB db(*dbw);
                if (!db.WriteAccount(getUUIDAsString(accountPair.second->getUUID()), accountPair.second))
                {
                    throw std::runtime_error("Writing account failed");
                }
            }
        }
        for (auto seedPair : mapSeeds)
        {
            if (!seedPair.second->Unlock(vMasterKeyIn))
                ret = false;
        }
        return ret;
    }


    virtual bool IsCrypted() const
    {
        for (auto accountPair : mapAccounts)
        {
            if(accountPair.second->IsCrypted())
                return true;
        }
        for (auto seedPair : mapSeeds)
        {
            if (seedPair.second->IsCrypted())
                return true;
        }
        return false;
    }


    virtual bool IsLocked() const
    {
        for (auto accountPair : mapAccounts)
        {
            if(accountPair.second->IsLocked())
                return true;
        }
        for (auto seedPair : mapSeeds)
        {
            if (seedPair.second->IsLocked())
                return true;
        }
        return false;
    }

    virtual bool GetKey(const CKeyID &address, CKey& keyOut) const
    {
        LOCK(cs_wallet);
        for (auto accountPair : mapAccounts)
        {
            if (accountPair.second->GetKey(address, keyOut))
                return true;
        }
        return false;
    }


    virtual bool GetPubKey(const CKeyID &address, CPubKey& vchPubKeyOut)
    {
        LOCK(cs_wallet);
        for (auto accountPair : mapAccounts)
        {
            if (accountPair.second->GetPubKey(address, vchPubKeyOut))
                return true;
        }
        return false;
    }


    virtual bool HaveWatchOnly(const CScript &dest) const
    {
        LOCK(cs_wallet);
        for (auto accountPair : mapAccounts)
        {
            if (accountPair.second->HaveWatchOnly(dest))
                return true;
        }
        return false;
    }

    virtual bool HaveWatchOnly() const
    {
        LOCK(cs_wallet);
        for (auto accountPair : mapAccounts)
        {
            if (accountPair.second->HaveWatchOnly())
                return true;
        }
        return false;
    }

    virtual bool HaveCScript(const CScriptID &hash)
    {
        LOCK(cs_wallet);
        for (auto accountPair : mapAccounts)
        {
            if (accountPair.second->HaveCScript(hash))
                return true;
        }
        return false;
    }

    virtual bool GetCScript(const CScriptID &hash, CScript& redeemScriptOut)
    {
        LOCK(cs_wallet);
        for (auto accountPair : mapAccounts)
        {
            if (accountPair.second->GetCScript(hash, redeemScriptOut))
                return true;
        }
        return false;
    }


    virtual bool HaveKey(const CKeyID &address) const
    {
        LOCK(cs_wallet);
        for (auto accountPair : mapAccounts)
        {
            if (accountPair.second->HaveKey(address))
                return true;
        }
        return false;
    }

    virtual bool HaveWatchOnly(const CScript &dest)
    {
        LOCK(cs_wallet);
        for (auto accountPair : mapAccounts)
        {
            if (accountPair.second->HaveWatchOnly(dest))
            return true;
        }
        return false;
    }
    virtual bool AddKeyPubKey(int64_t HDKeyIndex, const CPubKey &pubkey, CAccount& forAccount, int keyChain);
    virtual bool LoadKey(int64_t HDKeyIndex, int64_t keyChain, const CPubKey &pubkey, const std::string& forAccount);

    virtual void MarkKeyUsed(CKeyID keyID, uint64_t usageTime);

    isminetype IsMine(const CKeyStore &keystore, const CTxIn& txin) const;
    virtual void RemoveAddressFromKeypoolIfIsMine(const CTxIn& txin, uint64_t time);
    virtual void RemoveAddressFromKeypoolIfIsMine(const CTxOut& txout, uint64_t time);
    virtual void RemoveAddressFromKeypoolIfIsMine(const CTransaction& tx, uint64_t time);

    virtual void changeAccountName(CAccount* account, const std::string& newName, bool notify=true);
    virtual void addAccount(CAccount* account, const std::string& newName, bool bMakeActive=true);
    virtual void deleteAccount(CAccount* account);

    virtual CAccountHD* GenerateNewAccount(std::string strAccount, AccountState state, AccountType subType, bool bMakeActive=true);
    virtual CAccount* GenerateNewLegacyAccount(std::string strAccount);
    virtual CAccount* CreateWitnessOnlyWitnessAccount(std::string strAccount, SecureString encPrivWitnessKey);
    virtual CAccountHD* CreateReadOnlyAccount(std::string strAccount, SecureString encExtPubKey);

    virtual void setActiveAccount(CAccount* newActiveAccount);
    virtual CAccount* getActiveAccount();
    virtual void setActiveSeed(CHDSeed* newActiveSeed);
    virtual CHDSeed* GenerateHDSeed(CHDSeed::SeedType);
    virtual void DeleteSeed(CHDSeed* deleteSeed, bool purge);
    virtual CHDSeed* ImportHDSeed(SecureString mnemonic, CHDSeed::SeedType type);
    virtual CHDSeed* ImportHDSeedFromPubkey(SecureString pubKeyString);
    virtual CHDSeed* getActiveSeed();

    //! for wallet upgrade
    virtual void ForceRewriteKeys(CAccount& forAccount);

    // The 'shadow pool thread' sets delay lock true if it had a backlog of work it wants to do on the unlocked wallet
    bool delayLock;
    bool wantDelayLock;
    mutable bool didDelayLock;


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

    // Currently active account changed
    boost::signals2::signal<void (CWallet* wallet, CAccount* account)> NotifyActiveAccountChanged;
};
#endif // GULDEN_WALLET_GULDEN_H
