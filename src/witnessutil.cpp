// Copyright (c) 2015-2022 The Centure developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the Libre Chain License, see the accompanying
// file COPYING

#include "wallet/wallettx.h"
#ifdef ENABLE_WALLET
#include "wallet/wallet.h"
#endif
#include "timedata.h"
#include "util.h"
#include "consensus/validation.h"
#include "validation/validation.h"
#include "validation/versionbitsvalidation.h"
#include "validation/witnessvalidation.h"
#include "versionbits.h"
#include "net_processing.h"
#include "txdb.h"

#include "primitives/transaction.h"
#include <LRUCache/LRUCache11.hpp>


#ifdef ENABLE_WALLET
static bool alreadyInRescan = false;
void rescanThread()
{
    if (alreadyInRescan)
        return;
    alreadyInRescan = true;
    pactiveWallet->ScanForWalletTransactions(chainActive.Genesis(), true);
    alreadyInRescan = false;
}

//fixme: (2.2) (UNITY) (HIGH) - While this works as a temporary way of achieving things it is far from ideal
//Rearchitecture this into something more appropriate.
//When we perform a link we need to ensure that the wallet that empties our transaction broadcasts
//As we reset all the networking as part of the rescan this doesn't always happen "naturally" like it should
//And the 'trickle' transaction code further works against us etc.
//So for now we use this heavy handed approach of forcefully rebroadcasting the transactions every five seconds for 30 seconds which is enough to ensure the transactions propagate
void broadcastTransactionsThread()
{
    for (int i=0; i < 6; ++i)
    {
        MilliSleep(5000);
        // Resend any inventory that might have been pending send before we cleared all the connections.
        for (CWallet* pwallet : vpwallets) {
            LOCK2(cs_main, pwallet->cs_wallet);
            pwallet->ResendWalletTransactionsBefore(GetTime(), g_connman.get());
        }
    }
}

void ResetSPVStartRescanThread()
{
    // reset SPV first
    {
        LOCK(cs_main);

        // disconnect all nodes to avoid problems with inconsistent block index state
        g_connman->ForEachNode([](CNode* node)
        {
            g_connman->DisconnectNode(node->GetId());
        });

        // clear old spv requests (and also avoid invalid block index access)
        CancelAllPriorityDownloads();

        // reset wallet spv, note that this will also
        // 1) restart spv and partial sync
        // 2) clear the partial sync state underneath (in most cases, as likely
        // too much of the index will have been pruned)
        if (fSPV) {
            for (CWallet* pwallet : vpwallets) {
                LOCK(pwallet->cs_wallet);
                pwallet->ResetSPV();
            }
        }

        std::thread(broadcastTransactionsThread).detach();
    }

    // second do the standard rescan thread in fullsync
    if (isFullSyncMode()) {
        std::thread(rescanThread).detach();
    }
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
#endif

bool IsPow2Phase2Active(const CBlockIndex* pindexPrev)
{
    if (pindexPrev && ((uint64_t)pindexPrev->nHeight >= Params().GetConsensus().pow2Phase2FirstBlockHeight))
            return true;
    
    return false;
}

bool IsPow2Phase3Active(uint64_t nHeight)
{
    if (nHeight >= Params().GetConsensus().pow2Phase3FirstBlockHeight)
        return true;
    return false;
}

bool IsPow2Phase4Active(const CBlockIndex* pindexPrev)
{
    if (pindexPrev && ((uint64_t)pindexPrev->nHeight >= Params().GetConsensus().pow2Phase4FirstBlockHeight))
    {
        return true;
    }
    return false;
}


// Phase 5 becomes active after all 'backwards compatible' witness addresses have been flushed from the system.
// At this point 'backwards compatible' witness addresses in the chain are treated simply as anyone can spend transactions (as they have all been spent this is fine) and old witness data is discarded.
bool IsPow2Phase5Active(uint64_t nHeight)
{
    if (nHeight >= Params().GetConsensus().pow2Phase5FirstBlockHeight)
        return true;
    return false;
}

bool IsPow2Phase5Active(const CBlockIndex* pIndex, const CChainParams& params, CChain& chain, CCoinsViewCache* viewOverride)
{
    if (pIndex)
    {
        return IsPow2Phase5Active((uint64_t)pIndex->nHeight);
    }
    return false;
}

bool IsPow2WitnessingActive(uint64_t nHeight)
{
    // If phase 3, 4 or 5 is active then witnessing is active - we only need to check phase 3 to be sure as if 4 or 5 is active for a block 3 will be as well.
    return IsPow2Phase3Active(nHeight);
}

int GetPoW2Phase(const CBlockIndex* pindexPrev)
{
    int nRet = 1;
    if (IsPow2Phase2Active(pindexPrev))
    {
        nRet = 2;
        if (IsPow2Phase3Active(pindexPrev?pindexPrev->nHeight:0))
        {
            nRet = 3;
            if (IsPow2Phase4Active(pindexPrev))
                nRet = 4;
            //NB! We don't call IsPow2Phase5Active here as it can lead to infinite recursion in some cases.
        }
    }
    return nRet;
}

//NB! nAmount is already in internal monetary format (8 zeros) form when entering this function - i.e. the nAmount for 22 NLG is '2200000000' and not '22'
int64_t GetPoW2RawWeightForAmount(int64_t nAmount, int64_t nHeight, int64_t nLockLengthInBlocks)
{
    (unused) nHeight;
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


uint64_t GetPoW2LockLengthInBlocksFromOutput(const CTxOut& out, uint64_t txBlockNumber, uint64_t& nFromBlockOut, uint64_t& nUntilBlockOut)
{
    if (out.GetType() == CTxOutType::PoW2WitnessOutput)
    {
        nFromBlockOut = out.output.witnessDetails.lockFromBlock == 0 ? txBlockNumber : out.output.witnessDetails.lockFromBlock;
        nUntilBlockOut = out.output.witnessDetails.lockUntilBlock;
    }
    else if (out.GetType() <= CTxOutType::ScriptLegacyOutput)
    {
        CTxOutPoW2Witness witnessDetails;
        out.output.scriptPubKey.ExtractPoW2WitnessFromScript(witnessDetails);
        nFromBlockOut =  witnessDetails.lockFromBlock == 0 ? txBlockNumber : witnessDetails.lockFromBlock;
        nUntilBlockOut = witnessDetails.lockUntilBlock;
    }
    else
    {
        assert(0);
    }
    return (nUntilBlockOut - nFromBlockOut)+1;
}

uint64_t GetPoW2RemainingLockLengthInBlocks(uint64_t lockUntilBlock, uint64_t tipHeight)
{
    return (lockUntilBlock < tipHeight) ? 0 : (lockUntilBlock - tipHeight) + 1;
}

typedef std::pair<int64_t, int64_t> NumAndWeight;
typedef lru11::Cache<uint256, NumAndWeight, lru11::NullLock, std::unordered_map<uint256, typename std::list<lru11::KeyValuePair<uint256, NumAndWeight>>::iterator, BlockHasher>> BlockWeightCache;
BlockWeightCache networkWeightCache(800,100);
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
        #ifdef ENABLE_WALLET
        LOCK2(cs_main, pactiveWallet?&pactiveWallet->cs_wallet:NULL);
        #else
        LOCK(cs_main);
        #endif

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
                nTotalWeight += GetPoW2RawWeightForAmount(output.nValue, pIndex->nHeight, GetPoW2LockLengthInBlocksFromOutput(output, iter.second.nHeight, nUnused1, nUnused2));
                ++nNumWitnessAddresses;
            }
        }
    }
    networkWeightCache.insert(blockHash, std::pair(nNumWitnessAddresses, nTotalWeight));
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
        if (!ReadBlockFromDisk(*pBlockPoW, pIndex, Params()))
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
        pBlockPoW->witnessUTXODelta.clear();

        if (!ProcessNewBlock(Params(), pBlockPoW, true, nullptr, false, true))
            return nullptr;
    }
    return mapBlockIndex[powHash];
}

