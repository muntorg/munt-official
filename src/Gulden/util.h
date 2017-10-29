// Copyright (c) 2015-2017 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#ifndef GULDEN_UTIL_H
#define GULDEN_UTIL_H

#include <string>
#include "wallet/wallet.h"

void rescanThread();
std::string StringFromSeedType(CHDSeed* seed);
CHDSeed::SeedType SeedTypeFromString(std::string type);

class CBlockIndex;
bool IsPow2Phase2Active(const CBlockIndex* pindexPrev, const Consensus::Params& params);
bool IsPow2Phase3Active(const CBlockIndex* pindexPrev, const Consensus::Params& params);
bool IsPow2Phase4Active(const CBlockIndex* pindexPrev, const Consensus::Params& params);
bool IsPow2Phase5Active(const CBlockIndex* pindexPrev, const Consensus::Params& params);
bool IsPow2WitnessingActive(const CBlockIndex* pindexPrev, const Consensus::Params& params);

bool IsPow2Phase2Active(const CBlockIndex* pindexPrev, const CChainParams& chainparams);
bool IsPow2Phase3Active(const CBlockIndex* pindexPrev, const CChainParams& chainparams);
bool IsPow2Phase4Active(const CBlockIndex* pindexPrev, const CChainParams& chainparams);
bool IsPow2Phase5Active(const CBlockIndex* pindexPrev, const CChainParams& chainparams);

bool IsPow2WitnessingActive(const CBlockIndex* pindexPrev, const CChainParams& chainparams);
int GetPoW2Phase(const CBlockIndex* pIndex, const Consensus::Params& params);

int64_t GetPoW2Phase3ActivationTime();

int64_t GetPoW2RawWeightForAmount(int64_t nAmount, int64_t nLockLengthInBlocks);
int64_t GetPoW2LockLengthInBlocksFromOutput(CTxOut& out, uint64_t txBlockNumber);
void GetPow2NetworkWeight(const CBlockIndex* pIndex, int64_t& nNumWitnessAddresses, int64_t& nTotalWeight);
CBlockIndex* GetPoWBlockForPoSBlock(const CBlockIndex* pIndex);

inline bool IsPow2WitnessOutput(const CTxOut& out)
{
    if ( (out.GetType() <= CTxOutType::ScriptOutput && out.output.scriptPubKey.IsPoW2Witness()) || (out.GetType() == CTxOutType::PoW2WitnessOutput) )
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
    else if ( (out.GetType() <= CTxOutType::ScriptOutput && out.output.scriptPubKey.IsPoW2Witness()) )  //fixme: (GULDEN) (2.1) we can remove this
    {
        out.output.scriptPubKey.ExtractPoW2WitnessFromScript(witnessInput);
    }
    else
    {
        assert(0);
    }
    return witnessInput;
}

#endif
