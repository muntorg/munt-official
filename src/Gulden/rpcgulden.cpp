// Copyright (c) 2015-2016 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#include "rpcgulden.h"
#include "../miner.h"
#include <rpc/server.h>
#include <wallet/rpcwallet.h>
#include "main.h"

#include <boost/assign/list_of.hpp>

#include "wallet/wallet.h"

#include <univalue.h>

#include "wallet/wallet.h"
#include <Gulden/translate.h>
#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/accumulators/statistics/median.hpp>
#include <boost/accumulators/statistics/mean.hpp>
#include <boost/accumulators/statistics/min.hpp>
#include <boost/accumulators/statistics/max.hpp>


using namespace std;

#ifdef ENABLE_WALLET

bool EnsureWalletIsAvailable(bool avoidException);

UniValue gethashps(const UniValue& params, bool fHelp)
{
    if (fHelp)
        throw std::runtime_error(
            "gethashps\n"
            "\nReturns the estimated hashes per second that this computer is mining at.\n");

    if (dHashesPerSec > 1000000)
        return strprintf("%lf mh/s (%lf)", dHashesPerSec/1000000, dBestHashesPerSec/1000000);
    else if (dHashesPerSec > 1000)
        return strprintf("%lf kh/s (%ls)", dHashesPerSec/1000, dBestHashesPerSec/1000);
    else
        return strprintf("%lf h/s (%lf)", dHashesPerSec, dBestHashesPerSec);
}

UniValue sethashlimit(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw std::runtime_error(
            "sethashlimit  ( limit )\n"
            "\nSet the maximum number of hashes to calculate per second when mining.\n"
            "\nThis mainly exists for testing purposes but can also be used to limit CPU usage a little.\n"
            "\nArguments:\n"
            "1. limit     (numeric) The number of hashes to allow per second, or -1 to remove limit.\n"
            "\nExamples:\n"
            + HelpExampleCli("sethashlimit 500000", "")
            + HelpExampleRpc("sethashlimit 500000", ""));

    RPCTypeCheck(params, boost::assign::list_of(UniValue::VNUM));

    nHashThrottle = params[0].get_int();

    LogPrintf("<DELTA> hash throttle %ld\n", nHashThrottle);

    return nHashThrottle;
}

UniValue dumpdiffarray(const UniValue& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;
    
    if (fHelp || params.size() != 1)
        throw std::runtime_error(
            "dumpdiffarray  ( height )\n"
            "\nDump code for a c++ array containing 'height' integers, where each integer represents the difficulty (nBits) of a block.\n"
            "\nThis mainly exists for testing and development purposes, and can be used to help verify that your client has not been tampered with.\n"
            "\nArguments:\n"
            "1. height     (numeric) The number of blocks to add to the array.\n"
            "\nExamples:\n"
            + HelpExampleCli("dumpdiffarray 260000", ""));

    RPCTypeCheck(params, boost::assign::list_of(UniValue::VNUM));

    std::string reverseOutBuffer;
    std::string scratchBuffer;
    int nNumToOutput = params[0].get_int();
    reverseOutBuffer.reserve(16*nNumToOutput);

    CBlockIndex* pBlock = chainActive.Tip();
    while(pBlock->pprev && pBlock->nHeight>nNumToOutput)
        pBlock = pBlock->pprev;

    int count=1;
    while(pBlock)
    {
        --nNumToOutput;
        scratchBuffer = itostr(pBlock->nBits);
        std::reverse(scratchBuffer.begin(), scratchBuffer.end());
        if (count!= 1)
            reverseOutBuffer += " ,";
        reverseOutBuffer += scratchBuffer;
        if (count%10 == 0)
            reverseOutBuffer += "        \n";
        pBlock = pBlock->pprev;
        count=count+1;
    }

    std::reverse(reverseOutBuffer.begin(), reverseOutBuffer.end());
    LogPrintf("%s",reverseOutBuffer);

    return reverseOutBuffer;
}


