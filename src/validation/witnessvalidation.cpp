// File contains modifications by: The Gulden developers
// All modifications:
// Copyright (c) 2017-2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#include "validation/validation.h"
#include "validation/witnessvalidation.h"
#include <consensus/validation.h>
#include <witnessutil.h>
#include "timedata.h" // GetAdjustedTime()

#ifdef ENABLE_WALLET
#include "wallet/wallet.h"
#endif

#include <boost/foreach.hpp> // reverse_foreach
#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/thread.hpp>

#include "alert.h"



CWitViewDB *ppow2witdbview = NULL;
std::shared_ptr<CCoinsViewCache> ppow2witTip = NULL;
SimplifiedWitnessUTXOSet pow2SimplifiedWitnessUTXO;

//fixme: (PHASE5) Can remove this.
int GetPoW2WitnessCoinbaseIndex(const CBlock& block)
{
    int commitpos = -1;
    if (!block.vtx.empty()) {
        for (size_t o = 0; o < block.vtx[0]->vout.size(); o++) {
            if (block.vtx[0]->vout[o].GetType() <= CTxOutType::ScriptLegacyOutput)
            {
                if (block.vtx[0]->vout[o].output.scriptPubKey.size() == 143 && block.vtx[0]->vout[o].output.scriptPubKey[0] == OP_RETURN && block.vtx[0]->vout[o].output.scriptPubKey[1] == 0x50 && block.vtx[0]->vout[o].output.scriptPubKey[2] == 0x6f && block.vtx[0]->vout[o].output.scriptPubKey[3] == 0x57 && block.vtx[0]->vout[o].output.scriptPubKey[4] == 0xc2 && block.vtx[0]->vout[o].output.scriptPubKey[5] == 0xb2) {
                    commitpos = o;
                }
            }
        }
    }
    return commitpos;
}

std::vector<CBlockIndex*> GetTopLevelPoWOrphans(const int64_t nHeight, const uint256& prevHash)
{
    LOCK(cs_main);
    std::vector<CBlockIndex*> vRet;
    for (const auto candidateIter : setBlockIndexCandidates)
    {
        if (candidateIter->nVersionPoW2Witness == 0)
        {
            if (candidateIter->nHeight >= nHeight)
            {
                vRet.push_back(candidateIter);
            }
        }
    }
    return vRet;
}

std::vector<CBlockIndex*> GetTopLevelWitnessOrphans(const int64_t nHeight)
{
    std::vector<CBlockIndex*> vRet;

    // Don't hold up the witness loop if we can't get the lock, it can just check this again next time
    TRY_LOCK(cs_main, lockGetOrphans);
    if(!lockGetOrphans)
    {
        return vRet;
    }
    
    for (const auto candidateIter : setBlockIndexCandidates)
    {
        if (candidateIter->nVersionPoW2Witness != 0)
        {
            if (candidateIter->nHeight >= nHeight)
            {
                vRet.push_back(candidateIter);
            }
        }
    }
    return vRet;
}

CBlockIndex* GetWitnessOrphanForBlock(const int64_t nHeight, const uint256& prevHash, const uint256& powHash)
{
    LOCK(cs_main);
    for (const auto candidateIter : setBlockIndexCandidates)
    {
        if (candidateIter->nVersionPoW2Witness != 0)
        {
            if (candidateIter->nHeight == nHeight && candidateIter->pprev && *candidateIter->pprev->phashBlock == prevHash)
            {
                if (candidateIter->GetBlockHashLegacy() == powHash)
                {
                    return candidateIter;
                }
            }
        }
    }
    return NULL;
}

static bool ForceActivateChainStep(CValidationState& state, CChain& currentChain, const CChainParams& chainparams, CBlockIndex* pindexMostWork, const std::shared_ptr<const CBlock>& pblock, bool& fInvalidFound, CCoinsViewCache& coinView)
{
    AssertLockHeld(cs_main); // Required for ReadBlockFromDisk.
    const CBlockIndex *pindexFork = currentChain.FindFork(pindexMostWork);

    if (!pindexFork)
    {
        while (currentChain.Tip() && currentChain.Tip()->nHeight >= pindexMostWork->nHeight - 1)
        {
            CBlockIndex* pindexNew = currentChain.Tip()->pprev;
            std::shared_ptr<CBlock> pblock = std::make_shared<CBlock>();
            CBlock& block = *pblock;
            if (!ReadBlockFromDisk(block, currentChain.Tip(), chainparams))
                return false;
            if (DisconnectBlock(block, currentChain.Tip(), coinView) != DISCONNECT_OK)
                return false;
            currentChain.SetTip(pindexNew);
        }
        pindexFork = currentChain.FindFork(pindexMostWork);
    }

    // Disconnect active blocks which are no longer in the best chain.
    while (currentChain.Tip() && currentChain.Tip() != pindexFork) {
        CBlockIndex* pindexNew = currentChain.Tip()->pprev;
        std::shared_ptr<CBlock> pblock = std::make_shared<CBlock>();
        CBlock& block = *pblock;
        if (!ReadBlockFromDisk(block, currentChain.Tip(), chainparams))
            return false;
        if (DisconnectBlock(block, currentChain.Tip(), coinView) != DISCONNECT_OK)
            return false;
        currentChain.SetTip(pindexNew);
    }

    // Build list of new blocks to connect.
    std::vector<CBlockIndex*> vpindexToConnect;
    bool fContinue = true;
    int nHeight = pindexFork ? pindexFork->nHeight : -1;
    while (fContinue && nHeight != pindexMostWork->nHeight) {
        // Don't iterate the entire list of potential improvements toward the best tip, as we likely only need
        // a few blocks along the way.
        int nTargetHeight = std::min(nHeight + 32, pindexMostWork->nHeight);
        vpindexToConnect.clear();
        vpindexToConnect.reserve(nTargetHeight - nHeight);
        CBlockIndex *pindexIter = pindexMostWork->GetAncestor(nTargetHeight);
        while (pindexIter && pindexIter->nHeight != nHeight) {
            vpindexToConnect.push_back(pindexIter);
            pindexIter = pindexIter->pprev;
        }
        nHeight = nTargetHeight;

        // Connect new blocks.
        BOOST_REVERSE_FOREACH(CBlockIndex *pindexConnect, vpindexToConnect) {
            std::shared_ptr<CBlock> pblockConnect = nullptr;
            if (pindexConnect != pindexMostWork || !pblock)
            {
                pblockConnect = std::make_shared<CBlock>();
                CBlock& block = *pblockConnect;
                if (!ReadBlockFromDisk(block, pindexConnect, chainparams))
                    return false;
            }
            bool rv = ConnectBlock(currentChain, pblockConnect?*pblockConnect:*pblock, state, pindexConnect, coinView, chainparams, false, false, false, false);
            if (!rv)
                return false;
            currentChain.SetTip(pindexConnect);
        }
    }

    return true;
}

// pblock is either NULL or a pointer to a CBlock corresponding to pActiveIndex, to bypass loading it again from disk.
bool ForceActivateChain(CBlockIndex* pActivateIndex, std::shared_ptr<const CBlock> pblock, CValidationState& state, const CChainParams& chainparams, CChain& currentChain, CCoinsViewCache& coinView)
{
    DO_BENCHMARK("WIT: ForceActivateChain", BCLog::BENCH|BCLog::WITNESS);

    CBlockIndex* pindexNewTip = nullptr;
    do {
        {
            LOCK(cs_main);

            // Whether we have anything to do at all.
            if (pActivateIndex == NULL || pActivateIndex == currentChain.Tip())
                return true;

            bool fInvalidFound = false;
            std::shared_ptr<const CBlock> nullBlockPtr;
            if (!ForceActivateChainStep(state, currentChain, chainparams, pActivateIndex, pblock, fInvalidFound, coinView))
                return false;

            if (fInvalidFound) {
                return false;
            }
            pindexNewTip = currentChain.Tip();
        }
        // When we reach this point, we switched to a new tip (stored in pindexNewTip).
    } while (pindexNewTip != pActivateIndex);

    return true;
}

bool ForceActivateChainWithBlockAsTip(CBlockIndex* pActivateIndex, std::shared_ptr<const CBlock> pblock, CValidationState& state, const CChainParams& chainparams, CChain& currentChain, CCoinsViewCache& coinView, CBlockIndex* pnewblockastip)
{
    if(!ForceActivateChain(pActivateIndex, pblock, state, chainparams, currentChain, coinView))
        return false;
    return ForceActivateChain(pnewblockastip, nullptr, state, chainparams, currentChain, coinView);
}

uint64_t expectedWitnessBlockPeriod(uint64_t nWeight, uint64_t networkTotalWeight)
{
    if (nWeight == 0 || networkTotalWeight == 0)
        return 0;

    if (nWeight > networkTotalWeight/100)
        nWeight = networkTotalWeight/100;

    static const arith_uint256 base = arith_uint256(100000000) * arith_uint256(100000000) * arith_uint256(100000000);
    #define BASE(x) (arith_uint256(x)*base)
    #define AI(x) arith_uint256(x)
    return 100 + std::max(( ((BASE(1)/((BASE(nWeight)/AI(networkTotalWeight))))).GetLow64() * 10 ), (uint64_t)1000);
    #undef AI
    #undef BASE
}

uint64_t estimatedWitnessBlockPeriod(uint64_t nWeight, uint64_t networkTotalWeight)
{
    DO_BENCHMARK("WIT: estimatedWitnessBlockPeriod", BCLog::BENCH|BCLog::WITNESS);

    if (nWeight == 0 || networkTotalWeight == 0)
        return 0;

    if (nWeight > networkTotalWeight/100)
        nWeight = networkTotalWeight/100;

    static const arith_uint256 base = arith_uint256(100000000) * arith_uint256(100000000) * arith_uint256(100000000);
    #define BASE(x) (arith_uint256(x)*base)
    #define AI(x) arith_uint256(x)
    return 100 + ((BASE(1)/((BASE(nWeight)/AI(networkTotalWeight))))).GetLow64();
    #undef AI
    #undef BASE
}


