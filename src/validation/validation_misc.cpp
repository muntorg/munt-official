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
                return error("%s: txid mismatch", __func__);
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

//dev subsidy (over 10 years):
//(994'744'000+(900'000'000*10)) == 9'994'744'000
//mining subsidy:
//10'000'000'000-9'994'744'000 = 5256000
//5'256'000/(0.10*2) == 26'280'000 (blocks)
CAmount GetBlockSubsidy(uint64_t nHeight)
{
    static bool fRegTest = GetBoolArg("-regtest", false);
    if (fRegTest)
        return 50*COIN;
    
    if (nHeight == 0)
        return 994'744'000*COIN+GetBlockSubsidyWitness(nHeight);
        
    if (nHeight == 105120 || nHeight == 105120*2 || nHeight == 105120*3 || nHeight == 105120*4 || nHeight == 105120*5 || nHeight == 105120*6 || nHeight == 105120*7 || nHeight == 105120*8 || nHeight == 105120*9  || nHeight == 105120*10)
        return (10*CENT)+(900'000'000*COIN)+GetBlockSubsidyWitness(nHeight);

    if (nHeight > 26280000)
        return 0;

    return 10*CENT+GetBlockSubsidyWitness(nHeight);
}

CAmount GetBlockSubsidyWitness(uint64_t nHeight)
{
    if (nHeight > 26280000)
        return 0;

    return 10*CENT;
}

CAmount GetBlockSubsidyDev(uint64_t nHeight)
{
    if (nHeight == 105120 || nHeight == 105120*2 || nHeight == 105120*3 || nHeight == 105120*4 || nHeight == 105120*5 || nHeight == 105120*6 || nHeight == 105120*7 || nHeight == 105120*8 || nHeight == 105120*9 || nHeight == 105120*10)
        return (900'000'000*COIN);
    return 0;
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
double GuessVerificationProgress(const ChainTxData& data, CBlockIndex *pindex) {
    if (pindex == NULL)
        return 0.0;

    int64_t nNow = time(NULL);

    double fTxTotal;

    if (pindex->nChainTx <= data.nTxCount) {
        fTxTotal = data.nTxCount + (nNow - data.nTime) * data.dTxRate;
    } else {
        fTxTotal = pindex->nChainTx + (nNow - pindex->GetBlockTime()) * data.dTxRate;
    }

    return pindex->nChainTx / fTxTotal;
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
