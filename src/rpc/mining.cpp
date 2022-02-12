// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// File contains modifications by: The Gulden developers
// All modifications:
// Copyright (c) 2016-2020 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#include "base58.h"
#include "amount.h"
#include "chain.h"
#include "chainparams.h"
#include "consensus/consensus.h"
#include "consensus/params.h"
#include "consensus/validation.h"
#include "validation/validation.h"
#include "validation/validationinterface.h"
#include "validation/versionbitsvalidation.h"
#include "core_io.h"
#include "init.h"
#include "versionbits.h"
#include "generation/miner.h"
#include "net.h"
#include "policy/fees.h"
#include "pow/pow.h"
#include "rpc/blockchain.h"
#include "rpc/server.h"
#include "txmempool.h"
#include "util.h"
#include "utilstrencodings.h"
#include "arith_uint256.h"
#include "warnings.h"
#include "witnessutil.h"
#include <compat/sys.h>

#include <memory>
#include <stdint.h>

#include <univalue.h>
#ifdef ENABLE_WALLET
#include "wallet/account.h"
#include "wallet/wallet.h"
#include "wallet/rpcwallet.h"
#endif
#include "generation/generation.h"
#include "script/script.h"
#include <rpc/accounts.h>
#include <validation/witnessvalidation.h>

#include <boost/algorithm/string/predicate.hpp> // for ends_with()
#include <boost/uuid/uuid.hpp>

/**
 * Return average network hashes per second based on the last 'lookup' blocks,
 * or from the last difficulty change if 'lookup' is nonpositive.
 * If 'height' is nonnegative, compute the estimate at the time when a given block was found.
 */
static UniValue GetNetworkHashPS(uint32_t lookup, int height)
{
    CBlockIndex *pb = chainActive.Tip();

    if (height >= 0 && height < chainActive.Height())
        pb = chainActive[height];

    if (pb == NULL || !pb->nHeight)
        return 0;

    // If lookup is larger than chain, then set it to chain length.
    if (lookup > (uint32_t)pb->nHeight)
        lookup = (uint32_t)pb->nHeight;
    
    bool sigmaActive=false;
    if (pb->nTime >= defaultSigmaSettings.activationDate)
    {
        sigmaActive = true;
    }
        

    CBlockIndex *pb0 = pb;
    int64_t minTime = pb0->GetBlockTime();
    int64_t maxTime = minTime;
    uint64_t count = 0;
    arith_uint256 workDiff = 0;
    for (uint32_t i = 0; i < lookup; i++)
    {
        if (sigmaActive && pb0->pprev->nTime < defaultSigmaSettings.activationDate)
            break;
        workDiff += GetBlockProof(*pb0);
        count++;
        pb0 = pb0->pprev;
        int64_t time = pb0->GetBlockTime();
        minTime = std::min(time, minTime);
        maxTime = std::max(time, maxTime);
    }

    // In case there's a situation where minTime == maxTime, we don't want a divide by zero exception.
    if (minTime == maxTime)
        return 0;


    int64_t timeDiff = maxTime - minTime;

    if (sigmaActive)
    {
        // SIGMA: Not 100% clear that this is the best we can do or if we can approximate a better estimate, this seems to be a reasonable approximation for now.
        // What we report to the user as 'hashes' are actually 'half hashes' (see sigma_bench to understand the distinction)
        // So to match up we have to try approximate half hashes here as well instead of hashes.
        // We can achieve this by squaring the individual chain works.
        workDiff = workDiff/count;
        workDiff = workDiff*workDiff;
        workDiff *= count;
    }
    return (workDiff.getdouble() / timeDiff);
}

static UniValue getnetworkhashps(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() > 2)
        throw std::runtime_error(
            "getnetworkhashps ( nblocks height )\n"
            "\nReturns the estimated network hashes per second based on the last n blocks.\n"
            "Pass in [blocks] to override # of blocks.\n"
            "Pass in [height] to estimate the network speed at the time when a certain block was found.\n"
            "\nArguments:\n"
            "1. nblocks     (numeric, optional, default=120) The number of blocks, or -1 for blocks since last difficulty change.\n"
            "2. height      (numeric, optional, default=-1) To estimate at the time of the given height.\n"
            "\nResult:\n"
            "x             (numeric) Hashes per second estimated\n"
            "\nExamples:\n"
            + HelpExampleCli("getnetworkhashps", "")
            + HelpExampleRpc("getnetworkhashps", "")
       );

    LOCK(cs_main);
    int64_t lookup = request.params.size() > 0 ? request.params[0].get_int() : 120;
    if (lookup < 0)
        lookup = 120;

    return GetNetworkHashPS(lookup, request.params.size() > 1 ? request.params[1].get_int() : -1);
}

extern void TryPopulateAndSignWitnessBlock(CBlockIndex* candidateIter, CChainParams& chainparams, Consensus::Params& consensusParams, CGetWitnessInfo witnessInfo, std::shared_ptr<CBlock> pWitnessBlock, std::map<boost::uuids::uuid, std::shared_ptr<CReserveKeyOrScript>>& reserveKeys, bool& encounteredError, bool& signedBlock);

