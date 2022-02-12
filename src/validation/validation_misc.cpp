// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// File contains modifications by: The Gulden developers
// All modifications:
// Copyright (c) 2016-2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#include "validation.h"
#include "validationinterface.h"
#include "witnessvalidation.h"
#include "versionbitsvalidation.h"
#include <consensus/validation.h>

#include "blockstore.h"
#include "txdb.h"
#include "net.h"
#include "chainparams.h"
#include "versionbits.h"
#include <net_processing.h>

/** Convert CValidationState to a human-readable message for logging */
std::string FormatStateMessage(const CValidationState &state)
{
    return strprintf("%s%s (code %i)",
        state.GetRejectReason(),
        state.GetDebugMessage().empty() ? "" : ", "+state.GetDebugMessage(),
        state.GetRejectCode());
}

/** Return transaction in txOut, and if it was found inside a block, its hash is placed in hashBlock */
bool GetTransaction(const uint256 &hash, CTransactionRef &txOut, const CChainParams& params, uint256 &hashBlock, bool fAllowSlow)
{
    CBlockIndex *pindexSlow = NULL;

    LOCK(cs_main); // Required for ReadBlockFromDisk.

    CTransactionRef ptx = mempool.get(hash);
    if (ptx)
    {
        txOut = ptx;
        return true;
    }

    if (fTxIndex) {
        CDiskTxPos postx;
        if (pblocktree->ReadTxIndex(hash, postx)) {
            CFile file(blockStore.GetBlockFile(postx, true), SER_DISK, CLIENT_VERSION);
            if (file.IsNull())
                return error("%s: OpenBlockFile failed", __func__);
            CBlockHeader header;
            try {
                file >> header;
                fseek(file.Get(), postx.nTxOffset, SEEK_CUR);
                file >> txOut;
            } catch (const std::exception& e) {
                return error("%s: Deserialize or I/O error - %s", __func__, e.what());
            }
            hashBlock = header.GetHashPoW2();
            if (txOut->GetHash() != hash)
                return error("%s: txid mismatch, block hash [%s]", __func__, hashBlock.ToString());
            return true;
        }
    }

    if (fAllowSlow) { // use coin database to locate block that contains transaction, and scan it
        const Coin& coin = AccessByTxid(*pcoinsTip, hash);
        if (!coin.IsSpent()) pindexSlow = chainActive[coin.nHeight];
    }

    if (pindexSlow) {
        CBlock block;
        if (ReadBlockFromDisk(block, pindexSlow, params)) {
            for (const auto& tx : block.vtx) {
                if (tx->GetHash() == hash) {
                    txOut = tx;
                    hashBlock = pindexSlow->GetBlockHashPoW2();
                    return true;
                }
            }
        }
    }

    return false;
}


