// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// File contains modifications by: The Centure developers
// All modifications:
// Copyright (c) 2016-2022 The Centure developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the GNU Lesser General Public License v3, see the accompanying
// file COPYING

#include "generation/generation.h"
#include "miner.h"

#include "net.h"

#include "alert.h"
#include "amount.h"
#include "appname.h"
#include "chain.h"
#include "chainparams.h"
#include "coins.h"
#include "consensus/consensus.h"
#include "consensus/tx_verify.h"
#include "consensus/merkle.h"
#include "consensus/validation.h"
#include "hash.h"
#include "validation/validation.h"
#include "validation/witnessvalidation.h"
#include "policy/feerate.h"
#include "policy/policy.h"
#include "pow/pow.h"
#include "primitives/transaction.h"
#include "script/standard.h"
#include "timedata.h"
#include "txmempool.h"
#include "util.h"
#include "util/thread.h"
#include "util/threadnames.h"
#include "util/time.h"
#include "util/moneystr.h"
#include "validation/validationinterface.h"

#ifdef ENABLE_WALLET
#include <wallet/extwallet.h>
#endif

#include <algorithm>
#include <queue>
#include <utility>

#include <pow/diff.h>
#include <crypto/hash/hash.h>
#include <witnessutil.h>
#include <openssl/sha.h>

#include <boost/thread.hpp>

#include "txdb.h"

//Munt includes
#include "streams.h"
#include <boost/scope_exit.hpp>

#include "base58.h"

//
// Unconfirmed transactions in the memory pool often depend on other
// transactions in the memory pool. When we select transactions from the
// pool, we select by highest fee rate of a transaction combined with all
// its ancestors.
uint64_t nLastBlockTx = 0;
uint64_t nLastBlockSize = 0;
uint64_t nLastBlockWeight = 0;

uint64_t CalculateMissedTimeSteps(uint64_t nEnd, uint64_t nStart)
{
    static const uint64_t nDrift   = 1;
    static uint64_t nLongTimeLimit = ((6 * nDrift)) * 60;
    static uint64_t nLongTimeStep  = nDrift * 60;
    uint64_t nNumMissedSteps = 0;
    if ((nEnd - nStart) > nLongTimeLimit)
    {
        nNumMissedSteps = ((nEnd - nStart - nLongTimeLimit) / nLongTimeStep) + 1;
    }
    return nNumMissedSteps;
}

uint64_t GetAdjustedFutureTime()
{
    return GetAdjustedTime()+MAX_FUTURE_BLOCK_TIME-1;
}

int64_t UpdateTime(CBlockHeader* pblock, const Consensus::Params& consensusParams, const CBlockIndex* pindexPrev)
{
    int64_t nOldTime = pblock->nTime;
    int64_t nNewTime = std::max((uint64_t)pindexPrev->GetMedianTimePastWitness()+1, GetAdjustedFutureTime());

    if (nOldTime < nNewTime)
        pblock->nTime = nNewTime;

    //fixme: (PHASE5) - We can likely remove this
    // Allow miners to control maximum diff drop to suit their hashrate (prevent sudden self-ddos from 100s of blocks being found per second)
    int64_t nMaxMissedSteps = GetArg("-limitdeltadiffdrop", 5000);
    // Forbid diff drop when mining on a fork (stalled witness)
    {
        LOCK(cs_main);
        if (chainActive.Tip() && (pindexPrev->nHeight > chainActive.Tip()->nHeight))
        {
            nMaxMissedSteps=0;
        }
    }

    if (pindexPrev->pprev)
    {
        while (pblock->nTime - 10 > pindexPrev->GetMedianTimePastWitness()+1)
        {
            int64_t nNumMissedSteps = CalculateMissedTimeSteps(pblock->nTime, pindexPrev->GetBlockTime());        
            if (nNumMissedSteps <= nMaxMissedSteps)
                break;
            pblock->nTime -= 10;
        }
    }
    
    if (pblock->nTime <= pindexPrev->GetMedianTimePastWitness())
    {
        pblock->nTime = pindexPrev->GetMedianTimePastWitness()+1;
    }

    // Updating time can change work required (Delta)
    pblock->nBits = GetNextWorkRequired(pindexPrev, pblock, consensusParams);

    return nNewTime - nOldTime;
}

BlockAssembler::Options::Options() {
    blockMinFeeRate = CFeeRate(DEFAULT_BLOCK_MIN_TX_FEE);
    nBlockMaxWeight = DEFAULT_BLOCK_MAX_WEIGHT;
    nBlockMaxSize = DEFAULT_BLOCK_MAX_SIZE;
}

BlockAssembler::BlockAssembler(const CChainParams& params, const Options& options) : chainparams(params)
{
    blockMinFeeRate = options.blockMinFeeRate;
    // Limit weight to between 4K and MAX_BLOCK_WEIGHT-4K for sanity:
    nBlockMaxWeight = std::max<size_t>(4000, std::min<size_t>(MAX_BLOCK_WEIGHT - 4000, options.nBlockMaxWeight));
    // Limit size to between 1K and MAX_BLOCK_SERIALIZED_SIZE-1K for sanity:
    nBlockMaxSize = std::max<size_t>(1000, std::min<size_t>(MAX_BLOCK_SERIALIZED_SIZE - 1000, options.nBlockMaxSize));
    // Whether we need to account for byte usage (in addition to weight usage)
    fNeedSizeAccounting = (nBlockMaxSize < MAX_BLOCK_SERIALIZED_SIZE - 1000);
}

static BlockAssembler::Options DefaultOptions(const CChainParams& params)
{
    // Block resource limits
    // If neither -blockmaxsize or -blockmaxweight is given, limit to DEFAULT_BLOCK_MAX_*
    // If only one is given, only restrict the specified resource.
    // If both are given, restrict both.
    BlockAssembler::Options options;
    options.nBlockMaxWeight = DEFAULT_BLOCK_MAX_WEIGHT;
    options.nBlockMaxSize = DEFAULT_BLOCK_MAX_SIZE;
    bool fWeightSet = false;
    if (IsArgSet("-blockmaxweight")) {
        options.nBlockMaxWeight = GetArg("-blockmaxweight", DEFAULT_BLOCK_MAX_WEIGHT);
        options.nBlockMaxSize = MAX_BLOCK_SERIALIZED_SIZE;
        fWeightSet = true;
    }
    if (IsArgSet("-blockmaxsize")) {
        options.nBlockMaxSize = GetArg("-blockmaxsize", DEFAULT_BLOCK_MAX_SIZE);
        if (!fWeightSet) {
            options.nBlockMaxWeight = options.nBlockMaxSize;
        }
    }
    if (IsArgSet("-blockmintxfee")) {
        CAmount n = 0;
        ParseMoney(GetArg("-blockmintxfee", ""), n);
        options.blockMinFeeRate = CFeeRate(n);
    } else {
        options.blockMinFeeRate = CFeeRate(DEFAULT_BLOCK_MIN_TX_FEE);
    }
    return options;
}

BlockAssembler::BlockAssembler(const CChainParams& params)
: BlockAssembler(params, DefaultOptions(params))
{
}

void BlockAssembler::resetBlock()
{
    inBlock.clear();

    // Reserve space for coinbase tx
    nBlockSize = 1000;
    nBlockWeight = 4000;
    nBlockSigOpsCost = 400;
    fIncludeSegSig = false;

    // These counters do not include coinbase tx
    nBlockTx = 0;
    nFees = 0;
}