static UniValue generateBlocks(std::shared_ptr<CReserveKeyOrScript> coinbaseScript, int nGenerate, uint64_t nMaxTries, bool keepScript)
{
    static const int nInnerLoopCount = 0x10000;
    int nHeightEnd = 0;
    int nHeight = 0;

    std::map<boost::uuids::uuid, std::shared_ptr<CReserveKeyOrScript>> reservedKeys;
    
    {   // Don't keep cs_main locked
        LOCK(cs_main);
        nHeight = chainActive.Height();
        nHeightEnd = nHeight+nGenerate;
    }
    unsigned int nExtraNonce = 0;
    UniValue blockHashes(UniValue::VARR);
    while (nHeight < nHeightEnd)
    {
        std::unique_ptr<CBlockTemplate> pblocktemplate(BlockAssembler(Params()).CreateNewBlock(chainActive.Tip(), coinbaseScript));
        if (!pblocktemplate.get())
            throw JSONRPCError(RPC_INTERNAL_ERROR, "Couldn't create new block");
        CBlock *pblock = &pblocktemplate->block;
        {
            LOCK(cs_main);
            IncrementExtraNonce(pblock, chainActive.Tip(), nExtraNonce);
        }
        while (nMaxTries > 0 && pblock->nNonce < nInnerLoopCount && !CheckProofOfWork(pblock, Params().GetConsensus())) {
            ++pblock->nNonce;
            --nMaxTries;
        }
        if (nMaxTries == 0) {
            break;
        }
        if (pblock->nNonce == nInnerLoopCount) {
            continue;
        }

        CChainParams chainparams = Params();
        Consensus::Params consensus = chainparams.GetConsensus();
        CGetWitnessInfo witnessInfo;
        if (Params().IsRegtest())
        {
            if (!GetWitness(chainActive, chainparams, nullptr, chainActive.Tip(), *pblock, witnessInfo))
            {
                throw JSONRPCError(RPC_INTERNAL_ERROR, "Unable to get witness for block");
            }
        }
            
        std::shared_ptr<CBlock> shared_pblock = std::make_shared<CBlock>(*pblock);
        if (!ProcessNewBlock(Params(), shared_pblock, true, NULL, false, true))
            throw JSONRPCError(RPC_INTERNAL_ERROR, "ProcessNewBlock, PoW block not accepted");
        
        // Perform witnessing
        if (Params().IsRegtest())
        {   
            bool encounteredError=false;
            bool signedBlock=false;
            TryPopulateAndSignWitnessBlock(chainActive.Tip(), chainparams, consensus, witnessInfo, shared_pblock, reservedKeys, encounteredError, signedBlock);
            if (encounteredError || !signedBlock)
                throw JSONRPCError(RPC_INTERNAL_ERROR, "Unable to witness block");
            
            if (!ProcessNewBlock(Params(), shared_pblock, true, NULL, false, true))
                throw JSONRPCError(RPC_INTERNAL_ERROR, "ProcessNewBlock, witness block not accepted");
        }
        
        ++nHeight;
        blockHashes.push_back(pblock->GetHashLegacy().GetHex());

        //mark script as important because it was used at least for one coinbase output if the script came from the wallet
        if (keepScript)
        {
            coinbaseScript->KeepScript();
        }
    }
    return blockHashes;
}

void InitRPCMining()
{
}

void ShutdownRPCMining()
{
}

static UniValue getgenerate(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 0)
        throw std::runtime_error(
            "getgenerate\n"
            "\nReturn if the server is set to generate coins or not. The default is false.\n"
            "It is set with the command line argument -gen (or " + std::string(DEFAULT_CONF_FILENAME) + " setting gen)\n"
            "It can also be set with the setgenerate call.\n"
            "\nResult\n"
            "true|false      (boolean) If the server is set to generate coins or not\n"
            "\nExamples:\n"
            + HelpExampleCli("getgenerate", "")
            + HelpExampleRpc("getgenerate", "")
        );

    LOCK(cs_main);
    return PoWGenerationIsActive();
}

static UniValue generate(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() < 1 || request.params.size() > 3)
        throw std::runtime_error(
            "generate num_blocks ( max_tries )\n"
            "\ngenerate up to n blocks immediately (before the RPC call returns)\n"
            "\nArguments:\n"
            "1. nblocks      (numeric, required) How many blocks are generated immediately.\n"
            "2. maxtries     (numeric, optional) How many iterations to try (default = 1000000).\n"
            "3. account      (string, optional) The UUID or unique label of the account.\n"
            "\nResult:\n"
            "[ blockhashes ]     (array) hashes of blocks generated\n"
            "\nExamples:\n"
            "\nGenerate 11 blocks\n"
            + HelpExampleCli("generate", "11")
        );

    if (!Params().IsRegtest())
        throw std::runtime_error("generate command only for regtest; for mainnet/testnet use setgenerate");

    int nGenerate = request.params[0].get_int();
    uint64_t nMaxTries = 1000000;
    if (request.params.size() > 1) {
        nMaxTries = request.params[1].get_int();
    }

    CAccount* forAccount = nullptr;

