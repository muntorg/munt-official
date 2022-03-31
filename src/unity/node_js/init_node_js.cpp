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
#include "warnings.h"
#include "node/context.h"

#include "unity/djinni/cpp/legacy_wallet_result.hpp"
#include "unity/djinni/cpp/i_library_controller.hpp"

#include <boost/thread.hpp>
#include <thread>
#include <torcontrol.h>
#include <rpc/server.h>
#include <httpserver.h>
#include <httprpc.h>
#include "ui_interface.h"
#include "rpc/blockchain.h"
#include "validation/validation.h"


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
    InterruptHTTPServer();
    InterruptHTTPRPC();
    InterruptRPC();
    InterruptREST();
    InterruptTorControl();
}

void ServerShutdown(boost::thread_group& threadGroup, node::NodeContext& nodeContext)
{
    RPCUnsetTimerInterface(timerInterface);
    StopHTTPServer();
    StopHTTPRPC();
    MilliSleep(20); //Allow other threads (UI etc. a chance to cleanup as well)
    if (nodeContext.scheduler) nodeContext.scheduler->stop();
    StopRPC();
    StopREST();
    MilliSleep(20); //Allow other threads (UI etc. a chance to cleanup as well)
    StopTorControl();
}

static void OnRPCStarted()
{
    uiInterface.NotifyBlockTip.connect(&RPCNotifyBlockChange);
}

static void OnRPCStopped()
{
    uiInterface.NotifyBlockTip.disconnect(&RPCNotifyBlockChange);
    RPCNotifyBlockChange(false, nullptr);
    cvBlockChange.notify_all();
    LogPrint(BCLog::RPC, "RPC stopped.\n");
}

static void OnRPCPreCommand(const CRPCCommand& cmd)
{
    // Observe safe mode
    std::string strWarning = GetWarnings("rpc");
    if (strWarning != "" && !GetBoolArg("-disablesafemode", DEFAULT_DISABLE_SAFEMODE) &&
        !cmd.okSafeMode)
        throw JSONRPCError(RPC_FORBIDDEN_BY_SAFE_MODE, std::string("Safe mode: ") + strWarning);
}

static bool AppInitServers(boost::thread_group& threadGroup)
{
    RPCServer::OnStarted(&OnRPCStarted);
    RPCServer::OnStopped(&OnRPCStopped);
    RPCServer::OnPreCommand(&OnRPCPreCommand);
    if (!InitHTTPServer())
        return false;
    if (!StartRPC())
        return false;
    if (!StartHTTPRPC())
        return false;
    if (GetBoolArg("-rest", DEFAULT_REST_ENABLE) && !StartREST())
        return false;
    if (!StartHTTPServer())
        return false;
    return true;
}

bool InitRPCWarmup(boost::thread_group& threadGroup)
{
    if (GetBoolArg("-server", false))
    {
        uiInterface.InitMessage.connect(SetRPCWarmupStatus);
        if (!AppInitServers(threadGroup))
            return InitError("Unable to start HTTP server. See debug log for details.");
    }
    return true;
}

bool InitTor(boost::thread_group& threadGroup, CScheduler& scheduler)
{
    if (GetBoolArg("-listenonion", DEFAULT_LISTEN_ONION))
        StartTorControl(threadGroup, scheduler);
    return true;
}

bool ILibraryController::InitWalletFromAndroidLegacyProtoWallet(const std::string& walletFile, const std::string& oldPassword, const std::string& newPassword)
{
    // only exists here to keep the compiler happy, never call this on nodejs
    LogPrintf("DO NOT call ILibraryController::InitWalletFromAndroidLegacyProtoWallet on nodejs\n");
    assert(false);
}

LegacyWalletResult ILibraryController::isValidAndroidLegacyProtoWallet(const std::string& walletFile, const std::string& oldPassword)
{
    // only exists here to keep the compiler happy, never call this on nodejs
    LogPrintf("DO NOT call ILibraryController::isValidAndroidLegacyProtoWallet on nodejs\n");
    assert(false);
    return LegacyWalletResult::INVALID_OR_CORRUPT;
}
