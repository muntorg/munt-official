// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// File contains modifications by: The Gulden developers
// All modifications:
// Copyright (c) 2016-2017 The Gulden developers
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
#include "hash.h"
#include "validation.h"
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
// BitcoinMiner
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
    fIncludeWitness = false;

    // These counters do not include coinbase tx
    nBlockTx = 0;
    nFees = 0;
}


void InsertPoW2WitnessIntoCoinbase(CBlock& block, const CBlockIndex* pindexPrev, const Consensus::Params& consensusParams)
{
    std::vector<unsigned char> commitment;
    int commitpos = GetPoW2WitnessCoinbaseIndex(block);

    CBlockIndex* pPoW2Index = GetWitnessOrphanForBlock(pindexPrev->nHeight, pindexPrev->pprev->GetBlockHashLegacy(), pindexPrev->GetBlockHashLegacy());
    std::shared_ptr<CBlock> pWitnessBlock(new CBlock);
    if (!ReadBlockFromDisk(*pWitnessBlock, pPoW2Index, consensusParams))
        assert(0);
    assert(pPoW2Index);

    if (commitpos == -1)
    {
        std::vector<unsigned char> serialisedWitnessHeaderInfo;
        {
            CVectorWriter serialisedWitnessHeaderInfoStream(SER_NETWORK, INIT_PROTO_VERSION, serialisedWitnessHeaderInfo, 0);
            ::Serialize(serialisedWitnessHeaderInfoStream, pPoW2Index->nVersionPoW2Witness); //4 bytes
            ::Serialize(serialisedWitnessHeaderInfoStream, pPoW2Index->nTimePoW2Witness); //4 bytes
            ::Serialize(serialisedWitnessHeaderInfoStream, pPoW2Index->hashMerkleRootPoW2Witness); // 32 bytes
            ::Serialize(serialisedWitnessHeaderInfoStream, NOSIZEVECTOR(pPoW2Index->witnessHeaderPoW2Sig)); //65 bytes
            ::Serialize(serialisedWitnessHeaderInfoStream, pindexPrev->GetBlockHashLegacy()); //32 bytes
        }

        assert(serialisedWitnessHeaderInfo.size() == 137);

        CTxOut out;
        out.SetType(CTxOutType::ScriptLegacyOutput);

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
}

std::unique_ptr<CBlockTemplate> BlockAssembler::CreateNewBlock(const CScript& scriptPubKeyIn, bool fMineWitnessTx)
{
    fMineWitnessTx = true;

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
    LOCK(mempool.cs);

    CBlockIndex* pindexPrev = chainActive.Tip();
    nHeight = pindexPrev->nHeight + 1;

    //Until PoW2 activates mining subsidy remains full, after it activates PoW part of subsidy is reduced.
    //fixme: (GULDEN) (2.1) (CLEANUP) - We can remove this after 2.1 becomes active.
    Consensus::Params consensusParams = chainparams.GetConsensus();
    CAmount nSubsidy = GetBlockSubsidy(nHeight, consensusParams);
    if (IsPow2WitnessingActive(pindexPrev, chainparams))
        nSubsidy -= GetBlockSubsidyWitness(nHeight, consensusParams);

    int nPow2Phase = GetPoW2Phase(pindexPrev, chainparams);
    int nPrevPow2Phase = GetPoW2Phase(pindexPrev->pprev, chainparams);

    // First block of phase 4 contains two witness subsidies so miner loses out on 20 NLG for this block
    // This block is treated special. (but special casing can dissapear for 2.1 release.
    if (nPrevPow2Phase == 3)
    {
        if (nPow2Phase == 4)
        {
            nSubsidy -= GetBlockSubsidyWitness(nHeight, consensusParams);
        }
    }

    // PoW mining on top of a PoS block during phase 3 indicates an error of some kind.
    if (nPow2Phase <= 3)
        assert(pindexPrev->nVersionPoW2Witness == 0);

    pblock->nVersion = ComputeBlockVersion(pindexPrev, consensusParams);
    // -regtest only: allow overriding block.nVersion with
    // -blockversion=N to test forking scenarios
    if (chainparams.MineBlocksOnDemand())
        pblock->nVersion = GetArg("-blockversion", pblock->nVersion);

    pblock->nTime = GetAdjustedTime();
    const int64_t nMedianTimePast = pindexPrev->GetMedianTimePastWitness();

    nLockTimeCutoff = (STANDARD_LOCKTIME_VERIFY_FLAGS & LOCKTIME_MEDIAN_TIME_PAST)
                       ? nMedianTimePast
                       : pblock->GetBlockTime();

    // Decide whether to include witness transactions
    // This is only needed in case the witness softfork activation is reverted
    // (which would require a very deep reorganization) or when
    // -promiscuousmempoolflags is used.
    // TODO: replace this with a call to main to assess validity of a mempool
    // transaction (which in most cases can be a no-op).
    //fIncludeWitness = IsWitnessEnabled(pindexPrev, consensusParams) && fMineWitnessTx;

    int nPackagesSelected = 0;
    int nDescendantsUpdated = 0;
    addPackageTxs(nPackagesSelected, nDescendantsUpdated);

    int64_t nTime1 = GetTimeMicros();

    nLastBlockTx = nBlockTx;
    nLastBlockSize = nBlockSize;
    nLastBlockWeight = nBlockWeight;

    // Create coinbase transaction.
    CMutableTransaction coinbaseTx(CURRENT_TX_VERSION_POW2);
    coinbaseTx.vin.resize(1);
    coinbaseTx.vin[0].prevout.SetNull();
    coinbaseTx.vout.resize(1);
    //fixme: (GULDEN) (2.0) (SEGSIG) - Change to other output types for phase 4 onward?
    coinbaseTx.vout[0].output.scriptPubKey = scriptPubKeyIn;
    coinbaseTx.vout[0].nValue = nFees + nSubsidy;
    std::string coinbaseSignature = GetArg("-coinbasesignature", "");

    // Insert the height into the coinbase (to ensure all coinbase transactions have a unique hash)
    // Further, also insert any optional 'signature' data (identifier of miner or other private miner data etc.)
    if (nPrevPow2Phase < 4)
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
    //fixme: (GULDEN) (2.0) (SEGSIG)
    //pblocktemplate->vchCoinbaseCommitment = GenerateCoinbaseCommitment(*pblock, pindexPrev, chainparams.GetConsensus());
    pblocktemplate->vTxFees[0] = -nFees;

    // From second phase 3 block onward until (and including first phase 4 block) embed the PoW2 witness.
    if (nPrevPow2Phase == 3)
    {
        InsertPoW2WitnessIntoCoinbase(*pblock, pindexPrev, consensusParams);
    }

    // Until phase 4 activates we don't point to the hash of the previous PoW2 block but rather the hash of the previous PoW block (as we need to communicate with legacy clients that don't know about PoW blocks)
    // For phase 3 we embed the hash of the contents of the PoW2 block in a special coinbase transaction.
    // Fill in header
    if (nPow2Phase >= 4)
    {
        pblock->hashPrevBlock  = pindexPrev->GetBlockHashPoW2();
    }
    else
    {
        pblock->hashPrevBlock  = pindexPrev->GetBlockHashLegacy();
    }


    //uint64_t nSerializeSize = GetSerializeSize(*pblock, SER_NETWORK, PROTOCOL_VERSION);
    //LogPrintf("CreateNewBlock(): total size: %u block weight: %u txs: %u fees: %ld sigops %d\n", nSerializeSize, GetBlockWeight(*pblock), nBlockTx, nFees, nBlockSigOpsCost);


    //fixme: (GULDEN) (2.0) (POW2) (HIGH) - Other mining logic, coinbase etc. logic here.

    UpdateTime(pblock, consensusParams, pindexPrev);
    //Already done inside UpdateTime
    //pblock->nBits          = GetNextWorkRequired(pindexPrev, pblock, consensusParams);
    pblock->nNonce         = 0;
    pblocktemplate->vTxSigOpsCost[0] = WITNESS_SCALE_FACTOR * GetLegacySigOpCount(*pblock->vtx[0]);

    CValidationState state;
    if (!TestBlockValidity(chainActive, state, chainparams, *pblock, pindexPrev, false, false)) {
        throw std::runtime_error(strprintf("%s: TestBlockValidity failed: %s", __func__, FormatStateMessage(state)));
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
    BOOST_FOREACH (const CTxMemPool::txiter it, package) {
        if (!IsFinalTx(it->GetTx(), nHeight, nLockTimeCutoff))
            return false;
        if (!fIncludeWitness && it->GetTx().HasWitness())
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
    BOOST_FOREACH(const CTxMemPool::txiter it, alreadyAdded) {
        CTxMemPool::setEntries descendants;
        mempool.CalculateDescendants(it, descendants);
        // Insert all descendants (not yet in block) into the modified set
        BOOST_FOREACH(CTxMemPool::txiter desc, descendants) {
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

    //fixme: (GULDEN) (2.0) (POW2) - We need to avoid adding any transactions from possible 'tip candidates' here (only for witness blocks) - figure out best way to do that.

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
    CBlockIndex* pIndexPrev = chainActive.Tip();

    LogPrintf("%s\n", pblock->ToString());
    LogPrintf("generated hash= %s hashpow2= %s  amt= %s [PoW2 phase: %d %d]\n", pblock->GetPoWHash().ToString(), pblock->GetHashPoW2().ToString(), FormatMoney(pblock->vtx[0]->vout[0].nValue), GetPoW2Phase(chainActive.Tip(), chainparams), GetPoW2Phase(chainActive.Tip()->pprev, chainparams));

    // Found a solution
    if (IsPow2Phase4Active(pIndexPrev->pprev, chainparams) || IsPow2Phase5Active(pIndexPrev, chainparams))
    {
        if (pIndexPrev->nVersionPoW2Witness == 0 ||  pblock->hashPrevBlock != pIndexPrev->GetBlockHashPoW2())
            return error("GuldenWitness: Generated phase4 block is stale");
    }
    else
    {
        if (pIndexPrev->nVersionPoW2Witness != 0 ||  pblock->hashPrevBlock != pIndexPrev->GetBlockHashPoW2())
            return error("GuldenWitness: Generated block is stale");
    }


    // Process this block the same as if we had received it from another node
    if (!ProcessNewBlock(chainparams, pblock, true, NULL))
        return error("GuldenMiner: ProcessNewBlock, block not accepted");

    return true;
}

double dBestHashesPerSec = 0.0;
double dHashesPerSec = 0.0;
int64_t nHPSTimerStart = 0;
int64_t nHashCounter=0;
int64_t nHashThrottle=-1;
static CCriticalSection timerCS;
static CCriticalSection processBlockCS;


void static BitcoinMiner(const CChainParams& chainparams)
{
    LogPrintf("GuldenMiner started\n");
    RenameThread("gulden-miner");

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
            CBlockIndex* pindexPrev = chainActive.Tip();

            if ( !pindexPrev )
                continue;

            boost::this_thread::interruption_point();

            //fixme: (GULDEN) (POW2) (2.0) - Implement this also for RPC mining.
            // If PoW2 witnessing is active.
            // Phase 3 - Tip must always be PoS with witness so we can always keep mining on tip.
            // Phase 4 - Tip must always be PoW2 with witness so we can always keep mining on tip. 
            int nPoW2Phase = GetPoW2Phase(pindexPrev, Params());

            boost::this_thread::interruption_point();

            if (nPoW2Phase == 3)
            {
                assert(pindexPrev->nVersionPoW2Witness == 0);
                if ( IsPow2Phase3Active(pindexPrev->pprev, Params()) )
                {
                    if (!GetWitnessOrphanForBlock(pindexPrev->nHeight, pindexPrev->pprev->GetBlockHashLegacy(), pindexPrev->GetBlockHashLegacy()))
                    {
                        MilliSleep(500);
                        continue;
                    }
                }
            }
            else if ( GetPoW2Phase(pindexPrev->pprev, Params()) >= 4 )
            {
                // Only mine on top of witnessed blocks
                if (pindexPrev->nVersionPoW2Witness == 0)
                {
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

                pblocktemplate =  BlockAssembler(Params()).CreateNewBlock(coinbaseScript->reserveScript);
                if (!pblocktemplate.get())
                {
                    LogPrintf("Error in GuldenMiner: Keypool ran out, please call keypoolrefill before restarting the mining thread\n");
                    return;
                }
            }
            CBlock *pblock = &pblocktemplate->block;
            IncrementExtraNonce(pblock, pindexPrev, nExtraNonce);

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
                if (GetTimeMillis() - nHPSTimerStart > 5000)
                {
                    // Check for stop or if block needs to be rebuilt
                    boost::this_thread::interruption_point();

                    TRY_LOCK(timerCS, lockhc);
                    if (lockhc && GetTimeMillis() - nHPSTimerStart > 5000)
                    {
                        int64_t nTemp = nHashCounter;
                        nHashCounter = 0;
                        dHashesPerSec = 5000 * (nTemp / (GetTimeMillis() - nHPSTimerStart));
                        dBestHashesPerSec = std::max(dBestHashesPerSec, dHashesPerSec);
                        nHPSTimerStart = GetTimeMillis();
                    }
                    // Update nTime every few seconds
                    if (UpdateTime(pblock, chainparams.GetConsensus(), pindexPrev) < 0)
                        break; // Recreate the block if the clock has run backwards,so that we can use the correct time.
                    arith_uint256 compBits = pblock->nBits;
                    if (GetNextWorkRequired(chainActive.Tip(), pblock, Params().GetConsensus()) != compBits)
                    {
                        break;
                    }
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
                if (pindexPrev != chainActive.Tip())
                    break;
                /*if (UpdateTime(pblock, chainparams.GetConsensus(), pindexPrev) < 0)
                    //break; // Recreate the block if the clock has run backwards,so that we can use the correct time.
                if (chainparams.GetConsensus().fPowAllowMinDifficultyBlocks)
                {
                    // Changing pblock->nTime can change work required on testnet:
                    hashTarget.SetCompact(pblock->nBits);
                }*/
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
        minerThreads->create_thread(boost::bind(&BitcoinMiner, boost::cref(chainparams)));
}


bool SignBlockAsWitness(std::shared_ptr<CBlock> pBlock, CTxOut fittestWitnessOutput)
{
    assert(pBlock->nVersionPoW2Witness != 0);

    CKeyID witnessKeyID;


    if (fittestWitnessOutput.GetType() == CTxOutType::PoW2WitnessOutput)
    {
        witnessKeyID = fittestWitnessOutput.output.witnessDetails.witnessKeyID;
    }
    else if ( (fittestWitnessOutput.GetType() <= CTxOutType::ScriptOutput && fittestWitnessOutput.output.scriptPubKey.IsPoW2Witness()) ) //fixme: (GULDEN) (2.1) We can remove this.
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

    //fixme: (GULDEN) (PoW2) (2.0) - Anything else to serialise here? Add the block height maybe? probably overkill.
    uint256 hash = pBlock->GetHashPoW2();
    if (!key.SignCompact(hash, pBlock->witnessHeaderPoW2Sig))
        return false;

    //fixme: (GULDEN) (2.0) (FINALRELEASE) - Remove this, it is here for testing purposes only.
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


void CreateWitnessSubsidyOutputs(CMutableTransaction& coinbaseTx, std::shared_ptr<CReserveScript> coinbaseScript, CAmount witnessSubsidy, CTxOut& selectedWitnessOutput, COutPoint& selectedWitnessOutPoint, bool compoundWitnessEarnings, int nPoW2Phase, unsigned int nSelectedWitnessBlockHeight)
{
    // Forbid compound earnings for phase 3, as we can't handle this in a backwards compatible way.
    if (nPoW2Phase == 3)
        compoundWitnessEarnings = false;

    // First obtain the details of the signing witness transaction which must be consumed as an input and recreated as an output.
    CTxOutPoW2Witness witnessInput = GetPow2WitnessOutput(selectedWitnessOutput);

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

    if (nPoW2Phase >= 4)
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
        //fixme: (GULDEN) (2.0) (SEGSIG)
        if (nPoW2Phase >= 4)
        {
            coinbaseTx.vout[1].SetType(CTxOutType::ScriptOutput);
        }
        else
        {
            coinbaseTx.vout[1].SetType(CTxOutType::ScriptLegacyOutput);
        }
        coinbaseTx.vout[1].output.scriptPubKey = coinbaseScript->reserveScript;
        coinbaseTx.vout[1].nValue = witnessSubsidy;
    }
}

CMutableTransaction CreateWitnessCoinbase(int nWitnessHeight, int nPrevPoW2Phase, int nPoW2Phase, std::shared_ptr<CReserveScript> coinbaseScript, CAmount witnessSubsidy, CTxOut& selectedWitnessOutput, COutPoint& selectedWitnessOutPoint, unsigned int nSelectedWitnessBlockHeight, CAccount* selectedWitnessAccount)
{
    CMutableTransaction coinbaseTx(CURRENT_TX_VERSION_POW2);
    coinbaseTx.vin.resize(2);
    coinbaseTx.vin[0].prevout.SetNull();
    coinbaseTx.vin[0].nSequence = 0;
    coinbaseTx.vin[1].prevout = selectedWitnessOutPoint;
    coinbaseTx.vin[1].nSequence = 0;

    // Phase 3 - we restrict the coinbase signature to only the block height.
    // This helps simplify the logic for the PoW mining (which has to stuff all this info into it's own coinbase signature).
    if (nPrevPoW2Phase < 4)
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
        pactiveWallet->SignTransaction(selectedWitnessAccount, coinbaseTx, Witness);
    }

    //fixme: (GULDEN) (2.0) - Optionally compound here instead.
    bool compoundWitnessEarnings = false;

    // Output for subsidy and refresh witness address.
    CreateWitnessSubsidyOutputs(coinbaseTx, coinbaseScript, witnessSubsidy, selectedWitnessOutput, selectedWitnessOutPoint, compoundWitnessEarnings, nPoW2Phase, nSelectedWitnessBlockHeight);

    return coinbaseTx;
}

//fixme: (GULDEN) (2.0) If running for a very long time this will eventually use up obscene amounts of memory - empty it every now and again
//fixme: (GULDEN) (2.0) We should also check for already signed block coming from ourselves (from e.g. a different machine - think witness devices for instance) - Don't sign it if we already have a signed copy of the block lurking around...
std::set<CBlockIndex*, CBlockIndexCacheComparator> cacheAlreadySeenWitnessCandidates;

void static GuldenWitness()
{
    LogPrintf("GuldenWitness started\n");
    RenameThread("gulden-witness");

    static bool hashCity = IsArgSet("-testnet") ? ( GetArg("-testnet", "")[0] == 'C' ? true : false ) : false;
    static bool regTest = GetBoolArg("-regtest", false);

    CChainParams chainparams = Params();
    try
    {
        while (true)
        {
            if (!regTest && !hashCity)
            {
                // Busy-wait for the network to come online so we don't waste time mining
                // on an obsolete chain. In regtest mode we expect to fly solo.
                do {
                    if (g_connman->GetNodeCount(CConnman::CONNECTIONS_ALL) > 0 && !IsInitialBlockDownload())
                        break;
                    MilliSleep(1000);
                } while (true);
            }


            CBlockIndex* pindexTip = chainActive.Tip();
            Consensus::Params pParams = chainparams.GetConsensus();

            //We can only start witnessing from phase 3 onward.
            if ( !pindexTip || !pindexTip->pprev || !IsPow2WitnessingActive(pindexTip, chainparams)  )
            {
                MilliSleep(5000);
                continue;
            }
            int nPoW2PhaseTip = GetPoW2Phase(pindexTip, chainparams);

            //fixme: (GULDEN) (PoW2) (HIGH) Shorter sleep here?
            //Or ideally instead of just sleeping/busy polling rather wait on a signal that gets triggered only when new blocks come in??
            MilliSleep(100);

            // Check for stop or if block needs to be rebuilt
            boost::this_thread::interruption_point();

            int nWitnessHeight = pindexTip->nHeight + 1;

            // If we already have a witnessed block at the tip don't bother looking at any orphans, just patiently wait for next unsigned tip.
            if (nPoW2PhaseTip <= 3 && pindexTip->nVersionPoW2Witness != 0)
                continue;

            // Use a cache to prevent trying the same blocks over and over.
            std::vector<CBlockIndex*> candidateOrphans;
            for (const auto candidateIter : GetTopLevelPoWOrphans(nWitnessHeight, *(pindexTip->phashBlock)))
            {
                if (cacheAlreadySeenWitnessCandidates.find(candidateIter) == cacheAlreadySeenWitnessCandidates.end())
                {
                    cacheAlreadySeenWitnessCandidates.insert(candidateIter);
                    candidateOrphans.push_back(candidateIter);
                }
            }
            if (cacheAlreadySeenWitnessCandidates.size() > 50)
            {
                auto eraseEnd = cacheAlreadySeenWitnessCandidates.begin();
                std::advance(eraseEnd, cacheAlreadySeenWitnessCandidates.size() - 50);
                cacheAlreadySeenWitnessCandidates.erase(cacheAlreadySeenWitnessCandidates.begin(), eraseEnd);
            }

            if (candidateOrphans.size() > 0)
            {
                LOCK(processBlockCS);

                if (chainActive.Tip() != pindexTip)
                {
                    continue;
                }

                // Look for all potential signable blocks at same height as the index tip - don't limit ourselves to just the tip
                // This is important because otherwise the chain can stall if there is an absent signer for the current tip.
                for (const auto candidateIter : candidateOrphans)
                {
                    //Create new block
                    std::shared_ptr<CBlock> pWitnessBlock(new CBlock);
                    if (ReadBlockFromDisk(*pWitnessBlock, candidateIter, pParams))
                    {
                        CTxOut selectedWitnessOutput;
                        COutPoint selectedWitnessOutPoint;
                        unsigned int nSelectedWitnessBlockHeight;
                        GetWitness(chainActive, pindexTip, *pWitnessBlock, chainparams, selectedWitnessOutput, selectedWitnessOutPoint, nSelectedWitnessBlockHeight, nullptr);

                        CAmount witnessSubsidy = GetBlockSubsidyWitness(nWitnessHeight, pParams);

                        //fixme: (GULDEN) (2.0) (POW2) (ISMINE_WITNESS)
                        if (pactiveWallet->IsMine(selectedWitnessOutput) == ISMINE_SPENDABLE)
                        {
                            CAccount* selectedWitnessAccount = pactiveWallet->FindAccountForTransaction(selectedWitnessOutput);
                            if (selectedWitnessAccount)
                            {
                                //We must do this before we add the blank coinbase otherwise GetBlockWeight crashes on a NULL pointer dereference.
                                int nStartingBlockWeight = GetBlockWeight(*pWitnessBlock);

                                /** First we add the new witness coinbase to the block, this acts as a seperator between transactions from the initial mined block and the witness block **/
                                /** We add a placeholder for now as we don't know the fees we will generate **/
                                pWitnessBlock->vtx.emplace_back();
                                int nWitnessCoinbaseIndex = pWitnessBlock->vtx.size()-1;
                                nStartingBlockWeight += 200;

                                std::shared_ptr<CReserveScript> coinbaseScript;
                                //fixme: (2.0) (SEGSIG)
                                GetMainSignals().ScriptForMining(coinbaseScript, selectedWitnessAccount);

                                /** Now add any additional transactions if there is space left **/
                                if (GetPoW2Phase(pindexTip, chainparams) >= 4)
                                {
                                    // Piggy back off existing block assembler code to grab the transactions we want to include.
                                    // Setup maximum size for assembler so that size of existing (PoW) block transactions are subtracted from overall maximum.
                                    BlockAssembler::Options assemblerOptions;
                                    assemblerOptions.nBlockMaxWeight = DEFAULT_BLOCK_MAX_WEIGHT - nStartingBlockWeight;
                                    assemblerOptions.nBlockMaxSize = assemblerOptions.nBlockMaxWeight;

                                    std::unique_ptr<CBlockTemplate> pblocktemplate(BlockAssembler(Params(), assemblerOptions).CreateNewBlock(coinbaseScript->reserveScript));
                                    if (!pblocktemplate.get())
                                    {
                                        LogPrintf("Error in GuldenWitness: Keypool ran out, please call keypoolrefill before restarting the mining thread\n");
                                        continue;
                                    }

                                    // Skip the coinbase as we obviously don't want this included again, it is already in the PoW part of the block.
                                    size_t nSkipCoinbase = 1;
                                    if (GetPoW2Phase(pindexTip->pprev, chainparams) == 3)
                                        nSkipCoinbase = 2;
                                    //fixme: (GULDEN) (CBSU)? pre-allocate for vtx.size().
                                    for (size_t i=nSkipCoinbase; i < pblocktemplate->block.vtx.size(); ++i)
                                    {
                                        //fixme: (GULDEN) (CBSU)? emplace_back?
                                        pWitnessBlock->vtx.push_back(pblocktemplate->block.vtx[i]);
                                    }

                                    //testme: (GULDEN) (2.0) (HIGH) test this is right.
                                    witnessSubsidy += (-pblocktemplate->vTxFees[0]);
                                }


                                //fixme: (GULDEN) (2.0) (SEGSIG) - Implement new transaction types here?
                                /** Populate witness coinbase placeholder with real information now that we have it **/
                                CMutableTransaction coinbaseTx = CreateWitnessCoinbase(nWitnessHeight, GetPoW2Phase(pindexTip->pprev, chainparams), GetPoW2Phase(pindexTip, chainparams), coinbaseScript, witnessSubsidy, selectedWitnessOutput, selectedWitnessOutPoint, nSelectedWitnessBlockHeight, selectedWitnessAccount);
                                pWitnessBlock->vtx[nWitnessCoinbaseIndex] = MakeTransactionRef(std::move(coinbaseTx));


                                /** Set witness specific block header information **/
                                //testme: (GULDEN) (POW2) (2.0)
                                {
                                    // ComputeBlockVersion returns the right version flag to signal for phase 4 activation here, assuming we are already in phase 3 and 95 percent of peers are upgraded.
                                    pWitnessBlock->nVersionPoW2Witness = ComputeBlockVersion(pindexTip, pParams);


                                    // Second witness timestamp gets added to the block as an additional time source to the miner timestamp.
                                    // Witness timestamp must exceed median of previous mined timestamps.
                                    pWitnessBlock->nTimePoW2Witness = std::max(candidateIter->GetMedianTimePastPoW()+1, GetAdjustedTime());


                                    // Set witness merkle hash.
                                    pWitnessBlock->hashMerkleRootPoW2Witness = BlockMerkleRoot(pWitnessBlock->vtx.begin()+nWitnessCoinbaseIndex, pWitnessBlock->vtx.end());
                                }


                                /** Do the witness operation (Sign the block using our witness key) and broadcast the final product to the network. **/
                                if (SignBlockAsWitness(pWitnessBlock, selectedWitnessOutput))
                                {
                                    LogPrintf("GuldenWitness: witness found %s", pWitnessBlock->GetHashPoW2().ToString());
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
        LogPrintf("GuldenWitness terminated\n");
        throw;
    }
    catch (const std::runtime_error &e)
    {
        LogPrintf("GuldenWitness runtime error: %s\n", e.what());
        return;
    }
}


void StartPoW2WitnessThread(boost::thread_group& threadGroup)
{
    threadGroup.create_thread(boost::bind(&TraceThread<void (*)()>, "pow2_witness", &GuldenWitness));
}
