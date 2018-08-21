// Copyright (c) 2015-2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#ifndef GULDEN_GULDEN_UTIL_H
#define GULDEN_GULDEN_UTIL_H

#include <string>
#ifdef ENABLE_WALLET
#include "wallet/wallet.h"
#include "account.h"
#endif
#include "script/standard.h"
#include "chainparams.h"
#include "chain.h"
#include "coins.h"

#ifdef ENABLE_WALLET
void rescanThread();
std::string StringFromSeedType(CHDSeed* seed);
CHDSeed::SeedType SeedTypeFromString(std::string type);
#endif

class CBlockIndex;
bool IsPow2Phase2Active(const CBlockIndex* pindexPrev, const CChainParams& chainparams, CChain& chain, CCoinsViewCache* viewOverride=nullptr);
bool IsPow2Phase3Active(uint64_t nHeight);
bool IsPow2Phase4Active(const CBlockIndex* pindexPrev, const CChainParams& chainparams, CChain& chain, CCoinsViewCache* viewOverride=nullptr);
bool IsPow2Phase5Active(const CBlockIndex* pindexPrev, const CChainParams& chainparams, CChain& chain, CCoinsViewCache* viewOverride=nullptr);

int GetPhase2ActivationHeight();

bool IsPow2WitnessingActive(const CBlockIndex* pindexPrev, const CChainParams& chainparams, CChain& chain, CCoinsViewCache* viewOverride=nullptr);
int GetPoW2Phase(const CBlockIndex* pIndex, const CChainParams& chainparams, CChain& chain, CCoinsViewCache* viewOverride=nullptr);
bool GetPow2NetworkWeight(const CBlockIndex* pIndex, const CChainParams& chainparams, int64_t& nNumWitnessAddresses, int64_t& nTotalWeight, CChain& chain, CCoinsViewCache* viewOverride=nullptr);

int64_t GetPoW2Phase3ActivationTime(CChain& chain, CCoinsViewCache* viewOverride=nullptr);

int64_t GetPoW2RawWeightForAmount(int64_t nAmount, int64_t nLockLengthInBlocks);


//! Calculate how many blocks a witness transaction is locked for from an output
//! Always use this helper instead of attempting to calculate directly - to avoid off by 1 errors.
int64_t GetPoW2LockLengthInBlocksFromOutput(const CTxOut& out, uint64_t txBlockNumber, uint64_t& nFromBlockOut, uint64_t& nUntilBlockOut);

//! Calculate how many blocks are left from `tipHeight` until a witness output unlocks, given inputs 'lockUntilBlock' and 'tipHeight'
//! Though this is a relatively simple calculation, always use this helper instead of attempting to calculate directly - to avoid off by 1 errors.
uint64_t GetPoW2RemainingLockLengthInBlocks(uint64_t lockUntilBlock, uint64_t tipHeight);

CBlockIndex* GetPoWBlockForPoSBlock(const CBlockIndex* pIndex);

int GetPow2ValidationCloneHeight(CChain& chain, const CBlockIndex* pIndex, int nMargin);

inline bool IsPow2WitnessOutput(const CTxOut& out)
{
    if ( (out.GetType() <= CTxOutType::ScriptLegacyOutput && out.output.scriptPubKey.IsPoW2Witness()) || (out.GetType() == CTxOutType::PoW2WitnessOutput) )
        return true;
    return false;
}

inline bool GetPow2WitnessOutput(const CTxOut& out, CTxOutPoW2Witness& witnessDetails)
{
    if (out.GetType() == CTxOutType::PoW2WitnessOutput)
    {
        witnessDetails = out.output.witnessDetails;
        return true;
    }
    else if ( (out.GetType() <= CTxOutType::ScriptLegacyOutput && out.output.scriptPubKey.IsPoW2Witness()) )  //fixme: (2.1) we can remove this
    {
        if (!out.output.scriptPubKey.ExtractPoW2WitnessFromScript(witnessDetails))
            return false;
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

#endif
