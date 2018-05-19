// File contains modifications by: The Gulden developers
// All modifications:
// Copyright (c) 2017-2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#include "validation.h"
#include "witnessvalidation.h"
#include <consensus/validation.h>
#include <Gulden/util.h>
#include "timedata.h" // GetAdjustedTime()

#ifdef ENABLE_WALLET
#include "wallet/wallet.h"
#endif

#include <boost/foreach.hpp> // Zreverse_foreach
#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/thread.hpp>


CWitViewDB *ppow2witdbview = NULL;
std::shared_ptr<CCoinsViewCache> ppow2witTip = NULL;


CAmount GetBlockSubsidyWitness(int nHeight, const Consensus::Params& consensusParams)
{
    CAmount nSubsidy = 0;
    if (nHeight <= 12850000) // Switch to fixed reward of 100 Gulden per block (no halving) - to continue until original coin target is met.
    {
        nSubsidy = 20 * COIN;
    }
    return nSubsidy;
}

//fixme: (2.1) Can remove this.
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
    LOCK(cs_main);
    std::vector<CBlockIndex*> vRet;
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
            if (!ReadBlockFromDisk(block, currentChain.Tip(), chainparams.GetConsensus()))
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
        if (!ReadBlockFromDisk(block, currentChain.Tip(), chainparams.GetConsensus()))
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
                if (!ReadBlockFromDisk(block, pindexConnect, chainparams.GetConsensus()))
                    return false;
            }
            bool rv = ConnectBlock(currentChain, pblockConnect?*pblockConnect:*pblock, state, pindexConnect, coinView, chainparams);
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

        //fixme: (2.0) - I think we can remove this now, but make sure?
        // Disallow the following as they should never happen:
        // 1) Activating a witness block as tip during phase 3 is an error.
        // 2) Activating an unwitnessed PoW block as tip for phase 3.
        // 3) Activating a PoW block as tip from phase 4 onwards...
        /*if (pindexNewTip->pprev && IsPow2Phase3Active(pindexNewTip->pprev->pprev, chainparams.GetConsensus()) && !IsPow2Phase4Active(pindexNewTip->pprev, chainparams.GetConsensus()))
        {
            assert(pindexNewTip->nVersionPoW2Witness == 0);
            assert (GetWitnessOrphanForBlock(pindexNewTip->nHeight, pindexNewTip->pprev->GetBlockHashLegacy(), pindexNewTip->GetBlockHashLegacy()) != NULL);
        }
        if (GetPoW2Phase(pindexNewTip->pprev, chainparams.GetConsensus()) >= 4)
        {
            assert(pindexNewTip->nVersionPoW2Witness != 0);
        }*/
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
    DO_BENCHMARK("WIT: expectedWitnessBlockPeriod", BCLog::BENCH|BCLog::WITNESS);

    if (nWeight == 0 || networkTotalWeight == 0)
        return 0;

    if (nWeight > networkTotalWeight/100)
        nWeight = networkTotalWeight/100;

    static const arith_uint256 base = arith_uint256(100000000) * arith_uint256(100000000) * arith_uint256(100000000);
    #define BASE(x) (arith_uint256(x)*base)
    #define AI(x) arith_uint256(x)
    return 100 + std::max(( ((BASE(1)/((BASE(nWeight)/AI(networkTotalWeight))))).GetLow64() * 5 ), (uint64_t)500);
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