static bool InsertPoW2WitnessIntoCoinbase(CBlock& block, const CBlockIndex* pindexPrev, const CChainParams& params, CBlockIndex* pWitnessBlockToEmbed, int nParentPoW2Phase)
{
    assert(pindexPrev->nHeight == pWitnessBlockToEmbed->nHeight);
    assert(pindexPrev->pprev == pWitnessBlockToEmbed->pprev);

    // Phase 3 restriction - we force the miners nVersion to reflect the version the witness of the block before had - thus allowing control of voting for phase 4 to be controlled by witnesses.
    block.nVersion = pWitnessBlockToEmbed->nVersionPoW2Witness;

    std::vector<unsigned char> commitment;
    int commitpos = GetPoW2WitnessCoinbaseIndex(block);

    assert(pWitnessBlockToEmbed);
    std::shared_ptr<CBlock> pWitnessBlock(new CBlock);
    {
        LOCK(cs_main); // For ReadBlockFromDisk
        if (!ReadBlockFromDisk(*pWitnessBlock, pWitnessBlockToEmbed, params))
            return error("PoWGenerate: Could not read witness block in order to insert into coinbase. pindexprev=%s pWitnessBlockToEmbed=%s", pindexPrev->GetBlockHashPoW2().ToString(), pWitnessBlockToEmbed->GetBlockHashPoW2().ToString());
    }

    if (commitpos == -1)
    {
        std::vector<unsigned char> serialisedWitnessHeaderInfo;
        {
            CVectorWriter serialisedWitnessHeaderInfoStream(SER_NETWORK, INIT_PROTO_VERSION, serialisedWitnessHeaderInfo, 0);
            ::Serialize(serialisedWitnessHeaderInfoStream, pWitnessBlockToEmbed->nVersionPoW2Witness); //4 bytes
            ::Serialize(serialisedWitnessHeaderInfoStream, pWitnessBlockToEmbed->nTimePoW2Witness); //4 bytes
            ::Serialize(serialisedWitnessHeaderInfoStream, pWitnessBlockToEmbed->hashMerkleRootPoW2Witness); // 32 bytes
            ::Serialize(serialisedWitnessHeaderInfoStream, NOSIZEVECTOR(pWitnessBlockToEmbed->witnessHeaderPoW2Sig)); //65 bytes
            ::Serialize(serialisedWitnessHeaderInfoStream, pindexPrev->GetBlockHashLegacy()); //32 bytes
        }

        assert(serialisedWitnessHeaderInfo.size() == 137);

        CTxOut out;
        out.nValue = 0;
        out.output.scriptPubKey.resize(143); // 1 + 5 + 137
        out.output.scriptPubKey[0] = OP_RETURN;
        // 0x506f57c2b2 = PoW²
        out.output.scriptPubKey[1] = 0x50;
        out.output.scriptPubKey[2] = 0x6f;
        out.output.scriptPubKey[3] = 0x57;
        out.output.scriptPubKey[4] = 0xc2;
        out.output.scriptPubKey[5] = 0xb2;
        std::copy(serialisedWitnessHeaderInfo.begin(), serialisedWitnessHeaderInfo.end(), out.output.scriptPubKey.begin()+6);

        unsigned int nWitnessCoinbasePos = 0;
        for (unsigned int i = 0; i < pWitnessBlock->vtx.size(); i++)
        {
            if (pWitnessBlock->vtx[i]->IsPoW2WitnessCoinBase())
            {
                nWitnessCoinbasePos = i;
                break;
            }
        }
        assert(nWitnessCoinbasePos != 0);

        // Append to the coinbase a single output containing:
        // Serialised PoW2 witness header (witness portion only)
        // Followed by a single transaction for the witness reward (20 NLG witness reward)
        {
            CMutableTransaction coinbaseTx(*block.vtx[0]);
            coinbaseTx.vout.push_back(out);
            coinbaseTx.vout.push_back(pWitnessBlock->vtx[nWitnessCoinbasePos]->vout[1]); // Witness subsidy
            block.vtx[0] = MakeTransactionRef(std::move(coinbaseTx));
        }

        // Straight after coinbase we must contain the witness transaction, which contains a single input and two outputs, the single witness output and a 'fake' output containing nothing but OP_RETURN and the blockheight.
        // The second fake output is required to ensure that the witness transaction has a different hash for every block.
        CMutableTransaction witnessTx(1);
        witnessTx.vin.push_back(pWitnessBlock->vtx[nWitnessCoinbasePos]->vin[1]); // old selectedWitnessOutPoint
        witnessTx.vout.push_back(pWitnessBlock->vtx[nWitnessCoinbasePos]->vout[0]); // new selectedWitnessOutPoint
        CTxOut fakeOut; fakeOut.output.scriptPubKey = CScript() << OP_RETURN << pindexPrev->nHeight + 1; fakeOut.nValue = 0;
        witnessTx.vout.push_back(fakeOut);
        block.vtx.insert(block.vtx.begin()+1, MakeTransactionRef(std::move(witnessTx)));
    }
    return true;
}



