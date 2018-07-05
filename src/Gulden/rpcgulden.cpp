// Copyright (c) 2015-2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#include "rpcgulden.h"
#include "generation/miner.h"
#include "generation/witness.h"
#include <rpc/server.h>
#include <wallet/rpcwallet.h>
#include "validation/validation.h"
#include "validation/witnessvalidation.h"
#include <consensus/consensus.h>
#include <boost/assign/list_of.hpp>

#include "wallet/wallet.h"

#include <univalue.h>

#include <Gulden/util.h>
#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/accumulators/statistics/median.hpp>
#include <boost/accumulators/statistics/mean.hpp>
#include <boost/accumulators/statistics/min.hpp>
#include <boost/accumulators/statistics/max.hpp>
#include <boost/algorithm/string/predicate.hpp> // for starts_with() and ends_with()

#include "txdb.h"
#include "coins.h"
#include "wallet/coincontrol.h"
#include "wallet/wallet.h"
#include "primitives/transaction.h"

#include <Gulden/util.h>
#include "utilmoneystr.h"

#include <consensus/validation.h>
#include <consensus/consensus.h>
#include "net.h"


#ifdef ENABLE_WALLET

static UniValue gethashps(const JSONRPCRequest& request)
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

static UniValue sethashlimit(const JSONRPCRequest& request)
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

    return strprintf("Throttling hash: %d", nHashThrottle);
}

static UniValue getwitnessinfo(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() > 3)
        throw std::runtime_error(
            "getwitnessinfo \"block_specifier\" verbose mine_only\n"
            "\nReturns witness related network info for a given block."
            "\nWhen verbose is enabled returns additional statistics.\n"
            "\nArguments:\n"
            "1. \"block_specifier\"       (string, optional, default=tip) The block_specifier for which to display witness information, if empty or 'tip' the tip of the current chain is used.\n"
            "\nSpecifier can be the hash of the block; an absolute height in the blockchain or a tip~# specifier to iterate backwards from tip; for which to return witness details\n"
            "2. verbose                  (boolean, optional, default=false) Display additional verbose information.\n"
            "3. mine_only                (boolean, optional, default=false) In verbose display only show account info for accounts belonging to this wallet.\n"
            "\nResult:\n"
            "[{\n"
            "     \"pow2_phase\": n                                  (number) The number of the currently active pow2_phase.\n"
            "     \"number_of_witnesses_raw\": n                     (number) The total number of funded witness addresses in existence on the network.\n"
            "     \"number_of_witnesses_total\": n                   (number) The total number of funded witness addresses in existence on the network which pass basic restrictions to witness like minimum weight.\n"
            "     \"number_of_witnesses_eligible\": n                (number) The total number of witness addresses on the network which were considered eligible candidates to witness for this block.\n"
            "     \"total_witness_weight_raw\": n                    (number) The total weight of all witness addresses that were part of \"number_of_witnesses_total\".\n"
            "     \"total_witness_weight_eligible_raw\": n,          (number) The total weight of all witness addresses that were part of \"number_of_witnesses_eligible\".\n"
            "     \"total_witness_weight_eligible_adjusted\": n,     (number) The adjusted weight (after applying maximum weight restrictions) of all witness addresses that were part of \"number_of_witnesses_eligible\".\n"
            "     \"selected_witness_address\": address              (string) The address of the witness that has been selected for the current chain tip.\n"
            "     \"witness_statistics\": {\n"
            "         \"weight\": {                                  Weight statistics based on all witness addresses\n"
            "             \"largest\": n                             (number) The largest single address weight on the network.\n"
            "             \"smallest\": n                            (number) The smallest single address weight on the network.\n"
            "             \"mean\": n                                (number) The mean weight of all witness addresses.\n"
            "             \"median\": n                              (number) The median weight of all witness addresses.\n"
            "         }\n"
            "         \"amount\": {                                  Amount statistics based on all witness addresses\n"
            "             \"largest\": n                             (number) The largest single address amount on the network.\n"
            "             \"smallest\": n                            (number) The smallest single address amount on the network.\n"
            "             \"mean\": n                                (number) The mean amount of all witness addresses.\n"
            "             \"median\": n                              (number) The median amount of all witness addresses.\n"
            "         }\n"
            "         \"lock_period\": {                             Lock period statistics based on all witness addresses\n"
            "             \"largest\": n                             (number) The largest single address lock_period on the network.\n"
            "             \"smallest\": n                            (number) The smallest single address lock_period on the network.\n"
            "             \"mean\": n                                (number) The mean lock_period of all witness addresses.\n"
            "             \"median\": n                              (number) The median lock_period of all witness addresses.\n"
            "         }\n"
            "         \"age\": {                                     Age statistics based on all witness addresses (age is how long an address has existed since it last performed an operation of some kind)\n"
            "             \"largest\": n                             (number) The oldest address on the network.\n"
            "             \"smallest\": n                            (number) The more recent address on the network.\n"
            "             \"mean\": n                                (number) The mean age of all witness addresses.\n"
            "             \"median\": n                              (number) The median age of all witness addresses.\n"
            "         }\n"
            "     }\n"
            "     \"witness_address_list\": [                        List of all witness addresses on the network, with address specific information\n"
            "         {\n"
            "             \"type\": address_type                     (string) The type of address output used to create the address. Either SCRIPT or POW2WITNESS depending on whether SegSig was activated at the time of creation or not.\n"
            "             \"address\": address                       (string) The address of the witness that has been selected for the current chain tip.\n"
            "             \"age\": n                                 (number) The age of the address (how long since it was last active in any way)\n"
            "             \"amount\": n                              (number) The amount that is locked in the address.\n"
            "             \"raw_weight\": n                          (number) The raw weight of the address before any adjustments.\n"
            "             \"adjusted_weight\": n                     (number) The weight after 1% limit is applied\n"
            "             \"adjusted_weight_final\": n               (number) The weight considered by the witness algorithm after all adjustments are applied.\n"
            "             \"expected_witness_period\": n             (number) The period that the network will allow this address to go without witnessing before it expires.\n"
            "             \"estimated_witness_period\": n            (number) The average period in which this address should earn a reward over time\n"
            "             \"last_active_block\": n                   (number) The last block in which this address was active.\n"
            "             \"lock_from_block\": n                     (number) The block where this address was originally locked.\n"
            "             \"lock_until_block\": n                    (number) The block that this address will remain locked until.\n"
            "             \"lock_period\": n                         (number) The complete length in time that this address will be locked for\n"
            "             \"lock_period_expired\": n                 (boolean)true if the lock has expired (funds can be withdrawed)\n"
            "             \"eligible_to_witness\": n                 (number) true if the address is eligible to witness.\n"
            "             \"expired_from_inactivity\": n             (number) true if the network has expired (kicked off) this address due to it failing to witness in the expected period\n"
            //"             \"fail_count\": n                          (number) Internal accounting for how many times this address has been renewed; Note it increases in a non-linear fashion but decreases by 1 for every valid witnessing operation.\n"
            //"             \"action_nonce\": n                        (number) Internal count of how many actions this address has been involved in since creation; Used to ensure address transaction uniqueness across operations.\n"
            "             \"ismine_accountname\": n                  (string) If the address belongs to an account in this wallet, the name of the account.\n"
            "         }\n"
            "         ...\n"
            "     ]\n"
            "}]\n"
            "\nExamples:\n"
            "\nBasic witness info for current chain tip\n"
            + HelpExampleCli("getwitnessinfo tip false", "")
            + "\nExtended witness info for the block two blocks before tip\n"
            + HelpExampleCli("getwitnessinfo tip~2 true", "")
            + "\nExtended witness info for block 400000\n"
            + HelpExampleCli("getwitnessinfo 400000 true", "")
            + "\nExtended witness info for block with hash 8383d8e9999ade8ad0c9f84e7816afec3b9e4855341f678bb0fdc3af46ee6f31\n"
            + HelpExampleCli("getwitnessinfo \"8383d8e9999ade8ad0c9f84e7816afec3b9e4855341f678bb0fdc3af46ee6f31\" true", ""));

    #ifdef ENABLE_WALLET
    CWallet * const pwallet = GetWalletForJSONRPCRequest(request);
    LOCK2(cs_main, pwallet ? &pwallet->cs_wallet : NULL);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp))
        return NullUniValue;
    #else
    LOCK(cs_main);
    #endif

    int64_t nTotalWeightAll = 0;
    int64_t nNumWitnessAddressesAll = 0;
    int64_t nPow2Phase = 1;
    std::string sWitnessAddress;
    UniValue jsonAllWitnessAddresses(UniValue::VARR);
    boost::accumulators::accumulator_set<double, boost::accumulators::stats<boost::accumulators::tag::median(boost::accumulators::with_p_square_quantile), boost::accumulators::tag::mean, boost::accumulators::tag::min, boost::accumulators::tag::max> > witnessWeightStats;
    boost::accumulators::accumulator_set<double, boost::accumulators::stats<boost::accumulators::tag::median(boost::accumulators::with_p_square_quantile), boost::accumulators::tag::mean, boost::accumulators::tag::min, boost::accumulators::tag::max> > witnessAmountStats;
    boost::accumulators::accumulator_set<double, boost::accumulators::stats<boost::accumulators::tag::median(boost::accumulators::with_p_square_quantile), boost::accumulators::tag::mean, boost::accumulators::tag::min, boost::accumulators::tag::max> > lockPeriodWeightStats;
    boost::accumulators::accumulator_set<double, boost::accumulators::stats<boost::accumulators::tag::median(boost::accumulators::with_p_square_quantile), boost::accumulators::tag::mean, boost::accumulators::tag::min, boost::accumulators::tag::max> > ageStats;

    CBlockIndex* pTipIndex = nullptr;
    bool fVerbose = false;
    bool showMineOnly = false;
    if (request.params.size() > 0)
    {
        std::string sTipHash = request.params[0].get_str();
        int32_t nTipHeight;
        if (ParseInt32(sTipHash, &nTipHeight))
        {
            pTipIndex = chainActive[nTipHeight];
            if (!pTipIndex)
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Block not found.");
        }
        else
        {
            if (sTipHash == "tip" || sTipHash.empty())
            {
                pTipIndex = chainActive.Tip();
                if (!pTipIndex)
                    throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Chain has no tip.");
            }
            else if(boost::starts_with(sTipHash, "tip~"))
            {
                int nReverseHeight;
                if (!ParseInt32(sTipHash.substr(4,std::string::npos), &nReverseHeight))
                    throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid block specifier.");
                pTipIndex = chainActive.Tip();
                if (!pTipIndex)
                    throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Chain has no tip.");
                while(pTipIndex && nReverseHeight>0)
                {
                    pTipIndex = pTipIndex->pprev;
                    --nReverseHeight;
                }
                if (!pTipIndex)
                    throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid block specifier, chain does not go back that far.");
            }
            else
            {
                uint256 hash(uint256S(sTipHash));
                if (mapBlockIndex.count(hash) == 0)
                    throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Block not found.");
                pTipIndex = mapBlockIndex[hash];
            }
        }
    }
    else
    {
        pTipIndex = chainActive.Tip();
    }

    if (request.params.size() >= 2)
        fVerbose = request.params[1].get_bool();

    if (request.params.size() > 2)
        showMineOnly = request.params[2].get_bool();

    CBlockIndex* pTipIndex_ = nullptr;
    CCloneChain tempChain = chainActive.Clone(pTipIndex, pTipIndex_);

    if (!pTipIndex_)
        throw std::runtime_error("Could not locate a valid PoW² chain that contains this block as tip.");
    CCoinsViewCache viewNew(pcoinsTip);
    CValidationState state;
    if (!ForceActivateChain(pTipIndex_, nullptr, state, Params(), tempChain, viewNew))
        throw std::runtime_error("Could not locate a valid PoW² chain that contains this block as tip.");
    if (!state.IsValid())
        throw JSONRPCError(RPC_DATABASE_ERROR, state.GetRejectReason());

    if (IsPow2Phase5Active(pTipIndex_, Params(), tempChain, &viewNew))
        nPow2Phase = 5;
    else if (IsPow2Phase4Active(pTipIndex_, Params(), tempChain, &viewNew))
        nPow2Phase = 4;
    else if (IsPow2Phase3Active(pTipIndex_, Params(), tempChain, &viewNew))
        nPow2Phase = 3;
    else if (IsPow2Phase2Active(pTipIndex_, Params(), tempChain, &viewNew))
        nPow2Phase = 2;

    CGetWitnessInfo witInfo;

    if (nPow2Phase >= 2)
    {
        CBlock block;
        {
            LOCK(cs_main);// cs_main lock required for ReadBlockFromDisk
            if (!ReadBlockFromDisk(block, pTipIndex_, Params()))
                throw std::runtime_error("Could not load block to obtain PoW² information.");
        }

        if (!GetWitnessInfo(tempChain, Params(), &viewNew, pTipIndex_->pprev, block, witInfo, pTipIndex_->nHeight))
            throw std::runtime_error("Could not enumerate all PoW² witness information for block.");

        if (!GetPow2NetworkWeight(pTipIndex_, Params(), nNumWitnessAddressesAll, nTotalWeightAll, tempChain, &viewNew))
            throw std::runtime_error("Block does not form part of a valid PoW² chain.");

        if (nPow2Phase >= 3)
        {
            if (!GetWitnessHelper(tempChain, Params(), &viewNew, pTipIndex_->pprev, block.GetHashLegacy(), witInfo, pTipIndex_->nHeight))
                throw std::runtime_error("Could not select a valid PoW² witness for block.");

            CTxDestination selectedWitnessAddress;
            if (!ExtractDestination(witInfo.selectedWitnessTransaction, selectedWitnessAddress))
                throw std::runtime_error("Could not extract PoW² witness for block.");

            sWitnessAddress = CGuldenAddress(selectedWitnessAddress).ToString();
        }
    }

    if (fVerbose)
    {
        for (auto& iter : witInfo.allWitnessCoins)
        {
            bool fEligible = false;
            uint64_t nAdjustedWeight = 0;
            {
                auto poolIter = witInfo.witnessSelectionPoolFiltered.begin();
                while (poolIter != witInfo.witnessSelectionPoolFiltered.end())
                {
                    if (poolIter->outpoint == iter.first)
                    {
                        if (poolIter->coin.out == iter.second.out)
                        {
                            nAdjustedWeight = poolIter->nWeight;
                            fEligible = true;
                            break;
                        }
                    }
                    ++poolIter;
                }
            }
            bool fExpired = false;
            {
                auto poolIter = witInfo.witnessSelectionPoolUnfiltered.begin();
                while (poolIter != witInfo.witnessSelectionPoolUnfiltered.end())
                {
                    if (poolIter->outpoint == iter.first)
                    {
                        if (poolIter->coin.out == iter.second.out)
                        {
                            if (witnessHasExpired(poolIter->nAge, poolIter->nWeight, witInfo.nTotalWeight))
                            {
                                fExpired = true;
                            }
                            break;
                        }
                    }
                    ++poolIter;
                }
            }

            CTxDestination address;
            if (!ExtractDestination(iter.second.out, address))
                throw std::runtime_error("Could not extract PoW² witness for block.");

            uint64_t nLastActiveBlock = iter.second.nHeight;
            uint64_t nLockFromBlock = 0;
            uint64_t nLockUntilBlock = 0;
            uint64_t nLockPeriodInBlocks = GetPoW2LockLengthInBlocksFromOutput(iter.second.out, iter.second.nHeight, nLockFromBlock, nLockUntilBlock);
            uint64_t nRawWeight = GetPoW2RawWeightForAmount(iter.second.out.nValue, nLockPeriodInBlocks);
            uint64_t nAge = pTipIndex_->nHeight - nLastActiveBlock;
            CAmount nValue = iter.second.out.nValue;

            bool fLockPeriodExpired = (GetPoW2RemainingLockLengthInBlocks(nLockUntilBlock, pTipIndex_->nHeight) == 0);

            #ifdef ENABLE_WALLET
            std::string accountName = accountNameForAddress(*pwallet, address);
            #endif

            UniValue rec(UniValue::VOBJ);
            rec.push_back(Pair("type", iter.second.out.GetTypeAsString()));
            rec.push_back(Pair("address", CGuldenAddress(address).ToString()));
            rec.push_back(Pair("age", nAge));
            rec.push_back(Pair("amount", ValueFromAmount(nValue)));
            rec.push_back(Pair("raw_weight", nRawWeight));
            rec.push_back(Pair("adjusted_weight", std::min(nRawWeight, witInfo.nMaxIndividualWeight)));
            rec.push_back(Pair("adjusted_weight_final", nAdjustedWeight));
            rec.push_back(Pair("expected_witness_period", expectedWitnessBlockPeriod(nRawWeight, witInfo.nTotalWeight)));
            rec.push_back(Pair("estimated_witness_period", estimatedWitnessBlockPeriod(nRawWeight, witInfo.nTotalWeight)));
            rec.push_back(Pair("last_active_block", nLastActiveBlock));
            rec.push_back(Pair("lock_from_block", nLockFromBlock));
            rec.push_back(Pair("lock_until_block", nLockUntilBlock));
            rec.push_back(Pair("lock_period", nLockPeriodInBlocks));
            rec.push_back(Pair("lock_period_expired", fLockPeriodExpired));
            rec.push_back(Pair("eligible_to_witness", fEligible));
            rec.push_back(Pair("expired_from_inactivity", fExpired));
            //fixme: (2.1) Add these two
            //rec.push_back(Pair("fail_count", fExpired));
            //rec.push_back(Pair("action_nonce", fExpired));
            #ifdef ENABLE_WALLET
            rec.push_back(Pair("ismine_accountname", accountName));
            #else
            rec.push_back(Pair("ismine_accountname", ""));
            #endif

            witnessWeightStats(nRawWeight);
            lockPeriodWeightStats(nLockPeriodInBlocks);
            witnessAmountStats(nValue);
            ageStats(nAge);

            #ifdef ENABLE_WALLET
            if (showMineOnly && accountName.empty())
                continue;
            #endif

            jsonAllWitnessAddresses.push_back(rec);
        }
    }

    UniValue witnessInfoForBlock(UniValue::VARR);
    UniValue rec(UniValue::VOBJ);
    rec.push_back(Pair("pow2_phase", nPow2Phase));
    rec.push_back(Pair("number_of_witnesses_raw", (uint64_t)nNumWitnessAddressesAll));
    rec.push_back(Pair("number_of_witnesses_total", (uint64_t)witInfo.witnessSelectionPoolUnfiltered.size()));
    rec.push_back(Pair("number_of_witnesses_eligible", (uint64_t)witInfo.witnessSelectionPoolFiltered.size()));
    rec.push_back(Pair("total_witness_weight_raw", (uint64_t)nTotalWeightAll));
    rec.push_back(Pair("total_witness_weight_eligible_raw", (uint64_t)witInfo.nTotalWeight));
    rec.push_back(Pair("total_witness_weight_eligible_adjusted", (uint64_t)witInfo.nReducedTotalWeight));
    rec.push_back(Pair("selected_witness_address", sWitnessAddress));
    if (fVerbose)
    {
        UniValue averages(UniValue::VOBJ);
        {
            if (boost::accumulators::count(witnessWeightStats) > 0)
            {
                UniValue weight(UniValue::VOBJ);
                weight.push_back(Pair("largest", boost::accumulators::max(witnessWeightStats)));
                weight.push_back(Pair("smallest", boost::accumulators::min(witnessWeightStats)));
                weight.push_back(Pair("mean", boost::accumulators::mean(witnessWeightStats)));
                weight.push_back(Pair("median", boost::accumulators::median(witnessWeightStats)));
                averages.push_back(Pair("weight", weight));
            }
        }
        {
            if (boost::accumulators::count(witnessAmountStats) > 0)
            {
                UniValue amount(UniValue::VOBJ);
                amount.push_back(Pair("largest", ValueFromAmount(boost::accumulators::max(witnessAmountStats))));
                amount.push_back(Pair("smallest", ValueFromAmount(boost::accumulators::min(witnessAmountStats))));
                amount.push_back(Pair("mean", ValueFromAmount(boost::accumulators::mean(witnessAmountStats))));
                amount.push_back(Pair("median", ValueFromAmount(boost::accumulators::median(witnessAmountStats))));
                averages.push_back(Pair("amount", amount));
            }
        }
        {
            if (boost::accumulators::count(lockPeriodWeightStats) > 0)
            {
                UniValue lockPeriod(UniValue::VOBJ);
                lockPeriod.push_back(Pair("largest", boost::accumulators::max(lockPeriodWeightStats)));
                lockPeriod.push_back(Pair("smallest", boost::accumulators::min(lockPeriodWeightStats)));
                lockPeriod.push_back(Pair("mean", boost::accumulators::mean(lockPeriodWeightStats)));
                lockPeriod.push_back(Pair("median", boost::accumulators::median(lockPeriodWeightStats)));
                averages.push_back(Pair("lock_period", lockPeriod));
            }
        }
        {
            if (boost::accumulators::count(ageStats) > 0)
            {
                UniValue age(UniValue::VOBJ);
                age.push_back(Pair("largest", boost::accumulators::max(ageStats)));
                age.push_back(Pair("smallest", boost::accumulators::min(ageStats)));
                age.push_back(Pair("mean", boost::accumulators::mean(ageStats)));
                age.push_back(Pair("median", boost::accumulators::median(ageStats)));
                averages.push_back(Pair("age", age));
            }
        }
        rec.push_back(Pair("witness_statistics", averages));
        rec.push_back(Pair("witness_address_list", jsonAllWitnessAddresses));
    }
    witnessInfoForBlock.push_back(rec);

    return witnessInfoForBlock;
}


