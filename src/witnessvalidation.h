// Copyright (c) 2017-2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#ifndef GULDEN_WITNESS_VALIDATION_H
#define GULDEN_WITNESS_VALIDATION_H

#include "validation.h"

//fixme: (2.0) - Properly document all of these; including pre/post conditions; implement unit tests.

/** Global variable that points to the witness coins database (protected by cs_main) */
extern CWitViewDB* ppow2witdbview;
extern std::shared_ptr<CCoinsViewCache> ppow2witTip;

struct RouletteItem
{
public:
    RouletteItem(const COutPoint& outpoint_, const Coin& coin_, int64_t nWeight_, int64_t nAge_) : outpoint(outpoint_), coin(coin_), nWeight(nWeight_), nAge(nAge_), nCumulativeWeight(0) {};
    COutPoint outpoint;
    Coin coin;
    uint64_t nWeight;
    uint64_t nAge;
    uint64_t nCumulativeWeight;

    friend inline bool operator<(const RouletteItem& a, const RouletteItem& b)
    {
        if (a.nAge == b.nAge)
            return a.outpoint < b.outpoint;
        return a.nAge < b.nAge;
    }
    friend inline bool operator<(const RouletteItem& a, const uint64_t& b)
    {
        return a.nCumulativeWeight < b;
    }
};
struct CGetWitnessInfo
{
    std::map<COutPoint, Coin> allWitnessCoins;
    std::vector<RouletteItem> witnessSelectionPoolUnfiltered;
    std::vector<RouletteItem> witnessSelectionPoolFiltered;
    CTxOut selectedWitnessTransaction;
    COutPoint selectedWitnessOutpoint;
    Coin selectedWitnessCoin;
    uint64_t selectedWitnessBlockHeight = 0;

    uint64_t nTotalWeight = 0;
    uint64_t nReducedTotalWeight = 0;
};

int GetPoW2WitnessCoinbaseIndex(const CBlock& block);

CAmount GetBlockSubsidyWitness(int nHeight, const Consensus::Params& consensusParams);

// Returns all competing orphans at same height and same parent as current tip.
// NB! It is important that we consider height and not chain weight here.
// If there is a stalled chain due to absentee signer(s) delta may drop the difficulty so competing PoW blocks will have a lower chain weight, but we still want to sign them to get the chain moving again.
std::vector<CBlockIndex*> GetTopLevelPoWOrphans(const int64_t nHeight, const uint256& prevhash);

std::vector<CBlockIndex*> GetTopLevelWitnessOrphans(const int64_t nHeight);

// Retrieve the witness for a PoW block (phase 3 only)
CBlockIndex* GetWitnessOrphanForBlock(const int64_t nHeight, const uint256& prevHash, const uint256& powHash);

bool ForceActivateChain(CBlockIndex* pActivateIndex, std::shared_ptr<const CBlock> pblock, CValidationState& state, const CChainParams& chainparams, CChain& currentChain, CCoinsViewCache& coinView);

bool ForceActivateChainWithBlockAsTip(CBlockIndex* pActivateIndex, std::shared_ptr<const CBlock> pblock, CValidationState& state, const CChainParams& chainparams, CChain& currentChain, CCoinsViewCache& coinView, CBlockIndex* pnewblockastip);

// The period in which we are required to witness before we are forcefully removed from the pool.
uint64_t expectedWitnessBlockPeriod(uint64_t nWeight, uint64_t networkTotalWeight);

// The average frequency which we are expected to witness in.
uint64_t estimatedWitnessBlockPeriod(uint64_t nWeight, uint64_t networkTotalWeight);

bool getAllUnspentWitnessCoins(CChain& chain, const CChainParams& chainParams, const CBlockIndex* pPreviousIndexChain, std::map<COutPoint, Coin>& allWitnessCoins, CBlock* newBlock=nullptr, CCoinsViewCache* viewOverride=nullptr);

bool GetWitnessHelper(CChain& chain, const CChainParams& chainParams, CCoinsViewCache* viewOverride, CBlockIndex* pPreviousIndexChain, uint256 blockHash, CGetWitnessInfo& witnessInfo, uint64_t nBlockHeight);

bool GetWitnessInfo(CChain& chain, const CChainParams& chainParams, CCoinsViewCache* viewOverride, CBlockIndex* pPreviousIndexChain, CBlock block, CGetWitnessInfo& witnessInfo, uint64_t nBlockHeight);

bool GetWitness(CChain& chain, const CChainParams& chainParams, CCoinsViewCache* viewOverride, CBlockIndex* pPreviousIndexChain, CBlock block, CGetWitnessInfo& witnessInfo);

bool witnessHasExpired(uint64_t nWitnessAge, uint64_t nWitnessWeight, uint64_t nNetworkTotalWitnessWeight);

bool ExtractWitnessBlockFromWitnessCoinbase(CChain& chain, int nWitnessCoinbaseIndex, const CBlockIndex* pindexPrev, const CBlock& block, const CChainParams& chainParams, CCoinsViewCache& view, CBlock& embeddedWitnessBlock);

bool WitnessCoinbaseInfoIsValid(CChain& chain, int nWitnessCoinbaseIndex, const CBlockIndex* pindexPrev, const CBlock& block, const CChainParams& chainParams, CCoinsViewCache& view);

#endif // GULDEN_WITNESS_VALIDATION_H