UniValue dumpblockgaps(const UniValue& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;
    
    if (fHelp || params.size() != 2)
        throw std::runtime_error(
            "dumpblockgaps  startheight count\n"
            "\nDump the block gaps for the last n blocks.\n"
            "\nArguments:\n"
            "1. startheight     (numeric) Where to start dumping from, counting backwards from chaintip.\n"
            "2. count           (numeric) The number of blocks to dump the block gaps of - going backwards from the startheight.\n"
            "\nExamples:\n"
            + HelpExampleCli("dumpblockgaps 50", ""));

    RPCTypeCheck(params, boost::assign::list_of(UniValue::VNUM));
    
    int nStart = params[0].get_int();
    int nNumToOutput = params[1].get_int();

    CBlockIndex* pBlock = chainActive.Tip();

    UniValue jsonGaps(UniValue::VARR);
    
    boost::accumulators::accumulator_set<double, boost::accumulators::stats<boost::accumulators::tag::median(boost::accumulators::with_p_square_quantile), boost::accumulators::tag::mean, boost::accumulators::tag::min, boost::accumulators::tag::max> > gapStats;

    while(pBlock && pBlock->pprev && --nStart>0)
    {
        pBlock = pBlock->pprev;
    }
        
    while(pBlock && pBlock->pprev && --nNumToOutput>0)
    {        
        int64_t gap = std::abs((int64_t)pBlock->nTime - (int64_t)pBlock->pprev->nTime);
        pBlock = pBlock->pprev;
        if (gap > 6000)
        {
            continue;
        }
        jsonGaps.push_back(gap);
        gapStats(gap);                
    }
    
    jsonGaps.push_back("max:");
    jsonGaps.push_back(boost::accumulators::max(gapStats));
    jsonGaps.push_back("min:");
    jsonGaps.push_back(boost::accumulators::min(gapStats));
    jsonGaps.push_back("mean:");
    jsonGaps.push_back(boost::accumulators::mean(gapStats));
    jsonGaps.push_back("median:");
    jsonGaps.push_back(boost::accumulators::median(gapStats));

    return jsonGaps;
}



UniValue changeaccountname(const UniValue& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;
    
    if (fHelp || params.size() != 2)
        throw std::runtime_error(
            "changeaccountname \"account\" \"name\"\n"
            "\nChange the name of an account.\n"
            "\nArguments:\n"
            "1. \"account\"         (string) The UUID or unique label of the account.\n"
            "2. \"name\"           (string) The new label for the account.\n"
            "\nResult:\n"
            "\nReturn the final label for the account. Note this may be different from the given label in the case of duplicates.\n"
            "\nActive account is used as the default for all commands that take an optional account argument.\n"
            "\nExamples:\n"
            + HelpExampleCli("changeaccountname", "")
            + HelpExampleRpc("changeaccountname", ""));


    if (!pwalletMain)
        throw runtime_error("Cannot use command without an active wallet");
    
    CAccount* account = AccountFromValue(params[0], false);
    std::string label = params[1].get_str();
    
    pwalletMain->changeAccountName(account, label);
    
    return account->getLabel();
}

#define MINIMUM_VALUABLE_AMOUNT 1000000000 
UniValue deleteaccount(const UniValue& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;
    
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw std::runtime_error(
            "deleteaccount \"account\" (force)\n"
            "\nDelete an account.\n"
            "\nArguments:\n"
            "1. \"account\"        (string) The UUID or unique label of the account.\n"
            "2. \"force\"          (string) Specify string force to force deletion of a non-empty account.\n"
            "\nExamples:\n"
            + HelpExampleCli("deleteaccount", "")
            + HelpExampleRpc("deleteaccount", ""));


    if (!pwalletMain)
        throw runtime_error("Cannot use command without an active wallet");
    
    CAccount* account = AccountFromValue(params[0], false);
    
    if (params.size() == 1 || params[1].get_str() != "force")
    {
        CAmount balance = pwalletMain->GetAccountBalance(account->getUUID(), 0, ISMINE_SPENDABLE, true );
        if (balance > MINIMUM_VALUABLE_AMOUNT)
        {
            throw runtime_error("Account not empty, please first empty your account before trying to delete it.");
        }
    }
            
    pwalletMain->deleteAccount(account);
    return true;
}



