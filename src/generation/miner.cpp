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
#include "generation.h"

#include "net.h"

#include "alert.h"
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
#include "validation/validation.h"
#include "validation/witnessvalidation.h"
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
#include "validation/validationinterface.h"

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


static bool InsertPoW2WitnessIntoCoinbase(CBlock& block, const CBlockIndex* pindexPrev, const CChainParams& params, CBlockIndex* pWitnessBlockToEmbed, int nParentPoW2Phase, std::vector<unsigned char>& witnessCoinbaseHex, std::vector<unsigned char>& witnessSubsidyHex)
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
            // For the benefit of GetBlockTemplate
            CVectorWriter serialisedCoinbase(SER_NETWORK, INIT_PROTO_VERSION,witnessCoinbaseHex, 0);
            out.WriteToStream(serialisedCoinbase, CTransaction::CURRENT_VERSION);
            CVectorWriter serialisedWitnessSubsidy(SER_NETWORK, INIT_PROTO_VERSION,witnessSubsidyHex, 0);
            pWitnessBlock->vtx[nWitnessCoinbasePos]->vout[1].WriteToStream(serialisedWitnessSubsidy, CTransaction::CURRENT_VERSION);

            // For the actual coinbase
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

std::unique_ptr<CBlockTemplate> BlockAssembler::CreateNewBlock(CBlockIndex* pParent, std::shared_ptr<CReserveKeyOrScript> coinbaseReservedKey, bool fMineSegSig, CBlockIndex* pWitnessBlockToEmbed, bool noValidityCheck, std::vector<unsigned char>* pWitnessCoinbaseHex, std::vector<unsigned char>* pWitnessSubsidyHex, CAmount* pAmountPoW2Subsidy)
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

    nHeight = pParent->nHeight + 1;

    int nParentPoW2Phase = GetPoW2Phase(pParent, chainparams, chainActive);
    int nGrandParentPoW2Phase = GetPoW2Phase(pParent->pprev, chainparams, chainActive);
    bool bSegSigIsEnabled = IsSegSigEnabled(pParent);

    //Until PoW2 activates mining subsidy remains full, after it activates PoW part of subsidy is reduced.
    //fixme: (2.1) (CLEANUP) - We can remove this after 2.1 becomes active.
    Consensus::Params consensusParams = chainparams.GetConsensus();
    CAmount nSubsidy = GetBlockSubsidy(nHeight);
    CAmount nSubsidyWitness = 0;
    if (nParentPoW2Phase >= 3)
    {
        nSubsidyWitness = GetBlockSubsidyWitness(nHeight);
        if (pAmountPoW2Subsidy)
            *pAmountPoW2Subsidy = nSubsidyWitness;
        nSubsidy -= nSubsidyWitness;
    }


    // First 'active' block of phase 4 (first block with a phase 4 parent) contains two witness subsidies so miner loses out on 20 NLG for this block
    // This block is treated special. (but special casing can dissapear for 2.1 release.
    if (nGrandParentPoW2Phase == 3 && nParentPoW2Phase == 4)
        nSubsidy -= GetBlockSubsidyWitness(nHeight);

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

    int nPackagesSelected = 0;
    int nDescendantsUpdated = 0;
    {
        LOCK(mempool.cs);

        // Decide whether to include segsig signature information
        // This is only needed in case the segsig signature activation is reverted
        // (which would require a very deep reorganization) or when
        // -promiscuousmempoolflags is used.
        // TODO: replace this with a call to main to assess validity of a mempool
        // transaction (which in most cases can be a no-op).
        fIncludeSegSig = IsSegSigEnabled(pParent) && fMineSegSig;
        addPackageTxs(nPackagesSelected, nDescendantsUpdated);
    }

    int64_t nTime1 = GetTimeMicros();

    nLastBlockTx = nBlockTx;
    nLastBlockSize = nBlockSize;
    nLastBlockWeight = nBlockWeight;

    // Create coinbase transaction.
    CMutableTransaction coinbaseTx( bSegSigIsEnabled ? CTransaction::SEGSIG_ACTIVATION_VERSION : CTransaction::CURRENT_VERSION );
    coinbaseTx.vin.resize(1);
    coinbaseTx.vin[0].prevout.SetNull();
    coinbaseTx.vout.resize(1);
    if (bSegSigIsEnabled && !coinbaseReservedKey->scriptOnly())
    {
        coinbaseTx.vout[0].SetType(CTxOutType::StandardKeyHashOutput);
        CPubKey addressPubKey;
        if (!coinbaseReservedKey->GetReservedKey(addressPubKey))
        {
            LogPrintf("Error in CreateNewBlock: could not retrieve public key for output address.\n");
            return nullptr;
        }
        coinbaseTx.vout[0].output.standardKeyHash = CTxOutStandardKeyHash(addressPubKey.GetID());
    }
    else
    {
        coinbaseTx.vout[0].SetType(CTxOutType::ScriptLegacyOutput);
        coinbaseTx.vout[0].output.scriptPubKey = coinbaseReservedKey->reserveScript;
    }
    coinbaseTx.vout[0].nValue = nFees + nSubsidy;

    // Insert the height into the coinbase (to ensure all coinbase transactions have a unique hash)
    // Further, also insert any optional 'signature' data (identifier of miner or other private miner data etc.)
    std::string coinbaseSignature = GetArg("-coinbasesignature", "");
    if (bSegSigIsEnabled)
    {
        coinbaseTx.vin[0].segregatedSignatureData.stack.clear();
        coinbaseTx.vin[0].segregatedSignatureData.stack.push_back(std::vector<unsigned char>());
        CVectorWriter(0, 0, coinbaseTx.vin[0].segregatedSignatureData.stack[0], 0) << VARINT(nHeight);
        coinbaseTx.vin[0].segregatedSignatureData.stack.push_back(std::vector<unsigned char>(coinbaseSignature.begin(), coinbaseSignature.end()));
    }
    else
    {
        coinbaseTx.vin[0].scriptSig = CScript() << nHeight << OP_0 << std::vector<unsigned char>(coinbaseSignature.begin(), coinbaseSignature.end());
    }

    pblock->vtx[0] = MakeTransactionRef(std::move(coinbaseTx));
    pblocktemplate->vTxFees[0] = -nFees;

    // From second 'active' phase 3 block (second block whose parent is phase 3) - embed the PoW2 witness. First doesn't have a witness to embed.
    if (nGrandParentPoW2Phase == 3)
    {
        if (pWitnessBlockToEmbed)
        {
            //NB! Modifies block version so must be called *after* ComputeBlockVersion and not before.
            std::vector<unsigned char> witnessCoinbaseHex;
            std::vector<unsigned char> witnessSubsidyHex;
            if (!InsertPoW2WitnessIntoCoinbase(*pblock, pParent, chainparams, pWitnessBlockToEmbed, nParentPoW2Phase, witnessCoinbaseHex, witnessSubsidyHex))
                return nullptr;
            if (pWitnessCoinbaseHex)
                *pWitnessCoinbaseHex = witnessCoinbaseHex;
            if (pWitnessSubsidyHex)
                *pWitnessSubsidyHex = witnessSubsidyHex;
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
    pblocktemplate->vTxSigOpsCost[0] = GetLegacySigOpCount(*pblock->vtx[0]);

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
        if (!TestBlockValidity(chainActive, state, chainparams, *pblock, pParent, false, false, nullptr))
        {
            LogPrintf("%s: TestBlockValidity failed: %s", __func__, FormatStateMessage(state));
            return nullptr;
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
        if (!CheckTransactionContextual(it->GetTx(), state, nHeight, nullptr))
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

static void SHA256Transform(void* pstate, void* pinput, const void* pinit)
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
    int nPoW2PhaseTip = GetPoW2Phase(chainActive.Tip(), chainparams, chainActive);
    int nPoW2PhasePrev = GetPoW2Phase(chainActive.Tip()->pprev, chainparams, chainActive);
    LogPrintf("%s\n", pblock->ToString());
    LogPrintf("generated hash= %s hashpow2= %s  amt= %s [PoW2 phase: tip=%d tipprevious=%d]\n", pblock->GetPoWHash().ToString(), pblock->GetHashPoW2().ToString(), FormatMoney(pblock->vtx[0]->vout[0].nValue), nPoW2PhaseTip, nPoW2PhasePrev);

    //fixme: (2.1) we should avoid submitting stale blocks here
    //but only if they are really stale (there are cases where we want to mine not on the tip (stalled chain)
    //so detecting this is a bit tricky...

    // Found a solution
    // Process this block the same as if we had received it from another node
    if (!ProcessNewBlock(chainparams, pblock, true, NULL, false, true))
        return error("GuldenMiner: ProcessNewBlock, block not accepted");

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
    int nPoW2PhaseGreatGrandParent = pIndexParent->pprev && pIndexParent->pprev->pprev ? GetPoW2Phase(pIndexParent->pprev->pprev, Params(), chainActive) : 1;
    int nPoW2PhaseGrandParent = pIndexParent->pprev ? GetPoW2Phase(pIndexParent->pprev, Params(), chainActive) : 1;
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
                    strError = "Error in GuldenMiner: mining stalled, unable to read the witness block we intend to embed.";
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

                        //We don't have it in our index - try extract it from the tip in which we have already embedded it.
                        if (!pWitnessBlockToEmbed)
                        {
                            std::shared_ptr<CBlock> pBlockPoWParent(new CBlock);
                            LOCK(cs_main); // For ReadBlockFromDisk
                            if (ReadBlockFromDisk(*pBlockPoWParent.get(), pIndexParent, Params()))
                            {
                                int nWitnessCoinbaseIndex = GetPoW2WitnessCoinbaseIndex(*pBlockPoWParent.get());
                                if (nWitnessCoinbaseIndex != -1)
                                {
                                    std::shared_ptr<CBlock> embeddedWitnessBlock(new CBlock);
                                    if (ExtractWitnessBlockFromWitnessCoinbase(chainActive, nWitnessCoinbaseIndex, pIndexParent->pprev, *pBlockPoWParent.get(), chainparams, *pcoinsTip, *embeddedWitnessBlock.get()))
                                    {
                                        uint256 hashPoW2Witness = embeddedWitnessBlock->GetHashPoW2();
                                        if (mapBlockIndex.count(hashPoW2Witness) > 0)
                                        {
                                            pWitnessBlockToEmbed = mapBlockIndex[hashPoW2Witness];
                                            break;
                                        }
                                        else
                                        {
                                            if (ProcessNewBlock(Params(), embeddedWitnessBlock, true, nullptr, false, true))
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
                            pIndexParent = pIndexParent->pprev;
                            continue;
                        }
                    }
                    if (!pWitnessBlockToEmbed)
                    {
                        strError = "Error in GuldenMiner: mining stalled, unable to locate suitable witness block to embed.\n";
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
            strError = "Error in GuldenMiner: mining stalled, unable to locate suitable witness tip on which to build.\n";
            return nullptr;
        }
    }

    return pIndexParent;
}

double dBestHashesPerSec = 0.0;
double dHashesPerSec = 0.0;
int64_t nHPSTimerStart = 0;
int64_t nHashCounter=0;
std::atomic<int64_t> nHashThrottle(-1);
static CCriticalSection timerCS;

static const unsigned int hashPSTimerInterval = 200;
void static GuldenMiner(const CChainParams& chainparams)
{
    LogPrintf("GuldenMiner started\n");
    RenameThread("gulden-miner");

    int64_t nUpdateTimeStart = GetTimeMillis();

    static bool hashCity = IsArgSet("-testnet") ? ( GetArg("-testnet", "")[0] == 'C' ? true : false ) : false;
    static bool regTest = GetBoolArg("-regtest", false);

    unsigned int nExtraNonce = 0;
    std::shared_ptr<CReserveKeyOrScript> coinbaseScript;
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
        //fixme: (2.1) - We should allow for an empty reserveScript with only the key as script is technically no longer essential...
        if (!coinbaseScript || coinbaseScript->reserveScript.empty())
            throw std::runtime_error("No coinbase script available (mining requires a wallet)");


        while (true)
        {
            if (!regTest && !hashCity)
            {
                // Busy-wait for the network to come online so we don't waste time mining on an obsolete chain. In regtest mode we expect to fly solo.
                while (true)
                {
                    if (g_connman->GetNodeCount(CConnman::CONNECTIONS_ALL) > 0)
                    {
                        if (IsArgSet("-testnet") || !IsInitialBlockDownload())
                        {
                            break;
                        }
                    }
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

                pblocktemplate = BlockAssembler(Params()).CreateNewBlock(pindexParent, coinbaseScript, true, pWitnessBlockToEmbed);
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

