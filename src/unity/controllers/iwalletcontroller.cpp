// Copyright (c) 2020 The Novo developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the NOVO software license, see the accompanying
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
#include "i_wallet_controller.hpp"
#include "i_wallet_listener.hpp"
#include "account_record.hpp"
#include "balance_record.hpp"
#include "mutation_record.hpp"

std::shared_ptr<IWalletListener> walletListener;

// rate limited mutations notifier
static CRateLimit<std::pair<uint256, bool>>* walletNewMutationsNotifier=nullptr;
// rate limited balance change notifier
static CRateLimit<int>* walletBalanceChangeNotifier=nullptr;

extern bool syncDoneFired;

#ifdef ENABLE_WALLET
void NotifyRequestUnlockS(CWallet* wallet, std::string reason)
{
    if (walletListener)
    {
        walletListener->notifyCoreWantsUnlock(reason);
    }
}

void NotifyRequestUnlockWithCallbackS(CWallet* wallet, std::string reason, std::function<void (void)> successCallback)
{
    if (walletListener)
    {
        walletListener->notifyCoreWantsUnlock(reason);
    }
    //fixme: (UNITY) (HIGH)
    //Need to be able to get a response here and deal with the callback, or else fix the code that relies on this
    //successCallback();
}
#endif

bool unityMessageBox(const std::string& message, const std::string& caption, unsigned int style)
{
    bool fSecure = style & CClientUIInterface::SECURE;
    style &= ~CClientUIInterface::SECURE;
    
    std::string strType;
    switch (style)
    {
        case CClientUIInterface::MSG_ERROR:
            strType = "MSG_ERROR";
            break;
        case CClientUIInterface::MSG_WARNING:
            strType = "MSG_WARNING";
            break;
        case CClientUIInterface::MSG_INFORMATION:
            strType = "MSG_INFORMATION";
            break;
        default:
            break;
    }

    if (!fSecure)
        LogPrintf("%s: %s\n", strType, caption, message);
    
    if (walletListener)
    {
        walletListener->notifyCoreInfo(strType, caption, message);
    }
    
    return false;
}

