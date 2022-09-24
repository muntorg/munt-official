// Copyright (c) 2017-2022 The Centure developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the Libre Chain License, see the accompanying
// file COPYING

#ifndef WITNESS_VALIDATION_H
#define WITNESS_VALIDATION_H

#include "validation/validation.h"
#include <boost/container/flat_set.hpp>

//fixme: (PHASE5) - Properly document all of these; including pre/post conditions;
//fixme: (PHASE5) implement unit tests.


// Encapusulate the bare minimum information we need to know about every witness address in order to select/verify a valid witness for a block
// Without any of the additional information thats necessary for other parts of the witness system (e.g. spending key) but not this specific function
// And without information that can be derived from this core information (e.g. age)
class SimplifiedWitnessRouletteItem
{
public:
    SimplifiedWitnessRouletteItem(){};
    SimplifiedWitnessRouletteItem(const std::tuple<const CTxOut, CTxOutPoW2Witness, COutPoint>& witnessInput)
    {    
        blockNumber = std::get<2>(witnessInput).getTransactionBlockNumber();
        transactionIndex = std::get<2>(witnessInput).getTransactionIndex();
        transactionOutputIndex = std::get<2>(witnessInput).n;

        lockUntilBlock = std::get<1>(witnessInput).lockUntilBlock;
        lockFromBlock = std::get<1>(witnessInput).lockFromBlock;
        if (lockFromBlock == 0)
        {
            lockFromBlock = blockNumber;
        }
        witnessPubKeyID = std::get<1>(witnessInput).witnessKeyID;
        nValue = std::get<0>(witnessInput).nValue;
    }
    uint64_t blockNumber;
    uint64_t transactionIndex;
    uint32_t transactionOutputIndex;
    uint64_t lockUntilBlock;
    uint64_t lockFromBlock;
    CKeyID witnessPubKeyID;
    CAmount nValue;
    
    uint64_t GetLockLength()
    {
        return (lockUntilBlock-lockFromBlock)+1;
    }
    
    friend inline bool operator!=(const SimplifiedWitnessRouletteItem& a, const SimplifiedWitnessRouletteItem& b)
    {
        return !(a == b);
    }

    friend inline bool operator==(const SimplifiedWitnessRouletteItem& a, const SimplifiedWitnessRouletteItem& b)
    {
        if (a.blockNumber != b.blockNumber ||
            a.transactionIndex != b.transactionIndex ||
            a.transactionOutputIndex != b.transactionOutputIndex ||
            a.lockUntilBlock != b.lockUntilBlock ||
            a.nValue != b.nValue ||
            a.witnessPubKeyID != b.witnessPubKeyID
        )
        {
            return false;
        }
        return true;
    }
    
    //NB! This ordering must precisely match the ordering of RouletteItem::operator< (assuming all items use index based outpoints)
    friend inline bool operator<(const SimplifiedWitnessRouletteItem& a, const SimplifiedWitnessRouletteItem& b)
    {
        if (a.blockNumber == b.blockNumber)
        {
            if (a.transactionIndex == b.transactionIndex)
            {
                return a.transactionOutputIndex < b.transactionOutputIndex;
            }
            return a.transactionIndex < b.transactionIndex;
        }
        return a.blockNumber < b.blockNumber;
    }
};

// Simplified view of the entire "witness UTXO" encapsulated as basic SimplifiedWitnessRouletteItem items instead of transactions
class SimplifiedWitnessUTXOSet
{
public:
    uint256 currentTipForSet;
    //fixme: (OPTIMISE) - We make an educated guess that for our specific case 'flat_set' will perform well
    //We base this on a few things:
    // 1) Set size is not very large (around 600-1200 elements in size)
    // 2) We have to iterate the entire set a lot for witness selection (for which contiguous memory is good)
    // 3) We only perform at most a few inserts/deletions per block, so slightly slower insertion/deletion isn't necessarily that bad
    //However we don't know this for a fact; therefore this should actually be tested against other alternatives and the most performant container for the job chosen.
    boost::container::flat_set<SimplifiedWitnessRouletteItem> witnessCandidates;
    
    friend inline bool operator!=(const SimplifiedWitnessUTXOSet& a, const SimplifiedWitnessUTXOSet& b)
    {
        return !(a == b);
    }
    