#ifdef ENABLE_WALLET
    if (request.params.size() > 2)
        forAccount = AccountFromValue(pactiveWallet, request.params[2], false);
    else {
        if (!pactiveWallet->activeAccount)
            throw std::runtime_error("No active account selected, first select an active account.");
        forAccount = pactiveWallet->activeAccount;
    }

    if (forAccount->IsPoW2Witness())
        throw std::runtime_error("Witness account selected, first select a regular account as the active account or specifiy a regular account.");
#endif

    std::shared_ptr<CReserveKeyOrScript> coinbaseScript;
    GetMainSignals().ScriptForMining(coinbaseScript, forAccount);

    // If the keypool is exhausted, no script is returned at all.  Catch this.
    if (!coinbaseScript)
        throw JSONRPCError(RPC_WALLET_KEYPOOL_RAN_OUT, "Error: Keypool ran out, please call keypoolrefill first");

    //throw an error if no script was provided
    if (coinbaseScript->reserveScript.empty())
        throw JSONRPCError(RPC_INTERNAL_ERROR, "No coinbase script available; a wallet is required");

    return generateBlocks(coinbaseScript, nGenerate, nMaxTries, true);
}

static UniValue setgenerate(const JSONRPCRequest& request)
{
    #ifdef ENABLE_WALLET
    CWallet * const pwallet = GetWalletForJSONRPCRequest(request);
    LOCK2(cs_main, pwallet ? &pwallet->cs_wallet : NULL);
    #else
    LOCK(cs_main);
    #endif
    
    if (request.fHelp || request.params.size() < 1 || request.params.size() > 5)
        throw std::runtime_error(
            "setgenerate generate ( gen_proc_limit )\n"
            "\nSet 'generate' true or false to turn generation on or off.\n"
            "Generation is limited to 'gen_proc_limit' processors, -1 is unlimited.\n"
            "Arena setup is limited to 'gen_arena_proc_limit' processors, -1 is unlimited, takes the value of 'gen_proc_limit' if not explicitely set.\n"
            "See the getgenerate call for the current setting.\n"
            "\nArguments:\n"
            "1. generate             (boolean, required) Set to true to turn on generation, off to turn off.\n"
            "2. gen_proc_limit       (numeric, optional) Set the processor limit for when generation is on. Can be -1 for unlimited.\n"
            "3. gen_arena_proc_limit (numeric, optional) Set the processor limit for when generation is on. Can be -1 for unlimited.\n"
            "4. gen_memory_limit     (string, optional) How much system memory to use, specify a letter G/M/K/B to determine size e.g. 524288K (Kilobytes), 512M (Megabytes), 3G (Gigabytes).\n"
            "5. account              (string, optional) The UUID or unique label of the account.\n"
            "\nExamples:\n"
            "\nSet the generation on with a limit of one processor\n"
            + HelpExampleCli("setgenerate", "true 1") +
            "\nCheck the setting\n"
            + HelpExampleCli("getgenerate", "") +
            "\nTurn off generation\n"
            + HelpExampleCli("setgenerate", "false") +
            "\nUsing json rpc\n"
            + HelpExampleRpc("setgenerate", "true, 1")
        );

    if (Params().MineBlocksOnDemand())
        throw JSONRPCError(RPC_METHOD_NOT_FOUND, "Use the generate method instead of setgenerate on this network");

    #ifdef ENABLE_WALLET
    if (!pwallet)
    {
        throw std::runtime_error("Cannot use command without an active wallet");
    }
    
    bool fGenerate = true;
    if (request.params.size() > 0)
    {
        fGenerate = request.params[0].get_bool();
    }
    
    int nGenProcLimit = GetArg("-genproclimit", DEFAULT_GENERATE_THREADS);
    if (request.params.size() > 1)
    {
        nGenProcLimit = request.params[1].get_int();
        if (nGenProcLimit == 0)
        {
            fGenerate = false;
        }
    }
    
    int nGenArenaProcLimit = GetArg("-genarenaproclimit", DEFAULT_GENERATE_THREADS);
    if (request.params.size() > 2)
    {
        nGenArenaProcLimit = request.params[2].get_int();
        if (nGenArenaProcLimit == 0)
        {
            fGenerate = false;
        }
    }
    
    if (!fGenerate)
    {
        std::thread([=]
        {
            PoWStopGeneration();
        }).detach();
        return "Block generation disabled.";
    }

    
    CAccount* forAccount = nullptr;
    if (request.params.size() > 4 && request.params[4].get_str().length()>0)
    {
        forAccount = AccountFromValue(pactiveWallet, request.params[4], false);
        if (forAccount && forAccount->IsPoW2Witness())
        {
            throw std::runtime_error("Witness account selected, first select a regular account as the active account or specify a regular account.");
        }
    }
    
    std::string overrideAccountAddress;
    if (!forAccount)
    {
        for (const auto& [accountUUID, account] : pactiveWallet->mapAccounts)
        {
            (unused) accountUUID;
            if (account->IsMiningAccount() && account->m_State == AccountState::Normal)
            {
                forAccount = account;
                break;
            }
        }
        CWalletDB(*pactiveWallet->dbw).ReadMiningAddressString(overrideAccountAddress);
    }
    
    if (!forAccount)
    {
        throw std::runtime_error("No mining account present in wallet. Create a mining account using `createminingaccount` or pass an explicit target account to yout `setgenerate` call.");
    }

    // Try to avoid swap by never using more than sysmem-1gb or on machines with only 1gb of memory sysmem-512mb.
    uint64_t systemMemory = systemPhysicalMemoryInBytes();
    if (systemMemory > 1 * 1024 * 1024 * 1024)
    {
        systemMemory -= 1 * 1024 * 1024 * 1024;
    }
    else if (systemMemory > 512 * 1024 * 1024)
    {
        systemMemory -= 512 * 1024 * 1024;
    }
    else if (systemMemory > 256 * 1024 * 1024)
    {
        systemMemory -= 256 * 1024 * 1024;
    }
    else if (systemMemory > 128 * 1024 * 1024)
    {
        systemMemory -= 128 * 1024 * 1024;
    }

    // Allow user to override default memory selection.
    uint64_t nGenMemoryLimitBytes = std::min(systemMemory, defaultSigmaSettings.arenaSizeKb*1024);
    if (request.params.size() > 3)
    {
        std::string sMemLimit = request.params[3].get_str();
        nGenMemoryLimitBytes = GetMemLimitInBytesFromFormattedStringSpecifier(sMemLimit);
    }
    
    // Normalise for SIGMA arena expectations (arena size must be a multiple of 16mb)
    normaliseBufferSize(nGenMemoryLimitBytes);
    
    SoftSetArg("-genproclimit", itostr(nGenProcLimit));
    SoftSetArg("-genarenaproclimit", itostr(nGenArenaProcLimit));
    SoftSetArg("-genmemlimit", i64tostr(nGenMemoryLimitBytes/1024));
    std::thread([=]
    {
        try
        {
            PoWGenerateBlocks(true, nGenProcLimit, nGenArenaProcLimit, nGenMemoryLimitBytes/1024, Params(), forAccount, overrideAccountAddress);
        }
        catch(...)
        {
        }
    }).detach();
    
    if (overrideAccountAddress.length() > 0)
    {
        return strprintf("Block generation enabled into account [%s] using target address [%s], thread limit: [%d threads], memory: [%d Mb].", pwallet->mapAccountLabels[forAccount->getUUID()], overrideAccountAddress ,nGenProcLimit, nGenMemoryLimitBytes/1024/1024);
    }
    else
    {
        return strprintf("Block generation enabled into account [%s], thread limit: [%d threads], memory: [%d Mb].", pwallet->mapAccountLabels[forAccount->getUUID()] ,nGenProcLimit, nGenMemoryLimitBytes/1024/1024);
    }
    #else
    throw std::runtime_error("Cannot use command without an active wallet");
    return nullptr;
    #endif
}

