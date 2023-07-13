// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// File contains modifications by: The Centure developers
// All modifications:
// Copyright (c) 2016-2022 The Centure developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the GNU Lesser General Public License v3, see the accompanying
// file COPYING

#include "chainparamsbase.h"

#include "tinyformat.h"
#include "util.h"

#include <assert.h>

const std::string CBaseChainParams::MAIN = "main";
const std::string CBaseChainParams::TESTNET = "test";
const std::string CBaseChainParams::REGTEST = "regtest";
const std::string CBaseChainParams::REGTESTLEGACY = "regtestlegacy";

void AppendParamsHelpMessages(std::string& strUsage, bool debugHelp)
{
    strUsage += HelpMessageGroup(_("Chain selection options:"));
    strUsage += HelpMessageOpt("-testnet", _("Use or create a test blockchain. Specify the chain using the format <hashtype><genesistimestamp>:<blockintervaltarget> e.g. S1505211751:20 (Scrypt with 20 second block target) or C1505211751:10 (City hash with 10 second block target). For 'official' testnet chain specifiers view the latest README.md where the latest chains will always be listed."));
    if (debugHelp) {
        strUsage += HelpMessageOpt("-regtest", "Enter regression test mode, which uses a special chain in which blocks can be solved instantly. "
                                   "This is intended for regression testing tools and app development.");
    }
}

/**
 * Main network
 */
class CBaseMainParams : public CBaseChainParams
{
public:
    CBaseMainParams()
    {
        nRPCPort = 9232;
    }
};

/**
 * Testnet (v3)
 */
class CBaseTestNetParams : public CBaseChainParams
{
public:
    CBaseTestNetParams()
    {
        nRPCPort = 9924;
        std::string testnetArgs = GetArg("-testnet", "");
        std::replace( testnetArgs.begin(), testnetArgs.end(), ':', '_');
        strDataDir = (fs::path("testnet") / testnetArgs).string();
    }
};

/*
 * Regression test
 */
class CBaseRegTestParams : public CBaseChainParams
{
public:
    CBaseRegTestParams()
    {
        nRPCPort = 18332;
        strDataDir = "regtest";
    }
};

/*
 * Regression test
 */
class CBaseRegTestLegacyParams : public CBaseChainParams
{
public:
    CBaseRegTestLegacyParams()
    {
        nRPCPort = 18332;
        strDataDir = "regtestlegacy";
    }
};

static std::unique_ptr<CBaseChainParams> globalChainBaseParams;

const CBaseChainParams& BaseParams()
{
    assert(globalChainBaseParams);
    return *globalChainBaseParams;
}

std::unique_ptr<CBaseChainParams> CreateBaseChainParams(const std::string& chain)
{
    if (chain == CBaseChainParams::MAIN)
        return std::unique_ptr<CBaseChainParams>(new CBaseMainParams());
    else if (chain == CBaseChainParams::TESTNET)
        return std::unique_ptr<CBaseChainParams>(new CBaseTestNetParams());
    else if (chain == CBaseChainParams::REGTEST)
        return std::unique_ptr<CBaseChainParams>(new CBaseRegTestParams());
    else if (chain == CBaseChainParams::REGTESTLEGACY)
        return std::unique_ptr<CBaseChainParams>(new CBaseRegTestLegacyParams());
    else
        throw std::runtime_error(strprintf("%s: Unknown chain %s.", __func__, chain));
}

void SelectBaseParams(const std::string& chain)
{
    globalChainBaseParams = CreateBaseChainParams(chain);
}

std::string ChainNameFromCommandLine()
{
    bool fRegTest = GetBoolArg("-regtest", false);
    bool fRegTestLegacy = GetBoolArg("-regtestlegacy", false);
    bool fTestNet = IsArgSet("-testnet");

    if (fTestNet && (fRegTest||fRegTestLegacy))
        throw std::runtime_error("Invalid combination of -regtest and -testnet.");
    if (fRegTest)
        return CBaseChainParams::REGTEST;
    if (fRegTestLegacy)
        return CBaseChainParams::REGTESTLEGACY;
    if (fTestNet)
    {
        std::string sTestnetParams = GetArg("-testnet", "");
        if (sTestnetParams.empty())
            throw std::runtime_error("Invalid seed timestamp for testnet.");
        return CBaseChainParams::TESTNET;
    }
    return CBaseChainParams::MAIN;
}
