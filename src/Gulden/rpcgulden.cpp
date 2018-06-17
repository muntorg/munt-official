// Copyright (c) 2015-2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#include "rpcgulden.h"
#include "../miner.h"
#include <rpc/server.h>
#include <wallet/rpcwallet.h>
#include "validation/validation.h"
#include "validation/witnessvalidation.h"

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

#include <Gulden/util.h>

#include <consensus/validation.h>
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
    if (request.fHelp || request.params.size() > 2)
        throw std::runtime_error(
            "getwitnessinfo \"blockspecifier\" verbose\n"
            "\nReturns statistics and witness related info for a block:"
            "\nNumber of witnesses."
            "\nOverall network weight."
            "\nCurrent phase of PoW2 activation.\n"
            "\nAdditional information when verbose is on:"
            "\nList of all witness addresses with individual weights.\n"
            "\nArguments:\n"
            "1. \"blockspecifier\" (string, optional, default=tip) The blockspecifier for which to display witness information, if empty or 'tip' the tip of the current chain is used.\n"
            "blockspecifier can be the hash of the block; an absolute height in the blockchain or a tip~# specifier to iterate backwards from tip; for which to return witness details\n"
            "2. verbose            (bool, optional, default=false) Display additional verbose information.\n"
            "\nExamples:\n"
            + HelpExampleCli("getwitnessinfo tip false", "")
            + HelpExampleCli("getwitnessinfo tip~2 true", "")
            + HelpExampleCli("getwitnessinfo 400000 true", "")
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

    if (request.params.size() == 2)
    {
        fVerbose = request.params[1].get_bool() ? true : false;
    }

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
            if (!ReadBlockFromDisk(block, pTipIndex_, Params().GetConsensus()))
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
            {
                auto poolIter = witInfo.witnessSelectionPoolFiltered.begin();
                while (poolIter != witInfo.witnessSelectionPoolFiltered.end())
                {
                    if (poolIter->outpoint == iter.first)
                    {
                        if (poolIter->coin.out == iter.second.out)
                        {
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

            bool fLockPeriodExpired = nLockUntilBlock < pTipIndex_->nHeight;

            UniValue rec(UniValue::VOBJ);
            rec.push_back(Pair("type", iter.second.out.GetTypeAsString()));
            rec.push_back(Pair("address", CGuldenAddress(address).ToString()));
            rec.push_back(Pair("age", nAge));
            rec.push_back(Pair("amount", ValueFromAmount(nValue)));
            rec.push_back(Pair("weight", nRawWeight));
            rec.push_back(Pair("expected_witness_period", expectedWitnessBlockPeriod(nRawWeight, witInfo.nTotalWeight)));
            rec.push_back(Pair("estimated_witness_period", estimatedWitnessBlockPeriod(nRawWeight, witInfo.nTotalWeight)));
            rec.push_back(Pair("last_active_block", nLastActiveBlock));
            rec.push_back(Pair("lock_from_block", nLockFromBlock));
            rec.push_back(Pair("lock_until_block", nLockUntilBlock));
            rec.push_back(Pair("lock_period", nLockPeriodInBlocks));
            rec.push_back(Pair("lock_period_expired", fLockPeriodExpired));
            rec.push_back(Pair("eligible_to_witness", fEligible));
            rec.push_back(Pair("expired_from_inactivity", fExpired));
            #ifdef ENABLE_WALLET
            rec.push_back(Pair("ismine_accountname", accountNameForAddress(*pwallet, address)));
            #else
            rec.push_back(Pair("ismine_accountname", ""));
            #endif

            witnessWeightStats(nRawWeight);
            lockPeriodWeightStats(nLockPeriodInBlocks);
            witnessAmountStats(nValue);
            ageStats(nAge);

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


static UniValue dumptransactionstats(const JSONRPCRequest& request)
{
    CWallet * const pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp))
        return NullUniValue;

    if (request.fHelp || request.params.size() != 2)
        throw std::runtime_error(
            "dumptransactionstats startheight count\n"
            "\nDump the transaction stats for the last n blocks.\n"
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
        if (ReadBlockFromDisk(block, pBlock, Params().GetConsensus()))
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
            + HelpExampleCli("changeaccountname", "")
            + HelpExampleRpc("changeaccountname", ""));



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
            "deleteaccount \"account\" (force)\n"
            "\nDelete an account.\n"
            "\nArguments:\n"
            "1. \"account\"        (string) The UUID or unique label of the account.\n"
            "2. \"force\"          (string) Specify string force to force deletion of a non-empty account.\n"
            "\nExamples:\n"
            + HelpExampleCli("deleteaccount", "")
            + HelpExampleRpc("deleteaccount", ""));



    if (!pwallet)
        throw std::runtime_error("Cannot use command without an active wallet");

    CAccount* account = AccountFromValue(pwallet, request.params[0], false);

    if (request.params.size() == 1 || request.params[1].get_str() != "force")
    {
        boost::uuids::uuid accountUUID = account->getUUID();
        CAmount balance = pwallet->GetLegacyBalance(ISMINE_SPENDABLE, 0, &accountUUID );
        if (balance > MINIMUM_VALUABLE_AMOUNT && !account->IsReadOnly())
        {
            throw std::runtime_error("Account not empty, please first empty your account before trying to delete it.");
        }
    }

    pwallet->deleteAccount(account);
    return true;
}


static UniValue createaccounthelper(CWallet* pwallet, std::string accountName, std::string accountType, bool bMakeActive=true)
{
    CAccount* account = NULL;

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
        EnsureWalletIsUnlocked(pwallet);
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
            + HelpExampleCli("createaccount", "")
            + HelpExampleRpc("createaccount", ""));



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
            + HelpExampleCli("createwitnessaccount", "")
            + HelpExampleRpc("createwitnessaccount", ""));

    if (!pwallet)
        throw std::runtime_error("Cannot use command without an active wallet");

    if (GetPoW2Phase(chainActive.Tip(), Params(), chainActive) < 2)
        throw std::runtime_error("Cannot create witness accounts before phase 2 activates.");

    return createaccounthelper(pwallet, request.params[0].get_str(), "Witness", false);
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

    if (request.fHelp || request.params.size() != 4)
        throw std::runtime_error(
            "fundwitnessaccount \"fundingaccount\" \"witnessaccount\" \"amount\" \"time\" \n"
            "Lock \"amount\" NLG in witness account \"witnessaccount\" for time period \"time\"\n"
            "\"fundingaccount\" is the account from which the locked funds will be claimed.\n"
            "\"time\" may be a minimum of 1 month and a maximum of 3 years.\n"
            "\nArguments:\n"
            "1. \"fundingaccount\"  (string, required) The unique UUID or label for the account from which money will be removed. Use \"\" for the active account or \"*\" for all accounts to be considered.\n"
            "2. \"witnessaccount\"  (string, required) The unique UUID or label for the witness account that will hold the locked funds.\n"
            "3. \"amount\"          (string, required) The amount of NLG to hold locked in the witness account. Minimum amount of 5000 NLG is allowed.\n"
            "4. \"time\"            (string, required) The time period for which the funds should be locked in the witness account. By default this is interpreted as blocks e.g. \"1000\", prefix with \"y\", \"m\", \"w\", \"d\", \"b\" to specifically work in years, months, weeks, days or blocks.\n"
            "\nResult:\n"
            "\"txid\"               (string) The transaction id.\n"
            "\nExamples:\n"
            + HelpExampleCli("fundwitnessaccount \"mysavingsaccount\" \"mywitnessaccount\" \"10000\" \"2y\"", "")
            + HelpExampleRpc("fundwitnessaccount \"mysavingsaccount\" \"mywitnessaccount\" \"10000\" \"2y\"", ""));

    int nPoW2TipPhase = GetPoW2Phase(chainActive.Tip(), Params(), chainActive);

    // Basic sanity checks.
    if (!pwallet)
        throw std::runtime_error("Cannot use command without an active wallet");
    if (nPoW2TipPhase < 2)
        throw std::runtime_error("Cannot fund witness accounts before phase 2 activates.");

    // arg1 - 'from' account.
    CAccount* fundingAccount = AccountFromValue(pwallet, request.params[0], true);
    if (!fundingAccount)
        throw std::runtime_error(strprintf("Unable to locate funding account [%s].",  request.params[0].get_str()));

    // arg2 - 'to' account.
    CAccount* witnessAccount = AccountFromValue(pwallet, request.params[1], true);
    if (!witnessAccount)
        throw std::runtime_error(strprintf("Unable to locate witness account [%s].",  request.params[1].get_str()));
    if (!witnessAccount->IsPoW2Witness())
        throw std::runtime_error(strprintf("Specified account is not a witness account [%s].",  request.params[1].get_str()));
    if (witnessAccount->m_Type == WitnessOnlyWitnessAccount || !witnessAccount->IsHD())
        throw std::runtime_error(strprintf("Cannot fund a witness only witness account [%s].",  request.params[1].get_str()));

    // arg3 - amount
    CAmount nAmount =  AmountFromValue(request.params[2]);
    if (nAmount < nMinimumWitnessAmount*COIN)
        throw JSONRPCError(RPC_TYPE_ERROR, strprintf("Witness amount must be %d or larger", nMinimumWitnessAmount));

    // arg4 - lock period.
    // Calculate lock period based on suffix (if one is present) otherwise leave as is.
    int nLockPeriodInBlocks = 0;
    int nMultiplier = 1;
    std::string sLockPeriodInBlocks = request.params[3].getValStr();
    if (boost::algorithm::ends_with(sLockPeriodInBlocks, "y"))
    {
        nMultiplier = 365 * 576;
        sLockPeriodInBlocks.pop_back();
    }
    else if (boost::algorithm::ends_with(sLockPeriodInBlocks, "m"))
    {
        nMultiplier = 30 * 576;
        sLockPeriodInBlocks.pop_back();
    }
    else if (boost::algorithm::ends_with(sLockPeriodInBlocks, "w"))
    {
        nMultiplier = 7 * 576;
        sLockPeriodInBlocks.pop_back();
    }
    else if (boost::algorithm::ends_with(sLockPeriodInBlocks, "d"))
    {
        nMultiplier = 576;
        sLockPeriodInBlocks.pop_back();
    }
    else if (boost::algorithm::ends_with(sLockPeriodInBlocks, "b"))
    {
        nMultiplier = 1;
        sLockPeriodInBlocks.pop_back();
    }
    if (!ParseInt32(sLockPeriodInBlocks, &nLockPeriodInBlocks))
        throw JSONRPCError(RPC_TYPE_ERROR, "Invalid number passed for lock period.");
    nLockPeriodInBlocks *=  nMultiplier;

    if (nLockPeriodInBlocks > 3 * 365 * 576)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Maximum lock period of 3 years exceeded.");

    if (nLockPeriodInBlocks < 30 * 576)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Minimum lock period of 1 month exceeded.");

    // Add a small buffer to give us time to enter the blockchain
    if (nLockPeriodInBlocks == 30 * 576)
        nLockPeriodInBlocks += 50;

    // Enforce minimum weight
    int64_t nWeight = GetPoW2RawWeightForAmount(nAmount, nLockPeriodInBlocks);
    if (nWeight < nMinimumWitnessWeight)
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "PoW2 witness has insufficient weight.");
    }

    EnsureWalletIsUnlocked(pwallet);

    // Finally attempt to create and send the witness transaction.
    CPoW2WitnessDestination destinationPoW2Witness;
    destinationPoW2Witness.lockFromBlock = 0;
    destinationPoW2Witness.lockUntilBlock = chainActive.Tip()->nHeight + nLockPeriodInBlocks;
    destinationPoW2Witness.failCount = 0;
    {
        CReserveKeyOrScript keyWitness(pactiveWallet, witnessAccount, KEYCHAIN_WITNESS);
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
        CReserveKeyOrScript keySpending(pactiveWallet, witnessAccount, KEYCHAIN_SPENDING);
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

    return wtx.GetHash().GetHex();
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
            + HelpExampleCli("getreadonlyaccount", "")
            + HelpExampleRpc("getreadonlyaccount", ""));



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
            + HelpExampleCli("importreadonlyaccount \"watcher\" \"dd3tNdQ8A4KqYvYVvXzGEU7ChdNye9RdTixnLSFqpQHG-2Rakbbkn7GDUTdD6wtSd5KV5PnCFgQt3FPc8eYkMonRM\"", "")
            + HelpExampleRpc("importreadonlyaccount \"watcher\" \"dd3tNdQ8A4KqYvYVvXzGEU7ChdNye9RdTixnLSFqpQHG-2Rakbbkn7GDUTdD6wtSd5KV5PnCFgQt3FPc8eYkMonRM\"", ""));



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
            + HelpExampleCli("setactiveaccount", "")
            + HelpExampleRpc("setactiveaccount", ""));



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
            "2. include_watchonly (bool, optional, default=false) Also include balance in watch-only addresses (see 'importaddress')\n"
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
            + HelpExampleCli("setactiveseed", "")
            + HelpExampleRpc("setactiveseed", ""));



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
            + HelpExampleCli("deleteseed", "")
            + HelpExampleRpc("deleteseed", ""));



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
            "importseed \"mnemonic or pubkey\" \"read only\" \n"
            "\nSet the currently active seed by UUID.\n"
            "1. \"mnemonic or pubkey\"       (string) Specify the BIP44 mnemonic that will be used to generate the seed.\n"
            "2. \"type\"       (string, optional default=BIP44) Type of seed to create (BIP44; BIP44NH; BIP44E; BIP32; BIP32L)\n"
            "\nThe default is correct in almost all cases, only experts should work with the other types\n"
            "\nBIP44 - This is the standard Gulden seed type that should be used in almost all cases.\n"
            "\nBIP44NH - (No Hardening) This is the same as above, however with weakened security required for \"read only\" (watch) seed capability, use this only if you understand the implications and if you want to share your seed with another read only wallet.\n"
            "\nBIP44E - This is a modified BIP44 with a different hash value, required for compatibility with some external wallets (e.g. Coinomi).\n"
            "\nBIP32 - Older HD standard that was used by our mobile wallets before 1.6.0, use this to import/recover old mobile recovery phrases.\n"
            "\nBIP32L - (Legacy) Even older HD standard that was used by our first android wallets, use this to import/recover very old mobile recovery phrases.\n"
            "\nIn the case of read only seeds a pubkey rather than a mnemonic is required.\n"
            "3. \"read only\"      (boolean, optional, default=false) Account is a 'read only account' - type argument will be ignored and always set to BIP44NH in this case. Wallet will be rescanned for transactions.\n"
            "\nResult:\n"
            "\nReturn the UUID of the new seed.\n"
            "\nExamples:\n"
            + HelpExampleCli("importseed", "")
            + HelpExampleRpc("importseed", ""));

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
            "listaccounts ( forseed )\n"
            "\nArguments:\n"
            "1. \"seed\"        (string, optional) The unique UUID for the seed that we want accounts of.\n"
            "2. \"state\"       (string, optional, default=Normal) The state of account to list, options are: Normal, Deleted, Shadow, ShadowChild. \"*\" or \"\" to list all account states.\n"
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
                rec.push_back(Pair("type", ""));
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
            + HelpExampleCli("getmnemonicfromseed", "")
            + HelpExampleRpc("getmnemonicfromseed", ""));


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
            + HelpExampleCli("getreadonlyseed", "")
            + HelpExampleRpc("getreadonlyseed", ""));

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

    if (request.fHelp || request.params.size() != 1)
        throw std::runtime_error(
            "rotatewitnessaddress \"address\" \n"
            "\nChange the \"witnessing key\" of a witness account, the wallet needs to be unlocked to do this, the \"spending key\" will remain unchanged. \n"
            "1. \"address\"        (required) The unique UUID or label for the account.\n"
            "\nResult:\n"
            "\nReturns the new witness address.\n"
            "\nExamples:\n"
            + HelpExampleCli("rotatewitnessaddress 2ZnFwkJyYeEftAoQDe7PC96t2Y7XMmKdNtekRdtx32GNQRJztULieFRFwQoQqN", "")
            + HelpExampleRpc("rotatewitnessaddress 2ZnFwkJyYeEftAoQDe7PC96t2Y7XMmKdNtekRdtx32GNQRJztULieFRFwQoQqN", ""));

    CGuldenAddress forAddress(request.params[0].get_str());
    bool isValid = forAddress.IsValidWitness(Params());

    if (!isValid)
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Not a valid witness address.");

    //fixme: (2.0) implement
    return "Not yet implemented, please check back in next release";
}