static UniValue generatetoaddress(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() < 2 || request.params.size() > 3)
        throw std::runtime_error(
            "generatetoaddress num_blocks address (max_tries)\n"
            "\nGenerate blocks immediately to a specified address (before the RPC call returns)\n"
            "\nArguments:\n"
            "1. nblocks      (numeric, required) How many blocks are generated immediately.\n"
            "2. address      (string, required) The address to send the newly generated " GLOBAL_APPNAME " to.\n"
            "3. maxtries     (numeric, optional) How many iterations to try (default = 1000000).\n"
            "\nResult:\n"
            "[ blockhashes ]     (array) hashes of blocks generated\n"
            "\nExamples:\n"
            "\nGenerate 11 blocks to myaddress\n"
            + HelpExampleCli("generatetoaddress", "11 \"myaddress\"")
        );

    if (!Params().IsRegtest() && !Params().IsRegtestLegacy())
        throw std::runtime_error("generatetoaddress command only for regtest; for mainnet/testnet use setgenerate");

    int nGenerate = request.params[0].get_int();
    uint64_t nMaxTries = 1000000;
    if (request.params.size() > 2) {
        nMaxTries = request.params[2].get_int();
    }

    CNativeAddress address(request.params[1].get_str());
    if (!address.IsValid())
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Error: Invalid address");

    std::shared_ptr<CReserveKeyOrScript> coinbaseScript = std::make_shared<CReserveKeyOrScript>();
    coinbaseScript->reserveScript = GetScriptForDestination(address.Get());

    return generateBlocks(coinbaseScript, nGenerate, nMaxTries, false);
}