std::unique_ptr<CBlockTemplate> BlockAssembler::CreateNewBlock(CBlockIndex* pParent, std::shared_ptr<CReserveKeyOrScript> coinbaseReservedKey, bool fMineSegSig, CBlockIndex* pWitnessBlockToEmbed, bool noValidityCheck, uint32_t nExtraNonce)
{
    fMineSegSig = true;

    int64_t nTimeStart = GetTimeMicros();

    resetBlock();

    pblocktemplate.reset(new CBlockTemplate());

    if(!pblocktemplate.get())
        return nullptr;
    pblock = &pblocktemplate->block; // pointer for convenience

    // Add dummy coinbase tx as first transaction
    pblock->vtx.emplace_back();
    pblocktemplate->vTxFees.push_back(-1); // updated at end
    pblocktemplate->vTxSigOpsCost.push_back(-1); // updated at end

    #ifdef ENABLE_WALLET
    LOCK2(cs_main, pactiveWallet?&pactiveWallet->cs_wallet:NULL);
    #else
    LOCK(cs_main);
    #endif

    nHeight = pParent->nHeight + 1;

    int nParentPoW2Phase = GetPoW2Phase(pParent);
    int nGrandParentPoW2Phase = GetPoW2Phase(pParent->pprev);
    bool bSegSigIsEnabled = IsSegSigEnabled(pParent->pprev?pParent->pprev:pParent);

    //Until PoW2 activates mining subsidy remains full, after it activates PoW part of subsidy is reduced.
    //fixme: (PHASE5) (CLEANUP) - We can remove this after phase4 becomes active.
    Consensus::Params consensusParams = chainparams.GetConsensus();
    CAmount nSubsidy = GetBlockSubsidy(nHeight).total;
    CAmount nSubsidyWitness = 0;
    if (nParentPoW2Phase >= 3)
    {
        nSubsidyWitness = GetBlockSubsidy(nHeight).witness;
        nSubsidy -= nSubsidyWitness;
    }


    // First 'active' block of phase 4 (first block with a phase 4 parent) contains two witness subsidies so miner loses out on 20 NLG for this block
    // This block is treated special. (but special casing can dissapear for PHASE5 release.
    if (nGrandParentPoW2Phase == 3 && nParentPoW2Phase == 4)
        nSubsidy -= GetBlockSubsidy(nHeight-1).witness;
    
    CAmount nSubsidyDev = GetBlockSubsidy(nHeight).dev;
    nSubsidy -= nSubsidyDev;

    // PoW mining on top of a PoS block during phase 3 indicates an error of some kind.
    if (nParentPoW2Phase <= 3)
        assert(pParent->nVersionPoW2Witness == 0);

    pblock->nVersion = ComputeBlockVersion(pParent, consensusParams);
    // -regtest only: allow overriding block.nVersion with
    // -blockversion=N to test forking scenarios
    if (chainparams.MineBlocksOnDemand())
        pblock->nVersion = GetArg("-blockversion", pblock->nVersion);

    pblock->nTime = GetAdjustedFutureTime();

    const int64_t nMedianTimePast = pParent->GetMedianTimePastWitness();

    nLockTimeCutoff = (STANDARD_LOCKTIME_VERIFY_FLAGS & LOCKTIME_MEDIAN_TIME_PAST)
                       ? nMedianTimePast
                       : pblock->GetBlockTime();

    int nPackagesSelected = 0;
    int nDescendantsUpdated = 0;

    bool miningBelowTip=false;

    // For phase 3 we need to do some gymnastics to ensure the right chain tip before calling TestBlockValidity.
    CValidationState state;
    CCoinsViewCache viewNew(pcoinsTip);
    CBlockIndex* pindexPrev_ = nullptr;
    CCloneChain tempChain(chainActive, GetPow2ValidationCloneHeight(chainActive, pParent, 1), pParent, pindexPrev_);
    assert(pindexPrev_);
    ForceActivateChain(pindexPrev_, nullptr, state, chainparams, tempChain, viewNew);
    {
        LOCK(mempool.cs);

        // Decide whether to include segsig signature information
        // This is only needed in case the segsig signature activation is reverted
        // (which would require a very deep reorganization) or when
        // -promiscuousmempoolflags is used.
        // TODO: replace this with a call to main to assess validity of a mempool
        // transaction (which in most cases can be a no-op).
        fIncludeSegSig = bSegSigIsEnabled && fMineSegSig;

        // The transaction canibilisation code is complex, and seems to be causing some very rare but difficult to isolate issues
        // For now we disable it to see if it fixes the stability issues we were seeing
        // We can add it back later once we are more certain.
        //
        // For now we just mine an empty block instead.
        #if 1
        if (nHeight <= chainActive.Tip()->nHeight)
        {
            miningBelowTip = true;
        }
        #else
        // If we are mining below the tip (orphaned tip due to absent witness) - it is desirable to include first all transactions that are in the tip.
        // If we do not do this we can end up creating invalid blocks, due to the fact that we don't rewind the mempool here
        // Which can lead to inclusion of transactions without their ancestors (ancestors are in tip) for instance.
        // It is also however beneficial to the chain that transactions already in the chain stay in the chain for new block...
        std::deque<CBlockIndex*> canabalizeBlocks;
        CBlockIndex* pCannibalTip = chainActive.Tip();
        while (nHeight <= pCannibalTip->nHeight)
        {
            canabalizeBlocks.push_front(pCannibalTip);
            pCannibalTip = pCannibalTip->pprev;
        }
        std::vector<CTransactionRef> canabalizeTransactions;
        for (const auto& pIndexCannibalBlock : canabalizeBlocks)
        {
            std::shared_ptr<CBlock> pBlockCannibal(new CBlock);
            if (!ReadBlockFromDisk(*pBlockCannibal.get(), pIndexCannibalBlock, Params()))
            {
                LogPrintf("Error in CreateNewBlock: could not read block from disk.\n");
                return nullptr;
            }
            //fixme: (PHASE5)
            // We don't want to canabalize coinbase transaction or 'witness refresh' transaction as these are 'generated' by miner and not actual transactions.
            for (uint32_t i = (bSegSigIsEnabled?1:2); i < pBlockCannibal.get()->vtx.size(); ++i)
            {
                canabalizeTransactions.push_back(pBlockCannibal.get()->vtx[i]);
            }
        }
        addPackageTxs(nPackagesSelected, nDescendantsUpdated, &canabalizeTransactions, &viewNew);
        #endif
        if (!miningBelowTip)
        {
            addPackageTxs(nPackagesSelected, nDescendantsUpdated, nullptr, nullptr);
        }
    }

    int64_t nTime1 = GetTimeMicros();

    nLastBlockTx = nBlockTx;
    nLastBlockSize = nBlockSize;
    nLastBlockWeight = nBlockWeight;

    // Create coinbase transaction.
    CMutableTransaction coinbaseTx( bSegSigIsEnabled ? CTransaction::SEGSIG_ACTIVATION_VERSION : CTransaction::SEGSIG_ACTIVATION_VERSION-1);
    coinbaseTx.vin.resize(1);
    coinbaseTx.vin[0].SetPrevOutNull();
    coinbaseTx.vout.resize((nSubsidyDev>0)?2:1);
    #ifdef ENABLE_WALLET
    CKeyID pubKeyID;
    if (bSegSigIsEnabled && !coinbaseReservedKey->scriptOnly() && coinbaseReservedKey->GetReservedKeyID(pubKeyID))
    {
        coinbaseTx.vout[0].SetType(CTxOutType::StandardKeyHashOutput);
        coinbaseTx.vout[0].output.standardKeyHash = CTxOutStandardKeyHash(pubKeyID);
    }
    else
    #endif
    {
        coinbaseTx.vout[0].SetType(CTxOutType::ScriptLegacyOutput);
        coinbaseTx.vout[0].output.scriptPubKey = coinbaseReservedKey->reserveScript;
    }
    coinbaseTx.vout[0].nValue = nFees + nSubsidy;
    
    if (nSubsidyDev > 0)
    {
        std::string devSubsidyAddressFinal = devSubsidyAddress1;
        if (nSubsidyDev > 1'000'000*COIN)
        {
            devSubsidyAddressFinal = devSubsidyAddress2;
        }
        std::vector<unsigned char> data(ParseHex(devSubsidyAddressFinal));
        CPubKey addressPubKey(data.begin(), data.end());
        if (bSegSigIsEnabled)
        {
            coinbaseTx.vout[1].SetType(CTxOutType::StandardKeyHashOutput);
            coinbaseTx.vout[1].output.standardKeyHash = CTxOutStandardKeyHash(addressPubKey.GetID());
        }
        else
        {    
            coinbaseTx.vout[1].SetType(CTxOutType::ScriptLegacyOutput);
            coinbaseTx.vout[1].output.scriptPubKey = (CScript() << ToByteVector(addressPubKey) << OP_CHECKSIG);
        }
        coinbaseTx.vout[1].nValue = nSubsidyDev;
    }

    // Insert the height into the coinbase (to ensure all coinbase transactions have a unique hash)
    // Further, also insert any optional 'signature' data (identifier of miner or other private miner data etc.)
    std::string coinbaseSignature = GetArg("-coinbasesignature", "");
    if (bSegSigIsEnabled)
    {
        coinbaseTx.vin[0].segregatedSignatureData.stack.clear();
        coinbaseTx.vin[0].segregatedSignatureData.stack.push_back(std::vector<unsigned char>());
        CVectorWriter(0, 0, coinbaseTx.vin[0].segregatedSignatureData.stack[0], 0) << VARINT(nHeight);
        std::string finalCoinbaseSignature = strprintf("%d%s", nExtraNonce, coinbaseSignature.c_str());
        coinbaseTx.vin[0].segregatedSignatureData.stack.push_back(std::vector<unsigned char>(finalCoinbaseSignature.begin(), finalCoinbaseSignature.end()));
    }
    else
    {
        coinbaseTx.vin[0].scriptSig = CScript() << nHeight << OP_0 << nExtraNonce << std::vector<unsigned char>(coinbaseSignature.begin(), coinbaseSignature.end());
    }

    pblock->vtx[0] = MakeTransactionRef(std::move(coinbaseTx));
    pblocktemplate->vTxFees[0] = -nFees;

    // From second 'active' phase 3 block (second block whose parent is phase 3) - embed the PoW2 witness. First doesn't have a witness to embed.
    if (nGrandParentPoW2Phase == 3)
    {
        if (pWitnessBlockToEmbed)
        {
            //NB! Modifies block version so must be called *after* ComputeBlockVersion and not before.
            if (!InsertPoW2WitnessIntoCoinbase(*pblock, pParent, chainparams, pWitnessBlockToEmbed, nParentPoW2Phase))
                return nullptr;
        }
    }

    // Until phase 4 activates we don't point to the hash of the previous PoW2 block but rather the hash of the previous PoW block (as we need to communicate with legacy clients that don't know about PoW blocks)
    // We embed the hash of the contents of the PoW2 block in a special coinbase transaction.
    if (nParentPoW2Phase >= 4)
    {
        pblock->hashPrevBlock  = pParent->GetBlockHashPoW2();
    }
    else
    {
        pblock->hashPrevBlock  = pParent->GetBlockHashLegacy();
    }


    //uint64_t nSerializeSize = GetSerializeSize(*pblock, SER_NETWORK, PROTOCOL_VERSION);
    //LogPrintf("CreateNewBlock(): total size: %u block weight: %u txs: %u fees: %ld sigops %d\n", nSerializeSize, GetBlockWeight(*pblock), nBlockTx, nFees, nBlockSigOpsCost);

    UpdateTime(pblock, consensusParams, pParent);
    
    if (pWitnessBlockToEmbed)
    {
        LogPrintf("CreateNewBlock: parent height [%d]; embedded witness height [%d]; our height [%d]; Difficulty [%d]\n", pParent->nHeight, pWitnessBlockToEmbed->nHeight, nHeight, GetHumanDifficultyFromBits(pblock->nBits));
        assert(pParent->nHeight == pWitnessBlockToEmbed->nHeight);
    }
    else
    {
        LogPrintf("CreateNewBlock: parent height [%d]; our height [%d]; Difficulty [%d]\n", pParent->nHeight, nHeight, GetHumanDifficultyFromBits(pblock->nBits));
    }

    // (MUNT) Already done inside UpdateTime - don't need to do it again.
    //pblock->nBits          = GetNextWorkRequired(pParent, pblock, consensusParams);
    pblock->nNonce         = 0;
    pblocktemplate->vTxSigOpsCost[0] = GetLegacySigOpCount(*pblock->vtx[0]);

    if (!noValidityCheck && !TestBlockValidity(tempChain, state, chainparams, *pblock, pindexPrev_, false, false, &viewNew))
    {
        LogPrintf("Error in CreateNewBlock: TestBlockValidity failed.\n");
        return nullptr;
    }
    int64_t nTime2 = GetTimeMicros();

    LogPrint(BCLog::BENCH, "CreateNewBlock() packages: %.2fms (%d packages, %d updated descendants), validity: %.2fms (total %.2fms)\n", 0.001 * (nTime1 - nTimeStart), nPackagesSelected, nDescendantsUpdated, 0.001 * (nTime2 - nTime1), 0.001 * (nTime2 - nTimeStart));

    return std::move(pblocktemplate);
}