static UniValue dumpdiffarray(const JSONRPCRequest& request)
{
    CWallet* const pwallet = GetWalletForJSONRPCRequest(request);
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
    unsigned int nNumToOutput = request.params[0].get_int();
    reverseOutBuffer.reserve(16*nNumToOutput);

    CBlockIndex* pBlock = chainActive.Tip();
    while(pBlock->pprev && (unsigned int)pBlock->nHeight > nNumToOutput)
        pBlock = pBlock->pprev;

    int count=1;
    while(pBlock)
    {
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


static UniValue dumpblockgaps(const JSONRPCRequest& request)
{
    CWallet * const pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp))
        return NullUniValue;

    if (request.fHelp || request.params.size() != 2)
        throw std::runtime_error(
            "dumpblockgaps  start_height count\n"
            "\nDump the block gaps for the last n blocks.\n"
            "\nArguments:\n"
            "1. start_height     (numeric) Where to start dumping from, counting backwards from chaintip.\n"
            "2. count           (numeric) The number of blocks to dump the block gaps of - going backwards from the start_height.\n"
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


static UniValue dumptransactionstats(const JSONRPCRequest& request)
{
    CWallet * const pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp))
        return NullUniValue;

    if (request.fHelp || request.params.size() != 2)
        throw std::runtime_error(
            "dumptransactionstats start_height count\n"
            "\nDump the transaction stats for the last n blocks.\n"
            "\nArguments:\n"
            "1. start_height     (numeric) Where to start dumping from, counting backwards from chaintip.\n"
            "2. count           (numeric) The number of blocks to dump the block gaps of - going backwards from the start_height.\n"
            "\nExamples:\n"
            + HelpExampleCli("dumpblockgaps 50", ""));

    RPCTypeCheck(request.params, boost::assign::list_of(UniValue::VNUM));

    int nStart = request.params[0].get_int();
    int nNumToOutput = request.params[1].get_int();

    CBlockIndex* pBlock = chainActive.Tip();

    UniValue jsonGaps(UniValue::VARR);

    while(pBlock && pBlock->pprev && --nStart>0)
    {
        pBlock = pBlock->pprev;
    }

    int count = 0;
    std::map<int64_t, int64_t> inputCount;
    std::map<int64_t, int64_t> outputCount;
    inputCount[1]=outputCount[1]=0;
    inputCount[2]=outputCount[2]=0;
    inputCount[3]=outputCount[3]=0;
    inputCount[4]=outputCount[4]=0;
    inputCount[5]=outputCount[5]=0;
    inputCount[6]=outputCount[6]=0;
    inputCount[7]=outputCount[7]=0;

    while(pBlock && pBlock->pprev && --nNumToOutput>0)
    {
        CBlock block;
        LOCK(cs_main);// cs_main lock required for ReadBlockFromDisk
        if (ReadBlockFromDisk(block, pBlock, Params()))
        {
            for (auto transaction : block.vtx)
            {
                ++count;
                if (transaction->vin.size() >=7)
                {
                    ++inputCount[7];
                }
                else
                {
                    ++inputCount[transaction->vin.size()];
                }
                if (transaction->vout.size() >=7)
                {
                    ++outputCount[7];
                }
                else
                {
                    ++outputCount[transaction->vout.size()];
                }
            }
        }
        pBlock = pBlock->pprev;
    }

    jsonGaps.push_back("count:");
    jsonGaps.push_back(count);
    jsonGaps.push_back("1:");
    jsonGaps.push_back(inputCount[1]);
    jsonGaps.push_back(outputCount[1]);
    jsonGaps.push_back("2:");
    jsonGaps.push_back(inputCount[2]);
    jsonGaps.push_back(outputCount[2]);
    jsonGaps.push_back("3:");
    jsonGaps.push_back(inputCount[3]);
    jsonGaps.push_back(outputCount[3]);
    jsonGaps.push_back("4:");
    jsonGaps.push_back(inputCount[4]);
    jsonGaps.push_back(outputCount[4]);
    jsonGaps.push_back("5:");
    jsonGaps.push_back(inputCount[5]);
    jsonGaps.push_back(outputCount[5]);
    jsonGaps.push_back("6:");
    jsonGaps.push_back(inputCount[6]);
    jsonGaps.push_back(outputCount[6]);
    jsonGaps.push_back("n:");
    jsonGaps.push_back(inputCount[7]);
    jsonGaps.push_back(outputCount[7]);


    return jsonGaps;
}


static UniValue changeaccountname(const JSONRPCRequest& request)
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
            + HelpExampleCli("changeaccountname \"My witness account\" \"Charity donations\"", "")
            + HelpExampleRpc("changeaccountname \"My witness account\" \"Charity donations\"", ""));



    if (!pwallet)
        throw std::runtime_error("Cannot use command without an active wallet");

    CAccount* account = AccountFromValue(pwallet, request.params[0], false);
    std::string label = request.params[1].get_str();

    pwallet->changeAccountName(account, label);

    return account->getLabel();
}

#define MINIMUM_VALUABLE_AMOUNT 1000000000
static UniValue deleteaccount(const JSONRPCRequest& request)
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
            "deleteaccount \"account\" \"force\"\n"
            "\nDelete an account.\n"
            "\nArguments:\n"
            "1. \"account\"        (string) The UUID or unique label of the account.\n"
            "2. \"force\"          (string) Specify string force to force deletion of a non-empty account.\n"
            "\nExamples:\n"
            + HelpExampleCli("deleteaccount \"My account\"", "")
            + HelpExampleRpc("deleteaccount \"My account\"", ""));



    if (!pwallet)
        throw std::runtime_error("Cannot use command without an active wallet");

    CAccount* account = AccountFromValue(pwallet, request.params[0], false);

    bool forcePurge = false;
    if (account->IsPoW2Witness() && account->IsFixedKeyPool())
        forcePurge = true;
    if (request.params.size() == 1 || request.params[1].get_str() != "force")
    {
        boost::uuids::uuid accountUUID = account->getUUID();
        CAmount balance = pwallet->GetLegacyBalance(ISMINE_SPENDABLE, 0, &accountUUID );
        if (account->IsPoW2Witness() && account->IsFixedKeyPool())
        {
            balance = pwallet->GetBalance(account, false, true);
        }
        if (balance > MINIMUM_VALUABLE_AMOUNT && !account->IsReadOnly())
        {
            throw std::runtime_error("Account not empty, please first empty your account before trying to delete it.");
        }
    }

    pwallet->deleteAccount(account, forcePurge);
    return true;
}


static UniValue createaccounthelper(CWallet* pwallet, std::string accountName, std::string accountType, bool bMakeActive=true)
{
    CAccount* account = NULL;

    EnsureWalletIsUnlocked(pwallet);

    if (accountType == "HD")
    {
        account = pwallet->GenerateNewAccount(accountName, AccountState::Normal, AccountType::Desktop, bMakeActive);
    }
    else if (accountType == "Mobile")
    {
        account = pwallet->GenerateNewAccount(accountName.c_str(), AccountState::Normal, AccountType::Mobi, bMakeActive);
    }
    else if (accountType == "Witness")
    {
        account = pwallet->GenerateNewAccount(accountName.c_str(), AccountState::Normal, AccountType::PoW2Witness, bMakeActive);
    }
    else if (accountType == "Legacy")
    {
        account = pwallet->GenerateNewLegacyAccount(accountName.c_str());
    }

    if (!account)
        throw std::runtime_error("Unable to create account.");

    return getUUIDAsString(account->getUUID());
}

static UniValue createaccount(const JSONRPCRequest& request)
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
            "createaccount \"name\" \"type\"\n"
            "Create an account, for HD accounts the currently active seed will be used to create the account.\n"
            "\nArguments:\n"
            "1. \"name\"       (string) Specify the label for the account.\n"
            "2. \"type\"       (string, optional) Type of account to create (HD; Mobile; Legacy; Witness)\n"
            "\nExamples:\n"
            + HelpExampleCli("createaccount \"My new account\"", "")
            + HelpExampleRpc("createaccount \"My new account\"", ""));



    if (!pwallet)
        throw std::runtime_error("Cannot use command without an active wallet");


    std::string accountType = "HD";
    if (request.params.size() > 1)
    {
        accountType = request.params[1].get_str();
        if (accountType != "HD" && accountType != "Mobile" && accountType != "Legacy" && accountType != "Witness")
            throw std::runtime_error("Invalid account type");
    }

    return createaccounthelper(pwallet, request.params[0].get_str(), accountType);
}


static UniValue createwitnessaccount(const JSONRPCRequest& request)
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
            "createwitnessaccount \"name\"\n"
            "Create an account, the currently active seed will be used to create the account.\n"
            "\nArguments:\n"
            "1. \"name\"       (string) Specify the label for the account.\n"
            "\nExamples:\n"
            + HelpExampleCli("createwitnessaccount \"My 3y savings\"", "")
            + HelpExampleRpc("createwitnessaccount \"My 3y savings\"", ""));

    if (!pwallet)
        throw std::runtime_error("Cannot use command without an active wallet");

    if (GetPoW2Phase(chainActive.Tip(), Params(), chainActive) < 2)
        throw std::runtime_error("Cannot create witness accounts before phase 2 activates.");

    return createaccounthelper(pwallet, request.params[0].get_str(), "Witness", false);
}

static std::vector<std::tuple<CTxOut, uint64_t, COutPoint>> getCurrentOutputsForWitnessAddress(CGuldenAddress& searchAddress)
{
    std::map<COutPoint, Coin> allWitnessCoins;
    if (!getAllUnspentWitnessCoins(chainActive, Params(), chainActive.Tip(), allWitnessCoins))
        throw std::runtime_error("Failed to enumerate all witness coins.");

    std::vector<std::tuple<CTxOut, uint64_t, COutPoint>> matchedOutputs;
    for (const auto& [outpoint, coin] : allWitnessCoins)
    {
        CTxDestination compareDestination;
        bool fValidAddress = ExtractDestination(coin.out, compareDestination);

        if (fValidAddress && (CGuldenAddress(compareDestination) == searchAddress))
        {
            matchedOutputs.push_back(std::tuple(coin.out, coin.nHeight, outpoint));
        }
    }
    return matchedOutputs;
}