static UniValue splitwitnessaddress(const JSONRPCRequest& request)
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
            "splitwitnessaddress \"address\" \"amounts\" \n"
            "\nSplit a witness address into two seperate witness addresses, all details of the addresses remain identical other than a reduction in amounts.\n"
            "\nThis is useful in the event that an account has exceeded 1 percent of the network weight. \n"
            "1. \"address\"        (required) The unique UUID or label for the account.\n"
            "2. \"amounts\"        (string, required) A json object with amounts for the new addresses\n"
            "    {\n"
            "      \"amount\"      (numeric) The amount that should go in each new account\n"
            "      ,...\n"
            "    }\n"
            "\nResult:\n"
            "\nReturns the new witness addresses.\n"
            "\nExamples:\n"
            + HelpExampleCli("splitwitnessaddress 2ZnFwkJyYeEftAoQDe7PC96t2Y7XMmKdNtekRdtx32GNQRJztULieFRFwQoQqN {10000, 5000, 5000}", "")
            + HelpExampleRpc("splitwitnessaddress 2ZnFwkJyYeEftAoQDe7PC96t2Y7XMmKdNtekRdtx32GNQRJztULieFRFwQoQqN {10000, 5000, 5000}", ""));

    CGuldenAddress forAddress(request.params[0].get_str());
    bool isValid = forAddress.IsValidWitness(Params());

    if (!isValid)
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Not a valid witness address.");

    UniValue splitInto = request.params[1].get_obj();
    if (splitInto.getValues().size() < 2)
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Split command requires at least two outputs");

    //fixme: (2.0) implement
    return "Not yet implemented, please check back in next release";
}