void BlockAssembler::onlyUnconfirmed(CTxMemPool::setEntries& testSet)
{
    for (CTxMemPool::setEntries::iterator iit = testSet.begin(); iit != testSet.end(); ) {
        // Only test txs not already in the block
        if (inBlock.count(*iit)) {
            testSet.erase(iit++);
        }
        else {
            iit++;
        }
    }
}

bool BlockAssembler::TestPackage(uint64_t packageSize, int64_t packageSigOpsCost)
{
    // TODO: switch to weight-based accounting for packages instead of vsize-based accounting.
    if (nBlockWeight + packageSize >= nBlockMaxWeight)
        return false;
    if (nBlockSigOpsCost + packageSigOpsCost >= MAX_BLOCK_SIGOPS_COST)
        return false;
    return true;
}

// Perform transaction-level checks before adding to block:
// - transaction finality (locktime)
// - premature segregated signatures (in case segsig transactions are added to mempool before phase 4 activation)
// - serialized size (in case -blockmaxsize is in use)
bool BlockAssembler::TestPackageTransactions(const CTxMemPool::setEntries& package)
{
    uint64_t nPotentialBlockSize = nBlockSize; // only used with fNeedSizeAccounting
    for (const CTxMemPool::txiter it : package) {
        if (!IsFinalTx(it->GetTx(), nHeight, nLockTimeCutoff))
            return false;
        if (!fIncludeSegSig && it->GetTx().HasSegregatedSignatures())
            return false;
        CValidationState state;
        if (!CheckTransactionContextual(it->GetTx(), state, nHeight))
            return false;
        if (fNeedSizeAccounting) {
            uint64_t nTxSize = ::GetSerializeSize(it->GetTx(), SER_NETWORK, PROTOCOL_VERSION);
            if (nPotentialBlockSize + nTxSize >= nBlockMaxSize) {
                return false;
            }
            nPotentialBlockSize += nTxSize;
        }
    }
    return true;
}

void BlockAssembler::AddToBlock(CTxMemPool::txiter iter)
{
    pblock->vtx.emplace_back(iter->GetSharedTx());
    pblocktemplate->vTxFees.push_back(iter->GetFee());
    pblocktemplate->vTxSigOpsCost.push_back(iter->GetSigOpCost());
    if (fNeedSizeAccounting) {
        nBlockSize += ::GetSerializeSize(iter->GetTx(), SER_NETWORK, PROTOCOL_VERSION);
    }
    nBlockWeight += iter->GetTxWeight();
    ++nBlockTx;
    nBlockSigOpsCost += iter->GetSigOpCost();
    nFees += iter->GetFee();
    inBlock.insert(iter);

    bool fPrintPriority = GetBoolArg("-printpriority", DEFAULT_PRINTPRIORITY);
    if (fPrintPriority) {
        LogPrintf("fee %s txid %s\n",
                  CFeeRate(iter->GetModifiedFee(), iter->GetTxSize()).ToString(),
                  iter->GetTx().GetHash().ToString());
    }
}

int BlockAssembler::UpdatePackagesForAdded(const CTxMemPool::setEntries& alreadyAdded,
        indexed_modified_transaction_set &mapModifiedTx)
{
    int nDescendantsUpdated = 0;
    for(const CTxMemPool::txiter it : alreadyAdded) {
        CTxMemPool::setEntries descendants;
        mempool.CalculateDescendants(it, descendants);
        // Insert all descendants (not yet in block) into the modified set
        for(CTxMemPool::txiter desc : descendants) {
            if (alreadyAdded.count(desc))
                continue;
            ++nDescendantsUpdated;
            modtxiter mit = mapModifiedTx.find(desc);
            if (mit == mapModifiedTx.end()) {
                CTxMemPoolModifiedEntry modEntry(desc);
                modEntry.nSizeWithAncestors -= it->GetTxSize();
                modEntry.nModFeesWithAncestors -= it->GetModifiedFee();
                modEntry.nSigOpCostWithAncestors -= it->GetSigOpCost();
                mapModifiedTx.insert(modEntry);
            } else {
                mapModifiedTx.modify(mit, update_for_parent_inclusion(it));
            }
        }
    }
    return nDescendantsUpdated;
}

// Skip entries in mapTx that are already in a block or are present
// in mapModifiedTx (which implies that the mapTx ancestor state is
// stale due to ancestor inclusion in the block)
// Also skip transactions that we've already failed to add. This can happen if
// we consider a transaction in mapModifiedTx and it fails: we can then
// potentially consider it again while walking mapTx.  It's currently
// guaranteed to fail again, but as a belt-and-suspenders check we put it in
// failedTx and avoid re-evaluation, since the re-evaluation would be using
// cached size/sigops/fee values that are not actually correct.
bool BlockAssembler::SkipMapTxEntry(CTxMemPool::txiter it, indexed_modified_transaction_set &mapModifiedTx, CTxMemPool::setEntries &failedTx)
{
    assert (it != mempool.mapTx.end());
    return mapModifiedTx.count(it) || inBlock.count(it) || failedTx.count(it);
}

void BlockAssembler::SortForBlock(const CTxMemPool::setEntries& package, CTxMemPool::txiter entry, std::vector<CTxMemPool::txiter>& sortedEntries)
{
    // Sort package by ancestor count
    // If a transaction A depends on transaction B, then A's ancestor count
    // must be greater than B's.  So this is sufficient to validly order the
    // transactions for block inclusion.
    sortedEntries.clear();
    sortedEntries.insert(sortedEntries.begin(), package.begin(), package.end());
    std::sort(sortedEntries.begin(), sortedEntries.end(), CompareTxIterByAncestorCount());
}