bool getAllUnspentWitnessCoins(CChain& chain, const CChainParams& chainParams, const CBlockIndex* pPreviousIndexChain_, std::map<COutPoint, Coin>& allWitnessCoins, CBlock* newBlock, CCoinsViewCache* viewOverride, bool forceIndexBased)
{
    DO_BENCHMARK("WIT: getAllUnspentWitnessCoins", BCLog::BENCH|BCLog::WITNESS);

    #ifdef ENABLE_WALLET
    LOCK2(cs_main, pactiveWallet?&pactiveWallet->cs_wallet:NULL);
    #else
    LOCK(cs_main);
    #endif

    assert(pPreviousIndexChain_);

    allWitnessCoins.clear();
    //fixme: (PHASE5) Add more error handling to this function.
    // Sort out pre-conditions.
    // We have to make sure that we are using a view and chain that includes the PoW block we are witnessing and all of its transactions as the tip.
    // It won't necessarily be part of the chain yet; if we are in the process of witnessing; or if the block is an older one on a fork; because only blocks that have already been witnessed can be part of the chain.
    // So we have to temporarily force disconnect/reconnect of blocks as necessary to make a temporary working chain that suits the properties we want.
    // NB!!! - It is important that we don't flush either of these before destructing, we want to throw the result away.
    CCoinsViewCache viewNew(viewOverride?viewOverride:pcoinsTip);

    if ((uint64_t)pPreviousIndexChain_->nHeight < Params().GetConsensus().pow2Phase2FirstBlockHeight)
        return true;

    // We work on a clone of the chain to prevent modifying the actual chain.
    CBlockIndex* pPreviousIndexChain = nullptr;
    CCloneChain tempChain(chain, GetPow2ValidationCloneHeight(chain, pPreviousIndexChain_, 2), pPreviousIndexChain_, pPreviousIndexChain);
    CValidationState state;
    assert(pPreviousIndexChain);

    // Force the tip of the chain to the block that comes before the block we are examining.
    // For phase 3 this must be a PoW block - from phase 4 it should be a witness block 
    if (pPreviousIndexChain->nHeight == 0)
    {
        ForceActivateChain(pPreviousIndexChain, nullptr, state, chainParams, tempChain, viewNew);
    }
    else
    {
        if (pPreviousIndexChain->nVersionPoW2Witness==0 || IsPow2Phase4Active(pPreviousIndexChain->pprev))
        {
            ForceActivateChain(pPreviousIndexChain, nullptr, state, chainParams, tempChain, viewNew);
        }
        else
        {
            CBlockIndex* pPreviousIndexChainPoW = new CBlockIndex(*GetPoWBlockForPoSBlock(pPreviousIndexChain));
            assert(pPreviousIndexChainPoW);
            pPreviousIndexChainPoW->pprev = pPreviousIndexChain->pprev;
            ForceActivateChainWithBlockAsTip(pPreviousIndexChain->pprev, nullptr, state, chainParams, tempChain, viewNew, pPreviousIndexChainPoW);
            pPreviousIndexChain = tempChain.Tip();
        }
    }

    // If we have been passed a new tip block (not yet part of the chain) then add it to the chain now.
    if (newBlock)
    {
        // Strip any witness information from the block we have been given we want a non-witness block as the tip in order to calculate the witness for it.
        if (newBlock->nVersionPoW2Witness != 0)
        {
            for (unsigned int i = 1; i < newBlock->vtx.size(); i++)
            {
                if (newBlock->vtx[i]->IsCoinBase() && newBlock->vtx[i]->IsPoW2WitnessCoinBase())
                {
                    while (newBlock->vtx.size() > i)
                    {
                        newBlock->vtx.pop_back();
                    }
                    break;
                }
            }
            newBlock->nVersionPoW2Witness = 0;
            newBlock->nTimePoW2Witness = 0;
            newBlock->hashMerkleRootPoW2Witness = uint256();
            newBlock->witnessHeaderPoW2Sig.clear();
            newBlock->witnessUTXODelta.clear();
        }

        // Place the block in question at the tip of the chain.
        CBlockIndex* indexDummy = new CBlockIndex(*newBlock);
        indexDummy->pprev = pPreviousIndexChain;
        indexDummy->nHeight = pPreviousIndexChain->nHeight + 1;
        if (!ConnectBlock(tempChain, *newBlock, state, indexDummy, viewNew, chainParams, true, false, false, false))
        {
            //fixme: (PHASE5) If we are inside a GetWitness call ban the peer that sent us this?
            return false;
        }
        tempChain.SetTip(indexDummy);
    }

    /** Gather a list of all unspent witness outputs.
        NB!!! There are multiple layers of cache at play here, with insertions/deletions possibly having taken place at each layer.
        Therefore the order of operations is crucial, we must first iterate the lowest layer, then the second lowest and finally the highest layer.
        For each iteration we should remove items from allWitnessCoins if they have been deleted in the higher layer as the higher layer overrides the lower layer.
        GetAllCoins takes care of all of this automatically.
    **/
    if (forceIndexBased || (uint64_t)tempChain.Tip()->nHeight >= Params().GetConsensus().pow2WitnessSyncHeight)
    {
        viewNew.pChainedWitView->GetAllCoinsIndexBased(allWitnessCoins);
    }
    else
    {
        viewNew.pChainedWitView->GetAllCoins(allWitnessCoins);
    }

    return true;
}


//fixme: (PHASE5) Improve error handling.
//fixme: (PHASE5) Handle nodes with excessive pruning. //pblocktree->ReadFlag("prunedblockfiles", fHavePruned);
bool GetWitnessHelper(uint256 blockHash, CGetWitnessInfo& witnessInfo, uint64_t nBlockHeight)
{
    DO_BENCHMARK("WIT: GetWitnessHelper", BCLog::BENCH|BCLog::WITNESS);

    #ifdef ENABLE_WALLET
    LOCK2(cs_main, pactiveWallet?&pactiveWallet->cs_wallet:nullptr);
    #else
    LOCK(cs_main);
    #endif

    /** Generate the pool of potential witnesses for the given block index **/
    /** Addresses younger than nMinAge blocks are discarded **/
    uint64_t nMinAge = gMinimumParticipationAge;
    while (true)
    {
        witnessInfo.witnessSelectionPoolFiltered.clear();
        witnessInfo.witnessSelectionPoolFiltered = witnessInfo.witnessSelectionPoolUnfiltered;

        /** Eliminate addresses that have witnessed within the last `gMinimumParticipationAge` blocks **/
        witnessInfo.witnessSelectionPoolFiltered.erase(std::remove_if(witnessInfo.witnessSelectionPoolFiltered.begin(), witnessInfo.witnessSelectionPoolFiltered.end(), [&](RouletteItem& x){ return (x.nAge <= nMinAge); }), witnessInfo.witnessSelectionPoolFiltered.end());

        /** Eliminate addresses that have not witnessed within the expected period of time that they should have **/
        witnessInfo.witnessSelectionPoolFiltered.erase(std::remove_if(witnessInfo.witnessSelectionPoolFiltered.begin(), witnessInfo.witnessSelectionPoolFiltered.end(), [&](RouletteItem& x){ return witnessHasExpired(x.nAge, x.nWeight, witnessInfo.nTotalWeightRaw); }), witnessInfo.witnessSelectionPoolFiltered.end());

        /** Eliminate addresses that are within 100 blocks from lock period expiring, or whose lock period has expired. **/
        witnessInfo.witnessSelectionPoolFiltered.erase(std::remove_if(witnessInfo.witnessSelectionPoolFiltered.begin(), witnessInfo.witnessSelectionPoolFiltered.end(), [&](RouletteItem& x){ CTxOutPoW2Witness details; GetPow2WitnessOutput(x.coin.out, details); return (GetPoW2RemainingLockLengthInBlocks(details.lockUntilBlock, nBlockHeight) <= nMinAge); }), witnessInfo.witnessSelectionPoolFiltered.end());

        // We must have at least 100 accounts to keep odds of being selected down below 1% at all times.
        if (witnessInfo.witnessSelectionPoolFiltered.size() < 100)
        {
            if(!Params().IsTestnet() && nBlockHeight > 880000)
                CAlert::Notify("Warning network is experiencing low levels of witnessing participants!", true, true);


            // NB!! This part of the code should (ideally) never actually be used, it exists only for instances where there are a shortage of witnesses paticipating on the network.
            if (nMinAge == 0 || (nMinAge <= 10 && witnessInfo.witnessSelectionPoolFiltered.size() > 5))
            {
                break;
            }
            else
            {
                // Try again to reach 100 candidates with a smaller min age.
                nMinAge -= 5;
            }
        }
        else
        {
            break;
        }
    }

    if (witnessInfo.witnessSelectionPoolFiltered.size() == 0)
    {
        return error("Unable to determine any witnesses for block.");
    }

    /** Ensure the pool is sorted deterministically **/
    std::sort(witnessInfo.witnessSelectionPoolFiltered.begin(), witnessInfo.witnessSelectionPoolFiltered.end());

    /** Calculate total eligible weight **/
    witnessInfo.nTotalWeightEligibleRaw = 0;
    for (auto& item : witnessInfo.witnessSelectionPoolFiltered)
    {
        witnessInfo.nTotalWeightEligibleRaw += item.nWeight;
    }

    uint64_t genesisWeight=0;
    if (Params().numGenesisWitnesses > 0)
    {
        genesisWeight = std::max(witnessInfo.nTotalWeightEligibleRaw / Params().genesisWitnessWeightDivisor, (uint64_t)1000);
        witnessInfo.nTotalWeightEligibleRaw += Params().numGenesisWitnesses*genesisWeight;
    }
    
    /** Reduce larger weightings to a maximum weighting of 1% of network weight. **/
    /** NB!! this actually will end up a little bit more than 1% as the overall network weight will also be reduced as a result. **/
    /** This is however unimportant as 1% is in and of itself also somewhat arbitrary, simpler code is favoured here over exactness. **/
    /** So we delibritely make no attempt to compensate for this. **/
    witnessInfo.nMaxIndividualWeight = witnessInfo.nTotalWeightEligibleRaw / 100;
    witnessInfo.nTotalWeightEligibleAdjusted = 0;
    for (auto& item : witnessInfo.witnessSelectionPoolFiltered)
    {
        if (item.nWeight == 0)
            item.nWeight = genesisWeight;
        if (item.nWeight > witnessInfo.nMaxIndividualWeight)
            item.nWeight = witnessInfo.nMaxIndividualWeight;
        witnessInfo.nTotalWeightEligibleAdjusted += item.nWeight;
        item.nCumulativeWeight = witnessInfo.nTotalWeightEligibleAdjusted;
    }

    /** sha256 as random roulette spin/seed - NB! We delibritely use sha256 and -not- the normal PoW hash here as the normal PoW hash is biased towards certain number ranges by -design- (block target) so is not a good RNG... **/
    arith_uint256 rouletteSelectionSeed = UintToArith256(blockHash);

    //fixme: (PHASE5) Update whitepaper then delete this code.
    /** ensure random seed exceeds one full spin of the wheel to prevent any possible bias towards low numbers **/
    //while (rouletteSelectionSeed < witnessInfo.nTotalWeightEligibleAdjusted)
    //{
        //rouletteSelectionSeed = rouletteSelectionSeed * 2;
    //}

    /** Reduce selection number to fit within possible range of values **/
    if (rouletteSelectionSeed > arith_uint256(witnessInfo.nTotalWeightEligibleAdjusted))
    {
        // 'BigNum' Modulo operator via mathematical identity:  a % b = a - (b * int(a/b))
        rouletteSelectionSeed = rouletteSelectionSeed - (arith_uint256(witnessInfo.nTotalWeightEligibleAdjusted) * arith_uint256(rouletteSelectionSeed/arith_uint256(witnessInfo.nTotalWeightEligibleAdjusted)));
    }

    /** Perform selection **/
    auto selectedWitness = std::lower_bound(witnessInfo.witnessSelectionPoolFiltered.begin(), witnessInfo.witnessSelectionPoolFiltered.end(), rouletteSelectionSeed.GetLow64());
    witnessInfo.selectedWitnessTransaction = selectedWitness->coin.out;
    witnessInfo.selectedWitnessIndex = selectedWitness-(witnessInfo.witnessSelectionPoolFiltered.begin());
    #ifdef DEBUG
    assert((witnessInfo.witnessSelectionPoolFiltered[witnessInfo.selectedWitnessIndex].coin.out == selectedWitness->coin.out));
    #endif
    witnessInfo.selectedWitnessBlockHeight = selectedWitness->coin.nHeight;
    witnessInfo.selectedWitnessOutpoint = selectedWitness->outpoint;

    return true;
}

