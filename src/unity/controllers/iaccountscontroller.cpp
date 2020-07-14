// Copyright (c) 2020 The Novo developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the NOVO software license, see the accompanying
// file COPYING

//Workaround braindamaged 'hack' in libtool.m4 that defines DLL_EXPORT when building a dll via libtool (this in turn imports unwanted symbols from e.g. pthread that breaks static pthread linkage)
#ifdef DLL_EXPORT
#undef DLL_EXPORT
#endif

#include "net.h"
#include "net_processing.h"
#include "validation/validation.h"
#include "ui_interface.h"

#include <wallet/wallet.h>
#include <wallet/account.h>

// Unity specific includes
#include "../unity_impl.h"
#include "i_accounts_controller.hpp"
#include "i_accounts_listener.hpp"
#include "account_record.hpp"

#include <boost/asio.hpp>
#include <boost/date_time/posix_time/posix_time_duration.hpp>

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

//fixme: (DEDUP) - try share common code with RPC listallaccounts function
std::vector<AccountRecord> IAccountsController::listAccounts()
{
    std::vector<AccountRecord> ret;

    if (!pactiveWallet)
        return ret;
    
    DS_LOCK2(cs_main, pactiveWallet->cs_wallet);
    for (const auto& accountPair : pactiveWallet->mapAccounts)
    {
        AccountRecord rec("", "", "", "", false);
        rec.UUID = getUUIDAsString(accountPair.first);
        rec.label = accountPair.second->getLabel();
        rec.state = GetAccountStateString(accountPair.second->m_State);
        rec.type = GetAccountTypeString(accountPair.second->m_Type);
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