static std::vector<std::tuple<CTxOut, uint64_t, COutPoint>> getCurrentOutputsForWitnessAccount(CAccount* forAccount)
{
    std::map<COutPoint, Coin> allWitnessCoins;
    if (!getAllUnspentWitnessCoins(chainActive, Params(), chainActive.Tip(), allWitnessCoins))
        throw std::runtime_error("Failed to enumerate all witness coins.");

    std::vector<std::tuple<CTxOut, uint64_t, COutPoint>> matchedOutputs;
    for (const auto& [outpoint, coin] : allWitnessCoins)
    {
        if (IsMine(*forAccount, coin.out))
        {
            matchedOutputs.push_back(std::tuple(coin.out, coin.nHeight, outpoint));
        }
    }
    return matchedOutputs;
}

//! Given a string specifier, calculate a lock length in blocks to match it. e.g. 1d -> 576; 5b -> 5; 1m -> 17280
//! Returns 0 if specifier is invalid.
static uint64_t GetLockPeriodInBlocksFromFormattedStringSpecifier(std::string formattedLockPeriodSpecifier)
{
    uint64_t lockPeriodInBlocks = 0;
    int nMultiplier = 1;
    if (boost::algorithm::ends_with(formattedLockPeriodSpecifier, "y"))
    {
        nMultiplier = 365 * 576;
        formattedLockPeriodSpecifier.pop_back();
    }
    else if (boost::algorithm::ends_with(formattedLockPeriodSpecifier, "m"))
    {
        nMultiplier = 30 * 576;
        formattedLockPeriodSpecifier.pop_back();
    }
    else if (boost::algorithm::ends_with(formattedLockPeriodSpecifier, "w"))
    {
        nMultiplier = 7 * 576;
        formattedLockPeriodSpecifier.pop_back();
    }
    else if (boost::algorithm::ends_with(formattedLockPeriodSpecifier, "d"))
    {
        nMultiplier = 576;
        formattedLockPeriodSpecifier.pop_back();
    }
    else if (boost::algorithm::ends_with(formattedLockPeriodSpecifier, "b"))
    {
        nMultiplier = 1;
        formattedLockPeriodSpecifier.pop_back();
    }
    if (!ParseUInt64(formattedLockPeriodSpecifier, &lockPeriodInBlocks))
        return 0;
    lockPeriodInBlocks *=  nMultiplier;
    return lockPeriodInBlocks;
}

static UniValue fundwitnessaccount(const JSONRPCRequest& request)
{
    #ifdef ENABLE_WALLET
    CWallet * const pwallet = GetWalletForJSONRPCRequest(request);
    LOCK2(cs_main, pwallet ? &pwallet->cs_wallet : NULL);
    #else
    LOCK(cs_main);
    #endif

    if (!EnsureWalletIsAvailable(pwallet, request.fHelp))
        return NullUniValue;

    if (request.fHelp || request.params.size() < 4 || request.params.size() > 5)
        throw std::runtime_error(
            "fundwitnessaccount \"funding_account\" \"witness_account\" \"amount\" \"time\" \"force_multiple\" \n"
            "Lock \"amount\" NLG in \"witness_account\" for time period \"time\" using funds from \"funding_account\"\n"
            "NB! Though it is possible to fund a witness account that already has a balance, this can cause UI issues and is not strictly supported.\n"
            "It is highly recommended to rather use 'extendwitnessaccount' in this case which behaves more like what most people would expect.\n"
            "By default this command will fail if an account already contains an existing funded address.\n"
            "Note that this command is not currently calendar aware, it performs simplistic conversion i.e. 1 month is 30 days. This may change in future.\n"
            "\nArguments:\n"
            "1. \"funding_account\"      (string, required) The unique UUID or label for the account from which money will be removed. Use \"\" for the active account or \"*\" for all accounts to be considered.\n"
            "2. \"witness_account\"      (string, required) The unique UUID or label for the witness account that will hold the locked funds.\n"
            "3. \"amount\"               (string, required) The amount of NLG to hold locked in the witness account. Minimum amount of 5000 NLG is allowed.\n"
            "4. \"time\"                 (string, required) The time period for which the funds should be locked in the witness account. Minimum of 1 month and a maximum of 3 years. By default this is interpreted as blocks e.g. \"1000\", suffix with \"y\", \"m\", \"w\", \"d\", \"b\" to specifically work in years, months, weeks, days or blocks.\n"
            "5. force_multiple         (boolean, optional, default=false) Allow funding an account that already contains a valid witness address. \n"
            "\nResult:\n"
            "[\n"
            "     \"address\":\"address\", (string) The witness address that has been created \n"
            "     \"txid\":\"txid\"        (string) The transaction id.\n"
            "]\n"
            "\nExamples:\n"
            "\nTake 10000NLG out of \"mysavingsaccount\" and lock in \"mywitnessaccount\" for 2 years.\n"
            + HelpExampleCli("fundwitnessaccount \"mysavingsaccount\" \"mywitnessaccount\" \"10000\" \"2y\"", "")
            + "\nTake 10000NLG out of \"mysavingsaccount\" and lock in \"mywitnessaccount\" for 2 months.\n"
            + HelpExampleCli("fundwitnessaccount \"mysavingsaccount\" \"mywitnessaccount\" \"10000\" \"2m\"", "")
            + "\nTake 10000NLG out of \"mysavingsaccount\" and lock in \"mywitnessaccount\" for 100 days.\n"
            + HelpExampleCli("fundwitnessaccount \"mysavingsaccount\" \"mywitnessaccount\" \"10000\" \"100d\"", "")
            + HelpExampleRpc("fundwitnessaccount \"mysavingsaccount\" \"mywitnessaccount\" \"10000\" \"2y\"", ""));

    int nPoW2TipPhase = GetPoW2Phase(chainActive.Tip(), Params(), chainActive);

    // Basic sanity checks.
    if (!pwallet)
        throw std::runtime_error("Cannot use command without an active wallet");
    if (nPoW2TipPhase < 2)
        throw std::runtime_error("Cannot fund witness accounts before phase 2 activates.");

    EnsureWalletIsUnlocked(pwallet);

    // arg1 - 'from' account.
    CAccount* fundingAccount = AccountFromValue(pwallet, request.params[0], true);
    if (!fundingAccount)
        throw std::runtime_error(strprintf("Unable to locate funding account [%s].",  request.params[0].get_str()));

    // arg2 - 'to' account.
    CAccount* targetWitnessAccount = AccountFromValue(pwallet, request.params[1], true);
    if (!targetWitnessAccount)
        throw std::runtime_error(strprintf("Unable to locate witness account [%s].",  request.params[1].get_str()));
    if (!targetWitnessAccount->IsPoW2Witness())
        throw std::runtime_error(strprintf("Specified account is not a witness account [%s].",  request.params[1].get_str()));
    if (targetWitnessAccount->m_Type == WitnessOnlyWitnessAccount || !targetWitnessAccount->IsHD())
        throw std::runtime_error(strprintf("Cannot fund a witness only witness account [%s].",  request.params[1].get_str()));

    const auto& unspentWitnessOutputs = getCurrentOutputsForWitnessAccount(targetWitnessAccount);
    if (unspentWitnessOutputs.size() > 0)
    {
        bool fAllowMultiple = false;
        if (request.params.size() >= 5)
            fAllowMultiple = request.params[5].get_bool();

        if (!fAllowMultiple)
            throw std::runtime_error("Account already has an active funded witness address. Perhaps you intended to 'extend' it? See: 'help extendwitnessaccount'");
    }

    // arg3 - amount
    CAmount nAmount =  AmountFromValue(request.params[2]);
    if (nAmount < (gMinimumWitnessAmount*COIN))
        throw JSONRPCError(RPC_TYPE_ERROR, strprintf("Witness amount must be %d or larger", gMinimumWitnessAmount));

    // arg4 - lock period.
    // Calculate lock period based on suffix (if one is present) otherwise leave as is.
    std::string formattedLockPeriodSpecifier = request.params[3].getValStr();
    uint64_t nLockPeriodInBlocks = GetLockPeriodInBlocksFromFormattedStringSpecifier(formattedLockPeriodSpecifier);
    if (nLockPeriodInBlocks == 0)
        throw JSONRPCError(RPC_TYPE_ERROR, "Invalid number passed for lock period.");

    if (nLockPeriodInBlocks > 3 * 365 * 576)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Maximum lock period of 3 years exceeded.");

    if (nLockPeriodInBlocks < 30 * 576)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Minimum lock period of 1 month exceeded.");

    // Add a small buffer to give us time to enter the blockchain
    if (nLockPeriodInBlocks == 30 * 576)
        nLockPeriodInBlocks += 50;

    // Enforce minimum weight
    int64_t nWeight = GetPoW2RawWeightForAmount(nAmount, nLockPeriodInBlocks);
    if (nWeight < gMinimumWitnessWeight)
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "PoW² witness has insufficient weight.");
    }

    // Finally attempt to create and send the witness transaction.
    CPoW2WitnessDestination destinationPoW2Witness;
    destinationPoW2Witness.lockFromBlock = 0;
    destinationPoW2Witness.lockUntilBlock = chainActive.Tip()->nHeight + nLockPeriodInBlocks;
    destinationPoW2Witness.failCount = 0;
    destinationPoW2Witness.actionNonce = 0;
    {
        CReserveKeyOrScript keyWitness(pactiveWallet, targetWitnessAccount, KEYCHAIN_WITNESS);
        CPubKey pubWitnessKey;
        if (!keyWitness.GetReservedKey(pubWitnessKey))
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Error allocating witness key for witness account.");

        //We delibritely return the key here, so that if we fail we won't leak the key.
        //The key will be marked as used when the transaction is accepted anyway.
        keyWitness.ReturnKey();
        destinationPoW2Witness.witnessKey = pubWitnessKey.GetID();
    }
    {
        //Code should be refactored to only call 'KeepKey' -after- success, a bit tricky to get there though.
        CReserveKeyOrScript keySpending(pactiveWallet, targetWitnessAccount, KEYCHAIN_SPENDING);
        CPubKey pubSpendingKey;
        if (!keySpending.GetReservedKey(pubSpendingKey))
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Error allocating spending key for witness account.");

        //We delibritely return the key here, so that if we fail we won't leak the key.
        //The key will be marked as used when the transaction is accepted anyway.
        keySpending.ReturnKey();
        destinationPoW2Witness.spendingKey = pubSpendingKey.GetID();
    }

    CAmount nFeeRequired;
    std::string strError;
    std::vector<CRecipient> vecSend;
    int nChangePosRet = -1;

    CRecipient recipient = ( IsSegSigEnabled(chainActive.TipPrev()) ? ( CRecipient(GetPoW2WitnessOutputFromWitnessDestination(destinationPoW2Witness), nAmount, false) ) : ( CRecipient(GetScriptForDestination(destinationPoW2Witness), nAmount, false) ) ) ;
    if (!IsSegSigEnabled(chainActive.TipPrev()))
    {
        // We have to copy this anyway even though we are using a CSCript as later code depends on it to grab the witness key id.
        recipient.witnessDetails.witnessKeyID = destinationPoW2Witness.witnessKey;
    }

    //NB! Setting this is -super- important, if we don't then encrypted wallets may fail to witness.
    recipient.witnessForAccount = targetWitnessAccount;
    vecSend.push_back(recipient);

    CWalletTx wtx;
    CReserveKeyOrScript reservekey(pwallet, fundingAccount, KEYCHAIN_CHANGE);
    if (!pwallet->CreateTransaction(fundingAccount, vecSend, wtx, reservekey, nFeeRequired, nChangePosRet, strError))
    {
        throw JSONRPCError(RPC_WALLET_ERROR, strError);
    }

    CValidationState state;
    if (!pwallet->CommitTransaction(wtx, reservekey, g_connman.get(), state))
    {
        strError = strprintf("Error: The transaction was rejected! Reason given: %s", state.GetRejectReason());
        throw JSONRPCError(RPC_WALLET_ERROR, strError);
    }

    UniValue result(UniValue::VOBJ);
    CTxDestination dest;
    ExtractDestination(wtx.tx->vout[0], dest);
    result.push_back(Pair("address",CGuldenAddress(dest).ToString()));
    result.push_back(Pair("txid", wtx.GetHash().GetHex()));
    return result;
}

static UniValue extendwitnessaddresshelper(CAccount* fundingAccount, std::vector<std::tuple<CTxOut, uint64_t, COutPoint>> unspentWitnessOutputs, CWallet* pwallet, const JSONRPCRequest& request)
{
    if (unspentWitnessOutputs.size() > 1)
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, strprintf("Address has too many witness outputs cannot extend "));

    // arg3 - amount
    CAmount requestedAmount =  AmountFromValue(request.params[2]);
    if (requestedAmount < (gMinimumWitnessAmount*COIN))
        throw JSONRPCError(RPC_TYPE_ERROR, strprintf("Witness amount must be %d or larger", gMinimumWitnessAmount));

    // arg4 - lock period.
    // Calculate lock period based on suffix (if one is present) otherwise leave as is.
    std::string formattedLockPeriodSpecifier = request.params[3].getValStr();
    uint64_t requestedLockPeriodInBlocks = GetLockPeriodInBlocksFromFormattedStringSpecifier(formattedLockPeriodSpecifier);
    if (requestedLockPeriodInBlocks == 0)
        throw JSONRPCError(RPC_TYPE_ERROR, "Invalid number passed for lock period.");

    if (requestedLockPeriodInBlocks > 3 * 365 * 576)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Maximum lock period of 3 years exceeded.");

    if (requestedLockPeriodInBlocks < 30 * 576)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Minimum lock period of 1 month exceeded.");

    // Add a small buffer to give us time to enter the blockchain
    if (requestedLockPeriodInBlocks == 30 * 576)
        requestedLockPeriodInBlocks += 50;

    // Check for immaturity
    const auto& [currentWitnessTxOut, currentWitnessHeight, currentWitnessOutpoint] = unspentWitnessOutputs[0];
    //fixme: (2.1) - This check should go through the actual chain maturity stuff (via wtx) and not calculate directly.
    if (chainActive.Tip()->nHeight - currentWitnessHeight < (uint64_t)(COINBASE_MATURITY))
        throw JSONRPCError(RPC_MISC_ERROR, "Cannot perform operation on immature transaction, please wait for transaction to mature and try again");

    // Calculate existing lock period
    CTxOutPoW2Witness currentWitnessDetails;
    GetPow2WitnessOutput(currentWitnessTxOut, currentWitnessDetails);
    uint64_t remainingLockDurationInBlocks = GetPoW2RemainingLockLengthInBlocks(currentWitnessDetails.lockUntilBlock, chainActive.Tip()->nHeight);
    if (remainingLockDurationInBlocks == 0)
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "PoW² witness has already unlocked so cannot be extended.");
    }

    if (requestedLockPeriodInBlocks < remainingLockDurationInBlocks)
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("New lock period [%d] does not exceed remaining lock period [%d]", requestedLockPeriodInBlocks, remainingLockDurationInBlocks));
    }

    if (requestedAmount < currentWitnessTxOut.nValue)
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("New amount [%d] is smaller than current amount [%d]", requestedAmount, currentWitnessTxOut.nValue));
    }

    // Enforce minimum weight
    int64_t newWeight = GetPoW2RawWeightForAmount(requestedAmount, requestedLockPeriodInBlocks);
    if (newWeight < gMinimumWitnessWeight)
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "PoW² witness has insufficient weight.");
    }

    // Enforce new weight > old weight
    uint64_t notUsed1, notUsed2;
    int64_t oldWeight = GetPoW2RawWeightForAmount(currentWitnessTxOut.nValue, GetPoW2LockLengthInBlocksFromOutput(currentWitnessTxOut, currentWitnessHeight, notUsed1, notUsed2));
    if (newWeight <= oldWeight)
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("New weight [%d] does not exceed old weight [%d].", newWeight, oldWeight));
    }

    // Find the account for this address
    CAccount* witnessAccount = pwallet->FindAccountForTransaction(currentWitnessTxOut);
    if (!witnessAccount)
        throw JSONRPCError(RPC_MISC_ERROR, "Could not locate account for address");
    if ((!witnessAccount->IsPoW2Witness()) || witnessAccount->IsFixedKeyPool())
    {
        throw JSONRPCError(RPC_MISC_ERROR, "Cannot extend a witness-only account as spend key is required to do this.");
    }

    //fixme: (2.1) factor this all out into a helper.
    // Finally attempt to create and send the witness transaction.
    CReserveKeyOrScript reservekey(pwallet, fundingAccount, KEYCHAIN_CHANGE);
    std::string reasonForFail;
    CAmount transactionFee;
    CMutableTransaction extendWitnessTransaction(CTransaction::SEGSIG_ACTIVATION_VERSION);
    {
        // Add the existing witness output as an input
        pwallet->AddTxInput(extendWitnessTransaction, CInputCoin(currentWitnessOutpoint, currentWitnessTxOut), false);

        // Add new witness output
        CTxOut extendedWitnessTxOutput;
        extendedWitnessTxOutput.SetType(CTxOutType::PoW2WitnessOutput);
        // As we are extending the address we reset the "lock from" and we set the "lock until" everything else except the value remains unchanged.
        extendedWitnessTxOutput.output.witnessDetails.lockFromBlock = 0;
        extendedWitnessTxOutput.output.witnessDetails.lockUntilBlock = chainActive.Tip()->nHeight + requestedLockPeriodInBlocks;
        extendedWitnessTxOutput.output.witnessDetails.spendingKeyID = currentWitnessDetails.spendingKeyID;
        extendedWitnessTxOutput.output.witnessDetails.witnessKeyID = currentWitnessDetails.witnessKeyID;
        extendedWitnessTxOutput.output.witnessDetails.failCount = currentWitnessDetails.failCount;
        extendedWitnessTxOutput.output.witnessDetails.actionNonce = currentWitnessDetails.actionNonce+1;
        extendedWitnessTxOutput.nValue = requestedAmount;
        extendWitnessTransaction.vout.push_back(extendedWitnessTxOutput);

        // Fund the additional amount in the transaction (including fees)
        int changeOutputPosition = 1;
        std::set<int> subtractFeeFromOutputs; // Empty - we don't subtract fee from outputs
        CCoinControl coincontrol;
        if (!pwallet->FundTransaction(fundingAccount, extendWitnessTransaction, transactionFee, changeOutputPosition, reasonForFail, false, subtractFeeFromOutputs, coincontrol, reservekey))
        {
            throw JSONRPCError(RPC_MISC_ERROR, strprintf("Failed to fund transaction [%s]", reasonForFail.c_str()));
        }
    }

    uint256 finalTransactionHash;
    {
        LOCK2(cs_main, pactiveWallet->cs_wallet);
        if (!pwallet->SignAndSubmitTransaction(reservekey, extendWitnessTransaction, reasonForFail, &finalTransactionHash))
        {
            throw JSONRPCError(RPC_MISC_ERROR, strprintf("Failed to commit transaction [%s]", reasonForFail.c_str()));
        }
    }

    UniValue result(UniValue::VOBJ);
    result.push_back(Pair("txid", finalTransactionHash.GetHex()));
    result.push_back(Pair("fee_amount", ValueFromAmount(transactionFee)));
    return result;
}

