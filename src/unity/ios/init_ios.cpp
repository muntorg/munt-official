// Copyright (c) 2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/thread.hpp>
#include "chain.h"
#include "init.h"

#include "unity/djinni/cpp/legacy_wallet_result.hpp"
#include "unity/djinni/cpp/i_library_controller.hpp"

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

void ServerShutdown(boost::thread_group& threadGroup, node::NodeContext& nodeContext)
{
    // After everything has been shut down, but before things get flushed, stop the
    // CScheduler/checkqueue, scheduler and load block thread.
    if (node.scheduler) node.scheduler->stop();
}

void InitRPCMining()
{
}

bool InitTor(boost::thread_group& threadGroup, CScheduler& scheduler)
{
    return true;
}

bool ILibraryController::InitWalletFromAndroidLegacyProtoWallet(const std::string& walletFile, const std::string& oldPassword, const std::string& newPassword)
{
    // only exists here to keep the compiler happy, never call this on iOS
    LogPrintf("DO NOT call ILibraryController::InitWalletFromAndroidLegacyProtoWallet on iOS\n");
    assert(false);
}

LegacyWalletResult ILibraryController::isValidAndroidLegacyProtoWallet(const std::string& walletFile, const std::string& oldPassword)
{
    // only exists here to keep the compiler happy, never call this on iOS
    LogPrintf("DO NOT call ILibraryController::isValidAndroidLegacyProtoWallet on iOS\n");
    assert(false);
    return LegacyWalletResult::INVALID_OR_CORRUPT;
}
