// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// File contains modifications by: The Gulden developers
// All modifications:
// Copyright (c) 2016-2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#include "miner.h"

#include "net.h"

#include "amount.h"
#include "chain.h"
#include "chainparams.h"
#include "coins.h"
#include "consensus/consensus.h"
#include "consensus/tx_verify.h"
#include "consensus/merkle.h"
#include "consensus/validation.h"
#include "Gulden/auto_checkpoints.h"
#include "hash.h"
#include "validation.h"
#include "witnessvalidation.h"
#include "net.h"
#include "policy/feerate.h"
#include "policy/policy.h"
#include "pow.h"
#include "primitives/transaction.h"
#include "script/standard.h"
#include "timedata.h"
#include "txmempool.h"
#include "util.h"
#include "utiltime.h"
#include "utilmoneystr.h"
#include "validationinterface.h"

#include <algorithm>
#include <queue>
#include <utility>

#include <Gulden/Common/diff.h>
#include <Gulden/Common/hash/hash.h>
#include <Gulden/util.h>
#include <openssl/sha.h>

#include <boost/thread.hpp>

#include "txdb.h"

//Gulden includes
#include "streams.h"

//////////////////////////////////////////////////////////////////////////////
//
// GuldenMiner
//

//
// Unconfirmed transactions in the memory pool often depend on other
// transactions in the memory pool. When we select transactions from the
// pool, we select by highest fee rate of a transaction combined with all
// its ancestors.

uint64_t nLastBlockTx = 0;
uint64_t nLastBlockSize = 0;
uint64_t nLastBlockWeight = 0;

