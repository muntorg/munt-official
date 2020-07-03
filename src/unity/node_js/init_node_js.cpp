// Copyright (c) 2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "chain.h"
#include "init.h"
#include "rpc/register.h"
#include "rpc/server.h"
#ifdef ENABLE_WALLET
#include "wallet/rpcwallet.h"
#endif

#include "unity/djinni/cpp/legacy_wallet_result.hpp"
#include "unity/djinni/cpp/unified_backend.hpp"

#include <boost/thread.hpp>

extern std::string HelpMessage(HelpMessageMode mode)
{
    return "";
}

void InitRegisterRPC()
{
    RegisterAllCoreRPCCommands(tableRPC);
    #ifdef ENABLE_WALLET
        RegisterWalletRPCCommands(tableRPC);
    #endif
}

void ServerInterrupt(boost::thread_group& threadGroup)
{
    //InterruptRPC();
}

bool InitRPCWarmup(boost::thread_group& threadGroup)
{
    return true;
}

/*void SetRPCWarmupFinished()
{
}*/

/*void RPCNotifyBlockChange(bool ibd, const CBlockIndex * pindex)
{
}*/

void ServerShutdown(boost::thread_group& threadGroup)
{
    //StopRPC();
}

/*void InitRPCMining()
{
}*/

bool InitTor(boost::thread_group& threadGroup, CScheduler& scheduler)
{
    return true;
}

bool UnifiedBackend::InitWalletFromAndroidLegacyProtoWallet(const std::string& walletFile, const std::string& oldPassword, const std::string& newPassword)
{
    // only exists here to keep the compiler happy, never call this on iOS
    LogPrintf("DO NOT call UnifiedBackend::InitWalletFromAndroidLegacyProtoWallet on iOS\n");
    assert(false);
}

LegacyWalletResult UnifiedBackend::isValidAndroidLegacyProtoWallet(const std::string& walletFile, const std::string& oldPassword)
{
    // only exists here to keep the compiler happy, never call this on iOS
    LogPrintf("DO NOT call UnifiedBackend::isValidAndroidLegacyProtoWallet on iOS\n");
    assert(false);
    return LegacyWalletResult::INVALID_OR_CORRUPT;
}