    friend inline bool operator==(const SimplifiedWitnessUTXOSet& a, const SimplifiedWitnessUTXOSet& b)
    {
        if (a.currentTipForSet != b.currentTipForSet)
            return false;

        if (a.witnessCandidates.size() != b.witnessCandidates.size())
            return false;

        for (uint64_t i=0; i<a.witnessCandidates.size(); ++i)
        {
            auto aComp = *a.witnessCandidates.nth(i);
            auto bComp = *b.witnessCandidates.nth(i);
            if (aComp != bComp)
            {
                return false;
            }
        }
        return true;
    }
};

SimplifiedWitnessUTXOSet GenerateSimplifiedWitnessUTXOSetFromUTXOSet(std::map<COutPoint, Coin> allWitnessCoinsIndexBased);
bool GetSimplifiedWitnessUTXOSetForIndex(const CBlockIndex* pBlockIndex, SimplifiedWitnessUTXOSet& pow2SimplifiedWitnessUTXOForBlock);
bool GetSimplifiedWitnessUTXODeltaForBlock(const CBlockIndex* pBlockIndexPrev, const CBlock& block, std::shared_ptr<SimplifiedWitnessUTXOSet> pow2SimplifiedWitnessUTXOForPrevBlock, std::vector<unsigned char>& compWitnessUTXODelta, CPubKey* pubkey);

/** Global variable that points to the witness coins database (protected by cs_main) */
extern CWitViewDB* ppow2witdbview;
extern std::shared_ptr<CCoinsViewCache> ppow2witTip;
extern SimplifiedWitnessUTXOSet pow2SimplifiedWitnessUTXO;

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
    //! All unspent witness coins on the network
    std::map<COutPoint, Coin> allWitnessCoins;

    //! All witness coins on the network that meet the basic criteria of minimum weight/amount
    std::vector<RouletteItem> witnessSelectionPoolUnfiltered;

    //! All witness coins on the network that were considered eligible for the purpose of selecting the winner. (All contenders)
    std::vector<RouletteItem> witnessSelectionPoolFiltered;

    //! Transaction output of the selected witness
    CTxOut selectedWitnessTransaction;
    
    //! The index of the selected witness transaction in the filtered pool
    uint64_t selectedWitnessIndex = 0;

    //! Coin object containing the output of the selected witness
    COutPoint selectedWitnessOutpoint;

    //! Coin object containing the output of the selected witness
    Coin selectedWitnessCoin;

    uint64_t selectedWitnessBlockHeight = 0;

    //! The total weight of all witness accounts
    uint64_t nTotalWeightRaw = 0;

    //! The total weight of all witness accounts considered eligible for this round of witnessing
    uint64_t nTotalWeightEligibleRaw = 0;

    //! The reduced total weight after applying the individual weight maximum
    uint64_t nTotalWeightEligibleAdjusted = 0;

    //! The maximum individual weight that any single address was allowed when performing the reduction.
    uint64_t nMaxIndividualWeight = 0;
};

int GetPoW2WitnessCoinbaseIndex(const CBlock& block);

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

bool getAllUnspentWitnessCoins(CChain& chain, const CChainParams& chainParams, const CBlockIndex* pPreviousIndexChain, std::map<COutPoint, Coin>& allWitnessCoins, CBlock* newBlock=nullptr, CCoinsViewCache* viewOverride=nullptr, bool forceIndexBased=false);

bool GetWitnessHelper(uint256 blockHash, CGetWitnessInfo& witnessInfo, uint64_t nBlockHeight);

bool GetWitnessInfo(CChain& chain, const CChainParams& chainParams, CCoinsViewCache* viewOverride, CBlockIndex* pPreviousIndexChain, CBlock block, CGetWitnessInfo& witnessInfo, uint64_t nBlockHeight);

bool GetWitness(CChain& chain, const CChainParams& chainParams, CCoinsViewCache* viewOverride, CBlockIndex* pPreviousIndexChain, CBlock block, CGetWitnessInfo& witnessInfo);
bool GetWitnessFromSimplifiedUTXO(SimplifiedWitnessUTXOSet simplifiedWitnessUTXO, const CBlockIndex* pBlockIndex, CGetWitnessInfo& witnessInfo);

bool witnessHasExpired(uint64_t nWitnessAge, uint64_t nWitnessWeight, uint64_t nNetworkTotalWitnessWeight);

#endif
