// Copyright (c) 2020-2022 The Centure developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the Libre Chain License, see the accompanying
// file COPYING

//Workaround braindamaged 'hack' in libtool.m4 that defines DLL_EXPORT when building a dll via libtool (this in turn imports unwanted symbols from e.g. pthread that breaks static pthread linkage)
#ifdef DLL_EXPORT
#undef DLL_EXPORT
#endif

#include "appname.h"
#include "net.h"
#include "net_processing.h"
#include "validation/validation.h"
#include "ui_interface.h"
#include "util/strencodings.h"


#include <wallet/wallet.h>
#include <wallet/account.h>
#include <wallet/witness_operations.h>

// Unity specific includes
#include "../unity_impl.h"
#include "i_accounts_controller.hpp"
#include "i_accounts_listener.hpp"
#include "account_record.hpp"
#include "account_link_record.hpp"

std::shared_ptr<IAccountsListener> accountsListener;
std::list<boost::signals2::connection> coreSignalConnections;

void IAccountsController::setListener(const std::shared_ptr<IAccountsListener>& accountsListener_)
{
    accountsListener = accountsListener_;
        
    // Disconnect all already connected signals
    for (auto& connection: coreSignalConnections)
    {
        connection.disconnect();
    }
    coreSignalConnections.clear();
        
    if (pactiveWallet && accountsListener)
    {
        coreSignalConnections.push_back(pactiveWallet->NotifyActiveAccountChanged.connect([](CWallet* pWallet, CAccount* pAccount)
        {
            accountsListener->onActiveAccountChanged(getUUIDAsString(pAccount->getUUID()));
        }));
        coreSignalConnections.push_back(pactiveWallet->NotifyAccountNameChanged.connect([](CWallet* pWallet, CAccount* pAccount)
        {
            accountsListener->onAccountNameChanged(getUUIDAsString(pAccount->getUUID()), pAccount->getLabel());
            if (pAccount == pactiveWallet->activeAccount)
            {
                accountsListener->onActiveAccountNameChanged(pAccount->getLabel());
            }
        }));
        coreSignalConnections.push_back(pactiveWallet->NotifyAccountModified.connect([](CWallet* pWallet, CAccount* pAccount)
        {
            accountsListener->onAccountModified(getUUIDAsString(pAccount->getUUID()), GetAccountRecord(pAccount->getUUID(), pAccount));
        }));
        coreSignalConnections.push_back(pactiveWallet->NotifyAccountAdded.connect([](CWallet* pWallet, CAccount* pAccount)
        {
            accountsListener->onAccountAdded(getUUIDAsString(pAccount->getUUID()), pAccount->getLabel());
        }));
        coreSignalConnections.push_back(pactiveWallet->NotifyAccountDeleted.connect([](CWallet* pWallet, CAccount* pAccount)
        {
            accountsListener->onAccountDeleted(getUUIDAsString(pAccount->getUUID()));
        }));
        //NotifyAccountWarningChanged
        //NotifyAccountCompoundingChanged
    }
}

bool IAccountsController::setActiveAccount(const std::string & accountUUID)
{
    if (!pactiveWallet)
        return false;
    
    DS_LOCK2(cs_main, pactiveWallet->cs_wallet);
    CAccount* forAccount = NULL;
    auto findIter = pactiveWallet->mapAccounts.find(getUUIDFromString(accountUUID));
    if (findIter != pactiveWallet->mapAccounts.end())
    {
        forAccount = findIter->second;
        CWalletDB walletdb(*pactiveWallet->dbw);
        pactiveWallet->setActiveAccount(walletdb, forAccount);
        return true;
    }
    return false;
}

std::string IAccountsController::getActiveAccount()
{
    if (pactiveWallet)
    {        
        CAccount* activeAccount = pactiveWallet->getActiveAccount();
        if (activeAccount)
        {
            return getUUIDAsString(activeAccount->getUUID());
        }
    }
    return "";
}

