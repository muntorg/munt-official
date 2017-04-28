// Copyright (c) 2015-2016 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#include "rpcgulden.h"
#include "../miner.h"
#include <rpc/server.h>
#include <wallet/rpcwallet.h>
#include "validation.h"

#include <boost/assign/list_of.hpp>

#include "wallet/wallet.h"

#include <univalue.h>

#include <Gulden/translate.h>
#include <Gulden/util.h>
#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/accumulators/statistics/median.hpp>
#include <boost/accumulators/statistics/mean.hpp>
#include <boost/accumulators/statistics/min.hpp>
#include <boost/accumulators/statistics/max.hpp>


using namespace std;

#ifdef ENABLE_WALLET

UniValue gethashps(const JSONRPCRequest& request)
{
    if (request.fHelp)
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

UniValue sethashlimit(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw std::runtime_error(
            "sethashlimit  ( limit )\n"
            "\nSet the maximum number of hashes to calculate per second when mining.\n"
            "\nThis mainly exists for testing purposes but can also be used to limit CPU usage a little.\n"
            "\nArguments:\n"
            "1. limit     (numeric) The number of hashes to allow per second, or -1 to remove limit.\n"
            "\nExamples:\n"
            + HelpExampleCli("sethashlimit 500000", "")
            + HelpExampleRpc("sethashlimit 500000", ""));

    RPCTypeCheck(request.params, boost::assign::list_of(UniValue::VNUM));

    nHashThrottle = request.params[0].get_int();

    LogPrintf("<DELTA> hash throttle %ld\n", nHashThrottle);

    return nHashThrottle;
}

UniValue dumpdiffarray(const JSONRPCRequest& request)
{
    CWallet * const pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp))
        return NullUniValue;
    
    if (request.fHelp || request.params.size() != 1)
        throw std::runtime_error(
            "dumpdiffarray  ( height )\n"
            "\nDump code for a c++ array containing 'height' integers, where each integer represents the difficulty (nBits) of a block.\n"
            "\nThis mainly exists for testing and development purposes, and can be used to help verify that your client has not been tampered with.\n"
            "\nArguments:\n"
            "1. height     (numeric) The number of blocks to add to the array.\n"
            "\nExamples:\n"
            + HelpExampleCli("dumpdiffarray 260000", ""));

    RPCTypeCheck(request.params, boost::assign::list_of(UniValue::VNUM));

    std::string reverseOutBuffer;
    std::string scratchBuffer;
    int nNumToOutput = request.params[0].get_int();
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


UniValue dumpblockgaps(const JSONRPCRequest& request)
{
    CWallet * const pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp))
        return NullUniValue;
    
    if (request.fHelp || request.params.size() != 2)
        throw std::runtime_error(
            "dumpblockgaps  startheight count\n"
            "\nDump the block gaps for the last n blocks.\n"
            "\nArguments:\n"
            "1. startheight     (numeric) Where to start dumping from, counting backwards from chaintip.\n"
            "2. count           (numeric) The number of blocks to dump the block gaps of - going backwards from the startheight.\n"
            "\nExamples:\n"
            + HelpExampleCli("dumpblockgaps 50", ""));

    RPCTypeCheck(request.params, boost::assign::list_of(UniValue::VNUM));
    
    int nStart = request.params[0].get_int();
    int nNumToOutput = request.params[1].get_int();

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



UniValue changeaccountname(const JSONRPCRequest& request)
{
    #ifdef ENABLE_WALLET
    CWallet * const pwallet = GetWalletForJSONRPCRequest(request);

    LOCK2(cs_main, pwallet ? &pwallet->cs_wallet : NULL);
    #else
    LOCK(cs_main);
    #endif
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp))
        return NullUniValue;
    
    if (request.fHelp || request.params.size() != 2)
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

   

    if (!pwallet)
        throw runtime_error("Cannot use command without an active wallet");
    
    CAccount* account = AccountFromValue(pwallet, request.params[0], false);
    std::string label = request.params[1].get_str();
    
    pwallet->changeAccountName(account, label);
    
    return account->getLabel();
}