UniValue createaccount(const UniValue& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;
    
    if (fHelp || params.size() != 1)
        throw std::runtime_error(
            "createaccount \"name\"\n"
            "Create an account.\n"
            "\nArguments:\n"
            //"1. \"seed\"        (string) The UUID or unique label of the seed from which to create the account. \"\" to use the currently active seed.\n"
            "1. \"name\"       (string) Specify the label for the account.\n"
            "\nExamples:\n"
            + HelpExampleCli("createaccount", "")
            + HelpExampleRpc("createaccount", ""));


    if (!pwalletMain)
        throw runtime_error("Cannot use command without an active wallet");
    
    //CHDSeed* seed = SeedFromValue(params[0], true);
                    
    CAccount* account = pwalletMain->GenerateNewAccount(params[0].get_str(), AccountType::Normal, AccountSubType::Desktop);
    
    if (!account)
        throw runtime_error("Unable to create account.");
    
    return account->getUUID();
}



UniValue getactiveaccount(const UniValue& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;
    
    if (fHelp || params.size() != 0)
        throw std::runtime_error(
            "getactiveaccount \n"
            "\nReturn the UUID for the currently active account.\n"
            "\nActive account is used as the default for all commands that take an optional account argument.\n"
            "\nExamples:\n"
            + HelpExampleCli("getactiveaccount", "")
            + HelpExampleRpc("getactiveaccount", ""));


    if (!pwalletMain)
        throw runtime_error("Cannot use command without an active wallet");
    
    if (!pwalletMain->activeAccount)
        throw runtime_error("No account active");
    
    return pwalletMain->activeAccount->getUUID();
}

UniValue getactiveseed(const UniValue& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;
    
    if (fHelp || params.size() != 0)
        throw std::runtime_error(
            "getactiveseed \n"
            "\nReturn the UUID for the currently active account.\n"
            "\nActive account is used as the default for all commands that take an optional account argument.\n"
            "\nExamples:\n"
            + HelpExampleCli("getactiveseed", "")
            + HelpExampleRpc("getactiveseed", ""));


    if (!pwalletMain)
        throw runtime_error("Cannot use command without an active wallet");
    
    if (!pwalletMain->activeSeed)
        throw runtime_error("No seed active");
    
    return pwalletMain->activeSeed->getUUID();
}

UniValue setactiveaccount(const UniValue& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;
    
    if (fHelp || params.size() != 1)
        throw std::runtime_error(
            "setactiveaccount \n"
            "\nSet the currently active account based on name or uuid.\n"
            "\nExamples:\n"
            + HelpExampleCli("setactiveaccount", "")
            + HelpExampleRpc("setactiveaccount", ""));

    if (!pwalletMain)
        throw runtime_error("Cannot use command without an active wallet");
    
    CAccount* account = AccountFromValue(params[0], false);  

    pwalletMain->setActiveAccount(account);
    return account->getUUID();
}

CHDSeed* SeedFromValue(const UniValue& value, bool useDefaultIfEmpty)
{
    string strSeedUUID = value.get_str();
          
    if (!pwalletMain)
        throw runtime_error("Cannot use command without an active wallet");
    
    if (strSeedUUID.empty())
    {
        if (!pwalletMain->getActiveSeed())
        {
            throw runtime_error("No seed identifier passed, and no active seed selected, please select an active seed or pass a valid identifier.");
        }
        return pwalletMain->getActiveSeed();
    }
    
    CHDSeed* foundSeed = NULL;
    if (pwalletMain->mapSeeds.find(strSeedUUID) != pwalletMain->mapSeeds.end())
    {
        foundSeed = pwalletMain->mapSeeds[strSeedUUID];
    }
    
    if (!foundSeed)
        throw JSONRPCError(RPC_WALLET_INVALID_ACCOUNT_NAME, "Not a valid seed UUID.");
    
    return foundSeed;
}

