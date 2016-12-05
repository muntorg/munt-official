// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// File contains modifications by: The Gulden developers
// All modifications:
// Copyright (c) 2016 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#include "miner.h"

#include "amount.h"
#include "chain.h"
#include "chainparams.h"
#include "coins.h"
#include "consensus/consensus.h"
#include "consensus/merkle.h"
#include "consensus/validation.h"
#include "hash.h"
#include "main.h"
#include "net.h"
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
#include <boost/thread.hpp>
#include <boost/tuple/tuple.hpp>
#include <queue>

#include <Gulden/Common/diff.h>
#include <Gulden/Common/hash/hash.h>
#include <openssl/sha.h>

using namespace std;

uint64_t nLastBlockTx = 0;
uint64_t nLastBlockSize = 0;
uint64_t nLastBlockWeight = 0;

class ScoreCompare {
public:
    ScoreCompare() {}

    bool operator()(const CTxMemPool::txiter a, const CTxMemPool::txiter b)
    {
        return CompareTxMemPoolEntryByScore()(*b, *a); // Convert to less than
    }
};

int64_t UpdateTime(CBlockHeader* pblock, const Consensus::Params& consensusParams, const CBlockIndex* pindexPrev)
{
    int64_t nOldTime = pblock->nTime;
    int64_t nNewTime = std::max(pindexPrev->GetMedianTimePast(pindexPrev->nHeight) + 1, GetAdjustedTime());

    if (nOldTime < nNewTime)
        pblock->nTime = nNewTime;

    if (consensusParams.fPowAllowMinDifficultyBlocks)
        pblock->nBits = GetNextWorkRequired(pindexPrev, pblock, consensusParams);

    return nNewTime - nOldTime;
}

BlockAssembler::BlockAssembler(const CChainParams& _chainparams)
    : chainparams(_chainparams)
{

    nBlockMaxWeight = DEFAULT_BLOCK_MAX_WEIGHT;
    nBlockMaxSize = DEFAULT_BLOCK_MAX_SIZE;
    bool fWeightSet = false;
    if (mapArgs.count("-blockmaxweight")) {
        nBlockMaxWeight = GetArg("-blockmaxweight", DEFAULT_BLOCK_MAX_WEIGHT);
        nBlockMaxSize = MAX_BLOCK_SERIALIZED_SIZE;
        fWeightSet = true;
    }
    if (mapArgs.count("-blockmaxsize")) {
        nBlockMaxSize = GetArg("-blockmaxsize", DEFAULT_BLOCK_MAX_SIZE);
        if (!fWeightSet) {
            nBlockMaxWeight = nBlockMaxSize * WITNESS_SCALE_FACTOR;
        }
    }

    nBlockMaxWeight = std::max((unsigned int)4000, std::min((unsigned int)(MAX_BLOCK_WEIGHT - 4000), nBlockMaxWeight));

    nBlockMaxSize = std::max((unsigned int)1000, std::min((unsigned int)(MAX_BLOCK_SERIALIZED_SIZE - 1000), nBlockMaxSize));

    fNeedSizeAccounting = (nBlockMaxSize < MAX_BLOCK_SERIALIZED_SIZE - 1000);
}

void BlockAssembler::resetBlock()
{
    inBlock.clear();

    nBlockSize = 1000;
    nBlockWeight = 4000;
    nBlockSigOpsCost = 400;
    fIncludeWitness = false;

    nBlockTx = 0;
    nFees = 0;

    lastFewTxs = 0;
    blockFinished = false;
}

