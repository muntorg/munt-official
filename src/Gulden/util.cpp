// Copyright (c) 2015-2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#include "util.h"
#include "wallet/wallettx.h"
#include "wallet/wallet.h"
#include "consensus/validation.h"
#include "validation.h"
#include "witnessvalidation.h"


#include "txdb.h"

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

//fixme: (2.0) (POW2) (LAUNCH) - set this.
const int earliestPossibleMainnetWitnessACtivationHeight = 850000;

// Phase 2 becomes active after 75% of miners signal upgrade.
// After activation creation of 'backwards compatible' PoW2 addresses becomes possible.
std::map<uint256, bool> phase2ActivationCache;
static uint256 phase2ActivationHash;
static uint256 phase3ActivationHash;
static uint256 phase4ActivationHash;
void PerformFullChainPhaseScan(const CBlockIndex* pIndex, const CChainParams& chainparams, CChain& chain, CCoinsViewCache* viewOverride)
{
    DO_BENCHMARK("WIT: PerformFullChainPhaseScan", BCLog::BENCH|BCLog::WITNESS);

    LOCK2(cs_main, pactiveWallet?&pactiveWallet->cs_wallet:NULL);

    bool phase2Active = false;
    bool phase3Active = false;
    int nStartHeight = 0;
    for(int i=nStartHeight; i<chain.Height(); ++i)
    {
        if (phase2Active)
            phase2ActivationCache[chain[i]->GetBlockHashPoW2()] = true;
        else if (IsPow2Phase2Active(chain[i], chainparams, chain, viewOverride))
        {
            phase2Active = true;
            nStartHeight = i + 1;
        }
    }
    if (phase2Active)
    {
        for(int i=nStartHeight; i<chain.Height(); ++i)
        {
            if (IsPow2Phase3Active(chain[i], chainparams, chain, viewOverride))
            {
                phase3Active = true;
                nStartHeight = i;
                break;
            }
        }
    }
    if (phase3Active)
    {
        for(int i=nStartHeight; i<chain.Height(); ++i)
        {
            if (IsPow2Phase4Active(chain[i], chainparams, chain, viewOverride))
            {
                break;
            }
        }
    }
}

bool IsPow2Phase2Active(const CBlockIndex* pIndex, const CChainParams& chainparams, CChain& chain, CCoinsViewCache* viewOverride)
{
    DO_BENCHMARK("WIT: IsPow2Phase2Active", BCLog::BENCH|BCLog::WITNESS);

    // If we don't yet have any information on phase activation then do a once off scan on the entire chain so that our caches are correctly primed.
    if (pIndex && pIndex->nHeight > 1 && phase2ActivationCache.empty() && phase2ActivationHash == uint256())
    {
        // Prevent infinite recursion.
        static bool once = true;
        if (once)
        {
            once = false;
            PerformFullChainPhaseScan(pIndex, chainparams, chain, viewOverride);
        }
    }

    // First make sure that none of the obvious conditions that would preclude us from being active are true, if they are we can just abort testing immediately.
    static int checkDepth = IsArgSet("-testnet") ? 10 : earliestPossibleMainnetWitnessACtivationHeight;
    if (!pIndex || !pIndex->pprev || pIndex->nHeight < checkDepth )
        return false;

    // Next check if we have been already called for this hash, if we are just grab the value from the cache.
    auto findIter = phase2ActivationCache.find(pIndex->GetBlockHashPoW2());
    if (findIter != phase2ActivationCache.end())
        return phase2ActivationCache[pIndex->GetBlockHashPoW2()];

    // Now we try make use of the activation hash (if phase 2 is already active) to speed up testing.
    if (phase2ActivationHash == uint256())
        phase2ActivationHash = ppow2witdbview->GetPhase2ActivationHash();
    if (phase2ActivationHash != uint256())
    {
        const CBlockIndex* pIndexPrev = pIndex->pprev;
        if (pIndexPrev)
        {
            // If our parent is the activation hash then we can are also active.
            if (pIndexPrev->phashBlock && pIndexPrev->GetBlockHashPoW2() == phase2ActivationHash)
            {
                phase2ActivationCache[pIndex->GetBlockHashPoW2()] = true;
                return true;
            }
            else
            {
                // If our parent is cached then we can use our parents cache value.
                findIter = phase2ActivationCache.find(pIndexPrev->GetBlockHashPoW2());
                if (findIter != phase2ActivationCache.end())
                {
                    if (findIter->second)
                    {
                        phase2ActivationCache[pIndex->GetBlockHashPoW2()] = true;
                        return true;
                    }
                }
            }
        }
    }

    {
        LOCK2(cs_main, pactiveWallet?&pactiveWallet->cs_wallet:NULL);
        // If our ancestor is not active for phase 2 then we have to consult the version bits to know for sure, otherwise we can assume we are as well.
        bool isActive = (VersionBitsState(pIndex, chainparams.GetConsensus(), Consensus::DEPLOYMENT_POW2_PHASE2, versionbitscache) == THRESHOLD_ACTIVE);
        // If we are the first ever block to test as active, or if we are active and none of our ancestors are (can happen if a fork starts from before previous activation point)
        // Then set ourselves as the activation hash.
        if (isActive)
        {
            phase2ActivationHash = pIndex->GetBlockHashPoW2();
            ppow2witdbview->SetPhase2ActivationHash(phase2ActivationHash);
        }
        // Cache our value as well to assist future lookups.
        phase2ActivationCache[pIndex->GetBlockHashPoW2()] = isActive;
        return isActive;
    }
}