static UniValue mergewitnessaddresses(const JSONRPCRequest& request)
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
            "mergewitnessaddresses \"addresses\" \n"
            "\nMerge multiple witness addresses into a single one.\n"
            "\nAddresses must share identical characteristics other than \"amount\" and therefore this will usually only work on addresses that were created via \"splitwitnessaddress\".\n"
            "\nThis is useful in the event that the network weight has risen significantly and an account that was previously split could now earn better as a single account. \n"
            "1. \"addresses\"      (string, required) A json object with amounts for the new addresses\n"
            "    {\n"
            "      \"address\"     (string) The address that should be merged\n"
            "      ,...\n"
            "    }\n"
            "\nResult:\n"
            "\nReturns the new witness address.\n"
            "\nExamples:\n"
            + HelpExampleCli("mergewitnessaddresses {\"2ZnFwkJyYeEftAoQDe7PC96t2Y7XMmKdNtekRdtx32GNQRJztULieFRFwQoQqN\", \"2a8srWupARTwNnzBe67CrAejXYg4QwNvyEr2mQkbdTuD8BqUwL6rAEx2XTTvCo}\"", "")
            + HelpExampleRpc("mergewitnessaddresses {\"2ZnFwkJyYeEftAoQDe7PC96t2Y7XMmKdNtekRdtx32GNQRJztULieFRFwQoQqN\", \"2a8srWupARTwNnzBe67CrAejXYg4QwNvyEr2mQkbdTuD8BqUwL6rAEx2XTTvCo}\"", ""));

    UniValue mergeFrom = request.params[0].get_obj();
    if (mergeFrom.getValues().size() < 2)
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Merge command requires at least two inputs");

    for(const auto& addressObj : mergeFrom.getValues())
    {
        CGuldenAddress forAddress(addressObj.get_str());
        bool isValid = forAddress.IsValidWitness(Params());

        if (!isValid)
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Not a valid witness address.");
    }

    //fixme: (2.0) implement
    return "Not yet implemented, please check back in next release";
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
            "setwitnesscompound \"account\" \"amount\"\n"
            "\nSet whether a witness account should compound or not.\n"
            "\nCompounding is controlled as follows:\n"
            "\n    1) When set to 0 no compounding will be done, all rewards will be sent to the non-compound output set by \"setwitnessgeneration\" or a key in the account if \"setwitnessgeneration\" has not been called.\n"
            "\n    2) When set to a positive number \"n\", earnings up until \"n\" will be compounded, and the remainder will be sent to the non-compound output (as describe in 1).\n"
            "\n    3) When set to a negative number \"n\", \"n\" will be deducted and sent to a non-compound output (as described in 1) and the remainder will be compounded.\n"
            "\nIn all cases it is important to remember the following:\n"
            "\n    4) Transaction fees and not just the witness reward can be compounded, so while the witness reward is 20 NLG compounding amount should be set considering possible transaction fees as well.\n"
            "\n    5) A maximum of 40 NLG can be compounded, regardless of whether a block contains more fees. In the event that there is additional fees to distribute after applying the compunding settings, the settings will be ignored for the additional fees and paid to a non-compound output (as described in 1)\n"
            "\nArguments:\n"
            "1. \"account\"        (string) The UUID or unique label of the account.\n"
            "2. amount             (numeric or string, required) The amount in " + CURRENCY_UNIT + "\n"
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
    result.push_back(Pair(getUUIDAsString(forAccount->getUUID()), forAccount->getCompounding()));
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
            "getwitnesscompound \"account\"\n"
            "\nGet the current compound setting for an account.\n"
            "\nArguments:\n"
            "1. \"account\"        (string) The UUID or unique label of the account.\n"
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

    return forAccount->getCompounding();
}

