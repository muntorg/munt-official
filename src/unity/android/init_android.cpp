// Copyright (c) 2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/thread.hpp>
#include "chain.h"
#include "init.h"

#include "unity/compat/android_wallet.h"
#include "unity/djinni/cpp/legacy_wallet_result.hpp"
#include "unity/djinni/cpp/gulden_unified_backend.hpp"

extern std::string HelpMessage(HelpMessageMode mode)
{
    return "";
}

void InitRegisterRPC()
{
}

void ServerInterrupt(boost::thread_group& threadGroup)
{
}

bool InitRPCWarmup(boost::thread_group& threadGroup)
{
    return true;
}

void SetRPCWarmupFinished()
{
}

void RPCNotifyBlockChange(bool ibd, const CBlockIndex * pindex)
{
}

void ServerShutdown(boost::thread_group& threadGroup)
{
}

void InitRPCMining()
{
}

bool InitTor(boost::thread_group& threadGroup, CScheduler& scheduler)
{
    return true;
}


bool GuldenUnifiedBackend::InitWalletFromAndroidLegacyProtoWallet(const std::string& walletFile, const std::string& oldPassword, const std::string& newPassword)
{
    android_wallet wallet = ParseAndroidProtoWallet(walletFile, oldPassword);
    if (wallet.validWalletProto && wallet.validWallet)
    {
        if (wallet.walletSeedMnemonic.length() > 0)
        {
            if (wallet.walletSeedMnemonic.find("-") != std::string::npos && wallet.walletSeedMnemonic.find(":") != std::string::npos)
            {
                return InitWalletLinkedFromURI("guldensync:"+wallet.walletSeedMnemonic+";unused_payout_address", newPassword.c_str());
            }
            else
            {
                if (wallet.walletBirth > 0)
                {
                    return InitWalletFromRecoveryPhrase(ComposeRecoveryPhrase(wallet.walletSeedMnemonic, wallet.walletBirth), newPassword.c_str());
                }
                else
                {
                    return InitWalletFromRecoveryPhrase(wallet.walletSeedMnemonic.c_str(), newPassword.c_str());
                }
            }
        }
    }
    return false;
}

LegacyWalletResult GuldenUnifiedBackend::isValidAndroidLegacyProtoWallet(const std::string& walletFile, const std::string& oldPassword)
{
    LogPrintf("Checking for valid legacy wallet proto [%s]\n", walletFile.c_str());

    android_wallet wallet = ParseAndroidProtoWallet(walletFile, oldPassword);

    if (wallet.validWalletProto)
    {
        LogPrintf("Valid proto found\n");

        if (wallet.encrypted)
        {
            LogPrintf("Proto is encrypted\n");

            if (!wallet.validWallet)
            {
                if ( oldPassword.length() == 0 )
                {
                    LogPrintf("Password required\n");
                    return LegacyWalletResult::ENCRYPTED_PASSWORD_REQUIRED;
                }
                LogPrintf("Password is invalid\n");
                return LegacyWalletResult::PASSWORD_INVALID;
            }
        }
        if (wallet.walletSeedMnemonic.length() > 0)
        {
            LogPrintf("Wallet is valid\n");
            return LegacyWalletResult::VALID;
        }
    }
    LogPrintf("Wallet invalid or corrupt [%s]\n", wallet.resultMessage);
    return LegacyWalletResult::INVALID_OR_CORRUPT;
}