// Phase 3 becomes active after 200 or more witnessing addresses are present on the chain, as well as a combined witness weight of 20 000 000 or more.
// 'backwards compatible' witnessing becomes possible at this point.
// prevhash of blocks continue to point to previous PoW block alone.
// prevhash of witness block is stored in coinbase.
bool IsPow2Phase3Active(const CBlockIndex* pIndex,  const CChainParams& chainparams, CChain& chain, CCoinsViewCache* viewOverride)
{
    DO_BENCHMARK("WIT: IsPow2Phase3Active", BCLog::BENCH|BCLog::WITNESS);

    // First make sure that none of the obvious conditions that would preclude us from being active are true, if they are we can just abort testing immediately.
    static int checkDepth = IsArgSet("-testnet") ? 10 : earliestPossibleMainnetWitnessACtivationHeight;
    if (!pIndex || !pIndex->pprev || pIndex->nHeight < checkDepth )
        return false;

    // Optimisation - If we have never activated phase 2 then phase 3 can't possibly be active either.
    if (phase2ActivationHash == uint256())
        return false;

    // Now we try make use of the activation hash (if phase 3 is already active) to speed up testing.
    if (phase3ActivationHash == uint256())
        phase3ActivationHash = ppow2witdbview->GetPhase3ActivationHash();
    if (phase3ActivationHash != uint256())
    {
        const CBlockIndex* pIndexPrev = pIndex;
        while (pIndexPrev)
        {
            if (pIndexPrev->phashBlock && pIndexPrev->GetBlockHashPoW2() == phase3ActivationHash)
                return true;
            // No point searching any further as we won't find any true matches below this depth.
            if (pIndexPrev->nHeight < checkDepth)
                break;
            pIndexPrev = pIndexPrev->pprev;
        }
    }

    {
        LOCK2(cs_main, pactiveWallet?&pactiveWallet->cs_wallet:NULL);
        // If none of our ancestors were active for phase 3 then we have to calculate whether we qualify for phase 3 activation or not.
        {
            // First try the optimised route to avoid the expensive route if it is unnecessary.
            // We check whether this block has created any new witness address, if it has not then the phase can't possibly be any different than before.
            // NB! Even though this requires disk access it is still faster for -long- phase 2 chains because phase 3 check requires cloning the entire chain and performing several undos to the mempool each time.
            std::shared_ptr<CBlock> pCheckBlock(new CBlock);
            if (!ReadBlockFromDisk(*pCheckBlock, pIndex, Params().GetConsensus()))
                return false;
            bool containsAnyNewWitnessAddresses = false;
            for (const auto& transactionRef : pCheckBlock->vtx)
            {
                for (const auto& outRef : transactionRef->vout)
                {
                    if (IsPow2WitnessOutput(outRef))
                    {
                        containsAnyNewWitnessAddresses = true;
                        break;
                    }
                }
                if (containsAnyNewWitnessAddresses)
                    break;
            }
            if (!containsAnyNewWitnessAddresses)
                return false;
        }
        {
            // Perform the expensive phase 3 check, we want to make sure that a certain quantity of witnessing addresses and a certain weight are present before we allow phase 3 to activate.
            int64_t nNumWitnessAddresses;
            int64_t nTotalWeight;
            GetPow2NetworkWeight(pIndex, chainparams, nNumWitnessAddresses, nTotalWeight, chain, viewOverride);
            //fixme: (2.0) (POW2) (LAUNCH) - Finalise paramaters here.
            const int64_t nNumWitnessAddressesRequired = IsArgSet("-testnet") ? 10 : 200;
            const int64_t nTotalWeightRequired = IsArgSet("-testnet") ? 2000000 : 20000000;
            // If we are the first ever block to test as active, or if the previous active block is not our parent (can happen in the case of a fork from before activation)
            // Then set ourselves as the activation hash.
            if (nNumWitnessAddresses >= nNumWitnessAddressesRequired && nTotalWeight > nTotalWeightRequired)
            {
                phase3ActivationHash = pIndex->GetBlockHashPoW2();
                ppow2witdbview->SetPhase3ActivationHash(phase3ActivationHash);
                return true;
            }
            return false;
        }
    }
}