#define MINIMUM_VALUABLE_AMOUNT 1000000000 
UniValue deleteaccount(const JSONRPCRequest& request)
{
    #ifdef ENABLE_WALLET
    CWallet * const pwallet = GetWalletForJSONRPCRequest(request);

    LOCK2(cs_main, pwallet ? &pwallet->cs_wallet : NULL);
    #else
    LOCK(cs_main);
    #endif
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp))
        return NullUniValue;
    
    if (request.fHelp || request.params.size() < 1 || request.params.size() > 2)
        throw std::runtime_error(
            "deleteaccount \"account\" (force)\n"
            "\nDelete an account.\n"
            "\nArguments:\n"
            "1. \"account\"        (string) The UUID or unique label of the account.\n"
            "2. \"force\"          (string) Specify string force to force deletion of a non-empty account.\n"
            "\nExamples:\n"
            + HelpExampleCli("deleteaccount", "")
            + HelpExampleRpc("deleteaccount", ""));

    

    if (!pwallet)
        throw runtime_error("Cannot use command without an active wallet");
    
    CAccount* account = AccountFromValue(pwallet, request.params[0], false);
    
    if (request.params.size() == 1 || request.params[1].get_str() != "force")
    {
        CAmount balance = pwallet->GetAccountBalance(account->getUUID(), 0, ISMINE_SPENDABLE, true );
        if (balance > MINIMUM_VALUABLE_AMOUNT && !account->IsReadOnly())
        {
            throw runtime_error("Account not empty, please first empty your account before trying to delete it.");
        }
    }
            
    pwallet->deleteAccount(account);
    return true;
}



UniValue createaccount(const JSONRPCRequest& request)
{
    #ifdef ENABLE_WALLET
    CWallet * const pwallet = GetWalletForJSONRPCRequest(request);

    LOCK2(cs_main, pwallet ? &pwallet->cs_wallet : NULL);
    #else
    LOCK(cs_main);
    #endif
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp))
        return NullUniValue;
    
    if (request.fHelp || request.params.size() == 0 || request.params.size() > 2)
        throw std::runtime_error(
            "createaccount \"name\"\n"
            "Create an account, for HD accounts the currently active seed will be used to create the account.\n"
            "\nArguments:\n"
            "1. \"name\"       (string) Specify the label for the account.\n"
            "2. \"type\"       (string, optional) Type of account to create (HD; Mobile; Legacy)\n"
            "\nExamples:\n"
            + HelpExampleCli("createaccount", "")
            + HelpExampleRpc("createaccount", ""));

    

    if (!pwallet)
        throw runtime_error("Cannot use command without an active wallet");
    
    
    std::string accountType = "HD";
    if (request.params.size() > 1)
    {
        accountType = request.params[1].get_str();
        if (accountType != "HD" && accountType != "Mobile" && accountType != "Legacy")
            throw runtime_error("Invalid account type");    
    }
                       
    CAccount* account = NULL;
    
    if (accountType == "HD")
        account = pwallet->GenerateNewAccount(request.params[0].get_str(), AccountType::Normal, AccountSubType::Desktop);
    else if (accountType == "Mobile")
        account = pwallet->GenerateNewAccount(request.params[0].get_str(), AccountType::Normal, AccountSubType::Mobi);
    else if (accountType == "Legacy")
    {
        EnsureWalletIsUnlocked(pwallet);
        account = pwallet->GenerateNewLegacyAccount(request.params[0].get_str());
    }
    
    if (!account)
        throw runtime_error("Unable to create account.");
    
    return account->getUUID();
}



UniValue getactiveaccount(const JSONRPCRequest& request)
{
    #ifdef ENABLE_WALLET
    CWallet * const pwallet = GetWalletForJSONRPCRequest(request);

    LOCK2(cs_main, pwallet ? &pwallet->cs_wallet : NULL);
    #else
    LOCK(cs_main);
    #endif
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp))
        return NullUniValue;
    
    if (request.fHelp || request.params.size() != 0)
        throw std::runtime_error(
            "getactiveaccount \n"
            "\nReturn the UUID for the currently active account.\n"
            "\nActive account is used as the default for all commands that take an optional account argument.\n"
            "\nExamples:\n"
            + HelpExampleCli("getactiveaccount", "")
            + HelpExampleRpc("getactiveaccount", ""));

    

    if (!pwallet)
        throw runtime_error("Cannot use command without an active wallet");
    
    if (!pwallet->activeAccount)
        throw runtime_error("No account active");
    
    return pwallet->activeAccount->getUUID();
}