void IWalletController::setListener(const std::shared_ptr<IWalletListener>& walletListener_)
{
    walletListener = walletListener_;

    if (pactiveWallet && walletListener)
    {
        walletBalanceChangeNotifier = new CRateLimit<int>([](int)
        {
            if (pactiveWallet && walletListener)
            {
                WalletBalances balances;
                pactiveWallet->GetBalances(balances, nullptr, true);
                walletListener->notifyBalanceChange(BalanceRecord(balances.availableIncludingLocked, balances.availableExcludingLocked, balances.availableLocked, balances.unconfirmedIncludingLocked, balances.unconfirmedExcludingLocked, balances.unconfirmedLocked, balances.immatureIncludingLocked, balances.immatureExcludingLocked, balances.immatureLocked, balances.totalLocked));
            }
        }, std::chrono::milliseconds(BALANCE_NOTIFY_THRESHOLD_MS));

        walletNewMutationsNotifier = new CRateLimit<std::pair<uint256, bool>>([](const std::pair<uint256, bool>& txInfo)
        {
            if (pactiveWallet && walletListener && syncDoneFired)
            {
                const uint256& txHash = txInfo.first;
                const bool fSelfComitted = txInfo.second;

                LOCK2(cs_main, pactiveWallet->cs_wallet);
                if (pactiveWallet->mapWallet.find(txHash) != pactiveWallet->mapWallet.end())
                {
                    const CWalletTx& wtx = pactiveWallet->mapWallet[txHash];
                    std::vector<MutationRecord> mutations;
                    addMutationsForTransaction(&wtx, mutations, nullptr);
                    for (auto& m: mutations)
                    {
                        LogPrintf("unity: notify new wallet mutation for tx %s", txHash.ToString().c_str());
                        walletListener->notifyNewMutation(m, fSelfComitted);
                    }
                }
            }
        }, std::chrono::milliseconds(NEW_MUTATIONS_NOTIFY_THRESHOLD_MS));

        // Fire events for lock status changes
        pactiveWallet->NotifyLockingChanged.connect( [&](const bool isLocked)
        {
            if (isLocked)
            {
                walletListener->notifyWalletLocked();
            }
            else
            {
                walletListener->notifyWalletUnlocked();
            }
        } );

        // Fire events for transaction depth changes (up to depth 10 only)
        pactiveWallet->NotifyTransactionDepthChanged.connect( [&](CWallet* pwallet, const uint256& hash)
        {
            if (syncDoneFired)
            {
                DS_LOCK2(cs_main, pwallet->cs_wallet);
                if (pwallet->mapWallet.find(hash) != pwallet->mapWallet.end())
                {
                    const CWalletTx& wtx = pwallet->mapWallet[hash];
                    LogPrintf("unity: notify transaction depth changed %s",hash.ToString().c_str());
                    if (walletListener)
                    {
                        bool anyInputsOrOutputsAreMine = false;
                        std::vector<CAccount*> forAccounts;
                        for (const auto& [accountUUID, account] : pactiveWallet->mapAccounts)
                        {
                            forAccounts.push_back(account);
                        }
                        TransactionRecord walletTransaction = calculateTransactionRecordForWalletTransaction(wtx, forAccounts, anyInputsOrOutputsAreMine);
                        if (anyInputsOrOutputsAreMine)
                        {
                            walletListener->notifyUpdatedTransaction(walletTransaction);
                        }
                    }
                }
            }
        } );

        // Fire events for transaction status changes, or new transactions (this won't fire for simple depth changes)
        pactiveWallet->NotifyTransactionChanged.connect( [&](CWallet* pwallet, const uint256& hash, ChangeType status, bool fSelfComitted) {
            if (syncDoneFired)
            {
                DS_LOCK2(cs_main, pwallet->cs_wallet);
                if (pwallet->mapWallet.find(hash) != pwallet->mapWallet.end())
                {
                    if (status == CT_NEW) {
                        walletNewMutationsNotifier->trigger(std::make_pair(hash, fSelfComitted));
                    }
                    else if (status == CT_UPDATED && walletListener)
                    {
                        LogPrintf("unity: notify tx updated %s",hash.ToString().c_str());
                        const CWalletTx& wtx = pwallet->mapWallet[hash];
                        bool anyInputsOrOutputsAreMine = false;
                        std::vector<CAccount*> forAccounts;
                        for (const auto& [accountUUID, account] : pactiveWallet->mapAccounts)
                        {
                            forAccounts.push_back(account);
                        }
                        TransactionRecord walletTransaction = calculateTransactionRecordForWalletTransaction(wtx, forAccounts, anyInputsOrOutputsAreMine);
                        if (anyInputsOrOutputsAreMine)
                        {
                            walletListener->notifyUpdatedTransaction(walletTransaction);
                        }
                    }
                    //fixme: (UNITY) - Consider implementing f.e.x if a 0 conf transaction gets deleted...
                    // else if (status == CT_DELETED)
                }
                walletBalanceChangeNotifier->trigger(0);
            }
        });
    }
}

bool IWalletController::HaveUnconfirmedFunds()
{
    if (!pactiveWallet)
        return true;

    WalletBalances balances;
    pactiveWallet->GetBalances(balances, nullptr, true);

    if (balances.unconfirmedIncludingLocked > 0 || balances.immatureIncludingLocked > 0)
    {
        return true;
    }
    return false;
}

BalanceRecord IWalletController::GetBalance()
{
    if (!pactiveWallet)
        return BalanceRecord(0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

    WalletBalances balances;
    pactiveWallet->GetBalances(balances, nullptr, true);
    return BalanceRecord(balances.availableIncludingLocked, balances.availableExcludingLocked, balances.availableLocked, balances.unconfirmedIncludingLocked, balances.unconfirmedExcludingLocked, balances.unconfirmedLocked, balances.immatureIncludingLocked, balances.immatureExcludingLocked, balances.immatureLocked, balances.totalLocked);
    
}

int64_t IWalletController::GetBalanceSimple()
{
    if (!pactiveWallet)
        return 0;

    WalletBalances balances;
    pactiveWallet->GetBalances(balances, nullptr, true);
    return balances.availableIncludingLocked + balances.unconfirmedIncludingLocked + balances.immatureIncludingLocked;
}

bool IWalletController::AbandonTransaction(const std::string& txHash)
{
    if (!pactiveWallet)
        return false;

    uint256 hash = uint256S(txHash);
    return pactiveWallet->AbandonTransaction(hash);
}

std::string IWalletController::GetUUID()
{
    if (!pactiveWallet || !pactiveWallet->getActiveAccount())
        return "";
    
    return getUUIDAsString(pactiveWallet->getActiveAccount()->getUUID());
}