//fixme: Move this out into a common helper and add an RPC equivalent
std::string IAccountsController::getAccountLinkURI(const std::string & accountUUID)
{
    if (!pactiveWallet || dynamic_cast<CExtWallet*>(pactiveWallet)->IsLocked())
        return "";
    
    DS_LOCK2(cs_main, pactiveWallet->cs_wallet);
    CAccount* forAccount = NULL;
    auto findIter = pactiveWallet->mapAccounts.find(getUUIDFromString(accountUUID));
    if (findIter != pactiveWallet->mapAccounts.end())
    {
        forAccount = findIter->second;
        
        int64_t currentTime = forAccount->getEarliestPossibleCreationTime();
        std::string payoutAddress;
        std::string qrString = "";
        if (forAccount->IsHD() && !forAccount->IsPoW2Witness())
        {
            CReserveKeyOrScript reservekey(pactiveWallet, forAccount, KEYCHAIN_CHANGE);
            CPubKey vchPubKey;
            if (!reservekey.GetReservedKey(vchPubKey))
                return "";
            payoutAddress = CNativeAddress(vchPubKey.GetID()).ToString();

            qrString = GLOBAL_APPNAME"sync:" + CEncodedSecretKeyExt<CExtKey>(*(static_cast<CAccountHD*>(forAccount)->GetAccountMasterPrivKey())).SetCreationTime(i64tostr(currentTime)).SetPayAccount(payoutAddress).ToURIString();
            
            return qrString;
        }
        
        return "";
    }
    return "";
}

std::string IAccountsController::getWitnessKeyURI(const std::string & accountUUID)
{
    if (!pactiveWallet || dynamic_cast<CExtWallet*>(pactiveWallet)->IsLocked())
        return "";
    
    DS_LOCK2(cs_main, pactiveWallet->cs_wallet);
    CAccount* forAccount = NULL;
    auto findIter = pactiveWallet->mapAccounts.find(getUUIDFromString(accountUUID));
    if (findIter != pactiveWallet->mapAccounts.end())
    {
        forAccount = findIter->second;
        if (forAccount->IsPoW2Witness())
        {
            return witnessKeysLinkUrlForAccount(pactiveWallet, forAccount);
        }
    }
    return "";
}

std::string IAccountsController::createAccountFromWitnessKeyURI(const std::string& witnessKeyURI, const std::string& newAccountName)
{
    if (!pactiveWallet || dynamic_cast<CExtWallet*>(pactiveWallet)->IsLocked())
        return "";

    DS_LOCK2(cs_main, pactiveWallet->cs_wallet);
    for (const auto& [accountUUID, account] : pactiveWallet->mapAccounts)
    {
        (unused)accountUUID;
        if (account->getLabel() == newAccountName)
        {
            return "";
        }
    }
        
    bool shouldRescan = true;
    const auto& keysAndBirthDates = pactiveWallet->ParseWitnessKeyURL(SecureString(witnessKeyURI.begin(), witnessKeyURI.end()));
    if (keysAndBirthDates.empty())
        throw std::runtime_error("Invalid encoded key URL");

    CAccount* account = nullptr;
    //NB! CreateWitnessOnlyWitnessAccount triggers a rescan for us
    account = pactiveWallet->CreateWitnessOnlyWitnessAccount(newAccountName, keysAndBirthDates, shouldRescan);
    if (!account)
        return "";

    return getUUIDAsString(account->getUUID());
}

bool IAccountsController::deleteAccount(const std::string & accountUUID)
{
    if (!pactiveWallet)
        return false;
    
    DS_LOCK2(cs_main, pactiveWallet->cs_wallet);
    CAccount* forAccount = NULL;
    auto findIter = pactiveWallet->mapAccounts.find(getUUIDFromString(accountUUID));
    if (findIter != pactiveWallet->mapAccounts.end())
    {
        forAccount = findIter->second;
        CWalletDB walletdb(*pactiveWallet->dbw);
        pactiveWallet->deleteAccount(walletdb, forAccount, false);
        return true;
    }
    return false;
}

bool IAccountsController::purgeAccount(const std::string & accountUUID)
{
    if (!pactiveWallet)
        return false;
    
    DS_LOCK2(cs_main, pactiveWallet->cs_wallet);
    CAccount* forAccount = NULL;
    auto findIter = pactiveWallet->mapAccounts.find(getUUIDFromString(accountUUID));
    if (findIter != pactiveWallet->mapAccounts.end())
    {
        forAccount = findIter->second;
        CWalletDB walletdb(*pactiveWallet->dbw);
        pactiveWallet->deleteAccount(walletdb, forAccount, true);
        return true;
    }
    return false;
}

std::string IAccountsController::createAccount(const std::string& accountName, const std::string& accountType)
{
    if (!pactiveWallet)
        return "";

    DS_LOCK2(cs_main, pactiveWallet->cs_wallet);
    CAccount* pNewAccount = CreateAccountHelper(pactiveWallet, accountName, accountType, false);
    if (pNewAccount)
        return getUUIDAsString(pNewAccount->getUUID());
    
    return "";
}