static UniValue setwitnessoutputkey(const JSONRPCRequest& request)
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
            "setwitnessoutputkey \"account\" \n"
            "\nSet the output key into which all non-compound witness earnings will be paid.\n"
            "\nSee \"setwitnesscompound\" for how to control compounding and additional information.\n"
            "1. \"account\"        (required) The unique UUID or label for the account.\n"
            "\nResult:\n"
            "\nReturns the address into which non-compound earning payments will occur.\n"
            "\nExamples:\n"
            + HelpExampleCli("setwitnessoutputkey \"my witness account\"", "")
            + HelpExampleRpc("setwitnessoutputkey \"my witness account\"", ""));

    CAccount* forAccount = AccountFromValue(pwallet, request.params[0], false);

    if (!forAccount)
        throw std::runtime_error("Invalid account name or UUID");

    if (!forAccount->IsPoW2Witness())
        throw std::runtime_error(strprintf("Specified account is not a witness account [%s].",  request.params[0].get_str()));

    //fixme: (2.0) implement
    return "Not yet implemented, please check back in next release";
}

static UniValue getwitnessoutputkey(const JSONRPCRequest& request)
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
            "getwitnessoutputkey \"account\" \n"
            "\nGet the output key into which all non-compound witness earnings will be paid.\n"
            "\nSee \"getwitnesscompound\" for how to control compounding and additional information.\n"
            "1. \"account\"        (required) The unique UUID or label for the account.\n"
            "\nResult:\n"
            "\nReturns the current key into which non-compound earning payments will occur.\n"
            "\nExamples:\n"
            + HelpExampleCli("setwitnessoutputkey \"my witness account\"", "")
            + HelpExampleRpc("setwitnessoutputkey \"my witness account\"", ""));

    CAccount* forAccount = AccountFromValue(pwallet, request.params[0], false);

    if (!forAccount)
        throw std::runtime_error("Invalid account name or UUID");

    if (!forAccount->IsPoW2Witness())
        throw std::runtime_error(strprintf("Specified account is not a witness account [%s].",  request.params[0].get_str()));

    //fixme: (2.0) implement
    return "Not yet implemented, please check back in next release";
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
            "getwitnessaccountkeys \"account\" \n"
            "\nGet the witness keys of an HD account, this can be used to import the account as a witness only account in another wallet via the \"importwitnessaccountkey\" command.\n"
            "\nA single account can theoretically contain multiple keys, if it has been split \"splitwitnessaddress\", this will include all of them \n"
            "1. \"account\"        (required) The unique UUID or label for the account.\n"
            "\nResult:\n"
            "\nReturn the private witness keys as an encoded string, that can be used with the \"importwitnessaccountkey\" command.\n"
            "\nNB! The exported private key is only the \"witnessing\" key and not the \"spending\" key for the witness account.\n"
            "\nIf the \"witness\" key is compromised your funds will remain completely safe however the attacker will be able to use the key to claim your earnings.\n"
            "\nIf you believe your key is or may have been compromised use \"rotatewitnessaddress\" to rotate to a new witness key.\n"
            "\nExamples:\n"
            + HelpExampleCli("getwitnessaccountkeys", "")
            + HelpExampleRpc("getwitnessaccountkeys", ""));

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
            "getwitnessaddresskeys \"address\" \n"
            "\nGet the witness key of an HD address, this can be used to import the account as a witness only account in another wallet via the \"importwitnessaccountkey\" command.\n"
            "1. \"address\"        (required) The Gulden address for the witness key.\n"
            "\nResult:\n"
            "\nReturn the private witness key as an encoded string, that can be used with the \"importwitnessaccountkey\" command.\n"
            "\nNB! The exported private key is only the \"witnessing\" key and not the \"spending\" key for the witness account.\n"
            "\nIf the \"witness\" key is compromised your funds will remain completely safe however the attacker will be able to use the key to claim your earnings.\n"
            "\nIf you believe your key is or may have been compromised use \"rotatewitnessaddress\" to rotate to a new witness key.\n"
            "\nExamples:\n"
            + HelpExampleCli("getwitnessaddresskeys 2ZnFwkJyYeEftAoQDe7PC96t2Y7XMmKdNtekRdtx32GNQRJztULieFRFwQoQqN", "")
            + HelpExampleRpc("getwitnessaddresskeys 2ZnFwkJyYeEftAoQDe7PC96t2Y7XMmKdNtekRdtx32GNQRJztULieFRFwQoQqN", ""));

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
            "importwitnesskeys \"account\" \"encoded_key_url\" \"create_account\" \n"
            "\nAdd keys imported from an \"encoded_key_url\" which has been obtained by using \"getwitnessaddresskeys\" or \"getwitnessaccountkeys\" \n"
            "\nUses an existing account if \"create_account\" is false, otherwise creates a new one. \n"
            "1. \"account\"         (string) name/label of the new account.\n"
            "2. \"encoded_key_url\" (string) Encoded string containing the extended public key for the account.\n"
            "3. \"create_account\"  (boolean, optional, default=false) Encoded string containing the extended public key for the account.\n"
            "\nResult:\n"
            "\nReturn the UUID of account.\n"
            "\nExamples:\n"
            + HelpExampleCli("importwitnesskeys \"my witness account\" \"gulden://witnesskeys?keys=Vd69eLAZ2r76C47xB3pDLa9Fx4Li8Xt5AHgzjJDuLbkP8eqUjToC#1529049773\"", "")
            + HelpExampleRpc("importwitnesskeys \"my witness account\" \"gulden://witnesskeys?keys=Vd69eLAZ2r76C47xB3pDLa9Fx4Li8Xt5AHgzjJDuLbkP8eqUjToC#1529049773\"", ""));

    if (!pwallet)
        throw std::runtime_error("Cannot use command without an active wallet");

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
        account = pwallet->CreateWitnessOnlyWitnessAccount(request.params[0].get_str().c_str(), keysAndBirthDates);
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

    { "witness",                 "getwitnessinfo",                  &getwitnessinfo,                 true,    {"blockspecifier", "verbose"} },
    { "witness",                 "createwitnessaccount",            &createwitnessaccount,           true,    {"name"} },
    { "witness",                 "fundwitnessaccount",              &fundwitnessaccount,             true,    {"fundingaccountname", "witnessaccountname", "amount", "time" } },

    //fixme: (2.1) Many of these belong in accounts category as well.
    //We should consider allowing multiple categories for commands, so its easier for people to discover commands under specific topics they are interested in.
    { "witness",                 "rotatewitnessaddress",            &rotatewitnessaddress,           true,    {"address"} },
    { "witness",                 "splitwitnessaddress",             &splitwitnessaddress,            true,    {"address", "amounts"} },
    { "witness",                 "mergewitnessaddresses",           &mergewitnessaddresses,          true,    {"addresses"} },
    { "witness",                 "setwitnesscompound",              &setwitnesscompound,             true,    {"account", "amount"} },
    { "witness",                 "getwitnesscompound",              &getwitnesscompound,             true,    {"account"} },
    { "witness",                 "setwitnessoutputkey",             &setwitnessoutputkey,            true,    {"account"} },
    { "witness",                 "getwitnessoutputkey",             &getwitnessoutputkey,            true,    {"account"} },
    { "witness",                 "getwitnessaccountkeys",           &getwitnessaccountkeys,          true,    {"account"} },
    { "witness",                 "getwitnessaddresskeys",           &getwitnessaddresskeys,          true,    {"address"} },
    { "witness",                 "importwitnesskeys",               &importwitnesskeys,              true,    {"account", "encoded_key_url", "create_account"} },

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
    { "mnemonics",               "importseed",                      &importseed,                     true,    {"mnemonic_or_enckey", "type", "read_only"} },
    { "mnemonics",               "listseeds",                       &listseeds,                      true,    {} },
};

void RegisterGuldenRPCCommands(CRPCTable &t)
{
    for (unsigned int vcidx = 0; vcidx < ARRAYLEN(commands); vcidx++)
        t.appendCommand(commands[vcidx].name, &commands[vcidx]);
}

#endif