const int finalSubsidyBlock = 17727500;
BlockSubsidy GetBlockSubsidy(uint64_t nHeight)
{
    static bool fRegTest = Params().IsRegtest();
    static bool fRegTestLegacy = Params().IsRegtestLegacy();
    static bool fTestnet = Params().IsTestnet();
    if (fRegTestLegacy)
        return BlockSubsidy(50*COIN, 0, 0); 
    if (fRegTest)
    {
        if (nHeight == 0)
        {
            return BlockSubsidy(50*COIN, 50*COIN, 0*COIN);
        }
        else
        {
            return BlockSubsidy(50*COIN, 50*COIN, 50*COIN);
        }
    }
    

    if(nHeight == 1)
    {
        CAmount witnessSubsidyGenesis=0;
        if (fTestnet)
            witnessSubsidyGenesis=30*COIN;
        return BlockSubsidy(170000000*COIN, witnessSubsidyGenesis, 0); // First block (premine)
    }
    else if(nHeight < Params().GetConsensus().fixedRewardReductionHeight)
    {
        return BlockSubsidy(1000*COIN, 0, 0); // 1000 Gulden per block for first 250k blocks
    }
    else if(nHeight < Params().GetConsensus().devBlockSubsidyActivationHeight)
    {
        return BlockSubsidy(100*COIN, 0, 0); // 100 Gulden per block (fixed reward/no halving)
    }
    else if (nHeight < Params().GetConsensus().pow2Phase4FirstBlockHeight+1)
    {
        return BlockSubsidy(50*COIN, 20*COIN, 40*COIN); // 110 Gulden per block (fixed reward/no halving) - 50 mining, 40 development, 20 witness.
    }
    else if(nHeight <= 1226651)
    {
        return BlockSubsidy(50*COIN, 30*COIN, 40*COIN); // 120 Gulden per block (fixed reward/no halving) - 50 mining, 40 development, 30 witness.
    }
    else if(nHeight <= 1228003)
    {
        return BlockSubsidy(90*COIN, 30*COIN, 80*COIN); // 200 Gulden per block (fixed reward/no halving) - 90 mining, 80 development, 30 witness.
    }
    else if(nHeight <= Params().GetConsensus().halvingIntroductionHeight)
    {
        return BlockSubsidy(50*COIN, 30*COIN, 80*COIN); // 160 Gulden per block (fixed reward/no halving) - 50 mining, 80 development, 30 witness.
    }
    // From this point on reward is as follows:
    // 90 Gulden per block; 10 mining, 15 witness, 65 development
    // Halving every 842500 blocks (roughly 4 years)
    // Rewards truncated to a maximum of 2 decimal places if large enough to have a number on the left of the decimal place
    // Otherwise truncated to 3 decimal places (if first place is occupied with a non zero number or otherwise a maximum of 4 decimal places
    // This is done to keep numbers a bit cleaner and more manageable
    // Halvings as follows:
    // 5 mining, 7.5 witness, 32.5 development
    // 2.5 mining, 3.75 witness, 16.25 development
    // 1.25 mining, 1.87 witness, 8.12 development
    // 0.625 mining, 0.937 witness, 4.06 development
    // 0.312 mining, 0.468 witness, 2.03 development
    // 0.156 mining, 0.234 witness, 1.01 development
    // 0.0781 mining, 0.117 witness, 0.507 development
    // 0.0390 mining, 0.0585 witness, 0.253 development
    // 0.0195 mining, 0.0292 witness, 0.126 development
    // 0.0976 mining, 0.0146 witness, 0.634 development
    // 0.0488 mining, 0.0732 witness, 0.317 development
    // 0.0244 mining, 0.0366 witness, 0.158 development
    // 0.0122 mining, 0.0183 witness, 0.0793 development
    // 0.0061 mining, 0.0091 witness, 0.0396 development
    // 0.0030 mining, 0.0045 witness, 0.0198 development
    // 0.0015 mining, 0.0022 witness, 0.0099 development
    // 0.0007 mining, 0.0011 witness, 0.0049 development
    // 0.0003 mining, 0.0005 witness, 0.0024 development
    // 0.0001 mining, 0.0002 witness, 0.0012 development
    else
    {
        // NB! We could use some bit shifts and other tricks here to do the halving calculations (the specific truncation rounding we are using makes it a bit difficult)
        // However we opt instead for this simple human readable "table" layout so that it is easier for humans to inspect/verify this.
        int nHalvings = (nHeight - 1 - Params().GetConsensus().halvingIntroductionHeight) / 842500;
        switch(nHalvings)
        {
            case 0:  return BlockSubsidy(1000000*MILLICENT, 1500000*MILLICENT, 6500000*MILLICENT);
            case 1:  return BlockSubsidy( 500000*MILLICENT,  750000*MILLICENT, 3250000*MILLICENT);
            case 2:  return BlockSubsidy( 250000*MILLICENT,  375000*MILLICENT, 1625000*MILLICENT);
            case 3:  return BlockSubsidy( 125000*MILLICENT,  187000*MILLICENT, 812000*MILLICENT );
            case 4:  return BlockSubsidy(  62500*MILLICENT,   93700*MILLICENT, 406000*MILLICENT );
            case 5:  return BlockSubsidy(  31200*MILLICENT,   46800*MILLICENT, 203000*MILLICENT );
            case 6:  return BlockSubsidy(  15600*MILLICENT,   23400*MILLICENT, 101000*MILLICENT );
            case 7:  return BlockSubsidy(   7810*MILLICENT,   11700*MILLICENT,  50700*MILLICENT );
            case 8:  return BlockSubsidy(   3900*MILLICENT,    5850*MILLICENT,  25300*MILLICENT );
            case 9:  return BlockSubsidy(   1950*MILLICENT,    2920*MILLICENT,  12600*MILLICENT );
            case 10: return BlockSubsidy(    976*MILLICENT,    1460*MILLICENT,   6340*MILLICENT );
            case 11: return BlockSubsidy(    488*MILLICENT,     732*MILLICENT,   3170*MILLICENT );
            case 12: return BlockSubsidy(    244*MILLICENT,     366*MILLICENT,   1580*MILLICENT );
            case 13: return BlockSubsidy(    122*MILLICENT,     183*MILLICENT,    793*MILLICENT );
            case 14: return BlockSubsidy(     61*MILLICENT,      91*MILLICENT,    396*MILLICENT );
            case 15: return BlockSubsidy(     30*MILLICENT,      45*MILLICENT,    198*MILLICENT );
            case 16: return BlockSubsidy(     15*MILLICENT,      22*MILLICENT,     99*MILLICENT );
            case 17: return BlockSubsidy(      7*MILLICENT,      11*MILLICENT,     49*MILLICENT );
            case 18: return BlockSubsidy(      3*MILLICENT,       5*MILLICENT,     24*MILLICENT );
            case 19: if (nHeight <= finalSubsidyBlock)
                     {
                         return BlockSubsidy(  1*MILLICENT,       2*MILLICENT,     12*MILLICENT );
                     }
        }
    }
    return BlockSubsidy(0, 0, 0);
}

