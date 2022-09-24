// Copyright (c) 2018-2022 The Centure developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the Libre Chain License, see the accompanying
// file COPYING

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

#include <thread>
#include <torcontrol.h>
#include <rpc/server.h>
#include <httpserver.h>
#include <httprpc.h>
#include "ui_interface.h"
#include "rpc/blockchain.h"
#include "validation/validation.h"


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

void ServerInterrupt()
{
    InterruptHTTPServer();
    InterruptHTTPRPC();
    InterruptRPC();
    InterruptREST();
    InterruptTorControl();
}

void ServerShutdown(node::NodeContext& nodeContext)
{
    StopHTTPServer();
    StopHTTPRPC();
    MilliSleep(20); //Allow other threads (UI etc. a chance to cleanup as well)
    StopRPC();
    StopREST();
    MilliSleep(20); //Allow other threads (UI etc. a chance to cleanup as well)
    StopTorControl();
    // After everything has been shut down, but before things get flushed, stop the
    // CScheduler/checkqueue, scheduler and load block thread.
    if (nodeContext.scheduler) nodeContext.scheduler->stop();
    StopScriptCheckWorkerThreads();
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

static bool AppInitServers()
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

bool InitRPCWarmup()
{
    if (GetBoolArg("-server", false))
    {
        uiInterface.InitMessage.connect(SetRPCWarmupStatus);
        if (!AppInitServers())
            return InitError("Unable to start HTTP server. See debug log for details.");
    }
    return true;
}

bool InitTor()
{
    if (GetBoolArg("-listenonion", DEFAULT_LISTEN_ONION))
        StartTorControl();
    return true;
}

bool ILibraryController::InitWalletFromAndroidLegacyProtoWallet(const std::string& walletFile, const std::string& oldPassword, const std::string& newPassword)
{
    // only exists here to keep the compiler happy, never call this on nodejs
    LogPrintf("DO NOT call ILibraryController::InitWalletFromAndroidLegacyProtoWallet on nodejs\n");
    assert(false);
    return false;
}

LegacyWalletResult ILibraryController::isValidAndroidLegacyProtoWallet(const std::string& walletFile, const std::string& oldPassword)
{
    // only exists here to keep the compiler happy, never call this on nodejs
    LogPrintf("DO NOT call ILibraryController::isValidAndroidLegacyProtoWallet on nodejs\n");
    assert(false);
    return LegacyWalletResult::INVALID_OR_CORRUPT;
}