bool getAllUnspentWitnessCoins(CChain& chain, const CChainParams& chainParams, const CBlockIndex* pPreviousIndexChain_, std::map<COutPoint, Coin>& allWitnessCoins, CBlock* newBlock, CCoinsViewCache* viewOverride)
{
    DO_BENCHMARK("WIT: getAllUnspentWitnessCoins", BCLog::BENCH|BCLog::WITNESS);

    LOCK2(cs_main, pactiveWallet?&pactiveWallet->cs_wallet:NULL);
    assert(pPreviousIndexChain_);

    allWitnessCoins.clear();
    //fixme: (2.0) (PoW2) Error handling
    // Sort out pre-conditions.
    // We have to make sure that we are using a view and chain that includes the PoW block we are witnessing and all of its transactions as the tip.
    // It won't necessarily be part of the chain yet; if we are in the process of witnessing; or if the block is an older one on a fork; because only blocks that have already been witnessed can be part of the chain.
    // So we have to temporarily force disconnect/reconnect of blocks as necessary to make a temporary working chain that suits the properties we want.
    // NB!!! - It is important that we don't flush either of these before destructing, we want to throw the result away.
    CCoinsViewCache viewNew(viewOverride?viewOverride:pcoinsTip);

    // fixme: (2.1) SBSU - We really don't need to clone the entire chain here, could we clone just the last 1000 or something?
    // We work on a clone of the chain to prevent modifying the actual chain.
    CBlockIndex* pPreviousIndexChain = nullptr;
    CCloneChain tempChain = chain.Clone(pPreviousIndexChain_, pPreviousIndexChain);
    CValidationState state;
    assert(pPreviousIndexChain);

    // Force the tip of the chain to the block that comes before the block we are examining.
    // For phase 3 this must be a PoW block - from phase 4 it should be a witness block 
    if (pPreviousIndexChain->nVersionPoW2Witness==0 || IsPow2Phase4Active(pPreviousIndexChain->pprev, chainParams, tempChain, &viewNew))
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
        }

        // Place the block in question at the tip of the chain.
        CBlockIndex indexDummy(*newBlock);
        indexDummy.pprev = pPreviousIndexChain;
        indexDummy.nHeight = pPreviousIndexChain->nHeight + 1;
        if (!ConnectBlock(tempChain, *newBlock, state, &indexDummy, viewNew, chainParams, true, false))
        {
            //fixme: (2.0) If we are inside a GetWitness call ban the peer that sent us this?
            return false;
        }
    }

    /** Gather a list of all unspent witness outputs.
        NB!!! There are multiple layers of cache at play here, with insertions/deletions possibly having taken place at each layer.
        Therefore the order of operations is crucial, we must first iterate the lowest layer, then the second lowest and finally the highest layer.
        For each iteration we should remove items from allWitnessCoins if they have been deleted in the higher layer as the higher layer overrides the lower layer.
        GetAllCoins takes care of all of this automatically.
    **/
    viewNew.pChainedWitView->GetAllCoins(allWitnessCoins);

    return true;
}