static UniValue getmininginfo(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 0)
        throw std::runtime_error(
            "getmininginfo\n"
            "\nReturns a json object containing information about block generation."
            "\nResult:\n"
            "{\n"
            "  \"blocks\": nnn,             (numeric) The current block\n"
            "  \"currentblocksize\": nnn,   (numeric) The last block size\n"
            "  \"currentblockweight\": nnn, (numeric) The last block weight\n"
            "  \"currentblocktx\": nnn,     (numeric) The last block transaction\n"
            "  \"difficulty\": xxx.xxxxx    (numeric) The current difficulty\n"
            "  \"errors\": \"...\"            (string) Current errors\n"
            "  \"networkhashps\": nnn,      (numeric) The network hashes per second\n"
            "  \"generate\": true|false     (boolean) If the generation is on or off (see getgenerate or setgenerate calls)\n"
            "  \"genproclimit\": n          (numeric) The processor limit for generation. -1 if no generation. (see getgenerate or setgenerate calls)\n"
            "  \"genmemlimit\": n           (numeric) The memory limit for generation; In Kilobytes. (see getgenerate or setgenerate calls)\n"
            "  \"pooledtx\": n              (numeric) The size of the mempool\n"
            "  \"chain\": \"xxxx\",           (string) current network name as defined in BIP70 (main, test, regtest)\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("getmininginfo", "")
            + HelpExampleRpc("getmininginfo", "")
        );


    LOCK(cs_main);

    UniValue obj(UniValue::VOBJ);
    obj.push_back(Pair("blocks",                           (int)chainActive.Height()));
    obj.push_back(Pair("currentblocksize",                 (uint64_t)nLastBlockSize));
    obj.push_back(Pair("currentblockweight",               (uint64_t)nLastBlockWeight));
    obj.push_back(Pair("currentblocktx",                   (uint64_t)nLastBlockTx));
    obj.push_back(Pair("difficulty",                       (double)GetDifficulty()));
    obj.push_back(Pair("errors",                           GetWarnings("statusbar")));
    obj.push_back(Pair("genproclimit",                     (int)GetArg("-genproclimit", DEFAULT_GENERATE_THREADS)));
    obj.push_back(Pair("genmemlimit",                      (uint64_t)GetArg("-genmemlimit", DEFAULT_GENERATE_THREADS)));
    obj.push_back(Pair("networkhashps",                    getnetworkhashps(request)));
    obj.push_back(Pair("pooledtx",                         (uint64_t)mempool.size()));
    obj.push_back(Pair("chain",                            Params().NetworkIDString()));
    obj.push_back(Pair("selected_shavite_implementation",  selectedAlgorithmName(gSelShavite)));
    obj.push_back(Pair("selected_argon_implementation",    selectedAlgorithmName(gSelArgon)));
    obj.push_back(Pair("selected_echo_implementation",     selectedAlgorithmName(gSelEcho)));
    return obj;
}


// NOTE: Unlike wallet RPC (which use NLG values), mining RPCs follow GBT (BIP 22) in using satoshi amounts
static UniValue prioritisetransaction(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 3)
        throw std::runtime_error(
            "prioritisetransaction <txid> <dummy_value> <fee_delta>\n"
            "Accepts the transaction into generated blocks at a higher (or lower) priority\n"
            "\nArguments:\n"
            "1. \"txid\"         (string, required) The transaction id.\n"
            "2. dummy_value    (numeric, optional) API-Compatibility for previous API. Must be zero or null.\n"
            "                  DEPRECATED. For forward compatibility use named arguments and omit this parameter.\n"
            "3. fee_delta      (numeric, required) The fee value (in satoshis) to add (or subtract, if negative).\n"
            "                  The fee is not actually paid, only the algorithm for selecting transactions into a block\n"
            "                  considers the transaction as it would have paid a higher (or lower) fee.\n"
            "\nResult:\n"
            "true              (boolean) Returns true\n"
            "\nExamples:\n"
            + HelpExampleCli("prioritisetransaction", "\"txid\" 0.0 10000")
            + HelpExampleRpc("prioritisetransaction", "\"txid\", 0.0, 10000")
        );

    LOCK(cs_main);

    uint256 hash = ParseHashStr(request.params[0].get_str(), "txid");
    CAmount nAmount = request.params[2].get_int64();

    if (!(request.params[1].isNull() || request.params[1].get_real() == 0)) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Priority is no longer supported, dummy argument to prioritisetransaction must be 0.");
    }

    mempool.PrioritiseTransaction(hash, nAmount);
    return true;
}


// NOTE: Assumes a conclusive result; if result is inconclusive, it must be handled by caller
static UniValue BIP22ValidationResult(const CValidationState& state)
{
    if (state.IsValid())
        return NullUniValue;

    std::string strRejectReason = state.GetRejectReason();
    if (state.IsError())
        throw JSONRPCError(RPC_VERIFY_ERROR, strRejectReason);
    if (state.IsInvalid())
    {
        if (strRejectReason.empty())
            return "rejected";
        return strRejectReason;
    }
    // Should be impossible
    return "valid?";
}

class submitblock_StateCatcher : public CValidationInterface
{
public:
    uint256 hash;
    bool found;
    CValidationState state;

    submitblock_StateCatcher(const uint256 &hashIn) : hash(hashIn), found(false), state() {}

protected:
    void BlockChecked(const CBlock& block, const CValidationState& stateIn) override {
        if (block.GetHashPoW2() != hash)
            return;
        found = true;
        state = stateIn;
    }
};