int GetPow2ValidationCloneHeight(CChain& chain, const CBlockIndex* pIndex, int nMargin)
{
    if (pIndex->nHeight <= 10)
        return 0;

    const CBlockIndex* pprevFork = chainActive.FindFork(pIndex);
    return (pprevFork->nHeight > nMargin ? pprevFork->nHeight - nMargin : 0);
}

bool IsPartialSyncActive()
{
    return partialChain.Height() >= partialChain.HeightOffset();
}

bool IsPartialNearPresent(enum BlockStatus nUpTo)
{
    if (!IsPartialSyncActive())
        return false;

    CBlockIndex* index = partialChain.Tip();
    int64_t farBack = GetAdjustedTime() - Params().GetConsensus().nPowTargetSpacing * 20;
    while (   index
           && index->nHeight >= partialChain.HeightOffset()
           && (index->GetBlockTime() <= farBack || !index->IsPartialValid(nUpTo)))
    {
        index = index->pprev;
    }

    // index && index->nHeight >= partialChain.HeightOffset() => index->GetBlockTime() > farBack && index->IsPartialValid(nUpTo)
    return index && index->nHeight >= partialChain.HeightOffset();
}

bool IsChainNearPresent()
{
    CBlockIndex* index = chainActive.Tip();
    if (index == nullptr)
        return false;

    int64_t farBack = GetAdjustedTime() - Params().GetConsensus().nPowTargetSpacing * 20;

    return index->GetBlockTime() > farBack;
}

int DailyBlocksTarget()
{
    return (24 * 60 * 60) / Params().GetConsensus().nPowTargetSpacing;
}

uint64_t MinimumWitnessLockLength()
{
    if (Params().IsOfficialTestnetV1())
    {
        return gMinimumWitnessLockDays * gRefactorDailyBlocksUsage;
    }
    else if (Params().IsTestnet())
    {
        return 50;
    }
    else
    {
        return gMinimumWitnessLockDays * DailyBlocksTarget();
    }
}

uint64_t MaximumWitnessLockLength()
{
    if (Params().IsTestnet())
    {
        return gMaximumWitnessLockDays * gRefactorDailyBlocksUsage;
    }
    else
    {
        return gMaximumWitnessLockDays * DailyBlocksTarget();
    }
}

static constexpr int recovery_birth_period = 7 * 24 * 3600; // one week

int timeToBirthNumber(const int64_t time)
{
    // 0 for invalid time
    if (time < Params().GenesisBlock().nTime)
        return 0;

    int periods = (time - Params().GenesisBlock().nTime) / recovery_birth_period;

    return Base10ChecksumEncode(periods);
}

int64_t birthNumberToTime(int number)
{
    int periods;
    if (!Base10ChecksumDecode(number, &periods))
        return 0;

    return Params().GenesisBlock().nTime + int64_t(periods) * recovery_birth_period;
}
