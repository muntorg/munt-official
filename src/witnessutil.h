// Copyright (c) 2015-2022 The Centure developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the Libre Chain License, see the accompanying
// file COPYING

#ifndef WITNESS_UTIL_H
#define WITNESS_UTIL_H

#include <string>
#ifdef ENABLE_WALLET
#include "wallet/wallet.h"
#include "wallet/account.h"
#endif
#include "script/standard.h"
#include "chainparams.h"
#include "chain.h"
#include "coins.h"

#ifdef ENABLE_WALLET
void ResetSPVStartRescanThread();
std::string StringFromSeedType(CHDSeed* seed);
CHDSeed::SeedType SeedTypeFromString(std::string type);
#endif

class CBlockIndex;
bool IsPow2Phase2Active(const CBlockIndex* pindexPrev);
bool IsPow2Phase3Active(uint64_t nHeight);
bool IsPow2Phase4Active(const CBlockIndex* pindexPrev);
bool IsPow2Phase5Active(const CBlockIndex* pindexPrev, const CChainParams& chainparams, CChain& chain, CCoinsViewCache* viewOverride=nullptr);
bool IsPow2Phase5Active(uint64_t nHeight);
bool IsPow2WitnessingActive(uint64_t nHeight);
int GetPoW2Phase(const CBlockIndex* pindexPrev);

bool GetPow2NetworkWeight(const CBlockIndex* pIndex, const CChainParams& chainparams, int64_t& nNumWitnessAddresses, int64_t& nTotalWeight, CChain& chain, CCoinsViewCache* viewOverride=nullptr);

int64_t GetPoW2RawWeightForAmount(int64_t nAmount, int64_t nHeight, int64_t nLockLengthInBlocks);


//! Calculate how many blocks a witness transaction is locked for from an output
//! Always use this helper instead of attempting to calculate directly - to avoid off by 1 errors.
uint64_t GetPoW2LockLengthInBlocksFromOutput(const CTxOut& out, uint64_t txBlockNumber, uint64_t& nFromBlockOut, uint64_t& nUntilBlockOut);

//! Calculate how many blocks are left from `tipHeight` until a witness output unlocks, given inputs 'lockUntilBlock' and 'tipHeight'
//! Though this is a relatively simple calculation, always use this helper instead of attempting to calculate directly - to avoid off by 1 errors.
uint64_t GetPoW2RemainingLockLengthInBlocks(uint64_t lockUntilBlock, uint64_t tipHeight);

CBlockIndex* GetPoWBlockForPoSBlock(const CBlockIndex* pIndex);

int GetPow2ValidationCloneHeight(CChain& chain, const CBlockIndex* pIndex, int nMargin);

inline bool IsPow2WitnessOutput(const CTxOut& out)
{
    if (out.GetType() == CTxOutType::PoW2WitnessOutput)
        return true;
    return false;
}

inline bool GetPow2WitnessOutput(const CTxOut& out, CTxOutPoW2Witness& witnessDetails)
{
    if (out.GetType() == CTxOutType::PoW2WitnessOutput)
    {
        witnessDetails = out.output.witnessDetails;
        witnessDetails.nType = CTxOutType::PoW2WitnessOutput;
        return true;
    }
    return false;
}

inline CTxOutPoW2Witness GetPoW2WitnessOutputFromWitnessDestination(const CPoW2WitnessDestination& fromDest)
{
    CTxOutPoW2Witness txout;
    txout.spendingKeyID = fromDest.spendingKey;
    txout.witnessKeyID = fromDest.witnessKey;
    txout.lockFromBlock = fromDest.lockFromBlock;
    txout.lockUntilBlock = fromDest.lockUntilBlock;
    txout.failCount = fromDest.failCount;
    txout.actionNonce = fromDest.actionNonce;
    return txout;
}

inline bool IsPoW2WitnessLocked(const CTxOutPoW2Witness& witnessDetails, uint64_t nTipHeight)
{
    if (witnessDetails.lockUntilBlock >= nTipHeight)
        return true;
    return false;
}

inline bool IsPoW2WitnessLocked(const CTxOut& out, uint64_t nTipHeight)
{
    CTxOutPoW2Witness witnessDetails;
    if (!GetPow2WitnessOutput(out, witnessDetails))
        return false;
    return IsPoW2WitnessLocked(witnessDetails, nTipHeight);
}

/**
 * Partial (header) sync is considered active if the partial chain has been populatd with at least one block.
 * Requires cs_main
 */
bool IsPartialSyncActive();

/**
  * Check if the most recent block in the partial tree is near the present (if it exists)
  * Requires cs_main
  */
bool IsPartialNearPresent(enum BlockStatus nUpTo = BLOCK_PARTIAL_TREE);

/**
  * Check if the most recent block in the full chainActive is near the present
  * Requires cs_main
  */
bool IsChainNearPresent();

int DailyBlocksTarget();
uint64_t MinimumWitnessLockLength();
uint64_t MaximumWitnessLockLength();

int timeToBirthNumber(const int64_t time);

int64_t birthNumberToTime(int number);

#endif