bool GetWitnessInfo(CChain& chain, const CChainParams& chainParams, CCoinsViewCache* viewOverride, CBlockIndex* pPreviousIndexChain, CBlock block, CGetWitnessInfo& witnessInfo, uint64_t nBlockHeight)
{
    DO_BENCHMARK("WIT: GetWitnessInfo", BCLog::BENCH|BCLog::WITNESS);

    #ifdef DISABLE_WALLET
    LOCK2(cs_main, pactiveWallet?&pactiveWallet->cs_wallet:nullptr);
    #else
    LOCK(cs_main);
    #endif

    // Fetch all unspent witness outputs for the chain in which -block- acts as the tip.
    if (!getAllUnspentWitnessCoins(chain, chainParams, pPreviousIndexChain, witnessInfo.allWitnessCoins, &block, viewOverride))
        return false;

    bool outputsShouldBeHashes = (nBlockHeight < Params().GetConsensus().pow2WitnessSyncHeight);

    // Gather all witnesses that exceed minimum weight and count the total witness weight.
    for (auto coinIter : witnessInfo.allWitnessCoins)
    {
        //fixme: (PHASE5) Unit tests
        uint64_t nAge = nBlockHeight - coinIter.second.nHeight;
        COutPoint outPoint = coinIter.first;
        assert(outPoint.isHash == outputsShouldBeHashes);
        Coin coin = coinIter.second;
        if (coin.out.nValue >= (gMinimumWitnessAmount*COIN))
        {
            uint64_t nUnused1, nUnused2;
            int64_t nWeight = GetPoW2RawWeightForAmount(coin.out.nValue, pPreviousIndexChain->nHeight, GetPoW2LockLengthInBlocksFromOutput(coin.out, coin.nHeight, nUnused1, nUnused2));
            if (nWeight < gMinimumWitnessWeight)
                continue;
            witnessInfo.witnessSelectionPoolUnfiltered.push_back(RouletteItem(outPoint, coin, nWeight, nAge));
            witnessInfo.nTotalWeightRaw += nWeight;
        }
        else if (coin.out.output.witnessDetails.lockFromBlock == 1)
        {
            int64_t nWeight = 0;
            witnessInfo.witnessSelectionPoolUnfiltered.push_back(RouletteItem(outPoint, coin, nWeight, nAge));                     
        }
    }
    
    return true;
}

bool GetWitness(CChain& chain, const CChainParams& chainParams, CCoinsViewCache* viewOverride, CBlockIndex* pPreviousIndexChain, CBlock block, CGetWitnessInfo& witnessInfo)
{
    DO_BENCHMARK("WIT: GetWitness", BCLog::BENCH|BCLog::WITNESS);

    #ifdef ENABLE_WALLET
    LOCK2(cs_main, pactiveWallet?&pactiveWallet->cs_wallet:nullptr);
    #else
    LOCK(cs_main);
    #endif

    // Fetch all the chain info (for specific block) we will need to calculate the witness.
    uint64_t nBlockHeight = pPreviousIndexChain->nHeight + 1;
    if (!GetWitnessInfo(chain, chainParams, viewOverride, pPreviousIndexChain, block, witnessInfo, nBlockHeight))
        return false;

    return GetWitnessHelper(block.GetHashLegacy(), witnessInfo, nBlockHeight);
}


bool GetWitnessFromSimplifiedUTXO(SimplifiedWitnessUTXOSet simplifiedWitnessUTXO, const CBlockIndex* pBlockIndex, CGetWitnessInfo& witnessInfo)
{
    DO_BENCHMARK("WIT: GetWitnessFromSimplifiedUTXO", BCLog::BENCH|BCLog::WITNESS);
    
    #ifdef ENABLE_WALLET
    LOCK2(cs_main, pactiveWallet?&pactiveWallet->cs_wallet:nullptr);
    #else
    LOCK(cs_main);
    #endif

    // Populate the witness info from the utxo
    uint64_t nBlockHeight = pBlockIndex->nHeight;
    
    // Equivalent of GetWitnessInfo
    {
        // Gather all witnesses that exceed minimum weight and count the total witness weight.
        for (auto simplifiedRouletteItem : simplifiedWitnessUTXO.witnessCandidates)
        {
            // We delibritely leave failCount, actionNonce and spendingKeyId unset here, as they aren't used by the code that follows.
            CTxOutPoW2Witness simplifiedWitnessInfo;
            simplifiedWitnessInfo.witnessKeyID = simplifiedRouletteItem.witnessPubKeyID;
            simplifiedWitnessInfo.lockFromBlock = simplifiedRouletteItem.lockFromBlock;
            simplifiedWitnessInfo.lockUntilBlock = simplifiedRouletteItem.lockUntilBlock;
            
            // Set our partially filled in coin item (we have filled in all the parts that GetWitnessHelper touches)
            Coin rouletteCoin = Coin(CTxOut(simplifiedRouletteItem.nValue, CTxOutPoW2Witness(simplifiedWitnessInfo)), simplifiedRouletteItem.blockNumber, 0, false, false);
            COutPoint rouletteOutpoint = COutPoint(simplifiedRouletteItem.blockNumber, simplifiedRouletteItem.transactionIndex, simplifiedRouletteItem.transactionOutputIndex);
            
            RouletteItem item(rouletteOutpoint, rouletteCoin, 0, 0);
            item.nAge = nBlockHeight - simplifiedRouletteItem.blockNumber;

            if (simplifiedRouletteItem.nValue >= (gMinimumWitnessAmount*COIN))
            {
                item.nWeight = GetPoW2RawWeightForAmount(item.coin.out.nValue, nBlockHeight, simplifiedRouletteItem.GetLockLength());
                if (item.nWeight < gMinimumWitnessWeight)
                    continue;
                witnessInfo.witnessSelectionPoolUnfiltered.push_back(item);
                witnessInfo.nTotalWeightRaw += item.nWeight;
            }
            else if (simplifiedRouletteItem.lockFromBlock == 1)
            {
                item.nWeight = 0;
                witnessInfo.witnessSelectionPoolUnfiltered.push_back(item);
            }
        }
    }

    return GetWitnessHelper(pBlockIndex->GetBlockHashLegacy(), witnessInfo, nBlockHeight);
}

bool GetWitnessFromUTXO(std::vector<RouletteItem> witnessUtxo, CBlockIndex* pBlockIndex, CGetWitnessInfo& witnessInfo)
{
    DO_BENCHMARK("WIT: GetWitnessFromUTXO", BCLog::BENCH|BCLog::WITNESS);
    
    #ifdef ENABLE_WALLET
    LOCK2(cs_main, pactiveWallet?&pactiveWallet->cs_wallet:nullptr);
    #else
    LOCK(cs_main);
    #endif

    // Populate the witness info from the utxo
    uint64_t nBlockHeight = pBlockIndex->nHeight;
    
    // Equivalent of GetWitnessInfo
    {
        // Gather all witnesses that exceed minimum weight and count the total witness weight.
        for (auto rouletteItem : witnessUtxo)
        {
            //uint64_t nAge = nBlockHeight - coinIter.second.nHeight;
            assert(!rouletteItem.outpoint.isHash);
            //COutPoint outPoint = coinIter.first;
            if (rouletteItem.coin.out.nValue >= (gMinimumWitnessAmount*COIN))
            {
                uint64_t nUnused1, nUnused2;
                int64_t nWeight = GetPoW2RawWeightForAmount(rouletteItem.coin.out.nValue, nBlockHeight, GetPoW2LockLengthInBlocksFromOutput(rouletteItem.coin.out, rouletteItem.coin.nHeight, nUnused1, nUnused2));
                if (nWeight < gMinimumWitnessWeight)
                    continue;
                witnessInfo.witnessSelectionPoolUnfiltered.push_back(rouletteItem);
                witnessInfo.nTotalWeightRaw += nWeight;
            }
            else if (rouletteItem.coin.out.output.witnessDetails.lockFromBlock == 1)
            {
                rouletteItem.nWeight = 0;
                witnessInfo.witnessSelectionPoolUnfiltered.push_back(rouletteItem);
            }
        }
    }

    return GetWitnessHelper(pBlockIndex->GetBlockHashLegacy(), witnessInfo, nBlockHeight);
}

// Ideally this should have been some hybrid of witInfo.nTotalWeight / witInfo.nReducedTotalWeight - as both independantly aren't perfect.
// Total weight is prone to be too high if there are lots of large >1% witnesses, nReducedTotalWeight is prone to be too low if there is one large witness who has recently witnessed.
// However on a large network with lots of participants this should not matter - and technical constraints make the total the best compromise
// As we need to call this from within the witness algorithm from before nReducedTotalWeight is even known. 
bool witnessHasExpired(uint64_t nWitnessAge, uint64_t nWitnessWeight, uint64_t nNetworkTotalWitnessWeight)
{
    if (nWitnessWeight == 0)
        return false;

    uint64_t nExpectedWitnessPeriod = expectedWitnessBlockPeriod(nWitnessWeight, nNetworkTotalWitnessWeight);
    return ( nWitnessAge > gMaximumParticipationAge ) || ( nWitnessAge > nExpectedWitnessPeriod );
}

