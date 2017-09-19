// Copyright (c) 2015-2017 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#include "util.h"
#include "wallet/wallet.h"
#include "validation.h"

#include "txdb.h"
#include "coins.h"

#include "primitives/transaction.h"


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


// Phase 5 becomes active after all 'backwards compatible' witness addresses have been flushed from the system.
// At this point 'backwards compatible' witness addresses in the chain are treated simply as anyone can spend transactions (as they have all been spent this is fine) and old witness data is discarded.
bool IsPow2Phase5Active(const CBlockIndex* pIndex, const Consensus::Params& params)
{
    if (!pIndex)
        return false;
    
    if (!IsPow2Phase4Active(pIndex, params))
        return false;
    
    std::map<COutPoint, Coin> allWitnessCoins;
    CCoinsViewCursor* cursor = ppow2witdbview->Cursor();
    while (cursor && cursor->Valid())
    {
        COutPoint outPoint;
        if (!cursor->GetKey(outPoint))
            throw std::runtime_error("Error fetching record from witness cache.");
            
        Coin outCoin;
        if (!cursor->GetValue(outCoin))
            throw std::runtime_error("Error fetching record from witness cache.");
        
        allWitnessCoins.emplace(std::make_pair(outPoint, outCoin));
        
        cursor->Next();
    }
    for (auto iter : ppow2witTip->GetCachedCoins())
    {
        allWitnessCoins[iter.first] = iter.second.coin;
    }
    
    // If any PoW2WitnessOutput remain then we aren't active yet.
    for (auto iter : allWitnessCoins)
    {
        if (iter.second.out.GetType() != CTxOutType::PoW2WitnessOutput)
            return false;
    }
    
    return true;
}


bool IsPow2WitnessingActive(const CBlockIndex* pIndex, const Consensus::Params& params)
{
    if (!pIndex)
        return false;
    
    return IsPow2Phase3Active(pIndex, params) || IsPow2Phase4Active(pIndex, params) || IsPow2Phase5Active(pIndex, params);
}

int GetPoW2Phase(const CBlockIndex* pIndex, const Consensus::Params& params)
{
    if (IsPow2Phase5Active(pIndex, params))
    {
        return 5;
    }
    if (IsPow2Phase4Active(pIndex, params))
    {
        return 4;
    }
    if (IsPow2Phase3Active(pIndex, params))
    {
        return 3;
    }
    if (IsPow2Phase2Active(pIndex, params))
    {
        return 2;
    }
    return 1;
}

bool IsPow2Phase2Active(const CBlockIndex* pindexPrev, const CChainParams& chainparams) { return IsPow2Phase2Active(pindexPrev, chainparams.GetConsensus()); }
bool IsPow2Phase3Active(const CBlockIndex* pindexPrev, const CChainParams& chainparams) { return IsPow2Phase3Active(pindexPrev, chainparams.GetConsensus()); }
bool IsPow2Phase4Active(const CBlockIndex* pindexPrev, const CChainParams& chainparams) { return IsPow2Phase4Active(pindexPrev, chainparams.GetConsensus()); }
bool IsPow2Phase5Active(const CBlockIndex* pindexPrev, const CChainParams& chainparams) { return IsPow2Phase5Active(pindexPrev, chainparams.GetConsensus()); }
bool IsPow2WitnessingActive(const CBlockIndex* pindexPrev, const CChainParams& chainparams) { return IsPow2WitnessingActive(pindexPrev, chainparams.GetConsensus()); }


int64_t GetPoW2RawWeightForAmount(int64_t nAmount, int64_t nLockLengthInBlocks)
{
    // We rebase the entire formula to to match internal monetary format (8 zeros), so that we can work with fixed point precision.
    // We rebase to 10 at the end for the final weight.
    arith_uint256 base = arith_uint256(COIN);
    #define BASE(x) (arith_uint256(x)*base)
    arith_uint256 nWeight = (arith_uint256(nAmount) * ( BASE(1) + ((BASE(nLockLengthInBlocks))/(365*576)) ) * 2) - BASE(BASE(10000));
    #undef BASE
    nWeight /= base;
    nWeight /= base;
    return nWeight.GetLow64();
}


int64_t GetPoW2LockLengthInBlocksFromOutput(CTxOut& out, uint64_t txBlockNumber)
{
    //fixme: (GULDEN) (2.0) - Check for off by 1 error (lockUntil - lockFrom)
    if ( (out.GetType() <= CTxOutType::ScriptOutput && out.scriptPubKey.IsPoW2Witness()) )
    {
        CTxOutPoW2Witness witnessDetails;
        out.scriptPubKey.ExtractPoW2WitnessFromScript(witnessDetails);
        return witnessDetails.lockUntilBlock - ( witnessDetails.lockFromBlock == 0 ? txBlockNumber : witnessDetails.lockFromBlock);
    }
    else if (out.GetType() == CTxOutType::PoW2WitnessOutput)
    {
        //implement
    }
    return 0;
}

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
    std::map<COutPoint, Coin> allWitnessCoins;
    CCoinsViewCursor* cursor = ppow2witdbview->Cursor();
    while (cursor && cursor->Valid())
    {
        COutPoint outPoint;
        if (!cursor->GetKey(outPoint))
            throw std::runtime_error("Error fetching record from witness cache.");
            
        Coin outCoin;
        if (!cursor->GetValue(outCoin))
            throw std::runtime_error("Error fetching record from witness cache.");
        
        allWitnessCoins.emplace(std::make_pair(outPoint, outCoin));
        
        cursor->Next();
    }
    for (auto iter : ppow2witTip->GetCachedCoins())
    {
        allWitnessCoins[iter.first] = iter.second.coin;
    }
    
    nNumWitnessAddresses = 0;
    nTotalWeight = 0;
    
    for (auto iter : allWitnessCoins)
    {        
        if (pIndex == nullptr || iter.second.nHeight < pIndex->nHeight)
        {
            CTxOut output = iter.second.out;

            nTotalWeight += GetPoW2RawWeightForAmount(output.nValue, GetPoW2LockLengthInBlocksFromOutput(output, iter.second.nHeight));
            ++nNumWitnessAddresses;
        }
    }
}



CBlockIndex* GetPoWBlockForPoSBlock(const CBlockIndex* pIndex)
{
    uint256 powHash = pIndex->GetBlockHashLegacy();
    if (!mapBlockIndex.count(powHash))
        return NULL;
    return mapBlockIndex[powHash];
}