bool IsInitialBlockDownload()
{
    //AssertLockHeld(cs_main);
    const CChainParams& chainParams = Params();

    // Once this function has returned false, it must remain false.
    static std::atomic<bool> latchToFalse{false};
    // Optimization: pre-test latch before taking the lock.
    if (latchToFalse.load(std::memory_order_relaxed))
        return false;

    if (latchToFalse.load(std::memory_order_relaxed))
        return false;
    if (fImporting || fReindex)
        return true;
    if (chainActive.Tip() == NULL)
        return true;
    if (chainActive.Tip()->nChainWork < UintToArith256(chainParams.GetConsensus().nMinimumChainWork))
        return true;
    if (chainActive.Tip()->GetBlockTime() < (GetTime() - nMaxTipAge))
        return true;
    LogPrintf("Leaving InitialBlockDownload (latching to false)\n");
    latchToFalse.store(true, std::memory_order_relaxed);
    return false;
}

bool HaveRequiredPeerUpgradePercent(int nRequiredProtoVersion, unsigned int nRequiredPercent)
{
    std::vector<CNodeStats> vstats;
    g_connman->GetNodeStats(vstats);

    // Insufficient peers to determine.
    if (vstats.size() < 3)
    {
        return true;
    }

    int nUpgradeCount = 0;
    for (const CNodeStats& stats : vstats)
    {
        if (stats.nVersion >= nRequiredProtoVersion)
        {
            ++nUpgradeCount;
        }
    }
    return (100 * nUpgradeCount) / vstats.size() > nRequiredPercent;
}

int32_t ComputeBlockVersion(const CBlockIndex* pindexPrev, const Consensus::Params& params)
{
    LOCK(cs_main);
    int32_t nVersion = VERSIONBITS_TOP_BITS;

    for (int i = 0; i < (int)Consensus::MAX_VERSION_BITS_DEPLOYMENTS; i++)
    {
        ThresholdState state = VersionBitsState(pindexPrev, params, (Consensus::DeploymentPos)i, versionbitscache);
        if (state == THRESHOLD_LOCKED_IN)
        {
            nVersion |= VersionBitsMask(params, (Consensus::DeploymentPos)i);
        }
        else if (state == THRESHOLD_STARTED)
        {
            if (params.vDeployments[i].protoVersion == 0 || HaveRequiredPeerUpgradePercent(params.vDeployments[i].protoVersion, params.vDeployments[i].requiredProtoUpgradePercent))
            {
                nVersion |= VersionBitsMask(params, (Consensus::DeploymentPos)i);
            }
        }
    }

    return nVersion;
}

//! Guess how far we are in the verification process at the given block index
double GuessVerificationProgress(CBlockIndex *pindex) {
    if (pindex == NULL)
        return 0.0;

    double nSyncProgress = std::min(1.0, (double)pindex->nHeight / GetProbableHeight());
    return nSyncProgress;
}

bool GetTxHash(const COutPoint& outpoint, uint256& txHash)
{
    if (outpoint.isHash)
    {
        txHash = outpoint.getTransactionHash();
        return true;
    }
    else
    {
        LOCK(cs_main);
        CBlock block;
        if ((int)outpoint.getTransactionBlockNumber() <= chainActive.Height() && ReadBlockFromDisk(block, chainActive[outpoint.getTransactionBlockNumber()], Params()))
        {
            if (outpoint.getTransactionIndex() < block.vtx.size())
            {
                txHash = block.vtx[outpoint.getTransactionIndex()]->GetHash();
                return true;
            }
        }
    }
    return false;
}