static UniValue submitblock(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() < 1 || request.params.size() > 2) {
        throw std::runtime_error(
            "submitblock \"hexdata\" ( \"jsonparametersobject\" )\n"
            "\nAttempts to submit new block to network.\n"
            "The 'jsonparametersobject' parameter is currently ignored.\n"
            "See https://en.bitcoin.it/wiki/BIP_0022 for full specification.\n"

            "\nArguments\n"
            "1. \"hexdata\"        (string, required) the hex-encoded block data to submit\n"
            "2. \"parameters\"     (string, optional) object of optional parameters\n"
            "    {\n"
            "      \"workid\" : \"id\"    (string, optional) if the server provided a workid, it MUST be included with submissions\n"
            "    }\n"
            "\nResult:\n"
            "\nExamples:\n"
            + HelpExampleCli("submitblock", "\"mydata\"")
            + HelpExampleRpc("submitblock", "\"mydata\"")
        );
    }

    std::shared_ptr<CBlock> blockptr = std::make_shared<CBlock>();
    CBlock& block = *blockptr;
    if (!DecodeHexBlk(block, request.params[0].get_str())) {
        throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "Block decode failed");
    }

    if (block.vtx.empty() || !block.vtx[0]->IsCoinBase()) {
        throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "Block does not start with a coinbase");
    }

    uint256 hash = block.GetHashPoW2();
    bool fBlockPresent = false;
    {
        LOCK(cs_main);
        BlockMap::iterator mi = mapBlockIndex.find(hash);
        if (mi != mapBlockIndex.end()) {
            CBlockIndex *pindex = mi->second;
            if (pindex->IsValid(BLOCK_VALID_SCRIPTS)) {
                return "duplicate";
            }
            if (pindex->nStatus & BLOCK_FAILED_MASK) {
                return "duplicate-invalid";
            }
            // Otherwise, we might only have the header - process the block before returning
            fBlockPresent = true;
        }
    }

    submitblock_StateCatcher sc(block.GetHashPoW2());
    RegisterValidationInterface(&sc);
    bool fAccepted = ProcessNewBlock(Params(), blockptr, true, NULL, false, true);
    UnregisterValidationInterface(&sc);


    if (fBlockPresent) {
        if (fAccepted && !sc.found) {
            return "duplicate-inconclusive";
        }
        return "duplicate";
    }
    if (!fAccepted)
        return "invalid";
    if (!sc.found) {
        return "inconclusive";
    }
    return BIP22ValidationResult(sc.state);
}

static UniValue submitheader(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
    {
        throw std::runtime_error(
            "submitheader \"hexdata\"\n"
            "\nAttempts to submit new header to network.\n"
            "\nArguments\n"
            "1. \"hexdata\"        (string, required) the hex-encoded block data to submit\n"
            "\nResult:\n"
            "\nExamples:\n"
            + HelpExampleCli("submitblock", "\"mydata\"")
            + HelpExampleRpc("submitblock", "\"mydata\"")
        );
    }

    std::shared_ptr<CBlock> blockptr = std::make_shared<CBlock>();
    CBlock& block = *blockptr;
    if (!DecodeHexBlk(block, request.params[0].get_str()+"00"))
    {
        throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "Header decode failed");
    }

    UniValue result(UniValue::VOBJ);
    result.push_back(Pair("hash", block.GetHashPoW2().ToString()));

    const CBlockIndex *pindex = NULL;
    CValidationState state;
    if (!ProcessNewBlockHeaders( {block.GetBlockHeader()}, state, Params(), &pindex))
    {
        result.push_back(Pair("status", "failed"));        
    }
    else
    {
        result.push_back(Pair("status", "success"));
    }
    return result;
}

static UniValue estimatefee(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw std::runtime_error(
            "estimatefee num_blocks\n"
            "\nDEPRECATED. Please use estimatesmartfee for more intelligent estimates."
            "\nEstimates the approximate fee per kilobyte needed for a transaction to begin\n"
            "confirmation within num_blocks blocks. Uses virtual transaction size of transaction\n"
            "as defined in BIP 141 (witness data is discounted).\n"
            "\nArguments:\n"
            "1. num_blocks  (numeric, required)\n"
            "\nResult:\n"
            "n              (numeric) estimated fee-per-kilobyte\n"
            "\n"
            "A negative value is returned if not enough transactions and blocks\n"
            "have been observed to make an estimate.\n"
            "-1 is always returned for num_blocks == 1 as it is impossible to calculate\n"
            "a fee that is high enough to get reliably included in the next block.\n"
            "\nExample:\n"
            + HelpExampleCli("estimatefee", "6")
            );

    RPCTypeCheck(request.params, {UniValue::VNUM});

    int nBlocks = request.params[0].get_int();
    if (nBlocks < 1)
        nBlocks = 1;

    CFeeRate feeRate = ::feeEstimator.estimateFee(nBlocks);
    if (feeRate == CFeeRate(0))
        return -1.0;

    return ValueFromAmount(feeRate.GetFeePerK());
}

/* Used to determine type of fee estimation requested */
enum class FeeEstimateMode {
    UNSET,        //!< Use default settings based on other criteria
    ECONOMICAL,   //!< Force estimateSmartFee to use non-conservative estimates
    CONSERVATIVE, //!< Force estimateSmartFee to use conservative estimates
};

bool FeeModeFromString(const std::string& mode_string, FeeEstimateMode& fee_estimate_mode)
{
    static const std::map<std::string, FeeEstimateMode> fee_modes = {
        {"UNSET", FeeEstimateMode::UNSET},
        {"ECONOMICAL", FeeEstimateMode::ECONOMICAL},
        {"CONSERVATIVE", FeeEstimateMode::CONSERVATIVE},
    };
    auto mode = fee_modes.find(mode_string);

    if (mode == fee_modes.end()) return false;

    fee_estimate_mode = mode->second;
    return true;
}