static UniValue extendwitnessaddress(const JSONRPCRequest& request)
{
    #ifdef ENABLE_WALLET
    CWallet * const pwallet = GetWalletForJSONRPCRequest(request);
    LOCK2(cs_main, pwallet ? &pwallet->cs_wallet : NULL);
    #else
    LOCK(cs_main);
    #endif

    if (!EnsureWalletIsAvailable(pwallet, request.fHelp))
        return NullUniValue;

    if (request.fHelp || request.params.size() != 4)
        throw std::runtime_error(
            "extendwitnessaddress \"funding_account\" \"witness_address\" \"amount\" \"time\" \n"
            "Change the currently locked amount and time period for \"witness_address\" to match the new \"amount\" ant time period \"time\"\n"
            "Note the new amount must be ≥ the old amount, and the new time period must exceed the remaining lock period that is currently set on the account\n"
            "\"funding_account\" is the account from which the locked funds will be claimed.\n"
            "\"time\" may be a minimum of 1 month and a maximum of 3 years.\n"
            "\nArguments:\n"
            "1. \"funding_account\"  (string, required) The unique UUID or label for the account from which money will be removed.\n"
            "2. \"witness_address\"  (string, required) The Gulden address for the witness key.\n"
            "3. \"amount\"           (string, required) The amount of NLG to hold locked in the witness account. Minimum amount of 5000 NLG is allowed.\n"
            "4. \"time\"             (string, required) The time period for which the funds should be locked in the witness account. By default this is interpreted as blocks e.g. \"1000\", prefix with \"y\", \"m\", \"w\", \"d\", \"b\" to specifically work in years, months, weeks, days or blocks.\n"
            "\nResult:\n"
            "[\n"
            "     \"txid\":\"txid\",   (string) The txid of the created transaction\n"
            "     \"fee_amount\":n   (number) The fee that was paid.\n"
            "]\n"
            "\nExamples:\n"
            + HelpExampleCli("extendwitnessaddress \"My account\" \"2ZnFwkJyYeEftAoQDe7PC96t2Y7XMmKdNtekRdtx32GNQRJztULieFRFwQoQqN\" \"50000\" \"2y\"", "")
            + HelpExampleRpc("extendwitnessaddress \"My account\" \"2ZnFwkJyYeEftAoQDe7PC96t2Y7XMmKdNtekRdtx32GNQRJztULieFRFwQoQqN\" \"50000\" \"2y\"", ""));

    // Basic sanity checks.
    if (!pwallet)
        throw std::runtime_error("Cannot use command without an active wallet");
    if (!IsSegSigEnabled(chainActive.TipPrev()))
        throw std::runtime_error("Cannot use this command before segsig activates");

    EnsureWalletIsUnlocked(pwallet);

    // arg1 - 'from' account.
    CAccount* fundingAccount = AccountFromValue(pwallet, request.params[0], false);
    if (!fundingAccount)
        throw std::runtime_error(strprintf("Unable to locate funding account [%s].",  request.params[0].get_str()));

    // arg2 - 'to' address.
    CGuldenAddress witnessAddress(request.params[1].get_str());
    bool isValid = witnessAddress.IsValidWitness(Params());

    if (!isValid)
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, strprintf("Not a valid witness address [%s].", request.params[1].get_str()));

    const auto& unspentWitnessOutputs = getCurrentOutputsForWitnessAddress(witnessAddress);
    if (unspentWitnessOutputs.size() == 0)
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, strprintf("Address does not contain any witness outputs [%s].", request.params[1].get_str()));

    return extendwitnessaddresshelper(fundingAccount, unspentWitnessOutputs, pwallet, request);
}

static UniValue extendwitnessaccount(const JSONRPCRequest& request)
{
    #ifdef ENABLE_WALLET
    CWallet * const pwallet = GetWalletForJSONRPCRequest(request);
    LOCK2(cs_main, pwallet ? &pwallet->cs_wallet : NULL);
    #else
    LOCK(cs_main);
    #endif

    if (!EnsureWalletIsAvailable(pwallet, request.fHelp))
        return NullUniValue;

    if (request.fHelp || request.params.size() != 4)
        throw std::runtime_error(
            "extendwitnessaccount \"funding_account\" \"witness_account\" \"amount\" \"time\" \n"
            "Change the currently locked amount and time period for \"witness_account\" to match the new \"amount\" ant time period \"time\"\n"
            "Note the new amount must be ≥ the old amount, and the new time period must exceed the remaining lock period that is currently set on the account\n"
            "\"funding_account\" is the account from which the locked funds will be claimed.\n"
            "\"time\" may be a minimum of 1 month and a maximum of 3 years.\n"
            "\nArguments:\n"
            "1. \"funding_account\" (string, required) The unique UUID or label for the account from which money will be removed.\n"
            "2. \"witness_account\" (string, required) The unique UUID or label for the witness account that will hold the locked funds.\n"
            "3. \"amount\"          (string, required) The amount of NLG to hold locked in the witness account. Minimum amount of 5000 NLG is allowed.\n"
            "4. \"time\"            (string, required) The time period for which the funds should be locked in the witness account. By default this is interpreted as blocks e.g. \"1000\", prefix with \"y\", \"m\", \"w\", \"d\", \"b\" to specifically work in years, months, weeks, days or blocks.\n"
            "\nResult:\n"
            "[\n"
            "     \"txid\":\"txid\",  (string) The txid of the created transaction\n"
            "     \"fee_amount\":n  (string) The fee that was paid.\n"
            "]\n"
            "\nExamples:\n"
            + HelpExampleCli("extendwitnessaccount \"My account\" \"My witness account\" \"50000\" \"2y\"", "")
            + HelpExampleRpc("extendwitnessaccount \"My account\" \"My witness account\" \"50000\" \"2y\"", ""));

    // Basic sanity checks.
    if (!pwallet)
        throw std::runtime_error("Cannot use command without an active wallet");
    if (!IsSegSigEnabled(chainActive.TipPrev()))
        throw std::runtime_error("Cannot use this command before segsig activates");

    EnsureWalletIsUnlocked(pwallet);

    // arg1 - 'from' account.
    CAccount* fundingAccount = AccountFromValue(pwallet, request.params[0], false);
    if (!fundingAccount)
        throw std::runtime_error(strprintf("Unable to locate funding account [%s].",  request.params[0].get_str()));

    // arg2 - 'to' account.
    CAccount* witnessAccount = AccountFromValue(pwallet, request.params[1], false);
    if (!witnessAccount)
        throw std::runtime_error(strprintf("Unable to locate witness account [%s].",  request.params[1].get_str()));

    if ((!witnessAccount->IsPoW2Witness()) || witnessAccount->IsFixedKeyPool())
    {
        throw JSONRPCError(RPC_MISC_ERROR, "Cannot extend a witness-only account as spend key is required to do this.");
    }

    const auto& unspentWitnessOutputs = getCurrentOutputsForWitnessAccount(witnessAccount);
    if (unspentWitnessOutputs.size() == 0)
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, strprintf("Address does not contain any witness outputs [%s].", request.params[1].get_str()));

    return extendwitnessaddresshelper(fundingAccount, unspentWitnessOutputs, pwallet, request);
}

static UniValue getactiveaccount(const JSONRPCRequest& request)
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
        throw std::runtime_error("Cannot use command without an active wallet");

    if (!pwallet->activeAccount)
        throw std::runtime_error("No account active");

    return getUUIDAsString(pwallet->activeAccount->getUUID());
}

static UniValue getreadonlyaccount(const JSONRPCRequest& request)
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
            "1. \"account\"        (required) The unique UUID or label for the account .\n"
            "\nResult:\n"
            "\nReturn the public key as an encoded string, that can be used with the \"importreadonlyaccount\" command.\n"
            "\nNB! it is important to be careful with and protect access to this public key as if it is compromised it can compromise security of your entire wallet, in cases where one or more child private keys are also compromised.\n"
            "\nExamples:\n"
            + HelpExampleCli("getreadonlyaccount \"My account\"", "")
            + HelpExampleRpc("getreadonlyaccount \"My account\"", ""));



    if (!pwallet)
        throw std::runtime_error("Cannot use command without an active wallet");

    CAccount* account = AccountFromValue(pwallet, request.params[0], false);

    if (!account->IsHD())
        throw std::runtime_error("Can only be used on a HD account.");

    CAccountHD* accountHD = dynamic_cast<CAccountHD*>(account);

    EnsureWalletIsUnlocked(pwallet);

    return accountHD->GetAccountMasterPubKeyEncoded().c_str();
}

static UniValue importreadonlyaccount(const JSONRPCRequest& request)
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
            "2. \"encodedkey\" (string) Encoded string containing the extended public key for the account.\n"
            "\nResult:\n"
            "\nReturn the UUID of the new account.\n"
            "\nExamples:\n"
            + HelpExampleCli("importreadonlyaccount \"Watch account\" \"dd3tNdQ8A4KqYvYVvXzGEU7ChdNye9RdTixnLSFqpQHG-2Rakbbkn7GDUTdD6wtSd5KV5PnCFgQt3FPc8eYkMonRM\"", "")
            + HelpExampleRpc("importreadonlyaccount \"Watch account\" \"dd3tNdQ8A4KqYvYVvXzGEU7ChdNye9RdTixnLSFqpQHG-2Rakbbkn7GDUTdD6wtSd5KV5PnCFgQt3FPc8eYkMonRM\"", ""));



    if (!pwallet)
        throw std::runtime_error("Cannot use command without an active wallet");

    EnsureWalletIsUnlocked(pwallet);

    CAccount* account = pwallet->CreateReadOnlyAccount(request.params[0].get_str().c_str(), request.params[1].get_str().c_str());

    if (!account)
        throw std::runtime_error("Unable to create account.");

    //fixme: (2.1) Use a timestamp here
    // Whenever a key is imported, we need to scan the whole chain - do so now
    pwallet->nTimeFirstKey = 1;
    boost::thread t(rescanThread); // thread runs free

    return getUUIDAsString(account->getUUID());
}

static UniValue getactiveseed(const JSONRPCRequest& request)
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
        throw std::runtime_error("Cannot use command without an active wallet");

    if (!pwallet->activeSeed)
        throw std::runtime_error("No seed active");

    return getUUIDAsString(pwallet->activeSeed->getUUID());
}

static UniValue setactiveaccount(const JSONRPCRequest& request)
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
            + HelpExampleCli("setactiveaccount \"My account\"", "")
            + HelpExampleRpc("setactiveaccount \"My account\"", ""));



    if (!pwallet)
        throw std::runtime_error("Cannot use command without an active wallet");

    CAccount* account = AccountFromValue(pwallet, request.params[0], false);

    pwallet->setActiveAccount(account);
    return getUUIDAsString(account->getUUID());
}

//fixme: (2.1) Improve help for this.
static UniValue getaccountbalances(const JSONRPCRequest& request)
{
    CWallet * const pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() > 2)
        throw std::runtime_error(
            "getaccountbalances ( minconf include_watchonly )\n"
            "Returns a list of balances for all accounts in the wallet.\n"
            "\nArguments:\n"
            "1. minconf           (numeric, optional, default=1) Only include transactions confirmed at least this many times.\n"
            "2. include_watchonly (boolean, optional, default=false) Also include balance in watch-only addresses (see 'importaddress')\n"
            "\nResult:\n"
            "amount              (numeric) The total amount in " + CURRENCY_UNIT + " received for this account.\n"
            "\nExamples:\n"
            "\nThe total amount in the wallet\n"
            + HelpExampleCli("getaccountbalances", "") +
            "\nThe total amount in the wallet at least 5 blocks confirmed\n"
            + HelpExampleCli("getaccountbalances", "6") +
            "\nAs a json rpc call\n"
            + HelpExampleRpc("getaccountbalances", "6")
        );

    DS_LOCK2(cs_main, pwallet->cs_wallet);

    int nMinDepth = 1;
    if (request.params.size() > 0)
        nMinDepth = request.params[1].get_int();

    bool includeWatchOnly = false;
    if (request.params.size() > 1)
        includeWatchOnly = request.params[1].get_bool();

    UniValue allAccounts(UniValue::VARR);

    //NB! - Intermediate AccountFromValue step is required in order to handle default account semantics.
    for (const auto& accountPair : pwallet->mapAccounts)
    {
        UniValue rec(UniValue::VOBJ);
        rec.push_back(Pair("UUID", getUUIDAsString(accountPair.first)));
        rec.push_back(Pair("label", accountPair.second->getLabel()));
        rec.push_back(Pair("balance", ValueFromAmount(pwallet->GetLegacyBalance(includeWatchOnly?ISMINE_ALL:ISMINE_SPENDABLE, nMinDepth, &accountPair.first))));
        allAccounts.push_back(rec);
    }

    return allAccounts;
}