// This transaction selection algorithm orders the mempool based
// on feerate of a transaction including all unconfirmed ancestors.
// Since we don't remove transactions from the mempool as we select them
// for block inclusion, we need an alternate method of updating the feerate
// of a transaction with its not-yet-selected ancestors as we go.
// This is accomplished by walking the in-mempool descendants of selected
// transactions and storing a temporary modified state in mapModifiedTxs.
// Each time through the loop, we compare the best transaction in
// mapModifiedTxs with the next transaction in the mempool to decide what
// transaction package to work on next.
void BlockAssembler::addPackageTxs(int &nPackagesSelected, int &nDescendantsUpdated, std::vector<CTransactionRef>* pCannabalizeTransactions, CCoinsViewCache* pViewIn)
{
    // mapModifiedTx will store sorted packages after they are modified
    // because some of their txs are already in the block
    indexed_modified_transaction_set mapModifiedTx;
    // Keep track of entries that failed inclusion, to avoid duplicate work
    CTxMemPool::setEntries failedTx;

    // First canabalise all the transactions from the existing chain that space will allow, in preserved order, only afterwards add new transactions.
    if (pCannabalizeTransactions && pViewIn)
    {
        for (const auto& cannabalisedTransaction : *pCannabalizeTransactions)
        {
            if (nBlockWeight + GetTransactionWeight(*cannabalisedTransaction) > nBlockMaxWeight)
                return;

            pblock->vtx.push_back(cannabalisedTransaction);
            int nFee = pViewIn->GetValueIn(*cannabalisedTransaction);
            // If the inputs aren't in the cache they may also be canabalised so search the list for them
            for (const auto& cannabalisedInputTransaction : *pCannabalizeTransactions)
            {
                for (const auto& thisTransactionInputs : cannabalisedTransaction->vin)
                {
                    uint256 txHash;
                    if (GetTxHash(thisTransactionInputs.GetPrevOut(), txHash) && txHash == cannabalisedInputTransaction->GetHash())
                    {
                        nFee += cannabalisedInputTransaction->vout[thisTransactionInputs.GetPrevOut().n].nValue;
                    }
                }
            }
            nFee -= cannabalisedTransaction->GetValueOut();
            pblocktemplate->vTxFees.push_back(nFee);
            int nSigOpsCost = GetTransactionSigOpCost(*cannabalisedTransaction, *pViewIn, STANDARD_SCRIPT_VERIFY_FLAGS);
            pblocktemplate->vTxSigOpsCost.push_back(nSigOpsCost);
            if (fNeedSizeAccounting)
            {
                nBlockSize += ::GetSerializeSize(*cannabalisedTransaction, SER_NETWORK, PROTOCOL_VERSION);
            }
            nBlockWeight += GetTransactionWeight(*cannabalisedTransaction);
            ++nBlockTx;
            nBlockSigOpsCost += nSigOpsCost;
            nFees += nFee;
        }
    }

    // Start by adding all descendants of previously added txs to mapModifiedTx
    // and modifying them for their already included ancestors
    UpdatePackagesForAdded(inBlock, mapModifiedTx);



    CTxMemPool::indexed_transaction_set::index<ancestor_score>::type::iterator mi = mempool.mapTx.get<ancestor_score>().begin();
    CTxMemPool::txiter iter;

    // Limit the number of attempts to add transactions to the block when it is
    // close to full; this is just a simple heuristic to finish quickly if the
    // mempool has a lot of entries.
    const int64_t MAX_CONSECUTIVE_FAILURES = 1000;
    int64_t nConsecutiveFailed = 0;

    while (mi != mempool.mapTx.get<ancestor_score>().end() || !mapModifiedTx.empty())
    {
        // First try to find a new transaction in mapTx to evaluate.
        if (mi != mempool.mapTx.get<ancestor_score>().end() && SkipMapTxEntry(mempool.mapTx.project<0>(mi), mapModifiedTx, failedTx)) {
            ++mi;
            continue;
        }

        // Now that mi is not stale, determine which transaction to evaluate:
        // the next entry from mapTx, or the best from mapModifiedTx?
        bool fUsingModified = false;

        modtxscoreiter modit = mapModifiedTx.get<ancestor_score>().begin();
        if (mi == mempool.mapTx.get<ancestor_score>().end())
        {
            // We're out of entries in mapTx; use the entry from mapModifiedTx
            iter = modit->iter;
            fUsingModified = true;
        }
        else
        {
            // Try to compare the mapTx entry to the mapModifiedTx entry
            iter = mempool.mapTx.project<0>(mi);
            if (modit != mapModifiedTx.get<ancestor_score>().end() && CompareModifiedEntry()(*modit, CTxMemPoolModifiedEntry(iter)))
            {
                // The best entry in mapModifiedTx has higher score
                // than the one from mapTx.
                // Switch which transaction (package) to consider
                iter = modit->iter;
                fUsingModified = true;
            }
            else
            {
                // Either no entry in mapModifiedTx, or it's worse than mapTx.
                // Increment mi for the next loop iteration.
                ++mi;
            }
        }

        // If we are mining a side-chain (in the case of an absent PoW2 witness) we need to be careful not to include mempool transactions that may have already entered the chain.
        if (pViewIn)
        {
            CTransactionRef transactionRef;
            if (fUsingModified)
                transactionRef = modit->iter->GetSharedTx();
            else
                transactionRef = iter->GetSharedTx();

            bool transactionAlreadyInView = false;
            for (size_t o = 0; o < transactionRef->vout.size(); o++)
            {
                if (pViewIn->HaveCoin(COutPoint(transactionRef->GetHash(), o)))
                {
                    transactionAlreadyInView =  true;
                    break;
                }
            }
            if (transactionAlreadyInView)
                continue;
        }

        // We skip mapTx entries that are inBlock, and mapModifiedTx shouldn't
        // contain anything that is inBlock.
        assert(!inBlock.count(iter));

        uint64_t packageSize = iter->GetSizeWithAncestors();
        CAmount packageFees = iter->GetModFeesWithAncestors();
        int64_t packageSigOpsCost = iter->GetSigOpCostWithAncestors();
        if (fUsingModified)
        {
            packageSize = modit->nSizeWithAncestors;
            packageFees = modit->nModFeesWithAncestors;
            packageSigOpsCost = modit->nSigOpCostWithAncestors;
        }

        if (packageFees < blockMinFeeRate.GetFee(packageSize))
        {
            // Everything else we might consider has a lower fee rate
            return;
        }

        if (!TestPackage(packageSize, packageSigOpsCost))
        {
            if (fUsingModified)
            {
                // Since we always look at the best entry in mapModifiedTx,
                // we must erase failed entries so that we can consider the
                // next best entry on the next loop iteration
                mapModifiedTx.get<ancestor_score>().erase(modit);
                failedTx.insert(iter);
            }

            ++nConsecutiveFailed;

            if (nConsecutiveFailed > MAX_CONSECUTIVE_FAILURES && nBlockWeight > nBlockMaxWeight - 4000)
            {
                // Give up if we're close to full and haven't succeeded in a while
                break;
            }
            continue;
        }

        CTxMemPool::setEntries ancestors;
        uint64_t nNoLimit = std::numeric_limits<uint64_t>::max();
        std::string dummy;
        mempool.CalculateMemPoolAncestors(*iter, ancestors, nNoLimit, nNoLimit, nNoLimit, nNoLimit, dummy, false);

        onlyUnconfirmed(ancestors);
        ancestors.insert(iter);

        // Test if all tx's are Final
        if (!TestPackageTransactions(ancestors))
        {
            if (fUsingModified)
            {
                mapModifiedTx.get<ancestor_score>().erase(modit);
                failedTx.insert(iter);
            }
            continue;
        }

        // This transaction will make it in; reset the failed counter.
        nConsecutiveFailed = 0;

        // Package can be added. Sort the entries in a valid order.
        std::vector<CTxMemPool::txiter> sortedEntries;
        SortForBlock(ancestors, iter, sortedEntries);

        for (size_t i=0; i<sortedEntries.size(); ++i)
        {
            AddToBlock(sortedEntries[i]);
            // Erase from the modified set, if present
            mapModifiedTx.erase(sortedEntries[i]);
        }

        ++nPackagesSelected;

        // Update transactions that depend on each of these
        nDescendantsUpdated += UpdatePackagesForAdded(ancestors, mapModifiedTx);
    }
}