const char changeTypeCreation = 0;
const char changeTypeSpend = 1;
const char changeTypeRenew = 2;
const char changeTypeRearrange = 3;
const char changeTypeIncrease = 4;
const char changeTypeChangeKey = 5;
const char changeTypeWitnessAction = 6;

struct deltaItem
{
public:
    int changeType;
    std::vector<SimplifiedWitnessRouletteItem> removedItems;
    std::vector<SimplifiedWitnessRouletteItem> addedItems;
};

bool GenerateSimplifiedWitnessUTXODeltaUndoForHeader(std::vector<unsigned char>& undoWitnessUTXODelta, SimplifiedWitnessUTXOSet& pow2SimplifiedWitnessUTXOUndo, std::vector<deltaItem>& deltaItems)
{
    CVectorWriter deltaUndoStream(SER_NETWORK, 0, undoWitnessUTXODelta, 0);

    // Play back the changes to generate the undo info
    // Note that we have to actually perform the changes as we go, and not just serialise them
    // The reason for this is that each operation that does an insert/remove can change the index of all future insert/removes
    // So if we just serialise the indexes will be wrong when we replay the changes later
    
    // First handle the witness that signed the block as a special case, as there is always only one of these at the start, then loop for everything else.
    {
        // Remove the updated witness item and put back the original one
        const auto& deltaWitnessItem = deltaItems[0];

        assert(deltaWitnessItem.addedItems.size() == 1);
        assert(deltaWitnessItem.removedItems.size() == 1);
        assert(deltaWitnessItem.changeType == changeTypeWitnessAction);

        const auto& addedItemIter = pow2SimplifiedWitnessUTXOUndo.witnessCandidates.find(deltaWitnessItem.addedItems[0]);
        
        deltaUndoStream << VARINT(pow2SimplifiedWitnessUTXOUndo.witnessCandidates.index_of(addedItemIter));
        deltaUndoStream << COMPRESSEDAMOUNT(deltaWitnessItem.removedItems[0].nValue);
        deltaUndoStream << VARINT(deltaWitnessItem.removedItems[0].blockNumber);
        deltaUndoStream << VARINT(deltaWitnessItem.removedItems[0].transactionIndex);
        deltaUndoStream << COMPACTSIZE(deltaWitnessItem.removedItems[0].transactionOutputIndex);
        
        pow2SimplifiedWitnessUTXOUndo.witnessCandidates.erase(addedItemIter);
        pow2SimplifiedWitnessUTXOUndo.witnessCandidates.insert(deltaWitnessItem.removedItems[0]);
        
        deltaItems.erase(deltaItems.begin());
    }
    
    // Loop for remaining changes, and serialise along with change type identifier
    for (const auto& deltaItem : deltaItems)
    {
        switch(deltaItem.changeType)
        {
            case changeTypeWitnessAction:
            {
                continue;
            }
            case changeTypeCreation:
            {
                // We delete the created item
                assert(deltaItem.addedItems.size() == 1);
                assert(deltaItem.removedItems.size() == 0);

                auto addedItemIter = pow2SimplifiedWitnessUTXOUndo.witnessCandidates.find(deltaItem.addedItems[0]);
                               
                deltaUndoStream << changeTypeCreation;
                deltaUndoStream << VARINT(pow2SimplifiedWitnessUTXOUndo.witnessCandidates.index_of(addedItemIter));
                pow2SimplifiedWitnessUTXOUndo.witnessCandidates.erase(addedItemIter);
                
                break;
            }
            case changeTypeSpend:
            {
                // We add the spent item back into the set
                assert(deltaItem.addedItems.size() == 0);
                assert(deltaItem.removedItems.size() == 1);

                auto originalItem = deltaItem.removedItems[0];
                pow2SimplifiedWitnessUTXOUndo.witnessCandidates.insert(originalItem);

                deltaUndoStream << changeTypeSpend;
                deltaUndoStream << VARINT(originalItem.blockNumber);
                deltaUndoStream << VARINT(originalItem.transactionIndex);
                deltaUndoStream << COMPACTSIZE(originalItem.transactionOutputIndex);
                deltaUndoStream << COMPRESSEDAMOUNT(originalItem.nValue);
                deltaUndoStream << VARINT(originalItem.lockFromBlock);
                deltaUndoStream << VARINT(originalItem.lockUntilBlock);
                deltaUndoStream << originalItem.witnessPubKeyID;
                
                break;
            }
            case changeTypeRenew:
            {
                // Revert the renewed item to its original state/position
                assert(deltaItem.addedItems.size() == 1);
                assert(deltaItem.removedItems.size() == 1);
                
                auto& renewedItem = deltaItem.addedItems[0];
                auto renewedItemIter = pow2SimplifiedWitnessUTXOUndo.witnessCandidates.find(renewedItem);
                auto& originalItem = deltaItem.removedItems[0];
                
                pow2SimplifiedWitnessUTXOUndo.witnessCandidates.erase(renewedItemIter);
                pow2SimplifiedWitnessUTXOUndo.witnessCandidates.insert(originalItem);

                deltaUndoStream << changeTypeRenew;
                deltaUndoStream << VARINT(pow2SimplifiedWitnessUTXOUndo.witnessCandidates.index_of(renewedItemIter));
                deltaUndoStream << VARINT(originalItem.blockNumber);
                deltaUndoStream << VARINT(originalItem.transactionIndex);
                deltaUndoStream << COMPACTSIZE(originalItem.transactionOutputIndex);
                
                break;
            }
            case changeTypeRearrange:
            {
                // Remove all the rearranged items and put back the originals
                assert(deltaItem.addedItems.size() > 0);
                assert(deltaItem.removedItems.size() > 0);
                                
                deltaUndoStream << changeTypeRearrange << COMPACTSIZE(deltaItem.addedItems.size()) << COMPACTSIZE(deltaItem.removedItems.size());

                for (const auto& addItem : deltaItem.addedItems)
                {
                    auto addIter = pow2SimplifiedWitnessUTXOUndo.witnessCandidates.find(addItem);
                    deltaUndoStream << VARINT(pow2SimplifiedWitnessUTXOUndo.witnessCandidates.index_of(addIter));
                    pow2SimplifiedWitnessUTXOUndo.witnessCandidates.erase(addIter);
                }
                for (const auto& removeItem : deltaItem.removedItems)
                {
                    deltaUndoStream << VARINT(removeItem.blockNumber);
                    deltaUndoStream << VARINT(removeItem.transactionIndex);
                    deltaUndoStream << COMPACTSIZE(removeItem.transactionOutputIndex);
                    deltaUndoStream << COMPRESSEDAMOUNT(removeItem.nValue);
                    pow2SimplifiedWitnessUTXOUndo.witnessCandidates.insert(removeItem);
                }
                break;
            }
            case changeTypeIncrease:
            {
                // Remove all the increased items and put back the originals
                assert(deltaItem.addedItems.size() > 0);
                assert(deltaItem.removedItems.size() > 0);
                                
                deltaUndoStream << changeTypeIncrease << COMPACTSIZE(deltaItem.addedItems.size()) << COMPACTSIZE(deltaItem.removedItems.size()) << VARINT(deltaItem.removedItems[0].lockUntilBlock);

                for (const auto& addItem : deltaItem.addedItems)
                {
                    auto addIter = pow2SimplifiedWitnessUTXOUndo.witnessCandidates.find(addItem);
                    deltaUndoStream << VARINT(pow2SimplifiedWitnessUTXOUndo.witnessCandidates.index_of(addIter));
                    pow2SimplifiedWitnessUTXOUndo.witnessCandidates.erase(addIter);
                }
                for (const auto& removeItem : deltaItem.removedItems)
                {
                    deltaUndoStream << VARINT(removeItem.blockNumber);
                    deltaUndoStream << VARINT(removeItem.transactionIndex);
                    deltaUndoStream << COMPACTSIZE(removeItem.transactionOutputIndex);
                    deltaUndoStream << COMPRESSEDAMOUNT(removeItem.nValue);
                    deltaUndoStream << VARINT(removeItem.lockFromBlock);
                    pow2SimplifiedWitnessUTXOUndo.witnessCandidates.insert(removeItem);
                }
                break;
            }
            case changeTypeChangeKey:
            {
                // Remove all the updated items and put back the items with their original key
                assert(deltaItem.addedItems.size() > 0);
                assert(deltaItem.removedItems.size() > 0);
                assert(deltaItem.addedItems.size() == deltaItem.removedItems.size());
                
                deltaUndoStream << changeTypeChangeKey << COMPACTSIZE(deltaItem.removedItems.size()) << deltaItem.removedItems[0].witnessPubKeyID;
                for (uint64_t i=0; i < deltaItem.addedItems.size(); ++i)
                {
                    // Remove added item
                    auto addIter = pow2SimplifiedWitnessUTXOUndo.witnessCandidates.find(deltaItem.addedItems[i]);                    
                    deltaUndoStream << VARINT(pow2SimplifiedWitnessUTXOUndo.witnessCandidates.index_of(addIter));
                    
                    pow2SimplifiedWitnessUTXOUndo.witnessCandidates.erase(addIter);
                    auto [insertIter, didInsert] = pow2SimplifiedWitnessUTXOUndo.witnessCandidates.insert(deltaItem.removedItems[i]);
                    (unused) insertIter;
                    if (!didInsert)
                        return false;
                    
                    // Place back original item
                    deltaUndoStream << VARINT(deltaItem.removedItems[i].blockNumber);
                    deltaUndoStream << VARINT(deltaItem.removedItems[i].transactionIndex);
                    deltaUndoStream << COMPACTSIZE(deltaItem.removedItems[i].transactionOutputIndex);
                }
                break;
            }
        }
    }
    return true;
}