CBlockTemplate* BlockAssembler::CreateNewBlock(const CScript& scriptPubKeyIn)
{
    resetBlock();

    pblocktemplate.reset(new CBlockTemplate());

    if (!pblocktemplate.get())
        return NULL;
    pblock = &pblocktemplate->block; // pointer for convenience

    pblock->vtx.push_back(CTransaction());
    pblocktemplate->vTxFees.push_back(-1); // updated at end
    pblocktemplate->vTxSigOpsCost.push_back(-1); // updated at end

    LOCK2(cs_main, mempool.cs);
    CBlockIndex* pindexPrev = chainActive.Tip();
    nHeight = pindexPrev->nHeight + 1;

    pblock->nVersion = ComputeBlockVersion(pindexPrev, chainparams.GetConsensus());

    if (chainparams.MineBlocksOnDemand())
        pblock->nVersion = GetArg("-blockversion", pblock->nVersion);

    pblock->nTime = GetAdjustedTime();
    const int64_t nMedianTimePast = pindexPrev->GetMedianTimePast(pindexPrev->nHeight);

    nLockTimeCutoff = (STANDARD_LOCKTIME_VERIFY_FLAGS & LOCKTIME_MEDIAN_TIME_PAST)
                          ? nMedianTimePast
                          : pblock->GetBlockTime();

    fIncludeWitness = IsWitnessEnabled(pindexPrev, chainparams.GetConsensus());

    addPriorityTxs();
    addPackageTxs();

    nLastBlockTx = nBlockTx;
    nLastBlockSize = nBlockSize;
    nLastBlockWeight = nBlockWeight;
    LogPrintf("CreateNewBlock(): total size %u txs: %u fees: %ld sigops %d\n", nBlockSize, nBlockTx, nFees, nBlockSigOpsCost);

    CMutableTransaction coinbaseTx;
    coinbaseTx.vin.resize(1);
    coinbaseTx.vin[0].prevout.SetNull();
    coinbaseTx.vout.resize(1);
    coinbaseTx.vout[0].scriptPubKey = scriptPubKeyIn;
    coinbaseTx.vout[0].nValue = nFees + GetBlockSubsidy(nHeight, chainparams.GetConsensus());
    coinbaseTx.vin[0].scriptSig = CScript() << nHeight << OP_0;
    pblock->vtx[0] = coinbaseTx;
    pblocktemplate->vchCoinbaseCommitment = GenerateCoinbaseCommitment(*pblock, pindexPrev, chainparams.GetConsensus());
    pblocktemplate->vTxFees[0] = -nFees;

    pblock->hashPrevBlock = pindexPrev->GetBlockHash();
    UpdateTime(pblock, chainparams.GetConsensus(), pindexPrev);
    pblock->nBits = GetNextWorkRequired(pindexPrev, pblock, chainparams.GetConsensus());
    pblock->nNonce = 0;
    pblocktemplate->vTxSigOpsCost[0] = WITNESS_SCALE_FACTOR * GetLegacySigOpCount(pblock->vtx[0]);

    CValidationState state;
    if (!TestBlockValidity(state, chainparams, *pblock, pindexPrev, false, false)) {
        throw std::runtime_error(strprintf("%s: TestBlockValidity failed: %s", __func__, FormatStateMessage(state)));
    }

    return pblocktemplate.release();
}

bool BlockAssembler::isStillDependent(CTxMemPool::txiter iter)
{
    BOOST_FOREACH (CTxMemPool::txiter parent, mempool.GetMemPoolParents(iter)) {
        if (!inBlock.count(parent)) {
            return true;
        }
    }
    return false;
}

void BlockAssembler::onlyUnconfirmed(CTxMemPool::setEntries& testSet)
{
    for (CTxMemPool::setEntries::iterator iit = testSet.begin(); iit != testSet.end();) {

        if (inBlock.count(*iit)) {
            testSet.erase(iit++);
        } else {
            iit++;
        }
    }
}

bool BlockAssembler::TestPackage(uint64_t packageSize, int64_t packageSigOpsCost)
{

    if (nBlockWeight + WITNESS_SCALE_FACTOR * packageSize >= nBlockMaxWeight)
        return false;
    if (nBlockSigOpsCost + packageSigOpsCost >= MAX_BLOCK_SIGOPS_COST)
        return false;
    return true;
}

