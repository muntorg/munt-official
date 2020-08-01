// Copyright (c) 2020 The Novo developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the NOVO software license, see the accompanying
// file COPYING

//Workaround braindamaged 'hack' in libtool.m4 that defines DLL_EXPORT when building a dll via libtool (this in turn imports unwanted symbols from e.g. pthread that breaks static pthread linkage)
#ifdef DLL_EXPORT
#undef DLL_EXPORT
#endif

#include "appname.h"
#include "net.h"
#include "net_processing.h"
#include "validation/validation.h"
#include "validation/witnessvalidation.h"
#include "ui_interface.h"
#include "utilstrencodings.h"


#include "witnessutil.h"

#include <wallet/wallet.h>
#include <wallet/account.h>
#include <wallet/witness_operations.h>

// Unity specific includes
#include "../unity_impl.h"
#include "i_witness_controller.hpp"
#include "witness_estimate_info_record.hpp"
#include "witness_funding_result_record.hpp"

#include <consensus/validation.h>


std::unordered_map<std::string, std::string> IWitnessController::getNetworkLimits()
{
    std::unordered_map<std::string, std::string> ret;
    if (pactiveWallet)
    {        
        ret.insert(std::pair("expected_blocks_per_day", i64tostr(DailyBlocksTarget())));
        ret.insert(std::pair("witness_cooldown_period", i64tostr(gMinimumParticipationAge)));
        ret.insert(std::pair("minimum_witness_amount", i64tostr(gMinimumWitnessAmount)));
        ret.insert(std::pair("minimum_witness_weight", i64tostr(gMinimumWitnessWeight)));
        ret.insert(std::pair("minimum_lock_period_blocks", i64tostr(gMinimumWitnessLockDays*DailyBlocksTarget())));
        ret.insert(std::pair("minimum_lock_period_days", i64tostr(gMinimumWitnessLockDays)));
        ret.insert(std::pair("maximum_lock_period_blocks", i64tostr(gMaximumWitnessLockDays*DailyBlocksTarget())));
        ret.insert(std::pair("maximum_lock_period_days", i64tostr(gMaximumWitnessLockDays)));
    }
    return ret;
}

static int64_t GetNetworkWeight()
{
    int64_t nNetworkWeight = 20000;
    if (chainActive.Tip())
    {
        static uint64_t lastUpdate = 0;
        // Only check this once a minute, no need to be constantly updating.
        if (GetTimeMillis() - lastUpdate > 60000)
        {
            LOCK(cs_main);

            lastUpdate = GetTimeMillis();
            if (IsPow2WitnessingActive(chainActive.TipPrev()->nHeight))
            {
                CGetWitnessInfo witnessInfo;
                CBlock block;
                if (!ReadBlockFromDisk(block, chainActive.Tip(), Params()))
                {
                    return nNetworkWeight;
                }
                if (!GetWitnessInfo(chainActive, Params(), nullptr, chainActive.Tip()->pprev, block, witnessInfo, chainActive.Tip()->nHeight))
                {
                    return nNetworkWeight;
                }
                nNetworkWeight = witnessInfo.nTotalWeightEligibleRaw;
            }
        }
    }
    return nNetworkWeight;
}

WitnessEstimateInfoRecord IWitnessController::getEstimatedWeight(int64_t amountToLock, int64_t lockPeriodInDays)
{
    int64_t lockPeriodInBlocks = lockPeriodInDays * DailyBlocksTarget();
    
    uint64_t networkWeight = GetNetworkWeight();
    const auto optimalAmounts = optimalWitnessDistribution(amountToLock, lockPeriodInBlocks, networkWeight);
    int64_t ourTotalWeight = combinedWeight(optimalAmounts, lockPeriodInDays);
    
    double witnessProbability = witnessFraction(optimalAmounts, lockPeriodInDays, networkWeight);
    double estimatedBlocksPerDay = DailyBlocksTarget() * witnessProbability;
    
    CAmount witnessSubsidy = GetBlockSubsidyWitness(chainActive.Tip()?chainActive.Tip()->nHeight:1);

    CAmount estimatedDaileyEarnings = estimatedBlocksPerDay * witnessSubsidy;
    CAmount estimatedLifetimeEarnings = estimatedBlocksPerDay * lockPeriodInDays * witnessSubsidy;
    
    return WitnessEstimateInfoRecord(networkWeight, ourTotalWeight, optimalAmounts.size(), witnessProbability, estimatedBlocksPerDay, estimatedDaileyEarnings, estimatedLifetimeEarnings);
}

WitnessFundingResultRecord IWitnessController::fundWitnessAccount(const std::string& fundingAccountUUID, const std::string& witnessAccountUUID, int64_t fundingAmount, int64_t requestedLockPeriodInBlocks)
{
    if (!pactiveWallet)
        return WitnessFundingResultRecord("no active wallet present", "", 0);;
    
    DS_LOCK2(cs_main, pactiveWallet->cs_wallet);
    
    auto findIter = pactiveWallet->mapAccounts.find(getUUIDFromString(fundingAccountUUID));
    if (findIter == pactiveWallet->mapAccounts.end())
        return WitnessFundingResultRecord("invalid funding account", "", 0);;
    CAccount* fundingAccount = findIter->second;
    
    findIter = pactiveWallet->mapAccounts.find(getUUIDFromString(witnessAccountUUID));
    if (findIter == pactiveWallet->mapAccounts.end())
        return WitnessFundingResultRecord("invalid witness account", "", 0);;
    CAccount* witnessAccount = findIter->second;
    
    if (!witnessAccount->IsPoW2Witness())
        return WitnessFundingResultRecord("not a witness account", "", 0);;
        
    try
    {
        std::string txid;
        CAmount fee;
        fundwitnessaccount(pactiveWallet, fundingAccount, witnessAccount, fundingAmount, requestedLockPeriodInBlocks, true, &txid, &fee);
        return WitnessFundingResultRecord("success", txid, fee);
    }
    catch (witness_error& e)
    {
        return WitnessFundingResultRecord(e.what(), "", 0);
    }
    catch (std::runtime_error& e)
    {
        return WitnessFundingResultRecord(e.what(), "", 0);
    }
}