std::string IAccountsController::getAccountName(const std::string& accountUUID)
{
    if (!pactiveWallet)
        return "";
    
    DS_LOCK2(cs_main, pactiveWallet->cs_wallet);
    CAccount* forAccount = NULL;
    auto findIter = pactiveWallet->mapAccounts.find(getUUIDFromString(accountUUID));
    if (findIter != pactiveWallet->mapAccounts.end())
    {
        forAccount = findIter->second;
        return forAccount->getLabel();
    }
    return "";
}

bool IAccountsController::renameAccount(const std::string& accountUUID, const std::string& newAccountName)
{
    if (!pactiveWallet)
        return false;
    
    DS_LOCK2(cs_main, pactiveWallet->cs_wallet);
    CAccount* forAccount = NULL;
    auto findIter = pactiveWallet->mapAccounts.find(getUUIDFromString(accountUUID));
    if (findIter != pactiveWallet->mapAccounts.end())
    {
        forAccount = findIter->second;
        pactiveWallet->changeAccountName(forAccount, newAccountName);
        return true;
    }
        
    return false;
}

std::vector<AccountLinkRecord> listAccountLinksHelper(CAccount* forAccount)
{
    std::vector<AccountLinkRecord> result;
    if (forAccount)
    {
        const auto& accountLinks = forAccount->getLinks();
        for (const auto& [serviceName, serviceData] : accountLinks)
        {
            result.push_back(AccountLinkRecord(serviceName, serviceData));
        }
    }
    return result;
}

//fixme: (DEDUP) - try share common code with RPC listallaccounts function
std::vector<AccountRecord> IAccountsController::listAccounts()
{
    std::vector<AccountRecord> ret;

    if (!pactiveWallet)
        return ret;
    
    DS_LOCK2(cs_main, pactiveWallet->cs_wallet);
    for (const auto& accountPair : pactiveWallet->mapAccounts)
    {
        AccountRecord rec("", "", "", "", false, std::vector<AccountLinkRecord>());
        rec.UUID = getUUIDAsString(accountPair.first);
        rec.label = accountPair.second->getLabel();
        rec.state = GetAccountStateString(accountPair.second->m_State);
        rec.type = GetAccountTypeString(accountPair.second->m_Type);
        rec.accountLinks = listAccountLinksHelper(accountPair.second);
        if (!accountPair.second->IsHD())
        {
            rec.isHD = false;
        }
        else
        {
            rec.isHD = true;
        }
        ret.emplace_back(rec);
    }
    return ret;
}
  