UniValue setactiveseed(const UniValue& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;
    
    if (fHelp || params.size() != 1)
        throw std::runtime_error(
            "setactiveseed \n"
            "\nSet the currently active seed by UUID.\n"
            "\nExamples:\n"
            + HelpExampleCli("setactiveseed", "")
            + HelpExampleRpc("setactiveseed", ""));

    if (!pwalletMain)
        throw runtime_error("Cannot use command without an active wallet");
    
    CHDSeed* seed = SeedFromValue(params[0], false);  

    pwalletMain->setActiveSeed(seed);
    return seed->getUUID();
}

UniValue createseed(const UniValue& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;
    
    if (fHelp || params.size() != 0)
        throw std::runtime_error(
            "createseed \n"
            "\nCreate a new seed using random entropy.\n"
            "\nResult:\n"
            "\nReturn the UUID of the new seed.\n"
            "\nExamples:\n"
            + HelpExampleCli("createseed", "")
            + HelpExampleRpc("createseed", ""));

    if (!pwalletMain)
        throw runtime_error("Cannot use command without an active wallet");
    
    EnsureWalletIsUnlocked();
    
    CHDSeed *newSeed = pwalletMain->GenerateHDSeed();
    
    if(!newSeed)
        throw runtime_error("Failed to generate seed");

    return newSeed->getUUID();
}

UniValue importseed(const UniValue& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;
    
    if (fHelp || params.size() != 1)
        throw std::runtime_error(
            "importseed \"mnemonic\" \n"
            "\nSet the currently active seed by UUID.\n"
            "1. \"mnemonic\"       (string) Specify the BIP44 mnemonic that will be used to generate the seed.\n"
            "\nResult:\n"
            "\nReturn the UUID of the new seed.\n"
            "\nExamples:\n"
            + HelpExampleCli("importseed", "")
            + HelpExampleRpc("importseed", ""));

    if (!pwalletMain)
        throw runtime_error("Cannot use command without an active wallet");
    
    EnsureWalletIsUnlocked();
    
    SecureString mnemonic = params[0].get_str().c_str();
    CHDSeed* newSeed = pwalletMain->ImportHDSeed(mnemonic);

    pwalletMain->ScanForWalletTransactions(chainActive.Genesis(), true);
    
    return newSeed->getUUID();
}

UniValue listallaccounts(const UniValue& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;
    
    if (fHelp || params.size() > 1)
        throw std::runtime_error(
            "listaccounts ( forseed )\n"
            "\nArguments:\n"
            "1. \"seed\"        (string, optional) The unique UUID for the seed that we want accounts of.\n"
            "\nResult:\n"
            "\nReturn the UUID and label for all wallet accounts.\n"
            "\nNote UUID is guaranteed to be unique while label is not.\n"
            "\nExamples:\n"
            + HelpExampleCli("listaccounts", "")
            + HelpExampleRpc("listaccounts", ""));

    CHDSeed* forSeed = NULL;
    if (params.size() > 0)
        forSeed = SeedFromValue(params[0], false);

    if (!pwalletMain)
        throw runtime_error("Cannot use command without an active wallet");
    
    UniValue allAccounts(UniValue::VARR);
    
    for (const auto& accountPair : pwalletMain->mapAccounts)
    {
        if (!accountPair.second->IsHD())
        {
            if (!forSeed)
            {
                UniValue rec(UniValue::VOBJ);
                rec.push_back(Pair("UUID", accountPair.first));
                rec.push_back(Pair("label", accountPair.second->getLabel()));
                rec.push_back(Pair("type", "legacy"));
                allAccounts.push_back(rec);
            }
            continue;
        }
        if (accountPair.second->m_Type == AccountType::Shadow)
            continue;
        if (forSeed && ((CAccountHD*)accountPair.second)->getSeedUUID() != forSeed->getUUID())
            continue;
                
        UniValue rec(UniValue::VOBJ);
        rec.push_back(Pair("UUID", accountPair.first));
        rec.push_back(Pair("label", accountPair.second->getLabel()));
        rec.push_back(Pair("type", "HD"));
        allAccounts.push_back(rec);
    }
    
    return allAccounts;
}

