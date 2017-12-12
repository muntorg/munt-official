// Copyright (c) 2015-2017 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#ifndef GULDEN_UTIL_H
#define GULDEN_UTIL_H

#include <string>
#include "wallet/wallet.h"
#include "chain.h"
#include "coins.h"

void rescanThread();
std::string StringFromSeedType(CHDSeed* seed);
CHDSeed::SeedType SeedTypeFromString(std::string type);

class CBlockIndex;
bool IsPow2Phase2Active(const CBlockIndex* pindexPrev, const Consensus::Params& params, CChain& chain, CCoinsViewCache* viewOverride=nullptr);
bool IsPow2Phase4Active(const CBlockIndex* pindexPrev, const Consensus::Params& params, CChain& chain, CCoinsViewCache* viewOverride=nullptr);

bool IsPow2Phase2Active(const CBlockIndex* pindexPrev, const CChainParams& chainparams, CChain& chain, CCoinsViewCache* viewOverride=nullptr);
bool IsPow2Phase3Active(const CBlockIndex* pindexPrev, const CChainParams& chainparams, CChain& chain, CCoinsViewCache* viewOverride=nullptr);
bool IsPow2Phase4Active(const CBlockIndex* pindexPrev, const CChainParams& chainparams, CChain& chain, CCoinsViewCache* viewOverride=nullptr);
bool IsPow2Phase5Active(const CBlockIndex* pindexPrev, const CChainParams& chainparams, CChain& chain, CCoinsViewCache* viewOverride=nullptr);

bool IsPow2WitnessingActive(const CBlockIndex* pindexPrev, const CChainParams& chainparams, CChain& chain, CCoinsViewCache* viewOverride=nullptr);
int GetPoW2Phase(const CBlockIndex* pIndex, const CChainParams& chainparams, CChain& chain, CCoinsViewCache* viewOverride=nullptr);
bool GetPow2NetworkWeight(const CBlockIndex* pIndex, const CChainParams& chainparams, int64_t& nNumWitnessAddresses, int64_t& nTotalWeight, CChain& chain, CCoinsViewCache* viewOverride=nullptr);

int64_t GetPoW2Phase3ActivationTime(CChain& chain, CCoinsViewCache* viewOverride=nullptr);

int64_t GetPoW2RawWeightForAmount(int64_t nAmount, int64_t nLockLengthInBlocks);
int64_t GetPoW2LockLengthInBlocksFromOutput(CTxOut& out, uint64_t txBlockNumber);

CBlockIndex* GetPoWBlockForPoSBlock(const CBlockIndex* pIndex);

inline bool IsPow2WitnessOutput(const CTxOut& out)
{
    if ( (out.GetType() <= CTxOutType::ScriptLegacyOutput && out.output.scriptPubKey.IsPoW2Witness()) || (out.GetType() == CTxOutType::PoW2WitnessOutput) )
        return true;
    return false;
}

inline CTxOutPoW2Witness GetPow2WitnessOutput(const CTxOut& out)
{
    CTxOutPoW2Witness witnessInput;
    if (out.GetType() == CTxOutType::PoW2WitnessOutput)
    {
        witnessInput = out.output.witnessDetails;
    }
    else if ( (out.GetType() <= CTxOutType::ScriptLegacyOutput && out.output.scriptPubKey.IsPoW2Witness()) )  //fixme: (GULDEN) (2.1) we can remove this
    {
        out.output.scriptPubKey.ExtractPoW2WitnessFromScript(witnessInput);
    }
    else
    {
        assert(0);
    }
    return witnessInput;
}

inline CTxOutPoW2Witness GetPoW2WitnessOutputFromWitnessDestination(const CPoW2WitnessDestination& fromDest)
{
    CTxOutPoW2Witness txout;
    txout.spendingKeyID = fromDest.spendingKey;
    txout.witnessKeyID = fromDest.witnessKey;
    txout.lockFromBlock = fromDest.lockFromBlock;
    txout.lockUntilBlock = fromDest.lockUntilBlock;
    txout.failCount = fromDest.failCount;
}

#endif