static CHDSeed* SeedFromValue(CWallet* pwallet, const UniValue& value, bool useDefaultIfEmpty)
{
    std::string strSeedUUID = value.get_str();

    if (!pwallet)
        throw std::runtime_error("Cannot use command without an active wallet");

    if (strSeedUUID.empty())
    {
        if (!useDefaultIfEmpty || !pwallet->getActiveSeed())
        {
            throw std::runtime_error("No seed identifier passed, and no active seed selected, please select an active seed or pass a valid identifier.");
        }
        return pwallet->getActiveSeed();
    }

    boost::uuids::uuid seedUUID = getUUIDFromString(strSeedUUID);

    CHDSeed* foundSeed = NULL;
    if (pwallet->mapSeeds.find(seedUUID) != pwallet->mapSeeds.end())
    {
        foundSeed = pwallet->mapSeeds[seedUUID];
    }

    if (!foundSeed)
        throw JSONRPCError(RPC_WALLET_INVALID_ACCOUNT_NAME, "Not a valid seed UUID.");

    return foundSeed;
}

static UniValue setactiveseed(const JSONRPCRequest& request)
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
            + HelpExampleCli("setactiveseed \"827f0000-0300-0000-0000-000000000000\"", "")
            + HelpExampleRpc("setactiveseed \"827f0000-0300-0000-0000-000000000000\"", ""));



    if (!pwallet)
        throw std::runtime_error("Cannot use command without an active wallet");

    CHDSeed* seed = SeedFromValue(pwallet, request.params[0], false);

    pwallet->setActiveSeed(seed);
    return getUUIDAsString(seed->getUUID());
}


static UniValue createseed(const JSONRPCRequest& request)
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
        throw std::runtime_error("Cannot use command without an active wallet");

    EnsureWalletIsUnlocked(pwallet);

    CHDSeed::SeedType seedType = CHDSeed::CHDSeed::BIP44;
    if (request.params.size() > 0)
    {
        seedType = SeedTypeFromString(request.params[0].get_str());
    }

    CHDSeed* newSeed = pwallet->GenerateHDSeed(seedType);

    if(!newSeed)
        throw std::runtime_error("Failed to generate seed");

    return getUUIDAsString(newSeed->getUUID());
}

static UniValue deleteseed(const JSONRPCRequest& request)
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
            + HelpExampleCli("deleteseed \"827f0000-0300-0000-0000-000000000000\"", "")
            + HelpExampleRpc("deleteseed \"827f0000-0300-0000-0000-000000000000\"", ""));



    if (!pwallet)
        throw std::runtime_error("Cannot use command without an active wallet");

    CHDSeed* seed = SeedFromValue(pwallet, request.params[0], true);

    EnsureWalletIsUnlocked(pwallet);

    pwallet->DeleteSeed(seed, false);

    return true;
}

static UniValue importseed(const JSONRPCRequest& request)
{
    #ifdef ENABLE_WALLET
    CWallet * const pwallet = GetWalletForJSONRPCRequest(request);

    LOCK2(cs_main, pwallet ? &pwallet->cs_wallet : NULL);
    #else
    LOCK(cs_main);
    #endif
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp))
        return NullUniValue;

    if (request.fHelp || request.params.size() < 1 || request.params.size() > 3)
        throw std::runtime_error(
            "importseed \"mnemonic_or_pubkey\" \"type\" is_read_only \n"
            "\nSet the currently active seed by UUID.\n"
            "1. \"mnemonic_or_pubkey\"       (string) Specify the BIP44 mnemonic that will be used to generate the seed.\n"
            "2. \"type\"       (string, optional default=BIP44) Type of seed to create (BIP44; BIP44NH; BIP44E; BIP32; BIP32L)\n"
            "\nThe default is correct in almost all cases, only experts should work with the other types\n"
            "\nBIP44 - This is the standard Gulden seed type that should be used in almost all cases.\n"
            "\nBIP44NH - (No Hardening) This is the same as above, however with weakened security required for \"read only\" (watch) seed capability, use this only if you understand the implications and if you want to share your seed with another read only wallet.\n"
            "\nBIP44E - This is a modified BIP44 with a different hash value, required for compatibility with some external wallets (e.g. Coinomi).\n"
            "\nBIP32 - Older HD standard that was used by our mobile wallets before 1.6.0, use this to import/recover old mobile recovery phrases.\n"
            "\nBIP32L - (Legacy) Even older HD standard that was used by our first android wallets, use this to import/recover very old mobile recovery phrases.\n"
            "\nIn the case of read only seeds a pubkey rather than a mnemonic is required.\n"
            "3. is_read_only      (boolean, optional, default=false) Account is a 'read only account' - type argument will be ignored and always set to BIP44NH in this case. Wallet will be rescanned for transactions.\n"
            "\nResult:\n"
            "\nReturn the UUID of the new seed.\n"
            "\nExamples:\n"
            + HelpExampleCli("importseed \"green cliff good ghost orange charge cancel blue group interest walk yellow\"", "")
            + HelpExampleRpc("importseed \"green cliff good ghost orange charge cancel blue group interest walk yellow\"", ""));

    if (!pwallet)
        throw std::runtime_error("Cannot use command without an active wallet");

    EnsureWalletIsUnlocked(pwallet);

    CHDSeed::SeedType seedType = CHDSeed::CHDSeed::BIP44;
    if (request.params.size() > 1)
    {
        seedType = SeedTypeFromString(request.params[1].get_str());
    }

    bool fReadOnly = false;
    if (request.params.size() > 2)
        fReadOnly = request.params[2].get_bool();

    CHDSeed* newSeed = NULL;
    if (fReadOnly)
    {
        SecureString pubkeyString = request.params[0].get_str().c_str();
        newSeed = pwallet->ImportHDSeedFromPubkey(pubkeyString);
    }
    else
    {
        SecureString mnemonic = request.params[0].get_str().c_str();
        newSeed = pwallet->ImportHDSeed(mnemonic, seedType);
    }

    //fixme: (2.1) Use a timestamp here
    // Whenever a key is imported, we need to scan the whole chain - do so now
    pwallet->nTimeFirstKey = 1;
    boost::thread t(rescanThread); // thread runs free

    return getUUIDAsString(newSeed->getUUID());
}

static UniValue listallaccounts(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() > 2)
        throw std::runtime_error(
            "listaccounts \"seed\" \"state\"\n"
            "\nArguments:\n"
            "1. \"seed\"        (string, optional) The unique UUID for the seed that we want accounts of.\n"
            "2. \"state\"       (string, optional, default=Normal) The state of account to list, options are: Normal, Deleted, Shadow, ShadowChild. \"*\" or \"\" to list all account states.\n"
            "\nResult:\n"
            "\nReturn the UUID and label for all wallet accounts.\n"
            "\nNote UUID is guaranteed to be unique while label is not.\n"
            "\nExamples:\n"
            + HelpExampleCli("listaccounts \"827f0000-0300-0000-0000-000000000000\"", "")
            + HelpExampleRpc("listaccounts \"827f0000-0300-0000-0000-000000000000\"", ""));

    #ifdef ENABLE_WALLET
    CWallet * const pwallet = GetWalletForJSONRPCRequest(request);

    LOCK2(cs_main, pwallet ? &pwallet->cs_wallet : NULL);
    #else
    LOCK(cs_main);
    #endif

    if (!EnsureWalletIsAvailable(pwallet, request.fHelp))
        return NullUniValue;

    std::string sStateSearch = "*";

    CHDSeed* forSeed = NULL;
    if (request.params.size() > 0 && request.params[0].get_str() != "*")
        forSeed = SeedFromValue(pwallet, request.params[0], true);
    if (request.params.size() > 1)
        sStateSearch = request.params[1].get_str();
    if (sStateSearch.empty())
        sStateSearch = "*";

    if (!pwallet)
        throw std::runtime_error("Cannot use command without an active wallet");

    UniValue allAccounts(UniValue::VARR);

    for (const auto& accountPair : pwallet->mapAccounts)
    {
        if (sStateSearch != "*" && sStateSearch != GetAccountStateString(accountPair.second->m_State))
            continue;

        if (!accountPair.second->IsHD())
        {
            if (!forSeed)
            {
                UniValue rec(UniValue::VOBJ);
                rec.push_back(Pair("UUID", getUUIDAsString(accountPair.first)));
                rec.push_back(Pair("label", accountPair.second->getLabel()));
                rec.push_back(Pair("state", GetAccountStateString(accountPair.second->m_State)));
                rec.push_back(Pair("type", GetAccountTypeString(accountPair.second->m_Type)));
                rec.push_back(Pair("HD_type", "legacy"));
                allAccounts.push_back(rec);
            }
            continue;
        }
        if (accountPair.second->m_State == AccountState::Shadow && !(sStateSearch == "Shadow"||sStateSearch == "ShadowChild"))
            continue;
        if (forSeed && ((CAccountHD*)accountPair.second)->getSeedUUID() != forSeed->getUUID())
            continue;

        UniValue rec(UniValue::VOBJ);
        rec.push_back(Pair("UUID", getUUIDAsString(accountPair.first)));
        rec.push_back(Pair("label", accountPair.second->getLabel()));
        rec.push_back(Pair("state", GetAccountStateString(accountPair.second->m_State)));
        rec.push_back(Pair("type", GetAccountTypeString(accountPair.second->m_Type)));
        rec.push_back(Pair("HD_type", "HD"));
        rec.push_back(Pair("HDindex", (uint64_t) dynamic_cast<CAccountHD*>(accountPair.second)->getIndex()));

        allAccounts.push_back(rec);
    }

    return allAccounts;
}

static UniValue getmnemonicfromseed(const JSONRPCRequest& request)
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
            + HelpExampleCli("getmnemonicfromseed \"827f0000-0300-0000-0000-000000000000\"", "")
            + HelpExampleRpc("getmnemonicfromseed \"827f0000-0300-0000-0000-000000000000\"", ""));


    if (!pwallet)
        throw std::runtime_error("Cannot use command without an active wallet");

    CHDSeed* seed = SeedFromValue(pwallet, request.params[0], true);

    EnsureWalletIsUnlocked(pwallet);

    return seed->getMnemonic().c_str();
}

static UniValue getreadonlyseed(const JSONRPCRequest& request)
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
            + HelpExampleCli("getreadonlyseed \"827f0000-0300-0000-0000-000000000000\"", "")
            + HelpExampleRpc("getreadonlyseed \"827f0000-0300-0000-0000-000000000000\"", ""));

    if (!pwallet)
        throw std::runtime_error("Cannot use command without an active wallet");

    CHDSeed* seed = SeedFromValue(pwallet, request.params[0], true);

    if (seed->m_type != CHDSeed::SeedType::BIP44NoHardening)
        throw std::runtime_error("Can only use command with a non-hardened BIP44 seed");

    EnsureWalletIsUnlocked(pwallet);

    return seed->getPubkey().c_str();
}

static UniValue listseeds(const JSONRPCRequest& request)
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
        throw std::runtime_error("Cannot use command without an active wallet");

    UniValue AllSeeds(UniValue::VARR);

    for (const auto& seedPair : pwallet->mapSeeds)
    {
        UniValue rec(UniValue::VOBJ);
        rec.push_back(Pair("UUID", getUUIDAsString(seedPair.first)));
        rec.push_back(Pair("type", StringFromSeedType(seedPair.second)));
        if (seedPair.second->IsReadOnly())
            rec.push_back(Pair("readonly", "true"));
        AllSeeds.push_back(rec);
    }

    return AllSeeds;
}

static UniValue rotatewitnessaddresshelper(CAccount* fundingAccount, std::vector<std::tuple<CTxOut, uint64_t, COutPoint>> unspentWitnessOutputs, CWallet* pwallet)
{
    if (unspentWitnessOutputs.size() > 1)
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, strprintf("Too many witness outputs cannot rotate."));

    // Check for immaturity
    const auto& [currentWitnessTxOut, currentWitnessHeight, currentWitnessOutpoint] = unspentWitnessOutputs[0];
    //fixme: (2.1) - This check should go through the actual chain maturity stuff (via wtx) and not calculate directly.
    if (chainActive.Tip()->nHeight - currentWitnessHeight < (uint64_t)(COINBASE_MATURITY))
        throw JSONRPCError(RPC_MISC_ERROR, "Cannot perform operation on immature transaction, please wait for transaction to mature and try again");

    // Get the current witness details
    CTxOutPoW2Witness currentWitnessDetails;
    GetPow2WitnessOutput(currentWitnessTxOut, currentWitnessDetails);

    // Find the account for this address
    CAccount* witnessAccount = pwallet->FindAccountForTransaction(currentWitnessTxOut);
    if (!witnessAccount)
        throw JSONRPCError(RPC_MISC_ERROR, "Could not locate account for address");
    if ((!witnessAccount->IsPoW2Witness()) || witnessAccount->IsFixedKeyPool())
    {
        throw JSONRPCError(RPC_MISC_ERROR, "Cannot rotate a witness-only account as spend key is required to do this.");
    }

    //fixme: (2.1) factor this all out into a helper.
    // Finally attempt to create and send the witness transaction.
    CReserveKeyOrScript reservekey(pwallet, fundingAccount, KEYCHAIN_CHANGE);
    std::string reasonForFail;
    CAmount transactionFee;
    CMutableTransaction rotateWitnessTransaction(CTransaction::SEGSIG_ACTIVATION_VERSION);
    CTxOut rotatedWitnessTxOutput;
    {
        // Add the existing witness output as an input
        pwallet->AddTxInput(rotateWitnessTransaction, CInputCoin(currentWitnessOutpoint, currentWitnessTxOut), false);

        // Add new witness output
        rotatedWitnessTxOutput.SetType(CTxOutType::PoW2WitnessOutput);
        // As we are rotating the witness key we reset the "lock from" and we set the "lock until" everything else except the value remains unchanged.
        rotatedWitnessTxOutput.output.witnessDetails.lockFromBlock = currentWitnessDetails.lockFromBlock;
        rotatedWitnessTxOutput.output.witnessDetails.lockUntilBlock = currentWitnessDetails.lockUntilBlock;
        rotatedWitnessTxOutput.output.witnessDetails.spendingKeyID = currentWitnessDetails.spendingKeyID;
        {
            CReserveKeyOrScript keyWitness(pactiveWallet, witnessAccount, KEYCHAIN_WITNESS);
            CPubKey pubWitnessKey;
            if (!keyWitness.GetReservedKey(pubWitnessKey))
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Error allocating witness key for witness account.");

            //We delibritely return the key here, so that if we fail we won't leak the key.
            //The key will be marked as used when the transaction is accepted anyway.
            keyWitness.ReturnKey();
            rotatedWitnessTxOutput.output.witnessDetails.witnessKeyID = pubWitnessKey.GetID();
        }
        rotatedWitnessTxOutput.output.witnessDetails.failCount = currentWitnessDetails.failCount;
        rotatedWitnessTxOutput.output.witnessDetails.actionNonce = currentWitnessDetails.actionNonce+1;
        rotatedWitnessTxOutput.nValue = currentWitnessTxOut.nValue;
        rotateWitnessTransaction.vout.push_back(rotatedWitnessTxOutput);

        // Fund the additional amount in the transaction (including fees)
        int changeOutputPosition = 1;
        std::set<int> subtractFeeFromOutputs; // Empty - we don't subtract fee from outputs
        CCoinControl coincontrol;
        if (!pwallet->FundTransaction(fundingAccount, rotateWitnessTransaction, transactionFee, changeOutputPosition, reasonForFail, false, subtractFeeFromOutputs, coincontrol, reservekey))
        {
            throw JSONRPCError(RPC_MISC_ERROR, strprintf("Failed to fund transaction [%s]", reasonForFail.c_str()));
        }
    }

    //fixme: (2.1) Improve this, it should only happen if Sign transaction is a success.
    //Also We must make sure that the UI version of this command does the same
    //Also (low) this shares common code with CreateTransaction() - it should be factored out into a common helper.
    CKey privWitnessKey;
    if (!witnessAccount->GetKey(rotatedWitnessTxOutput.output.witnessDetails.witnessKeyID, privWitnessKey))
    {
        //fixme: (2.1) Localise
        reasonForFail = strprintf("Wallet error, failed to retrieve private witness key.");
        throw JSONRPCError(RPC_MISC_ERROR, strprintf("Failed to fund transaction [%s]", reasonForFail.c_str()));
        return false;
    }
    if (!pwallet->AddKeyPubKey(privWitnessKey, privWitnessKey.GetPubKey(), *witnessAccount, KEYCHAIN_WITNESS))
    {
        //fixme: (2.1) Localise
        reasonForFail = strprintf("Wallet error, failed to store witness key.");
        throw JSONRPCError(RPC_MISC_ERROR, strprintf("Failed to fund transaction [%s]", reasonForFail.c_str()));
        return false;
    }

    uint256 finalTransactionHash;
    {
        LOCK2(cs_main, pactiveWallet->cs_wallet);
        if (!pwallet->SignAndSubmitTransaction(reservekey, rotateWitnessTransaction, reasonForFail, &finalTransactionHash))
        {
            throw JSONRPCError(RPC_MISC_ERROR, strprintf("Failed to commit transaction [%s]", reasonForFail.c_str()));
        }
    }

    UniValue result(UniValue::VOBJ);
    result.push_back(Pair("txid", finalTransactionHash.GetHex()));
    result.push_back(Pair("fee_amount", ValueFromAmount(transactionFee)));
    return result;
}