UniValue getreadonlyaccount(const JSONRPCRequest& request)
{
    #ifdef ENABLE_WALLET
    CWallet * const pwallet = GetWalletForJSONRPCRequest(request);

    LOCK2(cs_main, pwallet ? &pwallet->cs_wallet : NULL);
    #else
    LOCK(cs_main);
    #endif
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp))
        return NullUniValue;
    
    if (request.fHelp || request.params.size() != 1)
        throw std::runtime_error(
            "getreadonlyaccount \"account\" \n"
            "\nGet the public key of an HD account, this can be used to import the account as a read only account in another wallet.\n"
            "1. \"account\"        (string, required) The unique UUID or label for the account or \"\" for the active account.\n"
            "\nResult:\n"
            "\nReturn the public key as an encoded string, that can be used with the \"importreadonlyaccount\" command.\n"
            "\nNB! it is important to be careful with and protect access to this public key as if it is compromised it can compromise security of your entire wallet, in cases where one or more child private keys are also compromised.\n"
            "\nExamples:\n"
            + HelpExampleCli("getreadonlyaccount", "")
            + HelpExampleRpc("getreadonlyaccount", ""));

    
    
    if (!pwallet)
        throw runtime_error("Cannot use command without an active wallet");
    
    CAccount* account = AccountFromValue(pwallet, request.params[0], false);
    
    if (!account->IsHD())
        throw runtime_error("Can only be used on a HD account.");
    
    CAccountHD* accountHD = dynamic_cast<CAccountHD*>(account);
    
    EnsureWalletIsUnlocked(pwallet);

    return accountHD->GetAccountMasterPubKeyEncoded().c_str();
}

UniValue importreadonlyaccount(const JSONRPCRequest& request)
{
    #ifdef ENABLE_WALLET
    CWallet * const pwallet = GetWalletForJSONRPCRequest(request);

    LOCK2(cs_main, pwallet ? &pwallet->cs_wallet : NULL);
    #else
    LOCK(cs_main);
    #endif
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp))
        return NullUniValue;
    
    if (request.fHelp || request.params.size() != 2)
        throw std::runtime_error(
            "importreadonlyaccount \"name\" \"encodedkey\" \n"
            "\nImport a read only account from an \"encodedkey\" which has been obtained by using \"getreadonlyaccount\"\n"
            "1. \"name\"       (string) Name to assign to the new account.\n"
            "1. \"encodedkey\" (string) Encoded string containing the extended public key for the account.\n"
            "\nResult:\n"
            "\nReturn the UUID of the new account.\n"
            "\nExamples:\n"
            + HelpExampleCli("importreadonlyaccount \"watcher\" \"dd3tNdQ8A4KqYvYVvXzGEU7ChdNye9RdTixnLSFqpQHG-2Rakbbkn7GDUTdD6wtSd5KV5PnCFgQt3FPc8eYkMonRM\"", "")
            + HelpExampleRpc("importreadonlyaccount \"watcher\" \"dd3tNdQ8A4KqYvYVvXzGEU7ChdNye9RdTixnLSFqpQHG-2Rakbbkn7GDUTdD6wtSd5KV5PnCFgQt3FPc8eYkMonRM\"", ""));

    
    
    if (!pwallet)
        throw runtime_error("Cannot use command without an active wallet");
    
    EnsureWalletIsUnlocked(pwallet);
       
    CAccount* account = pwallet->CreateReadOnlyAccount(request.params[0].get_str().c_str(), request.params[1].get_str().c_str());
    
    if (!account)
        throw runtime_error("Unable to create account.");
    
    //fixme: Use a timestamp here
    // Whenever a key is imported, we need to scan the whole chain - do so now
    pwallet->nTimeFirstKey = 1;
    boost::thread t(rescanThread); // thread runs free
    
    return account->getUUID();
}