// Phase 4 becomes active after witnesses signal that 95% of peers are upgraded, for an entire 'signal window'
// Creation of new 'backwards compatible' witness addresses becomes impossible at this point.
// Full 'backwards incompatible' witnessing triggers at this point.
// New 'segsig' transaction format triggers at this point.
// All peers of version < 2.0 can no longer transact at this point.
// prevhash of blocks starts to point to witness header instead of PoW header
bool IsPow2Phase4Active(const CBlockIndex* pIndex, const CChainParams& chainparams, CChain& chain, CCoinsViewCache* viewOverride)
{
    DO_BENCHMARK("WIT: IsPow2Phase4Active", BCLog::BENCH|BCLog::WITNESS);

    // First make sure that none of the obvious conditions that would preclude us from being active are true, if they are we can just abort testing immediately.
    static int checkDepth = IsArgSet("-testnet") ? 10 : earliestPossibleMainnetWitnessACtivationHeight;
    if (!pIndex || !pIndex->pprev || pIndex->nHeight < checkDepth )
        return false;

    // Optimisation - If we have never activated phase 3 then phase 4 can't possibly be active either.
    if (phase3ActivationHash == uint256())
        return false;

    // Now we try make use of the activation hash (if phase 3 is already active) to speed up testing.
    if (phase4ActivationHash == uint256())
        phase4ActivationHash = ppow2witdbview->GetPhase4ActivationHash();
    if (phase4ActivationHash != uint256())
    {
        const CBlockIndex* pIndexPrev = pIndex;
        while (pIndexPrev)
        {
            if (pIndexPrev->GetBlockHashPoW2() == phase4ActivationHash)
                return true;
            // No point searching any further as we won't find any true matches below this depth.
            if (pIndexPrev->nHeight < checkDepth)
                break;
            pIndexPrev = pIndexPrev->pprev;
        }
    }

    {
        LOCK2(cs_main, pactiveWallet?&pactiveWallet->cs_wallet:NULL);
        // Version bits - mined by PoW but controlled by witnesses.
        bool ret = (VersionBitsState(pIndex, chainparams.GetConsensus(), Consensus::DEPLOYMENT_POW2_PHASE4, versionbitscache) == THRESHOLD_ACTIVE);
        // If we are the first ever block to test as active, or if the previous active block is not our parent (can happen in the case of a fork from before activation)
        // Then set ourselves as the activation hash.
        if (ret)
        {
            phase4ActivationHash = pIndex->GetBlockHashPoW2();
            ppow2witdbview->SetPhase4ActivationHash(phase4ActivationHash);
            return true;
        }
    }
    return false;
}