BalanceRecord IAccountsController::getActiveAccountBalance()
{
    if (!pactiveWallet)
        return BalanceRecord(0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

    WalletBalances balances;
    pactiveWallet->GetBalances(balances, pactiveWallet->activeAccount, true);
    return BalanceRecord(balances.availableIncludingLocked, balances.availableExcludingLocked, balances.availableLocked, balances.unconfirmedIncludingLocked, balances.unconfirmedExcludingLocked, balances.unconfirmedLocked, balances.immatureIncludingLocked, balances.immatureExcludingLocked, balances.immatureLocked, balances.totalLocked);
}

BalanceRecord IAccountsController::getAccountBalance(const std::string& accountUUID)
{
    if (pactiveWallet)
    {
        DS_LOCK2(cs_main, pactiveWallet->cs_wallet);
        auto findIter = pactiveWallet->mapAccounts.find(getUUIDFromString(accountUUID));
        if (findIter != pactiveWallet->mapAccounts.end())
        {
            WalletBalances balances;
            pactiveWallet->GetBalances(balances, findIter->second, true);
            return BalanceRecord(balances.availableIncludingLocked, balances.availableExcludingLocked, balances.availableLocked, balances.unconfirmedIncludingLocked, balances.unconfirmedExcludingLocked, balances.unconfirmedLocked, balances.immatureIncludingLocked, balances.immatureExcludingLocked, balances.immatureLocked, balances.totalLocked);
        }
    }
    return BalanceRecord(0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
}

std::unordered_map<std::string, BalanceRecord> IAccountsController::getAllAccountBalances()
{
    std::unordered_map<std::string, BalanceRecord> ret;
    
    if (!pactiveWallet)
        return ret;
    
    DS_LOCK2(cs_main, pactiveWallet->cs_wallet);
    for (const auto& [accountUUID, account] : pactiveWallet->mapAccounts)
    {
        if (account->m_State == AccountState::Normal)
        {
            WalletBalances balances;
            pactiveWallet->GetBalances(balances, account, true);
            ret.emplace(getUUIDAsString(accountUUID), BalanceRecord(balances.availableIncludingLocked, balances.availableExcludingLocked, balances.availableLocked, balances.unconfirmedIncludingLocked, balances.unconfirmedExcludingLocked, balances.unconfirmedLocked, balances.immatureIncludingLocked, balances.immatureExcludingLocked, balances.immatureLocked, balances.totalLocked));
        }
    }    
    return ret;
}

std::vector<TransactionRecord> IAccountsController::getTransactionHistory(const std::string& accountUUID)
{
    if (pactiveWallet)
    {
        DS_LOCK2(cs_main, pactiveWallet->cs_wallet);
        auto findIter = pactiveWallet->mapAccounts.find(getUUIDFromString(accountUUID));
        if (findIter != pactiveWallet->mapAccounts.end())
        {
            return getTransactionHistoryForAccount(findIter->second);
        }
    }
    return std::vector<TransactionRecord>();
}

std::vector<MutationRecord> IAccountsController::getMutationHistory(const std::string& accountUUID)
{
    if (pactiveWallet)
    {
        DS_LOCK2(cs_main, pactiveWallet->cs_wallet);
        auto findIter = pactiveWallet->mapAccounts.find(getUUIDFromString(accountUUID));
        if (findIter != pactiveWallet->mapAccounts.end())
        {
            return getMutationHistoryForAccount(findIter->second);
        }
    }
    return std::vector<MutationRecord>();
}

std::string IAccountsController::getReceiveAddress(const std::string & accountUUID)
{
    if (pactiveWallet)
    {
        LOCK2(cs_main, pactiveWallet->cs_wallet);
        
        auto findIter = pactiveWallet->mapAccounts.find(getUUIDFromString(accountUUID));
        if (findIter != pactiveWallet->mapAccounts.end())
        {
            CReserveKeyOrScript* receiveAddress = new CReserveKeyOrScript(pactiveWallet, findIter->second, KEYCHAIN_EXTERNAL);
            CPubKey pubKey;
            if (receiveAddress->GetReservedKey(pubKey))
            {
                CKeyID keyID = pubKey.GetID();
                receiveAddress->ReturnKey();
                delete receiveAddress;
                return CNativeAddress(keyID).ToString();
            }
        }
    }
    return "";
}

bool IAccountsController::addAccountLink(const std::string& accountUUID, const std::string& serviceName, const std::string& serviceData)
{
    if (serviceName.empty())
    {
        return false;
    }
    if (pactiveWallet)
    {
        LOCK2(cs_main, pactiveWallet->cs_wallet);
        
        auto findIter = pactiveWallet->mapAccounts.find(getUUIDFromString(accountUUID));
        if (findIter != pactiveWallet->mapAccounts.end())
        {
            CAccount* forAccount = findIter->second;
            CWalletDB walletdb(*pactiveWallet->dbw);
            forAccount->addLink(serviceName, serviceData, &walletdb);
            return true;
        }
    }
    return false;
}

bool IAccountsController::removeAccountLink(const std::string& accountUUID, const std::string& serviceName)
{
    if (serviceName.empty())
    {
        return false;
    }
    if (pactiveWallet)
    {
        LOCK2(cs_main, pactiveWallet->cs_wallet);
        
        auto findIter = pactiveWallet->mapAccounts.find(getUUIDFromString(accountUUID));
        if (findIter != pactiveWallet->mapAccounts.end())
        {
            CAccount* forAccount = findIter->second;
            CWalletDB walletdb(*pactiveWallet->dbw);
            forAccount->removeLink(serviceName, &walletdb);
            return true;
        }
    }
    return false;
}

std::vector<AccountLinkRecord> IAccountsController::listAccountLinks(const std::string& accountUUID)
{
    if (pactiveWallet)
    {
        LOCK2(cs_main, pactiveWallet->cs_wallet);
        
        auto findIter = pactiveWallet->mapAccounts.find(getUUIDFromString(accountUUID));
        if (findIter != pactiveWallet->mapAccounts.end())
        {
            CAccount* forAccount = findIter->second;
            return listAccountLinksHelper(forAccount);
        }
    }
    return std::vector<AccountLinkRecord>();
}