UniValue getactiveseed(const JSONRPCRequest& request)
{
    #ifdef ENABLE_WALLET
    CWallet * const pwallet = GetWalletForJSONRPCRequest(request);

    LOCK2(cs_main, pwallet ? &pwallet->cs_wallet : NULL);
    #else
    LOCK(cs_main);
    #endif
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp))
        return NullUniValue;
    
    if (request.fHelp || request.params.size() != 0)
        throw std::runtime_error(
            "getactiveseed \n"
            "\nReturn the UUID for the currently active account.\n"
            "\nActive account is used as the default for all commands that take an optional account argument.\n"
            "\nExamples:\n"
            + HelpExampleCli("getactiveseed", "")
            + HelpExampleRpc("getactiveseed", ""));

    

    if (!pwallet)
        throw runtime_error("Cannot use command without an active wallet");
    
    if (!pwallet->activeSeed)
        throw runtime_error("No seed active");
    
    return pwallet->activeSeed->getUUID();
}

UniValue setactiveaccount(const JSONRPCRequest& request)
{
    #ifdef ENABLE_WALLET
    CWallet * const pwallet = GetWalletForJSONRPCRequest(request);

    LOCK2(cs_main, pwallet ? &pwallet->cs_wallet : NULL);
    #else
    LOCK(cs_main);
    #endif
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp))
        return NullUniValue;
    
    if (request.fHelp || request.params.size() != 1)
        throw std::runtime_error(
            "setactiveaccount \n"
            "\nSet the currently active account based on name or uuid.\n"
            "1. \"account\"        (string, required) The unique UUID or label for the account or \"\" for the active account.\n"
            "\nExamples:\n"
            + HelpExampleCli("setactiveaccount", "")
            + HelpExampleRpc("setactiveaccount", ""));

   
    
    if (!pwallet)
        throw runtime_error("Cannot use command without an active wallet");
    
    CAccount* account = AccountFromValue(pwallet, request.params[0], false);  

    pwallet->setActiveAccount(account);
    return account->getUUID();
}

CHDSeed* SeedFromValue(CWallet* pwallet, const UniValue& value, bool useDefaultIfEmpty)
{
    string strSeedUUID = value.get_str();
    
    if (!pwallet)
        throw runtime_error("Cannot use command without an active wallet");
    
    if (strSeedUUID.empty())
    {
        if (!pwallet->getActiveSeed())
        {
            throw runtime_error("No seed identifier passed, and no active seed selected, please select an active seed or pass a valid identifier.");
        }
        return pwallet->getActiveSeed();
    }
    
    CHDSeed* foundSeed = NULL;
    if (pwallet->mapSeeds.find(strSeedUUID) != pwallet->mapSeeds.end())
    {
        foundSeed = pwallet->mapSeeds[strSeedUUID];
    }
    
    if (!foundSeed)
        throw JSONRPCError(RPC_WALLET_INVALID_ACCOUNT_NAME, "Not a valid seed UUID.");
    
    return foundSeed;
}