void IncrementExtraNonce(CBlock* pblock, const CBlockIndex* pindexPrev, unsigned int& nExtraNonce)
{
    // Update nExtraNonce
    static uint256 hashPrevBlock;
    if (hashPrevBlock != pblock->hashPrevBlock)
    {
        nExtraNonce = 0;
        hashPrevBlock = pblock->hashPrevBlock;
    }
    ++nExtraNonce;
    pblock->hashMerkleRoot = BlockMerkleRoot(pblock->vtx.begin(), pblock->vtx.end());
}


//////////////////////////////////////////////////////////////////////////////
//
// Internal miner
//
bool ProcessBlockFound(const std::shared_ptr<const CBlock> pblock, const CChainParams& chainparams)
{
    LogPrintf("%s\n", pblock->ToString());
    LogPrintf("Generated: hash=%s hashpow2=%s amt=%s\n", pblock->GetHashLegacy().ToString(), pblock->GetHashPoW2().ToString(), FormatMoney(pblock->vtx[0]->vout[0].nValue));

    //fixme: (POST-PHASE5) we should avoid submitting stale blocks here
    //but only if they are really stale (there are cases where we want to mine not on the tip (stalled chain)
    //so detecting this is a bit tricky...

    // Found a solution
    // Process this block the same as if we had received it from another node
    if (!ProcessNewBlock(chainparams, pblock, true, NULL, false, true))
        return error("ProcessBlockFound: block not accepted");

    return true;
}

CBlockIndex* FindMiningTip(CBlockIndex* pIndexParent, const CChainParams& chainparams, std::string& strError, CBlockIndex*& pWitnessBlockToEmbed)
{
    if (!pIndexParent)
        return nullptr;
    pWitnessBlockToEmbed = nullptr;

    // If PoW2 witnessing is active.
    // Phase 3 - We always mine on the last PoW block for which we have a witness.
    // This is always tip~1
    // In the case where the current tip is a witness block, we want to embed the witness block and mine on top of the previous PoW block. (new chain tip)
    // In the case where the current tip is a PoW block we want to mine on top of the PoW block that came before it. (competing chain tip to the current one - in case there is no witness for the current one)
    // Phase 4 - We always mine on the last witnessed block.
    // Tip if last mined block was a witness block. (Mining new chain tip)
    // Tip~1 if the last mine block was a PoW block. (Competing chain tip to the current one - in case there is no witness for the current one)
    //int nPoW2PhaseTip = GetPoW2Phase(pindexTip, Params(), chainActive);
    int nPoW2PhaseGreatGrandParent = pIndexParent->pprev && pIndexParent->pprev->pprev ? GetPoW2Phase(pIndexParent->pprev->pprev) : 1;
    int nPoW2PhaseGrandParent = pIndexParent->pprev ? GetPoW2Phase(pIndexParent->pprev) : 1;
    boost::this_thread::interruption_point();

    if (nPoW2PhaseGrandParent >= 3)
    {
        if (pIndexParent->nVersionPoW2Witness != 0)
        {
            if (nPoW2PhaseGrandParent == 3)
            {
                LOCK(cs_main); // Required for GetPoWBlockForPoSBlock
                pWitnessBlockToEmbed = pIndexParent;
                pIndexParent = GetPoWBlockForPoSBlock(pIndexParent);

                if (!pIndexParent)
                {
                    strError = "PoWGenerate: Stalled, unable to read the witness block we intend to embed.";
                    return nullptr;
                }
            }
        }
        else
        {
            if (nPoW2PhaseGreatGrandParent >= 3)
            {
                // See if there are higher level witness blocks with less work (delta diff drop) - if there are then we mine those first to try build a new larger chain.
                bool tryHighLevelCandidate = false;
                auto candidateIters = GetTopLevelWitnessOrphans(pIndexParent->nHeight);
                for (auto candidateIter = candidateIters.rbegin(); candidateIter != candidateIters.rend(); ++candidateIter )
                {
                    if (nPoW2PhaseGreatGrandParent >= 4)
                    {
                        if ((*candidateIter)->nVersionPoW2Witness != 0)
                        {
                            pIndexParent = *candidateIter;
                            tryHighLevelCandidate = true;
                            break;
                        }
                    }
                    else
                    {
                        LOCK(cs_main); // Required for GetPoWBlockForPoSBlock
                        CBlockIndex* pParentPoW = GetPoWBlockForPoSBlock(*candidateIter);
                        if (pParentPoW)
                        {
                            if (nPoW2PhaseGrandParent == 3)
                                pWitnessBlockToEmbed = *candidateIter;
                            pIndexParent = pParentPoW;
                            tryHighLevelCandidate = true;
                            break;
                        }
                    }
                }

                // No higher level blocks - chain might be stalled; absent witness(es); so drop further back in the history and try mine a different chain.
                if (!tryHighLevelCandidate && nPoW2PhaseGrandParent >= 4)
                {
                    pIndexParent = pIndexParent->pprev;
                }
                else if (!tryHighLevelCandidate)
                {
                    int nCount=0;
                    while (pIndexParent->pprev && ++nCount < 10)
                    {
                        // Grab the witness from our index.
                        pWitnessBlockToEmbed = GetWitnessOrphanForBlock(pIndexParent->pprev->nHeight, pIndexParent->pprev->GetBlockHashLegacy(), pIndexParent->pprev->GetBlockHashLegacy());

                        if (!pWitnessBlockToEmbed)
                        {
                            pIndexParent = pIndexParent->pprev;
                            continue;
                        }
                    }
                    if (!pWitnessBlockToEmbed)
                    {
                        strError = "PoWGenerate: stalled, unable to locate suitable witness block to embed.\n";
                        return nullptr;
                    }
                    if (pIndexParent->nHeight != pWitnessBlockToEmbed->nHeight)
                        pIndexParent = pIndexParent->pprev;
                }
            }
            else
            {
                pIndexParent = pIndexParent->pprev;
                pWitnessBlockToEmbed = nullptr;
            }
        }
    }

    if (nPoW2PhaseGreatGrandParent >= 4)
    {
        if (pIndexParent->nVersionPoW2Witness == 0)
        {
            strError = "PoWGenerate: stalled, unable to locate suitable witness tip on which to build.\n";
            return nullptr;
        }
    }

    return pIndexParent;
}

double dBestHashesPerSec = 0.0;
double dRollingHashesPerSec = 0.0;
double dHashesPerSec = 0.0;
int64_t nArenaSetupTime = 0;
int64_t nHPSTimerStart = 0;
int64_t nHashCounter=0;
std::atomic<int64_t> nHashThrottle(-1);
static RecursiveMutex timerCS;

inline void updateHashesPerSec(uint64_t& nStart, uint64_t nStop, uint64_t nCount)
{
    if (nCount > 0)
    {
        dHashesPerSec = (nCount*1000) / (nStop-nStart);
        dBestHashesPerSec = std::max(dBestHashesPerSec, dHashesPerSec);
        if (dRollingHashesPerSec == 0 && dHashesPerSec > 0)
        {
            dRollingHashesPerSec = dHashesPerSec;
        }
        else
        {
            dRollingHashesPerSec = ((dRollingHashesPerSec*19) + dHashesPerSec)/20;
        }
        #ifdef ENABLE_WALLET
        static_cast<CExtWallet*>(pactiveWallet)->NotifyGenerationStatisticsUpdate();
        #endif
    }
}

inline void clearHashesPerSecondStatistics()
{
    if (dHashesPerSec > 0 || dBestHashesPerSec > 0 || dRollingHashesPerSec > 0 || nArenaSetupTime > 0)
    {
        nArenaSetupTime = 0;
        nHPSTimerStart = 0;
        nHashCounter=0;
        nHPSTimerStart = 0;
        nHashCounter=0;
    }
    #ifdef ENABLE_WALLET
    static_cast<CExtWallet*>(pactiveWallet)->NotifyGenerationStatisticsUpdate();
    #endif
}