//fixme: (2.0) NEXT.
//Proper error handling.
//Handle pruning.
// Check whether we have ever pruned block & undo files
//pblocktree->ReadFlag("prunedblockfiles", fHavePruned);
bool GetWitnessHelper(CChain& chain, const CChainParams& chainParams, CCoinsViewCache* viewOverride, CBlockIndex* pPreviousIndexChain, uint256 blockHash, CGetWitnessInfo& witnessInfo, uint64_t nBlockHeight)
{
    DO_BENCHMARK("WIT: GetWitnessHelper", BCLog::BENCH|BCLog::WITNESS);

    LOCK2(cs_main, pactiveWallet?&pactiveWallet->cs_wallet:nullptr);

    /** Generate the pool of potential witnesses for the given block index **/
    /** Addresses older than 10000 blocks or younger than 100 blocks are discarded **/
    uint64_t nMinAge = nMinimumParticipationAge;
    while (true)
    {
        witnessInfo.witnessSelectionPoolFiltered.clear();
        witnessInfo.witnessSelectionPoolFiltered = witnessInfo.witnessSelectionPoolUnfiltered;

        //LogPrint(BCLog::WITNESS, "Witness pool size1b: %d minage: %d\n", witnessInfo.witnessSelectionPoolFiltered.size(), nMinAge);
        /** Eliminate addresses that have witnessed within the last `nMinimumParticipationAge` blocks **/
        witnessInfo.witnessSelectionPoolFiltered.erase(std::remove_if(witnessInfo.witnessSelectionPoolFiltered.begin(), witnessInfo.witnessSelectionPoolFiltered.end(), [&](RouletteItem& x){ return (x.nAge <= nMinAge); }), witnessInfo.witnessSelectionPoolFiltered.end());
        //LogPrint(BCLog::WITNESS, "Witness pool size1a %d minage: %d\n", witnessInfo.witnessSelectionPoolFiltered.size(), nMinAge);

        /** Eliminate addresses that have not witnessed within the expected period of time that they should have **/
        //LogPrint(BCLog::WITNESS, "Witness pool size2b: %d minage: %d\n", witnessInfo.witnessSelectionPoolFiltered.size(), nMinAge);
        witnessInfo.witnessSelectionPoolFiltered.erase(std::remove_if(witnessInfo.witnessSelectionPoolFiltered.begin(), witnessInfo.witnessSelectionPoolFiltered.end(), [&](RouletteItem& x){ return witnessHasExpired(x.nAge, x.nWeight, witnessInfo.nTotalWeight); }), witnessInfo.witnessSelectionPoolFiltered.end());
        //LogPrint(BCLog::WITNESS, "Witness pool size2a: %d minage: %d\n", witnessInfo.witnessSelectionPoolFiltered.size(), nMinAge);

        //fixme: (2.0) check for off by 1 error.
        /** Eliminate addresses that are within 100 blocks from lock period expiring, or whose lock period has expired. **/
        //LogPrint(BCLog::WITNESS, "Witness pool size3b: %d minage: %d\n", witnessInfo.witnessSelectionPoolFiltered.size(), nMinAge);
        witnessInfo.witnessSelectionPoolFiltered.erase(std::remove_if(witnessInfo.witnessSelectionPoolFiltered.begin(), witnessInfo.witnessSelectionPoolFiltered.end(), [&](RouletteItem& x){ CTxOutPoW2Witness details; GetPow2WitnessOutput(x.coin.out, details); return !(details.lockUntilBlock - nMinAge >= nBlockHeight); }), witnessInfo.witnessSelectionPoolFiltered.end());
        //LogPrint(BCLog::WITNESS, "Witness pool size3a: %d minage: %d\n", witnessInfo.witnessSelectionPoolFiltered.size(), nMinAge);

        // We must have at least 100 accounts to keep odds of being selected down below 1% at all times.
        if (witnessInfo.witnessSelectionPoolFiltered.size() < 100)
        {
            // fixme: (2.0) Add warning/logging for this.
            // NB!! This part of the code should (ideally) never actually be used, it exists only for instances where their are a shortage of witnesses paticipating on the network.
            // Hard limit - we never allow a min age lower than 2 as this starts to cause code issues.
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

    /** Reduce larger weightings to a maximum weighting of 1% of network weight. **/
    /** NB!! this actually will end up a little bit more than 1% as the overall network weight will also be reduced as a result. **/
    /** This is however unimportant as 1% is in and of itself also somewhat arbitrary, simpler code is favoured here over exactness. **/
    /** So we delibritely make no attempt to compensate for this. **/
    uint64_t maxWeight = witnessInfo.nTotalWeight / 100;
    witnessInfo.nReducedTotalWeight = 0;
    for (auto& item : witnessInfo.witnessSelectionPoolFiltered)
    {
        if (item.nWeight > maxWeight)
            item.nWeight = maxWeight;
        witnessInfo.nReducedTotalWeight += item.nWeight;
        item.nCumulativeWeight = witnessInfo.nReducedTotalWeight;
    }

    /** sha256 as random roulette spin/seed - NB! We deliritely use sha256 and -not- the normal PoW hash here as the normal PoW hash is biased towards certain number ranges by -design- (block target) so is not a good RNG... **/
    arith_uint256 rouletteSelectionSeed = UintToArith256(blockHash);

    //checkme: (GULDEN) (2.0) (POW2) - Is this necessary? I don't think so, so disabling.
    /** ensure random seed exceeds one full spin of the wheel to prevent any possible bias towards low numbers **/
    //while (rouletteSelectionSeed < witnessInfo.nReducedTotalWeight)
    //{
        //rouletteSelectionSeed = rouletteSelectionSeed * 2;
    //}

    //LogPrint(BCLog::WITNESS, "RNG1 %d %d\n", rouletteSelectionSeed.GetLow64(), witnessInfo.nReducedTotalWeight);
    if (rouletteSelectionSeed > arith_uint256(witnessInfo.nReducedTotalWeight))
    {
        // 'BigNum' Modulo operator via mathematical identity:  a % b = a - (b * int(a/b))
        rouletteSelectionSeed = rouletteSelectionSeed - (arith_uint256(witnessInfo.nReducedTotalWeight) * arith_uint256(rouletteSelectionSeed/arith_uint256(witnessInfo.nReducedTotalWeight)));
    }
    //LogPrint(BCLog::WITNESS, "RNG2 %d %d\n", rouletteSelectionSeed.GetLow64(), witnessInfo.nReducedTotalWeight);

    auto selectedWitness = std::lower_bound(witnessInfo.witnessSelectionPoolFiltered.begin(), witnessInfo.witnessSelectionPoolFiltered.end(), rouletteSelectionSeed.GetLow64());
    //LogPrint(BCLog::WITNESS, "Selected witness age %d\n", selectedWitness->nAge);
    witnessInfo.selectedWitnessTransaction = selectedWitness->coin.out;
    witnessInfo.selectedWitnessBlockHeight = selectedWitness->coin.nHeight;
    witnessInfo.selectedWitnessOutpoint = selectedWitness->outpoint;

    //LogPrintf(">>>Selected witness=%s %s fromblockhash=%s fromblockheight=%d currentheight=%d prevout=%s \n",selectedWitness->coin.out.output.GetHex(selectedWitness->coin.out.GetType()), selectedWitness->coin.out.ToString(), block.GetHashPoW2().ToString(), resultBlockHeight, nBlockHeight, resultOutPoint.hash.ToString());
    return true;
}

bool GetWitnessInfo(CChain& chain, const CChainParams& chainParams, CCoinsViewCache* viewOverride, CBlockIndex* pPreviousIndexChain, CBlock block, CGetWitnessInfo& witnessInfo, uint64_t nBlockHeight)
{
    DO_BENCHMARK("WIT: GetWitnessInfo", BCLog::BENCH|BCLog::WITNESS);

    LOCK2(cs_main, pactiveWallet?&pactiveWallet->cs_wallet:nullptr);

    // Fetch all unspent witness outputs for the chain in which -block- acts as the tip.
    if (!getAllUnspentWitnessCoins(chain, chainParams, pPreviousIndexChain, witnessInfo.allWitnessCoins, &block, viewOverride))
        return false;

    // Calculate network weight based on current block, exclude witnesses that are too old.
    for (auto coinIter : witnessInfo.allWitnessCoins)
    {
        //testme: (GULDEN) (POW2) (2.0) (HIGH) - Make sure no off by 1 error here.
        uint64_t nAge = nBlockHeight - coinIter.second.nHeight;
        COutPoint outPoint = coinIter.first;
        Coin coin = coinIter.second;
        if (coin.out.nValue >= nMinimumWitnessAmount)
        {
            uint64_t nUnused1, nUnused2;
            int64_t nWeight = GetPoW2RawWeightForAmount(coin.out.nValue, GetPoW2LockLengthInBlocksFromOutput(coin.out, coin.nHeight, nUnused1, nUnused2));
            if (nWeight < nMinimumWitnessWeight)
                continue;
            witnessInfo.witnessSelectionPoolUnfiltered.push_back(RouletteItem(outPoint, coin, nWeight, nAge));
            witnessInfo.nTotalWeight += nWeight;
        }
    }
    return true;
}

bool GetWitness(CChain& chain, const CChainParams& chainParams, CCoinsViewCache* viewOverride, CBlockIndex* pPreviousIndexChain, CBlock block, CGetWitnessInfo& witnessInfo)
{
    DO_BENCHMARK("WIT: GetWitness", BCLog::BENCH|BCLog::WITNESS);

    LOCK2(cs_main, pactiveWallet?&pactiveWallet->cs_wallet:nullptr);

    // Fetch all the chain info (for specific block) we will need to calculate the witness.
    uint64_t nBlockHeight = pPreviousIndexChain->nHeight + 1;
    if (!GetWitnessInfo(chain, chainParams, viewOverride, pPreviousIndexChain, block, witnessInfo, nBlockHeight))
        return false;

    return GetWitnessHelper(chain, chainParams, viewOverride, pPreviousIndexChain, block.GetHashLegacy(), witnessInfo, nBlockHeight);
}

//fixme: (2.0) (HIGH) - switch to a hybrid of witInfo.nTotalWeight / witInfo.nReducedTotalWeight - as both independantly aren't perfect.
// total weight is prone to be too high if there are lots of large >1% witnesses, nReducedTotalWeight is prone to be too low if there is one large witness who has recently witnessed.
bool witnessHasExpired(uint64_t nWitnessAge, uint64_t nWitnessWeight, uint64_t nNetworkTotalWitnessWeight)
{
    DO_BENCHMARK("WIT: witnessHasExpired", BCLog::BENCH|BCLog::WITNESS);

    uint64_t nExpectedWitnessPeriod = expectedWitnessBlockPeriod(nWitnessWeight, nNetworkTotalWitnessWeight);
    return ( nWitnessAge > nMaximumParticipationAge ) || ( nWitnessAge > nExpectedWitnessPeriod );
}

bool ExtractWitnessBlockFromWitnessCoinbase(CChain& chain, int nWitnessCoinbaseIndex, const CBlockIndex* pindexPrev, const CBlock& block, const CChainParams& chainParams, CCoinsViewCache& view, CBlock& embeddedWitnessBlock)
{
    AssertLockHeld(cs_main); // Required for ReadBlockFromDisk.

    if (nWitnessCoinbaseIndex == -1)
        return error("Invalid coinbase index for embedded witness coinbase info.");
    if (block.vtx.size() < 2)
        return error("Missing transactions for witness coinbase index, must be at least two transactions.");
    if (block.vtx[1]->vout.size() != 2)
        return error("Missing outputs for witness coinbase index, must be exactly two outputs.");
    if (block.vtx[1]->vin.size() != 1)
        return error("Missing input for witness coinbase index, must be exactly one input.");
    if (!IsPow2WitnessOutput(block.vtx[1]->vout[0]))
        return error("Invalid transaction type for first witness coinbase output, must be a witness address/output.");
    if (IsPow2WitnessOutput(block.vtx[1]->vout[1]))
        return error("Invalid transaction type for second witness coinbase output, must not be a witness address/output.");

    CScript expect = CScript() << OP_RETURN << pindexPrev->nHeight + 1;
    if (block.vtx[1]->vout[1].output.scriptPubKey.size() < expect.size() || !std::equal(expect.begin(), expect.end(), block.vtx[1]->vout[1].output.scriptPubKey.begin()))
    {
        return error("Invalid scriptSig for embedded witness coinbase info.");
    }

    //'identifier' already checked in GetPoW2WitnessCoinbaseIndex - so just skip past it.
    std::vector<unsigned char> serialisedWitnessHeaderInfo = std::vector<unsigned char>(block.vtx[0]->vout[nWitnessCoinbaseIndex].output.scriptPubKey.begin() + 6, block.vtx[0]->vout[nWitnessCoinbaseIndex].output.scriptPubKey.end());
    CDataStream serialisedWitnessHeaderInfoStream(serialisedWitnessHeaderInfo, SER_NETWORK, INIT_PROTO_VERSION);

    uint256 hashPrevPoWIndex;

    // Reconstruct header information of previous witness block from the coinbase of this PoW block.
    if (!ReadBlockFromDisk(embeddedWitnessBlock, pindexPrev, chainParams.GetConsensus()))
        return false;

    embeddedWitnessBlock.witnessHeaderPoW2Sig.resize(65);
    ::Unserialize(serialisedWitnessHeaderInfoStream, embeddedWitnessBlock.nVersionPoW2Witness);
    ::Unserialize(serialisedWitnessHeaderInfoStream, embeddedWitnessBlock.nTimePoW2Witness);
    ::Unserialize(serialisedWitnessHeaderInfoStream, embeddedWitnessBlock.hashMerkleRootPoW2Witness);
    ::Unserialize(serialisedWitnessHeaderInfoStream, NOSIZEVECTOR(embeddedWitnessBlock.witnessHeaderPoW2Sig));
    ::Unserialize(serialisedWitnessHeaderInfoStream, hashPrevPoWIndex);

    // Check prev hash
    if (hashPrevPoWIndex != pindexPrev->GetBlockHashLegacy())
        return error("Embedded witness coinbase info contains mismatched prevHash.");

    // Check for valid signature size
    if (embeddedWitnessBlock.witnessHeaderPoW2Sig.size() != 65)
        return error("Embedded witness coinbase info contains invalid signature size.");

    // Check that block is a witness block
    if (embeddedWitnessBlock.nVersionPoW2Witness == 0)
        return error("Embedded witness coinbase info contains invalid witness version.");

    // Reconstruct transaction information of previous witness block from the coinbase of this PoW block.
    CMutableTransaction coinbaseTx(CTransaction::CURRENT_VERSION);
    coinbaseTx.vin.resize(2);
    coinbaseTx.vout.resize(2);
    coinbaseTx.vout[0] = block.vtx[1]->vout[0];
    coinbaseTx.vout[1].output.scriptPubKey = block.vtx[0]->vout[nWitnessCoinbaseIndex+1].output.scriptPubKey;
    coinbaseTx.vout[1].nValue = block.vtx[0]->vout[nWitnessCoinbaseIndex+1].nValue;
    coinbaseTx.vin[0].prevout.SetNull();
    coinbaseTx.vin[0].SetSequence(0, coinbaseTx.nVersion, CTxInFlags::None) ;
    coinbaseTx.vin[0].scriptSig = CScript() << pindexPrev->nHeight;
    coinbaseTx.vin[1] = block.vtx[1]->vin[0];
    embeddedWitnessBlock.vtx.emplace_back(MakeTransactionRef(std::move(coinbaseTx)));

    //LogPrintf(">>>[Embedded] Extract witness block from coinbase: prevheight:%d height:%d \nembedded block: %s\n", pindexPrev->nHeight, pindexPrev->nHeight+1 , embeddedWitnessBlock.ToString());
    return true;
}

bool WitnessCoinbaseInfoIsValid(CChain& chain, int nWitnessCoinbaseIndex, const CBlockIndex* pindexPrev, const CBlock& block, const CChainParams& chainParams, CCoinsViewCache& view)
{
    AssertLockHeld(cs_main);

    CBlock embeddedWitnessBlock;
    if (!ExtractWitnessBlockFromWitnessCoinbase(chain, nWitnessCoinbaseIndex, pindexPrev, block, chainParams, view, embeddedWitnessBlock))
        return error("Could not extract embedded witness information from witness coinbase.");

    // Ensure that the witness signature itself is actually valid.
    bool ret = true;
    if (ret)
    {
        CBlockIndex* pPreviousIndexChain = nullptr;
        CCloneChain tempChain = chain.Clone(pindexPrev->pprev, pPreviousIndexChain);
        CValidationState state;
        CCoinsViewCache viewNew(&view);

        CPubKey pubkey;
        uint256 hash = embeddedWitnessBlock.GetHashPoW2();
        if (!pubkey.RecoverCompact(hash, embeddedWitnessBlock.witnessHeaderPoW2Sig))
            ret = error("Could not recover public key from embedded witness coinbase header");
        //LogPrintf(">>>[Embedded] witness pubkey [%s]\n", pubkey.GetID().GetHex());

        // Phase 3 restriction - we force the miners nVersion to reflect the version the witness of the block before had - thus allowing control of voting for phase 4 to be controlled by witnesses.
        if (ret && block.nVersion != embeddedWitnessBlock.nVersionPoW2Witness)
            ret = error("Embedded witness version doesn't match version of parent PoW block.");

        if (ret)
        {
            CGetWitnessInfo witInfo;
            if (!GetWitness(tempChain, chainParams, &viewNew, pPreviousIndexChain, embeddedWitnessBlock, witInfo))
            {
                ret = error("Could not determine a valid witness for embedded witness coinbase header");
            }
            else
            {
                if (witInfo.selectedWitnessTransaction.GetType() <= CTxOutType::ScriptLegacyOutput)
                {
                    if (CKeyID(uint160(witInfo.selectedWitnessTransaction.output.scriptPubKey.GetPow2WitnessHash())) != pubkey.GetID())
                        ret = error("Mismatched witness signature for embedded witness coinbase header");
                }
                else
                {
                    ret = error("Invalid transaction type for embedded witness coinbase header");
                }
            }
        }

        if (!ret)
        {
            LogPrintf("%s\n", embeddedWitnessBlock.ToString());
            return false;
        }
    }

    // Now test that the reconstructed witness block is valid, if it is then the 'witness coinbase info' of this PoW block is valid.
    // fixme: (2.1) SBSU - We really don't need to clone the entire chain here, could we clone just the last 1000 or something?
    // We work on a clone of the chain to prevent modifying the actual chain.
    {
        CBlockIndex* pPreviousIndexChain = nullptr;
        CCloneChain tempChain = chain.Clone(pindexPrev->pprev, pPreviousIndexChain);
        CValidationState state;
        CCoinsViewCache viewNew(&view);
        // Force the tip of the chain to the block that comes before the block we are examining.
        ForceActivateChain(pPreviousIndexChain, nullptr, state, chainParams, tempChain, viewNew);

        CValidationState witnessValidationState;
        assert(pPreviousIndexChain && pPreviousIndexChain == tempChain.Tip());

        CBlockIndex indexDummy(embeddedWitnessBlock);
        indexDummy.pprev = pPreviousIndexChain;
        indexDummy.nHeight = pPreviousIndexChain->nHeight + 1;

        if (!ContextualCheckBlockHeader(embeddedWitnessBlock, state, chainParams.GetConsensus(), pPreviousIndexChain, GetAdjustedTime()))
            ret = error("Failed ContextualCheckBlockHeader for embedded witness block");
        if (!ret || !CheckBlock(embeddedWitnessBlock, state, chainParams.GetConsensus(), false, true))
            ret = error("Failed CheckBlock for embedded witness block");
        if (!ret || !ContextualCheckBlock(embeddedWitnessBlock, state, chainParams, pPreviousIndexChain, tempChain, &viewNew))
            ret = error("Failed ContextualCheckBlock for embedded witness block");
        if (!ret || !ConnectBlock(tempChain, embeddedWitnessBlock, state, &indexDummy, viewNew, chainParams, true, false))
            ret = error("Failed ConnectBlock for embedded witness block");
        if (!state.IsValid())
            ret = error("Invalid state after ConnectBlock for embedded witness block");

        if (!ret)
            LogPrintf("%s\n", embeddedWitnessBlock.ToString());
    }

    return ret;
}