static UniValue estimatesmartfee(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() < 1 || request.params.size() > 2)
        throw std::runtime_error(
            "estimatesmartfee num_blocks (conservative)\n"
            "\nEstimates the approximate fee per kilobyte needed for a transaction to begin\n"
            "confirmation within num_blocks blocks if possible and return the number of blocks\n"
            "for which the estimate is valid. Uses virtual transaction size as defined\n"
            "in BIP 141 (witness data is discounted).\n"
            "\nArguments:\n"
            "1. num_blocks    (numeric)\n"
            "2. conservative  (String, optional) Whether to return a more conservative estimate which\n"
            "                 also satisfies a longer history. A conservative estimate potentially returns a higher\n"
            "                 feerate and is more likely to be sufficient for the desired target, but is not as\n"
            "                 responsive to short term drops in the prevailing fee market\n"
            "\nResult:\n"
            "{\n"
            "  \"feerate\" : x.x,     (numeric) estimate fee-per-kilobyte (in " GLOBAL_COIN_CODE ")\n"
            "  \"blocks\" : n         (numeric) block number where estimate was found\n"
            "}\n"
            "\n"
            "A negative value is returned if not enough transactions and blocks\n"
            "have been observed to make an estimate for any number of blocks.\n"
            "However it will not return a value below the mempool reject fee.\n"
            "\nExample:\n"
            + HelpExampleCli("estimatesmartfee", "6")
            );

   
    RPCTypeCheck(request.params, {UniValue::VNUM, UniValue::VSTR});
    RPCTypeCheckArgument(request.params[0], UniValue::VNUM);
    //unsigned int max_target = ::feeEstimator.HighestTargetTracked(FeeEstimateHorizon::LONG_HALFLIFE);
    //unsigned int conf_target = ParseConfirmTarget(request.params[0], max_target);
    bool conservative = true;
    if (!request.params[1].isNull())
    {
        FeeEstimateMode fee_mode;
        if (!FeeModeFromString(request.params[1].get_str(), fee_mode))
        {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid estimate_mode parameter");
        }
        if (fee_mode == FeeEstimateMode::ECONOMICAL)
            conservative = false;
    }

    UniValue result(UniValue::VOBJ);
    UniValue errors(UniValue::VARR);
    //FeeCalculation feeCalc;
    int answerFound;
    //CFeeRate feeRate = ::feeEstimator.estimateSmartFee(conf_target, &feeCalc, conservative);
    CFeeRate feeRate = ::feeEstimator.estimateSmartFee(request.params[0].get_int(), &answerFound, ::mempool, conservative);
    if (feeRate.GetFeePerK() != 0)
    {
        result.pushKV("feerate", ValueFromAmount(feeRate.GetFeePerK()));
    }
    else
    {
        errors.push_back("Insufficient data or no feerate found");
        result.pushKV("errors", errors);
    }
    result.pushKV("blocks", answerFound);
    return result;
}