UniValue setactiveseed(const JSONRPCRequest& request)
{
    #ifdef ENABLE_WALLET
    CWallet * const pwallet = GetWalletForJSONRPCRequest(request);

    LOCK2(cs_main, pwallet ? &pwallet->cs_wallet : NULL);
    #else
    LOCK(cs_main);
    #endif
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp))
        return NullUniValue;
    
    if (request.fHelp || request.params.size() != 1)
        throw std::runtime_error(
            "setactiveseed \n"
            "\nSet the currently active seed by UUID.\n"
            "\nExamples:\n"
            + HelpExampleCli("setactiveseed", "")
            + HelpExampleRpc("setactiveseed", ""));

    
    
    if (!pwallet)
        throw runtime_error("Cannot use command without an active wallet");
    
    CHDSeed* seed = SeedFromValue(pwallet, request.params[0], false);  

    pwallet->setActiveSeed(seed);
    return seed->getUUID();
}

           
UniValue createseed(const JSONRPCRequest& request)
{
    #ifdef ENABLE_WALLET
    CWallet * const pwallet = GetWalletForJSONRPCRequest(request);

    LOCK2(cs_main, pwallet ? &pwallet->cs_wallet : NULL);
    #else
    LOCK(cs_main);
    #endif
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp))
        return NullUniValue;
    
    if (request.fHelp || request.params.size() > 1)
        throw std::runtime_error(
            "createseed \n"
            "\nCreate a new seed using random entropy.\n"
            "1. \"type\"       (string, optional default=BIP44) Type of seed to create (BIP44; BIP44NH; BIP44E; BIP32; BIP32L)\n"
            "\nThe default is correct in almost all cases, only experts should work with the other types\n"
            "\nBIP44 - This is the standard Gulden seed type that should be used in almost all cases.\n"
            "\nBIP44NH - (No Hardening) This is the same as above, however with weakened security required for \"read only\" (watch) seed capability, use this only if you understand the implications and if you want to share your seed with another read only wallet.\n"
            "\nBIP44E - This is a modified BIP44 with a different hash value, required for compatibility with some external wallets (e.g. Coinomi).\n"
            "\nBIP32 - Older HD standard that was used by our mobile wallets before 1.6.0, use this to import/recover old mobile recovery phrases.\n"
            "\nBIP32L - (Legacy) Even older HD standard that was used by our first android wallets, use this to import/recover very old mobile recovery phrases.\n"
            "\nResult:\n"
            "\nReturn the UUID of the new seed.\n"
            "\nExamples:\n"
            + HelpExampleCli("createseed", "")
            + HelpExampleRpc("createseed", ""));

    
    
    if (!pwallet)
        throw runtime_error("Cannot use command without an active wallet");
    
    EnsureWalletIsUnlocked(pwallet);
    
    CHDSeed::SeedType seedType = CHDSeed::CHDSeed::BIP44;
    if (request.params.size() > 0)
    {
        seedType = SeedTypeFromString(request.params[0].get_str());
    }
    
    CHDSeed* newSeed = pwallet->GenerateHDSeed(seedType);
    
    if(!newSeed)
        throw runtime_error("Failed to generate seed");

    return newSeed->getUUID();
}

UniValue deleteseed(const JSONRPCRequest& request)
{
    #ifdef ENABLE_WALLET
    CWallet * const pwallet = GetWalletForJSONRPCRequest(request);

    LOCK2(cs_main, pwallet ? &pwallet->cs_wallet : NULL);
    #else
    LOCK(cs_main);
    #endif
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp))
        return NullUniValue;
    
    if (request.fHelp || request.params.size() != 1)
        throw std::runtime_error(
            "deleteseed \"seed\" \n"
            "\nDelete a HD seed.\n"
            "1. \"seed\"        (string, required) The unique UUID for the seed that we want mnemonics of, or \"\" for the active seed.\n"
            "\nResult:\n"
            "\ntrue on success.\n"
            "\nExamples:\n"
            + HelpExampleCli("deleteseed", "")
            + HelpExampleRpc("deleteseed", ""));

    
    
    if (!pwallet)
        throw runtime_error("Cannot use command without an active wallet");
    
    CHDSeed* seed = SeedFromValue(pwallet, request.params[0], true);  
    
    EnsureWalletIsUnlocked(pwallet);
    
    pwallet->DeleteSeed(seed, false);

    return true;
}

UniValue importseed(const JSONRPCRequest& request)
{
    #ifdef ENABLE_WALLET
    CWallet * const pwallet = GetWalletForJSONRPCRequest(request);

    LOCK2(cs_main, pwallet ? &pwallet->cs_wallet : NULL);
    #else
    LOCK(cs_main);
    #endif
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp))
        return NullUniValue;
    
    if (request.fHelp || request.params.size() < 1 || request.params.size() > 2)
        throw std::runtime_error(
            "importseed \"mnemonic or pubkey\" \"read only\" \n"
            "\nSet the currently active seed by UUID.\n"
            "1. \"mnemonic or pubkey\"       (string) Specify the BIP44 mnemonic that will be used to generate the seed.\n"
            "\nIn the case of read only seeds a pubkey rather than a mnemonic is required.\n"
            "2. \"read only\"      (boolean, optional, default=false) Rescan the wallet for transactions\n"
            "\nResult:\n"
            "\nReturn the UUID of the new seed.\n"
            "\nExamples:\n"
            + HelpExampleCli("importseed", "")
            + HelpExampleRpc("importseed", ""));

    
    
    if (!pwallet)
        throw runtime_error("Cannot use command without an active wallet");
    
    EnsureWalletIsUnlocked(pwallet);
    
    bool fReadOnly = false;
    if (request.params.size() > 1)
        fReadOnly = request.params[1].get_bool();
    
    CHDSeed* newSeed = NULL;
    if (fReadOnly)
    {
        SecureString pubkeyString = request.params[0].get_str().c_str();
        newSeed = pwallet->ImportHDSeedFromPubkey(pubkeyString);
    }
    else
    {
        SecureString mnemonic = request.params[0].get_str().c_str();
        newSeed = pwallet->ImportHDSeed(mnemonic);
    }

    //fixme: Use a timestamp here
    // Whenever a key is imported, we need to scan the whole chain - do so now
    pwallet->nTimeFirstKey = 1;
    boost::thread t(rescanThread); // thread runs free
    
    return newSeed->getUUID();
}