// Phase 5 becomes active after all 'backwards compatible' witness addresses have been flushed from the system.
// At this point 'backwards compatible' witness addresses in the chain are treated simply as anyone can spend transactions (as they have all been spent this is fine) and old witness data is discarded.
static uint256 phase5ActivationHash;
bool IsPow2Phase5Active(const CBlockIndex* pIndex, const CChainParams& params, CChain& chain, CCoinsViewCache* viewOverride)
{
    DO_BENCHMARK("WIT: IsPow2Phase5Active", BCLog::BENCH|BCLog::WITNESS);

    LOCK2(cs_main, pactiveWallet?&pactiveWallet->cs_wallet:NULL);

    // First make sure that none of the obvious conditions that would preclude us from being active are true, if they are we can just abort testing immediately.
    static int checkDepth = IsArgSet("-testnet") ? 10 : earliestPossibleMainnetWitnessACtivationHeight;
    if (!pIndex || !pIndex->pprev || pIndex->nHeight < checkDepth )
        return false;

    // Optimisation - If we have never activated phase 4 then phase 5 can't possibly be active either.
    if (phase4ActivationHash == uint256())
        return false;

    // Now we try make use of the activation hash (if phase 3 is already active) to speed up testing.
    if (phase5ActivationHash == uint256())
        phase5ActivationHash = ppow2witdbview->GetPhase5ActivationHash();
    if (phase5ActivationHash != uint256())
    {
        const CBlockIndex* pIndexPrev = pIndex;
        while (pIndexPrev)
        {
            if (pIndexPrev->GetBlockHashPoW2() == phase5ActivationHash)
                return true;
            // No point searching any further as we won't find any true matches below this depth.
            if (pIndexPrev->nHeight < checkDepth)
                break;
            pIndexPrev = pIndexPrev->pprev;
        }
    }

    // Phase 5 can't be active if phase 4 is not.
    if (!IsPow2Phase4Active(pIndex, params, chain, viewOverride))
    {
        return false;
    }

    std::map<COutPoint, Coin> allWitnessCoins;
    if (!getAllUnspentWitnessCoins(chain, params, pIndex, allWitnessCoins, nullptr, viewOverride))
        return error("IsPow2Phase5Active: Failed to enumerate all witness coins");

    // If any PoW2WitnessOutput remain then we aren't active yet.
    for (auto iter : allWitnessCoins)
    {
        if (iter.second.out.GetType() != CTxOutType::PoW2WitnessOutput)
            return false;
    }

    // If we are the first ever block to test as active, or if the previous active block is not our parent (can happen in the case of a fork from before activation)
    // Then set ourselves as the activation hash.
    phase5ActivationHash = pIndex->GetBlockHashPoW2();
    ppow2witdbview->SetPhase5ActivationHash(phase5ActivationHash);
    return true;
}


bool IsPow2WitnessingActive(const CBlockIndex* pIndex, const CChainParams& chainparams, CChain& chain, CCoinsViewCache* viewOverride)
{
    DO_BENCHMARK("WIT: IsPow2WitnessingActive", BCLog::BENCH|BCLog::WITNESS);

    static int checkDepth = IsArgSet("-testnet") ? 10 : earliestPossibleMainnetWitnessACtivationHeight;
    if (!pIndex || !pIndex->pprev || pIndex->nHeight < checkDepth )
        return false;

    // If phase 3, 4 or 5 is active then witnessing is active - we only need to check phase 3 to be sure as if 4 or 5 is active for a block 3 will be as well.
    return IsPow2Phase3Active(pIndex, chainparams, chain, viewOverride);
}

