// Copyright (c) 2020-2022 The Centure developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the Libre Chain License, see the accompanying
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
#include "util/strencodings.h"


#include "witnessutil.h"

#include <wallet/wallet.h>
#include <wallet/account.h>
#include <wallet/witness_operations.h>

// Unity specific includes
#include "../unity_impl.h"
#include "i_witness_controller.hpp"
#include "witness_estimate_info_record.hpp"
#include "witness_funding_result_record.hpp"
#include "result_record.hpp"
#include "witness_account_statistics_record.hpp"

#include <consensus/validation.h>

// stdlib includes
#include <numeric>


std::unordered_map<std::string, std::string> IWitnessController::getNetworkLimits()
{
    std::unordered_map<std::string, std::string> ret;
    if (pactiveWallet)
    {
        // Testnet does these calculations on "mainnet time" even though its block targer may be faster/slower (giving a sort of "time warp" illusion for testers)
        if (Params().IsTestnet())
        {
            ret.insert(std::pair("expected_blocks_per_day", i64tostr(gRefactorDailyBlocksUsage)));
            ret.insert(std::pair("minimum_lock_period_blocks", i64tostr(gMinimumWitnessLockDays*gRefactorDailyBlocksUsage)));
            ret.insert(std::pair("maximum_lock_period_blocks", i64tostr(gMaximumWitnessLockDays*gRefactorDailyBlocksUsage)));
        }
        else
        {
            ret.insert(std::pair("expected_blocks_per_day", i64tostr(DailyBlocksTarget())));
            ret.insert(std::pair("minimum_lock_period_blocks", i64tostr(gMinimumWitnessLockDays*DailyBlocksTarget())));
            ret.insert(std::pair("maximum_lock_period_blocks", i64tostr(gMaximumWitnessLockDays*DailyBlocksTarget())));
        }
        ret.insert(std::pair("witness_cooldown_period", i64tostr(gMinimumParticipationAge)));
        ret.insert(std::pair("minimum_witness_amount", i64tostr(gMinimumWitnessAmount)));
        ret.insert(std::pair("minimum_witness_weight", i64tostr(gMinimumWitnessWeight)));
        ret.insert(std::pair("minimum_lock_period_days", i64tostr(gMinimumWitnessLockDays)));        
        ret.insert(std::pair("maximum_lock_period_days", i64tostr(gMaximumWitnessLockDays)));
    }
    return ret;
}

static int64_t GetNetworkWeight()
{
    static int64_t nNetworkWeight = 9000000;
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
                //fixme: Ideally this should use nTotalWeightEligibleRaw, but its not set from the above calls and would require additional computation
                if (witnessInfo.nTotalWeightRaw != 0)
                {
                    nNetworkWeight = witnessInfo.nTotalWeightRaw;
                }
            }
        }
    }
    return nNetworkWeight;
}