int64_t UpdateTime(CBlockHeader* pblock, const Consensus::Params& consensusParams, const CBlockIndex* pindexPrev)
{
    int64_t nOldTime = pblock->nTime;
    int64_t nNewTime = std::max(pindexPrev->GetMedianTimePastWitness()+1, GetAdjustedTime());

    if (nOldTime < nNewTime)
        pblock->nTime = nNewTime;

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
            options.nBlockMaxWeight = options.nBlockMaxSize * WITNESS_SCALE_FACTOR;
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

BlockAssembler::BlockAssembler(const CChainParams& params) : BlockAssembler(params, DefaultOptions(params)) {}

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


bool InsertPoW2WitnessIntoCoinbase(CBlock& block, const CBlockIndex* pindexPrev, const Consensus::Params& consensusParams, CBlockIndex* pWitnessBlockToEmbed, int nParentPoW2Phase)
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
        if (!ReadBlockFromDisk(*pWitnessBlock, pWitnessBlockToEmbed, consensusParams))
            return error("GuldenMiner: Could not read witness block in order to insert into coinbase. pindexprev=%s pWitnessBlockToEmbed=%s", pindexPrev->GetBlockHashPoW2().ToString(), pWitnessBlockToEmbed->GetBlockHashPoW2().ToString());
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
        //testme: (HIGH) (PoW2) (2.0) - is an OP_PUSH required here?
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
        CMutableTransaction coinbaseTx(*block.vtx[0]);
        coinbaseTx.vout.push_back(out);
        coinbaseTx.vout.push_back(pWitnessBlock->vtx[nWitnessCoinbasePos]->vout[1]); // Witness subsidy
        block.vtx[0] = MakeTransactionRef(std::move(coinbaseTx));

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

std::unique_ptr<CBlockTemplate> BlockAssembler::CreateNewBlock(CBlockIndex* pParent, const CScript& scriptPubKeyIn, bool fMineSegSig, CBlockIndex* pWitnessBlockToEmbed, bool noValidityCheck)
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

    LOCK2(cs_main, pactiveWallet?&pactiveWallet->cs_wallet:NULL);
    LOCK(Checkpoints::cs_hashSyncCheckpoint); // prevents potential deadlock being reported from tests
    LOCK(mempool.cs);

    nHeight = pParent->nHeight + 1;

    int nParentPoW2Phase = GetPoW2Phase(pParent, chainparams, chainActive);
    int nGrandParentPoW2Phase = GetPoW2Phase(pParent->pprev, chainparams, chainActive);

    //Until PoW2 activates mining subsidy remains full, after it activates PoW part of subsidy is reduced.
    //fixme: (2.1) (CLEANUP) - We can remove this after 2.1 becomes active.
    Consensus::Params consensusParams = chainparams.GetConsensus();
    CAmount nSubsidy = GetBlockSubsidy(nHeight, consensusParams);
    if (nParentPoW2Phase >= 3)
        nSubsidy -= GetBlockSubsidyWitness(nHeight, consensusParams);


    // First 'active' block of phase 4 (first block with a phase 4 parent) contains two witness subsidies so miner loses out on 20 NLG for this block
    // This block is treated special. (but special casing can dissapear for 2.1 release.
    if (nGrandParentPoW2Phase == 3 && nParentPoW2Phase == 4)
        nSubsidy -= GetBlockSubsidyWitness(nHeight, consensusParams);

    // PoW mining on top of a PoS block during phase 3 indicates an error of some kind.
    if (nParentPoW2Phase <= 3)
        assert(pParent->nVersionPoW2Witness == 0);

    pblock->nVersion = ComputeBlockVersion(pParent, consensusParams);
    // -regtest only: allow overriding block.nVersion with
    // -blockversion=N to test forking scenarios
    if (chainparams.MineBlocksOnDemand())
        pblock->nVersion = GetArg("-blockversion", pblock->nVersion);

    pblock->nTime = GetAdjustedTime();
    const int64_t nMedianTimePast = pParent->GetMedianTimePastWitness();

    nLockTimeCutoff = (STANDARD_LOCKTIME_VERIFY_FLAGS & LOCKTIME_MEDIAN_TIME_PAST)
                       ? nMedianTimePast
                       : pblock->GetBlockTime();

    // Decide whether to include segsig signature information
    // This is only needed in case the segsig signature activation is reverted
    // (which would require a very deep reorganization) or when
    // -promiscuousmempoolflags is used.
    // TODO: replace this with a call to main to assess validity of a mempool
    // transaction (which in most cases can be a no-op).
    fIncludeSegSig = IsSegSigEnabled(pParent, chainparams, chainActive, nullptr) && fMineSegSig;

    int nPackagesSelected = 0;
    int nDescendantsUpdated = 0;
    addPackageTxs(nPackagesSelected, nDescendantsUpdated);

    int64_t nTime1 = GetTimeMicros();

    nLastBlockTx = nBlockTx;
    nLastBlockSize = nBlockSize;
    nLastBlockWeight = nBlockWeight;

    // Create coinbase transaction.
    CMutableTransaction coinbaseTx( nParentPoW2Phase >= 4 ? CTransaction::SEGSIG_ACTIVATION_VERSION : CTransaction::CURRENT_VERSION );
    coinbaseTx.vin.resize(1);
    coinbaseTx.vin[0].prevout.SetNull();
    coinbaseTx.vout.resize(1);
    //fixme: (2.0) (SEGSIG) - Change to other output types for phase 4 onward?
    coinbaseTx.vout[0].output.scriptPubKey = scriptPubKeyIn;
    coinbaseTx.vout[0].nValue = nFees + nSubsidy;

    // Insert the height into the coinbase (to ensure all coinbase transactions have a unique hash)
    // Further, also insert any optional 'signature' data (identifier of miner or other private miner data etc.)
    std::string coinbaseSignature = GetArg("-coinbasesignature", "");
    if (nParentPoW2Phase < 4)
    {
        coinbaseTx.vin[0].scriptSig = CScript() << nHeight << OP_0 << std::vector<unsigned char>(coinbaseSignature.begin(), coinbaseSignature.end());
    }
    else
    {
        coinbaseTx.vin[0].scriptWitness.stack.clear();
        coinbaseTx.vin[0].scriptWitness.stack.push_back(std::vector<unsigned char>());
        CVectorWriter(0, 0, coinbaseTx.vin[0].scriptWitness.stack[0], 0) << VARINT(nHeight);
        coinbaseTx.vin[0].scriptWitness.stack.push_back(std::vector<unsigned char>(coinbaseSignature.begin(), coinbaseSignature.end()));
    }

    pblock->vtx[0] = MakeTransactionRef(std::move(coinbaseTx));
    //fixme: (2.0) (SEGSIG)
    //pblocktemplate->vchCoinbaseCommitment = GenerateCoinbaseCommitment(*pblock, pindexPrev, chainparams.GetConsensus());
    pblocktemplate->vTxFees[0] = -nFees;

    // From second 'active' phase 3 block (second block whose parent is phase 3) - embed the PoW2 witness. First doesn't have a witness to embed.
    if (nGrandParentPoW2Phase == 3)
    {
        if (pWitnessBlockToEmbed)
        {
            //NB! Modifies block version so must be called *after* ComputeBlockVersion and not before.
            if (!InsertPoW2WitnessIntoCoinbase(*pblock, pParent, consensusParams, pWitnessBlockToEmbed, nParentPoW2Phase))
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
    // (GULDEN) Already done inside UpdateTime - don't need to do it again.
    //pblock->nBits          = GetNextWorkRequired(pParent, pblock, consensusParams);
    pblock->nNonce         = 0;
    pblocktemplate->vTxSigOpsCost[0] = WITNESS_SCALE_FACTOR * GetLegacySigOpCount(*pblock->vtx[0]);

    if (nParentPoW2Phase/*nGrandParentPoW2Phase*/ >= 3) // For phase 3 we need to do some gymnastics to ensure the right chain tip before calling TestBlockValidity.
    {
        CValidationState state;
        CCoinsViewCache viewNew(pcoinsTip);

        CBlockIndex* pindexPrev_ = nullptr;
        CCloneChain tempChain = chainActive.Clone(pParent, pindexPrev_);
        assert(pindexPrev_);
        ForceActivateChain(pindexPrev_, nullptr, state, chainparams, tempChain, viewNew);

        if (!noValidityCheck && !TestBlockValidity(tempChain, state, chainparams, *pblock, pindexPrev_, false, false, &viewNew))
        {
            LogPrintf("Error in CreateNewBlock: TestBlockValidity failed.\n");
            return nullptr;
        }
    }
    else
    {
        CValidationState state;
        if (!TestBlockValidity(chainActive, state, chainparams, *pblock, pParent, false, false, nullptr)) {
            throw std::runtime_error(strprintf("%s: TestBlockValidity failed: %s", __func__, FormatStateMessage(state)));
        }
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
    if (nBlockWeight + WITNESS_SCALE_FACTOR * packageSize >= nBlockMaxWeight)
        return false;
    if (nBlockSigOpsCost + packageSigOpsCost >= MAX_BLOCK_SIGOPS_COST)
        return false;
    return true;
}

// Perform transaction-level checks before adding to block:
// - transaction finality (locktime)
// - premature witness (in case segwit transactions are added to mempool before
//   segwit activation)
// - serialized size (in case -blockmaxsize is in use)
bool BlockAssembler::TestPackageTransactions(const CTxMemPool::setEntries& package)
{
    uint64_t nPotentialBlockSize = nBlockSize; // only used with fNeedSizeAccounting
    for (const CTxMemPool::txiter it : package) {
        if (!IsFinalTx(it->GetTx(), nHeight, nLockTimeCutoff))
            return false;
        if (!fIncludeSegSig && it->GetTx().HasWitness())
            return false;
        CValidationState state;
        if (!CheckTransactionContextual(it->GetTx(), state, nHeight, nullptr, true))
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
void BlockAssembler::addPackageTxs(int &nPackagesSelected, int &nDescendantsUpdated)
{
    // mapModifiedTx will store sorted packages after they are modified
    // because some of their txs are already in the block
    indexed_modified_transaction_set mapModifiedTx;
    // Keep track of entries that failed inclusion, to avoid duplicate work
    CTxMemPool::setEntries failedTx;

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
        if (mi != mempool.mapTx.get<ancestor_score>().end() &&
                SkipMapTxEntry(mempool.mapTx.project<0>(mi), mapModifiedTx, failedTx)) {
            ++mi;
            continue;
        }

        // Now that mi is not stale, determine which transaction to evaluate:
        // the next entry from mapTx, or the best from mapModifiedTx?
        bool fUsingModified = false;

        modtxscoreiter modit = mapModifiedTx.get<ancestor_score>().begin();
        if (mi == mempool.mapTx.get<ancestor_score>().end()) {
            // We're out of entries in mapTx; use the entry from mapModifiedTx
            iter = modit->iter;
            fUsingModified = true;
        } else {
            // Try to compare the mapTx entry to the mapModifiedTx entry
            iter = mempool.mapTx.project<0>(mi);
            if (modit != mapModifiedTx.get<ancestor_score>().end() &&
                    CompareModifiedEntry()(*modit, CTxMemPoolModifiedEntry(iter))) {
                // The best entry in mapModifiedTx has higher score
                // than the one from mapTx.
                // Switch which transaction (package) to consider
                iter = modit->iter;
                fUsingModified = true;
            } else {
                // Either no entry in mapModifiedTx, or it's worse than mapTx.
                // Increment mi for the next loop iteration.
                ++mi;
            }
        }

        // We skip mapTx entries that are inBlock, and mapModifiedTx shouldn't
        // contain anything that is inBlock.
        assert(!inBlock.count(iter));

        uint64_t packageSize = iter->GetSizeWithAncestors();
        CAmount packageFees = iter->GetModFeesWithAncestors();
        int64_t packageSigOpsCost = iter->GetSigOpCostWithAncestors();
        if (fUsingModified) {
            packageSize = modit->nSizeWithAncestors;
            packageFees = modit->nModFeesWithAncestors;
            packageSigOpsCost = modit->nSigOpCostWithAncestors;
        }

        if (packageFees < blockMinFeeRate.GetFee(packageSize)) {
            // Everything else we might consider has a lower fee rate
            return;
        }

        if (!TestPackage(packageSize, packageSigOpsCost)) {
            if (fUsingModified) {
                // Since we always look at the best entry in mapModifiedTx,
                // we must erase failed entries so that we can consider the
                // next best entry on the next loop iteration
                mapModifiedTx.get<ancestor_score>().erase(modit);
                failedTx.insert(iter);
            }

            ++nConsecutiveFailed;

            if (nConsecutiveFailed > MAX_CONSECUTIVE_FAILURES && nBlockWeight >
                    nBlockMaxWeight - 4000) {
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
        if (!TestPackageTransactions(ancestors)) {
            if (fUsingModified) {
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

        for (size_t i=0; i<sortedEntries.size(); ++i) {
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


static const unsigned int pSHA256InitState[8] =
{0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};

void SHA256Transform(void* pstate, void* pinput, const void* pinit)
{
    SHA256_CTX ctx;
    unsigned char data[64];

    SHA256_Init(&ctx);

    for (int i = 0; i < 16; i++)
        ((uint32_t*)data)[i] = ByteReverse(((uint32_t*)pinput)[i]);

    for (int i = 0; i < 8; i++)
        ctx.h[i] = ((uint32_t*)pinit)[i];

    SHA256_Update(&ctx, data, sizeof(data));
    for (int i = 0; i < 8; i++)
        ((uint32_t*)pstate)[i] = ctx.h[i];
}

int static FormatHashBlocks(void* pbuffer, unsigned int len)
{
    unsigned char* pdata = (unsigned char*)pbuffer;
    unsigned int blocks = 1 + ((len + 8) / 64);
    unsigned char* pend = pdata + 64 * blocks;
    memset(pdata + len, 0, 64 * blocks - len);
    pdata[len] = 0x80;
    unsigned int bits = len * 8;
    pend[-1] = (bits >> 0) & 0xff;
    pend[-2] = (bits >> 8) & 0xff;
    pend[-3] = (bits >> 16) & 0xff;
    pend[-4] = (bits >> 24) & 0xff;
    return blocks;
}

void FormatHashBuffers(CBlock* pblock, char* pmidstate, char* pdata, char* phash1)
{
    //
    // Pre-build hash buffers
    //
    struct
    {
        struct unnamed2
        {
            int nVersion;
            uint256 hashPrevBlock;
            uint256 hashMerkleRoot;
            unsigned int nTime;
            unsigned int nBits;
            unsigned int nNonce;
        }
        block;
        unsigned char pchPadding0[64];
        uint256 hash1;
        unsigned char pchPadding1[64];
    }
    tmp;
    memset(&tmp, 0, sizeof(tmp));

    tmp.block.nVersion       = pblock->nVersion;
    tmp.block.hashPrevBlock  = pblock->hashPrevBlock;
    tmp.block.hashMerkleRoot = pblock->hashMerkleRoot;
    tmp.block.nTime          = pblock->nTime;
    tmp.block.nBits          = pblock->nBits;
    tmp.block.nNonce         = pblock->nNonce;

    FormatHashBlocks(&tmp.block, sizeof(tmp.block));
    FormatHashBlocks(&tmp.hash1, sizeof(tmp.hash1));

    // Byte swap all the input buffer
    for (unsigned int i = 0; i < sizeof(tmp)/4; i++)
        ((unsigned int*)&tmp)[i] = ByteReverse(((unsigned int*)&tmp)[i]);

    // Precalc the first half of the first hash, which stays constant
    SHA256Transform(pmidstate, &tmp.block, pSHA256InitState);

    memcpy(pdata, &tmp.block, 128);
    memcpy(phash1, &tmp.hash1, 64);
}

bool ProcessBlockFound(const std::shared_ptr<const CBlock> pblock, const CChainParams& chainparams)
{
    LogPrintf("%s\n", pblock->ToString());
    LogPrintf("generated hash= %s hashpow2= %s  amt= %s [PoW2 phase: tip=%d tipprevious=%d]\n", pblock->GetPoWHash().ToString(), pblock->GetHashPoW2().ToString(), FormatMoney(pblock->vtx[0]->vout[0].nValue), GetPoW2Phase(chainActive.Tip(), chainparams, chainActive), GetPoW2Phase(chainActive.Tip()->pprev, chainparams, chainActive));

    //fixme: (2.1) re-implement this.
    // Found a solution
    /*
    CBlockIndex* pIndexPrev = chainActive.Tip();
    if (IsPow2Phase4Active(pIndexPrev->pprev, chainparams))
    {
        if (pIndexPrev->nVersionPoW2Witness == 0 ||  pblock->hashPrevBlock != pIndexPrev->GetBlockHashPoW2())
            return error("GuldenWitness: Generated phase4 block is stale");
    }
    else
    {
        if (pIndexPrev->nVersionPoW2Witness != 0 ||  pblock->hashPrevBlock != pIndexPrev->GetBlockHashPoW2())
            return error("GuldenWitness: Generated block is stale");
    }*/


    // Process this block the same as if we had received it from another node
    if (!ProcessNewBlock(chainparams, pblock, true, NULL))
        return error("GuldenMiner: ProcessNewBlock, block not accepted");

    return true;
}

double dBestHashesPerSec = 0.0;
double dHashesPerSec = 0.0;
int64_t nHPSTimerStart = 0;
int64_t nHashCounter=0;
std::atomic<int64_t> nHashThrottle(-1);
static CCriticalSection timerCS;
static CCriticalSection processBlockCS;

struct CBlockIndexCacheComparator
{
    bool operator()(CBlockIndex *pa, CBlockIndex *pb) const {
        if (pa->nHeight > pb->nHeight) return false;
        if (pa->nHeight < pb->nHeight) return true;

        if (pa < pb) return false;
        if (pa > pb) return true;

        return false;
    }
};

static const unsigned int hashPSTimerInterval = 200;
void static GuldenMiner(const CChainParams& chainparams)
{
    LogPrintf("GuldenMiner started\n");
    RenameThread("gulden-miner");

    int64_t nUpdateTimeStart = GetTimeMillis();

    static bool hashCity = IsArgSet("-testnet") ? ( GetArg("-testnet", "")[0] == 'C' ? true : false ) : false;
    static bool regTest = GetBoolArg("-regtest", false);

    unsigned int nExtraNonce = 0;
    std::shared_ptr<CReserveScript> coinbaseScript;
    GetMainSignals().ScriptForMining(coinbaseScript, NULL);

     // Meter hashes/sec
    if (nHPSTimerStart == 0)
    {
        LOCK(timerCS);
        if (nHPSTimerStart == 0)
        {
            nHPSTimerStart = GetTimeMillis();
            nHashCounter = 0;
            dHashesPerSec = 0;
        }
    }

    try {
        // Throw an error if no script was provided.  This can happen
        // due to some internal error but also if the keypool is empty.
        // In the latter case, already the pointer is NULL.
        if (!coinbaseScript || coinbaseScript->reserveScript.empty())
            throw std::runtime_error("No coinbase script available (mining requires a wallet)");


        while (true)
        {
            if (!regTest && !hashCity)
            {
                // Busy-wait for the network to come online so we don't waste time mining on an obsolete chain. In regtest mode we expect to fly solo.
                while (true)
                {
                    if (g_connman->GetNodeCount(CConnman::CONNECTIONS_ALL) > 0 && !IsInitialBlockDownload())
                        break;
                    MilliSleep(1000);
                }
            }

            //
            // Create new block
            //
            unsigned int nTransactionsUpdatedLast = mempool.GetTransactionsUpdated();
            CBlockIndex* pindexParent = chainActive.Tip();
            CBlockIndex* pTipAtStartOfMining = pindexParent;

            if ( !pindexParent )
                continue;

            boost::this_thread::interruption_point();

            //fixme: (2.0) (POW2) - Implement this also for RPC mining.
            // If PoW2 witnessing is active.
            // Phase 3 - We always mine on the last PoW block for which we have a witness.
            // This is always tip~1
            // In the case where the current tip is a witness block, we want to embed the witness block and mine on top of the previous PoW block. (new chain tip)
            // In the case where the current tip is a PoW block we want to mine on top of the PoW block that came before it. (competing chain tip to the current one - in case there is no witness for the current one)
            // Phase 4 - We always mine on the last witnessed block.
            // Tip if last mined block was a witness block. (Mining new chain tip)
            // Tip~1 if the last mine block was a PoW block. (Competing chain tip to the current one - in case there is no witness for the current one)
            //int nPoW2PhaseTip = GetPoW2Phase(pindexTip, Params(), chainActive);
            int nPoW2PhaseGreatGrandParent = pindexParent->pprev && pindexParent->pprev->pprev ? GetPoW2Phase(pindexParent->pprev->pprev, Params(), chainActive) : 1;
            int nPoW2PhaseGrandParent = pindexParent->pprev ? GetPoW2Phase(pindexParent->pprev, Params(), chainActive) : 1;
            boost::this_thread::interruption_point();

            CBlockIndex* pWitnessBlockToEmbed = nullptr;

            if (nPoW2PhaseGrandParent >= 3)
            {
                if (pindexParent->nVersionPoW2Witness != 0)
                {
                    if (nPoW2PhaseGrandParent == 3)
                    {
                        LOCK(cs_main); // Required for GetPoWBlockForPoSBlock
                        pWitnessBlockToEmbed = pindexParent;
                        pindexParent = GetPoWBlockForPoSBlock(pindexParent);
                        assert(pindexParent);
                    }
                }
                else
                {
                    if (nPoW2PhaseGreatGrandParent >= 3)
                    {
                        // See if there are higher level witness blocks with less work (delta diff drop) - if there are then we mine those first to try build a new larger chain.
                        bool tryHighLevelCandidate = false;
                        auto candidateIters = GetTopLevelWitnessOrphans(pindexParent->nHeight);
                        for (auto candidateIter = candidateIters.rbegin(); candidateIter != candidateIters.rend(); ++candidateIter )
                        {
                            if (nPoW2PhaseGreatGrandParent >= 4)
                            {
                                if ((*candidateIter)->nVersionPoW2Witness != 0)
                                {
                                    pindexParent = *candidateIter;
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
                                    pindexParent = pParentPoW;
                                    tryHighLevelCandidate = true;
                                    break;
                                }
                            }
                        }

                        // No higher level blocks - chain might be stalled; absent witness(es); so drop further back in the history and try mine a different chain.
                        if (!tryHighLevelCandidate && nPoW2PhaseGrandParent >= 4)
                        {
                            pindexParent = pindexParent->pprev;
                        }
                        else if (!tryHighLevelCandidate)
                        {
                            int nCount=0;
                            while (pindexParent->pprev && ++nCount < 10)
                            {
                                // Grab the witness from our index.
                                pWitnessBlockToEmbed = GetWitnessOrphanForBlock(pindexParent->pprev->nHeight, pindexParent->pprev->GetBlockHashLegacy(), pindexParent->pprev->GetBlockHashLegacy());

                                //We don't have it in our index - try extract it from the tip in which we have already embedded it.
                                if (!pWitnessBlockToEmbed)
                                {
                                    std::shared_ptr<CBlock> pBlockPoWParent(new CBlock);
                                    LOCK(cs_main); // For ReadBlockFromDisk
                                    if (ReadBlockFromDisk(*pBlockPoWParent.get(), pindexParent, Params().GetConsensus()))
                                    {
                                        int nWitnessCoinbaseIndex = GetPoW2WitnessCoinbaseIndex(*pBlockPoWParent.get());
                                        if (nWitnessCoinbaseIndex != -1)
                                        {
                                            std::shared_ptr<CBlock> embeddedWitnessBlock(new CBlock);
                                            if (ExtractWitnessBlockFromWitnessCoinbase(chainActive, nWitnessCoinbaseIndex, pindexParent->pprev, *pBlockPoWParent.get(), chainparams, *pcoinsTip, *embeddedWitnessBlock.get()))
                                            {
                                                uint256 hashPoW2Witness = embeddedWitnessBlock->GetHashPoW2();
                                                if (mapBlockIndex.count(hashPoW2Witness) > 0)
                                                {
                                                    pWitnessBlockToEmbed = mapBlockIndex[hashPoW2Witness];
                                                    break;
                                                }
                                                else
                                                {
                                                    if (ProcessNewBlock(Params(), embeddedWitnessBlock, true, nullptr))
                                                    {
                                                        if (mapBlockIndex.count(hashPoW2Witness) > 0)
                                                        {
                                                            pWitnessBlockToEmbed = mapBlockIndex[hashPoW2Witness];
                                                            break;
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                                if (!pWitnessBlockToEmbed)
                                {
                                    pindexParent = pindexParent->pprev;
                                    continue;
                                }
                            }
                            if (!pWitnessBlockToEmbed)
                            {
                                if (GetTimeMillis() - nUpdateTimeStart > 5000)
                                {
                                    LogPrintf("Error in GuldenMiner: mining stalled, unable to locate suitable witness block to embed.\n");
                                    dHashesPerSec = 0;
                                }
                                continue;
                            }
                            if (pindexParent->nHeight != pWitnessBlockToEmbed->nHeight)
                                pindexParent = pindexParent->pprev;
                        }
                    }
                    else
                    {
                        pindexParent = pindexParent->pprev;
                        pWitnessBlockToEmbed = nullptr;
                    }
                }
            }

            if (nPoW2PhaseGreatGrandParent >= 4)
            {
                if (pindexParent->nVersionPoW2Witness == 0)
                {
                    if (GetTimeMillis() - nUpdateTimeStart > 5000)
                    {
                        dHashesPerSec = 0;
                        LogPrintf("Error in GuldenMiner: mining stalled, unable to locate suitable witness tip on which to build.\n");
                    }
                    continue;
                }
            }

            std::unique_ptr<CBlockTemplate> pblocktemplate;
            {
                TRY_LOCK(processBlockCS, lockProcessBlock);
                if(!lockProcessBlock)
                {
                    continue;
                }

                pblocktemplate = BlockAssembler(Params()).CreateNewBlock(pindexParent, coinbaseScript->reserveScript, true, pWitnessBlockToEmbed);
                if (!pblocktemplate.get())
                {
                    LogPrintf("Error in GuldenMiner: Keypool ran out, please call keypoolrefill.\n");
                    if (GetTimeMillis() - nUpdateTimeStart > 5000)
                            dHashesPerSec = 0;
                    continue;
                }
            }
            CBlock *pblock = &pblocktemplate->block;
            IncrementExtraNonce(pblock, pindexParent, nExtraNonce);

            //LogPrintf("Running GuldenMiner with %u transactions in block (%u bytes)\n", pblock->vtx.size(), ::GetSerializeSize(*pblock, SER_NETWORK, PROTOCOL_VERSION));

            //
            // Search
            //
            int64_t nStart = GetTime();
            arith_uint256 hashTarget = arith_uint256().SetCompact(pblock->nBits);

            // Check if something found
            arith_uint256 hashMined;
            while (true)
            {
                if (GetTimeMillis() - nHPSTimerStart > hashPSTimerInterval)
                {
                    // Check for stop or if block needs to be rebuilt
                    boost::this_thread::interruption_point();

                    TRY_LOCK(timerCS, lockhc);
                    if (lockhc && GetTimeMillis() - nHPSTimerStart > hashPSTimerInterval)
                    {
                        int64_t nTemp = nHashCounter;
                        nHashCounter = 0;
                        dHashesPerSec = hashPSTimerInterval * (nTemp / (GetTimeMillis() - nHPSTimerStart));
                        dBestHashesPerSec = std::max(dBestHashesPerSec, dHashesPerSec);
                        nHPSTimerStart = GetTimeMillis();
                    }
                }
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
                        LogPrintf("GuldenMiner: proof-of-work found  \n  hash: %s  \ntarget: %s\n", hashMined.GetHex(), hashTarget.GetHex());
                        std::shared_ptr<const CBlock> shared_pblock = std::make_shared<const CBlock>(*pblock);
                        ProcessBlockFound(shared_pblock, chainparams);
                        coinbaseScript->KeepScript();

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
                if (pTipAtStartOfMining != chainActive.Tip())
                    break;
            }
        }
    }
    catch (const boost::thread_interrupted&)
    {
        LogPrintf("GuldenMiner terminated\n");
        throw;
    }
    catch (const std::runtime_error &e)
    {
        LogPrintf("GuldenMiner runtime error: %s\n", e.what());
        return;
    }
}

void PoWMineGulden(bool fGenerate, int nThreads, const CChainParams& chainparams)
{
    static boost::thread_group* minerThreads = NULL;

    if (nThreads < 0)
        nThreads = GetNumCores();

    if (minerThreads != NULL)
    {
        minerThreads->interrupt_all();
        delete minerThreads;
        minerThreads = NULL;
    }

    if (nThreads == 0 || !fGenerate)
        return;

    minerThreads = new boost::thread_group();
    for (int i = 0; i < nThreads; i++)
        minerThreads->create_thread(boost::bind(&GuldenMiner, boost::cref(chainparams)));
}


bool SignBlockAsWitness(std::shared_ptr<CBlock> pBlock, CTxOut fittestWitnessOutput)
{
    assert(pBlock->nVersionPoW2Witness != 0);

    CKeyID witnessKeyID;


    if (fittestWitnessOutput.GetType() == CTxOutType::PoW2WitnessOutput)
    {
        witnessKeyID = fittestWitnessOutput.output.witnessDetails.witnessKeyID;
    }
    else if ( (fittestWitnessOutput.GetType() <= CTxOutType::ScriptLegacyOutput && fittestWitnessOutput.output.scriptPubKey.IsPoW2Witness()) ) //fixme: (2.1) We can remove this.
    {
        std::vector<unsigned char> hashWitnessBytes = fittestWitnessOutput.output.scriptPubKey.GetPow2WitnessHash();
        witnessKeyID = CKeyID(uint160(hashWitnessBytes));
    }
    else
    {
        assert(0);
    }

    CKey key;
    if (!pactiveWallet->GetKey(witnessKeyID, key))
        assert(0);

    // Do not allow uncompressed keys.
    if (!key.IsCompressed())
        assert(0);

    //fixme: (2.0) - Anything else to serialise here? Add the block height maybe? probably overkill.
    uint256 hash = pBlock->GetHashPoW2();
    if (!key.SignCompact(hash, pBlock->witnessHeaderPoW2Sig))
        return false;

    //LogPrintf(">>>[WitFound] witness pubkey [%s]\n", key.GetPubKey().GetID().GetHex());

    //fixme: (2.0) (RELEASE) - Remove this, it is here for testing purposes only.
    if (fittestWitnessOutput.GetType() == CTxOutType::PoW2WitnessOutput)
    {
        if (fittestWitnessOutput.output.witnessDetails.witnessKeyID != key.GetPubKey().GetID())
            assert(0);
    }
    else
    {
        if (CKeyID(uint160(fittestWitnessOutput.output.scriptPubKey.GetPow2WitnessHash())) != key.GetPubKey().GetID())
            assert(0);
    }

    return true;
}





void CreateWitnessSubsidyOutputs(CMutableTransaction& coinbaseTx, std::shared_ptr<CReserveScript> coinbaseScript, CAmount witnessSubsidy, CTxOut& selectedWitnessOutput, COutPoint& selectedWitnessOutPoint, bool compoundWitnessEarnings, int nPoW2PhaseParent, unsigned int nSelectedWitnessBlockHeight)
{
    // Forbid compound earnings for phase 3, as we can't handle this in a backwards compatible way.
    if (nPoW2PhaseParent == 3)
        compoundWitnessEarnings = false;

    // First obtain the details of the signing witness transaction which must be consumed as an input and recreated as an output.
    CTxOutPoW2Witness witnessInput; GetPow2WitnessOutput(selectedWitnessOutput, witnessInput);

    // Now ammend some details of the input that must change in the new output.
    CPoW2WitnessDestination witnessDestination;
    witnessDestination.spendingKey = witnessInput.spendingKeyID;
    witnessDestination.witnessKey = witnessInput.witnessKeyID;
    witnessDestination.lockFromBlock = witnessInput.lockFromBlock;
    witnessDestination.lockUntilBlock = witnessInput.lockUntilBlock;
    witnessDestination.failCount = witnessInput.failCount;
    // If this is the first time witnessing the lockFromBlock won't yet be filled in so fill it in now.
    if (witnessDestination.lockFromBlock == 0)
        witnessDestination.lockFromBlock = nSelectedWitnessBlockHeight;
    // If fail count is non-zero then we are allowed to decrement it by one every time we witness.
    if (witnessDestination.failCount > 0)
        witnessDestination.failCount = witnessDestination.failCount - 1;

    // Finally create the output(s).
    // If we are compounding then we add the subsidy to the witness output, otherwise we need a seperate output for the subsidy.
    coinbaseTx.vout.resize(compoundWitnessEarnings?1:2);

    if (nPoW2PhaseParent >= 4)
    {
        coinbaseTx.vout[0].SetType(CTxOutType::PoW2WitnessOutput);
        coinbaseTx.vout[0].output.witnessDetails.spendingKeyID = witnessDestination.spendingKey;
        coinbaseTx.vout[0].output.witnessDetails.witnessKeyID = witnessDestination.witnessKey;
        coinbaseTx.vout[0].output.witnessDetails.lockFromBlock = witnessDestination.lockFromBlock;
        coinbaseTx.vout[0].output.witnessDetails.lockUntilBlock = witnessDestination.lockUntilBlock;
        coinbaseTx.vout[0].output.witnessDetails.failCount = witnessDestination.failCount;
    }
    else
    {
        coinbaseTx.vout[0].SetType(CTxOutType::ScriptLegacyOutput);
        coinbaseTx.vout[0].output.scriptPubKey = GetScriptForDestination(witnessDestination);
    }
    coinbaseTx.vout[0].nValue = selectedWitnessOutput.nValue;

    if (compoundWitnessEarnings)
    {
        coinbaseTx.vout[0].nValue += witnessSubsidy;
    }
    else
    {
        //fixme: (2.0) standard key hash?
        coinbaseTx.vout[1].SetType(CTxOutType::ScriptLegacyOutput);
        coinbaseTx.vout[1].output.scriptPubKey = coinbaseScript->reserveScript;
        coinbaseTx.vout[1].nValue = witnessSubsidy;
    }
}

CMutableTransaction CreateWitnessCoinbase(int nWitnessHeight, int nPoW2PhaseParent, std::shared_ptr<CReserveScript> coinbaseScript, CAmount witnessSubsidy, CTxOut& selectedWitnessOutput, COutPoint& selectedWitnessOutPoint, unsigned int nSelectedWitnessBlockHeight, CAccount* selectedWitnessAccount)
{
    CMutableTransaction coinbaseTx(CURRENT_TX_VERSION_POW2);
    coinbaseTx.vin.resize(2);
    coinbaseTx.vin[0].prevout.SetNull();
    coinbaseTx.vin[0].SetSequence(0, CURRENT_TX_VERSION_POW2, CTxInFlags::None);
    coinbaseTx.vin[1].prevout = selectedWitnessOutPoint;
    coinbaseTx.vin[1].SetSequence(0, CURRENT_TX_VERSION_POW2, CTxInFlags::None);

    // Phase 3 - we restrict the coinbase signature to only the block height.
    // This helps simplify the logic for the PoW mining (which has to stuff all this info into it's own coinbase signature).
    if (nPoW2PhaseParent < 4)
    {
        coinbaseTx.vin[0].scriptSig = CScript() << nWitnessHeight;
    }
    else
    {
        std::string coinbaseSignature = GetArg("-coinbasesignature", "");
        coinbaseTx.vin[0].scriptWitness.stack.clear();
        coinbaseTx.vin[0].scriptWitness.stack.push_back(std::vector<unsigned char>());
        CVectorWriter(0, 0, coinbaseTx.vin[0].scriptWitness.stack[0], 0) << VARINT(nWitnessHeight);
        coinbaseTx.vin[0].scriptWitness.stack.push_back(std::vector<unsigned char>(coinbaseSignature.begin(), coinbaseSignature.end()));
    }

    // Sign witness coinbase.
    {
        LOCK(pactiveWallet->cs_wallet);
        if (!pactiveWallet->SignTransaction(selectedWitnessAccount, coinbaseTx, Witness))
        {
            //fixme: (2.0) error handling
            assert(0);
        }
    }

    //fixme: (2.0) - Optionally compound here instead.
    bool compoundWitnessEarnings = false;

    // Output for subsidy and refresh witness address.
    CreateWitnessSubsidyOutputs(coinbaseTx, coinbaseScript, witnessSubsidy, selectedWitnessOutput, selectedWitnessOutPoint, compoundWitnessEarnings, nPoW2PhaseParent, nSelectedWitnessBlockHeight);

    return coinbaseTx;
}


//fixme: (2.1) We should also check for already signed block coming from ourselves (from e.g. a different machine - think witness devices for instance) - Don't sign it if we already have a signed copy of the block lurking around...
std::set<CBlockIndex*, CBlockIndexCacheComparator> cacheAlreadySeenWitnessCandidates;

void static GuldenWitness()
{
    LogPrintf("GuldenWitness started\n");
    RenameThread("gulden-witness");

    static bool hashCity = IsArgSet("-testnet") ? ( GetArg("-testnet", "")[0] == 'C' ? true : false ) : false;
    static bool regTest = GetBoolArg("-regtest", false);

    uint64_t nTimeTotal = 0;

    CChainParams chainparams = Params();
    try
    {
        while (true)
        {
            if (!regTest)
            {
                // Busy-wait for the network to come online so we don't waste time mining
                // on an obsolete chain. In regtest mode we expect to fly solo.
                do
                {
                    if (hashCity || g_connman->GetNodeCount(CConnman::CONNECTIONS_ALL) > 0)
                    {
                        if(!IsInitialBlockDownload())
                            break;
                    }
                    MilliSleep(5000);
                } while (true);
            }
            DO_BENCHMARK("WIT: GuldenWitness", BCLog::BENCH|BCLog::WITNESS);

            CBlockIndex* pindexTip = chainActive.Tip();
            Consensus::Params pParams = chainparams.GetConsensus();

            //We can only start witnessing from phase 3 onward.
            if ( !pindexTip || !pindexTip->pprev || !IsPow2WitnessingActive(pindexTip, chainparams, chainActive)  )
            {
                MilliSleep(5000);
                continue;
            }
            int nPoW2PhasePrev = GetPoW2Phase(pindexTip->pprev, chainparams, chainActive);

            //fixme: (2.0) Shorter sleep here?
            //Or ideally instead of just sleeping/busy polling rather wait on a signal that gets triggered only when new blocks come in??
            MilliSleep(100);

            // Check for stop or if block needs to be rebuilt
            boost::this_thread::interruption_point();

            // If we already have a witnessed block at the tip don't bother looking at any orphans, just patiently wait for next unsigned tip.
            if (nPoW2PhasePrev < 3 || pindexTip->nVersionPoW2Witness != 0)
                continue;

            // Use a cache to prevent trying the same blocks over and over.
            // Look for all potential signable blocks at same height as the index tip - don't limit ourselves to just the tip
            // This is important because otherwise the chain can stall if there is an absent signer for the current tip.
            std::vector<CBlockIndex*> candidateOrphans;
            if (cacheAlreadySeenWitnessCandidates.find(pindexTip) == cacheAlreadySeenWitnessCandidates.end())
            {
                candidateOrphans.push_back(pindexTip);
            }
            if (candidateOrphans.size() == 0)
            {
                for (const auto candidateIter : GetTopLevelPoWOrphans(pindexTip->nHeight, *(pindexTip->pprev->phashBlock)))
                {
                    if (cacheAlreadySeenWitnessCandidates.find(candidateIter) == cacheAlreadySeenWitnessCandidates.end())
                    {
                        candidateOrphans.push_back(candidateIter);
                    }
                }
                if (cacheAlreadySeenWitnessCandidates.size() > 50)
                {
                    auto eraseEnd = cacheAlreadySeenWitnessCandidates.begin();
                    std::advance(eraseEnd, cacheAlreadySeenWitnessCandidates.size() - 50);
                    cacheAlreadySeenWitnessCandidates.erase(cacheAlreadySeenWitnessCandidates.begin(), eraseEnd);
                }
            }
            boost::this_thread::interruption_point();

            if (candidateOrphans.size() > 0)
            {
                LOCK(processBlockCS);

                if (chainActive.Tip() != pindexTip)
                {
                    continue;
                }

                LOCK(cs_main); // For ReadBlockFromDisk
                for (const auto candidateIter : candidateOrphans)
                {
                    boost::this_thread::interruption_point();

                    cacheAlreadySeenWitnessCandidates.insert(candidateIter);

                    //Create new block
                    std::shared_ptr<CBlock> pWitnessBlock(new CBlock);
                    if (ReadBlockFromDisk(*pWitnessBlock, candidateIter, pParams))
                    {
                        boost::this_thread::interruption_point();
                        CGetWitnessInfo witnessInfo;

                        //fixme: (2.0) Error handling
                        if (!GetWitness(chainActive, chainparams, nullptr, candidateIter->pprev, *pWitnessBlock, witnessInfo))
                            continue;

                        boost::this_thread::interruption_point();
                        CAmount witnessSubsidy = GetBlockSubsidyWitness(candidateIter->nHeight, pParams);

                        //fixme: (2.0) (POW2) (ISMINE_WITNESS)
                        if (pactiveWallet->IsMine(witnessInfo.selectedWitnessTransaction) == ISMINE_SPENDABLE)
                        {
                            CAccount* selectedWitnessAccount = pactiveWallet->FindAccountForTransaction(witnessInfo.selectedWitnessTransaction);
                            if (selectedWitnessAccount)
                            {
                                //We must do this before we add the blank coinbase otherwise GetBlockWeight crashes on a NULL pointer dereference.
                                int nStartingBlockWeight = GetBlockWeight(*pWitnessBlock);

                                /** First we add the new witness coinbase to the block, this acts as a seperator between transactions from the initial mined block and the witness block **/
                                /** We add a placeholder for now as we don't know the fees we will generate **/
                                pWitnessBlock->vtx.emplace_back();
                                unsigned int nWitnessCoinbaseIndex = pWitnessBlock->vtx.size()-1;
                                nStartingBlockWeight += 200;

                                std::shared_ptr<CReserveScript> coinbaseScript;
                                //fixme: (2.0) (SEGSIG)
                                GetMainSignals().ScriptForMining(coinbaseScript, selectedWitnessAccount);

                                int nPoW2PhaseParent = GetPoW2Phase(candidateIter->pprev, chainparams, chainActive);

                                /** Now add any additional transactions if there is space left **/
                                if (nPoW2PhaseParent >= 4)
                                {
                                    // Piggy back off existing block assembler code to grab the transactions we want to include.
                                    // Setup maximum size for assembler so that size of existing (PoW) block transactions are subtracted from overall maximum.
                                    BlockAssembler::Options assemblerOptions;
                                    assemblerOptions.nBlockMaxWeight = DEFAULT_BLOCK_MAX_WEIGHT - nStartingBlockWeight;
                                    assemblerOptions.nBlockMaxSize = assemblerOptions.nBlockMaxWeight;

                                    std::unique_ptr<CBlockTemplate> pblocktemplate(BlockAssembler(Params(), assemblerOptions).CreateNewBlock(candidateIter, coinbaseScript->reserveScript, true, nullptr, true));
                                    if (!pblocktemplate.get())
                                    {
                                        LogPrintf("Error in GuldenWitness: Keypool ran out, please call keypoolrefill before restarting the mining thread\n");
                                        continue;
                                    }

                                    // Skip the coinbase as we obviously don't want this included again, it is already in the PoW part of the block.
                                    size_t nSkipCoinbase = 1;
                                    //fixme: (2.1) (CBSU)? pre-allocate for vtx.size().
                                    for (size_t i=nSkipCoinbase; i < pblocktemplate->block.vtx.size(); i++)
                                    {
                                        bool bSkip = false;
                                        // fixme: (2.1) Check why we were getting duplicates - something to do with mempool not being updated for latest block or something?
                                        // Exclude any duplicates that somehow creep in.
                                        for(size_t j=0; j < nWitnessCoinbaseIndex; j++)
                                        {
                                            if (pWitnessBlock->vtx[j]->GetHash() == pblocktemplate->block.vtx[i]->GetHash())
                                            {
                                                bSkip = true;
                                                break;
                                            }
                                        }
                                        if (!bSkip)
                                        {
                                            //fixme: (2.1) emplace_back for better performace?
                                            pWitnessBlock->vtx.push_back(pblocktemplate->block.vtx[i]);
                                            //fixme: (2.0) (HIGH) do not include fees as part of witness subsidy if compounding.
                                            witnessSubsidy += (pblocktemplate->vTxFees[i]);
                                        }
                                    }
                                }


                                //fixme: (2.0) (SEGSIG) - Implement new transaction types here?
                                /** Populate witness coinbase placeholder with real information now that we have it **/
                                //fixme: (2.0) (HIGH) do not include fees as part of witness subsidy if compounding.
                                CMutableTransaction coinbaseTx = CreateWitnessCoinbase(candidateIter->nHeight, nPoW2PhaseParent, coinbaseScript, witnessSubsidy, witnessInfo.selectedWitnessTransaction, witnessInfo.selectedWitnessOutpoint, witnessInfo.selectedWitnessBlockHeight, selectedWitnessAccount);
                                pWitnessBlock->vtx[nWitnessCoinbaseIndex] = MakeTransactionRef(std::move(coinbaseTx));


                                /** Set witness specific block header information **/
                                //testme: (GULDEN) (POW2) (2.0)
                                {
                                    // ComputeBlockVersion returns the right version flag to signal for phase 4 activation here, assuming we are already in phase 3 and 95 percent of peers are upgraded.
                                    pWitnessBlock->nVersionPoW2Witness = ComputeBlockVersion(candidateIter->pprev, pParams);

                                    // Second witness timestamp gets added to the block as an additional time source to the miner timestamp.
                                    // Witness timestamp must exceed median of previous mined timestamps.
                                    pWitnessBlock->nTimePoW2Witness = std::max(candidateIter->GetMedianTimePastPoW()+1, GetAdjustedTime());

                                    // Set witness merkle hash.
                                    pWitnessBlock->hashMerkleRootPoW2Witness = BlockMerkleRoot(pWitnessBlock->vtx.begin()+nWitnessCoinbaseIndex, pWitnessBlock->vtx.end());
                                }


                                /** Do the witness operation (Sign the block using our witness key) and broadcast the final product to the network. **/
                                if (SignBlockAsWitness(pWitnessBlock, witnessInfo.selectedWitnessTransaction))
                                {
                                    LogPrint(BCLog::WITNESS, "GuldenWitness: witness found %s", pWitnessBlock->GetHashPoW2().ToString());
                                    ProcessBlockFound(pWitnessBlock, chainparams);
                                    continue;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    catch (const boost::thread_interrupted&)
    {
        cacheAlreadySeenWitnessCandidates.clear();
        LogPrintf("GuldenWitness terminated\n");
        throw;
    }
    catch (const std::runtime_error &e)
    {
        cacheAlreadySeenWitnessCandidates.clear();
        LogPrintf("GuldenWitness runtime error: %s\n", e.what());
        return;
    }
}


void StartPoW2WitnessThread(boost::thread_group& threadGroup)
{
    threadGroup.create_thread(boost::bind(&TraceThread<void (*)()>, "pow2_witness", &GuldenWitness));
}