UniValue getmnemonicfromseed(const UniValue& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;
    
    if (fHelp || params.size() != 1)
        throw std::runtime_error(
            "getmnemonicfromseed \"seed\" \n"
            "\nGet the mnemonic of a HD seed.\n"
            "1. \"seed\"        (string, required) The unique UUID for the seed that we want mnemonics of, or \"\" for the active seed.\n"
            "\nResult:\n"
            "\nReturn the mnemonic as a string.\n"
            "\nNote it is important to ensure that nobdy gets access to this mnemonic or all funds in accounts made from the seed can be compromised.\n"
            "\nExamples:\n"
            + HelpExampleCli("getmnemonicfromseed", "")
            + HelpExampleRpc("getmnemonicfromseed", ""));

    if (!pwalletMain)
        throw runtime_error("Cannot use command without an active wallet");
    
    CHDSeed* seed = SeedFromValue(params[0], true);  
    
    EnsureWalletIsUnlocked();

    return seed->getMnemonic().c_str();
}

std::string StringFromSeedType(CHDSeed* seed)
{
    switch(seed->m_type)
    {
        case CHDSeed::CHDSeed::BIP32:
            return "BIP32";
        case CHDSeed::CHDSeed::BIP32Legacy:
            return "BIP32 Legacy";
        case CHDSeed::CHDSeed::BIP44:
            return "BIP44";
        case CHDSeed::CHDSeed::BIP44External:
            return "BIP44 External";
    }
    return "unknown";
}

UniValue listseeds(const UniValue& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;
    
    if (fHelp || params.size() != 0)
        throw std::runtime_error(
            "listseeds \n"
            "\nReturn the UUID for all wallet seeds.\n"
            "\nExamples:\n"
            + HelpExampleCli("listseeds", "")
            + HelpExampleRpc("listseeds", ""));

    if (!pwalletMain)
        throw runtime_error("Cannot use command without an active wallet");
    
    UniValue AllSeeds(UniValue::VARR);
    
    for (const auto& seedPair : pwalletMain->mapSeeds)
    {
        UniValue rec(UniValue::VOBJ);
        rec.push_back(Pair("UUID", seedPair.first));
        rec.push_back(Pair("type", StringFromSeedType(seedPair.second)));
        AllSeeds.push_back(rec);
    }
    
    return AllSeeds;
}


static const CRPCCommand commands[] =
{ //  category              name                      actor (function)         okSafeMode
  //  --------------------- ------------------------  -----------------------  ----------
    { "mining",             "gethashps",              &gethashps,              true  },
    { "mining",             "sethashlimit",           &sethashlimit,           true  },

    { "developer",          "dumpdiffarray",          &dumpdiffarray,          true  },
    { "developer",          "dumpblockgaps",          &dumpblockgaps,          true  },
    
    { "accounts",           "changeaccountname",      &changeaccountname,     true  },
    { "accounts",           "createaccount",          &createaccount,          true  },
    { "accounts",           "deleteaccount",          &deleteaccount,          true  },
    { "accounts",           "getactiveaccount",       &getactiveaccount,       true  },
    { "accounts",           "listaccounts",           &listallaccounts,        true  },
    { "accounts",           "setactiveaccount",       &setactiveaccount,       true  },
    
    { "mnemonics",          "getactiveseed",          &getactiveseed,          true  },
    { "mnemonics",          "setactiveseed",          &setactiveseed,          true  },
    { "mnemonics",          "listseeds",              &listseeds,              true  },
    { "mnemonics",          "createseed",             &createseed,             true  },
    { "mnemonics",          "importseed",             &importseed,             true  },
    { "mnemonics",          "getmnemonicfromseed",    &getmnemonicfromseed,    true  },
};

void RegisterGuldenRPCCommands(CRPCTable &tableRPC)
{
    for (unsigned int vcidx = 0; vcidx < ARRAYLEN(commands); vcidx++)
        tableRPC.appendCommand(commands[vcidx].name, &commands[vcidx]);
}

#endif


