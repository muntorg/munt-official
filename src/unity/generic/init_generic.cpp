// Copyright (c) 2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "init.h"
#include "chainparams.h"
#include "chain.h"
#include "generation/miner.h"
#include "httprpc.h"
#include "httpserver.h"
#include "net.h"
#include "net_processing.h"
#include "policy/policy.h"
#include "rpc/blockchain.h"
#include "rpc/register.h"
#include "rpc/server.h"
#include "script/sigcache.h"
#include "torcontrol.h"
#include "txdb.h"
#include "util.h"

#ifdef ENABLE_WALLET
#include "wallet/wallet.h"
#endif

std::string HelpMessage(HelpMessageMode mode)
{
    return "";
}

void InitRegisterRPC()
{
}

void ServerInterrupt()
{
}

void ServerShutdown(node::NodeContext& nodeContext)
{
}

static void OnRPCStarted()
{
}

static void OnRPCStopped()
{
}

static void OnRPCPreCommand(const CRPCCommand& cmd)
{
}

static bool AppInitServers()
{
    return true;
}

bool InitRPCWarmup()
{
    return true;
}

bool InitTor()
{
    return true;
}