static UniValue rotatewitnessaddress(const JSONRPCRequest& request)
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
            "rotatewitnessaddress \"funding_account\" \"witness_address\" \n"
            "\nChange the \"witnessing key\" of a witness account, the wallet needs to be unlocked to do this, the \"spending key\" will remain unchanged. \n"
            "1. \"funding_account\"  (string, required) The unique UUID or label for the account from which money will be removed to pay for the transaction fee.\n"
            "2. \"witness_address\"  (string, required) The Gulden address for the witness key.\n"
            "\nResult:\n"
            "[\n"
            "     \"txid\":\"txid\",   (string) The txid of the created transaction\n"
            "     \"fee_amount\":n   (number) The fee that was paid.\n"
            "]\n"
            "\nExamples:\n"
            + HelpExampleCli("rotatewitnessaddress \"My account\" 2ZnFwkJyYeEftAoQDe7PC96t2Y7XMmKdNtekRdtx32GNQRJztULieFRFwQoQqN", "")
            + HelpExampleRpc("rotatewitnessaddress \"My account\" 2ZnFwkJyYeEftAoQDe7PC96t2Y7XMmKdNtekRdtx32GNQRJztULieFRFwQoQqN", ""));

    // Basic sanity checks.
    if (!pwallet)
        throw std::runtime_error("Cannot use command without an active wallet");
    if (!IsSegSigEnabled(chainActive.TipPrev()))
        throw std::runtime_error("Cannot use this command before segsig activates");

    EnsureWalletIsUnlocked(pwallet);

    // arg1 - 'from' account.
    CAccount* fundingAccount = AccountFromValue(pwallet, request.params[0], false);
    if (!fundingAccount)
        throw std::runtime_error(strprintf("Unable to locate funding account [%s].",  request.params[0].get_str()));

    // arg2 - 'to' address.
    CGuldenAddress witnessAddress(request.params[1].get_str());
    bool isValid = witnessAddress.IsValidWitness(Params());

    if (!isValid)
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, strprintf("Not a valid witness address [%s].", request.params[1].get_str()));

    const auto& unspentWitnessOutputs = getCurrentOutputsForWitnessAddress(witnessAddress);
    if (unspentWitnessOutputs.size() == 0)
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, strprintf("Address does not contain any witness outputs [%s].", request.params[1].get_str()));

    return rotatewitnessaddresshelper(fundingAccount, unspentWitnessOutputs, pwallet);
}

static UniValue rotatewitnessaccount(const JSONRPCRequest& request)
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
            "rotatewitnessaccount \"funding_account\" \"witness_account\" \n"
            "\nChange the \"witnessing key\" of a witness account, the wallet needs to be unlocked to do this, the \"spending key\" will remain unchanged. \n"
            "1. \"funding_account\"  (string, required) The unique UUID or label for the account from which money will be removed to pay for the transaction fee.\n"
            "2. \"witness_account\"  (string, required) The unique UUID or label for the witness account that will hold the locked funds.\n"
            "\nResult:\n"
            "[\n"
            "     \"txid\":\"txid\",   (string) The txid of the created transaction\n"
            "     \"fee_amount\":n   (number) The fee that was paid.\n"
            "]\n"
            "\nExamples:\n"
            + HelpExampleCli("rotatewitnessaddress \"My account\" \"My witness account\"", "")
            + HelpExampleRpc("rotatewitnessaddress \"My account\" \"My witness account\"", ""));

    // Basic sanity checks.
    if (!pwallet)
        throw std::runtime_error("Cannot use command without an active wallet");
    if (!IsSegSigEnabled(chainActive.TipPrev()))
        throw std::runtime_error("Cannot use this command before segsig activates");

    EnsureWalletIsUnlocked(pwallet);

    // arg1 - 'from' account.
    CAccount* fundingAccount = AccountFromValue(pwallet, request.params[0], false);
    if (!fundingAccount)
        throw std::runtime_error(strprintf("Unable to locate funding account [%s].",  request.params[0].get_str()));

    // arg2 - 'to' account.
    CAccount* witnessAccount = AccountFromValue(pwallet, request.params[1], false);
    if (!witnessAccount)
        throw std::runtime_error(strprintf("Unable to locate witness account [%s].",  request.params[1].get_str()));

    if ((!witnessAccount->IsPoW2Witness()) || witnessAccount->IsFixedKeyPool())
    {
        throw JSONRPCError(RPC_MISC_ERROR, "Cannot rotate a witness-only account as spend key is required to do this.");
    }

    const auto& unspentWitnessOutputs = getCurrentOutputsForWitnessAccount(witnessAccount);
    if (unspentWitnessOutputs.size() == 0)
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, strprintf("Address does not contain any witness outputs [%s].", request.params[1].get_str()));

    return rotatewitnessaddresshelper(fundingAccount, unspentWitnessOutputs, pwallet);
}

static UniValue renewwitnessaccount(const JSONRPCRequest& request)
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
            "renewwitnessaccount \"witness_account\" \n"
            "\nRenew an expired witness account. \n"
            "1. \"funding_account\"        (required) The unique UUID or label for the account.\n"
            "2. \"witness_account\"        (required) The unique UUID or label for the account.\n"
            "\nResult:\n"
            "[\n"
            "     \"txid\",                (string) The txid of the created transaction\n"
            "     \"fee_amount\"           (number) The fee that was paid.\n"
            "]\n"
            "\nExamples:\n"
            + HelpExampleCli("renewwitnessaccount \"My account\" \"My witness account\"", "")
            + HelpExampleRpc("renewwitnessaccount \"My account\" \"My witness account\"", ""));

    // Basic sanity checks.
    if (!pwallet)
        throw std::runtime_error("Cannot use command without an active wallet");
    if (!IsSegSigEnabled(chainActive.TipPrev()))
        throw std::runtime_error("Cannot use this command before segsig activates");

    EnsureWalletIsUnlocked(pwallet);

    // arg1 - 'from' account.
    CAccount* fundingAccount = AccountFromValue(pwallet, request.params[0], false);
    if (!fundingAccount)
        throw std::runtime_error(strprintf("Unable to locate funding account [%s].",  request.params[0].get_str()));

    // arg2 - 'to' account.
    CAccount* witnessAccount = AccountFromValue(pwallet, request.params[1], false);
    if (!fundingAccount)
        throw std::runtime_error(strprintf("Unable to locate funding account [%s].",  request.params[0].get_str()));

    if ((!witnessAccount->IsPoW2Witness()) || witnessAccount->IsFixedKeyPool())
    {
        throw JSONRPCError(RPC_MISC_ERROR, "Cannot split a witness-only account as spend key is required to do this.");
    }

    //fixme: (2.1) - Share common code with GUI::requestRenewWitness
    std::string strError;
    CMutableTransaction tx(CURRENT_TX_VERSION_POW2);
    CReserveKeyOrScript changeReserveKey(pactiveWallet, fundingAccount, KEYCHAIN_EXTERNAL);
    CAmount transactionFee;
    if (!pactiveWallet->PrepareRenewWitnessAccountTransaction(fundingAccount, witnessAccount, changeReserveKey, tx, transactionFee, strError))
    {
        throw std::runtime_error(strprintf("Failed to create renew transaction [%s]", strError.c_str()));
    }

    uint256 finalTransactionHash;
    {
        LOCK2(cs_main, pactiveWallet->cs_wallet);
        if (!pactiveWallet->SignAndSubmitTransaction(changeReserveKey, tx, strError, &finalTransactionHash))
        {
            throw std::runtime_error(strprintf("Failed to sign renew transaction [%s]", strError.c_str()));
        }
    }

    // Clear the failed flag in UI, and remove the 'renew' button for immediate user feedback.
    witnessAccount->SetWarningState(AccountStatus::WitnessPending);
    static_cast<const CGuldenWallet*>(pactiveWallet)->NotifyAccountWarningChanged(pactiveWallet, witnessAccount);

    UniValue result(UniValue::VOBJ);
    result.push_back(Pair(finalTransactionHash.GetHex(), ValueFromAmount(transactionFee)));
    return result;
}

static UniValue splitwitnessaccount(const JSONRPCRequest& request)
{
    #ifdef ENABLE_WALLET
    CWallet * const pwallet = GetWalletForJSONRPCRequest(request);
    LOCK2(cs_main, pwallet ? &pwallet->cs_wallet : NULL);
    #else
    LOCK(cs_main);
    #endif

    if (!EnsureWalletIsAvailable(pwallet, request.fHelp))
        return NullUniValue;

    if (request.fHelp || request.params.size() != 3)
        throw std::runtime_error(
            "splitwitnessaccount \"funding_account\" \"witness_account\" \"amounts\" \n"
            "\nSplit a witness address into two seperate witness addresses, all details of the addresses remain identical other than a reduction in amounts.\n"
            "\nThis is useful in the event that an account has exceeded 1 percent of the network weight. \n"
            "1. \"funding_account\"        (required) The unique UUID or label for the account.\n"
            "2. \"witness_account\"        (required) The unique UUID or label for the account.\n"
            "3. \"amounts\"                (string, required) A json object with amounts for the new addresses\n"
            "    {\n"
            "      \"amount\"              (numeric) The amount that should go in each new account\n"
            "      ,...\n"
            "    }\n"
            "\nResult:\n"
            "[\n"
            "     \"txid\",                (string) The txid of the created transaction\n"
            "     \"fee_amount\"           (number) The fee that was paid.\n"
            "]\n"
            "\nExamples:\n"
            + HelpExampleCli("splitwitnessaccount \"My account\" \"My witness account\"  [10000, 5000, 5000]", "")
            + HelpExampleRpc("splitwitnessaccount \"My account\" \"My witness account\"  [10000, 5000, 5000]", ""));

    // Basic sanity checks.
    if (!pwallet)
        throw std::runtime_error("Cannot use command without an active wallet");
    if (!IsSegSigEnabled(chainActive.TipPrev()))
        throw std::runtime_error("Cannot use this command before segsig activates");

    EnsureWalletIsUnlocked(pwallet);

    // arg1 - 'from' account.
    CAccount* fundingAccount = AccountFromValue(pwallet, request.params[0], false);
    if (!fundingAccount)
        throw std::runtime_error(strprintf("Unable to locate funding account [%s].",  request.params[0].get_str()));

    // arg2 - 'to' account.
    CAccount* witnessAccount = AccountFromValue(pwallet, request.params[1], false);
    if (!fundingAccount)
        throw std::runtime_error(strprintf("Unable to locate funding account [%s].",  request.params[1].get_str()));

    if ((!witnessAccount->IsPoW2Witness()) || witnessAccount->IsFixedKeyPool())
    {
        throw JSONRPCError(RPC_MISC_ERROR, "Cannot split a witness-only account as spend key is required to do this.");
    }

    // arg3 - 'split' paramaters
    std::vector<UniValue> splitInto = request.params[2].getValues();
    if (splitInto.size() < 2)
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Split command requires at least two outputs");

    const auto& unspentWitnessOutputs = getCurrentOutputsForWitnessAccount(witnessAccount);
    if (unspentWitnessOutputs.size() == 0)
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, strprintf("Account does not contain any witness outputs [%s].", request.params[1].get_str()));

    //fixme: (2.1) - Handle address
    if (unspentWitnessOutputs.size() > 1)
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, strprintf("Account has too many witness outputs cannot split [%s].", request.params[1].get_str()));

    // Check for immaturity
    const auto& [currentWitnessTxOut, currentWitnessHeight, currentWitnessOutpoint] = unspentWitnessOutputs[0];
    //fixme: (2.1) - This check should go through the actual chain maturity stuff (via wtx) and not calculate directly.
    if (chainActive.Tip()->nHeight - currentWitnessHeight < (uint64_t)(COINBASE_MATURITY))
        throw JSONRPCError(RPC_MISC_ERROR, "Cannot perform operation on immature transaction, please wait for transaction to mature and try again");

    CAmount splitTotal=0;
    std::vector<CAmount> splitAmounts;
    for (const auto& unparsedSplitAmount : splitInto)
    {
        CAmount splitValue = AmountFromValue(unparsedSplitAmount);
        splitTotal += splitValue;
        splitAmounts.emplace_back(splitValue);
    }

    if (splitTotal != currentWitnessTxOut.nValue)
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, strprintf("Split values don't match original value [%s] [%s]", FormatMoney(splitTotal), FormatMoney(currentWitnessTxOut.nValue)));

    // Get the current witness details
    CTxOutPoW2Witness currentWitnessDetails;
    GetPow2WitnessOutput(currentWitnessTxOut, currentWitnessDetails);

    //fixme: (2.1) factor this all out into a helper.
    // Finally attempt to create and send the witness transaction.
    CReserveKeyOrScript reservekey(pwallet, fundingAccount, KEYCHAIN_CHANGE);
    std::string reasonForFail;
    CAmount transactionFee;
    CMutableTransaction splitWitnessTransaction(CTransaction::SEGSIG_ACTIVATION_VERSION);
    {
        // Add the existing witness output as an input
        pwallet->AddTxInput(splitWitnessTransaction, CInputCoin(currentWitnessOutpoint, currentWitnessTxOut), false);

        // Add new witness outputs
        for (const CAmount& splitAmount : splitAmounts)
        {
            CTxOut splitWitnessTxOutput;
            splitWitnessTxOutput.SetType(CTxOutType::PoW2WitnessOutput);
            // As we are splitting the amount, only the amount may change.
            splitWitnessTxOutput.output.witnessDetails.lockFromBlock = currentWitnessDetails.lockFromBlock;
            splitWitnessTxOutput.output.witnessDetails.lockUntilBlock = currentWitnessDetails.lockUntilBlock;
            splitWitnessTxOutput.output.witnessDetails.spendingKeyID = currentWitnessDetails.spendingKeyID;
            splitWitnessTxOutput.output.witnessDetails.witnessKeyID = currentWitnessDetails.witnessKeyID;
            splitWitnessTxOutput.output.witnessDetails.failCount = currentWitnessDetails.failCount;
            splitWitnessTxOutput.output.witnessDetails.actionNonce = currentWitnessDetails.actionNonce+1;
            splitWitnessTxOutput.nValue = splitAmount;
            splitWitnessTransaction.vout.push_back(splitWitnessTxOutput);
        }

        // Fund the additional amount in the transaction (including fees)
        int changeOutputPosition = 1;
        std::set<int> subtractFeeFromOutputs; // Empty - we don't subtract fee from outputs
        CCoinControl coincontrol;
        if (!pwallet->FundTransaction(fundingAccount, splitWitnessTransaction, transactionFee, changeOutputPosition, reasonForFail, false, subtractFeeFromOutputs, coincontrol, reservekey))
        {
            throw JSONRPCError(RPC_MISC_ERROR, strprintf("Failed to fund transaction [%s]", reasonForFail.c_str()));
        }
    }

    uint256 finalTransactionHash;
    {
        LOCK2(cs_main, pactiveWallet->cs_wallet);
        if (!pwallet->SignAndSubmitTransaction(reservekey, splitWitnessTransaction, reasonForFail, &finalTransactionHash))
        {
            throw JSONRPCError(RPC_MISC_ERROR, strprintf("Failed to commit transaction [%s]", reasonForFail.c_str()));
        }
    }

    UniValue result(UniValue::VOBJ);
    result.push_back(Pair(finalTransactionHash.GetHex(), ValueFromAmount(transactionFee)));
    return result;
}

