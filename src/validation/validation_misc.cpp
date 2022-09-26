// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// File contains modifications by: The Centure developers
// All modifications:
// Copyright (c) 2016-2022 The Centure developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the Libre Chain License, see the accompanying
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
        return BlockSubsidy(1000*COIN, 0, 0); // 1000 Munt per block for first 250k blocks
    }
    else if(nHeight < Params().GetConsensus().devBlockSubsidyActivationHeight)
    {
        return BlockSubsidy(100*COIN, 0, 0); // 100 Munt per block (fixed reward/no halving)
    }
    else if (nHeight < Params().GetConsensus().pow2Phase4FirstBlockHeight+1)
    {
        return BlockSubsidy(50*COIN, 20*COIN, 40*COIN); // 110 Munt per block (fixed reward/no halving) - 50 mining, 40 development, 20 witness.
    }
    else if(nHeight <= 1226651)
    {
        return BlockSubsidy(50*COIN, 30*COIN, 40*COIN); // 120 Munt per block (fixed reward/no halving) - 50 mining, 40 development, 30 witness.
    }
    else if(nHeight <= 1228003)
    {
        return BlockSubsidy(90*COIN, 30*COIN, 80*COIN); // 200 Munt per block (fixed reward/no halving) - 90 mining, 80 development, 30 witness.
    }
    else if(nHeight <= Params().GetConsensus().halvingIntroductionHeight)
    {
        return BlockSubsidy(50*COIN, 30*COIN, 80*COIN); // 160 Munt per block (fixed reward/no halving) - 50 mining, 80 development, 30 witness.
    }
    // 90 Munt per block; 10 mining, 15 witness, 65 development    
    else if (nHeight < 1619997)
    {
        return BlockSubsidy(1000000*MILLICENT, 1500000*MILLICENT, 6500000*MILLICENT);
    }
    // Once off large development fund distribution after which the per block development reward is dropped
    else if (nHeight == 1619997)
    {
        return BlockSubsidy(1000000*MILLICENT, 1500000*MILLICENT, 100'000'000*COIN); //1619997  56817116000000000
    }
    // From this point on reward is as follows:
    // 25 Munt per block; 10 mining, 15 witness
    // Halving every 842500 blocks (roughly 4 years); first halving at block ???
    // We round to force only a single non-zero decimal digit instead of exact halving in order to keep the numbers as clean as possible throughout.
    // Halvings as follows:
    // 5 mining,          7.5 witness
    // 2 mining,          4 witness
    // 1 mining,          2 witness
    // 0.5 mining,        1 witness
    // 0.2 mining,        0.5 witness
    // 0.1 mining,        0.2 witness
    // 0.05 mining,       0.1 witness
    // 0.02 mining,       0.05 witness
    // 0.01 mining,       0.02 witness
    // 0.005 mining,      0.01 witness
    // 0.002 mining,      0.005 witness
    // 0.001 mining,      0.002 witness
    // 0.0005 mining,     0.001 witness
    // 0.0002 mining,     0.0005 witness
    // 0.0001 mining,     0.0002 witness
    // 0.00005 mining,    0.0001 witness
    // 0.00002 mining,    0.00005 witness
    // 0.00001 mining,    0.00002 witness
    // 0.000005 mining,   0.00001 witness
    // 0.000002 mining,   0.000005 witness
    // 0.000001 mining,   0.000002 witness
    // 0.0000005 mining,  0.000001 witness
    // 0.0000002 mining,  0.0000005 witness
    // 0.0000001 mining,  0.0000002 witness
    // 0.00000005 mining, 0.0000001 witness
    // 0.00000002 mining, 0.00000005 witness
    // 0.00000001 mining, 0.00000002 witness
    else
    {
        // NB! We opt for this simple human readable "table-like" layout so that it is easier for humans to inspect/verify this..
        int nHalvings = (nHeight - 1 - (Params().GetConsensus().halvingIntroductionHeight-167512)) / 842500;
        switch(nHalvings)
        {
            case 0:  return BlockSubsidy(1000000*MILLICENT, 1500000*MILLICENT, 0); // 1'619'998  668'171'185.00000000
            case 1:  return BlockSubsidy( 500000*MILLICENT,  750000*MILLICENT, 0); // 2'074'989  679'545'960.00000000
            case 2:  return BlockSubsidy( 200000*MILLICENT,  400000*MILLICENT, 0); // 2'917'489  690'077'210.00000000
            case 3:  return BlockSubsidy( 100000*MILLICENT,  200000*MILLICENT, 0); // 3'759'989  695'132'210.00000000
            case 4:  return BlockSubsidy(  50000*MILLICENT,  100000*MILLICENT, 0); // 4'602'489  697'659'710.00000000
            case 5:  return BlockSubsidy(  20000*MILLICENT,   50000*MILLICENT, 0); // 5'444'989  698'923'460.00000000
            case 6:  return BlockSubsidy(  10000*MILLICENT,   20000*MILLICENT, 0); // 6'287'489  699'513'210.00000000
            case 7:  return BlockSubsidy(   5000*MILLICENT,   10000*MILLICENT, 0); // 7'129'989  699'765'960.00000000
            case 8:  return BlockSubsidy(   2000*MILLICENT,    5000*MILLICENT, 0); // 7'972'489  699'892'335.00000000
            case 9:  return BlockSubsidy(   1000*MILLICENT,    2000*MILLICENT, 0); // 8'814'989  699'951'310.00000000
            case 10: return BlockSubsidy(    500*MILLICENT,    1000*MILLICENT, 0); // 9'657'489  699'976'585.00000000
            case 11: return BlockSubsidy(    200*MILLICENT,     500*MILLICENT, 0); //10'499'989  699'989'222.50000000
            case 12: return BlockSubsidy(    100*MILLICENT,     200*MILLICENT, 0); //11'342'489  699'995'120.00000000
            case 13: return BlockSubsidy(     50*MILLICENT,     100*MILLICENT, 0); //12'184'989  699'997'647.50000000
            case 14: return BlockSubsidy(     20*MILLICENT,      50*MILLICENT, 0); //13'027'489  699'998'911.25000000
            case 15: return BlockSubsidy(     10*MILLICENT,      20*MILLICENT, 0); //13'869'989  699'999'501.00000000
            case 16: return BlockSubsidy(      5*MILLICENT,      10*MILLICENT, 0); //14'712'489  699'999'753.75000000
            case 17: return BlockSubsidy(      2*MILLICENT,       5*MILLICENT, 0); //15'554'989  699'999'880.12500000
            case 18: return BlockSubsidy(      1*MILLICENT,       2*MILLICENT, 0); //16'397'489  699'999'939.10000000
            case 19: return BlockSubsidy(      500,               1*MILLICENT, 0); //17'239'989  699'999'964.37500000
            case 20: return BlockSubsidy(      200,               500        , 0); //18'082'489  699'999'977.01250000
            case 21: return BlockSubsidy(      100,               200        , 0); //18'924'989  699'999'982.91000000
            case 22: return BlockSubsidy(       50,               100        , 0); //19'767'489  699'999'985.43750000
            case 23: return BlockSubsidy(       20,                50        , 0); //20'609'989  699'999'986.70125000
            case 24: return BlockSubsidy(       10,                20        , 0); //21'452'489  699'999'987.29100000
            case 25: return BlockSubsidy(        5,                10        , 0); //22'294'989  699'999'987.54375000
            case 26: return BlockSubsidy(        2,                 5        , 0); //23'137'489  699'999'987.67012500
            default: {
                if (nHeight <= Params().GetConsensus().finalSubsidyBlockHeight)
                     return BlockSubsidy(        1,                 2        , 0); //23'979'989  699'999'987.72910000
            }
        }
    }
    return BlockSubsidy(0, 0, 0);                                                  //433'009'989 700'000'000.00000000
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