int GetPoW2Phase(const CBlockIndex* pIndex, const CChainParams& chainparams, CChain& chain, CCoinsViewCache* viewOverride)
{
    DO_BENCHMARK("WIT: GetPoW2Phase", BCLog::BENCH|BCLog::WITNESS);

    int nRet = 1;
    if (IsPow2Phase2Active(pIndex, chainparams, chain, viewOverride))
    {
        nRet = 2;
        if (IsPow2Phase3Active(pIndex, chainparams, chain, viewOverride))
        {
            nRet = 3;
            if (IsPow2Phase4Active(pIndex, chainparams, chain, viewOverride))
                nRet = 4;
            //NB! We don't call IsPow2Phase5Active here as it can lead to infinite recursion in some cases.
        }
    }
    return nRet;
}

//NB! nAmount is already in internal monetary format (8 zeros) form when entering this function - i.e. the nAmount for 22 NLG is '2200000000' and not '22'
int64_t GetPoW2RawWeightForAmount(int64_t nAmount, int64_t nLockLengthInBlocks)
{
    DO_BENCHMARK("WIT: GetPoW2RawWeightForAmount", BCLog::BENCH|BCLog::WITNESS);

    // We rebase the entire formula to to match internal monetary format (8 zeros), so that we can work with fixed point precision.
    // We rebase back to 10 at the end for the final weight.
    static const arith_uint256 base = arith_uint256(COIN);
    static const arith_uint256 base3 = base*base*base;
    static const arith_uint256 BlocksPerYear = arith_uint256(365 * 576);
    #define BASE(x) (arith_uint256(x)*base)
    arith_uint256 Quantity = arith_uint256(nAmount);
    arith_uint256 nWeight = ((BASE(Quantity)) + ((Quantity*Quantity) / arith_uint256(100000))) * (BASE(1) + (BASE(nLockLengthInBlocks) / BlocksPerYear));
    #undef BASE
    nWeight /= base3;
    return nWeight.GetLow64();
}


int64_t GetPoW2LockLengthInBlocksFromOutput(const CTxOut& out, uint64_t txBlockNumber, uint64_t& nFromBlockOut, uint64_t& nUntilBlockOut)
{
    DO_BENCHMARK("WIT: GetPoW2LockLengthInBlocksFromOutput", BCLog::BENCH|BCLog::WITNESS);

    if ( (out.GetType() <= CTxOutType::ScriptLegacyOutput && out.output.scriptPubKey.IsPoW2Witness()) )
    {
        CTxOutPoW2Witness witnessDetails;
        out.output.scriptPubKey.ExtractPoW2WitnessFromScript(witnessDetails);
        nFromBlockOut =  witnessDetails.lockFromBlock == 0 ? txBlockNumber : witnessDetails.lockFromBlock;
        nUntilBlockOut = witnessDetails.lockUntilBlock;
    }
    else if (out.GetType() == CTxOutType::PoW2WitnessOutput)
    {
        nFromBlockOut = out.output.witnessDetails.lockFromBlock == 0 ? txBlockNumber : out.output.witnessDetails.lockFromBlock;
        nUntilBlockOut = out.output.witnessDetails.lockUntilBlock;
    }
    return (nUntilBlockOut + 1) - nFromBlockOut;
}

int64_t GetPoW2Phase3ActivationTime(CChain& chain, CCoinsViewCache* viewOverride)
{
    DO_BENCHMARK("WIT: GetPoW2Phase3ActivationTime", BCLog::BENCH|BCLog::WITNESS);

    if (phase3ActivationHash == uint256())
        phase3ActivationHash = ppow2witdbview->GetPhase3ActivationHash();

    //NB! We don't have to worry about re-organisations here, as the phase checks will ensure that phase3ActivationHash is always current for the chain we are on.
    if (phase3ActivationHash != uint256())
    {
        if (mapBlockIndex.count(phase3ActivationHash))
        {
            CBlockIndex* pIndex = mapBlockIndex[phase3ActivationHash];
            return pIndex->nTime;
        }
    }
    if (IsPow2Phase3Active(chain.Tip(), Params(), chain, viewOverride))
    {
        CBlockIndex* pIndex = chain.Tip();
        while (pIndex->pprev && IsPow2Phase3Active(pIndex->pprev, Params(), chain, viewOverride))
        {
            pIndex = pIndex->pprev;
        }
        phase3ActivationHash = pIndex->GetBlockHashPoW2();
        ppow2witdbview->SetPhase3ActivationHash(phase3ActivationHash);
        return pIndex->nTime;
    }
    return std::numeric_limits<int64_t>::max();
}