WitnessEstimateInfoRecord IWitnessController::getEstimatedWeight(int64_t amountToLock, int64_t lockPeriodInBlocks)
{
    if (!pactiveWallet)
        return WitnessEstimateInfoRecord(0, 0, 0, 0, 0, 0, 0);
    
    if (!chainActive.Tip() || chainActive.Tip()->nHeight < 10)
        return WitnessEstimateInfoRecord(0, 0, 0, 0, 0, 0, 0);
    
    int64_t lockPeriodInDays;
    if (Params().IsTestnet())
    {
        lockPeriodInDays = lockPeriodInBlocks / gRefactorDailyBlocksUsage;
    }
    else
    {
        lockPeriodInDays = lockPeriodInBlocks / DailyBlocksTarget();
    }
    
    uint64_t networkWeight = GetNetworkWeight();
    const auto optimalAmounts = optimalWitnessDistribution(amountToLock, lockPeriodInBlocks, networkWeight);
    int64_t ourTotalWeight = combinedWeight(optimalAmounts, chainActive.Height(), lockPeriodInBlocks);
    
    double witnessProbability = witnessFraction(optimalAmounts, chainActive.Height(), lockPeriodInBlocks, networkWeight);
    double estimatedBlocksPerDay = DailyBlocksTarget() * witnessProbability;
    
    CAmount witnessSubsidy = GetBlockSubsidy(chainActive.Tip()?chainActive.Tip()->nHeight:1).witness;

    CAmount estimatedDailyEarnings = estimatedBlocksPerDay * witnessSubsidy;
    CAmount estimatedLifetimeEarnings = (DailyBlocksTarget() * lockPeriodInDays) * witnessProbability * witnessSubsidy;
    
    return WitnessEstimateInfoRecord(networkWeight, ourTotalWeight, optimalAmounts.size(), witnessProbability, estimatedBlocksPerDay, estimatedDailyEarnings, estimatedLifetimeEarnings);
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

WitnessFundingResultRecord IWitnessController::renewWitnessAccount(const std::string& fundingAccountUUID, const std::string& witnessAccountUUID)
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
        renewwitnessaccount(pactiveWallet, fundingAccount, witnessAccount, &txid, &fee);
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


struct WitnessInfoForAccount
{
    CWitnessAccountStatus accountStatus;

    uint64_t nOurWeight = 0;
    uint64_t nTotalNetworkWeightTip = 0;
    uint64_t nWitnessLength = 0;
    uint64_t nExpectedWitnessBlockPeriod = 0;
    uint64_t nEstimatedWitnessBlockPeriod = 0;
    uint64_t nLockBlocksRemaining = 0;
    int64_t  nOriginNetworkWeight = 0;
    uint64_t nOriginBlock = 0;
    uint64_t nOriginLength = 0;
    uint64_t nEarningsToDate = 0;
};

bool GetWitnessInfoForAccount(CAccount* forAccount, WitnessInfoForAccount& infoForAccount, CGetWitnessInfo& witnessInfo)
{
    if (!forAccount->IsPoW2Witness())
        return false;
        
    CWitnessAccountStatus accountStatus;
    accountStatus = GetWitnessAccountStatus(pactiveWallet, forAccount, &witnessInfo);

    infoForAccount.accountStatus = accountStatus;

    infoForAccount.nTotalNetworkWeightTip = accountStatus.networkWeight;
    infoForAccount.nOurWeight = accountStatus.currentWeight;

    infoForAccount.nOriginLength = accountStatus.nLockPeriodInBlocks;
    infoForAccount.nOriginBlock = accountStatus.nLockFromBlock;
    
    #if 0 //unused and potentially quite slow
    LOCK(cs_main);
    CBlockIndex* originIndex = chainActive[infoForAccount.nOriginBlock];

    // We take the network weight 100 blocks ahead to give a chance for our own weight to filter into things (and also if e.g. the first time witnessing activated - testnet - then weight will only climb once other people also join)
    CBlockIndex* sampleWeightIndex = chainActive[infoForAccount.nOriginBlock+100 > (uint64_t)chainActive.Tip()->nHeight ? infoForAccount.nOriginBlock : infoForAccount.nOriginBlock+100];
    int64_t nUnused1;
    if (!GetPow2NetworkWeight(sampleWeightIndex, Params(), nUnused1, infoForAccount.nOriginNetworkWeight, chainActive))
    {
        std::string strErrorMessage = "Error in witness dialog, failed to get weight for account";
        return false;
    }
    
    if (infoForAccount.nOriginNetworkWeight == 0)
        infoForAccount.nOriginNetworkWeight = gStartingWitnessNetworkWeightEstimate;
    #endif

    infoForAccount.nWitnessLength = infoForAccount.nOriginLength;

    const auto& parts = infoForAccount.accountStatus.parts;
    if (!parts.empty())
    {
        uint64_t networkWeight = infoForAccount.nTotalNetworkWeightTip;
        // Worst case all parts witness at latest opportunity so part with maximum weight will be the first to be required to witness
        infoForAccount.nExpectedWitnessBlockPeriod = expectedWitnessBlockPeriod(std::get<0>(*std::max_element(parts.begin(), parts.end(), [](std::tuple<uint64_t, uint64_t> l, std::tuple<uint64_t, uint64_t> r) {return std::get<0>(l) > std::get<0>(r);})), networkWeight);

        // Combine estimated witness frequency f for part frequencies f1..fN: 1/f = 1/f1 + .. 1/fN
        double fInv = std::accumulate(parts.begin(), parts.end(), 0.0, [=](const double acc, const std::tuple<uint64_t, uint64_t> w){
            uint64_t fn = estimatedWitnessBlockPeriod(std::get<0>(w), networkWeight);
            return acc + 1.0/fn;
        });
        infoForAccount.nEstimatedWitnessBlockPeriod = uint64_t(1.0/fInv);
    }

    infoForAccount.nLockBlocksRemaining = GetPoW2RemainingLockLengthInBlocks(accountStatus.nLockUntilBlock, chainActive.Tip()->nHeight);

    return true;
}

WitnessAccountStatisticsRecord IWitnessController::getAccountWitnessStatistics(const std::string& witnessAccountUUID)
{
    if (!pactiveWallet)
        return WitnessAccountStatisticsRecord("no active wallet present", "", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, false);
    
    DS_LOCK2(cs_main, pactiveWallet->cs_wallet);
    
    auto findIter = pactiveWallet->mapAccounts.find(getUUIDFromString(witnessAccountUUID));
    if (findIter == pactiveWallet->mapAccounts.end())
        return WitnessAccountStatisticsRecord("invalid witness account", "", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, false);
    CAccount* witnessAccount = findIter->second;
    
    CGetWitnessInfo witnessInfo;
    WitnessInfoForAccount infoForAccount;
    if (!GetWitnessInfoForAccount(witnessAccount, infoForAccount, witnessInfo))
    {
        return WitnessAccountStatisticsRecord("failed to get witness info for account", "", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, false);
    }
    std::string accountStatus;
    switch (infoForAccount.accountStatus.status)
    {
        case WitnessStatus::Empty: accountStatus = "empty"; break;
        case WitnessStatus::EmptyWithRemainder: accountStatus = "empty_with_remainder"; break;
        case WitnessStatus::Pending: accountStatus = "pending"; break;
        case WitnessStatus::Witnessing: accountStatus = "active"; break;
        case WitnessStatus::Ended: accountStatus = "ended"; break;
        case WitnessStatus::Expired: accountStatus = "expired"; break;
        case WitnessStatus::Emptying: accountStatus = "emptying"; break;
    }
    int64_t compoundingPercent = 0;
    if (witnessAccount->getCompounding() != 0)
    {
        compoundingPercent = 100;
    }
    else
    {
        compoundingPercent = witnessAccount->getCompoundingPercent();
    }

    bool accountNearOptimal = isWitnessDistributionNearOptimal(pactiveWallet, witnessAccount, witnessInfo);
    
    uint64_t nBlocksSinceLastActivity = std::get<1>(*std::min_element(infoForAccount.accountStatus.parts.begin(), infoForAccount.accountStatus.parts.end(), [](std::tuple<uint64_t, uint64_t> l, std::tuple<uint64_t, uint64_t> r) {return std::get<1>(l) < std::get<1>(r);}));
    
    return WitnessAccountStatisticsRecord(
        "success",                                                      //request_status
        accountStatus,                                                  //account_status
        nBlocksSinceLastActivity,                                       //account_blocks_since_last_activity
        infoForAccount.nOurWeight,                                      //account_weight
        infoForAccount.accountStatus.originWeight,                      //account_weight_at_creation
        infoForAccount.accountStatus.parts.size(),                      //account_parts
        infoForAccount.accountStatus.currentAmountLocked,               //account_amount_locked
        infoForAccount.accountStatus.originAmountLocked,                //account_amount_locked_at_creation
        infoForAccount.nTotalNetworkWeightTip,                          //network_tip_total_weight
        infoForAccount.accountStatus.networkWeight,                     //network_total_weight_at_creation
        infoForAccount.nOriginLength,                                   //account_initial_lock_period_in_blocks
        infoForAccount.nLockBlocksRemaining,                            //account_remaining_lock_period_in_blocks
        infoForAccount.nExpectedWitnessBlockPeriod,                     //account_expected_witness_period_in_blocks
        infoForAccount.nEstimatedWitnessBlockPeriod,                    //account_estimated_witness_period_in_blocks
        infoForAccount.nOriginBlock,                                    //account_initial_lock_creation_block_height
        compoundingPercent,                                             //compounding_percent
        accountNearOptimal                                              //is_optimal
    );
}


void IWitnessController::setAccountCompounding(const std::string& witnessAccountUUID, int32_t percentToCompound)
{
    if (pactiveWallet)
    {
        DS_LOCK2(cs_main, pactiveWallet->cs_wallet);
    
        auto findIter = pactiveWallet->mapAccounts.find(getUUIDFromString(witnessAccountUUID));
        if (findIter != pactiveWallet->mapAccounts.end())
        {
            CAccount* witnessAccount = findIter->second;
            
            CWalletDB db(*pactiveWallet->dbw);
            if (percentToCompound > 0)
            {
                // erase any regular compounding setting
                witnessAccount->setCompounding(0, &db);
            }
            witnessAccount->setCompoundingPercent(percentToCompound, &db);
        }
    }
}

int32_t IWitnessController::isAccountCompounding(const std::string& witnessAccountUUID)
{
    if (pactiveWallet)
    {
        DS_LOCK2(cs_main, pactiveWallet->cs_wallet);
    
        auto findIter = pactiveWallet->mapAccounts.find(getUUIDFromString(witnessAccountUUID));
        if (findIter != pactiveWallet->mapAccounts.end())
        {
            CAccount* witnessAccount = findIter->second;
            if (witnessAccount->getCompounding() != 0)
            {
                return 100;
            }
            else
            {
                return witnessAccount->getCompoundingPercent();
            }
        }
    }
    return 0;
}

std::string IWitnessController::getWitnessAddress(const std::string& witnessAccountUUID)
{
    if (pactiveWallet)
    {
        LOCK2(cs_main, pactiveWallet->cs_wallet);
    
        auto findIter = pactiveWallet->mapAccounts.find(getUUIDFromString(witnessAccountUUID));
        if (findIter != pactiveWallet->mapAccounts.end())
        {
            CAccount* witnessAccount = findIter->second;
            
            return witnessAddressForAccount(pactiveWallet, witnessAccount);
        }
    }
    return "";
}


std::vector<int64_t> IWitnessController::getOptimalWitnessDistribution(const int64_t amount, const int64_t durationInBlocks, const int64_t totalNetworkWeight)
{
    return optimalWitnessDistribution(amount, durationInBlocks, totalNetworkWeight);
}

std::vector<int64_t> IWitnessController::getOptimalWitnessDistributionForAccount(const std::string& witnessAccountUUID)
{
    if (pactiveWallet)
    {
        LOCK2(cs_main, pactiveWallet->cs_wallet);
    
        auto findIter = pactiveWallet->mapAccounts.find(getUUIDFromString(witnessAccountUUID));
        if (findIter != pactiveWallet->mapAccounts.end())
        {
            CAccount* witnessAccount = findIter->second;
            
            CGetWitnessInfo witnessInfo = GetWitnessInfoWrapper();
            auto [currentDistribution, duration, totalAmount] = witnessDistribution(pactiveWallet, witnessAccount);
            return getOptimalWitnessDistribution(totalAmount, duration, witnessInfo.nTotalWeightEligibleRaw);
        }
    }
    return std::vector<int64_t>();
}

ResultRecord IWitnessController::optimiseWitnessAccount(const std::string& witnessAccountUUID, const std::string& fundingAccountUUID, const std::vector<int64_t>& optimalDistribution)
{
    if (pactiveWallet)
    {
        LOCK2(cs_main, pactiveWallet->cs_wallet);
    
        auto findIter = pactiveWallet->mapAccounts.find(getUUIDFromString(witnessAccountUUID));
        if (findIter != pactiveWallet->mapAccounts.end())
        {
            auto findFundingIter = pactiveWallet->mapAccounts.find(getUUIDFromString(fundingAccountUUID));
            if (findFundingIter != pactiveWallet->mapAccounts.end())
            {
                CAccount* witnessAccount = findIter->second;
                CAccount* fundingAccount = findFundingIter->second;
                
                try
                {
                    redistributewitnessaccount(pactiveWallet, fundingAccount, witnessAccount, optimalDistribution, nullptr, nullptr); // ignore result params      
                }
                catch(const std::string& e)
                {
                    return ResultRecord(false, e);
                }
                catch(...)
                {
                    return ResultRecord(false, "unknown error");
                }
                return ResultRecord(true, "");
            }
        }
    }
    return ResultRecord(false, "no wallet available");
}