UniValue listallaccounts(const JSONRPCRequest& request)
{    
    if (request.fHelp || request.params.size() > 1)
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
    
    #ifdef ENABLE_WALLET
    CWallet * const pwallet = GetWalletForJSONRPCRequest(request);

    LOCK2(cs_main, pwallet ? &pwallet->cs_wallet : NULL);
    #else
    LOCK(cs_main);
    #endif
    
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp))
        return NullUniValue;

    CHDSeed* forSeed = NULL;
    if (request.params.size() > 0)
        forSeed = SeedFromValue(pwallet, request.params[0], false);
    
    if (!pwallet)
        throw runtime_error("Cannot use command without an active wallet");
    
    UniValue allAccounts(UniValue::VARR);
    
    for (const auto& accountPair : pwallet->mapAccounts)
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
        rec.push_back(Pair("HDindex", (uint64_t) dynamic_cast<CAccountHD*>(accountPair.second)->getIndex()));
        
        allAccounts.push_back(rec);
    }
    
    return allAccounts;
}

UniValue getmnemonicfromseed(const JSONRPCRequest& request)
{
    #ifdef ENABLE_WALLET
    CWallet * const pwallet = GetWalletForJSONRPCRequest(request);

    LOCK2(cs_main, pwallet ? &pwallet->cs_wallet : NULL);
    #else
    LOCK(cs_main);
    #endif
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp))
        return NullUniValue;
    
    if (request.fHelp || request.params.size() != 1)
        throw std::runtime_error(
            "getmnemonicfromseed \"seed\" \n"
            "\nGet the mnemonic of a HD seed.\n"
            "1. \"seed\"        (string, required) The unique UUID for the seed that we want mnemonics of, or \"\" for the active seed.\n"
            "\nResult:\n"
            "\nReturn the mnemonic as a string.\n"
            "\nNote it is important to ensure that nobody gets access to this mnemonic or all funds in accounts made from the seed can be compromised.\n"
            "\nExamples:\n"
            + HelpExampleCli("getmnemonicfromseed", "")
            + HelpExampleRpc("getmnemonicfromseed", ""));

    
    if (!pwallet)
        throw runtime_error("Cannot use command without an active wallet");
    
    CHDSeed* seed = SeedFromValue(pwallet, request.params[0], true);  
    
    EnsureWalletIsUnlocked(pwallet);

    return seed->getMnemonic().c_str();
}

UniValue getreadonlyseed(const JSONRPCRequest& request)
{
    #ifdef ENABLE_WALLET
    CWallet * const pwallet = GetWalletForJSONRPCRequest(request);

    LOCK2(cs_main, pwallet ? &pwallet->cs_wallet : NULL);
    #else
    LOCK(cs_main);
    #endif
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp))
        return NullUniValue;
    
    if (request.fHelp || request.params.size() != 1)
        throw std::runtime_error(
            "getreadonlyseed \"seed\" \n"
            "\nGet the public key of an HD seed, this can be used to import the seed as a read only seed in another wallet.\n"
            "1. \"seed\"        (string, required) The unique UUID for the seed that we want the public key of, or \"\" for the active seed.\n"
            "\nNote the seed must be a 'non hardened' BIP44NH seed and not a regular seed.\n"
            "\nResult:\n"
            "\nReturn the public key as a string.\n"
            "\nNote it is important to be careful with and protect access to this public key as if it is compromised it can weaken security in cases where private keys are also compromised.\n"
            "\nExamples:\n"
            + HelpExampleCli("getreadonlyseed", "")
            + HelpExampleRpc("getreadonlyseed", ""));
    
    if (!pwallet)
        throw runtime_error("Cannot use command without an active wallet");
    
    CHDSeed* seed = SeedFromValue(pwallet, request.params[0], true);
    
    if (seed->m_type != CHDSeed::SeedType::BIP44NoHardening)
        throw runtime_error("Can only use command with a non-hardened BIP44 seed");
    
    EnsureWalletIsUnlocked(pwallet);

    return seed->getPubkey().c_str();
}