static UniValue mergewitnessaccount(const JSONRPCRequest& request)
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
            "mergewitnessaccount \"funding_account\" \"witness_account\"\n"
            "\nMerge multiple witness addresses into a single one.\n"
            "\nAddresses must share identical characteristics other than \"amount\" and therefore this will usually only work on addresses that were created via \"splitwitnessaccount\".\n"
            "\nThis is useful in the event that the network weight has risen significantly and an account that was previously split could now earn better as a single account. \n"
            "1. \"funding_account\"        (required) The unique UUID or label for the account.\n"
            "2. \"witness_account\"        (required) The unique UUID or label for the account.\n"
            "\nResult:\n"
            "[\n"
            "     \"txid\",                (string) The txid of the created transaction\n"
            "     \"fee_amount\"           (number) The fee that was paid.\n"
            "]\n"
            "\nExamples:\n"
            + HelpExampleCli("mergewitnessaccount \"My account\" \"My witness account\"", "")
            + HelpExampleRpc("mergewitnessaccount \"My account\" \"My witness account\"", ""));

    // Basic sanity checks.
    if (!pwallet)
        throw std::runtime_error("Cannot use command without an active wallet");
    if (!IsSegSigEnabled(chainActive.TipPrev()))
        throw std::runtime_error("Cannot use this command before segsig activates");

    EnsureWalletIsUnlocked(pwallet);

    // arg1 - 'from' account.
    CAccount* fundingAccount = AccountFromValue(pwallet, request.params[0], false);
    if (!fundingAccount)
        throw std::runtime_error(strprintf("Unable to locate funding account [%s].",  request.params[0].get_str()));

    // arg2 - 'to' account.
    CAccount* witnessAccount = AccountFromValue(pwallet, request.params[1], false);
    if (!fundingAccount)
        throw std::runtime_error(strprintf("Unable to locate funding account [%s].",  request.params[0].get_str()));

    if ((!witnessAccount->IsPoW2Witness()) || witnessAccount->IsFixedKeyPool())
    {
        throw JSONRPCError(RPC_MISC_ERROR, "Cannot split a witness-only account as spend key is required to do this.");
    }

    const auto& unspentWitnessOutputs = getCurrentOutputsForWitnessAccount(witnessAccount);
    if (unspentWitnessOutputs.size() == 0)
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, strprintf("Account does not contain any witness outputs [%s].", request.params[1].get_str()));

    if (unspentWitnessOutputs.size() == 1)
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, strprintf("Account only contains one witness output at least two are required to merge [%s].", request.params[1].get_str()));

    // Check for immaturity
    for ( const auto& [currentWitnessTxOut, currentWitnessHeight, currentWitnessOutpoint] : unspentWitnessOutputs )
    {
        (unused) currentWitnessTxOut;
        (unused) currentWitnessOutpoint;
        //fixme: (2.1) - This check should go through the actual chain maturity stuff (via wtx) and not calculate directly.
        if (chainActive.Tip()->nHeight - currentWitnessHeight < (uint64_t)(COINBASE_MATURITY))
            throw JSONRPCError(RPC_MISC_ERROR, "Cannot perform operation on immature transaction, please wait for transaction to mature and try again");
    }

    const auto& [currentWitnessTxOut, currentWitnessHeight, currentWitnessOutpoint] = unspentWitnessOutputs[0];
    (unused) currentWitnessHeight;
    // Get the current witness details
    CTxOutPoW2Witness currentWitnessDetails;
    GetPow2WitnessOutput(currentWitnessTxOut, currentWitnessDetails);

    //fixme: (2.1) factor this all out into a helper.
    // Finally attempt to create and send the witness transaction.
    CReserveKeyOrScript reservekey(pwallet, fundingAccount, KEYCHAIN_CHANGE);
    std::string reasonForFail;
    CAmount transactionFee;
    CMutableTransaction mergeWitnessTransaction(CTransaction::SEGSIG_ACTIVATION_VERSION);
    {
        // Add all the existing witness outputs as inputs and sum the fail count.
        uint64_t totalFailCount = currentWitnessDetails.failCount;
        uint64_t highestActionNonce = currentWitnessDetails.actionNonce;
        CAmount totalAmount = currentWitnessTxOut.nValue;
        pwallet->AddTxInput(mergeWitnessTransaction, CInputCoin(currentWitnessOutpoint, currentWitnessTxOut), false);
        for (unsigned int i = 1; i < unspentWitnessOutputs.size(); ++i)
        {
            const auto& [compareWitnessTxOut, compareWitnessHeight, compareWitnessOutpoint] = unspentWitnessOutputs[i];
            (unused) compareWitnessHeight;
            CTxOutPoW2Witness compareWitnessDetails;
            GetPow2WitnessOutput(compareWitnessTxOut, compareWitnessDetails);
            if(    compareWitnessDetails.lockFromBlock != currentWitnessDetails.lockFromBlock
                || compareWitnessDetails.lockUntilBlock != currentWitnessDetails.lockUntilBlock
                || compareWitnessDetails.spendingKeyID != currentWitnessDetails.spendingKeyID
                || compareWitnessDetails.witnessKeyID != currentWitnessDetails.witnessKeyID)
            {
                throw JSONRPCError(RPC_MISC_ERROR, "Not all inputs share identical witness characteristics, cannot merge.");
            }
            totalFailCount += compareWitnessDetails.failCount;
            highestActionNonce = std::max(highestActionNonce, compareWitnessDetails.actionNonce);
            totalAmount += compareWitnessTxOut.nValue;
            pwallet->AddTxInput(mergeWitnessTransaction, CInputCoin(compareWitnessOutpoint, compareWitnessTxOut), false);
        }

        CTxOut mergeWitnessTxOutput;
        mergeWitnessTxOutput.SetType(CTxOutType::PoW2WitnessOutput);
        // As we are splitting the amount, only the amount may change and fail count must match the total of all accounts.
        mergeWitnessTxOutput.output.witnessDetails.lockFromBlock = currentWitnessDetails.lockFromBlock;
        mergeWitnessTxOutput.output.witnessDetails.lockUntilBlock = currentWitnessDetails.lockUntilBlock;
        mergeWitnessTxOutput.output.witnessDetails.spendingKeyID = currentWitnessDetails.spendingKeyID;
        mergeWitnessTxOutput.output.witnessDetails.witnessKeyID = currentWitnessDetails.witnessKeyID;
        mergeWitnessTxOutput.output.witnessDetails.failCount = totalFailCount;
        mergeWitnessTxOutput.output.witnessDetails.actionNonce = highestActionNonce+1;
        mergeWitnessTxOutput.nValue = totalAmount;
        mergeWitnessTransaction.vout.push_back(mergeWitnessTxOutput);

        // Fund the additional amount in the transaction (including fees)
        int changeOutputPosition = 1;
        std::set<int> subtractFeeFromOutputs; // Empty - we don't subtract fee from outputs
        CCoinControl coincontrol;
        if (!pwallet->FundTransaction(fundingAccount, mergeWitnessTransaction, transactionFee, changeOutputPosition, reasonForFail, false, subtractFeeFromOutputs, coincontrol, reservekey))
        {
            throw JSONRPCError(RPC_MISC_ERROR, strprintf("Failed to fund transaction [%s]", reasonForFail.c_str()));
        }
    }

    uint256 finalTransactionHash;
    {
        LOCK2(cs_main, pactiveWallet->cs_wallet);
        if (!pwallet->SignAndSubmitTransaction(reservekey, mergeWitnessTransaction, reasonForFail, &finalTransactionHash))
        {
            throw JSONRPCError(RPC_MISC_ERROR, strprintf("Failed to commit transaction [%s]", reasonForFail.c_str()));
        }
    }

    UniValue result(UniValue::VOBJ);
    result.push_back(Pair(finalTransactionHash.GetHex(), ValueFromAmount(transactionFee)));
    return result;
}

static UniValue setwitnesscompound(const JSONRPCRequest& request)
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
            "setwitnesscompound \"witness_account\" \"amount\"\n"
            "\nSet whether a witness account should compound or not.\n"
            "\nCompounding is controlled as follows:\n"
            "\n    1) When set to 0 no compounding will be done, all rewards will be sent to the non-compound output set by \"setwitnessgeneration\" or a key in the account if \"setwitnessgeneration\" has not been called.\n"
            "\n    2) When set to a positive number \"n\", earnings up until \"n\" will be compounded, and the remainder will be sent to the non-compound output (as describe in 1).\n"
            "\n    3) When set to a negative number \"n\", \"n\" will be deducted and sent to a non-compound output (as described in 1) and the remainder will be compounded.\n"
            "\nIn all cases it is important to remember the following:\n"
            "\n    4) Transaction fees and not just the witness reward can be compounded, so while the witness reward is 20 NLG compounding amount should be set considering possible transaction fees as well.\n"
            "\n    5) A maximum of 40 NLG can be compounded, regardless of whether a block contains more fees. In the event that there is additional fees to distribute after applying the compunding settings, the settings will be ignored for the additional fees and paid to a non-compound output (as described in 1)\n"
            "\nArguments:\n"
            "1. \"witness_account\"            (string) The UUID or unique label of the account.\n"
            "2. amount                        (numeric or string, required) The amount in " + CURRENCY_UNIT + "\n"
            "\nResult:\n"
            "[\n"
            "     \"account_uuid\",            (string) The UUID of the account that has been modified.\n"
            "     \"amount\"                   (string) The amount that has been set.\n"
            "]\n"
            "\nExamples:\n"
            + HelpExampleCli("setwitnesscompound \"My witness account\" 20", "")
            + HelpExampleRpc("setwitnesscompound \"My witness account\" 20", ""));

    if (!pwallet)
        throw std::runtime_error("Cannot use command without an active wallet");

    CAccount* forAccount = AccountFromValue(pwallet, request.params[0], false);
    if (!forAccount)
        throw std::runtime_error("Invalid account name or UUID");

    if (!forAccount->IsPoW2Witness())
        throw std::runtime_error(strprintf("Specified account is not a witness account [%s].",  request.params[0].get_str()));

    CAmount amount = AmountFromValue(request.params[1], true);

    CWalletDB walletdb(*pwallet->dbw);
    forAccount->setCompounding(amount, &walletdb);

    UniValue result(UniValue::VOBJ);
    result.push_back(Pair(getUUIDAsString(forAccount->getUUID()), ValueFromAmount(forAccount->getCompounding())));
    return result;
}

static UniValue getwitnesscompound(const JSONRPCRequest& request)
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
            "getwitnesscompound \"witness_account\"\n"
            "\nGet the current compound setting for an account.\n"
            "\nArguments:\n"
            "1. \"witness_account\"        (string) The UUID or unique label of the account.\n"
            "\nResult:\n"
            "\nReturn the current amount set for the account.\n"
            "\nCompounding is controlled as follows:\n"
            "\n    1) When set to 0 no compounding will be done, all rewards will be sent to the non-compound output set by \"setwitnessgeneration\" or a key in the account if \"setwitnessgeneration\" has not been called.\n"
            "\n    2) When set to a positive number \"n\", earnings up until \"n\" will be compounded, and the remainder will be sent to the non-compound output (as describe in 1).\n"
            "\n    3) When set to a negative number \"n\", \"n\" will be deducted and sent to a non-compound output (as described in 1) and the remainder will be compounded.\n"
            "\nIn all cases it is important to remember the following:\n"
            "\n    4) Transaction fees and not just the witness reward can be compounded, so while the witness reward is 20 NLG compounding amount should be set considering possible transaction fees as well.\n"
            "\n    5) A maximum of 40 NLG can be compounded, regardless of whether a block contains more fees. In the event that there is additional fees to distribute after applying the compunding settings, the settings will be ignored for the additional fees and paid to a non-compound output (as described in 1)\n"
            "\nExamples:\n"
            + HelpExampleCli("getwitnesscompound \"My witness account\"", "")
            + HelpExampleRpc("getwitnesscompound \"My witness account\"", ""));

    if (!pwallet)
        throw std::runtime_error("Cannot use command without an active wallet");

    CAccount* forAccount = AccountFromValue(pwallet, request.params[0], false);
    if (!forAccount)
        throw std::runtime_error("Invalid account name or UUID");

    if (!forAccount->IsPoW2Witness())
        throw std::runtime_error(strprintf("Specified account is not a witness account [%s].",  request.params[0].get_str()));

    return ValueFromAmount(forAccount->getCompounding());
}

static UniValue setwitnessrewardscript(const JSONRPCRequest& request)
{
    #ifdef ENABLE_WALLET
    CWallet * const pwallet = GetWalletForJSONRPCRequest(request);
    LOCK2(cs_main, pwallet ? &pwallet->cs_wallet : NULL);
    #else
    LOCK(cs_main);
    #endif

    if (!EnsureWalletIsAvailable(pwallet, request.fHelp))
        return NullUniValue;

    if (request.fHelp || request.params.size() < 2 || request.params.size() > 3 )
        throw std::runtime_error(
            "setwitnessrewardscript \"witness_account\" \"address_or_script\" force_pubkey \n"
            "\nSet the output key into which all non-compound witness earnings will be paid.\n"
            "\nSee \"setwitnesscompound\" for how to control compounding and additional information.\n"
            "1. \"witness_account\"        (required) The unique UUID or label for the account.\n"
            "2. \"pubkey_or_script\"       (required) An hex encoded script or public key.\n"
            "3. force_pubkey              (boolean, optional, default=false) Cause command to fail if an invalid pubkey is passed, without this the pubkey may be imported as a script.\n"
            "\nResult:\n"
            "[\n"
            "     \"account_uuid\",        (string) The UUID of the account that has been modified.\n"
            "     \"amount\"               (string) The amount that has been set.\n"
            "]\n"
            "\nExamples:\n"
            + HelpExampleCli("setwitnessrewardscript \"my witness account\" \"Vb5YMjiTA9BUYi9zPToKg3wAAdrpHNp1V2hSBVHpgLMm9sPojhnX\"", "")
            + HelpExampleRpc("setwitnessrewardscript \"my witness account\" \"Vb5YMjiTA9BUYi9zPToKg3wAAdrpHNp1V2hSBVHpgLMm9sPojhnX\"", ""));

    CAccount* forAccount = AccountFromValue(pwallet, request.params[0], false);

    if (!forAccount)
        throw std::runtime_error("Invalid account name or UUID");

    if (!forAccount->IsPoW2Witness())
        throw std::runtime_error(strprintf("Specified account is not a witness account [%s].",  request.params[0].get_str()));


    bool forcePubKey = false;
    if (request.params.size() > 2)
        forcePubKey = request.params[2].get_bool();

    std::string pubKeyOrScript = request.params[1].get_str();
    CScript scriptForNonCompoundPayments;
    if (!IsHex(pubKeyOrScript))
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Data is not hex encoded");

    // Try public key first.
    std::vector<unsigned char> data(ParseHex(pubKeyOrScript));
    CPubKey pubKey(data.begin(), data.end());
    if (pubKey.IsFullyValid())
    {
        scriptForNonCompoundPayments = CScript() << ToByteVector(pubKey) << OP_CHECKSIG;
    }
    else
    {
        if (forcePubKey)
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Not a valid hex encoded public key");

        // Not a public key so treat it as a script.
        scriptForNonCompoundPayments = CScript(data.begin(), data.end());
        if (!scriptForNonCompoundPayments.HasValidOps())
        {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Data is hex encoded, but not a valid pubkey or script");
        }
    }

    CWalletDB walletdb(*pwallet->dbw);
    forAccount->setNonCompoundRewardScript(scriptForNonCompoundPayments, &walletdb);

    witnessScriptsAreDirty = true;

    UniValue result(UniValue::VOBJ);
    result.push_back(Pair(getUUIDAsString(forAccount->getUUID()), HexStr(forAccount->getNonCompoundRewardScript())));
    return result;
}