bool UndoSimplifiedWitnessUTXODeltaForHeader(SimplifiedWitnessUTXOSet& pow2SimplifiedWitnessUTXO, std::vector<unsigned char>& undoWitnessUTXODelta)
{
    VectorReader deltaUndoStream(SER_NETWORK, 0, undoWitnessUTXODelta, 0);
    
    // First handle the witness that signed the block as a special case, as there is always only one of these at the start, then loop for everything else.
    {
        uint64_t selectedWitnessIndex;
        deltaUndoStream >> VARINT(selectedWitnessIndex);
        
        auto witnessIter = pow2SimplifiedWitnessUTXO.witnessCandidates.nth(selectedWitnessIndex);
        SimplifiedWitnessRouletteItem witnessItem = *witnessIter;
        SimplifiedWitnessRouletteItem updatedWitnessItem = witnessItem;
        deltaUndoStream >> COMPRESSEDAMOUNT(updatedWitnessItem.nValue);
        deltaUndoStream >> VARINT(updatedWitnessItem.blockNumber);
        deltaUndoStream >> VARINT(updatedWitnessItem.transactionIndex);
        deltaUndoStream >> COMPACTSIZE(updatedWitnessItem.transactionOutputIndex);
        
        pow2SimplifiedWitnessUTXO.witnessCandidates.erase(witnessIter);
        auto [insertIter, didInsert] = pow2SimplifiedWitnessUTXO.witnessCandidates.insert(updatedWitnessItem);
        (unused) insertIter;
        if (!didInsert)
            return false;
    }
    
    // Rest of the changes are encoded with a type
    char changeType;
    while (!deltaUndoStream.empty())
    {
        deltaUndoStream >> changeType;
        switch(changeType)
        {
            // Delete the created item
            case changeTypeCreation:
            {
                uint64_t createdItemIndex;
                deltaUndoStream >> VARINT(createdItemIndex);
                pow2SimplifiedWitnessUTXO.witnessCandidates.erase(pow2SimplifiedWitnessUTXO.witnessCandidates.nth(createdItemIndex));
                break;
            }
            // Recreate the deleted/spent item
            case changeTypeSpend:
            {
                SimplifiedWitnessRouletteItem item;
                deltaUndoStream >> VARINT(item.blockNumber);
                deltaUndoStream >> VARINT(item.transactionIndex);
                deltaUndoStream >> COMPACTSIZE(item.transactionOutputIndex);
                deltaUndoStream >> COMPRESSEDAMOUNT(item.nValue);
                deltaUndoStream >> VARINT(item.lockFromBlock);
                deltaUndoStream >> VARINT(item.lockUntilBlock);
                deltaUndoStream >> item.witnessPubKeyID;
                
                auto [insertIter, didInsert] = pow2SimplifiedWitnessUTXO.witnessCandidates.insert(item);
                (unused) insertIter;
                if (!didInsert)
                    return false;

                break;
            }
            // Remove the renewed item and place back the original item
            case changeTypeRenew:
            {
                uint64_t renewedItemIndex;
                deltaUndoStream >> VARINT(renewedItemIndex);
                
                auto itemIter = pow2SimplifiedWitnessUTXO.witnessCandidates.nth(renewedItemIndex);
                SimplifiedWitnessRouletteItem item = *itemIter;
                SimplifiedWitnessRouletteItem modifiedItem = item;
                deltaUndoStream >> VARINT(modifiedItem.blockNumber);
                deltaUndoStream >> VARINT(modifiedItem.transactionIndex);
                deltaUndoStream >> COMPACTSIZE(modifiedItem.transactionOutputIndex);
                
                pow2SimplifiedWitnessUTXO.witnessCandidates.erase(itemIter);
                auto [insertIter, didInsert] = pow2SimplifiedWitnessUTXO.witnessCandidates.insert(modifiedItem);
                (unused) insertIter;
                if (!didInsert)
                    return false;
                
                break;
            }
            // Perform the re-arrangement but in reverse
            case changeTypeRearrange:
            {
                uint64_t numItemsToRemove;
                uint64_t numItemsToAdd;
                deltaUndoStream >> COMPACTSIZE(numItemsToRemove) >> COMPACTSIZE(numItemsToAdd);
                
                SimplifiedWitnessRouletteItem item;
                for (uint64_t i=0; i<numItemsToRemove; ++i)
                {
                    uint64_t outputIndex;
                    deltaUndoStream >> VARINT(outputIndex);
                    auto itemIter = pow2SimplifiedWitnessUTXO.witnessCandidates.nth(outputIndex);
                    if (i == 0)
                    {
                        item = *itemIter;
                    }
                    pow2SimplifiedWitnessUTXO.witnessCandidates.erase(itemIter);
                }
                for (uint64_t i=0; i<numItemsToAdd; ++i)
                {
                    deltaUndoStream >> VARINT(item.blockNumber);
                    deltaUndoStream >> VARINT(item.transactionIndex);
                    deltaUndoStream >> COMPACTSIZE(item.transactionOutputIndex);
                    deltaUndoStream >> COMPRESSEDAMOUNT(item.nValue);
                    
                    auto [insertIter, didInsert] = pow2SimplifiedWitnessUTXO.witnessCandidates.insert(item);
                    (unused) insertIter;
                    if (!didInsert)
                        return false;
                }
                break;
            }
            // Reverse the increase/re-arrangement
            case changeTypeIncrease:
            {
                uint64_t numItemsToRemove;
                uint64_t numItemsToAdd;
                uint64_t originalLockUntilBlock;
                deltaUndoStream >> COMPACTSIZE(numItemsToRemove) >> COMPACTSIZE(numItemsToAdd) >> VARINT(originalLockUntilBlock);
                
                SimplifiedWitnessRouletteItem item;
                for (uint64_t i=0; i<numItemsToRemove; ++i)
                {
                    uint64_t outputIndex;
                    deltaUndoStream >> VARINT(outputIndex);
                    auto itemIter = pow2SimplifiedWitnessUTXO.witnessCandidates.nth(outputIndex);
                    if (i == 0)
                    {
                        item = *itemIter;
                        item.lockUntilBlock = originalLockUntilBlock;
                    }
                    pow2SimplifiedWitnessUTXO.witnessCandidates.erase(itemIter);
                }
                for (uint64_t i=0; i<numItemsToAdd; ++i)
                {
                    deltaUndoStream >> VARINT(item.blockNumber);
                    deltaUndoStream >> VARINT(item.transactionIndex);
                    deltaUndoStream >> COMPACTSIZE(item.transactionOutputIndex);
                    deltaUndoStream >> COMPRESSEDAMOUNT(item.nValue);
                    deltaUndoStream >> VARINT(item.lockFromBlock);
                    
                    auto [insertIter, didInsert] = pow2SimplifiedWitnessUTXO.witnessCandidates.insert(item);
                    (unused) insertIter;
                    if (!didInsert)
                        return false;
                }
                break;
            }
            // Change the key back
            case changeTypeChangeKey:
            {
                uint64_t numItems;
                deltaUndoStream >> COMPACTSIZE(numItems);
                CKeyID witnessKeyID;
                deltaUndoStream >> witnessKeyID;
                
                for (uint64_t i=0; i < numItems; ++i )
                {
                    uint64_t itemIndex;
                    deltaUndoStream >> VARINT(itemIndex);
                    auto itemIter = pow2SimplifiedWitnessUTXO.witnessCandidates.nth(itemIndex);
                    SimplifiedWitnessRouletteItem item = *itemIter;
                    
                    item.witnessPubKeyID = witnessKeyID;
                    deltaUndoStream >> VARINT(item.blockNumber);
                    deltaUndoStream >> VARINT(item.transactionIndex);
                    deltaUndoStream >> COMPACTSIZE(item.transactionOutputIndex);
                    
                    pow2SimplifiedWitnessUTXO.witnessCandidates.erase(itemIter);
                    auto [insertIter, didInsert] = pow2SimplifiedWitnessUTXO.witnessCandidates.insert(item);
                    (unused) insertIter;
                    if (!didInsert)
                        return false;
                }
                break;
            }
        }
    }
    return true;
}

//fixme: (WITNESS_SYNC) - REMOVE AFTER TESTING
#ifdef DEBUG
#define EXTRA_DELTA_TESTS 1
#endif