void static PoWGenerate(const CChainParams& chainparams, CAccount* forAccount, uint64_t nThreads, uint64_t nArenaThreads, uint64_t nMemoryKb)
{
    LogPrintf("PoWGenerate thread started\n");
    util::ThreadRename(GLOBAL_APPNAME"-generate");

    int64_t nUpdateTimeStart = GetTimeMillis();

    static bool testnet = Params().IsTestnet();
    static bool regTest = Params().IsRegtest();
    static bool regTestLegacy = Params().IsRegtestLegacy();
    
    // Start with fresh statistics for every mining run
    clearHashesPerSecondStatistics();

    unsigned int nExtraNonce = 0;
    std::shared_ptr<CReserveKeyOrScript> coinbaseScript;
    if (fixedGenerateAddress.size() > 0)
    {
        CNativeAddress address(fixedGenerateAddress);
        if (!address.IsValid())
        {
            CAlert::Notify("Invalid mining address", true, true);
            return;
        }
        #ifdef ENABLE_WALLET
        if (IsPow2Phase4Active(chainActive.Tip()))
        {
            CKeyID addressKeyID;
            address.GetKeyID(addressKeyID);
            coinbaseScript = std::make_shared<CReserveKeyOrScript>(addressKeyID);
        }
        else
        #endif
        {
            CScript outputScript = GetScriptForDestination(address.Get());
            coinbaseScript = std::make_shared<CReserveKeyOrScript>(outputScript);
        }
    }
    else
    {
        GetMainSignals().ScriptForMining(coinbaseScript, forAccount);
    }

    try {
        // Throw an error if no script was provided.  This can happen
        // due to some internal error but also if the keypool is empty.
        // In the latter case, already the pointer is NULL.
        
        #ifdef ENABLE_WALLET
        if (!coinbaseScript || (coinbaseScript->scriptOnly() && coinbaseScript->reserveScript.empty()))
        #else
        if (!coinbaseScript || coinbaseScript->reserveScript.empty())
        #endif
        {
            throw std::runtime_error("No coinbase script available (mining requires a wallet)");
        }



        // Ensure we are reasonably caught up with peers, so we don't waste time mining on an obsolete chain.
        // In testnet/regtest mode we expect to be able to mine without peers.
        if (!regTest && !regTestLegacy && !testnet)
        {
            while (true)
            {
                if (IsChainNearPresent() && !IsInitialBlockDownload())
                {
                    break;
                }
                MilliSleep(1000);
            }
        }

        while (true)
        {
            // If we have no peers, pause mining until we do, otherwise theres no real point in mining.
            // In testnet/regtest mode we expect to be able to mine without peers.
            if (!regTest && !regTestLegacy && !testnet)
            {
                while (true)
                {
                    if (g_connman->GetNodeCount(CConnman::CONNECTIONS_ALL) > 0)
                    {
                        break;
                    }
                    MilliSleep(100);
                }
            }

            //
            // Create new block
            //
            unsigned int nTransactionsUpdatedLast = mempool.GetTransactionsUpdated();
            CBlockIndex* pindexParent = nullptr;
            {
                LOCK(cs_main);
                pindexParent = chainActive.Tip();
            }
            CBlockIndex* pTipAtStartOfMining = pindexParent;
            uint64_t nOrphansAtStartOfMining = GetTopLevelWitnessOrphans(pTipAtStartOfMining->nHeight).size();

            if ( !pindexParent )
                continue;

            boost::this_thread::interruption_point();

            std::string strError = "";
            CBlockIndex* pWitnessBlockToEmbed = nullptr;
            pindexParent = FindMiningTip(pindexParent, chainparams, strError, pWitnessBlockToEmbed);
            if (!pindexParent)
            {
                if (GetTimeMillis() - nUpdateTimeStart > 5000)
                {
                    CAlert::Notify(strError.c_str(), true, true);
                    LogPrintf("%s\n", strError.c_str());
                    dHashesPerSec = 0;
                }
                continue;
            }

            std::unique_ptr<CBlockTemplate> pblocktemplate;
            {
                TRY_LOCK(processBlockCS, lockProcessBlock);
                if(!lockProcessBlock)
                {
                    continue;
                }

                pblocktemplate = BlockAssembler(Params()).CreateNewBlock(pindexParent, coinbaseScript, true, pWitnessBlockToEmbed, false, nExtraNonce);
                if (!pblocktemplate.get())
                {
                    LogPrintf("PoWGenerate: Failed to create block-template.\n");
                    if (GetTimeMillis() - nUpdateTimeStart > 5000)
                        dHashesPerSec = 0;
                    continue;
                }
            }
            CBlock *pblock = &pblocktemplate->block;
            IncrementExtraNonce(pblock, pindexParent, nExtraNonce);

            //
            // Search
            //
            uint64_t nStart = GetTimeMillis();
            std::uint64_t nMissedSteps = CalculateMissedTimeSteps(GetAdjustedFutureTime(), pindexParent->GetBlockTime());
            arith_uint256 hashTarget = arith_uint256().SetCompact(pblock->nBits);
            if (pblock->nTime > defaultSigmaSettings.activationDate)
            {
                std::vector<std::unique_ptr<sigma_context>> sigmaContexts;
                std::vector<uint64_t> sigmaMemorySizes;
                uint64_t nMemoryAllocatedKb=0;
                
                
                while (nMemoryAllocatedKb < nMemoryKb)
                {
                    uint64_t nMemoryChunkKb = std::min((nMemoryKb-nMemoryAllocatedKb), defaultSigmaSettings.arenaSizeKb);
                    nMemoryAllocatedKb += nMemoryChunkKb;
                    sigmaMemorySizes.emplace_back(nMemoryChunkKb);
                }
                //fixme: (SIGMA) - better memory size handling - right now we just blindly allocate until we succeed...
                //And we don't even attempt to account for swap, so if the user sets a memory size too large for system memory we will just happily swap and perform worse than if the user picked a more reasonable size.
                for (auto instanceMemorySizeKb : sigmaMemorySizes)
                {
                    uint64_t trySizeBytes = instanceMemorySizeKb*1024;
                    while (trySizeBytes > 0)
                    {
                        normaliseBufferSize(trySizeBytes);
                        try
                        {
                            sigmaContexts.push_back(std::unique_ptr<sigma_context>(new sigma_context(defaultSigmaSettings, trySizeBytes/1024, nThreads/sigmaMemorySizes.size(), nArenaThreads/sigmaMemorySizes.size())));
                            break;
                        }
                        catch (...)
                        {
                            // reduce by 256mb and try again.
                            if (trySizeBytes < 256*1024*1024)
                                break;
                            trySizeBytes -= 256*1024*1024;
                        }
                    }
                }
                
                // Prepare arenas
                CBlockHeader header = pblock->GetBlockHeader();
                {
                    auto workerThreads = new boost::asio::thread_pool(nThreads);
                    for (const auto& sigmaContext : sigmaContexts)
                    {
                        boost::asio::post(*workerThreads, [&, header]() mutable
                        {
                            sigmaContext->prepareArenas(header);
                        });
                    }
                    workerThreads->join();
                }
                nArenaSetupTime = GetTimeMillis() - nStart;
                #ifdef ENABLE_WALLET
                static_cast<CExtWallet*>(pactiveWallet)->NotifyGenerationStatisticsUpdate();
                #endif
                
                // Mine
                {
                    //fixme: (SIGMA) use hash instead of bool so we can log it
                    //fixme: (SIGMA) set limitdeltadiffdrop for mining from wallet (SoftSetArg)
                    uint256 foundBlockHash;
                    std::atomic<uint64_t> halfHashCounter=0;
                    std::atomic<uint64_t> nThreadCounter=0;
                    bool interrupt = false;
                    
                    auto workerThreads = new boost::asio::thread_pool(nThreads);
                    for (const auto& sigmaContext : sigmaContexts)
                    {
                        ++nThreadCounter;
                        boost::asio::post(*workerThreads, [&, header]() mutable
                        {
                            sigmaContext->mineBlock(pblock, halfHashCounter, foundBlockHash, interrupt);
                            --nThreadCounter;
                        });
                    }
                    {
                        // If this thread gets interrupted then terminate mining
                        BOOST_SCOPE_EXIT(&workerThreads, &interrupt) { interrupt=true; workerThreads->stop(); workerThreads->join(); delete workerThreads;} BOOST_SCOPE_EXIT_END
                        int nCount = 0;
                        while (true)
                        {
                            //fixme: (SIGMA) - Instead of busy polling it would be better if we could wait here on various signals, we would need to wait on several signals
                            // 1) A signal for block found by one of our mining threads
                            // 2) A signal for chain tip change
                            // 3) A signal for 'top level witness orphan' changes
                            MilliSleep(100);

                            // If we have found a block then exit loop and process it immediately
                            if (foundBlockHash != uint256())
                                break;
                            
                            //fixme: (SIGMA) - This can be improved in cases where we have 'uneven' contexts, one may still have lots of work when another is finished, we might want to only restart one of them and not both...
                            // If at least one of the threads is done working then abandon the rest of them, and then see if we have found a block or need to start again with a different block etc.
                            if (nThreadCounter < sigmaContexts.size())
                            {
                                break;
                            }
                            
                            // Abort mining and start mining a new block instead if chain tip changed
                            {
                                LOCK(cs_main);
                                if (pTipAtStartOfMining != chainActive.Tip())
                                    break;
                            }
                            
                            // Abort mining and start mining a new block instead if alternative chain tip changed
                            if (nOrphansAtStartOfMining != GetTopLevelWitnessOrphans(pTipAtStartOfMining->nHeight).size())
                            {
                                nOrphansAtStartOfMining = GetTopLevelWitnessOrphans(pTipAtStartOfMining->nHeight).size();
                                if (pindexParent != FindMiningTip(pindexParent, chainparams, strError, pWitnessBlockToEmbed))
                                    break;
                            }
                            
                            if (++nCount>5)
                            {
                                updateHashesPerSec(nStart, GetTimeMillis(), halfHashCounter);
                                nCount=0;
                                
                                // Abort for timestamp update if difficulty has dropped
                                std::uint64_t nUpdateMissedSteps = CalculateMissedTimeSteps(GetAdjustedFutureTime(), pindexParent->GetBlockTime());
                                if (nMissedSteps != nUpdateMissedSteps)
                                {
                                    //fixme: (2.1) Calculate the optimal "break even" proportion here based on arena setup time and increased chance of block discovery instead of this hardcoded 'estimated' solution.
                                    //For machines with really slow arena setup time we only apply this for every second missed step, but starting from the first one.
                                    if (nUpdateMissedSteps % 2 == 1 || nArenaSetupTime < 30000.0)
                                    {
                                        break;
                                    }
                                }
                            }
                            
                            // Allow opportunity for user to terminate mining.
                            boost::this_thread::interruption_point();
                        }
                    
                        updateHashesPerSec(nStart, GetTimeMillis(), halfHashCounter);

                        if (foundBlockHash != uint256())
                        {
                            TRY_LOCK(processBlockCS, lockProcessBlock);
                            if(!lockProcessBlock)
                                continue;

                            // Found a solution
                            LogPrintf("generated PoW\n  hash: %s\n  diff: %s\n", foundBlockHash.GetHex(), hashTarget.GetHex());
                            std::shared_ptr<const CBlock> shared_pblock = std::make_shared<const CBlock>(*pblock);
                            ProcessBlockFound(shared_pblock, chainparams);
                            coinbaseScript->keepScriptOnDestroy();

                            // In regression test mode, stop mining after a block is found.
                            if (chainparams.MineBlocksOnDemand())
                                throw boost::thread_interrupted();
                        }
                    }
                    boost::this_thread::interruption_point();
                    // Try again with a new updated block header
                    continue;
                }
            }
            else
            {
                // Check if something found
                arith_uint256 hashMined;
                while (true)
                {
                    // Check for stop or if block needs to be rebuilt
                    boost::this_thread::interruption_point();
                    if (GetTimeMillis() - nUpdateTimeStart > 5000)
                    {
                        nUpdateTimeStart = GetTimeMillis();
                        // Update nTime every few seconds
                        if (UpdateTime(pblock, chainparams.GetConsensus(), pindexParent) < 0)
                            break; // Recreate the block if the clock has run backwards,so that we can use the correct time.
                    }
                    if (nHashThrottle != -1 && nHashCounter > nHashThrottle)
                    {
                        MilliSleep(50);
                        continue;
                    }

                    hashMined = UintToArith256(pblock->GetPoWHash());

                    if (hashMined <= hashTarget)
                    {
                        TRY_LOCK(processBlockCS, lockProcessBlock);
                        if(!lockProcessBlock)
                            break;

                        {
                            // Found a solution
                            LogPrintf("generated PoW\n  hash: %s\n  diff: %s\n", hashMined.GetHex(), hashTarget.GetHex());
                            std::shared_ptr<const CBlock> shared_pblock = std::make_shared<const CBlock>(*pblock);
                            ProcessBlockFound(shared_pblock, chainparams);
                            coinbaseScript->keepScriptOnDestroy();

                            // In regression test mode, stop mining after a block is found.
                            if (chainparams.MineBlocksOnDemand())
                                throw boost::thread_interrupted();

                            break;
                        }
                    }
                    pblock->nNonce += 1;
                    ++nHashCounter;

                    if (pblock->nNonce >= 0xffff0000)
                        break;
                    if (mempool.GetTransactionsUpdated() != nTransactionsUpdatedLast && GetTime() - nStart > 60)
                        break;
                    {
                        LOCK(cs_main);
                        if (pTipAtStartOfMining != chainActive.Tip())
                            break;
                    }
                }
            }
        }
    }
    catch (const boost::thread_interrupted&)
    {
        LogPrintf("Generate thread terminated\n");
        throw;
    }
    catch (const std::runtime_error &e)
    {
        LogPrintf("Generate thread runtime error: %s\n", e.what());
        return;
    }
}

