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

bool IsPow2Phase2Active(const CBlockIndex* pindexPrev, const CChainParams& chainparams);
bool IsPow2Phase3Active(const CBlockIndex* pindexPrev, const CChainParams& chainparams);
bool IsPow2Phase4Active(const CBlockIndex* pindexPrev, const CChainParams& chainparams);

int64_t GetPoW2Phase3ActivationTime();
void GetPow2NetworkWeight(const CBlockIndex* pIndex, int64_t& nNumWitnessAddresses, int64_t& nTotalWeight);

#endif