UniValue listseeds(const JSONRPCRequest& request)
{
    #ifdef ENABLE_WALLET
    CWallet * const pwallet = GetWalletForJSONRPCRequest(request);

    LOCK2(cs_main, pwallet ? &pwallet->cs_wallet : NULL);
    #else
    LOCK(cs_main);
    #endif
    
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp))
        return NullUniValue;
    
    if (request.fHelp || request.params.size() != 0)
        throw std::runtime_error(
            "listseeds \n"
            "\nReturn the UUID for all wallet seeds.\n"
            "\nExamples:\n"
            + HelpExampleCli("listseeds", "")
            + HelpExampleRpc("listseeds", ""));
    
    if (!pwallet)
        throw runtime_error("Cannot use command without an active wallet");
    
    UniValue AllSeeds(UniValue::VARR);
    
    for (const auto& seedPair : pwallet->mapSeeds)
    {
        UniValue rec(UniValue::VOBJ);
        rec.push_back(Pair("UUID", seedPair.first));
        rec.push_back(Pair("type", StringFromSeedType(seedPair.second)));
        if (seedPair.second->IsReadOnly())
            rec.push_back(Pair("readonly", "true"));
        AllSeeds.push_back(rec);
    }
    
    return AllSeeds;
}


static const CRPCCommand commands[] =
{ //  category              name                      actor (function)         okSafeMode
  //  --------------------- ------------------------  -----------------------  ----------
    { "mining",             "gethashps",              &gethashps,              true,    {} },
    { "mining",             "sethashlimit",           &sethashlimit,           true,    {"limit"} },

    { "developer",          "dumpblockgaps",          &dumpblockgaps,          true,    {"startheight", "count"} },
    { "developer",          "dumpdiffarray",          &dumpdiffarray,          true,    {"height"} },
    
    { "accounts",           "changeaccountname",      &changeaccountname,      true,    {"account", "name"} },
    { "accounts",           "createaccount",          &createaccount,          true,    {"name", "type"} },
    { "accounts",           "deleteaccount",          &deleteaccount,          true,    {"accout", "force"} },
    { "accounts",           "getactiveaccount",       &getactiveaccount,       true,    {} },
    { "accounts",           "getreadonlyaccount",     &getreadonlyaccount,     true,    {"account"} },
    { "accounts",           "importreadonlyaccount",  &importreadonlyaccount,  true,    {"name", "encodedkey"} },
    { "accounts",           "listaccounts",           &listallaccounts,        true,    {"seed"} },
    { "accounts",           "setactiveaccount",       &setactiveaccount,       true,    {"account"} },
    
    { "mnemonics",          "createseed",             &createseed,             true,    {"type"} },
    { "mnemonics",          "deleteseed",             &deleteseed,             true,    {"seed"} },
    { "mnemonics",          "getactiveseed",          &getactiveseed,          true,    {} },
    { "mnemonics",          "getmnemonicfromseed",    &getmnemonicfromseed,    true,    {"seed"} },
    { "mnemonics",          "getreadonlyseed",        &getreadonlyseed,        true,    {"seed"} },
    { "mnemonics",          "setactiveseed",          &setactiveseed,          true,    {"seed"} },
    { "mnemonics",          "importseed",             &importseed,             true,    {"mnemonic or enckey", "read only"} },
    { "mnemonics",          "listseeds",              &listseeds,              true,    {} },
};

void RegisterGuldenRPCCommands(CRPCTable &tableRPC)
{
    for (unsigned int vcidx = 0; vcidx < ARRAYLEN(commands); vcidx++)
        tableRPC.appendCommand(commands[vcidx].name, &commands[vcidx]);
}

#endif