#include <LRUCache/LRUCache11.hpp>
typedef lru11::Cache<uint256, std::pair<int64_t, int64_t>, lru11::NullLock, std::unordered_map<uint256, typename std::list<lru11::KeyValuePair<uint256, std::pair<int64_t, int64_t>>>::iterator, BlockHasher>> BlockWeightCache;
BlockWeightCache networkWeightCache(1000,500);
bool GetPow2NetworkWeight(const CBlockIndex* pIndex, const CChainParams& chainparams, int64_t& nNumWitnessAddresses, int64_t& nTotalWeight, CChain& chain, CCoinsViewCache* viewOverride)
{
    DO_BENCHMARK("WIT: GetPow2NetworkWeight", BCLog::BENCH|BCLog::WITNESS);

    const auto& blockHash = pIndex->GetBlockHashPoW2();
    if (networkWeightCache.contains(blockHash))
    {
        const auto& cachePair = networkWeightCache.get(blockHash);
        nNumWitnessAddresses = cachePair.first;
        nTotalWeight = cachePair.second;
        return true;
    }

    {
        LOCK2(cs_main, pactiveWallet?&pactiveWallet->cs_wallet:NULL);

        std::map<COutPoint, Coin> allWitnessCoins;
        if (!getAllUnspentWitnessCoins(chain, chainparams, pIndex, allWitnessCoins, nullptr, viewOverride))
            return error("GetPow2NetworkWeight: Failed to enumerate all unspent witness coins");

        nNumWitnessAddresses = 0;
        nTotalWeight = 0;

        for (auto iter : allWitnessCoins)
        {
            if (pIndex == nullptr || iter.second.nHeight <= pIndex->nHeight)
            {
                CTxOut output = iter.second.out;

                uint64_t nUnused1, nUnused2;
                nTotalWeight += GetPoW2RawWeightForAmount(output.nValue, GetPoW2LockLengthInBlocksFromOutput(output, iter.second.nHeight, nUnused1, nUnused2));
                ++nNumWitnessAddresses;
            }
        }
    }
    networkWeightCache.insert(blockHash, std::make_pair(nNumWitnessAddresses, nTotalWeight));
    return true;
}



CBlockIndex* GetPoWBlockForPoSBlock(const CBlockIndex* pIndex)
{
    DO_BENCHMARK("WIT: GetPoWBlockForPoSBlock", BCLog::BENCH|BCLog::WITNESS);

    AssertLockHeld(cs_main); // Required for ReadBlockFromDisk.

    uint256 powHash = pIndex->GetBlockHashLegacy();
    if (!mapBlockIndex.count(powHash))
    {
        std::shared_ptr<CBlock> pBlockPoW(new CBlock);
        if (!ReadBlockFromDisk(*pBlockPoW, pIndex, Params().GetConsensus()))
            return nullptr;

        // Strip any witness information from the block we have been given to get back to the raw PoW block on which it was based.
        for (unsigned int i = 1; i < pBlockPoW->vtx.size(); i++)
        {
            if (pBlockPoW->vtx[i]->IsCoinBase() && pBlockPoW->vtx[i]->IsPoW2WitnessCoinBase())
            {
                while (pBlockPoW->vtx.size() > i)
                {
                    pBlockPoW->vtx.pop_back();
                }
                break;
            }
        }
        pBlockPoW->nVersionPoW2Witness = 0;
        pBlockPoW->nTimePoW2Witness = 0;
        pBlockPoW->hashMerkleRootPoW2Witness = uint256();
        pBlockPoW->witnessHeaderPoW2Sig.clear();

        if (!ProcessNewBlock(Params(), pBlockPoW, true, nullptr))
            return nullptr;
    }
    return mapBlockIndex[powHash];
}



