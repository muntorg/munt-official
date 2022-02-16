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
#include "node/context.h"

#include "unity/djinni/cpp/legacy_wallet_result.hpp"
#include "unity/djinni/cpp/i_library_controller.hpp"

#include <boost/thread.hpp>
#include <thread>


class NodeRPCTimer : public RPCTimerBase
{
public:
    NodeRPCTimer(std::function<void(void)>& func, int64_t millis)
    {
        std::thread([=]()
        {
            MilliSleep(millis);
            func();
        }).detach();
    }
private:
};

class NodeRPCTimerInterface : public RPCTimerInterface
{
public:
    NodeRPCTimerInterface()
    {
    }
    const char* Name()
    {
        return "Node";
    }
    RPCTimerBase* NewTimer(std::function<void(void)>& func, int64_t millis)
    {
        return new NodeRPCTimer(func, millis);
    }
private:
};


extern std::string HelpMessage(HelpMessageMode mode)
{
    return "";
}


NodeRPCTimerInterface* timerInterface = nullptr;

void InitRegisterRPC()
{
    RegisterAllCoreRPCCommands(tableRPC);
    #ifdef ENABLE_WALLET
        RegisterWalletRPCCommands(tableRPC);
    #endif
        
    timerInterface = new NodeRPCTimerInterface();
    RPCSetTimerInterface(timerInterface);
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

void ServerShutdown(boost::thread_group& threadGroup, node::NodeContext& nodeContext)
{
    RPCUnsetTimerInterface(timerInterface);
    // After everything has been shut down, but before things get flushed, stop the
    // CScheduler/checkqueue, scheduler and load block thread.
    if (nodeContext.scheduler) nodeContext.scheduler->stop();
    //StopRPC();
}

/*void InitRPCMining()
{
}*/

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
