// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef CORE_INIT_H
#define CORE_INIT_H

#include <string>

class CScheduler;
class CWallet;

static const bool DEFAULT_PROXYRANDOMIZE = true;
static const bool DEFAULT_REST_ENABLE = false;
static const bool DEFAULT_DISABLE_SAFEMODE = false;
static const bool DEFAULT_STOPAFTERBLOCKIMPORT = false;
static const bool DEFAULT_SPV = false;

//fixme: (PHASE4) comment this out for release
//#define BETA_BUILD

namespace boost
{
    class thread_group;
}

namespace node
{
    struct NodeContext;
}

/** Interrupt core threads */
void CoreInterrupt(boost::thread_group& threadGroup);
/** Stop core threads */
void CoreShutdown(boost::thread_group& threadGroup, node::NodeContext& nodeContext);
//!Initialize the logging infrastructure
void InitLogging();
//!Initialize any app specific hardcoded paramaters here (e.g. the android wallet will set -spv)
//!This is called after loading the config file
void InitAppSpecificConfigParamaters();
//!Parameter interaction: change current parameters depending on various rules
void InitParameterInteraction();

/** Initialize Gulden: Basic context setup.
 *  @note This can be done before daemonization.
 *  @pre Parameters should be parsed and config file should be read.
 */
bool AppInitBasicSetup();
/**
 * Initialization: parameter interaction.
 * @note This can be done before daemonization.
 * @pre Parameters should be parsed and config file should be read, AppInitBasicSetup should have been called.
 */
bool AppInitParameterInteraction();
/**
 * Initialization sanity checks: ecc init, sanity checks, dir lock.
 * @note This can be done before daemonization.
 * @pre Parameters should be parsed and config file should be read, AppInitParameterInteraction should have been called.
 */
bool AppInitSanityChecks();
/**
 * Gulden main initialization.
 * @note This should only be done after daemonization.
 * @pre Parameters should be parsed and config file should be read, AppInitSanityChecks should have been called.
 */
bool AppInitMain(boost::thread_group& threadGroup, node::NodeContext& node);

/** The help message mode determines what help message to show */
enum HelpMessageMode {
    HMM_GULDEND,
    HMM_GULDEN_QT
};

/** Help for options shared between UI and daemon (for -help) */
std::string HelpMessage(HelpMessageMode mode);
/** Returns licensing information (for -version) */
std::string LicenseInfo();

extern bool partiallyEraseDatadirOnShutdown;
extern bool fullyEraseDatadirOnShutdown;

#endif