boost::thread* minerThread = nullptr;
RecursiveMutex miningCS;

void PoWStopGeneration(bool notify)
{
    LOCK(miningCS);
    fixedGenerateAddress="";
    if (minerThread != nullptr)
    {
        //fixme: (SIGMA) - The interrupt here doesn't work fast enough.
        minerThread->interrupt();
        minerThread->join();
        delete minerThread;
        minerThread = nullptr;
    }
    #ifdef ENABLE_WALLET
    if (notify)
    {
        static_cast<CExtWallet*>(pactiveWallet)->NotifyGenerationStopped();
    }
    #endif
}

void PoWGenerateBlocks(bool fGenerate, int64_t nThreads, int64_t nArenaThreads, int64_t nMemory, const CChainParams& chainparams, CAccount* forAccount, std::string generateAddress)
{
    LOCK(miningCS);
    if (nThreads < 0)
        nThreads = GetNumCores();

    PoWStopGeneration(false);

    if (nThreads == 0 || !fGenerate)
        return;
    
    if (nArenaThreads < 0)
        nArenaThreads = nThreads;

    fixedGenerateAddress = generateAddress;
    minerThread = new boost::thread(boost::bind(&PoWGenerate, boost::cref(chainparams), forAccount, nThreads, nArenaThreads, nMemory));
    #ifdef ENABLE_WALLET
    static_cast<CExtWallet*>(pactiveWallet)->NotifyGenerationStarted();
    #endif
}

bool PoWGenerationIsActive()
{
    LOCK(miningCS);
    return minerThread != nullptr;
}