bool ApplySimplifiedWitnessUTXODeltaForHeader(const CBlockIndex* pIndex, SimplifiedWitnessUTXOSet& pow2SimplifiedWitnessUTXO, std::vector<unsigned char>& undoWitnessUTXODelta)
{
    #ifdef EXTRA_DELTA_TESTS
    SimplifiedWitnessUTXOSet& pow2SimplifiedWitnessUTXOOrig = pow2SimplifiedWitnessUTXO;
    #endif
    
    if (pIndex->witnessUTXODelta.size() == 0)
        return false;

    VectorReader deltaStream(SER_NETWORK, 0, pIndex->witnessUTXODelta, 0);
    
    std::vector<deltaItem> deltaItems;

    // First handle the witness that signed the block as a special case, as there is always only one of these at the start, then loop for everything else.
    {
        uint64_t selectedWitnessIndex;
        deltaStream >> VARINT(selectedWitnessIndex);
        
        auto removedItemIter = pow2SimplifiedWitnessUTXO.witnessCandidates.nth(selectedWitnessIndex);
        SimplifiedWitnessRouletteItem witnessItem = *removedItemIter;
        SimplifiedWitnessRouletteItem updatedWitnessItem = witnessItem;
        deltaStream >> COMPRESSEDAMOUNT(updatedWitnessItem.nValue);
        updatedWitnessItem.blockNumber = pIndex->nHeight;
        deltaStream >> VARINT(updatedWitnessItem.transactionIndex);
        //We don't encode the transactionOutputIndex it always becomes 0
        updatedWitnessItem.transactionOutputIndex=0;
        
        pow2SimplifiedWitnessUTXO.witnessCandidates.erase(removedItemIter);
        auto [updatedItemIter, didInsert] = pow2SimplifiedWitnessUTXO.witnessCandidates.insert(updatedWitnessItem);
        (unused) updatedItemIter;
        if (!didInsert)
            return false;
        
        deltaItem undo;
        undo.changeType=changeTypeWitnessAction;
        undo.removedItems.push_back(witnessItem);
        undo.addedItems.push_back(updatedWitnessItem);
        deltaItems.push_back(undo);
    }

    // Rest of the changes are encoded with a type
    // We store the changes as we go so that we can generate undo information
    // NB! Its not possible/enough to generate undo data on the fly, as each action can affect the index(es) of other actions, we must actually replay the actions as we generate the items (just like how we generate the actual changes)
    char changeType;
    while (!deltaStream.empty())
    {
        deltaStream >> changeType;
        switch(changeType)
        {
            case changeTypeCreation:
            {
                SimplifiedWitnessRouletteItem modifiedItem;
                modifiedItem.blockNumber = pIndex->nHeight;
                deltaStream >> VARINT(modifiedItem.transactionIndex);
                deltaStream >> COMPACTSIZE(modifiedItem.transactionOutputIndex);
                deltaStream >> COMPRESSEDAMOUNT(modifiedItem.nValue);
                modifiedItem.lockFromBlock = pIndex->nHeight;
                deltaStream >> VARINT(modifiedItem.lockUntilBlock);
                deltaStream >> modifiedItem.witnessPubKeyID;
                
                auto [insertIter, didInsert] = pow2SimplifiedWitnessUTXO.witnessCandidates.insert(modifiedItem);
                (unused) insertIter;
                if (!didInsert)
                    return false;
                
                deltaItem undo;
                undo.changeType=changeTypeCreation;
                undo.addedItems.push_back(modifiedItem);
                deltaItems.push_back(undo);                
                break;
            }
            case changeTypeSpend:
            {
                uint64_t spentWitnessSetIndex;
                deltaStream >> VARINT(spentWitnessSetIndex);
                
                auto iter = pow2SimplifiedWitnessUTXO.witnessCandidates.nth(spentWitnessSetIndex);
                SimplifiedWitnessRouletteItem originalItem = *iter;
                //only one input allowed, must be completely consumed, so we just cancel its existence in the set
                pow2SimplifiedWitnessUTXO.witnessCandidates.erase(iter);
                
                deltaItem undo;
                undo.changeType=changeTypeSpend;
                undo.removedItems.push_back(originalItem);
                deltaItems.push_back(undo);
                
                break;
            }
            case changeTypeRenew:
            {
                uint64_t renewWitnessSetIndex;
                deltaStream >> VARINT(renewWitnessSetIndex);
                
                auto itemIter = pow2SimplifiedWitnessUTXO.witnessCandidates.nth(renewWitnessSetIndex);
                SimplifiedWitnessRouletteItem originalItem = *itemIter;
                SimplifiedWitnessRouletteItem modifiedItem = originalItem;
                modifiedItem.blockNumber = pIndex->nHeight;
                deltaStream >> VARINT(modifiedItem.transactionIndex);
                deltaStream >> COMPACTSIZE(modifiedItem.transactionOutputIndex);
                
                pow2SimplifiedWitnessUTXO.witnessCandidates.erase(itemIter);
                auto [insertIter, didInsert] = pow2SimplifiedWitnessUTXO.witnessCandidates.insert(modifiedItem);
                (unused) insertIter;
                if (!didInsert)
                    return false;
                
                deltaItem undo;
                undo.changeType=changeTypeRenew;
                undo.removedItems.push_back(originalItem);
                undo.addedItems.push_back(modifiedItem);
                deltaItems.push_back(undo);
                
                break;
            }
            case changeTypeRearrange:
            {
                uint64_t numInputs;
                uint64_t numOutputs;
                deltaStream >> COMPACTSIZE(numInputs) >> COMPACTSIZE(numOutputs);
                
                deltaItem undo;
                undo.changeType=changeTypeRearrange;

                SimplifiedWitnessRouletteItem item;
                for (uint64_t i=0; i<numInputs; ++i)
                {
                    uint64_t inputIndex;
                    deltaStream >> VARINT(inputIndex);
                    auto itemIter = pow2SimplifiedWitnessUTXO.witnessCandidates.nth(inputIndex);
                    item=*itemIter;
                    pow2SimplifiedWitnessUTXO.witnessCandidates.erase(itemIter);
                    
                    undo.removedItems.push_back(item);
                }
                for (uint64_t i=0; i<numOutputs; ++i)
                {
                    item.blockNumber = pIndex->nHeight;
                    deltaStream >> VARINT(item.transactionIndex);
                    deltaStream >> COMPACTSIZE(item.transactionOutputIndex);
                    deltaStream >> COMPRESSEDAMOUNT(item.nValue);
                    auto [insertIter, didInsert] = pow2SimplifiedWitnessUTXO.witnessCandidates.insert(item);
                    (unused) insertIter;
                    if (!didInsert)
                        return false;
                    
                    undo.addedItems.push_back(item);
                }
                deltaItems.push_back(undo);
                break;
            }
            case changeTypeIncrease:
            {
                uint64_t numInputs;
                uint64_t numOutputs;
                uint64_t lockUntilBlock;
                deltaStream >> COMPACTSIZE(numInputs) >> COMPACTSIZE(numOutputs) >> VARINT(lockUntilBlock);
                
                deltaItem undo;
                undo.changeType=changeTypeIncrease;

                SimplifiedWitnessRouletteItem item;
                for (uint64_t i=0; i<numInputs; ++i)
                {
                    uint64_t inputIndex;
                    deltaStream >> VARINT(inputIndex);
                    auto itemIter = pow2SimplifiedWitnessUTXO.witnessCandidates.nth(inputIndex);
                    item = *itemIter;
                    pow2SimplifiedWitnessUTXO.witnessCandidates.erase(itemIter);
                    
                    undo.removedItems.push_back(item);
                }                
                item.lockFromBlock = pIndex->nHeight;
                item.lockUntilBlock = lockUntilBlock;
                for (uint64_t i=0; i<numOutputs; ++i)
                {
                    item.blockNumber = pIndex->nHeight;
                    deltaStream >> VARINT(item.transactionIndex);
                    deltaStream >> COMPACTSIZE(item.transactionOutputIndex);
                    deltaStream >> COMPRESSEDAMOUNT(item.nValue);
                    auto [insertIter, didInsert] = pow2SimplifiedWitnessUTXO.witnessCandidates.insert(item);
                    (unused) insertIter;
                    if (!didInsert)
                        return false;
                    
                    undo.addedItems.push_back(item);
                }
                deltaItems.push_back(undo);
                break;
            }
            case changeTypeChangeKey:
            {
                uint64_t numItems;
                deltaStream >> COMPACTSIZE(numItems);
                CKeyID witnessKeyID;
                deltaStream >> witnessKeyID;
                
                deltaItem undo;
                undo.changeType=changeTypeChangeKey;
                
                for (uint64_t i=0; i < numItems; ++i )
                {
                    uint64_t itemIndex;
                    deltaStream >> VARINT(itemIndex);
                    auto itemIter = pow2SimplifiedWitnessUTXO.witnessCandidates.nth(itemIndex);
                    SimplifiedWitnessRouletteItem originalItem = *itemIter;
                    SimplifiedWitnessRouletteItem changedItem = originalItem;
                    
                    changedItem.witnessPubKeyID = witnessKeyID;
                    changedItem.blockNumber = pIndex->nHeight;
                    deltaStream >> VARINT(changedItem.transactionIndex);
                    deltaStream >> COMPACTSIZE(changedItem.transactionOutputIndex);
                    
                    pow2SimplifiedWitnessUTXO.witnessCandidates.erase(itemIter);
                    auto [insertIter, didInsert] = pow2SimplifiedWitnessUTXO.witnessCandidates.insert(changedItem);
                    (unused) insertIter;
                    if (!didInsert)
                        return false;
                    
                    undo.removedItems.push_back(originalItem);
                    undo.addedItems.push_back(changedItem);
                }
                deltaItems.push_back(undo);
                break;
            }
        }
    }
    
    
    #ifdef EXTRA_DELTA_TESTS
    // After applying the undo information the two should be identical again
    assert(pow2SimplifiedWitnessUTXOOrig != pow2SimplifiedWitnessUTXO);
    #endif
    
    SimplifiedWitnessUTXOSet pow2SimplifiedWitnessUTXOUndo = pow2SimplifiedWitnessUTXO;
    if (!GenerateSimplifiedWitnessUTXODeltaUndoForHeader(undoWitnessUTXODelta, pow2SimplifiedWitnessUTXOUndo, deltaItems))
        return false;
    
    #ifdef EXTRA_DELTA_TESTS
    // After applying the undo information the two should be identical again
    assert(pow2SimplifiedWitnessUTXOOrig == pow2SimplifiedWitnessUTXOUndo);
    
    pow2SimplifiedWitnessUTXOUndo = pow2SimplifiedWitnessUTXO;
    UndoSimplifiedWitnessUTXODeltaForHeader(pow2SimplifiedWitnessUTXOUndo, undoWitnessUTXODelta);
    // After applying the undo information the two should be identical again
    assert(pow2SimplifiedWitnessUTXOOrig == pow2SimplifiedWitnessUTXOUndo);
    #endif
    
    return true;
}

SimplifiedWitnessUTXOSet GenerateSimplifiedWitnessUTXOSetFromUTXOSet(std::map<COutPoint, Coin> allWitnessCoinsIndexBased)
{
    SimplifiedWitnessUTXOSet witnessUTXOset;
    
    for (const auto& [outpoint, coin] : allWitnessCoinsIndexBased) 
    {
        SimplifiedWitnessRouletteItem item;
        
        item.blockNumber = outpoint.getTransactionBlockNumber();
        item.transactionIndex = outpoint.getTransactionIndex();
        item.transactionOutputIndex = outpoint.n;
        item.lockUntilBlock = coin.out.output.witnessDetails.lockUntilBlock;
        item.lockFromBlock = coin.out.output.witnessDetails.lockFromBlock;
        if (item.lockFromBlock == 0)
        {
            item.lockFromBlock = item.blockNumber;
        }
        item.witnessPubKeyID = coin.out.output.witnessDetails.witnessKeyID;
        item.nValue = coin.out.nValue;
        witnessUTXOset.witnessCandidates.insert(item);
    }
    
    return witnessUTXOset;
}

bool GetSimplifiedWitnessUTXOSetForIndex(const CBlockIndex* pBlockIndex, SimplifiedWitnessUTXOSet& pow2SimplifiedWitnessUTXOForBlock)
{
    SimplifiedWitnessUTXOSet pow2SimplifiedWitnessUTXOCopy = pow2SimplifiedWitnessUTXO;
    if (pow2SimplifiedWitnessUTXOCopy.currentTipForSet.IsNull() || pow2SimplifiedWitnessUTXO.currentTipForSet != pBlockIndex->GetBlockHashPoW2())
    {
        std::map<COutPoint, Coin> allWitnessCoinsIndexBased;
        if (!getAllUnspentWitnessCoins(chainActive, Params(), pBlockIndex, allWitnessCoinsIndexBased, nullptr, nullptr, true))
            return false;
    
        pow2SimplifiedWitnessUTXOCopy = GenerateSimplifiedWitnessUTXOSetFromUTXOSet(allWitnessCoinsIndexBased);
        pow2SimplifiedWitnessUTXOCopy.currentTipForSet = pBlockIndex->GetBlockHashPoW2();
        pow2SimplifiedWitnessUTXOForBlock = pow2SimplifiedWitnessUTXO = pow2SimplifiedWitnessUTXOCopy;
        return true;
    }
    else
    {
        // We are already the tip so no further action required
        pow2SimplifiedWitnessUTXOForBlock = pow2SimplifiedWitnessUTXOCopy;
        return true;
    }
}


