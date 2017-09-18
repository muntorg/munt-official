// Copyright (c) 2015-2017 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#include "util.h"
#include "wallet/wallet.h"
#include "validation.h"

static bool alreadyInRescan = false;
void rescanThread()
{
    if (alreadyInRescan)
        return;
    alreadyInRescan = true;
    pactiveWallet->ScanForWalletTransactions(chainActive.Genesis(), true);
    alreadyInRescan = false;
}

std::string StringFromSeedType(CHDSeed* seed)
{
    switch(seed->m_type)
    {
        case CHDSeed::CHDSeed::BIP32:
            return "BIP32";
        case CHDSeed::CHDSeed::BIP32Legacy:
            return "BIP32 Legacy";
        case CHDSeed::CHDSeed::BIP44:
            return "BIP44";
        case CHDSeed::CHDSeed::BIP44External:
            return "BIP44 External";
        case CHDSeed::CHDSeed::BIP44NoHardening:
            return "BIP44 No Hardening";
    }
    return "unknown";
}

CHDSeed::SeedType SeedTypeFromString(std::string type)
{
    if (type == "BIP32")
        return CHDSeed::CHDSeed::BIP32;
    else if (type == "BIP32 Legacy" || type == "Bip32L")
        return CHDSeed::CHDSeed::BIP32Legacy;
    else if (type == "BIP44")
        return CHDSeed::CHDSeed::BIP44;
    else if (type == "BIP44 External" || type == "BIP44E")
        return CHDSeed::CHDSeed::BIP44External;
    else if (type == "BIP44 No Hardening" || type == "BIP44NH")
        return CHDSeed::CHDSeed::BIP44NoHardening;

    throw std::runtime_error("Invalid seed type");

    return CHDSeed::CHDSeed::BIP44;
}

// Phase 2 becomes active after 75% of miners signal upgrade.
// After activation creation of 'backwards compatible' PoW2 addresses becomes possible.
std::map<uint256, bool> phase2ActivationCache;
bool IsPow2Phase2Active(const CBlockIndex* pIndex, const Consensus::Params& params)
{
    if (!pIndex)
        return false;

    if (phase2ActivationCache.find(pIndex->GetBlockHashLegacy()) != phase2ActivationCache.end())
        return phase2ActivationCache[pIndex->GetBlockHashLegacy()];

    phase2ActivationCache[pIndex->GetBlockHashLegacy()] = (VersionBitsState(pIndex, params, Consensus::DEPLOYMENT_POW2_PHASE2, versionbitscache) == THRESHOLD_ACTIVE);
    return phase2ActivationCache[pIndex->GetBlockHashLegacy()];
}


// Phase 3 becomes active after 200 or more witnessing addresses are present on the chain, as well as a combined witness weight of 20 000 000 or more.
// 'backwards compatible' witnessing becomes possible at this point.

// prevhash of blocks continue to point to previous PoW block alone.
// prevhash of witness block is stored in coinbase.
std::map<uint256, bool> phase3ActivationCache;
bool IsPow2Phase3Active(const CBlockIndex* pIndex, const Consensus::Params& params)
{
    if (!pIndex)
        return false;

    if (phase3ActivationCache.find(pIndex->GetBlockHashLegacy()) != phase3ActivationCache.end())
        return phase3ActivationCache[pIndex->GetBlockHashLegacy()];

    int64_t nNumWitnessAddresses;
    int64_t nTotalWeight;

    GetPow2NetworkWeight(pIndex, nNumWitnessAddresses, nTotalWeight);

    const int64_t nNumWitnessAddressesRequired = IsArgSet("-testnet") ? 10 : 200;
    if (nNumWitnessAddresses >= nNumWitnessAddressesRequired && nTotalWeight > 20000000)
    {
        phase3ActivationCache[pIndex->GetBlockHashLegacy()] = true;
        return true;
    }
    phase3ActivationCache[pIndex->GetBlockHashLegacy()] = false;
    return false;
}

// Phase 4 becomes active after witnesses signal that 95% of peers are upgraded, for an entire 'signal window'
// Creation of new 'backwards compatible' witness addresses becomes impossible at this point.
// Full 'backwards incompatible' witnessing triggers at this point.
// New 'segsig' transaction format triggers at this point.
// All peers of version < 2.0 can no longer transact at this point.

// prevhash of blocks starts to point to witness header instead of PoW header
std::map<uint256, bool> phase4ActivationCache;
bool IsPow2Phase4Active(const CBlockIndex* pIndex, const Consensus::Params& params)
{
    if (!pIndex)
        return false;

    if (phase4ActivationCache.find(pIndex->GetBlockHashLegacy()) != phase4ActivationCache.end())
        return phase4ActivationCache[pIndex->GetBlockHashLegacy()];

    //PoS version bits
    phase4ActivationCache[pIndex->GetBlockHashLegacy()] = (VersionBitsState(pIndex, params, Consensus::DEPLOYMENT_POW2_PHASE4, versionbitscache) == THRESHOLD_ACTIVE);
    return phase4ActivationCache[pIndex->GetBlockHashLegacy()];
}

bool IsPow2Phase2Active(const CBlockIndex* pindexPrev, const CChainParams& chainparams) { return IsPow2Phase2Active(pindexPrev, chainparams.GetConsensus()); }
bool IsPow2Phase3Active(const CBlockIndex* pindexPrev, const CChainParams& chainparams) { return IsPow2Phase3Active(pindexPrev, chainparams.GetConsensus()); }
bool IsPow2Phase4Active(const CBlockIndex* pindexPrev, const CChainParams& chainparams) { return IsPow2Phase4Active(pindexPrev, chainparams.GetConsensus()); }

//fixme: (GULDEN) (POW2) (2.0) (HIGH) Cache the activation block hash value across runs.
int64_t GetPoW2Phase3ActivationTime()
{
    if (IsPow2Phase3Active(chainActive.Tip(), Params()))
    {
        CBlockIndex* pIndex = chainActive.Tip();
        while (pIndex->pprev && IsPow2Phase3Active(pIndex->pprev, Params()))
        {
            pIndex = pIndex->pprev;
        }
        
        return pIndex->nTime;
    }
    return std::numeric_limits<int64_t>::max();
}

void GetPow2NetworkWeight(const CBlockIndex* pIndex, int64_t& nNumWitnessAddresses, int64_t& nTotalWeight)
{
    //implement
}