static UniValue getwitnessrewardscript(const JSONRPCRequest& request)
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
            "getwitnessrewardscript \"witness_account\" \n"
            "\nGet the output key into which all non-compound witness earnings will be paid.\n"
            "\nSee \"getwitnesscompound\" for how to control compounding and additional information.\n"
            "1. \"witness_account\"    (required) The unique UUID or label for the account.\n"
            "\nResult:\n"
            "[\n"
            "     \"account_uuid\",    (string) The UUID of the account that has been modified.\n"
            "     \"hex_script\"       (string) Hex encoded script that has been set.\n"
            "]\n"
            "\nExamples:\n"
            + HelpExampleCli("setwitnessrewardscript \"my witness account\"", "")
            + HelpExampleRpc("setwitnessrewardscript \"my witness account\"", ""));

    CAccount* forAccount = AccountFromValue(pwallet, request.params[0], false);

    if (!forAccount)
        throw std::runtime_error("Invalid account name or UUID");

    if (!forAccount->IsPoW2Witness())
        throw std::runtime_error(strprintf("Specified account is not a witness account [%s].",  request.params[0].get_str()));

    UniValue result(UniValue::VOBJ);
    if (!forAccount->hasNonCompoundRewardScript())
    {
        result.push_back(Pair(getUUIDAsString(forAccount->getUUID()), ""));
    }
    else
    {
        result.push_back(Pair(getUUIDAsString(forAccount->getUUID()), HexStr(forAccount->getNonCompoundRewardScript())));
    }
    return result;
}

static UniValue getwitnessaccountkeys(const JSONRPCRequest& request)
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
            "getwitnessaccountkeys \"witness_account\" \n"
            "\nGet the witness keys of an HD account, this can be used to import the account as a witness only account in another wallet via the \"importwitnesskeys\" command.\n"
            "\nA single account can theoretically contain multiple keys, if it has been split \"splitwitnessaccount\", this will include all of them \n"
            "1. \"witness_account\"        (required) The unique UUID or label for the account.\n"
            "\nResult:\n"
            "\nReturn the private witness keys as an encoded string, that can be used with the \"importwitnesskeys\" command.\n"
            "\nNB! The exported private key is only the \"witnessing\" key and not the \"spending\" key for the witness account.\n"
            "\nIf the \"witness\" key is compromised your funds will remain completely safe however the attacker will be able to use the key to claim your earnings.\n"
            "\nIf you believe your key is or may have been compromised use \"rotatewitnessaccount\" to rotate to a new witness key.\n"
            "\nExamples:\n"
            + HelpExampleCli("getwitnessaccountkeys \"My witness account\"", "")
            + HelpExampleRpc("getwitnessaccountkeys \"My witness account\"", ""));

    CAccount* forAccount = AccountFromValue(pwallet, request.params[0], false);

    if (!forAccount)
        throw std::runtime_error("Invalid account name or UUID");

    if (!forAccount->IsPoW2Witness())
        throw std::runtime_error("Can only be used on a witness account.");

    if (!chainActive.Tip())
        throw std::runtime_error("Wait for chain to synchronise before using command.");

    if (!IsPow2WitnessingActive(chainActive.Tip(), Params(), chainActive, nullptr))
        throw std::runtime_error("Wait for witnessing to activate before using this command.");

    EnsureWalletIsUnlocked(pwallet);

    std::map<COutPoint, Coin> allWitnessCoins;
    if (!getAllUnspentWitnessCoins(chainActive, Params(), chainActive.Tip(), allWitnessCoins))
        throw std::runtime_error("Failed to enumerate all witness coins.");

    std::string witnessAccountKeys = "";
    for (const auto& [witnessOutPoint, witnessCoin] : allWitnessCoins)
    {
        (unused)witnessOutPoint;
        CTxOutPoW2Witness witnessDetails;
        GetPow2WitnessOutput(witnessCoin.out, witnessDetails);
        if (forAccount->HaveKey(witnessDetails.witnessKeyID))
        {
            CKey witnessPrivKey;
            if (!forAccount->GetKey(witnessDetails.witnessKeyID, witnessPrivKey))
            {
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Unable to retrieve key for witness account.");
            }
            //fixme: (2.1) - to be 100% correct we should export the creation time of the actual key (where available) and not getEarliestPossibleCreationTime - however getEarliestPossibleCreationTime will do for now.
            witnessAccountKeys += CGuldenSecret(witnessPrivKey).ToString() + strprintf("#%s", forAccount->getEarliestPossibleCreationTime());
            witnessAccountKeys += ":";
        }
    }
    if (witnessAccountKeys.empty())
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Witness account has no active keys.");

    witnessAccountKeys.pop_back();
    witnessAccountKeys = "gulden://witnesskeys?keys=" + witnessAccountKeys;
    return witnessAccountKeys;
}

static UniValue getwitnessaddresskeys(const JSONRPCRequest& request)
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
            "getwitnessaddresskeys \"witness_address\" \n"
            "\nGet the witness key of an HD address, this can be used to import the account as a witness only account in another wallet via the \"importwitnesskeys\" command.\n"
            "1. \"witness_address\"        (required) The Gulden address for the witness key.\n"
            "\nResult:\n"
            "\nReturn the private witness key as an encoded string, that can be used with the \"importwitnesskeys\" command.\n"
            "\nNB! The exported private key is only the \"witnessing\" key and not the \"spending\" key for the witness account.\n"
            "\nIf the \"witness\" key is compromised your funds will remain completely safe however the attacker will be able to use the key to claim your earnings.\n"
            "\nIf you believe your key is or may have been compromised use \"rotatewitnessaccount\" to rotate to a new witness key.\n"
            "\nExamples:\n"
            + HelpExampleCli("getwitnessaddresskeys \"2ZnFwkJyYeEftAoQDe7PC96t2Y7XMmKdNtekRdtx32GNQRJztULieFRFwQoQqN\"", "")
            + HelpExampleRpc("getwitnessaddresskeys \"2ZnFwkJyYeEftAoQDe7PC96t2Y7XMmKdNtekRdtx32GNQRJztULieFRFwQoQqN\"", ""));

    CGuldenAddress forAddress(request.params[0].get_str());
    bool isValid = forAddress.IsValidWitness(Params());

    if (!isValid)
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Not a valid witness address.");

    EnsureWalletIsUnlocked(pwallet);

    std::string witnessAccountKeys = "";
    for (const auto& [accountUUID, forAccount] : pwallet->mapAccounts)
    {
        (unused)accountUUID;
        CPoW2WitnessDestination dest = boost::get<CPoW2WitnessDestination>(forAddress.Get());
        if (forAccount->HaveKey(dest.witnessKey))
        {
            CKey witnessPrivKey;
            if (!forAccount->GetKey(dest.witnessKey, witnessPrivKey))
            {
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Unable to retrieve key for witness address.");
            }
            //fixme: (2.1) - to be 100% correct we should export the creation time of the actual key (where available) and not getEarliestPossibleCreationTime - however getEarliestPossibleCreationTime will do for now.
            witnessAccountKeys += CGuldenSecret(witnessPrivKey).ToString() + strprintf("#%s", forAccount->getEarliestPossibleCreationTime());
            break;
        }
    }
    if (witnessAccountKeys.empty())
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Witness account has no active keys.");

    witnessAccountKeys = "gulden://witnesskeys?keys=" + witnessAccountKeys;
    return witnessAccountKeys;
}

static UniValue importwitnesskeys(const JSONRPCRequest& request)
{
    #ifdef ENABLE_WALLET
    CWallet * const pwallet = GetWalletForJSONRPCRequest(request);
    LOCK2(cs_main, pwallet ? &pwallet->cs_wallet : NULL);
    #else
    LOCK(cs_main);
    #endif
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp))
        return NullUniValue;

    if (request.fHelp || request.params.size() < 2 || request.params.size() > 3)
        throw std::runtime_error(
            "importwitnesskeys \"account_name\" \"encoded_key_url\" \"create_account\" \n"
            "\nAdd keys imported from an \"encoded_key_url\" which has been obtained by using \"getwitnessaddresskeys\" or \"getwitnessaccountkeys\" \n"
            "\nUses an existing account if \"create_account\" is false, otherwise creates a new one. \n"
            "1. \"account_name\"            (string) name/label of the new account.\n"
            "2. \"encoded_key_url\"         (string) Encoded string containing the extended public key for the account.\n"
            "3. \"create_account\"          (boolean, optional, default=false) Encoded string containing the extended public key for the account.\n"
            "\nResult:\n"
            "\nReturn the UUID of account.\n"
            "\nExamples:\n"
            + HelpExampleCli("importwitnesskeys \"my witness account\" \"gulden://witnesskeys?keys=Vd69eLAZ2r76C47xB3pDLa9Fx4Li8Xt5AHgzjJDuLbkP8eqUjToC#1529049773\"", "")
            + HelpExampleRpc("importwitnesskeys \"my witness account\" \"gulden://witnesskeys?keys=Vd69eLAZ2r76C47xB3pDLa9Fx4Li8Xt5AHgzjJDuLbkP8eqUjToC#1529049773\"", ""));

    if (!pwallet)
        throw std::runtime_error("Cannot use command without an active wallet");

    EnsureWalletIsUnlocked(pwallet);

    bool shouldCreateAccount = false;
    if (request.params.size() > 2)
        shouldCreateAccount = request.params[2].get_bool();

    const auto& keysAndBirthDates = pwallet->ParseWitnessKeyURL(request.params[1].get_str().c_str());
    if (keysAndBirthDates.empty())
        throw std::runtime_error("Invalid encoded key URL");

    CAccount* account = nullptr;
    if (shouldCreateAccount)
    {
        account = AccountFromValue(pwallet, request.params[0], false);
        if (!account)
            throw std::runtime_error("Invalid account name or UUID");
        if (account->m_Type != WitnessOnlyWitnessAccount)
            throw std::runtime_error("Account is not a witness-only account");

        if (!pwallet->ImportKeysIntoWitnessOnlyWitnessAccount(account, keysAndBirthDates))
            throw std::runtime_error("Failed to import keys into account");
    }
    else
    {
        std::string requestedAccountName = request.params[0].get_str();
        account = pwallet->CreateWitnessOnlyWitnessAccount(requestedAccountName, keysAndBirthDates);
        if (!account)
            throw std::runtime_error("Failed to create witness-only witness account");
    }

    //NB! No need to trigger a rescan CreateWitnessOnlyWitnessAccount already did this.

    return getUUIDAsString(account->getUUID());
}


static const CRPCCommand commands[] =
{ //  category                   name                               actor (function)                 okSafeMode
  //  ---------------------      ------------------------           -----------------------          ----------
    { "mining",                  "gethashps",                       &gethashps,                      true,    {} },
    { "mining",                  "sethashlimit",                    &sethashlimit,                   true,    {"limit"} },

    //fixme: (2.1) Many of these belong in accounts category as well.
    //We should consider allowing multiple categories for commands, so its easier for people to discover commands under specific topics they are interested in.
    { "witness",                 "createwitnessaccount",            &createwitnessaccount,           true,    {"name"} },
    { "witness",                 "extendwitnessaccount",            &extendwitnessaccount,           true,    {"funding_account", "witness_account", "amount", "time" } },
    { "witness",                 "extendwitnessaddress",            &extendwitnessaddress,           true,    {"funding_account", "witness_address", "amount", "time" } },
    { "witness",                 "fundwitnessaccount",              &fundwitnessaccount,             true,    {"funding_account", "witness_account", "amount", "time", "force_multiple" } },
    { "witness",                 "getwitnessaccountkeys",           &getwitnessaccountkeys,          true,    {"witness_account"} },
    { "witness",                 "getwitnessaddresskeys",           &getwitnessaddresskeys,          true,    {"witness_address"} },
    { "witness",                 "getwitnesscompound",              &getwitnesscompound,             true,    {"witness_account"} },
    { "witness",                 "getwitnessinfo",                  &getwitnessinfo,                 true,    {"block_specifier", "verbose", "mine_only"} },
    { "witness",                 "getwitnessrewardscript",          &getwitnessrewardscript,         true,    {"witness_account"} },
    { "witness",                 "importwitnesskeys",               &importwitnesskeys,              true,    {"account_name", "encoded_key_url", "create_account"} },
    { "witness",                 "mergewitnessaccount",             &mergewitnessaccount,            true,    {"funding_account", "witness_account"} },
    { "witness",                 "rotatewitnessaddress",            &rotatewitnessaddress,           true,    {"funding_account", "witness_address"} },
    { "witness",                 "rotatewitnessaccount",            &rotatewitnessaccount,           true,    {"funding_account", "witness_account"} },
    { "witness",                 "renewwitnessaccount",             &renewwitnessaccount,            true,    {"funding_account", "witness_account"} },
    { "witness",                 "setwitnesscompound",              &setwitnesscompound,             true,    {"witness_account", "amount"} },
    { "witness",                 "setwitnessrewardscript",          &setwitnessrewardscript,         true,    {"witness_account", "pubkey_or_script", "force_pubkey"} },
    { "witness",                 "splitwitnessaccount",             &splitwitnessaccount,            true,    {"funding_account", "witness_account", "amounts"} },

    { "developer",               "dumpblockgaps",                   &dumpblockgaps,                  true,    {"start_height", "count"} },
    { "developer",               "dumptransactionstats",            &dumptransactionstats,           true,    {"start_height", "count"} },
    { "developer",               "dumpdiffarray",                   &dumpdiffarray,                  true,    {"height"} },

    { "accounts",                "changeaccountname",               &changeaccountname,              true,    {"account", "name"} },
    { "accounts",                "createaccount",                   &createaccount,                  true,    {"name", "type"} },
    { "accounts",                "deleteaccount",                   &deleteaccount,                  true,    {"account", "force"} },
    { "accounts",                "getactiveaccount",                &getactiveaccount,               true,    {} },
    { "accounts",                "getreadonlyaccount",              &getreadonlyaccount,             true,    {"account"} },
    { "accounts",                "importreadonlyaccount",           &importreadonlyaccount,          true,    {"name", "encoded_key"} },
    { "accounts",                "listaccounts",                    &listallaccounts,                true,    {"seed", "state"} },
    { "accounts",                "setactiveaccount",                &setactiveaccount,               true,    {"account"} },
    { "accounts",                "getaccountbalances",              &getaccountbalances,             false,   {"min_conf", "include_watchonly"} },

    { "mnemonics",               "createseed",                      &createseed,                     true,    {"type"} },
    { "mnemonics",               "deleteseed",                      &deleteseed,                     true,    {"seed"} },
    { "mnemonics",               "getactiveseed",                   &getactiveseed,                  true,    {} },
    { "mnemonics",               "getmnemonicfromseed",             &getmnemonicfromseed,            true,    {"seed"} },
    { "mnemonics",               "getreadonlyseed",                 &getreadonlyseed,                true,    {"seed"} },
    { "mnemonics",               "setactiveseed",                   &setactiveseed,                  true,    {"seed"} },
    { "mnemonics",               "importseed",                      &importseed,                     true,    {"mnemonic_or_pubkey", "type", "is_read_only"} },
    { "mnemonics",               "listseeds",                       &listseeds,                      true,    {} },
};

void RegisterGuldenRPCCommands(CRPCTable &t)
{
    for (unsigned int vcidx = 0; vcidx < ARRAYLEN(commands); vcidx++)
        t.appendCommand(commands[vcidx].name, &commands[vcidx]);
}

#endif