bool GetSimplifiedWitnessUTXODeltaForBlockHelper(uint64_t nBlockHeight, const CBlock& block, CVectorWriter& deltaStream, std::vector<deltaItem>& deltaItems, SimplifiedWitnessUTXOSet& simplifiedWitnessUTXO, SimplifiedWitnessUTXOSet& simplifiedWitnessUTXOWithoutWitnessAction)
{
    bool anyChanges=false;
    // Calculate changes this block would make
    for (const auto& tx : block.vtx)
    {
        if (!tx->witnessBundles)
            return false;

        for (const auto& bundle : *tx->witnessBundles)
        {
            anyChanges = true;
            if (bundle.bundleType == CWitnessTxBundle::WitnessType)
            {
                // Basic sanity checks
                assert(bundle.inputs.size() == 1);
                assert(bundle.outputs.size() == 1);
                assert(!std::get<2>(bundle.inputs[0]).IsNull());
                
                // Find existing item
                SimplifiedWitnessRouletteItem originalItem(bundle.inputs[0]);
                auto iter = simplifiedWitnessUTXO.witnessCandidates.find(originalItem);
                if (iter == simplifiedWitnessUTXO.witnessCandidates.end())
                    return false;

                // Generate changeset
                {
                    deltaStream << VARINT(simplifiedWitnessUTXO.witnessCandidates.index_of(iter));
                    deltaStream << COMPRESSEDAMOUNT(std::get<0>(bundle.outputs[0]).nValue);
                    //No need to encode block number, we can obtain it from the block index
                    deltaStream << VARINT(std::get<2>(bundle.outputs[0]).getTransactionIndex());
                    //No need to encode vout position, its always 0
                    assert(std::get<2>(bundle.outputs[0]).n == 0);
                }
                
                // Perform the change so that subsequent changes can use the right indexing 
                // from here we can reset the blocknumber and value and identify that signature matches
                // Only ever one witness action.
                {
                    SimplifiedWitnessRouletteItem modifiedItem=originalItem;
                    modifiedItem.nValue = std::get<0>(bundle.outputs[0]).nValue;
                    modifiedItem.blockNumber = nBlockHeight;
                    if (modifiedItem.lockFromBlock == 0)
                        modifiedItem.lockFromBlock = modifiedItem.blockNumber;
                    modifiedItem.transactionIndex = std::get<2>(bundle.outputs[0]).getTransactionIndex();
                    modifiedItem.transactionOutputIndex = 0;
                    
                    simplifiedWitnessUTXO.witnessCandidates.erase(iter);
                    auto [insertIter, didInsert] = simplifiedWitnessUTXO.witnessCandidates.insert(modifiedItem);
                    (unused) insertIter;
                    if (!didInsert)
                        return false;
                    
                    deltaItem undo;
                    undo.changeType=changeTypeWitnessAction;
                    undo.removedItems.push_back(originalItem);
                    undo.addedItems.push_back(modifiedItem);
                    deltaItems.push_back(undo);
                }

                break;
            }
        }
    }
    for (const auto& tx : block.vtx)
    {   
        for (const auto& bundle : *tx->witnessBundles)
        {
            switch (bundle.bundleType)
            {
                case CWitnessTxBundle::WitnessType:
                {
                    // Already done in previous loop
                    continue;
                }
                case CWitnessTxBundle::CreationType:
                {
                    // Basic sanity checks
                    assert(bundle.inputs.size() == 0);
                    assert(bundle.outputs.size() > 0);

                    // Treat each item in the bundle as its own seperate creation, instead of trying to cram them all into one
                    for (const auto& output: bundle.outputs)
                    {
                        SimplifiedWitnessRouletteItem modifiedItem;
                        modifiedItem.blockNumber = nBlockHeight;
                        modifiedItem.transactionIndex = std::get<2>(output).getTransactionIndex();
                        modifiedItem.transactionOutputIndex = std::get<2>(output).n;
                        modifiedItem.nValue = std::get<0>(output).nValue;
                        modifiedItem.lockFromBlock = modifiedItem.blockNumber;
                        modifiedItem.lockUntilBlock = std::get<1>(output).lockUntilBlock;
                        modifiedItem.witnessPubKeyID = std::get<1>(output).witnessKeyID;
                        
                        deltaStream << changeTypeCreation;
                        //no need to encode blockNumber because it can be determined from the header
                        deltaStream << VARINT(modifiedItem.transactionIndex);
                        deltaStream << COMPACTSIZE(modifiedItem.transactionOutputIndex);
                        deltaStream << VARINT(modifiedItem.nValue);
                        deltaStream << VARINT(modifiedItem.lockUntilBlock);
                        //lockFrom can in turn be figured out from the blockNumber
                        deltaStream << modifiedItem.witnessPubKeyID;
                        
                        // Insert new item into set
                        simplifiedWitnessUTXO.witnessCandidates.insert(modifiedItem);
                        simplifiedWitnessUTXOWithoutWitnessAction.witnessCandidates.insert(modifiedItem);
                        
                        deltaItem undo;
                        undo.changeType=changeTypeCreation;
                        undo.addedItems.push_back(modifiedItem);
                        deltaItems.push_back(undo);
                    }
                    break;
                }
                //only one input allowed, must be completely consumed (removed from set)
                case CWitnessTxBundle::SpendType:
                {
                    // Basic sanity checks
                    assert(bundle.inputs.size() == 1);
                    assert(bundle.outputs.size() == 0);
                    assert(!std::get<2>(bundle.inputs[0]).IsNull());

                    // Find existing item
                    SimplifiedWitnessRouletteItem originalItem(bundle.inputs[0]);
                    auto iter = simplifiedWitnessUTXO.witnessCandidates.find(originalItem);
                    if (iter == simplifiedWitnessUTXO.witnessCandidates.end())
                        return false;
                
                    deltaStream << changeTypeSpend;
                    deltaStream << VARINT(simplifiedWitnessUTXO.witnessCandidates.index_of(iter));
                    
                    // Remove spent item from set
                    simplifiedWitnessUTXO.witnessCandidates.erase(iter);
                    simplifiedWitnessUTXOWithoutWitnessAction.witnessCandidates.erase(simplifiedWitnessUTXOWithoutWitnessAction.witnessCandidates.find(originalItem));
                    
                    deltaItem undo;
                    undo.changeType=changeTypeSpend;
                    undo.removedItems.push_back(originalItem);
                    deltaItems.push_back(undo);

                    break;
                }
                case CWitnessTxBundle::RenewType:
                {
                    // Basic sanity checks
                    assert(bundle.inputs.size() > 0);
                    assert(bundle.outputs.size() > 0);
                    assert(bundle.inputs.size() == bundle.outputs.size());
                    assert(!std::get<2>(bundle.inputs[0]).IsNull());
                    
                    // Treat each renew as a seperate change instead of trying to encode them all together
                    for (uint64_t i=0; i<bundle.inputs.size(); ++i)
                    {
                        const auto& input = bundle.inputs[i];
                        const auto& output = bundle.outputs[i];
                    
                        // Find existing item
                        SimplifiedWitnessRouletteItem originalItem(input);
                        auto iter = simplifiedWitnessUTXO.witnessCandidates.find(originalItem);
                        if (iter == simplifiedWitnessUTXO.witnessCandidates.end())
                            return false;
                        
                        SimplifiedWitnessRouletteItem modifiedItem = originalItem;
                        modifiedItem.blockNumber = nBlockHeight;
                        modifiedItem.transactionIndex = std::get<2>(output).getTransactionIndex();
                        modifiedItem.transactionOutputIndex = std::get<2>(output).n;
                        if (modifiedItem.lockFromBlock == 0)
                            modifiedItem.lockFromBlock = modifiedItem.blockNumber;
                        
                        deltaStream << changeTypeRenew;
                        deltaStream << VARINT(simplifiedWitnessUTXO.witnessCandidates.index_of(iter));
                        //no need to encode blockNumber because it can be determined from the header
                        deltaStream << VARINT(modifiedItem.transactionIndex);
                        deltaStream << COMPACTSIZE(modifiedItem.transactionOutputIndex);
                        
                        // Update renewed item in set
                        simplifiedWitnessUTXO.witnessCandidates.erase(iter);
                        simplifiedWitnessUTXOWithoutWitnessAction.witnessCandidates.erase(simplifiedWitnessUTXOWithoutWitnessAction.witnessCandidates.find(originalItem));
                        auto [insertIter, didInsert] = simplifiedWitnessUTXO.witnessCandidates.insert(modifiedItem);
                        (unused) insertIter;
                        if (!didInsert)
                            assert(0);
                        
                        deltaItem undo;
                        undo.changeType=changeTypeRenew;
                        undo.removedItems.push_back(originalItem);
                        undo.addedItems.push_back(modifiedItem);
                        deltaItems.push_back(undo);
                    }
                    break;
                }
                case CWitnessTxBundle::RearrangeType:
                {
                    // Basic sanity checks
                    assert(bundle.inputs.size() > 0);
                    assert(bundle.outputs.size() > 0);
                    
                    // Encode common information
                    deltaStream << changeTypeRearrange << COMPACTSIZE(bundle.inputs.size()) << COMPACTSIZE(bundle.outputs.size());
                    
                    deltaItem undo;
                    undo.changeType=changeTypeRearrange;
                    
                    // Encode removal of items; don't perform removal yet as they will then invalidate one anothers indexes
                    SimplifiedWitnessRouletteItem item;
                    for (const auto& input : bundle.inputs)
                    {
                        item = SimplifiedWitnessRouletteItem(input);
                        auto iter = simplifiedWitnessUTXO.witnessCandidates.find(item);
                        if (iter == simplifiedWitnessUTXO.witnessCandidates.end())
                            return false;

                        deltaStream << VARINT(simplifiedWitnessUTXO.witnessCandidates.index_of(iter));
                        
                        undo.removedItems.push_back(item);
                        simplifiedWitnessUTXO.witnessCandidates.erase(iter);
                        simplifiedWitnessUTXOWithoutWitnessAction.witnessCandidates.erase(simplifiedWitnessUTXOWithoutWitnessAction.witnessCandidates.find(item));
                    }

                    // Encode and perform reinsertion of modified items
                    for (const auto& output : bundle.outputs)
                    {
                        item.blockNumber = nBlockHeight;
                        item.transactionIndex = std::get<2>(output).getTransactionIndex();
                        item.transactionOutputIndex = std::get<2>(output).n;
                        item.nValue = std::get<0>(output).nValue;
                        if (item.lockFromBlock == 0)
                            item.lockFromBlock = nBlockHeight;
                        
                        //no need to encode blockNumber because it can be determined from the header
                        deltaStream << VARINT(item.transactionIndex);
                        deltaStream << COMPACTSIZE(item.transactionOutputIndex);
                        deltaStream << COMPRESSEDAMOUNT(item.nValue);
                        
                        simplifiedWitnessUTXO.witnessCandidates.insert(item);
                        simplifiedWitnessUTXOWithoutWitnessAction.witnessCandidates.insert(item);
                        undo.addedItems.push_back(item);
                    }
                    deltaItems.push_back(undo);
                    break;
                }
                case CWitnessTxBundle::IncreaseType:
                {
                    // Basic sanity checks
                    assert(bundle.inputs.size() > 0);
                    assert(bundle.outputs.size() > 0);
                    
                    // Encode common information, all new items must share the same lockUntilBlock so we encode that here instead of repeating it for each item
                    uint64_t newLockUntilBlock = std::get<1>(bundle.outputs[0]).lockUntilBlock;
                    deltaStream << changeTypeIncrease << COMPACTSIZE(bundle.inputs.size()) << COMPACTSIZE(bundle.outputs.size()) << VARINT(newLockUntilBlock);
                    
                    deltaItem undo;
                    undo.changeType=changeTypeIncrease;

                    // Encode removal of items; don't perform removal yet as they will then invalidate one anothers indexes
                    SimplifiedWitnessRouletteItem item;
                    for (const auto& input : bundle.inputs)
                    {
                        item = SimplifiedWitnessRouletteItem(input);
                        auto iter = simplifiedWitnessUTXO.witnessCandidates.find(item);
                        if (iter == simplifiedWitnessUTXO.witnessCandidates.end())
                            return false;
                    
                        deltaStream << VARINT(simplifiedWitnessUTXO.witnessCandidates.index_of(iter));

                        undo.removedItems.push_back(item);
                        simplifiedWitnessUTXO.witnessCandidates.erase(iter);
                        simplifiedWitnessUTXOWithoutWitnessAction.witnessCandidates.erase(simplifiedWitnessUTXOWithoutWitnessAction.witnessCandidates.find(item));
                    }

                    // Encode and perform reinsertion of modified items
                    for (const auto& output : bundle.outputs)
                    {
                        item.blockNumber = nBlockHeight;
                        item.transactionIndex = std::get<2>(output).getTransactionIndex();
                        item.transactionOutputIndex = std::get<2>(output).n;
                        item.nValue = std::get<0>(output).nValue;
                        item.lockFromBlock = nBlockHeight;
                        item.lockUntilBlock = std::get<1>(output).lockUntilBlock;
                        
                        //no need to encode blockNumber because it can be determined from the header
                        deltaStream << VARINT(item.transactionIndex);
                        deltaStream << COMPACTSIZE(item.transactionOutputIndex);
                        deltaStream << COMPRESSEDAMOUNT(item.nValue);
                        //no need to encode lockfrom  because it can be determined from the header (note lockfrom changes with an increase type bundle)
                        //lockUntilBlock encoded once for all bundles, before this loop
                        
                        simplifiedWitnessUTXO.witnessCandidates.insert(item);
                        simplifiedWitnessUTXOWithoutWitnessAction.witnessCandidates.insert(item);
                        undo.addedItems.push_back(item);
                    }
                    deltaItems.push_back(undo);
                    break;
                }
                case CWitnessTxBundle::ChangeWitnessKeyType:
                {
                    // Basic sanity checks
                    assert(bundle.inputs.size() > 0);
                    assert(bundle.inputs.size() == bundle.outputs.size());
                    
                    // Encode common information
                    // Can have multiple inputs/outputs, always matching in number so only encode output size
                    // All new items must share a new witness key, so encode the key once here instead of individually for each item
                    CKeyID newWitnessKeyID = std::get<1>(bundle.outputs[0]).witnessKeyID;
                    deltaStream << changeTypeChangeKey << COMPACTSIZE(bundle.outputs.size()) << newWitnessKeyID;
                    
                    deltaItem undo;
                    undo.changeType=changeTypeChangeKey;
                    
                    for (uint64_t index=0; index < bundle.inputs.size();++index)
                    {
                        auto& input = bundle.inputs[index];
                        auto& output = bundle.outputs[index];

                        SimplifiedWitnessRouletteItem originalItem(input);
                        auto iter = simplifiedWitnessUTXO.witnessCandidates.find(originalItem);
                        if (iter == simplifiedWitnessUTXO.witnessCandidates.end())
                            return false;
                    
                        deltaStream << VARINT(simplifiedWitnessUTXO.witnessCandidates.index_of(iter));
                        
                        SimplifiedWitnessRouletteItem modifiedItem = originalItem;
                        modifiedItem.blockNumber = nBlockHeight;
                        modifiedItem.transactionIndex = std::get<2>(output).getTransactionIndex();
                        modifiedItem.transactionOutputIndex = std::get<2>(output).n;
                        modifiedItem.witnessPubKeyID = newWitnessKeyID;
                        
                        //no need to encode blockNumber because it can be determined from the header
                        deltaStream << VARINT(modifiedItem.transactionIndex);
                        deltaStream << COMPACTSIZE(modifiedItem.transactionOutputIndex);
                        //no need to encode lockfrom  because it can be determined from the header (note lockfrom changes with an increase type bundle)
                        
                        simplifiedWitnessUTXO.witnessCandidates.erase(iter);
                        simplifiedWitnessUTXO.witnessCandidates.insert(modifiedItem);
                        simplifiedWitnessUTXOWithoutWitnessAction.witnessCandidates.erase(simplifiedWitnessUTXOWithoutWitnessAction.witnessCandidates.find(originalItem));
                        simplifiedWitnessUTXOWithoutWitnessAction.witnessCandidates.insert(modifiedItem);
                        
                        undo.removedItems.push_back(originalItem);
                        undo.addedItems.push_back(modifiedItem);
                    }
                    deltaItems.push_back(undo);
                    break;
                }
            }
        }
    }
    if (!anyChanges)
        return false;

    return true;
}