static UniValue estimaterawfee(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() < 1|| request.params.size() > 3)
        throw std::runtime_error(
            "estimaterawfee num_blocks (threshold horizon)\n"
            "\nWARNING: This interface is unstable and may disappear or change!\n"
            "\nWARNING: This is an advanced API call that is tightly coupled to the specific\n"
            "         implementation of fee estimation. The parameters it can be called with\n"
            "         and the results it returns will change if the internal implementation changes.\n"
            "\nEstimates the approximate fee per kilobyte needed for a transaction to begin\n"
            "confirmation within num_blocks blocks if possible. Uses virtual transaction size as defined\n"
            "in BIP 141 (witness data is discounted).\n"
            "\nArguments:\n"
            "1. num_blocks  (numeric)\n"
            "2. threshold   (numeric, optional) The proportion of transactions in a given feerate range that must have been\n"
            "               confirmed within num_blocks in order to consider those feerates as high enough and proceed to check\n"
            "               lower buckets.  Default: 0.95\n"
            "3. horizon     (numeric, optional) How long a history of estimates to consider. 0=short, 1=medium, 2=long.\n"
            "               Default: 1\n"
            "\nResult:\n"
            "{\n"
            "  \"feerate\" : x.x,        (numeric) estimate fee-per-kilobyte (in " GLOBAL_COIN_CODE ")\n"
            "  \"decay\" : x.x,          (numeric) exponential decay (per block) for historical moving average of confirmation data\n"
            "  \"scale\" : x,            (numeric) The resolution of confirmation targets at this time horizon\n"
            "  \"pass\" : {              (json object) information about the lowest range of feerates to succeed in meeting the threshold\n"
            "      \"startrange\" : x.x,     (numeric) start of feerate range\n"
            "      \"endrange\" : x.x,       (numeric) end of feerate range\n"
            "      \"withintarget\" : x.x,   (numeric) number of txs over history horizon in the feerate range that were confirmed within target\n"
            "      \"totalconfirmed\" : x.x, (numeric) number of txs over history horizon in the feerate range that were confirmed at any point\n"
            "      \"inmempool\" : x.x,      (numeric) current number of txs in mempool in the feerate range unconfirmed for at least target blocks\n"
            "      \"leftmempool\" : x.x,    (numeric) number of txs over history horizon in the feerate range that left mempool unconfirmed after target\n"
            "  }\n"
            "  \"fail\" : { ... }        (json object) information about the highest range of feerates to fail to meet the threshold\n"
            "}\n"
            "\n"
            "A negative feerate is returned if no answer can be given.\n"
            "\nExample:\n"
            + HelpExampleCli("estimaterawfee", "6 0.9 1")
            );

    RPCTypeCheck(request.params, {UniValue::VNUM, UniValue::VNUM, UniValue::VNUM}, true);
    RPCTypeCheckArgument(request.params[0], UniValue::VNUM);
    int nBlocks = request.params[0].get_int();
    double threshold = 0.95;
    if (!request.params[1].isNull())
        threshold = request.params[1].get_real();
    FeeEstimateHorizon horizon = FeeEstimateHorizon::MED_HALFLIFE;
    if (!request.params[2].isNull()) {
        int horizonInt = request.params[2].get_int();
        if (horizonInt < 0 || horizonInt > 2) {
            throw JSONRPCError(RPC_TYPE_ERROR, "Invalid horizon for fee estimates");
        } else {
            horizon = (FeeEstimateHorizon)horizonInt;
        }
    }
    UniValue result(UniValue::VOBJ);
    CFeeRate feeRate;
    EstimationResult buckets;
    feeRate = ::feeEstimator.estimateRawFee(nBlocks, threshold, horizon, &buckets);

    result.push_back(Pair("feerate", feeRate == CFeeRate(0) ? -1.0 : ValueFromAmount(feeRate.GetFeePerK())));
    result.push_back(Pair("decay", buckets.decay));
    result.push_back(Pair("scale", (int)buckets.scale));
    UniValue passbucket(UniValue::VOBJ);
    passbucket.push_back(Pair("startrange", round(buckets.pass.start)));
    passbucket.push_back(Pair("endrange", round(buckets.pass.end)));
    passbucket.push_back(Pair("withintarget", round(buckets.pass.withinTarget * 100.0) / 100.0));
    passbucket.push_back(Pair("totalconfirmed", round(buckets.pass.totalConfirmed * 100.0) / 100.0));
    passbucket.push_back(Pair("inmempool", round(buckets.pass.inMempool * 100.0) / 100.0));
    passbucket.push_back(Pair("leftmempool", round(buckets.pass.leftMempool * 100.0) / 100.0));
    result.push_back(Pair("pass", passbucket));
    UniValue failbucket(UniValue::VOBJ);
    failbucket.push_back(Pair("startrange", round(buckets.fail.start)));
    failbucket.push_back(Pair("endrange", round(buckets.fail.end)));
    failbucket.push_back(Pair("withintarget", round(buckets.fail.withinTarget * 100.0) / 100.0));
    failbucket.push_back(Pair("totalconfirmed", round(buckets.fail.totalConfirmed * 100.0) / 100.0));
    failbucket.push_back(Pair("inmempool", round(buckets.fail.inMempool * 100.0) / 100.0));
    failbucket.push_back(Pair("leftmempool", round(buckets.fail.leftMempool * 100.0) / 100.0));
    result.push_back(Pair("fail", failbucket));
    return result;
}

static const CRPCCommand commandsFull[] =
{ //  category              name                      actor (function)         okSafeMode
  //  --------------------- ------------------------  -----------------------  ----------
    { "block_generation",   "getnetworkhashps",       &getnetworkhashps,       true,  {"num_blocks","height"} },
    { "block_generation",   "getmininginfo",          &getmininginfo,          true,  {} },
    { "block_generation",   "prioritisetransaction",  &prioritisetransaction,  true,  {"txid","dummy_value","fee_delta"} },

    { "generating",         "generate",               &generate,               true,  {"num_blocks","max_tries"} },
    { "generating",         "generatetoaddress",      &generatetoaddress,      true,  {"num_blocks","address","max_tries"} },
    { "generating",         "getgenerate",            &getgenerate,            true,  {}  },
    { "generating",         "setgenerate",            &setgenerate,            true,  {"generate", "gen_proc_limit", "gen_memory_limit"}  },
};

static const CRPCCommand commandsSPV[] =
{ //  category              name                      actor (function)         okSafeMode
  //  --------------------- ------------------------  -----------------------  ----------
    { "block_generation",   "submitblock",            &submitblock,            true,  {"hexdata","parameters"} },
    { "block_generation",   "submitheader",           &submitheader,           true,  {"hexdata"} },

    { "util",               "estimatefee",            &estimatefee,            true,  {"num_blocks"} },
    { "util",               "estimatesmartfee",       &estimatesmartfee,       true,  {"num_blocks", "conservative"} },

    { "hidden",             "estimaterawfee",         &estimaterawfee,         true,  {"num_blocks", "threshold", "horizon"} },
};

void RegisterMiningRPCCommands(CRPCTable &t)
{
    if (!GetBoolArg("-spv", DEFAULT_SPV))
    {
        for (unsigned int vcidx = 0; vcidx < ARRAYLEN(commandsFull); vcidx++)
            t.appendCommand(commandsFull[vcidx].name, &commandsFull[vcidx]);
    }
    for (unsigned int vcidx = 0; vcidx < ARRAYLEN(commandsSPV); vcidx++)
        t.appendCommand(commandsSPV[vcidx].name, &commandsSPV[vcidx]);
}