bool BlockAssembler::TestPackageTransactions(const CTxMemPool::setEntries& package)
{
    uint64_t nPotentialBlockSize = nBlockSize; // only used with fNeedSizeAccounting
    BOOST_FOREACH (const CTxMemPool::txiter it, package) {
        if (!IsFinalTx(it->GetTx(), nHeight, nLockTimeCutoff))
            return false;
        if (!fIncludeWitness && !it->GetTx().wit.IsNull())
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

bool BlockAssembler::TestForBlock(CTxMemPool::txiter iter)
{
    if (nBlockWeight + iter->GetTxWeight() >= nBlockMaxWeight) {

        if (nBlockWeight > nBlockMaxWeight - 400 || lastFewTxs > 50) {
            blockFinished = true;
            return false;
        }

        if (nBlockWeight > nBlockMaxWeight - 4000) {
            lastFewTxs++;
        }
        return false;
    }

    if (fNeedSizeAccounting) {
        if (nBlockSize + ::GetSerializeSize(iter->GetTx(), SER_NETWORK, PROTOCOL_VERSION) >= nBlockMaxSize) {
            if (nBlockSize > nBlockMaxSize - 100 || lastFewTxs > 50) {
                blockFinished = true;
                return false;
            }
            if (nBlockSize > nBlockMaxSize - 1000) {
                lastFewTxs++;
            }
            return false;
        }
    }

    if (nBlockSigOpsCost + iter->GetSigOpCost() >= MAX_BLOCK_SIGOPS_COST) {

        if (nBlockSigOpsCost > MAX_BLOCK_SIGOPS_COST - 8) {
            blockFinished = true;
            return false;
        }

        return false;
    }

    if (!IsFinalTx(iter->GetTx(), nHeight, nLockTimeCutoff))
        return false;

    return true;
}

void BlockAssembler::AddToBlock(CTxMemPool::txiter iter)
{
    pblock->vtx.push_back(iter->GetTx());
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
        double dPriority = iter->GetPriority(nHeight);
        CAmount dummy;
        mempool.ApplyDeltas(iter->GetTx().GetHash(), dPriority, dummy);
        LogPrintf("priority %.1f fee %s txid %s\n",
                  dPriority,
                  CFeeRate(iter->GetModifiedFee(), iter->GetTxSize()).ToString(),
                  iter->GetTx().GetHash().ToString());
    }
}

void BlockAssembler::UpdatePackagesForAdded(const CTxMemPool::setEntries& alreadyAdded,
                                            indexed_modified_transaction_set& mapModifiedTx)
{
    BOOST_FOREACH (const CTxMemPool::txiter it, alreadyAdded) {
        CTxMemPool::setEntries descendants;
        mempool.CalculateDescendants(it, descendants);

        BOOST_FOREACH (CTxMemPool::txiter desc, descendants) {
            if (alreadyAdded.count(desc))
                continue;
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
}

bool BlockAssembler::SkipMapTxEntry(CTxMemPool::txiter it, indexed_modified_transaction_set& mapModifiedTx, CTxMemPool::setEntries& failedTx)
{
    assert(it != mempool.mapTx.end());
    if (mapModifiedTx.count(it) || inBlock.count(it) || failedTx.count(it))
        return true;
    return false;
}

void BlockAssembler::SortForBlock(const CTxMemPool::setEntries& package, CTxMemPool::txiter entry, std::vector<CTxMemPool::txiter>& sortedEntries)
{

    sortedEntries.clear();
    sortedEntries.insert(sortedEntries.begin(), package.begin(), package.end());
    std::sort(sortedEntries.begin(), sortedEntries.end(), CompareTxIterByAncestorCount());
}

void BlockAssembler::addPackageTxs()
{

    indexed_modified_transaction_set mapModifiedTx;

    CTxMemPool::setEntries failedTx;

    UpdatePackagesForAdded(inBlock, mapModifiedTx);

    CTxMemPool::indexed_transaction_set::index<ancestor_score>::type::iterator mi = mempool.mapTx.get<ancestor_score>().begin();
    CTxMemPool::txiter iter;
    while (mi != mempool.mapTx.get<ancestor_score>().end() || !mapModifiedTx.empty()) {

        if (mi != mempool.mapTx.get<ancestor_score>().end() && SkipMapTxEntry(mempool.mapTx.project<0>(mi), mapModifiedTx, failedTx)) {
            ++mi;
            continue;
        }

        bool fUsingModified = false;

        modtxscoreiter modit = mapModifiedTx.get<ancestor_score>().begin();
        if (mi == mempool.mapTx.get<ancestor_score>().end()) {

            iter = modit->iter;
            fUsingModified = true;
        } else {

            iter = mempool.mapTx.project<0>(mi);
            if (modit != mapModifiedTx.get<ancestor_score>().end() && CompareModifiedEntry()(*modit, CTxMemPoolModifiedEntry(iter))) {

                iter = modit->iter;
                fUsingModified = true;
            } else {

                ++mi;
            }
        }

        assert(!inBlock.count(iter));

        uint64_t packageSize = iter->GetSizeWithAncestors();
        CAmount packageFees = iter->GetModFeesWithAncestors();
        int64_t packageSigOpsCost = iter->GetSigOpCostWithAncestors();
        if (fUsingModified) {
            packageSize = modit->nSizeWithAncestors;
            packageFees = modit->nModFeesWithAncestors;
            packageSigOpsCost = modit->nSigOpCostWithAncestors;
        }

        if (packageFees < ::minRelayTxFee.GetFee(packageSize)) {

            return;
        }

        if (!TestPackage(packageSize, packageSigOpsCost)) {
            if (fUsingModified) {

                mapModifiedTx.get<ancestor_score>().erase(modit);
                failedTx.insert(iter);
            }
            continue;
        }

        CTxMemPool::setEntries ancestors;
        uint64_t nNoLimit = std::numeric_limits<uint64_t>::max();
        std::string dummy;
        mempool.CalculateMemPoolAncestors(*iter, ancestors, nNoLimit, nNoLimit, nNoLimit, nNoLimit, dummy, false);

        onlyUnconfirmed(ancestors);
        ancestors.insert(iter);

        if (!TestPackageTransactions(ancestors)) {
            if (fUsingModified) {
                mapModifiedTx.get<ancestor_score>().erase(modit);
                failedTx.insert(iter);
            }
            continue;
        }

        vector<CTxMemPool::txiter> sortedEntries;
        SortForBlock(ancestors, iter, sortedEntries);

        for (size_t i = 0; i < sortedEntries.size(); ++i) {
            AddToBlock(sortedEntries[i]);

            mapModifiedTx.erase(sortedEntries[i]);
        }

        UpdatePackagesForAdded(ancestors, mapModifiedTx);
    }
}

void BlockAssembler::addPriorityTxs()
{

    unsigned int nBlockPrioritySize = GetArg("-blockprioritysize", DEFAULT_BLOCK_PRIORITY_SIZE);
    nBlockPrioritySize = std::min(nBlockMaxSize, nBlockPrioritySize);

    if (nBlockPrioritySize == 0) {
        return;
    }

    bool fSizeAccounting = fNeedSizeAccounting;
    fNeedSizeAccounting = true;

    vector<TxCoinAgePriority> vecPriority;
    TxCoinAgePriorityCompare pricomparer;
    std::map<CTxMemPool::txiter, double, CTxMemPool::CompareIteratorByHash> waitPriMap;
    typedef std::map<CTxMemPool::txiter, double, CTxMemPool::CompareIteratorByHash>::iterator waitPriIter;
    double actualPriority = -1;

    vecPriority.reserve(mempool.mapTx.size());
    for (CTxMemPool::indexed_transaction_set::iterator mi = mempool.mapTx.begin();
         mi != mempool.mapTx.end(); ++mi) {
        double dPriority = mi->GetPriority(nHeight);
        CAmount dummy;
        mempool.ApplyDeltas(mi->GetTx().GetHash(), dPriority, dummy);
        vecPriority.push_back(TxCoinAgePriority(dPriority, mi));
    }
    std::make_heap(vecPriority.begin(), vecPriority.end(), pricomparer);

    CTxMemPool::txiter iter;
    while (!vecPriority.empty() && !blockFinished) { // add a tx from priority queue to fill the blockprioritysize
        iter = vecPriority.front().second;
        actualPriority = vecPriority.front().first;
        std::pop_heap(vecPriority.begin(), vecPriority.end(), pricomparer);
        vecPriority.pop_back();

        if (inBlock.count(iter)) {
            assert(false); // shouldn't happen for priority txs
            continue;
        }

        if (!fIncludeWitness && !iter->GetTx().wit.IsNull())
            continue;

        if (isStillDependent(iter)) {
            waitPriMap.insert(std::make_pair(iter, actualPriority));
            continue;
        }

        if (TestForBlock(iter)) {
            AddToBlock(iter);

            if (nBlockSize >= nBlockPrioritySize || !AllowFree(actualPriority)) {
                break;
            }

            BOOST_FOREACH (CTxMemPool::txiter child, mempool.GetMemPoolChildren(iter)) {
                waitPriIter wpiter = waitPriMap.find(child);
                if (wpiter != waitPriMap.end()) {
                    vecPriority.push_back(TxCoinAgePriority(wpiter->second, child));
                    std::push_heap(vecPriority.begin(), vecPriority.end(), pricomparer);
                    waitPriMap.erase(wpiter);
                }
            }
        }
    }
    fNeedSizeAccounting = fSizeAccounting;
}

void IncrementExtraNonce(CBlock* pblock, const CBlockIndex* pindexPrev, unsigned int& nExtraNonce)
{

    static uint256 hashPrevBlock;
    if (hashPrevBlock != pblock->hashPrevBlock) {
        nExtraNonce = 0;
        hashPrevBlock = pblock->hashPrevBlock;
    }
    ++nExtraNonce;
    unsigned int nHeight = pindexPrev->nHeight + 1; // Height first in coinbase required for block.version=2
    CMutableTransaction txCoinbase(pblock->vtx[0]);
    txCoinbase.vin[0].scriptSig = (CScript() << nHeight << CScriptNum(nExtraNonce)) + COINBASE_FLAGS;
    assert(txCoinbase.vin[0].scriptSig.size() <= 100);

    pblock->vtx[0] = txCoinbase;
    pblock->hashMerkleRoot = BlockMerkleRoot(*pblock);
}

static const unsigned int pSHA256InitState[8] = { 0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19 };

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

    struct
    {
        struct unnamed2 {
            int nVersion;
            uint256 hashPrevBlock;
            uint256 hashMerkleRoot;
            unsigned int nTime;
            unsigned int nBits;
            unsigned int nNonce;
        } block;
        unsigned char pchPadding0[64];
        uint256 hash1;
        unsigned char pchPadding1[64];
    } tmp;
    memset(&tmp, 0, sizeof(tmp));

    tmp.block.nVersion = pblock->nVersion;
    tmp.block.hashPrevBlock = pblock->hashPrevBlock;
    tmp.block.hashMerkleRoot = pblock->hashMerkleRoot;
    tmp.block.nTime = pblock->nTime;
    tmp.block.nBits = pblock->nBits;
    tmp.block.nNonce = pblock->nNonce;

    FormatHashBlocks(&tmp.block, sizeof(tmp.block));
    FormatHashBlocks(&tmp.hash1, sizeof(tmp.hash1));

    for (unsigned int i = 0; i < sizeof(tmp) / 4; i++)
        ((unsigned int*)&tmp)[i] = ByteReverse(((unsigned int*)&tmp)[i]);

    SHA256Transform(pmidstate, &tmp.block, pSHA256InitState);

    memcpy(pdata, &tmp.block, 128);
    memcpy(phash1, &tmp.hash1, 64);
}

bool ProcessBlockFound(const CBlock* pblock, const CChainParams& chainparams)
{
    LogPrintf("%s\n", pblock->ToString());
    LogPrintf("generated %s\n", FormatMoney(pblock->vtx[0].vout[0].nValue));

    {
        LOCK(cs_main);
        if (pblock->hashPrevBlock != chainActive.Tip()->GetBlockHash())
            return error("GuldenMiner: generated block is stale");
    }

    GetMainSignals().BlockFound(pblock->GetHash());

    CValidationState state;
    if (!ProcessNewBlock(state, chainparams, NULL, pblock, true, NULL))
        return error("GuldenMiner: ProcessNewBlock, block not accepted");

    return true;
}

double dBestHashesPerSec = 0.0;
double dHashesPerSec = 0.0;
int64_t nHPSTimerStart = 0;
int64_t nHashCounter = 0;
int64_t nHashThrottle = -1;
static CCriticalSection timerCS;

void static BitcoinMiner(const CChainParams& chainparams)
{
    LogPrintf("GuldenMiner started\n");
    RenameThread("gulden-miner");

    unsigned int nExtraNonce = 0;

    boost::shared_ptr<CReserveScript> coinbaseScript;
    GetMainSignals().ScriptForMining(coinbaseScript);

    if (nHPSTimerStart == 0) {
        LOCK(timerCS);
        if (nHPSTimerStart == 0) {
            nHPSTimerStart = GetTimeMillis();
            nHashCounter = 0;
            dHashesPerSec = 0;
        }
    }

    try {

        if (!coinbaseScript || coinbaseScript->reserveScript.empty())
            throw std::runtime_error("No coinbase script available (mining requires a wallet)");

        while (true) {
            if (chainparams.MiningRequiresPeers()) {

                do {
                    bool fvNodesEmpty;
                    {
                        LOCK(cs_vNodes);
                        fvNodesEmpty = vNodes.empty();
                    }
                    if (!fvNodesEmpty && !IsInitialBlockDownload())
                        break;
                    MilliSleep(1000);
                } while (true);
            }

            unsigned int nTransactionsUpdatedLast = mempool.GetTransactionsUpdated();
            CBlockIndex* pindexPrev = chainActive.Tip();

            std::unique_ptr<CBlockTemplate> pblocktemplate(BlockAssembler(chainparams).CreateNewBlock(coinbaseScript->reserveScript));
            if (!pblocktemplate.get()) {
                LogPrintf("Error in GuldenMiner: Keypool ran out, please call keypoolrefill before restarting the mining thread\n");
                return;
            }
            CBlock* pblock = &pblocktemplate->block;
            IncrementExtraNonce(pblock, pindexPrev, nExtraNonce);

            LogPrintf("Running GuldenMiner with %u transactions in block (%u bytes)\n", pblock->vtx.size(),
                      ::GetSerializeSize(*pblock, SER_NETWORK, PROTOCOL_VERSION));

            int64_t nStart = GetTime();
            arith_uint256 hashTarget = arith_uint256().SetCompact(pblock->nBits);
            uint256 hash;
            uint32_t nNonce = 0;
            while (true) {

                arith_uint256 thash;
                while (true) {
                    if (GetTimeMillis() - nHPSTimerStart > 1000) {
                        TRY_LOCK(timerCS, lockhc);
                        if (lockhc && GetTimeMillis() - nHPSTimerStart > 1000) {
                            int64_t nTemp = nHashCounter;
                            nHashCounter = 0;
                            dHashesPerSec = 1000 * (nTemp / (GetTimeMillis() - nHPSTimerStart));
                            dBestHashesPerSec = std::max(dBestHashesPerSec, dHashesPerSec);
                            nHPSTimerStart = GetTimeMillis();
                        }
                        UpdateTime(pblock, chainparams.GetConsensus(), pindexPrev);
                        arith_uint256 compBits = pblock->nBits;
                        if (GetNextWorkRequired(chainActive.Tip(), pblock, Params().GetConsensus()) != compBits) {
                            break;
                        }
                    }

                    char scratchpad[SCRYPT_SCRATCHPAD_SIZE];
                    scrypt_1024_1_1_256_sp(BEGIN(pblock->nVersion), BEGIN(thash), scratchpad);

                    if (thash <= hashTarget) {

                        LogPrintf("GuldenMiner:\n");
                        LogPrintf("proof-of-work found  \n  hash: %s  \ntarget: %s\n", thash.GetHex(), hashTarget.GetHex());
                        ProcessBlockFound(pblock, chainparams);
                        coinbaseScript->KeepScript();

                        if (chainparams.MineBlocksOnDemand())
                            throw boost::thread_interrupted();

                        break;
                    }
                    pblock->nNonce += 1;
                    nHashCounter++;
                    while (nHashThrottle != -1 && nHashCounter >= nHashThrottle) {
                        if (GetTimeMillis() - nHPSTimerStart > 1000) {
                            TRY_LOCK(timerCS, lockhc);
                            if (lockhc && GetTimeMillis() - nHPSTimerStart > 1000) {
                                int64_t nTemp = nHashCounter;
                                nHashCounter = 0;
                                dHashesPerSec = 1000 * (nTemp / (GetTimeMillis() - nHPSTimerStart));
                                dBestHashesPerSec = std::max(dBestHashesPerSec, dHashesPerSec);
                                nHPSTimerStart = GetTimeMillis();
                            }
                        }
                        MilliSleep(1);
                    }

                    if ((pblock->nNonce & 0xFF) == 0)
                        break;
                }

                boost::this_thread::interruption_point();

                if (vNodes.empty() && chainparams.MiningRequiresPeers())
                    break;
                if (nNonce >= 0xffff0000)
                    break;
                if (mempool.GetTransactionsUpdated() != nTransactionsUpdatedLast && GetTime() - nStart > 60)
                    break;
                if (pindexPrev != chainActive.Tip())
                    break;

                if (UpdateTime(pblock, chainparams.GetConsensus(), pindexPrev) < 0)
                    break; // Recreate the block if the clock has run backwards,

                if (chainparams.GetConsensus().fPowAllowMinDifficultyBlocks) {

                    hashTarget.SetCompact(pblock->nBits);
                }
            }
        }
    }
    catch (const boost::thread_interrupted&) {
        LogPrintf("GuldenMiner terminated\n");
        throw;
    }
    catch (const std::runtime_error& e) {
        LogPrintf("GuldenMiner runtime error: %s\n", e.what());
        return;
    }
}

void GenerateBitcoins(bool fGenerate, int nThreads, const CChainParams& chainparams)
{
    static boost::thread_group* minerThreads = NULL;

    if (nThreads < 0)
        nThreads = GetNumCores();

    if (minerThreads != NULL) {
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