bool GetSimplifiedWitnessUTXODeltaForBlock(const CBlockIndex* pBlockIndex, const CBlock& block, std::shared_ptr<SimplifiedWitnessUTXOSet> pow2SimplifiedWitnessUTXOForPrevBlock, std::vector<unsigned char>& compWitnessUTXODelta, CPubKey* pubkey)
{
    SimplifiedWitnessUTXOSet pow2SimplifiedWitnessUTXOModified = *pow2SimplifiedWitnessUTXOForPrevBlock;
    SimplifiedWitnessUTXOSet pow2SimplifiedWitnessUTXOModifiedWithoutWitnessAction = pow2SimplifiedWitnessUTXOModified;
    
    #ifdef EXTRA_DELTA_TESTS
    SimplifiedWitnessUTXOSet pow2SimplifiedWitnessUTXOOrig = pow2SimplifiedWitnessUTXOModified;
    #endif
    
    // Calculate what changes the block makes to the simplified witness utxo set
    std::vector<deltaItem> deltaItems;
    CVectorWriter deltaStream(SER_NETWORK, 0, compWitnessUTXODelta, 0);
    if (!GetSimplifiedWitnessUTXODeltaForBlockHelper(pBlockIndex->nHeight, block, deltaStream, deltaItems, pow2SimplifiedWitnessUTXOModified, pow2SimplifiedWitnessUTXOModifiedWithoutWitnessAction))
        return false;

    // Our copy must have changes so should not match the original
    #ifdef EXTRA_DELTA_TESTS
    assert(pow2SimplifiedWitnessUTXOModified != pow2SimplifiedWitnessUTXOOrig);
    #endif
    
    if (pubkey)
    {
        DO_BENCHMARKT("CheckBlockHeaderIsPoWValid - VERIFYWITNESS_SIMPLIFIED_INTERNAL", BCLog::BENCH, 0);

        CGetWitnessInfo witInfo;
        GetWitnessFromSimplifiedUTXO(pow2SimplifiedWitnessUTXOModifiedWithoutWitnessAction, pBlockIndex, witInfo);
        if(witInfo.selectedWitnessTransaction.GetType() == CTxOutType::PoW2WitnessOutput)
        {
            if (witInfo.selectedWitnessTransaction.output.witnessDetails.witnessKeyID != pubkey->GetID())
            {
                return false;
            }
        }
        else
        {
            return false;
        }
    }
    
    // Generate the undo info, when done pow2SimplifiedWitnessUTXOUndo should match our original item again
    SimplifiedWitnessUTXOSet pow2SimplifiedWitnessUTXOUndo = pow2SimplifiedWitnessUTXOModified;
    std::vector<unsigned char> undoWitnessUTXODelta;
    if (!GenerateSimplifiedWitnessUTXODeltaUndoForHeader(undoWitnessUTXODelta, pow2SimplifiedWitnessUTXOUndo, deltaItems))
        return false;
    
    // As we have now undone the changes while generating the undo info, we should match the original again
    #ifdef EXTRA_DELTA_TESTS
    assert(pow2SimplifiedWitnessUTXOUndo == pow2SimplifiedWitnessUTXOOrig);
    #endif
    
    // Revert to the modified set (with changes), apply the generated undo info, and ensure the final result matches the original set (this tests full roundtrip)
    #ifdef EXTRA_DELTA_TESTS
    pow2SimplifiedWitnessUTXOUndo = pow2SimplifiedWitnessUTXOModified;
    UndoSimplifiedWitnessUTXODeltaForHeader(pow2SimplifiedWitnessUTXOUndo, undoWitnessUTXODelta);
    assert(pow2SimplifiedWitnessUTXOUndo == pow2SimplifiedWitnessUTXOOrig);
    #endif

    // Set our modified set as the latest we have
    if (pBlockIndex->nVersionPoW2Witness != 0)
    {
        pow2SimplifiedWitnessUTXOModified.currentTipForSet = pBlockIndex->GetBlockHashPoW2();    
        pow2SimplifiedWitnessUTXO = pow2SimplifiedWitnessUTXOModified;
    }
    
    return true;
}
