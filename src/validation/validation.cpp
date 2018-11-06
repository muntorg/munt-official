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

#include "validation/validation.h"
#include "validation/witnessvalidation.h"

#include "alert.h"
#include "arith_uint256.h"
#include "blockstore.h"
#include "chain.h"
#include "chainparams.h"
#include "checkpoints.h"
#include "Gulden/auto_checkpoints.h"
#include "checkqueue.h"
#include "consensus/consensus.h"
#include "consensus/merkle.h"
#include "consensus/tx_verify.h"
#include "consensus/validation.h"
#include "fs.h"
#include "hash.h"
#include "unity/appmanager.h"
#include "init.h"
#include "policy/fees.h"
#include "policy/policy.h"
#include "pow.h"
#include <Gulden/Common/diff.h>
#include "primitives/block.h"
#include "primitives/transaction.h"
#include "random.h"
#include "script/script.h"
#include "script/sigcache.h"
#include "script/sign.h"
#include "script/standard.h"
#include "timedata.h"
#include "tinyformat.h"
#include "txdb.h"
#include "txmempool.h"
#include "ui_interface.h"
#include "undo.h"
#include "util.h"
#include "utilmoneystr.h"
#include "utilstrencodings.h"
#include "validation/validationinterface.h"
#include "validation/versionbitsvalidation.h"
#include "versionbits.h"
#include "warnings.h"
#ifdef ENABLE_WALLET
#include "wallet/wallet.h"
#endif

#include <atomic>
#include <sstream>

#include <boost/foreach.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/thread.hpp>

#if defined(NDEBUG)
# error "Gulden cannot be compiled without assertions."
#endif

/**
 * Global state
 */

CCriticalSection cs_main;

BlockMap mapBlockIndex;
CChain chainActive;
CBlockIndex *pindexBestHeader = NULL;
CPartialChain partialChain;
CBlockIndex *pindexBestPartial = nullptr;
CWaitableCriticalSection csBestBlock;
CConditionVariable cvBlockChange;
int nScriptCheckThreads = 0;
std::atomic_bool fImporting(false);
bool fReindex = false;
bool fReverseHeaders = false;
bool fTxIndex = false;
bool fHavePruned = false;
bool fPruneMode = false;
bool fIsBareMultisigStd = DEFAULT_PERMIT_BAREMULTISIG;
bool fRequireStandard = true;
bool fCheckBlockIndex = false;
bool fCheckpointsEnabled = DEFAULT_CHECKPOINTS_ENABLED;
size_t nCoinCacheUsage = 5000 * 300;
uint64_t nPruneTarget = 0;
bool fAlerts = DEFAULT_ALERTS;
int64_t nMaxTipAge = DEFAULT_MAX_TIP_AGE;
bool fEnableReplacement = DEFAULT_ENABLE_REPLACEMENT;

uint256 hashAssumeValid;

CFeeRate minRelayTxFee = CFeeRate(DEFAULT_MIN_RELAY_TX_FEE);
CAmount maxTxFee = DEFAULT_TRANSACTION_MAXFEE;

CBlockPolicyEstimator feeEstimator;
CTxMemPool mempool(&feeEstimator);

static void CheckBlockIndex(const Consensus::Params& consensusParams);

/** Constant stuff for coinbase transactions we create: */
CScript COINBASE_FLAGS;

int64_t nMinimumInputValue = DUST_HARD_LIMIT;

const std::string strMessageMagic = "Guldencoin Signed Message:\n";

std::set<CBlockIndex*, CBlockIndexWorkComparator> setBlockIndexCandidates;

// Internal stuff
namespace {
    CBlockIndex *pindexBestInvalid;

    /** All pairs A->B, where A (or one of its ancestors) misses transactions, but B has transactions.
     * Pruned nodes may have entries where B is missing data.
     */
    std::multimap<CBlockIndex*, CBlockIndex*> mapBlocksUnlinked;

    CCriticalSection cs_LastBlockFile;
    std::vector<CBlockFileInfo> vinfoBlockFile;
    int nLastBlockFile = 0;
    /** Global flag to indicate we should check to see if there are
     *  block/undo files that should be deleted.  Set on startup
     *  or if we allocate more file space when we're in prune mode
     */
    bool fCheckForPruning = false;
    std::atomic<int> nMaxSPVPruneHeight = 0;
    /**
     * Every received block is assigned a unique and increasing identifier, so we
     * know which one to give priority in case of a fork.
     */
    CCriticalSection cs_nBlockSequenceId;
    /** Blocks loaded from disk are assigned id 0, so start the counter at 1. */
    int32_t nBlockSequenceId = 1;
    /** Decreasing counter (used by subsequent preciousblock calls). */
    int32_t nBlockReverseSequenceId = -1;
    /** chainwork for the last block that preciousblock has been applied to. */
    arith_uint256 nLastPreciousChainwork = 0;

    /** Dirty block index entries. */
    std::set<CBlockIndex*> setDirtyBlockIndex;

    /** Dirty block file entries. */
    std::set<int> setDirtyFileInfo;

    std::atomic<bool> fFullSyncMode(DEFAULT_FULL_SYNC_MODE);

    boost::signals2::signal<void (const CBlockIndex *pTip)> headerTipSignal;
} // anon namespace

CBlockIndex* FindForkInGlobalIndex(const CChain& chain, const CBlockLocator& locator)
{
    // Find the first block the caller has in the main chain
    for(const uint256& hash : locator.vHave) {
        BlockMap::iterator mi = mapBlockIndex.find(hash);
        if (mi != mapBlockIndex.end())
        {
            CBlockIndex* pindex = (*mi).second;
            if (chain.Contains(pindex))
                return pindex;
            if (pindex->GetAncestor(chain.Height()) == chain.Tip()) {
                return chain.Tip();
            }
        }
    }
    return chain.Genesis();
}

CCoinsViewDB *pcoinsdbview = NULL;
CCoinsViewCache *pcoinsTip = NULL;
CBlockTreeDB *pblocktree = NULL;

bool CheckFinalTx(const CTransaction &tx, const CChain& chain, int flags)
{
    AssertLockHeld(cs_main);

    // By convention a negative value for flags indicates that the
    // current network-enforced consensus rules should be used. In
    // a future soft-fork scenario that would mean checking which
    // rules would be enforced for the next block and setting the
    // appropriate flags. At the present time no soft-forks are
    // scheduled, so no flags are set.
    flags = std::max(flags, 0);

    // CheckFinalTx() uses chainActive.Height()+1 to evaluate
    // nLockTime because when IsFinalTx() is called within
    // CBlock::AcceptBlock(), the height of the block *being*
    // evaluated is what is used. Thus if we want to know if a
    // transaction can be part of the *next* block, we need to call
    // IsFinalTx() with one more than chainActive.Height().
    const int nBlockHeight = chain.Height() + 1;

    // BIP113 will require that time-locked transactions have nLockTime set to
    // less than the median time of the previous block they're contained in.
    // When the next block is created its previous block will be the current
    // chain tip, so we use that to calculate the median time passed to
    // IsFinalTx() if LOCKTIME_MEDIAN_TIME_PAST is set.
    const int64_t nBlockTime = (flags & LOCKTIME_MEDIAN_TIME_PAST)
                             ? chain.Tip()->GetMedianTimePast()
                             : GetAdjustedTime();

    return IsFinalTx(tx, nBlockHeight, nBlockTime);
}

bool TestLockPointValidity(const LockPoints* lp)
{
    AssertLockHeld(cs_main);
    assert(lp);
    // If there are relative lock times then the maxInputBlock will be set
    // If there are no relative lock times, the LockPoints don't depend on the chain
    if (lp->maxInputBlock) {
        // Check whether chainActive is an extension of the block at which the LockPoints
        // calculation was valid.  If not LockPoints are no longer valid
        if (!chainActive.Contains(lp->maxInputBlock)) {
            return false;
        }
    }

    // LockPoints still valid
    return true;
}

bool CheckSequenceLocks(const CTransaction &tx, int flags, LockPoints* lp, bool useExistingLockPoints)
{
    AssertLockHeld(cs_main);
    AssertLockHeld(mempool.cs);

    CBlockIndex* tip = chainActive.Tip();
    CBlockIndex index;
    index.pprev = tip;
    // CheckSequenceLocks() uses chainActive.Height()+1 to evaluate
    // height based locks because when SequenceLocks() is called within
    // ConnectBlock(), the height of the block *being*
    // evaluated is what is used.
    // Thus if we want to know if a transaction can be part of the
    // *next* block, we need to use one more than chainActive.Height()
    index.nHeight = tip->nHeight + 1;

    std::pair<int, int64_t> lockPair;
    if (useExistingLockPoints) {
        assert(lp);
        lockPair.first = lp->height;
        lockPair.second = lp->time;
    }
    else {
        // pcoinsTip contains the UTXO set for chainActive.Tip()
        CCoinsViewMemPool viewMemPool(pcoinsTip, mempool);
        std::vector<int> prevheights;
        prevheights.resize(tx.vin.size());
        for (size_t txinIndex = 0; txinIndex < tx.vin.size(); txinIndex++) {
            const CTxIn& txin = tx.vin[txinIndex];
            Coin coin;
            if (!viewMemPool.GetCoin(txin.prevout, coin)) {
                return error("%s: Missing input", __func__);
            }
            if (coin.nHeight == MEMPOOL_HEIGHT) {
                // Assume all mempool transaction confirm in the next block
                prevheights[txinIndex] = tip->nHeight + 1;
            } else {
                prevheights[txinIndex] = coin.nHeight;
            }
        }
        lockPair = CalculateSequenceLocks(tx, flags, &prevheights, index);
        if (lp) {
            lp->height = lockPair.first;
            lp->time = lockPair.second;
            // Also store the hash of the block with the highest height of
            // all the blocks which have sequence locked prevouts.
            // This hash needs to still be on the chain
            // for these LockPoint calculations to be valid
            // Note: It is impossible to correctly calculate a maxInputBlock
            // if any of the sequence locked inputs depend on unconfirmed txs,
            // except in the special case where the relative lock time/height
            // is 0, which is equivalent to no sequence lock. Since we assume
            // input height of tip+1 for mempool txs and test the resulting
            // lockPair from CalculateSequenceLocks against tip+1.  We know
            // EvaluateSequenceLocks will fail if there was a non-zero sequence
            // lock on a mempool input, so we can use the return value of
            // CheckSequenceLocks to indicate the LockPoints validity
            int maxInputHeight = 0;
            for(int height : prevheights) {
                // Can ignore mempool inputs since we'll fail if they had non-zero locks
                if (height != tip->nHeight+1) {
                    maxInputHeight = std::max(maxInputHeight, height);
                }
            }
            lp->maxInputBlock = tip->GetAncestor(maxInputHeight);
        }
    }
    return EvaluateSequenceLocks(index, lockPair);
}




//////////////////////////////////////////////////////////////////////////////
//
// CBlock and CBlockIndex
//

bool ReadBlockFromDisk(CBlock& block, const CBlockIndex* pindex, const CChainParams& params)
{
    return blockStore.ReadBlockFromDisk(block, pindex->GetBlockPos(), params, pindex);
}


CBlockIndex *pindexBestForkTip = NULL, *pindexBestForkBase = NULL;


static void CheckForkWarningConditions()
{
    AssertLockHeld(cs_main);
    // Before we get past initial download, we cannot reliably alert about forks
    // (we assume we don't get stuck on a fork before finishing our initial sync)
    if (IsInitialBlockDownload())
        return;

    // If our best fork is no longer within 72 blocks (+/- 12 hours if no one mines it)
    // of our head, drop it
    if (pindexBestForkTip && chainActive.Height() - pindexBestForkTip->nHeight >= 72)
        pindexBestForkTip = NULL;

    if (pindexBestForkTip || (pindexBestInvalid && pindexBestInvalid->nChainWork > chainActive.Tip()->nChainWork + (GetBlockProof(*chainActive.Tip()) * 6)))
    {
        if (!GetfLargeWorkForkFound() && pindexBestForkBase)
        {
            std::string warning = std::string("'Warning: Large-work fork detected, forking after block ") +
                pindexBestForkBase->phashBlock->ToString() + std::string("'");
            CAlert::Notify(warning, true);
        }
        if (pindexBestForkTip && pindexBestForkBase)
        {
            LogPrintf("%s: Warning: Large valid fork found\n  forking the chain at height %d (%s)\n  lasting to height %d (%s).\nChain state database corruption likely.\n", __func__,
                   pindexBestForkBase->nHeight, pindexBestForkBase->phashBlock->ToString(),
                   pindexBestForkTip->nHeight, pindexBestForkTip->phashBlock->ToString());
            SetfLargeWorkForkFound(true);
        }
        else
        {
            LogPrintf("%s: Warning: Found invalid chain at least ~6 blocks longer than our best chain.\nChain state database corruption likely.\n", __func__);
            SetfLargeWorkInvalidChainFound(true);
        }
    }
    else
    {
        SetfLargeWorkForkFound(false);
        SetfLargeWorkInvalidChainFound(false);
    }
}

static void CheckForkWarningConditionsOnNewFork(CBlockIndex* pindexNewForkTip)
{
    AssertLockHeld(cs_main);
    // If we are on a fork that is sufficiently large, set a warning flag
    CBlockIndex* pfork = pindexNewForkTip;
    CBlockIndex* plonger = chainActive.Tip();
    while (pfork && pfork != plonger)
    {
        while (plonger && plonger->nHeight > pfork->nHeight)
            plonger = plonger->pprev;
        if (pfork == plonger)
            break;
        pfork = pfork->pprev;
    }

    // We define a condition where we should warn the user about as a fork of at least 7 blocks
    // with a tip within 72 blocks (+/- 12 hours if no one mines it) of ours
    // We use 7 blocks rather arbitrarily as it represents just under 10% of sustained network
    // hash rate operating on the fork.
    // or a chain that is entirely longer than ours and invalid (note that this should be detected by both)
    // We define it this way because it allows us to only store the highest fork tip (+ base) which meets
    // the 7-block condition and from this always have the most-likely-to-cause-warning fork
    if (pfork && (!pindexBestForkTip || (pindexBestForkTip && pindexNewForkTip->nHeight > pindexBestForkTip->nHeight)) &&
            pindexNewForkTip->nChainWork - pfork->nChainWork > (GetBlockProof(*pfork) * 7) &&
            chainActive.Height() - pindexNewForkTip->nHeight < 72)
    {
        pindexBestForkTip = pindexNewForkTip;
        pindexBestForkBase = pfork;
    }

    CheckForkWarningConditions();
}

void static InvalidChainFound(CBlockIndex* pindexNew)
{
    if (!pindexBestInvalid || pindexNew->nChainWork > pindexBestInvalid->nChainWork)
        pindexBestInvalid = pindexNew;

    LogPrintf("%s: invalid block=%s  height=%d  log2_work=%.8g  date=%s\n", __func__,
      pindexNew->GetBlockHashPoW2().ToString(), pindexNew->nHeight,
      log(pindexNew->nChainWork.getdouble())/log(2.0), DateTimeStrFormat("%Y-%m-%d %H:%M:%S",
      pindexNew->GetBlockTime()));
    CBlockIndex *tip = chainActive.Tip();
    assert (tip);
    LogPrintf("%s:  current best=%s  height=%d  log2_work=%.8g  date=%s\n", __func__,
      tip->GetBlockHashPoW2().ToString(), chainActive.Height(), log(tip->nChainWork.getdouble())/log(2.0),
      DateTimeStrFormat("%Y-%m-%d %H:%M:%S", tip->GetBlockTime()));
    CheckForkWarningConditions();
}

void static InvalidBlockFound(CBlockIndex *pindex, const CValidationState &state) {
    if (!state.CorruptionPossible()) {
        LOCK(cs_main);
        pindex->nStatus |= BLOCK_FAILED_VALID;
        setDirtyBlockIndex.insert(pindex);
        setBlockIndexCandidates.erase(pindex);
        InvalidChainFound(pindex);
    }
}

void UpdateCoins(const CTransaction& tx, CCoinsViewCache& inputs, CTxUndo &txundo, int nHeight)
{
    // mark inputs spent
    if (!tx.IsCoinBase() || tx.IsPoW2WitnessCoinBase()) {
        //fixme: (2.1) See if we can bring back the reserve efficiency here.
        //txundo.vprevout.reserve(tx.vin.size());
        for(const CTxIn &txin : tx.vin) {
            if (!txin.prevout.IsNull())
            {
                txundo.vprevout.emplace_back();
                inputs.SpendCoin(txin.prevout, &txundo.vprevout.back());
            }
        }
    }
    // add outputs
    AddCoins(inputs, tx, nHeight);
}

void UpdateCoins(const CTransaction& tx, CCoinsViewCache& inputs, int nHeight)
{
    CTxUndo txundo;
    UpdateCoins(tx, inputs, txundo, nHeight);
}

bool CScriptCheck::operator()() {
    const CScript &scriptSig = ptxTo->vin[nIn].scriptSig;
    const CSegregatedSignatureData *witness = &ptxTo->vin[nIn].segregatedSignatureData;
    return VerifyScript(scriptSig, scriptPubKey, witness, nFlags, CachingTransactionSignatureChecker(signingKeyID, spendingKeyID, ptxTo, nIn, amount, cacheStore, *txdata), &error);
}

int GetSpendHeight(const CCoinsViewCache& inputs)
{
    LOCK(cs_main);
    CBlockIndex* pindexPrev = mapBlockIndex.find(inputs.GetBestBlock())->second;
    return pindexPrev->nHeight + 1;
}

//fixme: (2.1) This should rather use move semantics, but CScript doesn't currently seem compatible with this.
//fixme: (2.0.1) Use this in places that are hardcoded instead.
CScript GetScriptForNonScriptOutput(const CTxOut& out)
{
    if (out.GetType() <= CTxOutType::PoW2WitnessOutput)
    {
        std::vector<unsigned char> sWitnessPlaceholder = {'p','o','w','2','w','i','t','n','e','s','s'};
        return CScript(sWitnessPlaceholder.begin(), sWitnessPlaceholder.end());
    }
    else if (out.GetType() <= CTxOutType::StandardKeyHashOutput)
    {
        std::vector<unsigned char> sWitnessPlaceholder = {'k','e','y','h','a','s','h'};
        return CScript(sWitnessPlaceholder.begin(), sWitnessPlaceholder.end());
    }
    else
    {
        assert(0);
    }
    return CScript();
}


/**
 * Check whether all inputs of this transaction are valid (no double spends, scripts & sigs, amounts)
 * This does not modify the UTXO set. If pvChecks is not NULL, script checks are pushed onto it
 * instead of being performed inline.
 */
bool CheckInputs(const CTransaction& tx, CValidationState &state, const CCoinsViewCache &inputs, bool fScriptChecks, unsigned int flags, bool cacheStore, PrecomputedTransactionData& txdata, std::vector<CWitnessTxBundle>* pWitnessBundles, std::vector<CScriptCheck> *pvChecks)
{
    if (!tx.IsCoinBase() || tx.IsPoW2WitnessCoinBase())
    {
        if (!Consensus::CheckTxInputs(tx, state, inputs, GetSpendHeight(inputs), pWitnessBundles))
            return false;

        if (pvChecks)
            pvChecks->reserve(tx.vin.size());

        // The first loop above does all the inexpensive checks.
        // Only if ALL inputs pass do we perform expensive ECDSA signature checks.
        // Helps prevent CPU exhaustion attacks.

        // Skip script verification when connecting blocks under the
        // assumevalid block. Assuming the assumevalid block is valid this
        // is safe because block merkle hashes are still computed and checked,
        // Of course, if an assumed valid block is invalid due to false scriptSigs
        // this optimization would allow an invalid chain to be accepted.
        if (fScriptChecks) {
            for (unsigned int i = 0; i < tx.vin.size(); i++) {
                const COutPoint &prevout = tx.vin[i].prevout;
                if (prevout.IsNull() && tx.IsPoW2WitnessCoinBase())
                    continue;
                const Coin& coin = inputs.AccessCoin(prevout);
                assert(!coin.IsSpent());

                // We very carefully only pass in things to CScriptCheck which
                // are clearly committed to by tx' witness hash. This provides
                // a sanity check that our caching is not introducing consensus
                // failures through additional data in, eg, the coins being
                // spent being checked as a part of CScriptCheck.
                const CAmount amount = coin.out.nValue;

                CKeyID witnessSigningKeyID = ExtractSigningPubkeyFromTxOutput(coin.out, SignType::Witness);
                if (coin.out.GetType() > CTxOutType::ScriptLegacyOutput)
                {
                    CKeyID spendSigningKeyID;
                    if (coin.out.GetType() == CTxOutType::StandardKeyHashOutput || coin.out.GetType() == CTxOutType::PoW2WitnessOutput)
                    {
                        if (coin.out.GetType() == CTxOutType::StandardKeyHashOutput)
                        {
                            if (tx.vin[i].segregatedSignatureData.stack.size() != 1)
                                return state.DoS(100,false, REJECT_INVALID, strprintf("invalid-segregated-signature-stack-size (%d) (standard-key-hash-input should always have a segregatedSignatureData stack size of exactly 1)", tx.vin[i].segregatedSignatureData.stack.size()));
                        }
                        else
                        {
                            // NB! The checks in tx_verify ensure that we have the right number of signatures (2 or 1) based on the type of witness operation.
                            // So we can assume at this point in the code that a spend will always have 2 signatures and won't try trick the system by providing only 1 signature.
                            // We therefore just validate here based on number of signatures provided.
                            if (tx.vin[i].segregatedSignatureData.stack.size() == 2)
                            {
                                spendSigningKeyID = ExtractSigningPubkeyFromTxOutput(coin.out, SignType::Spend);
                                if (spendSigningKeyID.IsNull())
                                    return state.DoS(100,false, REJECT_INVALID, strprintf("invalid-witness-prevout (unable to extract a valid spending key from prevout)"));
                                if (witnessSigningKeyID.IsNull())
                                    return state.DoS(100,false, REJECT_INVALID, strprintf("invalid-witness-prevout (unable to extract a valid witness key from prevout)"));
                            }
                            else if (tx.vin[i].segregatedSignatureData.stack.size() == 1)
                            {
                                if (witnessSigningKeyID.IsNull())
                                    return state.DoS(100,false, REJECT_INVALID, strprintf("invalid-witness-prevout (unable to extract a valid witness key from prevout)"));
                            }
                            else
                            {
                                return state.DoS(100,false, REJECT_INVALID, strprintf("invalid-scriptwitness-segregated-signature-data-size (%d) (witness-input should always have a segregatedSignatureData stack size of either 1 or 2 depending on whether it is a spend or witness operation)", tx.vin[i].segregatedSignatureData.stack.size()));
                            }
                        }

                        CScript scriptCodePlaceHolder;
                        if (coin.out.GetType() == CTxOutType::StandardKeyHashOutput)
                        {
                            std::vector<unsigned char> sKeyHashPlaceholder = {'k','e','y','h','a','s','h'};
                            scriptCodePlaceHolder = CScript(sKeyHashPlaceholder.begin(), sKeyHashPlaceholder.end());
                        }
                        else
                        {
                            std::vector<unsigned char> sWitnessPlaceholder = {'p','o','w','2','w','i','t','n','e','s','s'};
                            scriptCodePlaceHolder = CScript(sWitnessPlaceholder.begin(), sWitnessPlaceholder.end());
                        }

                        //We extract the pubkey from the signatures so just pass in an empty pubkey for the checks.
                        std::vector<unsigned char> vchEmptyPubKey;
                        CachingTransactionSignatureChecker check1(witnessSigningKeyID, CKeyID(), &tx, i, amount, cacheStore, txdata);
                        if (!check1.CheckSig(tx.vin[i].segregatedSignatureData.stack[0], vchEmptyPubKey, scriptCodePlaceHolder, SIGVERSION_SEGSIG))
                        {
                            return false;
                        }
                        // For a witness spend operation we have a second key that also needs checking.
                        if (!spendSigningKeyID.IsNull())
                        {
                            CachingTransactionSignatureChecker check2(spendSigningKeyID, CKeyID(), &tx, i, amount, cacheStore, txdata);
                            if (!check2.CheckSig(tx.vin[i].segregatedSignatureData.stack[1], vchEmptyPubKey, scriptCodePlaceHolder, SIGVERSION_SEGSIG))
                            {
                                return false;
                            }
                        }
                    }
                    else
                    {
                        return state.DoS(100,false, REJECT_INVALID, strprintf("unknown-transaction-type [%d]", coin.out.GetType()));
                    }
                    return true;
                }

                //fixme: (2.1) (SEGSIG) - also transition to extracted signature.
                // Verify signature
                //fixme: spendingKeyID
                const CScript& scriptPubKey = coin.out.output.scriptPubKey;
                CScriptCheck check(witnessSigningKeyID, scriptPubKey, amount, tx, i, flags, cacheStore, &txdata);
                if (scriptPubKey.IsPoW2Witness())
                {
                    check.spendingKeyID = ExtractSigningPubkeyFromTxOutput(coin.out, SignType::Spend);
                }
                if (pvChecks)
                {
                    pvChecks->push_back(CScriptCheck());
                    check.swap(pvChecks->back());
                }
                else if (!check())
                {
                    if (flags & STANDARD_NOT_MANDATORY_VERIFY_FLAGS)
                    {
                        // Check whether the failure was caused by a
                        // non-mandatory script verification check, such as
                        // non-standard DER encodings or non-null dummy
                        // arguments; if so, don't trigger DoS protection to
                        // avoid splitting the network between upgraded and
                        // non-upgraded nodes.
                        CScriptCheck check2(witnessSigningKeyID, scriptPubKey, amount, tx, i, flags & ~STANDARD_NOT_MANDATORY_VERIFY_FLAGS, cacheStore, &txdata);
                        if (check2())
                            return state.Invalid(false, REJECT_NONSTANDARD, strprintf("non-mandatory-script-verify-flag (%s)", ScriptErrorString(check.GetScriptError())));
                    }
                    // Failures of other flags indicate a transaction that is
                    // invalid in new blocks, e.g. a invalid P2SH. We DoS ban
                    // such nodes as they are not following the protocol. That
                    // said during an upgrade careful thought should be taken
                    // as to the correct behavior - we may want to continue
                    // peering with non-upgraded nodes even after soft-fork
                    // super-majority signaling has occurred.
                    return state.DoS(100,false, REJECT_INVALID, strprintf("mandatory-script-verify-flag-failed (%s)", ScriptErrorString(check.GetScriptError())));
                }
            }
        }
    }

    return true;
}

namespace {

/** Abort with a message */
bool AbortNode(const std::string& strMessage, const std::string& userMessage="")
{
    SetMiscWarning(strMessage);
    LogPrintf("*** %s\n", strMessage);
    uiInterface.ThreadSafeMessageBox(userMessage.empty() ? _("Error: A fatal internal error occurred, see debug.log for details") : userMessage, "", CClientUIInterface::MSG_ERROR);
    GuldenAppManager::gApp->shutdown();
    return false;
}

bool AbortNode(CValidationState& state, const std::string& strMessage, const std::string& userMessage="")
{
    AbortNode(strMessage, userMessage);
    return state.Error(strMessage);
}

} // anon namespace

/**
 * Restore the UTXO in a Coin at a given COutPoint
 * @param undo The Coin to be restored.
 * @param view The coins view to which to apply the changes.
 * @param out The out point that corresponds to the tx input.
 * @return A DisconnectResult as an int
 */
int ApplyTxInUndo(Coin&& undo, CCoinsViewCache& view, const COutPoint& out)
{
    bool fClean = true;

    if (view.HaveCoin(out)) fClean = false; // overwriting transaction output

    if (undo.nHeight == 0) {
        // Missing undo metadata (height and coinbase). Older versions included this
        // information only in undo records for the last spend of a transactions'
        // outputs. This implies that it must be present for some other output of the same tx.
        const Coin& alternate = AccessByTxid(view, out.getHash());
        if (!alternate.IsSpent()) {
            undo.nHeight = alternate.nHeight;
            undo.fCoinBase = alternate.fCoinBase;
        } else {
            return DISCONNECT_FAILED; // adding output for transaction without known metadata
        }
    }
    view.AddCoin(out, std::move(undo), undo.fCoinBase);

    return fClean ? DISCONNECT_OK : DISCONNECT_UNCLEAN;
}


DisconnectResult DisconnectBlock(const CBlock& block, const CBlockIndex* pindex, CCoinsViewCache& view)
{
    assert(pindex->GetBlockHashPoW2() == view.GetBestBlock());

    bool fClean = true;

    CBlockUndo blockUndo;
    CDiskBlockPos pos = pindex->GetUndoPos();
    if (pos.IsNull()) {
        error("DisconnectBlock(): no undo data available");
        return DISCONNECT_FAILED;
    }
    if (!blockStore.UndoReadFromDisk(blockUndo, pos, pindex->pprev->GetBlockHashPoW2())) {
        error("DisconnectBlock(): failure reading undo data");
        return DISCONNECT_FAILED;
    }

    if (blockUndo.vtxundo.size() + 1 != block.vtx.size()) {
        error("DisconnectBlock(): block and undo data inconsistent");
        return DISCONNECT_FAILED;
    }

    // undo transactions in reverse order
    for (int i = block.vtx.size() - 1; i >= 0; i--) {
        const CTransaction &tx = *(block.vtx[i]);
        uint256 hash = tx.GetHash();

        // Check that all outputs are available and match the outputs in the block itself
        // exactly.
        for (size_t o = 0; o < tx.vout.size(); o++) {
            if (!tx.vout[o].IsUnspendable()) {
                COutPoint out(hash, o);
                Coin coin;
                view.SpendCoin(out, &coin);
                if (tx.vout[o] != coin.out) {
                    fClean = false; // transaction output mismatch
                }
            }
        }

        // restore inputs
        if (i > 0) { // not coinbases
            CTxUndo &txundo = blockUndo.vtxundo[i-1];
            //fixme: (2.0.1) (HIGH) (force only 1 valid input in checkblock as well.)
            if (tx.IsPoW2WitnessCoinBase())
            {
                if (txundo.vprevout.size() != 1 || tx.vin.size() < 2)
                {
                    error("DisconnectBlock(): transaction and undo data inconsistent");
                    return DISCONNECT_FAILED;
                }
                for (unsigned int j = tx.vin.size(); j-- > 0;) {
                    if (!tx.vin[j].prevout.IsNull())
                    {
                        const COutPoint &out = tx.vin[j].prevout;
                        int res = ApplyTxInUndo(std::move(txundo.vprevout[0]), view, out);
                        if (res == DISCONNECT_FAILED)
                            return DISCONNECT_FAILED;
                        fClean = fClean && res != DISCONNECT_UNCLEAN;
                    }
                }
            }
            else
            {
                if (txundo.vprevout.size() != tx.vin.size()) {
                    error("DisconnectBlock(): transaction and undo data inconsistent");
                    return DISCONNECT_FAILED;
                }
                for (unsigned int j = tx.vin.size(); j-- > 0;) {
                    const COutPoint &out = tx.vin[j].prevout;
                    int res = ApplyTxInUndo(std::move(txundo.vprevout[j]), view, out);
                    if (res == DISCONNECT_FAILED)
                        return DISCONNECT_FAILED;
                    fClean = fClean && res != DISCONNECT_UNCLEAN;
                }
            }
            // At this point, all of txundo.vprevout should have been moved out.
        }
    }

    // move best block pointer to prevout block
    view.SetBestBlock(pindex->pprev->GetBlockHashPoW2());

    return fClean ? DISCONNECT_OK : DISCONNECT_UNCLEAN;
}

void static FlushBlockFile(bool fFinalize = false)
{
    LOCK(cs_LastBlockFile);

    CDiskBlockPos posOld(nLastBlockFile, 0);

    FILE *fileOld = blockStore.GetBlockFile(posOld);
    if (fileOld) {
        if (fFinalize)
            TruncateFile(fileOld, vinfoBlockFile[nLastBlockFile].nSize);
        FileCommit(fileOld);
    }

    fileOld = blockStore.GetUndoFile(posOld);
    if (fileOld) {
        if (fFinalize)
            TruncateFile(fileOld, vinfoBlockFile[nLastBlockFile].nUndoSize);
        FileCommit(fileOld);
    }
}

static bool FindUndoPos(CValidationState &state, int nFile, CDiskBlockPos &pos, unsigned int nAddSize);

static CCheckQueue<CScriptCheck> scriptcheckqueue(128);

void ThreadScriptCheck() {
    RenameThread("Gulden-scriptch");
    scriptcheckqueue.Thread();
}

/**
 * Threshold condition checker that triggers when unknown versionbits are seen on the network.
 */
class WarningBitsConditionChecker : public AbstractThresholdConditionChecker
{
private:
    int bit;

public:
    WarningBitsConditionChecker(int bitIn) : bit(bitIn) {}

    int64_t BeginTime(const Consensus::Params& params) const { return 0; }
    int64_t EndTime(const Consensus::Params& params) const { return std::numeric_limits<int64_t>::max(); }
    int Period(const Consensus::Params& params) const { return params.nMinerConfirmationWindow; }
    int Threshold(const Consensus::Params& params) const { return params.nRuleChangeActivationThreshold; }

    bool Condition(const CBlockIndex* pindex, const Consensus::Params& params) const
    {
        return ((pindex->nVersion & VERSIONBITS_TOP_MASK) == VERSIONBITS_TOP_BITS) &&
               ((pindex->nVersion >> bit) & 1) != 0 &&
               ((ComputeBlockVersion(pindex->pprev, params) >> bit) & 1) == 0;
    }
};

// Protected by cs_main
static ThresholdConditionCache warningcache[VERSIONBITS_NUM_BITS];

static int64_t nTimeCheck = 0;
static int64_t nTimeForks = 0;
static int64_t nTimeVerify = 0;
static int64_t nTimeConnect = 0;
static int64_t nTimeIndex = 0;
static int64_t nTimeCallbacks = 0;
static int64_t nTimeTotal = 0;

/** Apply the effects of this block (with given index) on the UTXO set represented by coins.
 *  Validity checks that depend on the UTXO set are also done; ConnectBlock()
 *  can fail if those validity checks fail (among other reasons). */
bool ConnectBlock(CChain& chain, const CBlock& block, CValidationState& state, CBlockIndex* pindex,
                  CCoinsViewCache& view, const CChainParams& chainparams, bool fJustCheck, bool fVerifyWitness)
{
    if (!ContextualCheckBlock(block, state, chainparams, pindex->pprev, chain, &view, true))
        return error("%s: Consensus::CheckBlock, failed ContextualCheckBlock with utxo check: %s", __func__, FormatStateMessage(state));

    AssertLockHeld(cs_main);
    assert(pindex);
    // pindex->phashBlock can be null if called by CreateNewBlock/TestBlockValidity
    assert((pindex->phashBlock == NULL) ||
           (*pindex->phashBlock == block.GetHashPoW2()));
    int64_t nTimeStart = GetTimeMicros();

    // Check it again in case a previous version let a bad block in
    if (!CheckBlock(block, state, chainparams.GetConsensus(), !fJustCheck, !fJustCheck))
        return error("%s: Consensus::CheckBlock: %s", __func__, FormatStateMessage(state));

    // verify that the view's current state corresponds to the previous block
    uint256 hashPrevBlock = pindex->pprev == NULL ? uint256() : pindex->pprev->GetBlockHashPoW2();
    assert(hashPrevBlock == view.GetBestBlock());

    // Special case for the genesis block, skipping connection of its transactions
    // (its coinbase is unspendable)
    if (block.GetHashLegacy() == chainparams.GetConsensus().hashGenesisBlock) {
        if (!fJustCheck)
            view.SetBestBlock(pindex->GetBlockHashLegacy());
        return true;
    }

    // Check transactions
    std::vector<std::vector<CWitnessTxBundle>> witnessBundles;
    for (const auto& tx : block.vtx)
    {
        witnessBundles.push_back(std::vector<CWitnessTxBundle>());
        if (!CheckTransactionContextual(*tx, state, pindex->nHeight, &witnessBundles.back()))
        {
            return state.Invalid(false, state.GetRejectCode(), state.GetRejectReason(), strprintf("Transaction check failed (tx hash %s) %s", tx->GetHash().ToString(), state.GetDebugMessage()));
        }
    }

    bool fScriptChecks = true;
    if (!hashAssumeValid.IsNull()) {
        // We've been configured with the hash of a block which has been externally verified to have a valid history.
        // A suitable default value is included with the software and updated from time to time.  Because validity
        //  relative to a piece of software is an objective fact these defaults can be easily reviewed.
        // This setting doesn't force the selection of any particular chain but makes validating some faster by
        //  effectively caching the result of part of the verification.
        BlockMap::const_iterator  it = mapBlockIndex.find(hashAssumeValid);
        if (it != mapBlockIndex.end()) {
            if (it->second->GetAncestor(pindex->nHeight) == pindex &&
                pindexBestHeader->GetAncestor(pindex->nHeight) == pindex &&
                pindexBestHeader->nChainWork >= UintToArith256(chainparams.GetConsensus().nMinimumChainWork)) {
                // This block is a member of the assumed verified chain and an ancestor of the best header.
                // The equivalent time check discourages hash power from extorting the network via DOS attack
                //  into accepting an invalid block through telling users they must manually set assumevalid.
                //  Requiring a software change or burying the invalid block, regardless of the setting, makes
                //  it hard to hide the implication of the demand.  This also avoids having release candidates
                //  that are hardly doing any signature verification at all in testing without having to
                //  artificially set the default assumed verified block further back.
                // The test against nMinimumChainWork prevents the skipping when denied access to any chain at
                //  least as good as the expected chain.
                fScriptChecks = (GetBlockProofEquivalentTime(*pindexBestHeader, *pindex, *pindexBestHeader, chainparams.GetConsensus()) <= 60 * 60 * 24 * 7 * 2);
            }
        }
    }

    int64_t nTime1 = GetTimeMicros(); nTimeCheck += nTime1 - nTimeStart;
    LogPrint(BCLog::BENCH, "    - Sanity checks: %.2fms [%.2fs]\n", 0.001 * (nTime1 - nTimeStart), nTimeCheck * 0.000001);

    // Do not allow blocks that contain transactions which 'overwrite' older transactions,
    // unless those are already completely spent.
    // If such overwrites are allowed, coinbases and transactions depending upon those
    // can be duplicated to remove the ability to spend the first instance -- even after
    // being sent to another address.
    // See BIP30 and http://r6.ca/blog/20120206T005236Z.html for more information.
    // This logic is not necessary for memory pool transactions, as AcceptToMemoryPool
    // already refuses previously-known transaction ids entirely.
    // This rule was originally applied to all blocks with a timestamp after March 15, 2012, 0:00 UTC.
    // Now that the whole chain is irreversibly beyond that time it is applied to all blocks except the
    // two in the chain that violate it. This prevents exploiting the issue against nodes during their
    // initial block download.
    //Gulden: BIP30 always true for us.
    bool fEnforceBIP30 = true;
/*
    bool fEnforceBIP30 = (!pindex->phashBlock) || // Enforce on CreateNewBlock invocations which don't have a hash.
                          !((pindex->nHeight==91842 && pindex->GetBlockHash() == uint256S("0x00000000000a4d0a398161ffc163c503763b1f4360639393e0e4c8e300e0caec")) ||
                           (pindex->nHeight==91880 && pindex->GetBlockHash() == uint256S("0x00000000000743f190a18c5577a3c2d2a1f610ae9601ac046a38084ccb7cd721")));
*/

    // Once BIP34 activated it was not possible to create new duplicate coinbases and thus other than starting
    // with the 2 existing duplicate coinbase pairs, not possible to create overwriting txs.  But by the
    // time BIP34 activated, in each of the existing pairs the duplicate coinbase had overwritten the first
    // before the first had been spent.  Since those coinbases are sufficiently buried its no longer possible to create further
    // duplicate transactions descending from the known pairs either.
    // If we're on the known chain at height greater than where BIP34 activated, we can save the db accesses needed for the BIP30 check.
    CBlockIndex *pindexBIP34height = pindex->pprev->GetAncestor(chainparams.GetConsensus().BIP34Height);
    //Only continue to enforce if we're below BIP34 activation height or the block hash at that height doesn't correspond.
    fEnforceBIP30 = fEnforceBIP30 && (!pindexBIP34height || !(pindexBIP34height->GetBlockHashPoW2() == chainparams.GetConsensus().BIP34Hash));

    if (fEnforceBIP30) {
        for (const auto& tx : block.vtx) {
            for (size_t o = 0; o < tx->vout.size(); o++) {
                if (view.HaveCoin(COutPoint(tx->GetHash(), o))) {
                    return state.DoS(100, error("ConnectBlock(): tried to overwrite transaction [%s]", tx->GetHash().ToString()), REJECT_INVALID, "bad-txns-BIP30");
                }
            }
        }
    }

    // BIP16 didn't become active until Oct 1 2012
    int64_t nBIP16SwitchTime = 1349049600;
    bool fStrictPayToScriptHash = (pindex->GetBlockTime() >= nBIP16SwitchTime);

    unsigned int flags = fStrictPayToScriptHash ? SCRIPT_VERIFY_P2SH : SCRIPT_VERIFY_NONE;

    // Start enforcing the DERSIG (BIP66) rule
    if (pindex->nHeight >= chainparams.GetConsensus().BIP66Height) {
        flags |= SCRIPT_VERIFY_DERSIG;
    }

    // Start enforcing CHECKLOCKTIMEVERIFY (BIP65) rule
    if (pindex->nHeight >= chainparams.GetConsensus().BIP65Height) {
        flags |= SCRIPT_VERIFY_CHECKLOCKTIMEVERIFY;
    }

    // Start enforcing BIP68 (sequence locks) and BIP112 (CHECKSEQUENCEVERIFY) using versionbits logic.
    int nLockTimeFlags = 0;
    if (VersionBitsState(pindex->pprev, chainparams.GetConsensus(), Consensus::DEPLOYMENT_CSV, versionbitscache) == THRESHOLD_ACTIVE) {
        flags |= SCRIPT_VERIFY_CHECKSEQUENCEVERIFY;
        nLockTimeFlags |= LOCKTIME_VERIFY_SEQUENCE;
    }

    //fixme: (2.1) (SEGSIG)
    // Start enforcing WITNESS rules using versionbits logic.
    if (IsSegSigEnabled(pindex->pprev))
        flags |= SCRIPT_VERIFY_NULLDUMMY;

    int64_t nTime2 = GetTimeMicros(); nTimeForks += nTime2 - nTime1;
    LogPrint(BCLog::BENCH, "    - Fork checks: %.2fms [%.2fs]\n", 0.001 * (nTime2 - nTime1), nTimeForks * 0.000001);

    CBlockUndo blockundo;

    //fixme: (2.1) Ideally this would be placed lower down (just before CAmount blockReward = nFees + nSubsidy;) 
    //However GetWitness calls recursively into ConnectBlock and CCheckQueueControl has a non-recursive mutex - so we must call this before creating 
    // Witness block must have valid signature from witness.
    if (block.nVersionPoW2Witness != 0)
    {
        CPubKey pubkey;
        uint256 hash = block.GetHashPoW2();
        if (!pubkey.RecoverCompact(hash, block.witnessHeaderPoW2Sig))
            return state.DoS(50, false, REJECT_INVALID, "invalid-witness-signature", false, "witness signature validation failed");

        if (fVerifyWitness)
        {
            CGetWitnessInfo witInfo;
            if (!GetWitness(chain, chainparams, &view, pindex->pprev, block, witInfo))
                return state.DoS(100, false, REJECT_INVALID, "invalid-witness", false, "could not determine a valid witness for block");
            if (witInfo.selectedWitnessTransaction.GetType() <= CTxOutType::ScriptLegacyOutput)
            {
                if (CKeyID(uint160(witInfo.selectedWitnessTransaction.output.scriptPubKey.GetPow2WitnessHash())) != pubkey.GetID())
                    return state.DoS(100, false, REJECT_INVALID, "invalid-witness-signature", false, "script witness signature incorrect for block");
            }
            else if(witInfo.selectedWitnessTransaction.GetType() == CTxOutType::PoW2WitnessOutput)
            {
                if (witInfo.selectedWitnessTransaction.output.witnessDetails.witnessKeyID != pubkey.GetID())
                    return state.DoS(100, false, REJECT_INVALID, "invalid-witness-signature", false, "witness signature incorrect for block");
            }
            else
            {
                return state.DoS(100, false, REJECT_INVALID, "invalid-witness-signature", false, "witness signature missing for block");
            }
        }
    }

    //fixme: (2.1) After 2.1 the below re-entrancy from WitnessCoinbaseInfoIsValid is no longer a thing.
    //So we can move this down and combine it with the scope where we check:
    //unsigned int nWitnessCoinbasePayoutIndex = nWitnessCoinbaseIndex + 1;
    //NB! This must occur before CCheckQueueControl to prevent CCheckQueueControl re-entrancy.

    int nPoW2PhaseParent = GetPoW2Phase(pindex->pprev, chainparams, chain, &view);
    int nPoW2PhaseGrandParent = GetPoW2Phase(pindex->pprev->pprev, chainparams, chain, &view);
    //NB! IMPORTANT - Below this point we should -not- do any further Is/Get PoW2 phase checks - as we modify the view below which alters the results of phase 3 check.
    //Do and store all such tests above this point in the code.


    //Only the second block with a phase 3 parent onwards has a witness coinbase with embedded data, as only the first block with a phase 3 parent has a witness.
    int nEmbeddedWitnessCoinbaseIndex = 0;
    if (nPoW2PhaseGrandParent == 3 && nPoW2PhaseParent != 4)
    {
        nEmbeddedWitnessCoinbaseIndex = GetPoW2WitnessCoinbaseIndex(block);
        if (nEmbeddedWitnessCoinbaseIndex == -1)
            return state.DoS(100, error("ConnectBlock(): PoW2 phase 3 coinbase lacks witness data)"), REJECT_INVALID, "bad-cb-nowitnessdata");

        if (fVerifyWitness)
        {
            if (!WitnessCoinbaseInfoIsValid(chain, nEmbeddedWitnessCoinbaseIndex, pindex->pprev, block, chainparams, view))
                return state.DoS(100, error("ConnectBlock(): PoW2 phase 3 coinbase has invalid coinbase info)"), REJECT_INVALID, "bad-cb-badwitnessinfo");
        }
    }
    unsigned int nWitnessCoinbaseIndex = 0;
    if (nPoW2PhaseParent >= 3)
    {
        if (block.nVersionPoW2Witness == 0)
        {
            // PoW block
            // Phase 4 + 5 - miner mines 80 reward instead of 100, so nothing to do here (GetBlockSubsidy returns correct amount)
        }
        else
        {
            // PoW2 block
            // Ensure witness coinbase is present and that it pays out the right amount.
            {
                for (unsigned int i = 1; i < block.vtx.size(); i++)
                {
                    if (block.vtx[i]->IsCoinBase() && block.vtx[i]->IsPoW2WitnessCoinBase())
                    {
                        nWitnessCoinbaseIndex = i;
                        break;
                    }
                }
                //testme: (GULDEN) (2.1) I think this is a duplicate check so can probably be removed.
                if (nWitnessCoinbaseIndex == 0)
                {
                    return state.DoS(100, error("ConnectBlock(): PoW2 witness coinbase missing)"), REJECT_INVALID, "bad-witness-cb");
                }
            }
        }
    }

    CCheckQueueControl<CScriptCheck> control(fScriptChecks && nScriptCheckThreads ? &scriptcheckqueue : NULL);

    std::vector<int> prevheights;
    CAmount nFees = 0;
    CAmount nFeesPoW2Witness = 0;
    int nInputs = 0;
    int64_t nSigOpsCost = 0;
    CDiskTxPos pos(pindex->GetBlockPos(), GetSizeOfCompactSize(block.vtx.size()));
    std::vector<std::pair<uint256, CDiskTxPos> > vPos;
    vPos.reserve(block.vtx.size());
    blockundo.vtxundo.reserve(block.vtx.size() - 1);
    std::vector<PrecomputedTransactionData> txdata;
    txdata.reserve(block.vtx.size()); // Required so that pointers to individual PrecomputedTransactionData don't get invalidated
    for (unsigned int i = 0; i < block.vtx.size(); i++)
    {
        const CTransaction &tx = *(block.vtx[i]);

        nInputs += tx.vin.size();

        if (tx.IsPoW2WitnessCoinBase())
        {
            for (unsigned int i = 0; i < tx.vin.size(); i++)
            {
                if (!tx.vin[i].prevout.IsNull())
                {
                    if (!view.HaveCoin(tx.vin[i].prevout))
                    {
                        return state.DoS(100, error("ConnectBlock(): witness coinbase inputs missing/spent"), REJECT_INVALID, "bad-txns-inputs-missingorspent");
                    }

                    //fixme: (2.1) (SEGSIG) - Find a way to implement the bip68 sequence stuff here as well with minimal code churn...
                }
            }
        }
        else
        {
            if (!tx.IsCoinBase())
            {
                if (!view.HaveInputs(tx))
                {
                    //fixme: (2.1) - Low level fix for problem of conflicting transaction entering mempool and causing miners to be unable to mine (due to selecting invalid transactions for block continuously).
                    //This fix should remain in place, but a follow up fix is needed to try stop the conflicting transaction entering the mempool to begin with - need to hunt the source of this down.
                    //Seems to have something to do with a double (conflicting) witness renewal transaction.
                    mempool.removeRecursive(tx, MemPoolRemovalReason::UNKNOWN);
                    LogPrintf("ConnectBlock: mempool contains transaction with invalid inputs [%s]", tx.GetHash().ToString());
                    return state.DoS(100, error("ConnectBlock: inputs missing/spent"), REJECT_INVALID, "bad-txns-inputs-missingorspent");
                }

                // Check that transaction is BIP68 final
                // BIP68 lock checks (as opposed to nLockTime checks) must
                // be in ConnectBlock because they require the UTXO set
                prevheights.resize(tx.vin.size());
                for (size_t j = 0; j < tx.vin.size(); j++) {
                    prevheights[j] = view.AccessCoin(tx.vin[j].prevout).nHeight;
                }

                if (!SequenceLocks(tx, nLockTimeFlags, &prevheights, *pindex)) {
                    return state.DoS(100, error("%s: contains a non-BIP68-final transaction", __func__),
                                    REJECT_INVALID, "bad-txns-nonfinal");
                }
            }
        }

        // GetTransactionSigOpCost counts 3 types of sigops:
        // * legacy (always)
        // * p2sh (when P2SH enabled in flags and excludes coinbase)
        // * witness (when witness enabled in flags and excludes coinbase)
        nSigOpsCost += GetTransactionSigOpCost(tx, view, flags);
        if (nSigOpsCost > MAX_BLOCK_SIGOPS_COST)
            return state.DoS(100, error("ConnectBlock(): too many sigops"),
                             REJECT_INVALID, "bad-blk-sigops");

        txdata.emplace_back(tx);
        //fixme: (2.0.1) (PHASE4) (CODEBASE CLEANUP) - CheckInputs needs to run as well for witness coinbase (can we just run this whole block for witness coinbase?) - we already test this elsewhere so it would only be a cleanness improvement.
        if (!tx.IsCoinBase())
        {
            if (nWitnessCoinbaseIndex != 0 && i > nWitnessCoinbaseIndex)
            {
                nFeesPoW2Witness += view.GetValueIn(tx)-tx.GetValueOut();;
            }
            else
            {
                nFees += view.GetValueIn(tx)-tx.GetValueOut();
            }

            std::vector<CScriptCheck> vChecks;
            bool fCacheResults = fJustCheck; /* Don't cache results if we're actually connecting blocks (still consult the cache, though) */
            if (!CheckInputs(tx, state, view, fScriptChecks, flags, fCacheResults, txdata[i], &witnessBundles[i], nScriptCheckThreads ? &vChecks : NULL))
                return error("ConnectBlock(): CheckInputs on %s failed with %s",
                    tx.GetHash().ToString(), FormatStateMessage(state));
            control.Add(vChecks);
        }

        CTxUndo undoDummy;
        if (i > 0) {
            blockundo.vtxundo.push_back(CTxUndo());
        }
        UpdateCoins(tx, view, i == 0 ? undoDummy : blockundo.vtxundo.back(), pindex->nHeight);

        vPos.push_back(std::pair(tx.GetHash(), pos));
        pos.nTxOffset += ::GetSerializeSize(tx, SER_DISK, CLIENT_VERSION);
    }
    int64_t nTime3 = GetTimeMicros(); nTimeConnect += nTime3 - nTime2;
    LogPrint(BCLog::BENCH, "      - Connect %u transactions: %.2fms (%.3fms/tx, %.3fms/txin) [%.2fs]\n", (unsigned)block.vtx.size(), 0.001 * (nTime3 - nTime2), 0.001 * (nTime3 - nTime2) / block.vtx.size(), nInputs <= 1 ? 0 : 0.001 * (nTime3 - nTime2) / (nInputs-1), nTimeConnect * 0.000001);

    //fixme: (2.1) (CLEANUP) - We can remove this after 2.1 becomes active.

    //Until PoW2 activates mining subsidy remains full.
    //Phase 3 - miner mines 80 reward for himself and 20 reward for previous blocks witness...
    //Phase 4/5 - miner mines 80 reward for himself, witness 20 reward for himself (two seperate coinbases)
    CAmount nSubsidy = GetBlockSubsidy(pindex->nHeight);
    CAmount nSubsidyWitness = GetBlockSubsidyWitness(pindex->nHeight);

    //fixme: (2.0.1) Unit tests
    // Second block with a phase 3 parent up to and including first block with a phase 4 parent.
    // Coinbase of previous witness block embedded in coinbase of current PoW block.
    if (nPoW2PhaseGrandParent == 3 && nPoW2PhaseParent != 4)
    {
        unsigned int nWitnessCoinbasePayoutIndex = nEmbeddedWitnessCoinbaseIndex + 1;

        if (block.vtx[0]->vout.size()-1 < nWitnessCoinbasePayoutIndex)
            return state.DoS(100, error("ConnectBlock(): PoW2 phase 3 coinbase lacks witness payout)"), REJECT_INVALID, "bad-cb-nowitnesspayout");

        if (block.vtx[0]->vout[nWitnessCoinbasePayoutIndex].nValue != nSubsidyWitness)
            return state.DoS(100, error("ConnectBlock(): PoW2 phase 3 coinbase has incorrect witness payout amount [%d] [%d])", block.vtx[0]->vout[nWitnessCoinbasePayoutIndex].nValue, nSubsidyWitness), REJECT_INVALID, "bad-cb-badwitnesspayoutamount");
    }
    else if (nPoW2PhaseParent >= 4)
    {
        nSubsidy -= nSubsidyWitness;
    }

    if (block.nVersionPoW2Witness == 0)
    {
        // PoW block
        // Phase 4 + 5 - miner mines 80 reward instead of 100, so nothing to do here (GetBlockSubsidy returns correct amount)
    }
    else
    {
        // PoW2 block
        // Ensure witness coinbase is present and that it pays out the right amount.
        if (nPoW2PhaseParent >= 3)
        {
            CAmount nValIn = 0;
            for (auto output : blockundo.vtxundo[nWitnessCoinbaseIndex-1].vprevout)
            {
                if (output.out.nValue > 0)
                    nValIn += output.out.nValue;
            }

            nSubsidyWitness += nFeesPoW2Witness;
            if (block.vtx[nWitnessCoinbaseIndex]->GetValueOut() - nValIn > nSubsidyWitness)
            {
                return state.DoS(100, error("ConnectBlock(): PoW2 witness pays too much (actual=%d vs limit=%d)", block.vtx[nWitnessCoinbaseIndex]->GetValueOut(), nSubsidyWitness), REJECT_INVALID, "bad-witness-cb-amount");
            }

            if (nPoW2PhaseParent == 3)
            {
                if (block.vtx[nWitnessCoinbaseIndex]->vout.size() != 2)
                    return state.DoS(100, error("ConnectBlock(): PoW2 witness coinbase invalid vout size)"), REJECT_INVALID, "bad-witness-cb");

                if (block.vtx[nWitnessCoinbaseIndex]->vin.size() != 2)
                    return state.DoS(100, error("ConnectBlock(): PoW2 witness coinbase invalid vin size)"), REJECT_INVALID, "bad-witness-cb");

                if (!block.vtx[nWitnessCoinbaseIndex]->vin[0].prevout.IsNull())
                    return state.DoS(100, error("ConnectBlock(): PoW2 witness coinbase invalid prevout)"), REJECT_INVALID, "bad-witness-cb");

                if (block.vtx[nWitnessCoinbaseIndex]->vin[0].GetSequence(block.vtx[nWitnessCoinbaseIndex]->nVersion) != 0)
                    return state.DoS(100, error("ConnectBlock(): PoW2 witness coinbase invalid sequence)"), REJECT_INVALID, "bad-witness-cb");
            }
            else if (nPoW2PhaseParent >= 4)
            {
                //fixme: (2.0.1) (SEGSIG/POW2) - Triple check that there are no remaining tests that should go here.
                //Phase 4 has no coinbase restrictions
            }
        }
        else
        {
            assert(0);
        }
    }


    //fixme: (2.0.1) (SEGSIG/POW2) - Triple check that there are no remaining tests that should go here.

    CAmount expectedBlockReward = nFees + nSubsidy;
    CAmount actualBlockReward = block.vtx[0]->GetValueOut() ;
    if (actualBlockReward > expectedBlockReward)
    {
        return state.DoS(100, error("ConnectBlock(): coinbase pays too much (actual=%d vs limit=%d)", actualBlockReward, expectedBlockReward), REJECT_INVALID, "bad-cb-amount");
    }
    // From phase 3 onward we forbid miners from not claiming the full reward.
    if (nPoW2PhaseParent >= 3 && expectedBlockReward < actualBlockReward)
    {
        return state.DoS(100, error("ConnectBlock(): coinbase pays too little (actual=%d vs limit=%d)", actualBlockReward, expectedBlockReward), REJECT_INVALID, "bad-cb-amount");
    }

    if (!control.Wait())
        return state.DoS(100, error("%s: CheckQueue failed", __func__), REJECT_INVALID, "block-validation-failed");
    int64_t nTime4 = GetTimeMicros(); nTimeVerify += nTime4 - nTime2;
    LogPrint(BCLog::BENCH, "    - Verify %u txins: %.2fms (%.3fms/txin) [%.2fs]\n", nInputs - 1, 0.001 * (nTime4 - nTime2), nInputs <= 1 ? 0 : 0.001 * (nTime4 - nTime2) / (nInputs-1), nTimeVerify * 0.000001);

    if (fJustCheck)
        return true;

    // Write undo information to disk
    if (pindex->GetUndoPos().IsNull() || !pindex->IsValid(BLOCK_VALID_SCRIPTS))
    {
        if (pindex->GetUndoPos().IsNull()) {
            CDiskBlockPos _pos;
            if (!FindUndoPos(state, pindex->nFile, _pos, ::GetSerializeSize(blockundo, SER_DISK, CLIENT_VERSION) + 40))
                return error("ConnectBlock(): FindUndoPos failed");
            if (!blockStore.UndoWriteToDisk(blockundo, _pos, pindex->pprev->GetBlockHashPoW2(), chainparams.MessageStart()))
                return AbortNode(state, "Failed to write undo data");

            // update nUndoPos in block index
            pindex->nUndoPos = _pos.nPos;
            pindex->nStatus |= BLOCK_HAVE_UNDO;
        }

        pindex->RaiseValidity(BLOCK_VALID_SCRIPTS);
        if (chain == chainActive)
        {
            setDirtyBlockIndex.insert(pindex);
        }
    }

    if (fTxIndex)
        if (!pblocktree->WriteTxIndex(vPos))
            return AbortNode(state, "Failed to write transaction index");

    // add this block to the view's block chain
    view.SetBestBlock(pindex->GetBlockHashPoW2());

    int64_t nTime5 = GetTimeMicros(); nTimeIndex += nTime5 - nTime4;
    LogPrint(BCLog::BENCH, "    - Index writing: %.2fms [%.2fs]\n", 0.001 * (nTime5 - nTime4), nTimeIndex * 0.000001);

    int64_t nTime6 = GetTimeMicros(); nTimeCallbacks += nTime6 - nTime5;
    LogPrint(BCLog::BENCH, "    - Callbacks: %.2fms [%.2fs]\n", 0.001 * (nTime6 - nTime5), nTimeCallbacks * 0.000001);

    return true;
}

void FindFilesToPruneExplicit(std::set<int>& setFilesToPrune, unsigned int nPruneHeight)
{
    LOCK2(cs_main, cs_LastBlockFile);

    int count=0;
    for (int fileNumber = 0; fileNumber < nLastBlockFile; fileNumber++) {
        if (vinfoBlockFile[fileNumber].nSize == 0 || vinfoBlockFile[fileNumber].nHeightLast >= nPruneHeight)
            continue;
        PruneOneBlockFile(fileNumber);
        setFilesToPrune.insert(fileNumber);
        count++;
    }
    LogPrint(BCLog::PRUNE, "Prune (Manual): prune_height=%d removed %d blk/rev pairs\n", nPruneHeight, count);
}

/**
 * Update the on-disk chain state.
 * The caches and indexes are flushed depending on the mode we're called with
 * if they're too large, if it's been a while since the last write,
 * or always and in all cases if we're in prune mode and are deleting files.
 */
bool FlushStateToDisk(const CChainParams& chainparams, CValidationState &state, FlushStateMode mode, int nManualPruneHeight, bool fFlushPartialSync) {
    int64_t nMempoolUsage = mempool.DynamicMemoryUsage();
    LOCK2(cs_main, cs_LastBlockFile);
    static int64_t nLastWrite = 0;
    static int64_t nLastFlush = 0;
    static int64_t nLastSetChain = 0;
    std::set<int> setFilesToPrune;
    bool fFlushForPrune = false;
    try {
    if (fPruneMode && (fCheckForPruning || nManualPruneHeight > 0) && !fReindex)
    {
        if (nManualPruneHeight > 0 && !fFlushPartialSync)
        {
            FindFilesToPruneManual(setFilesToPrune, nManualPruneHeight);
        }
        else if (nManualPruneHeight > 0 && fFlushPartialSync)
        {
            FindFilesToPruneExplicit(setFilesToPrune, nManualPruneHeight);
        }
        else
        {
            FindFilesToPrune(setFilesToPrune, chainparams.PruneAfterHeight());
            fCheckForPruning = false;
        }

        if (!setFilesToPrune.empty()) {
            fFlushForPrune = true;
            if (!fHavePruned) {
                pblocktree->WriteFlag("prunedblockfiles", true);
                fHavePruned = true;
            }
        }
    }
    int64_t nNow = GetTimeMicros();
    // Avoid writing/flushing immediately after startup.
    if (nLastWrite == 0) {
        nLastWrite = nNow;
    }
    if (nLastFlush == 0) {
        nLastFlush = nNow;
    }
    if (nLastSetChain == 0) {
        nLastSetChain = nNow;
    }
    int64_t nMempoolSizeMax = GetArg("-maxmempool", DEFAULT_MAX_MEMPOOL_SIZE) * 1000000;
    int64_t cacheSize = pcoinsTip->DynamicMemoryUsage() * DB_PEAK_USAGE_FACTOR;
    int64_t nTotalSpace = nCoinCacheUsage + std::max<int64_t>(nMempoolSizeMax - nMempoolUsage, 0);
    // The cache is large and we're within 10% and 10 MiB of the limit, but we have time now (not in the middle of a block processing).
    bool fCacheLarge = mode == FLUSH_STATE_PERIODIC && cacheSize > std::max((9 * nTotalSpace) / 10, nTotalSpace - MAX_BLOCK_COINSDB_USAGE * 1024 * 1024);
    // The cache is over the limit, we have to write now.
    bool fCacheCritical = mode == FLUSH_STATE_IF_NEEDED && cacheSize > nTotalSpace;
    // It's been a while since we wrote the block index to disk. Do this frequently, so we don't need to redownload after a crash.
    bool fPeriodicWrite = mode == FLUSH_STATE_PERIODIC && nNow > nLastWrite + (int64_t)DATABASE_WRITE_INTERVAL * 1000000;
    // It's been very long since we flushed the cache. Do this infrequently, to optimize cache usage.
    bool fPeriodicFlush = mode == FLUSH_STATE_PERIODIC && nNow > nLastFlush + (int64_t)DATABASE_FLUSH_INTERVAL * 1000000;
    // Combine all conditions that result in a full cache flush.
    bool fDoFullFlush = (mode == FLUSH_STATE_ALWAYS) || fCacheLarge || fCacheCritical || fPeriodicFlush || fFlushForPrune;
    // Write blocks and block index to disk.
    if (fDoFullFlush || fPeriodicWrite) {
        // Depend on nMinDiskSpace to ensure we can write block index
        if (!CheckDiskSpace(0))
            return state.Error("out of disk space");
        // First make sure all block and undo data is flushed to disk.
        FlushBlockFile();
        // Then update all block file information (which may refer to block and undo files).
        {
            std::vector<std::pair<int, const CBlockFileInfo*> > vFiles;
            vFiles.reserve(setDirtyFileInfo.size());
            for (std::set<int>::iterator it = setDirtyFileInfo.begin(); it != setDirtyFileInfo.end(); ) {
                vFiles.push_back(std::pair(*it, &vinfoBlockFile[*it]));
                setDirtyFileInfo.erase(it++);
            }

            // prune block index for partial sync
            if (fFlushPartialSync) {
                std::vector<const CBlockIndex*> removals;
                for(const auto& it: mapBlockIndex)
                {
                    CBlockIndex* index = it.second;
                    if (   (index->nHeight < nManualPruneHeight || !partialChain.Contains(index))
                           && index != chainActive.Genesis())
                    {
                        removals.push_back(index);
                        setDirtyBlockIndex.erase(index); // prevent pruned indexes to be rewritten
                    }
                }

                LogPrintf("%s: deleting %d block indexes, prune height = %d\n", __func__, removals.size(), nManualPruneHeight);

                // Will usually have at least one index to prune, because when loading the index
                // of the previous block of partial chain start is added. This is ok.
                if (removals.size() > 0)
                {
                    pblocktree->EraseBatchSync(removals);
                }
            }

            std::vector<const CBlockIndex*> vBlocks;
            vBlocks.reserve(setDirtyBlockIndex.size());
            for (std::set<CBlockIndex*>::iterator it = setDirtyBlockIndex.begin(); it != setDirtyBlockIndex.end(); ) {
                vBlocks.push_back(*it);
                setDirtyBlockIndex.erase(it++);
            }
            if (!pblocktree->WriteBatchSync(vFiles, nLastBlockFile, vBlocks)) {
                return AbortNode(state, "Failed to write to block index database");
            }
        }
        // Finally remove any pruned files
        if (fFlushForPrune)
            blockStore.UnlinkPrunedFiles(setFilesToPrune);
        nLastWrite = nNow;
    }
    // Flush best chain related state. This can only be done if the blocks / block index write was also done.
    if (fDoFullFlush) {
        // Typical Coin structures on disk are around 48 bytes in size.
        // Pushing a new one to the database can cause it to be written
        // twice (once in the log, and once in the tables). This is already
        // an overestimation, as most will delete an existing entry or
        // overwrite one. Still, use a conservative safety factor of 2.
        if (!CheckDiskSpace(48 * 2 * 2 * pcoinsTip->GetCacheSize()))
            return state.Error("out of disk space");
        // Flush the chainstate (which may refer to block index entries).
        if (!pcoinsTip->Flush())
            return AbortNode(state, "Failed to write to coin database");
        nLastFlush = nNow;
    }
    if (   !fFlushPartialSync
        && (fDoFullFlush || ((mode == FLUSH_STATE_ALWAYS || mode == FLUSH_STATE_PERIODIC) && nNow > nLastSetChain + (int64_t)DATABASE_WRITE_INTERVAL * 1000000))) {
        // Update best block in wallet (so we can detect restored wallets).
        GetMainSignals().SetBestChain(chainActive.GetLocatorPoW2());
        nLastSetChain = nNow;
    }
    } catch (const std::runtime_error& e) {
        return AbortNode(state, std::string("System error while flushing: ") + e.what());
    }
    return true;
}

void FlushStateToDisk() {
    CValidationState state;
    const CChainParams& chainparams = Params();
    FlushStateToDisk(chainparams, state, FLUSH_STATE_ALWAYS);
}

void PruneAndFlush() {
    CValidationState state;
    fCheckForPruning = true;
    const CChainParams& chainparams = Params();
    FlushStateToDisk(chainparams, state, FLUSH_STATE_NONE);
}

static void DoWarning(const std::string& strWarning)
{
    static bool fWarned = false;
    SetMiscWarning(strWarning);
    if (!fWarned)
    {
        CAlert::Notify(strWarning, true, true);
        fWarned = true;
    }
}

/** Update chainActive and related internal data structures. */
void static UpdateTip(CBlockIndex *pindexNew, const CChainParams& chainParams) {
    chainActive.SetTip(pindexNew);

    // New best block
    mempool.AddTransactionsUpdated(1);

    cvBlockChange.notify_all();

    std::vector<std::string> warningMessages;
    if (!IsInitialBlockDownload())
    {
        int nUpgraded = 0;
        const CBlockIndex* pindex = chainActive.Tip();
        for (int bit = 0; bit < VERSIONBITS_NUM_BITS; bit++)
        {
            WarningBitsConditionChecker checker(bit);
            ThresholdState state = checker.GetStateFor(pindex, chainParams.GetConsensus(), warningcache[bit]);
            // fixme: (2.1) We can remove
            // Bypass invalid warnings for phase 4 activation 
            if (bit == chainParams.GetConsensus().vDeployments[Consensus::DEPLOYMENT_POW2_PHASE4].bit)
                continue;
            if (state == THRESHOLD_ACTIVE || state == THRESHOLD_LOCKED_IN)
            {
                const std::string strWarning = strprintf(_("Warning: unknown new rules activated (versionbit %i)"), bit);
                if (state == THRESHOLD_ACTIVE)
                {
                    DoWarning(strWarning);
                }
                else
                {
                    warningMessages.push_back(strWarning);
                }
            }
        }
        // Check the version of the last 100 blocks to see if we need to upgrade:
        for (int i = 0; i < 100 && pindex != NULL; i++)
        {
            int32_t nExpectedVersion = ComputeBlockVersion(pindex->pprev, chainParams.GetConsensus());
            if (pindex->nVersion > VERSIONBITS_LAST_OLD_BLOCK_VERSION && (pindex->nVersion & ~nExpectedVersion) != 0)
                ++nUpgraded;
            pindex = pindex->pprev;
        }
        if (nUpgraded > 0)
            warningMessages.push_back(strprintf(_("%d of last 100 blocks have unexpected version"), nUpgraded));
        if (nUpgraded > 100/2)
        {
            std::string strWarning = _("Warning: Unknown block versions in chain! It's possible unknown rules are in effect");
            // notify GetWarnings(), called by Qt and the JSON-RPC code to warn the user:
            DoWarning(strWarning);
        }
    }
    if(!gbMinimalLogging || !warningMessages.empty() || IsArgSet("-testnet") || chainActive.Height() % 1000 == 0 || chainActive.Height() > 700000)
    {
        LogPrintf("%s: new best=%s height=%d version=0x%08x versionpow2=0x%08x log2_work=%.8g tx=%lu date='%s' progress=%f cache=%.1fMiB(%utxo)", __func__,
            chainActive.Tip()->GetBlockHashPoW2().ToString(), chainActive.Height(), chainActive.Tip()->nVersion, chainActive.Tip()->nVersionPoW2Witness,
            log(chainActive.Tip()->nChainWork.getdouble())/log(2.0), (unsigned long)chainActive.Tip()->nChainTx,
            DateTimeStrFormat("%Y-%m-%d %H:%M:%S", chainActive.Tip()->GetBlockTime()),
            GuessVerificationProgress(chainParams.TxData(), chainActive.Tip()), pcoinsTip->DynamicMemoryUsage() * (1.0 / (1<<20)), pcoinsTip->GetCacheSize());
        if (!warningMessages.empty())
            LogPrintf(" warning='%s'", boost::algorithm::join(warningMessages, ", "));
        LogPrintf("\n");
    }
}

/** Disconnect chainActive's tip.
  * After calling, the mempool will be in an inconsistent state, with
  * transactions from disconnected blocks being added to disconnectpool.  You
  * should make the mempool consistent again by calling UpdateMempoolForReorg.
  * with cs_main held.
  *
  * If disconnectpool is NULL, then no disconnected transactions are added to
  * disconnectpool (note that the caller is responsible for mempool consistency
  * in any case).
  */
bool static DisconnectTip(CValidationState& state, const CChainParams& chainparams, DisconnectedBlockTransactions *disconnectpool)
{
    AssertLockHeld(cs_main); // Required for ReadBlockFromDisk.

    CBlockIndex *pindexDelete = chainActive.Tip();
    assert(pindexDelete);
    // Read block from disk.
    std::shared_ptr<CBlock> pblock = std::make_shared<CBlock>();
    CBlock& block = *pblock;
    if (!ReadBlockFromDisk(block, pindexDelete, chainparams))
        return AbortNode(state, "Failed to read block");
    // Apply the block atomically to the chain state.
    int64_t nStart = GetTimeMicros();
    {
        CCoinsViewCache view(pcoinsTip);
        if (DisconnectBlock(block, pindexDelete, view) != DISCONNECT_OK)
            return error("DisconnectTip(): DisconnectBlock %s failed", pindexDelete->GetBlockHashPoW2().ToString());
        bool flushed = view.Flush();
        assert(flushed);
    }
    LogPrint(BCLog::BENCH, "- Disconnect block: %.2fms\n", (GetTimeMicros() - nStart) * 0.001);
    // Write the chain state to disk, if necessary.
    if (!FlushStateToDisk(chainparams, state, FLUSH_STATE_IF_NEEDED))
        return false;

    if (disconnectpool) {
        // Save transactions to re-add to mempool at end of reorg
        for (auto it = block.vtx.rbegin(); it != block.vtx.rend(); ++it) {
            disconnectpool->addTransaction(*it);
        }
        while (disconnectpool->DynamicMemoryUsage() > MAX_DISCONNECTED_TX_POOL_SIZE * 1000) {
            // Drop the earliest entry, and remove its children from the mempool.
            auto it = disconnectpool->queuedTx.get<insertion_order>().begin();
            mempool.removeRecursive(**it, MemPoolRemovalReason::REORG);
            disconnectpool->removeEntry(it);
        }
    }

    // Update chainActive and related variables.
    UpdateTip(pindexDelete->pprev, chainparams);
    // Let wallets know transactions went from 1-confirmed to
    // 0-confirmed or conflicted:
    GetMainSignals().BlockDisconnected(pblock);
    return true;
}

static int64_t nTimeReadFromDisk = 0;
static int64_t nTimeConnectTotal = 0;
static int64_t nTimeFlush = 0;
static int64_t nTimeChainState = 0;
static int64_t nTimePostConnect = 0;

struct PerBlockConnectTrace {
    CBlockIndex* pindex = NULL;
    std::shared_ptr<const CBlock> pblock;
    std::shared_ptr<std::vector<CTransactionRef>> conflictedTxs;
    PerBlockConnectTrace() : conflictedTxs(std::make_shared<std::vector<CTransactionRef>>()) {}
};
/**
 * Used to track blocks whose transactions were applied to the UTXO state as a
 * part of a single ActivateBestChainStep call.
 *
 * This class also tracks transactions that are removed from the mempool as
 * conflicts (per block) and can be used to pass all those transactions
 * through SyncTransaction.
 *
 * This class assumes (and asserts) that the conflicted transactions for a given
 * block are added via mempool callbacks prior to the BlockConnected() associated
 * with those transactions. If any transactions are marked conflicted, it is
 * assumed that an associated block will always be added.
 *
 * This class is single-use, once you call GetBlocksConnected() you have to throw
 * it away and make a new one.
 */
class ConnectTrace {
private:
    std::vector<PerBlockConnectTrace> blocksConnected;
    CTxMemPool &pool;

public:
    ConnectTrace(CTxMemPool &_pool) : blocksConnected(1), pool(_pool) {
        pool.NotifyEntryRemoved.connect(boost::bind(&ConnectTrace::NotifyEntryRemoved, this, _1, _2));
    }

    ~ConnectTrace() {
        pool.NotifyEntryRemoved.disconnect(boost::bind(&ConnectTrace::NotifyEntryRemoved, this, _1, _2));
    }

    void BlockConnected(CBlockIndex* pindex, std::shared_ptr<const CBlock> pblock) {
        assert(!blocksConnected.back().pindex);
        assert(pindex);
        assert(pblock);
        blocksConnected.back().pindex = pindex;
        blocksConnected.back().pblock = std::move(pblock);
        blocksConnected.emplace_back();
    }

    std::vector<PerBlockConnectTrace>& GetBlocksConnected() {
        // We always keep one extra block at the end of our list because
        // blocks are added after all the conflicted transactions have
        // been filled in. Thus, the last entry should always be an empty
        // one waiting for the transactions from the next block. We pop
        // the last entry here to make sure the list we return is sane.
        assert(!blocksConnected.back().pindex);
        assert(blocksConnected.back().conflictedTxs->empty());
        blocksConnected.pop_back();
        return blocksConnected;
    }

    void NotifyEntryRemoved(CTransactionRef txRemoved, MemPoolRemovalReason reason) {
        assert(!blocksConnected.back().pindex);
        if (reason == MemPoolRemovalReason::CONFLICT) {
            blocksConnected.back().conflictedTxs->emplace_back(std::move(txRemoved));
        }
    }
};

/**
 * Connect a new block to chainActive. pblock is either NULL or a pointer to a CBlock
 * corresponding to pindexNew, to bypass loading it again from disk.
 *
 * The block is added to connectTrace if connection succeeds.
 */
bool static ConnectTip(CValidationState& state, const CChainParams& chainparams, CBlockIndex* pindexNew, const std::shared_ptr<const CBlock>& pblock, ConnectTrace& connectTrace, DisconnectedBlockTransactions &disconnectpool)
{
    AssertLockHeld(cs_main); // Required for ReadBlockFromDisk.

    assert(pindexNew->pprev == chainActive.Tip());
    // Read block from disk.
    int64_t nTime1 = GetTimeMicros();
    std::shared_ptr<const CBlock> pthisBlock;
    if (!pblock) {
        std::shared_ptr<CBlock> pblockNew = std::make_shared<CBlock>();
        if (!ReadBlockFromDisk(*pblockNew, pindexNew, chainparams))
            return AbortNode(state, "Failed to read block");
        pthisBlock = pblockNew;
    } else {
        pthisBlock = pblock;
    }
    const CBlock& blockConnecting = *pthisBlock;
    // Apply the block atomically to the chain state.
    int64_t nTime2 = GetTimeMicros(); nTimeReadFromDisk += nTime2 - nTime1;
    int64_t nTime3;
    LogPrint(BCLog::BENCH, "  - Load block from disk: %.2fms [%.2fs]\n", (nTime2 - nTime1) * 0.001, nTimeReadFromDisk * 0.000001);
    {
        CCoinsViewCache view(pcoinsTip);
        bool rv = ConnectBlock(chainActive, blockConnecting, state, pindexNew, view, chainparams);
        GetMainSignals().BlockChecked(blockConnecting, state);
        if (!rv) {
            if (state.IsInvalid())
                InvalidBlockFound(pindexNew, state);
            return error("ConnectTip(): ConnectBlock %s failed", pindexNew->GetBlockHashPoW2().ToString());
        }
        nTime3 = GetTimeMicros(); nTimeConnectTotal += nTime3 - nTime2;
        LogPrint(BCLog::BENCH, "  - Connect total: %.2fms [%.2fs]\n", (nTime3 - nTime2) * 0.001, nTimeConnectTotal * 0.000001);
        bool flushed = view.Flush();
        assert(flushed);
    }
    int64_t nTime4 = GetTimeMicros(); nTimeFlush += nTime4 - nTime3;
    LogPrint(BCLog::BENCH, "  - Flush: %.2fms [%.2fs]\n", (nTime4 - nTime3) * 0.001, nTimeFlush * 0.000001);
    // Write the chain state to disk, if necessary.
    if (!FlushStateToDisk(chainparams, state, FLUSH_STATE_IF_NEEDED))
        return false;
    int64_t nTime5 = GetTimeMicros(); nTimeChainState += nTime5 - nTime4;
    LogPrint(BCLog::BENCH, "  - Writing chainstate: %.2fms [%.2fs]\n", (nTime5 - nTime4) * 0.001, nTimeChainState * 0.000001);
    // Remove conflicting transactions from the mempool.;
    mempool.removeForBlock(blockConnecting.vtx, pindexNew->nHeight);
    disconnectpool.removeForBlock(blockConnecting.vtx);
    // Update chainActive & related variables.
    UpdateTip(pindexNew, chainparams);

    int64_t nTime6 = GetTimeMicros(); nTimePostConnect += nTime6 - nTime5; nTimeTotal += nTime6 - nTime1;
    LogPrint(BCLog::BENCH, "  - Connect postprocess: %.2fms [%.2fs]\n", (nTime6 - nTime5) * 0.001, nTimePostConnect * 0.000001);
    LogPrint(BCLog::BENCH, "- Connect block: %.2fms [%.2fs]\n", (nTime6 - nTime1) * 0.001, nTimeTotal * 0.000001);

    connectTrace.BlockConnected(pindexNew, std::move(pthisBlock));
    return true;
}

/**
 * Return the tip of the chain with the most work in it, that isn't
 * known to be invalid (it's however far from certain to be valid).
 */
static CBlockIndex* FindMostWorkChain() {
    LOCK(cs_main);
    do {
        CBlockIndex *pindexNew = NULL;

        // Find the best candidate header.
        {
            std::set<CBlockIndex*, CBlockIndexWorkComparator>::reverse_iterator it = setBlockIndexCandidates.rbegin();
            if (it == setBlockIndexCandidates.rend())
                return NULL;
            pindexNew = *it;
        }

        // Check whether all blocks on the path between the currently active chain and the candidate are valid.
        // Just going until the active chain is an optimization, as we know all blocks in it are valid already.
        CBlockIndex *pindexTest = pindexNew;
        bool fInvalidAncestor = false;
        while (pindexTest && !chainActive.Contains(pindexTest)) {
            assert(pindexTest->nChainTx || pindexTest->nHeight == 0);

            // Pruned nodes may have entries in setBlockIndexCandidates for
            // which block files have been deleted.  Remove those as candidates
            // for the most work chain if we come across them; we can't switch
            // to a chain unless we have all the non-active-chain parent blocks.
            bool fFailedChain = pindexTest->nStatus & BLOCK_FAILED_MASK;
            bool fMissingData = !(pindexTest->nStatus & BLOCK_HAVE_DATA);
            if (fFailedChain || fMissingData) {
                // Candidate chain is not usable (either invalid or missing data)
                if (fFailedChain && (pindexBestInvalid == NULL || pindexNew->nChainWork > pindexBestInvalid->nChainWork))
                    pindexBestInvalid = pindexNew;
                CBlockIndex *pindexFailed = pindexNew;
                // Remove the entire chain from the set.
                while (pindexTest != pindexFailed) {
                    if (fFailedChain) {
                        pindexFailed->nStatus |= BLOCK_FAILED_CHILD;
                    } else if (fMissingData) {
                        // If we're missing data, then add back to mapBlocksUnlinked,
                        // so that if the block arrives in the future we can try adding
                        // to setBlockIndexCandidates again.
                        mapBlocksUnlinked.insert(std::pair(pindexFailed->pprev, pindexFailed));
                    }
                    setBlockIndexCandidates.erase(pindexFailed);
                    pindexFailed = pindexFailed->pprev;
                }
                setBlockIndexCandidates.erase(pindexTest);
                fInvalidAncestor = true;
                break;
            }
            pindexTest = pindexTest->pprev;
        }
        if (!fInvalidAncestor)
            return pindexNew;
    } while(true);
}

/** Delete all entries in setBlockIndexCandidates that are worse than the current tip. */
static void PruneBlockIndexCandidates() {
    LOCK(cs_main);
    // Note that we can't delete the current block itself, as we may need to return to it later in case a
    // reorganization to a better block fails.
    std::set<CBlockIndex*, CBlockIndexWorkComparator>::iterator it = setBlockIndexCandidates.begin();

    //NB! We don't prune blocks that are the same height as the current tip when the current tip is PoW.
    //The reason for this is that we must consider all such blocks as witness candidates even if they are of lower weight - in case the higher weight block has an "absent" witness.
    while ( ( it != setBlockIndexCandidates.end() ) && ( (*it)->nChainWork < chainActive.Tip()->nChainWork ) && ( (*it)->nHeight < chainActive.Tip()->nHeight /*|| (*it)->nVersionPoW2Witness == 0*/ ) ) {
        setBlockIndexCandidates.erase(it++);
    }
    // Either the current tip or a successor of it we're working towards is left in setBlockIndexCandidates.
    assert(!setBlockIndexCandidates.empty());
}

/**
 * Try to make some progress towards making pindexMostWork the active block.
 * pblock is either NULL or a pointer to a CBlock corresponding to pindexMostWork.
 */
static bool ActivateBestChainStep(CValidationState& state, const CChainParams& chainparams, CBlockIndex* pindexMostWork, const std::shared_ptr<const CBlock>& pblock, bool& fInvalidFound, ConnectTrace& connectTrace)
{
    AssertLockHeld(cs_main);
    const CBlockIndex *pindexOldTip = chainActive.Tip();
    const CBlockIndex *pindexFork = chainActive.FindFork(pindexMostWork);

    // Disconnect active blocks which are no longer in the best chain.
    bool fBlocksDisconnected = false;
    DisconnectedBlockTransactions disconnectpool;
    while (chainActive.Tip() && chainActive.Tip() != pindexFork) {
        if (!DisconnectTip(state, chainparams, &disconnectpool)) {
            // This is likely a fatal error, but keep the mempool consistent,
            // just in case. Only remove from the mempool in this case.
            UpdateMempoolForReorg(disconnectpool, false);
            return false;
        }
        fBlocksDisconnected = true;
    }

    // Build list of new blocks to connect.
    std::vector<CBlockIndex*> vpindexToConnect;
    bool fContinue = true;
    int nHeight = pindexFork ? pindexFork->nHeight : -1;
    while (fContinue && nHeight != pindexMostWork->nHeight) {
        // Don't iterate the entire list of potential improvements toward the best tip, as we likely only need
        // a few blocks along the way.
        int nTargetHeight = std::min(nHeight + 32, pindexMostWork->nHeight);
        vpindexToConnect.clear();
        vpindexToConnect.reserve(nTargetHeight - nHeight);
        CBlockIndex *pindexIter = pindexMostWork->GetAncestor(nTargetHeight);
        while (pindexIter && pindexIter->nHeight != nHeight) {
            vpindexToConnect.push_back(pindexIter);
            pindexIter = pindexIter->pprev;
        }
        nHeight = nTargetHeight;

        // Connect new blocks.
        BOOST_REVERSE_FOREACH(CBlockIndex *pindexConnect, vpindexToConnect) {
            if (!ConnectTip(state, chainparams, pindexConnect, pindexConnect == pindexMostWork ? pblock : std::shared_ptr<const CBlock>(), connectTrace, disconnectpool)) {
                if (state.IsInvalid()) {
                    // The block violates a consensus rule.
                    if (!state.CorruptionPossible())
                        InvalidChainFound(vpindexToConnect.back());
                    state = CValidationState();
                    fInvalidFound = true;
                    fContinue = false;
                    break;
                } else {
                    // A system error occurred (disk space, database error, ...).
                    // Make the mempool consistent with the current tip, just in case
                    // any observers try to use it before shutdown.
                    UpdateMempoolForReorg(disconnectpool, false);
                    return false;
                }
            } else {
                PruneBlockIndexCandidates();
                if (!pindexOldTip || chainActive.Tip()->nChainWork > pindexOldTip->nChainWork) {
                    // We're in a better position than we were. Return temporarily to release the lock.
                    fContinue = false;
                    break;
                }
            }
        }
    }

    if (fBlocksDisconnected) {
        // If any blocks were disconnected, disconnectpool may be non empty.  Add
        // any disconnected transactions back to the mempool.
        UpdateMempoolForReorg(disconnectpool, true);
    }
    mempool.check(pcoinsTip);

    // Callbacks/notifications for a new best chain.
    if (fInvalidFound)
        CheckForkWarningConditionsOnNewFork(vpindexToConnect.back());
    else
        CheckForkWarningConditions();

    return true;
}

// Requires cs_main
void DeactivatePartialSync()
{
    LogPrintf("Transition partial into full sync height = %d\n", chainActive.Height());

    // clear partial chain => !IsPartialSyncActive()
    partialChain.SetTip(nullptr);
    partialChain.SetHeightOffset(0);
    pindexBestPartial = nullptr;

    // clear partial chain from blockindex (not can trigger partial sync on next session)
    for (auto& it: mapBlockIndex) {
        CBlockIndex* idx = it.second;
        if (idx->IsPartialValid())
        {
            idx->nStatus &= ~BLOCK_PARTIAL_MASK;
            setDirtyBlockIndex.insert(idx);
        }
    }

    // notify and stop future notifications
    headerTipSignal(nullptr);
    headerTipSignal.disconnect_all_slots();

    // clear mempool to avoid complexity of transitioning partial validated mempool entries
    mempool.clear();
}

/**
 * Make the best chain active, in multiple steps. The result is either failure
 * or an activated best chain. pblock is either NULL or a pointer to a block
 * that is already loaded (to avoid loading it again from disk).
 */
bool ActivateBestChain(CValidationState &state, const CChainParams& chainparams, std::shared_ptr<const CBlock> pblock) {
    // Note that while we're often called here from ProcessNewBlock, this is
    // far from a guarantee. Things in the P2P/RPC will often end up calling
    // us in the middle of ProcessNewBlock - do not assume pblock is set
    // sanely for performance or correctness!

    CBlockIndex *pindexMostWork = NULL;
    CBlockIndex *pindexNewTip = NULL;
    int nStopAtHeight = GetArg("-stopatheight", DEFAULT_STOPATHEIGHT);
    do {
        boost::this_thread::interruption_point();
        if (ShutdownRequested())
            break;

        const CBlockIndex *pindexFork;
        bool fInitialDownload;
        {
            #ifdef ENABLE_WALLET
            LOCK2(cs_main, pactiveWallet?&pactiveWallet->cs_wallet:NULL);
            #else
            LOCK(cs_main);
            #endif

            ConnectTrace connectTrace(mempool); // Destructed before cs_main is unlocked

            CBlockIndex *pindexOldTip = chainActive.Tip();
            if (pindexMostWork == NULL) {
                pindexMostWork = FindMostWorkChain();
            }

            // Gulden - This is PoW2 related (unwitnessed blocks sit ahead of the chain tip with 0 extra chain work until they are activated).
            if (pindexMostWork && pindexMostWork->pprev && pindexMostWork->nChainWork == pindexMostWork->pprev->nChainWork)
                pindexMostWork = pindexMostWork->pprev;


            // Whether we have anything to do at all.
            if (pindexMostWork == NULL || pindexMostWork == chainActive.Tip())
                return true;

            bool fInvalidFound = false;
            std::shared_ptr<const CBlock> nullBlockPtr;
            if (!ActivateBestChainStep(state, chainparams, pindexMostWork, pblock && pblock->GetHashPoW2() == pindexMostWork->GetBlockHashPoW2() ? pblock : nullBlockPtr, fInvalidFound, connectTrace))
                return false;

            if (fInvalidFound) {
                // Wipe cache, we may need another branch now.
                pindexMostWork = NULL;
            }
            pindexNewTip = chainActive.Tip();
            pindexFork = chainActive.FindFork(pindexOldTip);
            fInitialDownload = IsInitialBlockDownload();

            for (const PerBlockConnectTrace& trace : connectTrace.GetBlocksConnected()) {
                assert(trace.pblock && trace.pindex);
                GetMainSignals().BlockConnected(trace.pblock, trace.pindex, *trace.conflictedTxs);
            }
        }
        // When we reach this point, we switched to a new tip (stored in pindexNewTip).

        if (IsPartialSyncActive() && chainActive.Height() >= partialChain.Height())
            DeactivatePartialSync();

        // Notifications/callbacks that can run without cs_main
        // Notify external listeners about the new tip.
        GetMainSignals().UpdatedBlockTip(pindexNewTip, pindexFork, fInitialDownload);

        // Always notify the UI if a new block tip was connected
        if (pindexFork != pindexNewTip) {
            uiInterface.NotifyBlockTip(fInitialDownload, pindexNewTip);
        }

        if (nStopAtHeight && pindexNewTip && pindexNewTip->nHeight >= nStopAtHeight)
            GuldenAppManager::gApp->shutdown();
    } while (pindexNewTip != pindexMostWork);
    CheckBlockIndex(chainparams.GetConsensus());

    // Write changes periodically to disk, after relay.
    if (!FlushStateToDisk(chainparams, state, FLUSH_STATE_PERIODIC)) {
        return false;
    }

    return true;
}


bool PreciousBlock(CValidationState& state, const CChainParams& params, CBlockIndex *pindex)
{
    {
        LOCK(cs_main);
        if (pindex->nChainWork < chainActive.Tip()->nChainWork) {
            // Nothing to do, this block is not at the tip.
            return true;
        }
        if (chainActive.Tip()->nChainWork > nLastPreciousChainwork) {
            // The chain has been extended since the last call, reset the counter.
            nBlockReverseSequenceId = -1;
        }
        nLastPreciousChainwork = chainActive.Tip()->nChainWork;
        setBlockIndexCandidates.erase(pindex);
        pindex->nSequenceId = nBlockReverseSequenceId;
        if (nBlockReverseSequenceId > std::numeric_limits<int32_t>::min()) {
            // We can't keep reducing the counter if somebody really wants to
            // call preciousblock 2**31-1 times on the same set of tips...
            nBlockReverseSequenceId--;
        }
        if (pindex->IsValid(BLOCK_VALID_TRANSACTIONS) && pindex->nChainTx) {
            if (!gbMinimalLogging)
                LogPrintf("PreciousBlock: New index candidate: [%s] [%d]\n", pindex->GetBlockHashPoW2().ToString(), pindex->nHeight);
            setBlockIndexCandidates.insert(pindex);
            PruneBlockIndexCandidates();
        }
    }

    return ActivateBestChain(state, params);
}

bool InvalidateBlock(CValidationState& state, const CChainParams& chainparams, CBlockIndex *pindex)
{
    AssertLockHeld(cs_main);

    // Mark the block itself as invalid.
    pindex->nStatus |= BLOCK_FAILED_VALID;
    setDirtyBlockIndex.insert(pindex);
    setBlockIndexCandidates.erase(pindex);

    DisconnectedBlockTransactions disconnectpool;
    while (chainActive.Contains(pindex)) {
        CBlockIndex *pindexWalk = chainActive.Tip();
        pindexWalk->nStatus |= BLOCK_FAILED_CHILD;
        setDirtyBlockIndex.insert(pindexWalk);
        setBlockIndexCandidates.erase(pindexWalk);
        // ActivateBestChain considers blocks already in chainActive
        // unconditionally valid already, so force disconnect away from it.
        if (!DisconnectTip(state, chainparams, &disconnectpool)) {
            // It's probably hopeless to try to make the mempool consistent
            // here if DisconnectTip failed, but we can try.
            UpdateMempoolForReorg(disconnectpool, false);
            return false;
        }
    }

    // DisconnectTip will add transactions to disconnectpool; try to add these
    // back to the mempool.
    UpdateMempoolForReorg(disconnectpool, true);

    // The resulting new best tip may not be in setBlockIndexCandidates anymore, so
    // add it again.
    BlockMap::iterator it = mapBlockIndex.begin();
    while (it != mapBlockIndex.end()) {
        if (it->second->IsValid(BLOCK_VALID_TRANSACTIONS) && it->second->nChainTx && !setBlockIndexCandidates.value_comp()(it->second, chainActive.Tip())) {
            setBlockIndexCandidates.insert(it->second);
        }
        it++;
    }

    InvalidChainFound(pindex);
    uiInterface.NotifyBlockTip(IsInitialBlockDownload(), pindex->pprev);
    return true;
}

bool ResetBlockFailureFlags(CBlockIndex *pindex) {
    AssertLockHeld(cs_main);

    int nHeight = pindex->nHeight;

    // Remove the invalidity flag from this block and all its descendants.
    BlockMap::iterator it = mapBlockIndex.begin();
    while (it != mapBlockIndex.end()) {
        if (!it->second->IsValid() && it->second->GetAncestor(nHeight) == pindex) {
            it->second->nStatus &= ~BLOCK_FAILED_MASK;
            setDirtyBlockIndex.insert(it->second);
            if (it->second->IsValid(BLOCK_VALID_TRANSACTIONS) && it->second->nChainTx && setBlockIndexCandidates.value_comp()(chainActive.Tip(), it->second)) {
                setBlockIndexCandidates.insert(it->second);
            }
            if (it->second == pindexBestInvalid) {
                // Reset invalid block marker if it was pointing to one of those.
                pindexBestInvalid = NULL;
            }
        }
        it++;
    }

    // Remove the invalidity flag from all ancestors too.
    while (pindex != NULL) {
        if (pindex->nStatus & BLOCK_FAILED_MASK) {
            pindex->nStatus &= ~BLOCK_FAILED_MASK;
            setDirtyBlockIndex.insert(pindex);
        }
        pindex = pindex->pprev;
    }
    return true;
}

static arith_uint256 CalculateChainWork(const CBlockIndex* pIndex, const CChainParams& chainparams)
{
    LOCK(cs_main);

    arith_uint256 nBlockProof = GetBlockProof(*pIndex);
    arith_uint256 chainWork = (pIndex->pprev ? pIndex->pprev->nChainWork : 0) + nBlockProof;
    if (pIndex->nVersionPoW2Witness != 0)
    {
        // Note: (PoW2) If we wanted to include witness weight in the chain weight this would be the place to do it.
        // This would have the benefit of making it harder to mine a side chain using lots of small witnesses.
        // However it would also bias the earnings and chain control even more to large witnesses and act as a 'centralisation' incentive.
        // So at this point we don't do this - and prefer instead to be agnostic, so we increase witnessed blocks always by a fixed weight.

        if (pIndex->pprev->nVersionPoW2Witness != 0)
        {
            // Witnessed blocks sit ahead of non-witnessed blocks in the chain so must have more work than our non-witnessed counterpart.
            // However this is more complicated than it would seem at first.
            // If we increase the work by some arbitrary amount e.g. 1 or 10000 then we sit with the following problem:
            // 1) Miner mines a block at high difficulty. 2) Witness is 'absent'. 3) Chain temporarily stalls.
            // 4) Miners start mining competing blocks at lower difficulty (delta difficulty halving). 5) Witnesses witness the block
            // 6) However because they are substantially lower difficulty - nBlockProof + 1 is not enough to put the new block at tip of chain
            // 7) It takes multiple lower difficulty blocks to form a new chain tip and for chain to progress (chain stalls unacceptably long)
            // If we base it on the current block weight (e.g. 2x current weight) then we face the opposite - i.e. an attacker can hold back a high difficulty witnessed block and then orphan a bunch of lower difficulty blocks using it.
            // If we base it on the weight of a previous block (some arbitrary distance backwards) then we fix the above some of the time, however with careful timing the possibility to manipulate is still there.
            // So instead we base it on the average of the last 10 blocks inclusive of the current, this provides sufficient protection from fluctuations.
            int nCount = 1;
            CBlockIndex* pprev = pIndex->pprev;
            while (pprev && nCount < 10)
            {
                ++nCount;
                nBlockProof += GetBlockProof(*pprev);
                pprev = pprev->pprev;
            }
            assert (nCount == 10);
            nBlockProof /= nCount;
            chainWork += nBlockProof;
        }
        else
        {
            // However for phase 3 we need subsequent PoW blocks to outweigh PoS blocks so we are stuck with the temporary limitation of using nChainWork + 1
            chainWork += 1;
        }
    }
    return chainWork;
}

static CBlockIndex* AddToBlockIndex(const CChainParams& chainParams, const CBlockHeader& block)
{
    // Check for duplicate
    uint256 hash = block.GetHashPoW2();
    BlockMap::iterator it = mapBlockIndex.find(hash);
    if (it != mapBlockIndex.end())
        return it->second;

    // Construct new block index object
    CBlockIndex* pindexNew = new CBlockIndex(block);
    assert(pindexNew);
    // We assign the sequence id to blocks only when the full data is available,
    // to avoid miners withholding blocks but broadcasting headers, to get a
    // competitive advantage.
    pindexNew->nSequenceId = 0;
    BlockMap::iterator mi = mapBlockIndex.insert(std::pair(hash, pindexNew)).first;
    pindexNew->phashBlock = &((*mi).first);
    BlockMap::iterator miPrev = mapBlockIndex.find(block.hashPrevBlock);
    if (miPrev != mapBlockIndex.end())
    {
        pindexNew->pprev = (*miPrev).second;
        pindexNew->nHeight = pindexNew->pprev->nHeight + 1;
        // Skip list can only be build for full chain
        if (pindexNew->pprev->pskip || pindexNew->pprev->nHeight == 0)
            pindexNew->BuildSkip();
    }
    pindexNew->nTimeMax = (pindexNew->pprev ? std::max(pindexNew->pprev->nTimeMax, pindexNew->nTime) : pindexNew->nTime);

    // we're only called for blocks that have the header verified (or Genenis)
    pindexNew->RaiseValidity(BLOCK_VALID_HEADER);

    pindexNew->nChainWork = CalculateChainWork(pindexNew, chainParams);
    if ((pindexNew->nHeight > 0 && pindexNew->pprev->IsValid(BLOCK_VALID_TREE)) || pindexNew->nHeight == 0)
    {
        // block is extending the main tree
        if (pindexNew->nChainTx && (pindexNew->nChainWork >= (chainActive.Tip() == NULL ? 0 : chainActive.Tip()->nChainWork) || pindexNew->nHeight >= (chainActive.Tip() == NULL ? 0 : chainActive.Tip()->nHeight)))
        {
            if (!gbMinimalLogging)
                LogPrintf("AddToBlockIndex: New index candidate: [%s] [%d]\n", pindexNew->GetBlockHashPoW2().ToString(), pindexNew->nHeight);
            setBlockIndexCandidates.insert(pindexNew);
        }
        pindexNew->RaiseValidity(BLOCK_VALID_TREE);
        if (pindexBestHeader == nullptr || pindexBestHeader->nChainWork < pindexNew->nChainWork)
            pindexBestHeader = pindexNew;
    }
    else
    {
        // block is not extending main tree, so it's extending the partial tree
        pindexNew->RaisePartialValidity(BLOCK_PARTIAL_TREE);
    }

    if (IsPartialSyncActive() && (pindexBestPartial == nullptr || pindexBestPartial->nChainWork < pindexNew->nChainWork))
    {
        if (pindexNew->nHeight > partialChain.HeightOffset())
        {
            pindexBestPartial = pindexNew;
            partialChain.SetTip(pindexBestPartial);
        }
    }

    setDirtyBlockIndex.insert(pindexNew);

    return pindexNew;
}

/** Promote a blockindex in the partial tree to the full tree, connecting it if needed */
static bool PromoteBlockIndex(CBlockIndex* pindexNew, const CBlockHeader& header, CBlockIndex* pindexPrev)
{
    if (!pindexPrev->IsValid(BLOCK_VALID_TREE))
        return false;

    // Partial block might not have been fully initalized if it was constructed from a checkpoint, do so here
    pindexNew->nVersionPoW2Witness = header.nVersionPoW2Witness;
    pindexNew->nTimePoW2Witness = header.nTimePoW2Witness;
    pindexNew->hashMerkleRootPoW2Witness = header.hashMerkleRootPoW2Witness;
    pindexNew->witnessHeaderPoW2Sig = header.witnessHeaderPoW2Sig;
    pindexNew->nVersion       = header.nVersion;
    pindexNew->hashMerkleRoot = header.hashMerkleRoot;
    pindexNew->nTime          = header.nTime;
    pindexNew->nBits          = header.nBits;
    pindexNew->nNonce         = header.nNonce;

    pindexNew->pprev = pindexPrev;
    pindexNew->nHeight = pindexNew->pprev->nHeight + 1;
    pindexNew->BuildSkip();
    pindexNew->nTimeMax = (pindexNew->pprev ? std::max(pindexNew->pprev->nTimeMax, pindexNew->nTime) : pindexNew->nTime);

    arith_uint256 chainWork = CalculateChainWork(pindexNew, Params());
    if (pindexNew->nChainTx && (pindexNew->nChainWork >= (chainActive.Tip() == NULL ? 0 : chainActive.Tip()->nChainWork) || pindexNew->nHeight >= (chainActive.Tip() == NULL ? 0 : chainActive.Tip()->nHeight)))
    {
        if (!gbMinimalLogging)
            LogPrintf("PromoteBlockIndex: new/updated index candidate: [%s] [%d]\n", pindexNew->GetBlockHashPoW2().ToString(), pindexNew->nHeight);
        setBlockIndexCandidates.erase(pindexNew);
        pindexNew->nChainWork = chainWork;
        setBlockIndexCandidates.insert(pindexNew);
    }
    else
    {
        pindexNew->nChainWork = chainWork;
    }

    pindexNew->RaiseValidity(BLOCK_VALID_TREE);
    if (pindexBestHeader == nullptr || pindexBestHeader->nChainWork < pindexNew->nChainWork)
        pindexBestHeader = pindexNew;

    setDirtyBlockIndex.insert(pindexNew);

    return true;
}

/** Mark a block as having its data received and checked (up to BLOCK_VALID_TRANSACTIONS or BLOCK_PARTIAL_TRANSACTIONS). */
static bool ReceivedBlockTransactions(const CBlock &block, CValidationState& state, CBlockIndex *pindexNew, const CDiskBlockPos& pos, const Consensus::Params& consensusParams)
{
    pindexNew->nTx = block.vtx.size();
    pindexNew->nChainTx = 0;
    pindexNew->nFile = pos.nFile;
    pindexNew->nDataPos = pos.nPos;
    pindexNew->nUndoPos = 0;
    pindexNew->nStatus |= BLOCK_HAVE_DATA;
    if (IsSegSigEnabled(pindexNew->pprev)) {
        pindexNew->nStatus |= BLOCK_OPT_WITNESS;
    }

    if (pindexNew->nHeight == 0 || (pindexNew->pprev && pindexNew->pprev->IsValid(BLOCK_VALID_TREE)))
    {
        pindexNew->RaiseValidity(BLOCK_VALID_TRANSACTIONS);
        setDirtyBlockIndex.insert(pindexNew);
    }
    else if (pindexNew->pprev && pindexNew->pprev->IsPartialValid(BLOCK_PARTIAL_TREE)) {
        pindexNew->RaisePartialValidity(BLOCK_PARTIAL_TRANSACTIONS);
        setDirtyBlockIndex.insert(pindexNew);
    }
    else
        return false;

    if (pindexNew->pprev == NULL || pindexNew->pprev->nChainTx) {
        // If pindexNew is the genesis block or all parents are BLOCK_VALID_TRANSACTIONS.
        std::deque<CBlockIndex*> queue;
        queue.push_back(pindexNew);

        LOCK(cs_main);
        // Recursively process any descendant blocks that now may be eligible to be connected.
        while (!queue.empty()) {
            CBlockIndex *pindex = queue.front();
            queue.pop_front();
            pindex->nChainTx = (pindex->pprev ? pindex->pprev->nChainTx : 0) + pindex->nTx;
            {
                LOCK(cs_nBlockSequenceId);
                setBlockIndexCandidates.erase(pindex);
                pindex->nSequenceId = nBlockSequenceId++;
            }
            if (pindex->nChainWork >= (chainActive.Tip() == NULL ? 0 : chainActive.Tip()->nChainWork) || pindex->nHeight >= (chainActive.Tip() == NULL ? 0 : chainActive.Tip()->nHeight))
            {
                if (!gbMinimalLogging)
                    LogPrintf("ReceivedBlockTransactions: New index candidate: [%s] [%d]\n", pindex->GetBlockHashPoW2().ToString(), pindex->nHeight);
                setBlockIndexCandidates.insert(pindex);
            }
            std::pair<std::multimap<CBlockIndex*, CBlockIndex*>::iterator, std::multimap<CBlockIndex*, CBlockIndex*>::iterator> range = mapBlocksUnlinked.equal_range(pindex);
            while (range.first != range.second) {
                std::multimap<CBlockIndex*, CBlockIndex*>::iterator it = range.first;
                queue.push_back(it->second);
                range.first++;
                mapBlocksUnlinked.erase(it);
            }
        }
    } else {
        if (pindexNew->pprev && pindexNew->pprev->IsValid(BLOCK_VALID_TREE)) {
            mapBlocksUnlinked.insert(std::pair(pindexNew->pprev, pindexNew));
        }
    }

    return true;
}

static bool FindBlockPos(CValidationState &state, CDiskBlockPos &pos, unsigned int nAddSize, unsigned int nHeight, uint64_t nTime, bool fKnown = false)
{
    LOCK(cs_LastBlockFile);

    unsigned int nFile = fKnown ? pos.nFile : nLastBlockFile;
    if (vinfoBlockFile.size() <= nFile) {
        vinfoBlockFile.resize(nFile + 1);
    }

    if (!fKnown) {
        while (vinfoBlockFile[nFile].nSize + nAddSize >= MAX_BLOCKFILE_SIZE) {
            nFile++;
            if (vinfoBlockFile.size() <= nFile) {
                vinfoBlockFile.resize(nFile + 1);
            }
        }
        pos.nFile = nFile;
        pos.nPos = vinfoBlockFile[nFile].nSize;
    }

    if ((int)nFile != nLastBlockFile) {
        if (!fKnown) {
            LogPrintf("Leaving block file %i: %s\n", nLastBlockFile, vinfoBlockFile[nLastBlockFile].ToString());
        }
        FlushBlockFile(!fKnown);
        nLastBlockFile = nFile;
    }

    vinfoBlockFile[nFile].AddBlock(nHeight, nTime);
    if (fKnown)
        vinfoBlockFile[nFile].nSize = std::max(pos.nPos + nAddSize, vinfoBlockFile[nFile].nSize);
    else
        vinfoBlockFile[nFile].nSize += nAddSize;

    if (!fKnown) {
        unsigned int nOldChunks = (pos.nPos + BLOCKFILE_CHUNK_SIZE - 1) / BLOCKFILE_CHUNK_SIZE;
        unsigned int nNewChunks = (vinfoBlockFile[nFile].nSize + BLOCKFILE_CHUNK_SIZE - 1) / BLOCKFILE_CHUNK_SIZE;
        if (nNewChunks > nOldChunks) {
            if (fPruneMode)
                fCheckForPruning = true;
            if (CheckDiskSpace(nNewChunks * BLOCKFILE_CHUNK_SIZE - pos.nPos)) {
                FILE *file = blockStore.GetBlockFile(pos);
                if (file) {
                    LogPrintf("Pre-allocating up to position 0x%x in blk%05u.dat\n", nNewChunks * BLOCKFILE_CHUNK_SIZE, pos.nFile);
                    AllocateFileRange(file, pos.nPos, nNewChunks * BLOCKFILE_CHUNK_SIZE - pos.nPos);
                }
            }
            else
                return state.Error("out of disk space");
        }
    }

    setDirtyFileInfo.insert(nFile);
    return true;
}

static bool FindUndoPos(CValidationState &state, int nFile, CDiskBlockPos &pos, unsigned int nAddSize)
{
    pos.nFile = nFile;

    LOCK(cs_LastBlockFile);

    unsigned int nNewSize;
    pos.nPos = vinfoBlockFile[nFile].nUndoSize;
    nNewSize = vinfoBlockFile[nFile].nUndoSize += nAddSize;
    setDirtyFileInfo.insert(nFile);

    unsigned int nOldChunks = (pos.nPos + UNDOFILE_CHUNK_SIZE - 1) / UNDOFILE_CHUNK_SIZE;
    unsigned int nNewChunks = (nNewSize + UNDOFILE_CHUNK_SIZE - 1) / UNDOFILE_CHUNK_SIZE;
    if (nNewChunks > nOldChunks) {
        if (fPruneMode)
            fCheckForPruning = true;
        if (CheckDiskSpace(nNewChunks * UNDOFILE_CHUNK_SIZE - pos.nPos)) {
            FILE *file = blockStore.GetUndoFile(pos);
            if (file) {
                LogPrintf("Pre-allocating up to position 0x%x in rev%05u.dat\n", nNewChunks * UNDOFILE_CHUNK_SIZE, pos.nFile);
                AllocateFileRange(file, pos.nPos, nNewChunks * UNDOFILE_CHUNK_SIZE - pos.nPos);
            }
        }
        else
            return state.Error("out of disk space");
    }

    return true;
}

static bool CheckBlockHeader(const CBlock& block, CValidationState& state, const Consensus::Params& consensusParams, bool fCheckPOW = true)
{
    // Check proof of work matches claimed amount
    if (fCheckPOW) {
        // Nested if statement for easier breakpoint management
        if (!CheckProofOfWork(block.GetPoWHash(), block.nBits, consensusParams))
            return state.DoS(50, false, REJECT_INVALID, "high-hash", false, "proof of work failed");
    }

    return true;
}

bool CheckBlock(const CBlock& block, CValidationState& state, const Consensus::Params& consensusParams, bool fCheckPOW, bool fCheckMerkleRoot, bool fAssumePOWGood)
{
    // These are checks that are independent of context.

    if (block.fChecked)
        return true;

    // Check that the header is valid (particularly PoW).  This is mostly
    // redundant with the call in AcceptBlockHeader.
    if (fCheckPOW)
    {
        if (!CheckBlockHeader(block, state, consensusParams, !fAssumePOWGood && !block.fPOWChecked))
            return false;
        else
            block.fPOWChecked = true;
    }

    // All potential-corruption validation must be done before we do any
    // transaction validation, as otherwise we may mark the header as invalid
    // because we receive the wrong transactions for it.
    // Note that witness malleability is checked in ContextualCheckBlock, so no
    // checks that use witness data may be performed here.

    // Size limits
    if (block.vtx.empty() || block.vtx.size() > MAX_BLOCK_BASE_SIZE || ::GetSerializeSize(block, SER_NETWORK, PROTOCOL_VERSION | SERIALIZE_TRANSACTION_NO_SEGREGATED_SIGNATURES) > MAX_BLOCK_BASE_SIZE)
        return state.DoS(100, false, REJECT_INVALID, "bad-blk-length", false, "size limits failed");

    // First transaction must be coinbase, the rest must not be
    if (block.vtx.empty() || !block.vtx[0]->IsCoinBase())
        return state.DoS(100, false, REJECT_INVALID, "bad-cb-missing", false, "first tx is not coinbase");

    // Witness block has two coinbases
    unsigned int nWitnessCoinbaseIndex = 0;
    if (block.nVersionPoW2Witness != 0)
    {
        for (unsigned int i = 1; i < block.vtx.size(); i++)
            if (block.vtx[i]->IsCoinBase() && block.vtx[i]->IsPoW2WitnessCoinBase())
                nWitnessCoinbaseIndex = i;
    }

    // Extra coinbase (invalid)
    for (unsigned int i = (nWitnessCoinbaseIndex == 0 ? 1 : nWitnessCoinbaseIndex+1); i < block.vtx.size(); i++)
        if (block.vtx[i]->IsCoinBase())
            return state.DoS(100, false, REJECT_INVALID, "bad-cb-multiple", false, "block contains excess coinbase transactions");

    // Check the merkle root.
    if (fCheckMerkleRoot) {
        bool mutated;
        uint256 hashMerkleRoot2 = BlockMerkleRoot(block.vtx.begin(), (nWitnessCoinbaseIndex == 0 ? block.vtx.end() : block.vtx.begin()+nWitnessCoinbaseIndex), &mutated);
        if (block.hashMerkleRoot != hashMerkleRoot2)
            return state.DoS(100, false, REJECT_INVALID, "bad-txnmrklroot", true, "hashMerkleRoot mismatch");

        // Check for merkle tree malleability (CVE-2012-2459): repeating sequences
        // of transactions in a block without affecting the merkle root of a block,
        // while still invalidating it.
        if (mutated)
            return state.DoS(100, false, REJECT_INVALID, "bad-txns-duplicate", true, "duplicate transaction");

        if (block.nVersionPoW2Witness != 0)
        {
            uint256 hashMerkleRoot3 = BlockMerkleRoot(block.vtx.begin()+nWitnessCoinbaseIndex, block.vtx.end(), &mutated);
            if (block.hashMerkleRootPoW2Witness != hashMerkleRoot3)
                return state.DoS(100, false, REJECT_INVALID, "bad-txnmrklroot", true, "pow2 witness hashMerkleRoot mismatch");

            // Check for merkle tree malleability (CVE-2012-2459): repeating sequences
            // of transactions in a block without affecting the merkle root of a block,
            // while still invalidating it.
            if (mutated)
                return state.DoS(100, false, REJECT_INVALID, "bad-txns-duplicate", true, "witness duplicate transaction");
        }
    }

    // Check transactions
    for (const auto& tx : block.vtx)
        if (!CheckTransaction(*tx, state, false))
            return state.Invalid(false, state.GetRejectCode(), state.GetRejectReason(),
                                 strprintf("Transaction check failed (tx hash %s) %s", tx->GetHash().ToString(), state.GetDebugMessage()));

    unsigned int nSigOps = 0;
    for (const auto& tx : block.vtx)
    {
        nSigOps += GetLegacySigOpCount(*tx);
    }
    if (nSigOps > MAX_BLOCK_SIGOPS_COST)
        return state.DoS(100, false, REJECT_INVALID, "bad-blk-sigops", false, "out-of-bounds SigOpCount");

    if (fCheckPOW && fCheckMerkleRoot)
        block.fChecked = true;

    return true;
}

static bool CheckIndexAgainstCheckpoint(const CBlockIndex* pindexPrev, CValidationState& state, const CChainParams& chainparams, const uint256& hash)
{
    if (*pindexPrev->phashBlock == chainparams.GetConsensus().hashGenesisBlock)
        return true;

    int nHeight = pindexPrev->nHeight+1;
    // Gulden: check that the block satisfies synchronized checkpoint
    if (!Checkpoints::CheckSync(hash, pindexPrev))
        return error("AcceptBlock() : rejected by synchronized checkpoint");


    // Don't accept any forks from the main chain prior to last checkpoint.
    // GetLastCheckpoint finds the last checkpoint in MapCheckpoints that's in our
    // MapBlockIndex.
    CBlockIndex* pcheckpoint = Checkpoints::GetLastCheckpointIndex();
    if (pcheckpoint && nHeight < pcheckpoint->nHeight)
        return state.DoS(100, error("%s: forked chain older than last checkpoint (height %d)", __func__, nHeight), REJECT_CHECKPOINT, "bad-fork-prior-to-checkpoint");

    return true;
}

// We go for a cheap check here, instead of checking for phase 4 (which can be expensive and lead to problems) 
// We instead just check if the previous block is a witness block (if it is we are in phase 4 as this isn't allowed in other phases.
bool IsSegSigEnabled(const CBlockIndex* pindexPrev)
{
    LOCK(cs_main);
    if (!pindexPrev)
        return false;
    if (pindexPrev->nVersionPoW2Witness != 0)
        return true;
    return false;
}


bool ContextualCheckBlockHeader(const CBlockHeader& block, CValidationState& state, const Consensus::Params& consensusParams, const CBlockIndex* pindexPrev, int64_t nAdjustedTime)
{
    assert(pindexPrev != NULL);
    //const int nHeight = pindexPrev->nHeight + 1;
    // Check proof of work
    if (block.nBits != GetNextWorkRequired(pindexPrev, &block, consensusParams))
        return state.DoS(100, false, REJECT_INVALID, "bad-diffbits", false, "incorrect proof of work");

    // Check timestamp against prev
    if (block.GetBlockTime() <= pindexPrev->GetMedianTimePastWitness())
        return state.Invalid(false, REJECT_INVALID, "time-too-old", "block's timestamp is too early");

    // Check timestamp
    if (pindexPrev->nHeight > (IsArgSet("-testnet") ? 446500 : 437500) )
    {
        if (block.GetBlockTime() > nAdjustedTime + MAX_FUTURE_BLOCK_TIME)
            return state.Invalid(false, REJECT_INVALID, "time-too-new", "block timestamp too far in the future");
    }
    else
    {
        if (block.GetBlockTime() > nAdjustedTime + 15 * 60)
            return state.Invalid(false, REJECT_INVALID, "time-too-new", "block timestamp too far in the future");
    }

    //fixme: (2.1) (SEGSIG) Enforce segsig upgrade rules here; this is I think redundany as it is already handled elsewhere, but we should catch it earlier.

    // Reject outdated version blocks when 95% (75% on testnet) of the network has upgraded:
    // check for version 2, 3 and 4 upgrades
    /* GULDEN - These aren't valid for Gulden
    if((block.nVersion < 2 && nHeight >= consensusParams.BIP34Height) ||
       (block.nVersion < 3 && nHeight >= consensusParams.BIP66Height) ||
       (block.nVersion < 4 && nHeight >= consensusParams.BIP65Height))
            return state.Invalid(false, REJECT_OBSOLETE, strprintf("bad-version(0x%08x)", block.nVersion),
                                 strprintf("rejected nVersion=0x%08x block", block.nVersion));
    */

    return true;
}

//fixme: (2.1) Do away with this doUTXOChecks junk
//Long story short; we need to call this twice, the second time from within ConnectTip as we can't do the 'phase' checks without the utxo
//After 2.1 we won't need the phase checks so we can code this properly.
bool ContextualCheckBlock(const CBlock& block, CValidationState& state, const CChainParams& chainParams, const CBlockIndex* pindexPrev, CChain& chainOverride, CCoinsViewCache* viewOverride, bool doUTXOChecks)
{
    const int nHeight = pindexPrev == NULL ? 0 : pindexPrev->nHeight + 1;

    // Start enforcing BIP113 (Median Time Past) using versionbits logic.
    int nLockTimeFlags = 0;
    if (VersionBitsState(pindexPrev, chainParams.GetConsensus(), Consensus::DEPLOYMENT_CSV, versionbitscache) == THRESHOLD_ACTIVE) {
        nLockTimeFlags |= LOCKTIME_MEDIAN_TIME_PAST;
    }

    int64_t nLockTimeCutoff = (nLockTimeFlags & LOCKTIME_MEDIAN_TIME_PAST)
                              ? pindexPrev->GetMedianTimePast()
                              : block.GetBlockTime();

    // Check that all transactions are finalized
    for (const auto& tx : block.vtx) {
        if (!IsFinalTx(*tx, nHeight, nLockTimeCutoff)) {
            return state.DoS(10, false, REJECT_INVALID, "bad-txns-nonfinal", false, "non-final transaction");
        }
    }

    // Check that no transactions (from phase2 onward) have transaction version above 4 - this behaviour is no longer allowed
    if (doUTXOChecks && GetPoW2Phase(pindexPrev, chainParams, chainOverride, viewOverride) >= 3)
    {
        for (const auto& tx : block.vtx)
        {
            if (tx->nVersion > CTransaction::MAX_STANDARD_VERSION)
            {
                return state.DoS(100, false, REJECT_INVALID, "bad-transaction-version", false, "non-standard transaction versions above are no longer permitted.");
            }
        }
    }

    bool fHaveSegregatedSignatures = IsSegSigEnabled(pindexPrev);
    // Check that no transactions (from phase4 onward) contain a scriptSig - scriptSig is completely deprecated.
    if (fHaveSegregatedSignatures)
    {
        for (const auto& tx : block.vtx)
        {
            if (!IsOldTransactionVersion(tx->nVersion))
            {
                return state.DoS(100, false, REJECT_INVALID, "bad-transaction-version", false, "mining segsig version transactions before activation is forbidden");
            }
            for (const auto& txIn : tx->vin)
            {
                if (txIn.scriptSig.size() > 0)
                {
                    return state.DoS(100, false, REJECT_INVALID, "bad-segsig-txin", false, "segsig blocks may not contain scriptSig which has been deprecated");
                }
            }
        }
    }

    // Enforce rule that the coinbase starts with serialized block height
    if (nHeight >= chainParams.GetConsensus().BIP34Height)
    {
        if (fHaveSegregatedSignatures)
        {
            std::vector<unsigned char> expect;
            CVectorWriter(0, 0, expect, 0) << VARINT(nHeight);
            if (block.vtx[0]->vin[0].segregatedSignatureData.stack.empty() || !std::equal(expect.begin(), expect.end(), block.vtx[0]->vin[0].segregatedSignatureData.stack[0].begin()))
            {
                return state.DoS(100, false, REJECT_INVALID, "bad-cb-height", false, "block height mismatch in coinbase2");
            }
        }
        else
        {
            CScript expect = CScript() << nHeight;
            if (block.vtx[0]->vin[0].scriptSig.size() < expect.size() ||
                !std::equal(expect.begin(), expect.end(), block.vtx[0]->vin[0].scriptSig.begin())) {
                return state.DoS(100, false, REJECT_INVALID, "bad-cb-height", false, "block height mismatch in coinbase1");
            }
        }
    }


    //Enforce embedded witness coinbase data in phase 3.
    //Only the second block with a phase 3 parent onwards has a witness coinbase with embedded data, as only the first block with a phase 3 parent has a witness.
    //Also having the check here prevents miners from broadcasting invalid blocks sooner.
    //fixme: (2.1) This is now a duplicate of the check in ConnectBlock - reconsider if we need both.
    //This was added as invalid blocks were still being accepted (just not connected) and this was combining with other factors to cause network issues.
    if (nHeight > 10 && IsPow2Phase3Active(nHeight-2) && !fHaveSegregatedSignatures && block.nVersionPoW2Witness == 0)
    {
        int nEmbeddedWitnessCoinbaseIndex = 0;
        nEmbeddedWitnessCoinbaseIndex = GetPoW2WitnessCoinbaseIndex(block);
        if (nEmbeddedWitnessCoinbaseIndex == -1)
            return state.DoS(20, error("ContextualCheckBlock(): PoW2 phase 3 coinbase lacks witness data)"), REJECT_INVALID, "bad-cb-nowitnessdata");

        unsigned int nWitnessCoinbasePayoutIndex = nEmbeddedWitnessCoinbaseIndex + 1;

        if (block.vtx[0]->vout.size()-1 < nWitnessCoinbasePayoutIndex)
            return state.DoS(20, error("ConnectBlock(): PoW2 phase 3 coinbase lacks witness payout)"), REJECT_INVALID, "bad-cb-nowitnesspayout");

        CAmount nSubsidyWitness = GetBlockSubsidyWitness(nHeight);
        if (block.vtx[0]->vout[nWitnessCoinbasePayoutIndex].nValue != nSubsidyWitness)
            return state.DoS(20, error("ConnectBlock(): PoW2 phase 3 coinbase has incorrect witness payout amount [%d] [%d])", block.vtx[0]->vout[nWitnessCoinbasePayoutIndex].nValue, nSubsidyWitness), REJECT_INVALID, "bad-cb-badwitnesspayoutamount");
    }

    // And the same for witness coinbase. (Enforce rule that the coinbase starts with serialized block height)
    unsigned int nWitnessCoinbaseIndex = 0;
    if (block.nVersionPoW2Witness != 0)
    {
        for (unsigned int i = 1; i < block.vtx.size(); i++)
        {
            if (block.vtx[i]->IsCoinBase() && block.vtx[i]->IsPoW2WitnessCoinBase())
            {
                nWitnessCoinbaseIndex = i;
                break;
            }
        }
        //checkme: (GULDEN) (2.1) Pretty sure this is a duplicate check - so we should eventually remove it, for now we just leave it in.
        if (nWitnessCoinbaseIndex == 0)
        {
            return state.DoS(100, error("ContextualCheckBlock(): PoW2 witness coinbase missing)"), REJECT_INVALID, "bad-witness-cb");
        }
    }

    if (block.nVersionPoW2Witness != 0)
    {
        // Phase 3 - we restrict the coinbase signature to only the block height.
        // This helps simplify the logic for the PoW mining (which has to stuff all this info into it's own coinbase signature).
        if (fHaveSegregatedSignatures)
        {
            std::vector<unsigned char> expect;
            CVectorWriter(0, 0, expect, 0) << VARINT(nHeight);
            if (block.vtx[nWitnessCoinbaseIndex]->vin[0].segregatedSignatureData.stack.empty() || !std::equal(expect.begin(), expect.end(), block.vtx[nWitnessCoinbaseIndex]->vin[0].segregatedSignatureData.stack[0].begin()))
            {
                return state.DoS(100, false, REJECT_INVALID, "bad-cb-height", false, "block height mismatch in coinbase3");
            }
        }
        else
        {
            CScript expect = CScript() << nHeight;
            if (block.vtx[nWitnessCoinbaseIndex]->vin[0].scriptSig.size() != expect.size())
                return state.DoS(100, error("ContextualCheckBlock(): PoW2 phase 3 witness coinbase has incorrect size for scriptSig)"), REJECT_INVALID, "bad-phase3-witness-cb-scriptsig");

            // Enforce rule that the coinbase starts with serialized block height
            if (block.vtx[nWitnessCoinbaseIndex]->vin[0].scriptSig.size() < expect.size() ||
                !std::equal(expect.begin(), expect.end(), block.vtx[nWitnessCoinbaseIndex]->vin[0].scriptSig.begin()))
            {
                return state.DoS(100, false, REJECT_INVALID, "bad-witness-cb-height", false, "block height mismatch in witness coinbase");
            }
        }
    }

    //NB!! GULDEN - segwit commits/adds a coinbase commitment here.
    //For segsig this is unnecessary; we hash this data as part of the normal merkle root instead.


    //fixme: (2.1) Below checks can be removed/simplified
    // No witness data is allowed in blocks that don't commit to witness data, as this would otherwise leave room for spam
    if (fHaveSegregatedSignatures)
    {
        for (const auto& tx : block.vtx)
        {
            if (!tx->HasSegregatedSignatures())
                return state.DoS(100, false, REJECT_INVALID, "missing-segregated-signature", true, strprintf("%s : missing segregated signature data found", __func__));
        }
    }
    else
    {
        for (const auto& tx : block.vtx)
        {
            if (tx->HasSegregatedSignatures())
                return state.DoS(100, false, REJECT_INVALID, "invalid-segregated-signature", true, strprintf("%s : segregated signature not allowed before phase 4 activation", __func__));
        }
    }

    // After the coinbase witness nonce and commitment are verified,
    // we can check if the block weight passes (before we've checked the
    // coinbase witness, it would be possible for the weight to be too
    // large by filling up the coinbase witness, which doesn't change
    // the block hash, so we couldn't mark the block as permanently
    // failed).
    if (GetBlockWeight(block) > MAX_BLOCK_WEIGHT) {
        return state.DoS(100, false, REJECT_INVALID, "bad-blk-weight", false, strprintf("%s : weight limit failed", __func__));
    }

    return true;
}

static bool AcceptBlockHeader(const CBlockHeader& block, CValidationState& state, const CChainParams& chainparams, CBlockIndex** ppindex, bool fAssumePOWGood = false)
{
    AssertLockHeld(cs_main);

    //fixme: (2.0.1) Double check handling of different header types.

    CBlockIndex* pindexPrev = nullptr;
    bool promoteToFullTree = false;

    // Check for duplicate
    uint256 hash = block.GetHashPoW2();
    BlockMap::iterator miSelf = mapBlockIndex.find(hash);
    CBlockIndex *pindex = NULL;
    if (hash != chainparams.GetConsensus().hashGenesisBlock) {

        if (miSelf != mapBlockIndex.end()) {
            // Block header is already known.
            pindex = miSelf->second;
            if (ppindex)
                *ppindex = pindex;
            if (pindex->nStatus & BLOCK_FAILED_MASK)
                return state.Invalid(error("%s: block %s is marked invalid", __func__, hash.ToString()), 0, "duplicate");
            if (!pindex->IsValid(BLOCK_VALID_TREE) && pindex->IsPartialValid(BLOCK_PARTIAL_TREE))
            {
                // check if the block can be promoted to the full tree
                if (pindex->pprev)
                    pindexPrev = pindex->pprev;
                else
                {
                    const auto prevIt = mapBlockIndex.find(block.hashPrevBlock);
                    if (prevIt != mapBlockIndex.end())
                        pindexPrev = prevIt->second;
                }
                promoteToFullTree = pindexPrev && pindexPrev->IsValid(BLOCK_VALID_TREE);
            }
            else
                return true;
        }

        if (!CheckBlockHeader(block, state, chainparams.GetConsensus(), !fAssumePOWGood))
            return error("%s: Consensus::CheckBlockHeader: %s, %s", __func__, hash.ToString(), FormatStateMessage(state));

        // Get prev block index if we don't have it yet
        if (!pindexPrev)
        {
            BlockMap::iterator mi = mapBlockIndex.find(block.hashPrevBlock);
            if (mi == mapBlockIndex.end())
                return state.DoS(10, error("%s: prev block not found", __func__), 0, "prev-blk-not-found");
            pindexPrev = (*mi).second;
        }

        if (pindexPrev->nStatus & BLOCK_FAILED_MASK)
            return state.DoS(100, error("%s: prev block invalid", __func__), REJECT_INVALID, "bad-prevblk");

        assert(pindexPrev);
        if (fCheckpointsEnabled && !CheckIndexAgainstCheckpoint(pindexPrev, state, chainparams, hash))
            return error("%s: CheckIndexAgainstCheckpoint(): %s", __func__, state.GetRejectReason().c_str());

        // do context check if block header connects to the full tree or when we have at least the required amount of partial tree available
        bool doContextCheck = pindexPrev->IsValid(BLOCK_VALID_TREE) || ((pindexPrev->IsPartialValid(BLOCK_PARTIAL_TREE)) && pindexPrev->nHeight - partialChain.HeightOffset() > 576);
        if (doContextCheck && !ContextualCheckBlockHeader(block, state, chainparams.GetConsensus(), pindexPrev, GetAdjustedTime()))
            return error("%s: Consensus::ContextualCheckBlockHeader: %s, %s", __func__, hash.ToString(), FormatStateMessage(state));
    }

    if (promoteToFullTree)
        PromoteBlockIndex(pindex, block, pindexPrev);
    else if (pindex == nullptr)
        pindex = AddToBlockIndex(chainparams, block);

    if (ppindex)
        *ppindex = pindex;

    CheckBlockIndex(chainparams.GetConsensus());

    return true;
}

static void CheckAndNotifyHeaderTip()
{
    if (!IsPartialSyncActive())
        return;

    static const CBlockIndex* pPreviousHeaderTip = nullptr;
    bool fNotify = false;
    {
        LOCK(cs_main);
        if (partialChain.Tip() != pPreviousHeaderTip) {
            fNotify = true;
            pPreviousHeaderTip = partialChain.Tip();
        }
    }
    if (fNotify)
        headerTipSignal(pPreviousHeaderTip);
}

// Exposed wrapper for AcceptBlockHeader
bool ProcessNewBlockHeaders(const std::vector<CBlockHeader>& headers, CValidationState& state, const CChainParams& chainparams, const CBlockIndex** ppindex, bool fAssumePOWGood)
{
    {
        LOCK(cs_main);
        for (const CBlockHeader& header : headers) {
            CBlockIndex *pindex = NULL; // Use a temp pindex instead of ppindex to avoid a const_cast
            if (!AcceptBlockHeader(header, state, chainparams, &pindex, fAssumePOWGood)) {
                return false;
            }
            if (ppindex) {
                *ppindex = pindex;
            }
        }
    }

    CheckAndNotifyHeaderTip();

    if (IsPartialSyncActive() && !isFullSyncMode())
        PersistAndPruneForPartialSync(true);

    return true;
}

/** Store block on disk. If dbp is non-NULL, the file is known to already reside on disk */
static bool AcceptBlock(const std::shared_ptr<const CBlock>& pblock, CValidationState& state, const CChainParams& chainparams, CBlockIndex** ppindex, bool fRequested, const CDiskBlockPos* dbp, bool* fNewBlock, bool fAssumePOWGood, bool checkFarAhead)
{
    const CBlock& block = *pblock;

    if (fNewBlock) *fNewBlock = false;
    AssertLockHeld(cs_main);

    CBlockIndex *pindexDummy = NULL;
    CBlockIndex *&pindex = ppindex ? *ppindex : pindexDummy;

    if (!AcceptBlockHeader(block, state, chainparams, &pindex, fAssumePOWGood))
        return false;

    // Try to process all requested blocks that we don't have, but only
    // process an unrequested block if it's new and has enough work to
    // advance our tip, and isn't too many blocks ahead.
    bool fAlreadyHave = pindex->nStatus & BLOCK_HAVE_DATA;
    bool fHasMoreWork = (chainActive.Tip() ? pindex->nChainWork > chainActive.Tip()->nChainWork : true);
    // Blocks that are too out-of-order needlessly limit the effectiveness of
    // pruning, because pruning will not delete block files that contain any
    // blocks which are too close in height to the tip.  Apply this test
    // regardless of whether pruning is enabled; it should generally be safe to
    // not process unrequested blocks.
    bool fTooFarAhead = (pindex->nHeight > int(chainActive.Height() + MIN_BLOCKS_TO_KEEP));

    // TODO: Decouple this function from the block download logic by removing fRequested
    // This requires some new chain data structure to efficiently look up if a
    // block is in a chain leading to a candidate for best tip, despite not
    // being such a candidate itself.

    // TODO: deal better with return value and error conditions for duplicate
    // and unrequested blocks.
    if (fAlreadyHave) return true;
    if (!fRequested) {  // If we didn't ask for it:
        if (pindex->nTx != 0) return true;  // This is a previously-processed block that was pruned
        if (!fHasMoreWork) return true;     // Don't process less-work chains
        if (checkFarAhead && fTooFarAhead) return true;      // Block height is too high
    }
    if (fNewBlock) *fNewBlock = true;

    if (!CheckBlock(block, state, chainparams.GetConsensus(), true, true, fAssumePOWGood) ||
        !ContextualCheckBlock(block, state, chainparams, pindex->pprev, chainActive)) {
        if (state.IsInvalid() && !state.CorruptionPossible()) {
            pindex->nStatus |= BLOCK_FAILED_VALID;
            setDirtyBlockIndex.insert(pindex);
        }
        return error("%s: %s", __func__, FormatStateMessage(state));
    }

    // TODO, do not relay if we are SPV and have not fully validated the block? Something with the contextual block check
    // fails in our partial sync.

    // Header is valid/has work, merkle tree are good...RELAY NOW
    // (but if it does not build on our best tip, let the SendMessages loop relay it)
    // fixme: (2.0.1) (HIGH) This will probably increase forks slightly - but we need to keep pushing tip contenders out in case of stalled witness
    // Maybe we could 'delay' such candidates slightly, store them in a cache and then only relay after some time has passed with tip not advancing.
    if (!IsInitialBlockDownload() && (chainActive.Tip() == pindex->pprev || pindex->nHeight >= chainActive.Tip()->nHeight))
        GetMainSignals().NewPoWValidBlock(pindex, pblock);

    int nHeight = pindex->nHeight;

    // Write block to history file
    try {
        unsigned int nBlockSize = ::GetSerializeSize(block, SER_DISK, CLIENT_VERSION);
        CDiskBlockPos blockPos;
        if (dbp != NULL)
            blockPos = *dbp;
        if (!FindBlockPos(state, blockPos, nBlockSize+8, nHeight, block.GetBlockTime(), dbp != NULL))
            return error("AcceptBlock(): FindBlockPos failed");
        if (dbp == NULL)
            if (!blockStore.WriteBlockToDisk(block, blockPos, chainparams.MessageStart()))
                AbortNode(state, "Failed to write block");
        if (!ReceivedBlockTransactions(block, state, pindex, blockPos, chainparams.GetConsensus()))
            return error("AcceptBlock(): ReceivedBlockTransactions failed");
    } catch (const std::runtime_error& e) {
        return AbortNode(state, std::string("System error: ") + e.what());
    }

    if (fCheckForPruning)
        FlushStateToDisk(chainparams, state, FLUSH_STATE_NONE); // we just allocated more disk space for block files

    return true;
}

bool ProcessNewBlock(const CChainParams& chainparams, const std::shared_ptr<const CBlock> pblock, bool fForceProcessing, bool *fNewBlock, bool fAssumePOWGood, bool checkFarAhead)
{
    bool fCloseToTip;
    {
        CBlockIndex *pindex = NULL;
        if (fNewBlock) *fNewBlock = false;
        CValidationState state;
        // Ensure that CheckBlock() passes before calling AcceptBlock, as
        // belt-and-suspenders.
        bool ret = CheckBlock(*pblock, state, chainparams.GetConsensus(), true, true, fAssumePOWGood);

        LOCK(cs_main);

        if (ret) {
            // Store to disk, skip toFarAway check if we are not planing to activate the best chain
            ret = AcceptBlock(pblock, state, chainparams, &pindex, fForceProcessing, nullptr, fNewBlock, fAssumePOWGood, checkFarAhead);
        }
        CheckBlockIndex(chainparams.GetConsensus());
        if (!ret) {
            GetMainSignals().BlockChecked(*pblock, state);
            return error("%s: AcceptBlock FAILED", __func__);
        }
        fCloseToTip = pindex->nHeight <= chainActive.Height() + int(MIN_BLOCKS_TO_KEEP);
    }

    if (fCloseToTip && isFullSyncMode()) {
        CValidationState state; // Only used to report errors, not invalidity - ignore it
        if (!ActivateBestChain(state, chainparams, pblock))
            return error("%s: ActivateBestChain failed", __func__);
    }

    if (!IsInitialBlockDownload())
    {
        // Gulden: if responsible for sync-checkpoint send it
        if (!CSyncCheckpoint::strMasterPrivKey.empty())
            Checkpoints::SendSyncCheckpoint(Checkpoints::AutoSelectSyncCheckpoint(), chainparams);

        // Gulden: check pending sync-checkpoint
        Checkpoints::AcceptPendingSyncCheckpoint(chainparams);
    }

    CheckAndNotifyHeaderTip();

    return true;
}

bool TestBlockValidity(CChain& chain, CValidationState& state, const CChainParams& chainparams, const CBlock& block, CBlockIndex* pindexPrev, bool fCheckPOW, bool fCheckMerkleRoot, CCoinsViewCache* cacheOverride)
{
    AssertLockHeld(cs_main);

    if(!pindexPrev || pindexPrev != chain.Tip())
        return false;

    if (fCheckpointsEnabled && !CheckIndexAgainstCheckpoint(pindexPrev, state, chainparams, block.GetHashPoW2()))
        return error("%s: CheckIndexAgainstCheckpoint(): %s", __func__, state.GetRejectReason().c_str());

    CCoinsViewCache viewNew(cacheOverride?cacheOverride:pcoinsTip);
    CBlockIndex indexDummy(block);
    indexDummy.pprev = pindexPrev;
    indexDummy.nHeight = pindexPrev->nHeight + 1;

    // NOTE: CheckBlockHeader is called by CheckBlock
    if (!ContextualCheckBlockHeader(block, state, chainparams.GetConsensus(), pindexPrev, GetAdjustedTime()))
        return error("%s: Consensus::ContextualCheckBlockHeader: %s", __func__, FormatStateMessage(state));
    if (!CheckBlock(block, state, chainparams.GetConsensus(), fCheckPOW, fCheckMerkleRoot))
        return error("%s: Consensus::CheckBlock: %s", __func__, FormatStateMessage(state));
    if (!ContextualCheckBlock(block, state, chainparams, pindexPrev, chain, cacheOverride))
        return error("%s: Consensus::ContextualCheckBlock: %s", __func__, FormatStateMessage(state));
    if (!ConnectBlock(chain, block, state, &indexDummy, viewNew, chainparams, true))
        return false;
    assert(state.IsValid());

    return true;
}

/**
 * BLOCK PRUNING CODE
 */

/* Calculate the amount of disk space the block & undo files currently use */
static uint64_t CalculateCurrentUsage()
{
    uint64_t retval = 0;
    for(const CBlockFileInfo &file : vinfoBlockFile) {
        retval += file.nSize + file.nUndoSize;
    }
    return retval;
}

/* Prune a block file (modify associated database entries)*/
void PruneOneBlockFile(const int fileNumber)
{
    for (BlockMap::iterator it = mapBlockIndex.begin(); it != mapBlockIndex.end(); ++it) {
        CBlockIndex* pindex = it->second;
        if (pindex->nFile == fileNumber) {
            pindex->nStatus &= ~BLOCK_HAVE_DATA;
            pindex->nStatus &= ~BLOCK_HAVE_UNDO;
            pindex->nFile = 0;
            pindex->nDataPos = 0;
            pindex->nUndoPos = 0;
            setDirtyBlockIndex.insert(pindex);

            // Prune from mapBlocksUnlinked -- any block we prune would have
            // to be downloaded again in order to consider its chain, at which
            // point it would be considered as a candidate for
            // mapBlocksUnlinked or setBlockIndexCandidates.
            std::pair<std::multimap<CBlockIndex*, CBlockIndex*>::iterator, std::multimap<CBlockIndex*, CBlockIndex*>::iterator> range = mapBlocksUnlinked.equal_range(pindex->pprev);
            while (range.first != range.second) {
                std::multimap<CBlockIndex *, CBlockIndex *>::iterator _it = range.first;
                range.first++;
                if (_it->second == pindex) {
                    mapBlocksUnlinked.erase(_it);
                }
            }
        }
    }

    vinfoBlockFile[fileNumber].SetNull();
    setDirtyFileInfo.insert(fileNumber);
}

/* Calculate the block/rev files to delete based on height specified by user with RPC command pruneblockchain */
void FindFilesToPruneManual(std::set<int>& setFilesToPrune, int nManualPruneHeight)
{
    assert(fPruneMode && nManualPruneHeight > 0);

    LOCK2(cs_main, cs_LastBlockFile);
    if (chainActive.Tip() == NULL)
        return;

    // last block to prune is the lesser of (user-specified height, MIN_BLOCKS_TO_KEEP from the tip)
    unsigned int nLastBlockWeCanPrune = std::min((unsigned)nManualPruneHeight, chainActive.Tip()->nHeight - MIN_BLOCKS_TO_KEEP);

    FindFilesToPruneExplicit(setFilesToPrune, nLastBlockWeCanPrune);
}

/* This function is called from the RPC code for pruneblockchain */
void PruneBlockFilesManual(int nManualPruneHeight)
{
    CValidationState state;
    const CChainParams& chainparams = Params();
    FlushStateToDisk(chainparams, state, FLUSH_STATE_NONE, nManualPruneHeight);
}

/**
 * Prune block and undo files (blk???.dat and undo???.dat) so that the disk space used is less than a user-defined target.
 * The user sets the target (in MB) on the command line or in config file.  This will be run on startup and whenever new
 * space is allocated in a block or undo file, staying below the target. Changing back to unpruned requires a reindex
 * (which in this case means the blockchain must be re-downloaded.)
 *
 * Pruning functions are called from FlushStateToDisk when the global fCheckForPruning flag has been set.
 * Block and undo files are deleted in lock-step (when blk00003.dat is deleted, so is rev00003.dat.)
 * Pruning cannot take place until the longest chain is at least a certain length (100000 on mainnet, 1000 on testnet, 1000 on regtest).
 * Pruning will never delete a block within a defined distance (currently 288) from the active chain's tip.
 * The block index is updated by unsetting HAVE_DATA and HAVE_UNDO for any blocks that were stored in the deleted files.
 * A db flag records the fact that at least some block files have been pruned.
 *
 * @param[out]   setFilesToPrune   The set of file indices that can be unlinked will be returned
 */
void FindFilesToPrune(std::set<int>& setFilesToPrune, uint64_t nPruneAfterHeight)
{
    LOCK2(cs_main, cs_LastBlockFile);
    if (chainActive.Tip() == NULL || nPruneTarget == 0) {
        return;
    }
    if ((uint64_t)chainActive.Tip()->nHeight <= nPruneAfterHeight) {
        return;
    }

    unsigned int nLastBlockWeCanPrune = chainActive.Tip()->nHeight - MIN_BLOCKS_TO_KEEP;
    uint64_t nCurrentUsage = CalculateCurrentUsage();
    // We don't check to prune until after we've allocated new space for files
    // So we should leave a buffer under our target to account for another allocation
    // before the next pruning.
    uint64_t nBuffer = BLOCKFILE_CHUNK_SIZE + UNDOFILE_CHUNK_SIZE;
    uint64_t nBytesToPrune;
    int count=0;

    if (nCurrentUsage + nBuffer >= nPruneTarget) {
        for (int fileNumber = 0; fileNumber < nLastBlockFile; fileNumber++) {
            nBytesToPrune = vinfoBlockFile[fileNumber].nSize + vinfoBlockFile[fileNumber].nUndoSize;

            if (vinfoBlockFile[fileNumber].nSize == 0)
                continue;

            if (nCurrentUsage + nBuffer < nPruneTarget)  // are we below our target?
                break;

            // don't prune files that could have a block within MIN_BLOCKS_TO_KEEP of the main chain's tip but keep scanning
            if (vinfoBlockFile[fileNumber].nHeightLast > nLastBlockWeCanPrune)
                continue;

            PruneOneBlockFile(fileNumber);
            // Queue up the files for removal
            setFilesToPrune.insert(fileNumber);
            nCurrentUsage -= nBytesToPrune;
            count++;
        }
    }

    LogPrint(BCLog::PRUNE, "Prune: target=%dMiB actual=%dMiB diff=%dMiB max_prune_height=%d removed %d blk/rev pairs\n",
           nPruneTarget/1024/1024, nCurrentUsage/1024/1024,
           ((int64_t)nPruneTarget - (int64_t)nCurrentUsage)/1024/1024,
           nLastBlockWeCanPrune, count);
}

bool CheckDiskSpace(uint64_t nAdditionalBytes)
{
    uint64_t nFreeBytesAvailable = fs::space(GetDataDir()).available;

    // Check for nMinDiskSpace bytes (currently 50MB)
    if (nFreeBytesAvailable < nMinDiskSpace + nAdditionalBytes)
        return AbortNode("Disk space is low!", _("Error: Disk space is low!"));

    return true;
}

CBlockIndex * InsertBlockIndex(uint256 hash)
{
    if (hash.IsNull())
        return NULL;

    // Return existing
    BlockMap::iterator mi = mapBlockIndex.find(hash);
    if (mi != mapBlockIndex.end())
        return (*mi).second;

    // Create new
    CBlockIndex* pindexNew = new CBlockIndex();
    if (!pindexNew)
        throw std::runtime_error(std::string(__func__) + ": new CBlockIndex failed");
    mi = mapBlockIndex.insert(std::pair(hash, pindexNew)).first;
    pindexNew->phashBlock = &((*mi).first);

    return pindexNew;
}

bool static LoadBlockIndexDB(const CChainParams& chainparams)
{
    LOCK(cs_main);

    if (!pblocktree->LoadBlockIndexGuts(InsertBlockIndex))
        return false;

    boost::this_thread::interruption_point();

    // Calculate nChainWork
    std::vector<std::pair<int, CBlockIndex*> > vSortedByHeight;
    vSortedByHeight.reserve(mapBlockIndex.size());
    for(const PAIRTYPE(uint256, CBlockIndex*)& item : mapBlockIndex)
    {
        CBlockIndex* pindex = item.second;
        vSortedByHeight.push_back(std::pair(pindex->nHeight, pindex));
    }
    sort(vSortedByHeight.begin(), vSortedByHeight.end(), [](const std::pair<int, CBlockIndex*>& a, const std::pair<int, CBlockIndex*>& b) -> bool 
    {
        //Ensure PoW block always comes first in sort before witness block of same height.
        if (a.first == b.first)
        {
            if (a.second->nVersionPoW2Witness == 0 && b.second->nVersionPoW2Witness > 0)
                return true;
            if (a.second->nVersionPoW2Witness > 0 && b.second->nVersionPoW2Witness == 0)
                return false;
            return a.second < b.second;
        }
        return a.first < b.first;
    }
    );
    for(const PAIRTYPE(int, CBlockIndex*)& item : vSortedByHeight)
    {
        CBlockIndex* pindex = item.second;
        pindex->nChainWork = CalculateChainWork(pindex, chainparams);;
        pindex->nTimeMax = (pindex->pprev ? std::max(pindex->pprev->nTimeMax, pindex->nTime) : pindex->nTime);
        // We can link the chain of blocks for which we've received transactions at some point.
        // Pruned nodes may have deleted the block.
        if (pindex->nTx > 0) {
            if (pindex->pprev) {
                if (pindex->pprev->nChainTx) {
                    pindex->nChainTx = pindex->pprev->nChainTx + pindex->nTx;
                } else {
                    pindex->nChainTx = 0;
                    mapBlocksUnlinked.insert(std::pair(pindex->pprev, pindex));
                }
            } else {
                pindex->nChainTx = pindex->nTx;
            }
        }

        if (pindex->IsValid(BLOCK_VALID_TRANSACTIONS) && (pindex->nChainTx || pindex->pprev == NULL))
        {
            setBlockIndexCandidates.insert(pindex);
            //LogPrintf("LoadBlockIndexDB: New index candidate: [%s] [%d]\n", pindex->GetBlockHashPoW2().ToString(), pindex->nHeight);
        }
        if (pindex->nStatus & BLOCK_FAILED_MASK && (!pindexBestInvalid || pindex->nChainWork > pindexBestInvalid->nChainWork))
            pindexBestInvalid = pindex;
        if (pindex->pprev)
            pindex->BuildSkip();
        if (pindex->IsValid(BLOCK_VALID_TREE) && (pindexBestHeader == NULL || CBlockIndexWorkComparator()(pindexBestHeader, pindex))) {
            pindexBestHeader = pindex;
        }
        if ((pindex->IsPartialValid(BLOCK_PARTIAL_TREE))
                && (!pindexBestPartial || pindex->nHeight >= pindexBestPartial->nHeight))
            pindexBestPartial = pindex;
    }

    if (pindexBestPartial)
    {
        CBlockIndex* pindex = pindexBestPartial;
        while (pindex->pprev && pindex->pprev->IsPartialValid(BLOCK_PARTIAL_TREE))
            pindex = pindex->pprev;
        partialChain.SetHeightOffset(pindex->nHeight);
        partialChain.SetTip(pindexBestPartial);

        // if block index loading createed an empty index in front of the partial chain it needs to be removed
        if (partialChain[partialChain.HeightOffset()]->pprev && partialChain[partialChain.HeightOffset()]->pprev->nStatus == 0)
        {
            uint256 prevHash = partialChain[partialChain.HeightOffset()]->pprev->GetBlockHashPoW2();
            mapBlockIndex.erase(prevHash);
            partialChain[partialChain.HeightOffset()]->pprev = nullptr;
        }
    }

    // Load block file info
    pblocktree->ReadLastBlockFile(nLastBlockFile);
    vinfoBlockFile.resize(nLastBlockFile + 1);
    LogPrintf("%s: last block file = %i\n", __func__, nLastBlockFile);
    for (int nFile = 0; nFile <= nLastBlockFile; nFile++) {
        pblocktree->ReadBlockFileInfo(nFile, vinfoBlockFile[nFile]);
    }
    LogPrintf("%s: last block file info: %s\n", __func__, vinfoBlockFile[nLastBlockFile].ToString());
    for (int nFile = nLastBlockFile + 1; true; nFile++) {
        CBlockFileInfo info;
        if (pblocktree->ReadBlockFileInfo(nFile, info)) {
            vinfoBlockFile.push_back(info);
        } else {
            break;
        }
    }

    // Check presence of blk files
    LogPrintf("Checking all blk files are present...\n");
    std::set<int> setBlkDataFiles;
    for(const PAIRTYPE(uint256, CBlockIndex*)& item : mapBlockIndex)
    {
        CBlockIndex* pindex = item.second;
        if (pindex->nStatus & BLOCK_HAVE_DATA) {
            setBlkDataFiles.insert(pindex->nFile);
        }
    }
    for (std::set<int>::iterator it = setBlkDataFiles.begin(); it != setBlkDataFiles.end(); it++)
    {
        CDiskBlockPos pos(*it, 0);
        if (CFile(blockStore.GetBlockFile(pos, true), SER_DISK, CLIENT_VERSION).IsNull()) {
            return false;
        }
    }

    // Check whether we have ever pruned block & undo files
    pblocktree->ReadFlag("prunedblockfiles", fHavePruned);
    if (fHavePruned)
        LogPrintf("LoadBlockIndexDB(): Block files have previously been pruned\n");

    // Check whether we need to continue reindexing
    bool fReindexing = false;
    pblocktree->ReadReindexing(fReindexing);
    fReindex |= fReindexing;

    // Check whether we have a transaction index
    pblocktree->ReadFlag("txindex", fTxIndex);
    LogPrintf("%s: transaction index %s\n", __func__, fTxIndex ? "enabled" : "disabled");

    // Load pointer to end of best chain
    BlockMap::iterator it = mapBlockIndex.find(pcoinsTip->GetBestBlock());
    if (it == mapBlockIndex.end())
        return true;
    chainActive.SetTip(it->second);

    PruneBlockIndexCandidates();

    //Temporary code to clean up old checkpoints database - We can remove this in future versions
    if ( fs::exists(GetDataDir() / "checkpoints") )
    {
        fs::remove_all( GetDataDir() / "checkpoints" );
    }

    // Gulden: load hashSyncCheckpoint
    Checkpoints::ReadSyncCheckpoint(Checkpoints::hashSyncCheckpoint);
    LogPrintf("LoadBlockIndexDB(): using synchronized checkpoint %s\n", Checkpoints::hashSyncCheckpoint.ToString().c_str());

    LogPrintf("%s: hashBestChain=%s height=%d date=%s progress=%f\n", __func__,
        chainActive.Tip()->GetBlockHashPoW2().ToString(), chainActive.Height(),
        DateTimeStrFormat("%Y-%m-%d %H:%M:%S", chainActive.Tip()->GetBlockTime()),
        GuessVerificationProgress(chainparams.TxData(), chainActive.Tip()));

    return true;
}

bool UpgradeBlockIndex(const CChainParams& chainparams, int nPreviousVersion, int nCurrentVersion)
{
    LOCK(cs_main);

    // Gulden 2.0 onwards
    // Refresh all blocks on disk - change in serialisation format.
    if (nPreviousVersion == 0 && nCurrentVersion >= 1)
    {
        blockStore.CloseBlockFiles();

        CBlockStore oldStore(true);

        oldStore.Rename("16_");

        {
            LOCK(cs_LastBlockFile);
            vinfoBlockFile.clear();
            nLastBlockFile = 0;
        }

        // (Optimisation) Sort by height so that the files end up "in order" in the new block files.
        std::vector<std::pair<int, CBlockIndex*> > vSortedByHeight;
        vSortedByHeight.reserve(mapBlockIndex.size());
        for(const PAIRTYPE(uint256, CBlockIndex*)& item : mapBlockIndex)
        {
            CBlockIndex* pindex = item.second;
            vSortedByHeight.push_back(std::pair(pindex->nHeight, pindex));
        }
        sort(vSortedByHeight.begin(), vSortedByHeight.end(), [](const std::pair<int, CBlockIndex*>& a, const std::pair<int, CBlockIndex*>& b) -> bool { return a.first < b.first; });

        // Now read the block files in one at a time from the old files and write them into the new files.
        CBlock* pblock = new CBlock();
        std::vector<std::pair<int, const CBlockFileInfo*> > vDirtyFiles;
        std::vector<const CBlockIndex*> vDirtyBlocks;
        for(const auto& item: vSortedByHeight)
        {
            CBlockIndex* pindex = item.second;
            pblock->SetNull();

            CDiskBlockPos blockpos = pindex->GetBlockPos();
            CDiskBlockPos undoPos = pindex->GetUndoPos();

            if (blockpos.nFile >= 0)
            {
                vDirtyFiles.push_back(std::pair(pindex->nFile, &vinfoBlockFile[pindex->nFile]));
                // Read block in, using old transaction format.
                {
                    if (!oldStore.ReadBlockFromDisk(*pblock, blockpos, chainparams))
                        return error("UpgradeBlockIndex: ReadBlockFromDisk: Errors in block at %s", blockpos.ToString());
                }

                // Write block out using new transaction format.
                {
                    unsigned int nSize = ::GetSerializeSize(*pblock, SER_DISK, CLIENT_VERSION);
                    CValidationState state;
                    FindBlockPos(state, blockpos, nSize+8, pindex->nHeight, pblock->GetBlockTime());
                    if (!blockStore.WriteBlockToDisk(*pblock, blockpos, chainparams.MessageStart()))
                        return error("UpgradeBlockIndex: WriteBlockToDisk: failed");
                    pindex->nFile = blockpos.nFile;
                    pindex->nDataPos = blockpos.nPos;
                }

                if (pindex->nStatus & BLOCK_HAVE_UNDO)
                {
                    // Read undo information
                    CBlockUndo blockundo;
                    {
                        if (!oldStore.UndoReadFromDisk(blockundo, undoPos, pindex->pprev->GetBlockHashPoW2()))
                            return error("UpgradeBlockIndex: UndoReadFromDisk failed");
                    }

                    // Write undo information back to disk with modified hash
                    {
                        CValidationState state;
                        CDiskBlockPos undoDiskPos;
                        if (!FindUndoPos(state, pindex->nFile, undoDiskPos, ::GetSerializeSize(blockundo, SER_DISK, CLIENT_VERSION) + 40))
                            return error("UpgradeBlockIndex: FindUndoPos failed");
                        if (!blockStore.UndoWriteToDisk(blockundo, undoDiskPos, pindex->pprev->GetBlockHashPoW2(), chainparams.MessageStart()))
                            return AbortNode(state, "UpgradeBlockIndex: Failed to write undo data");

                        // update nUndoPos in block index
                        pindex->nUndoPos = undoDiskPos.nPos;
                    }
                }

                // Update the block index as the position of the block on disk has changed.
                vDirtyBlocks.push_back(pindex);
                vDirtyFiles.push_back(std::pair(pindex->nFile, &vinfoBlockFile[pindex->nFile]));

                if (pindex->nHeight == chainActive.Tip()->nHeight)
                {
                    assert(pindex->nDataPos == chainActive.Tip()->nDataPos);
                    assert(pindex->nUndoPos == chainActive.Tip()->nUndoPos);
                }
            }
        }
        delete pblock;

        FlushBlockFile();
        if (!pblocktree->WriteBatchSync(vDirtyFiles, nLastBlockFile, vDirtyBlocks))
        {
            return error("UpgradeBlockIndex: Failed to write to block index database");
        }

        oldStore.Delete();
    }

    return true;
}

CVerifyDB::CVerifyDB()
{
    uiInterface.ShowProgress(_("Verifying blocks..."), 0);
}

CVerifyDB::~CVerifyDB()
{
    uiInterface.ShowProgress("", 100);
}

bool CVerifyDB::VerifyDB(const CChainParams& chainparams, CCoinsView *coinsview, int nCheckLevel, int nCheckDepth)
{
    LOCK(cs_main); // Required for ReadBlockFromDisk.
    if (chainActive.Tip() == NULL || chainActive.Tip()->pprev == NULL)
        return true;

    // Verify blocks in the best chain
    if (nCheckDepth <= 0)
        nCheckDepth = 1000000000; // suffices until the year 19000
    if (nCheckDepth > chainActive.Height())
        nCheckDepth = chainActive.Height();
    nCheckLevel = std::max(0, std::min(4, nCheckLevel));
    LogPrintf("Verifying last %i blocks at level %i\n", nCheckDepth, nCheckLevel);
    CCoinsViewCache coins(coinsview);
    CBlockIndex* pindexState = chainActive.Tip();
    CBlockIndex* pindexFailure = NULL;
    int nGoodTransactions = 0;
    CValidationState state;
    int reportDone = 0;
    LogPrintf("[0%%]...");
    for (CBlockIndex* pindex = chainActive.Tip(); pindex && pindex->pprev; pindex = pindex->pprev)
    {
        boost::this_thread::interruption_point();
        int percentageDone = std::max(1, std::min(99, (int)(((double)(chainActive.Height() - pindex->nHeight)) / (double)nCheckDepth * (nCheckLevel >= 4 ? 50 : 100))));
        if (reportDone < percentageDone/10) {
            // report every 10% step
            LogPrintf("[%d%%]...", percentageDone);
            reportDone = percentageDone/10;
        }
        uiInterface.ShowProgress(_("Verifying blocks..."), percentageDone);
        if (pindex->nHeight < chainActive.Height()-nCheckDepth)
            break;
        if (fPruneMode && !(pindex->nStatus & BLOCK_HAVE_DATA)) {
            // If pruning, only go back as far as we have data.
            LogPrintf("VerifyDB(): block verification stopping at height %d (pruning, no data)\n", pindex->nHeight);
            break;
        }
        CBlock block;
        // check level 0: read from disk
        if (!ReadBlockFromDisk(block, pindex, chainparams))
            return error("VerifyDB(): *** ReadBlockFromDisk failed at %d, hash=%s", pindex->nHeight, pindex->GetBlockHashPoW2().ToString());
        // check level 1: verify block validity
        if (nCheckLevel >= 1 && !CheckBlock(block, state, chainparams.GetConsensus()))
            return error("%s: *** found bad block at %d, hash=%s (%s)\n", __func__,
                         pindex->nHeight, pindex->GetBlockHashPoW2().ToString(), FormatStateMessage(state));
        // check level 2: verify undo validity
        if (nCheckLevel >= 2 && pindex) {
            CBlockUndo undo;
            CDiskBlockPos pos = pindex->GetUndoPos();
            if (!pos.IsNull()) {
                if (!blockStore.UndoReadFromDisk(undo, pos, pindex->pprev->GetBlockHashPoW2()))
                    return error("VerifyDB(): *** found bad undo data at %d, hash=%s\n", pindex->nHeight, pindex->GetBlockHashPoW2().ToString());
            }
        }
        // check level 3: check for inconsistencies during memory-only disconnect of tip blocks
        if (nCheckLevel >= 3 && pindex == pindexState && (coins.DynamicMemoryUsage() + pcoinsTip->DynamicMemoryUsage()) <= nCoinCacheUsage) {
            DisconnectResult res = DisconnectBlock(block, pindex, coins);
            if (res == DISCONNECT_FAILED) {
                return error("VerifyDB(): *** irrecoverable inconsistency in block data at %d, hash=%s", pindex->nHeight, pindex->GetBlockHashPoW2().ToString());
            }
            pindexState = pindex->pprev;
            if (res == DISCONNECT_UNCLEAN) {
                nGoodTransactions = 0;
                pindexFailure = pindex;
            } else {
                nGoodTransactions += block.vtx.size();
            }
        }
        if (ShutdownRequested())
            return true;
    }
    if (pindexFailure)
        return error("VerifyDB(): *** coin database inconsistencies found (last %i blocks, %i good transactions before that)\n", chainActive.Height() - pindexFailure->nHeight + 1, nGoodTransactions);

    // check level 4: try reconnecting blocks
    if (nCheckLevel >= 4) {
        CBlockIndex *pindex = pindexState;
        while (pindex != chainActive.Tip()) {
            boost::this_thread::interruption_point();
            uiInterface.ShowProgress(_("Verifying blocks..."), std::max(1, std::min(99, 100 - (int)(((double)(chainActive.Height() - pindex->nHeight)) / (double)nCheckDepth * 50))));
            pindex = chainActive.Next(pindex);
            CBlock block;
            if (!ReadBlockFromDisk(block, pindex, chainparams))
                return error("VerifyDB(): *** ReadBlockFromDisk failed at %d, hash=%s", pindex->nHeight, pindex->GetBlockHashPoW2().ToString());
            if (!ConnectBlock(chainActive, block, state, pindex, coins, chainparams))
                return error("VerifyDB(): *** found unconnectable block at %d, hash=%s", pindex->nHeight, pindex->GetBlockHashPoW2().ToString());
        }
    }

    LogPrintf("[DONE].\n");
    LogPrintf("No coin database inconsistencies in last %i blocks (%i transactions)\n", chainActive.Height() - pindexState->nHeight, nGoodTransactions);

    return true;
}

bool RewindBlockIndex(const CChainParams& params)
{
    LOCK(cs_main);

    int nHeight = 1;
    while (nHeight <= chainActive.Height())
    {
        if (IsSegSigEnabled(chainActive[nHeight - 1]) && !(chainActive[nHeight]->nStatus & BLOCK_OPT_WITNESS))
        {
            break;
        }
        nHeight++;
    }

    // nHeight is now the height of the first insufficiently-validated block, or tipheight + 1
    CValidationState state;
    CBlockIndex* pindex = chainActive.Tip();
    while (chainActive.Height() >= nHeight) {
        if (fPruneMode && !(chainActive.Tip()->nStatus & BLOCK_HAVE_DATA)) {
            // If pruning, don't try rewinding past the HAVE_DATA point;
            // since older blocks can't be served anyway, there's
            // no need to walk further, and trying to DisconnectTip()
            // will fail (and require a needless reindex/redownload
            // of the blockchain).
            break;
        }
        if (!DisconnectTip(state, params, NULL)) {
            return error("RewindBlockIndex: unable to disconnect block at height %i", pindex->nHeight);
        }
        // Occasionally flush state to disk.
        if (!FlushStateToDisk(params, state, FLUSH_STATE_PERIODIC))
            return false;
    }

    // Reduce validity flag and have-data flags.
    // We do this after actual disconnecting, otherwise we'll end up writing the lack of data
    // to disk before writing the chainstate, resulting in a failure to continue if interrupted.
    for (BlockMap::iterator it = mapBlockIndex.begin(); it != mapBlockIndex.end(); it++) {
        CBlockIndex* pindexIter = it->second;

        // Note: If we encounter an insufficiently validated block that
        // is on chainActive, it must be because we are a pruning node, and
        // this block or some successor doesn't HAVE_DATA, so we were unable to
        // rewind all the way.  Blocks remaining on chainActive at this point
        // must not have their validity reduced.
        if (IsSegSigEnabled(pindexIter->pprev) && !(pindexIter->nStatus & BLOCK_OPT_WITNESS) && !chainActive.Contains(pindexIter)) {
            // Reduce validity
            pindexIter->nStatus = std::min<unsigned int>(pindexIter->nStatus & BLOCK_VALID_MASK, BLOCK_VALID_TREE) | (pindexIter->nStatus & ~BLOCK_VALID_MASK);
            // Remove have-data flags.
            pindexIter->nStatus &= ~(BLOCK_HAVE_DATA | BLOCK_HAVE_UNDO);
            // Remove storage location.
            pindexIter->nFile = 0;
            pindexIter->nDataPos = 0;
            pindexIter->nUndoPos = 0;
            // Remove various other things
            pindexIter->nTx = 0;
            pindexIter->nChainTx = 0;
            pindexIter->nSequenceId = 0;
            // Make sure it gets written.
            setDirtyBlockIndex.insert(pindexIter);
            // Update indexes
            setBlockIndexCandidates.erase(pindexIter);
            std::pair<std::multimap<CBlockIndex*, CBlockIndex*>::iterator, std::multimap<CBlockIndex*, CBlockIndex*>::iterator> ret = mapBlocksUnlinked.equal_range(pindexIter->pprev);
            while (ret.first != ret.second) {
                if (ret.first->second == pindexIter) {
                    mapBlocksUnlinked.erase(ret.first++);
                } else {
                    ++ret.first;
                }
            }
        } else if (pindexIter->IsValid(BLOCK_VALID_TRANSACTIONS) && pindexIter->nChainTx)
        {
            setBlockIndexCandidates.insert(pindexIter);
            //LogPrintf("RewindBlockIndex: New index candidate: [%s] [%d]\n", pindexIter->GetBlockHashPoW2().ToString(), pindexIter->nHeight);
        }
    }

    PruneBlockIndexCandidates();

    CheckBlockIndex(params.GetConsensus());

    if (!FlushStateToDisk(params, state, FLUSH_STATE_ALWAYS)) {
        return false;
    }

    return true;
}

// May NOT be used after any connections are up as much
// of the peer-processing logic assumes a consistent
// block index state
void UnloadBlockIndex()
{
    LOCK(cs_main);
    setBlockIndexCandidates.clear();
    chainActive.SetTip(NULL);
    pindexBestInvalid = NULL;
    pindexBestHeader = NULL;
    partialChain.SetTip(nullptr);
    mempool.clear();
    mapBlocksUnlinked.clear();
    vinfoBlockFile.clear();
    nLastBlockFile = 0;
    nBlockSequenceId = 1;
    setDirtyBlockIndex.clear();
    setDirtyFileInfo.clear();
    versionbitscache.Clear();
    for (int b = 0; b < VERSIONBITS_NUM_BITS; b++) {
        warningcache[b].clear();
    }

    for(BlockMap::value_type& entry : mapBlockIndex) {
        delete entry.second;
    }
    mapBlockIndex.clear();
    fHavePruned = false;
}

bool LoadBlockIndex(const CChainParams& chainparams)
{
    // Load block index from databases
    if (!fReindex && !LoadBlockIndexDB(chainparams))
        return false;
    return true;
}

bool InitBlockIndex(const CChainParams& chainparams)
{
    LOCK(cs_main);

    // Check whether we're already initialized
    if (chainActive.Genesis() != NULL)
        return true;

    // Use the provided setting for -txindex in the new database
    fTxIndex = GetBoolArg("-txindex", DEFAULT_TXINDEX);
    pblocktree->WriteFlag("txindex", fTxIndex);
    LogPrintf("Initializing databases...\n");

    // Only add the genesis block if not reindexing (in which case we reuse the one already on disk)
    if (!fReindex) {
        try {
            CBlock &block = const_cast<CBlock&>(chainparams.GenesisBlock());
            // Start new block file
            unsigned int nBlockSize = ::GetSerializeSize(block, SER_DISK, CLIENT_VERSION);
            CDiskBlockPos blockPos;
            CValidationState state;
            if (!FindBlockPos(state, blockPos, nBlockSize+8, 0, block.GetBlockTime()))
                return error("LoadBlockIndex(): FindBlockPos failed");
            if (!blockStore.WriteBlockToDisk(block, blockPos, chainparams.MessageStart()))
                return error("LoadBlockIndex(): writing genesis block to disk failed");
            CBlockIndex *pindex = AddToBlockIndex(chainparams, block);
            if (!ReceivedBlockTransactions(block, state, pindex, blockPos, chainparams.GetConsensus()))
                return error("LoadBlockIndex(): genesis block not accepted");

            // Gulden: initialize synchronized checkpoint
            if (!Checkpoints::WriteSyncCheckpoint(Params().GenesisBlock().GetHashLegacy()))
                return error("LoadBlockIndex() : failed to init sync checkpoint");
            std::string strPubKey;
            std::string strPubKeyComp = IsArgSet("-testnet") ? CSyncCheckpoint::strMasterPubKeyTestnet : CSyncCheckpoint::strMasterPubKey;
            if (chainparams.UseSyncCheckpoints())
            {
                if (!Checkpoints::ReadCheckpointPubKey(strPubKey) || strPubKey != strPubKeyComp)
                {
                    // write checkpoint master key to db
                    if (!Checkpoints::WriteCheckpointPubKey(strPubKeyComp))
                        return error("LoadBlockIndex() : failed to write new checkpoint master key to db");
                    if (!Checkpoints::ResetSyncCheckpoint(chainparams))
                        return error("LoadBlockIndex() : failed to reset sync-checkpoint");
                }
            }
            LogPrintf("Wrote sync checkpoint...\n");

            // Force a chainstate write so that when we VerifyDB in a moment, it doesn't check stale data
            return FlushStateToDisk(chainparams, state, FLUSH_STATE_ALWAYS);
        } catch (const std::runtime_error& e) {
            return error("LoadBlockIndex(): failed to initialize block database: %s", e.what());
        }
    }

    return true;
}

void PersistAndPruneForPartialSync(bool periodic)
{
    // should be run at shutdown so that the block index remains small and
    // next startup stays fast
    // also run periodically (and on demand at key points, ie. app to background)
    // to prevent loss of data and needing to re-dowload

    LOCK(cs_main);

    if (isFullSyncMode() || !IsPartialSyncActive())
        return;

    // keep at least PARTIAL_SYNC_PRUNE_HEIGHT of history, also pruning before start of the partial chain
    // makes little sense
    int pruneHeight = std::max(partialChain.HeightOffset(), partialChain.Height() - PARTIAL_SYNC_PRUNE_HEIGHT);
    // never use a pruning height above what has been spv processed
    pruneHeight = std::min(nMaxSPVPruneHeight.load(), pruneHeight);

    CValidationState state;
    FlushStateToDisk(Params(), state,
                     periodic ? FlushStateMode::FLUSH_STATE_PERIODIC : FlushStateMode::FLUSH_STATE_ALWAYS,
                     pruneHeight, true);
}

bool LoadExternalBlockFile(const CChainParams& chainparams, FILE* fileIn, CDiskBlockPos *dbp)
{
    // Map of disk positions for blocks with unknown parent (only used for reindex)
    static std::multimap<uint256, CDiskBlockPos> mapBlocksUnknownParent;
    int64_t nStart = GetTimeMillis();

    int nLoaded = 0;
    try {
        // This takes over fileIn and calls fclose() on it in the CBufferedFile destructor
        CBufferedFile blkdat(fileIn, 2*MAX_BLOCK_SERIALIZED_SIZE, MAX_BLOCK_SERIALIZED_SIZE+8, SER_DISK, CLIENT_VERSION);
        uint64_t nRewind = blkdat.GetPos();
        while (!blkdat.eof()) {
            boost::this_thread::interruption_point();

            blkdat.SetPos(nRewind);
            nRewind++; // start one byte further next time, in case of failure
            blkdat.SetLimit(); // remove former limit
            unsigned int nSize = 0;
            try {
                // locate a header
                unsigned char buf[CMessageHeader::MESSAGE_START_SIZE];
                blkdat.FindByte(chainparams.MessageStart()[0]);
                nRewind = blkdat.GetPos()+1;
                blkdat >> FLATDATA(buf);
                if (memcmp(buf, chainparams.MessageStart(), CMessageHeader::MESSAGE_START_SIZE))
                    continue;
                // read size
                blkdat >> nSize;
                if (nSize < 80 || nSize > MAX_BLOCK_SERIALIZED_SIZE)
                    continue;
            } catch (const std::exception&) {
                // no valid block header found; don't complain
                break;
            }
            try {
                // read block
                uint64_t nBlockPos = blkdat.GetPos();
                if (dbp)
                    dbp->nPos = nBlockPos;
                blkdat.SetLimit(nBlockPos + nSize);
                blkdat.SetPos(nBlockPos);
                std::shared_ptr<CBlock> pblock = std::make_shared<CBlock>();
                CBlock& block = *pblock;
                blkdat >> block;
                nRewind = blkdat.GetPos();

                // detect out of order blocks, and store them for later
                uint256 hash = block.GetHashPoW2();
                if (hash != chainparams.GetConsensus().hashGenesisBlock && mapBlockIndex.find(block.hashPrevBlock) == mapBlockIndex.end()) {
                    LogPrint(BCLog::REINDEX, "%s: Out of order block %s, parent %s not known\n", __func__, hash.ToString(),
                            block.hashPrevBlock.ToString());
                    if (dbp)
                        mapBlocksUnknownParent.insert(std::pair(block.hashPrevBlock, *dbp));
                    continue;
                }

                // process in case the block isn't known yet
                if (mapBlockIndex.count(hash) == 0 || (mapBlockIndex[hash]->nStatus & BLOCK_HAVE_DATA) == 0) {
                    LOCK(cs_main);
                    CValidationState state;
                    if (AcceptBlock(pblock, state, chainparams, NULL, true, dbp, NULL, false, true))
                        nLoaded++;
                    if (state.IsError())
                        break;
                } else if (hash != chainparams.GetConsensus().hashGenesisBlock && mapBlockIndex[hash]->nHeight % 1000 == 0) {
                    LogPrint(BCLog::REINDEX, "Block Import: already had block %s at height %d\n", hash.ToString(), mapBlockIndex[hash]->nHeight);
                }

                // Activate the genesis block so normal node progress can continue
                if (hash == chainparams.GetConsensus().hashGenesisBlock) {
                    CValidationState state;
                    if (!ActivateBestChain(state, chainparams)) {
                        break;
                    }
                }

                // Recursively process earlier encountered successors of this block
                std::deque<uint256> queue;
                queue.push_back(hash);
                while (!queue.empty()) {
                    uint256 head = queue.front();
                    queue.pop_front();
                    std::pair<std::multimap<uint256, CDiskBlockPos>::iterator, std::multimap<uint256, CDiskBlockPos>::iterator> range = mapBlocksUnknownParent.equal_range(head);
                    while (range.first != range.second) {
                        std::multimap<uint256, CDiskBlockPos>::iterator it = range.first;
                        std::shared_ptr<CBlock> pblockrecursive = std::make_shared<CBlock>();
                        {
                            LOCK(cs_main); // acquire cs_main here to protect ReadBlockFromDisk
                            if (blockStore.ReadBlockFromDisk(*pblockrecursive, it->second, chainparams))
                            {
                                LogPrint(BCLog::REINDEX, "%s: Processing out of order child %s of %s\n", __func__, pblockrecursive->GetHashPoW2().ToString(),
                                        head.ToString());
                                CValidationState dummy;
                                if (AcceptBlock(pblockrecursive, dummy, chainparams, NULL, true, &it->second, NULL, false, true))
                                {
                                    nLoaded++;
                                    queue.push_back(pblockrecursive->GetHashPoW2());
                                }
                            }
                        }
                        range.first++;
                        mapBlocksUnknownParent.erase(it);
                    }
                }
            } catch (const std::exception& e) {
                LogPrintf("%s: Deserialize or I/O error - %s\n", __func__, e.what());
            }
        }
    } catch (const std::runtime_error& e) {
        AbortNode(std::string("System error: ") + e.what());
    }
    if (nLoaded > 0)
        LogPrintf("Loaded %i blocks from external file in %dms\n", nLoaded, GetTimeMillis() - nStart);
    return nLoaded > 0;
}

void static CheckBlockIndex(const Consensus::Params& consensusParams)
{
    if (!fCheckBlockIndex) {
        return;
    }

    LOCK(cs_main);

    // During a reindex, we read the genesis block and call CheckBlockIndex before ActivateBestChain,
    // so we have the genesis block in mapBlockIndex but no active chain.  (A few of the tests when
    // iterating the block tree require that chainActive has been initialized.)
    if (chainActive.Height() < 0) {
        assert(mapBlockIndex.size() <= 1);
        return;
    }

    // Build forward-pointing map of the entire block tree.
    std::multimap<CBlockIndex*,CBlockIndex*> forward;
    for (BlockMap::iterator it = mapBlockIndex.begin(); it != mapBlockIndex.end(); it++) {
        forward.insert(std::pair(it->second->pprev, it->second));
    }

    assert(forward.size() == mapBlockIndex.size());

    std::pair<std::multimap<CBlockIndex*,CBlockIndex*>::iterator,std::multimap<CBlockIndex*,CBlockIndex*>::iterator> rangeGenesis = forward.equal_range(NULL);
    CBlockIndex *pindex = rangeGenesis.first->second;
    rangeGenesis.first++;
    assert(rangeGenesis.first == rangeGenesis.second); // There is only one index entry with parent NULL.

    // Iterate over the entire block tree, using depth-first search.
    // Along the way, remember whether there are blocks on the path from genesis
    // block being explored which are the first to have certain properties.
    size_t nNodes = 0;
    int nHeight = 0;
    CBlockIndex* pindexFirstInvalid = NULL; // Oldest ancestor of pindex which is invalid.
    CBlockIndex* pindexFirstMissing = NULL; // Oldest ancestor of pindex which does not have BLOCK_HAVE_DATA.
    CBlockIndex* pindexFirstNeverProcessed = NULL; // Oldest ancestor of pindex for which nTx == 0.
    CBlockIndex* pindexFirstNotTreeValid = NULL; // Oldest ancestor of pindex which does not have BLOCK_VALID_TREE (regardless of being valid or not).
    CBlockIndex* pindexFirstNotTransactionsValid = NULL; // Oldest ancestor of pindex which does not have BLOCK_VALID_TRANSACTIONS (regardless of being valid or not).
    CBlockIndex* pindexFirstNotChainValid = NULL; // Oldest ancestor of pindex which does not have BLOCK_VALID_CHAIN (regardless of being valid or not).
    CBlockIndex* pindexFirstNotScriptsValid = NULL; // Oldest ancestor of pindex which does not have BLOCK_VALID_SCRIPTS (regardless of being valid or not).
    while (pindex != NULL) {
        nNodes++;
        if (pindexFirstInvalid == NULL && pindex->nStatus & BLOCK_FAILED_VALID) pindexFirstInvalid = pindex;
        if (pindexFirstMissing == NULL && !(pindex->nStatus & BLOCK_HAVE_DATA)) pindexFirstMissing = pindex;
        if (pindexFirstNeverProcessed == NULL && pindex->nTx == 0) pindexFirstNeverProcessed = pindex;
        if (pindex->pprev != NULL && pindexFirstNotTreeValid == NULL && (pindex->nStatus & BLOCK_VALID_MASK) < BLOCK_VALID_TREE) pindexFirstNotTreeValid = pindex;
        if (pindex->pprev != NULL && pindexFirstNotTransactionsValid == NULL && (pindex->nStatus & BLOCK_VALID_MASK) < BLOCK_VALID_TRANSACTIONS) pindexFirstNotTransactionsValid = pindex;
        if (pindex->pprev != NULL && pindexFirstNotChainValid == NULL && (pindex->nStatus & BLOCK_VALID_MASK) < BLOCK_VALID_CHAIN) pindexFirstNotChainValid = pindex;
        if (pindex->pprev != NULL && pindexFirstNotScriptsValid == NULL && (pindex->nStatus & BLOCK_VALID_MASK) < BLOCK_VALID_SCRIPTS) pindexFirstNotScriptsValid = pindex;

        // Begin: actual consistency checks.
        if (pindex->pprev == NULL) {
            // Genesis block checks.
            assert(pindex->GetBlockHashLegacy() == consensusParams.hashGenesisBlock); // Genesis block's hash must match.
            assert(pindex == chainActive.Genesis()); // The current active chain's genesis block must be this block.
        }
        if (pindex->nChainTx == 0) assert(pindex->nSequenceId <= 0);  // nSequenceId can't be set positive for blocks that aren't linked (negative is used for preciousblock)
        // VALID_TRANSACTIONS is equivalent to nTx > 0 for all nodes (whether or not pruning has occurred).
        // HAVE_DATA is only equivalent to nTx > 0 (or VALID_TRANSACTIONS) if no pruning has occurred.
        if (!fHavePruned) {
            // If we've never pruned, then HAVE_DATA should be equivalent to nTx > 0
            assert(!(pindex->nStatus & BLOCK_HAVE_DATA) == (pindex->nTx == 0));
            assert(pindexFirstMissing == pindexFirstNeverProcessed);
        } else {
            // If we have pruned, then we can only say that HAVE_DATA implies nTx > 0
            if (pindex->nStatus & BLOCK_HAVE_DATA) assert(pindex->nTx > 0);
        }
        if (pindex->nStatus & BLOCK_HAVE_UNDO) assert(pindex->nStatus & BLOCK_HAVE_DATA);
        assert(((pindex->nStatus & BLOCK_VALID_MASK) >= BLOCK_VALID_TRANSACTIONS) == (pindex->nTx > 0)); // This is pruning-independent.
        // All parents having had data (at some point) is equivalent to all parents being VALID_TRANSACTIONS, which is equivalent to nChainTx being set.
        assert((pindexFirstNeverProcessed != NULL) == (pindex->nChainTx == 0)); // nChainTx != 0 is used to signal that all parent blocks have been processed (but may have been pruned).
        assert((pindexFirstNotTransactionsValid != NULL) == (pindex->nChainTx == 0));
        assert(pindex->nHeight == nHeight); // nHeight must be consistent.
        assert(pindex->pprev == NULL || pindex->nChainWork >= pindex->pprev->nChainWork); // For every block except the genesis block, the chainwork must be larger than the parent's.
        assert(nHeight < 2 || (pindex->pskip && (pindex->pskip->nHeight < nHeight))); // The pskip pointer must point back for all but the first 2 blocks.
        assert(pindexFirstNotTreeValid == NULL); // All mapBlockIndex entries must at least be TREE valid
        if ((pindex->nStatus & BLOCK_VALID_MASK) >= BLOCK_VALID_TREE) assert(pindexFirstNotTreeValid == NULL); // TREE valid implies all parents are TREE valid
        if ((pindex->nStatus & BLOCK_VALID_MASK) >= BLOCK_VALID_CHAIN) assert(pindexFirstNotChainValid == NULL); // CHAIN valid implies all parents are CHAIN valid
        if ((pindex->nStatus & BLOCK_VALID_MASK) >= BLOCK_VALID_SCRIPTS) assert(pindexFirstNotScriptsValid == NULL); // SCRIPTS valid implies all parents are SCRIPTS valid
        if (pindexFirstInvalid == NULL) {
            // Checks for not-invalid blocks.
            assert((pindex->nStatus & BLOCK_FAILED_MASK) == 0); // The failed mask cannot be set for blocks without invalid parents.
        }
        if (!CBlockIndexWorkComparator()(pindex, chainActive.Tip()) && pindexFirstNeverProcessed == NULL) {
            if (pindexFirstInvalid == NULL) {
                // If this block sorts at least as good as the current tip and
                // is valid and we have all data for its parents, it must be in
                // setBlockIndexCandidates.  chainActive.Tip() must also be there
                // even if some data has been pruned.
                if (pindexFirstMissing == NULL || pindex == chainActive.Tip()) {
                    assert(setBlockIndexCandidates.count(pindex));
                }
                // If some parent is missing, then it could be that this block was in
                // setBlockIndexCandidates but had to be removed because of the missing data.
                // In this case it must be in mapBlocksUnlinked -- see test below.
            }
        } else { // If this block sorts worse than the current tip or some ancestor's block has never been seen, it cannot be in setBlockIndexCandidates.
            assert(setBlockIndexCandidates.count(pindex) == 0);
        }
        // Check whether this block is in mapBlocksUnlinked.
        std::pair<std::multimap<CBlockIndex*,CBlockIndex*>::iterator,std::multimap<CBlockIndex*,CBlockIndex*>::iterator> rangeUnlinked = mapBlocksUnlinked.equal_range(pindex->pprev);
        bool foundInUnlinked = false;
        while (rangeUnlinked.first != rangeUnlinked.second) {
            assert(rangeUnlinked.first->first == pindex->pprev);
            if (rangeUnlinked.first->second == pindex) {
                foundInUnlinked = true;
                break;
            }
            rangeUnlinked.first++;
        }
        if (pindex->pprev && (pindex->nStatus & BLOCK_HAVE_DATA) && pindexFirstNeverProcessed != NULL && pindexFirstInvalid == NULL) {
            // If this block has block data available, some parent was never received, and has no invalid parents, it must be in mapBlocksUnlinked.
            assert(foundInUnlinked);
        }
        if (!(pindex->nStatus & BLOCK_HAVE_DATA)) assert(!foundInUnlinked); // Can't be in mapBlocksUnlinked if we don't HAVE_DATA
        if (pindexFirstMissing == NULL) assert(!foundInUnlinked); // We aren't missing data for any parent -- cannot be in mapBlocksUnlinked.
        if (pindex->pprev && (pindex->nStatus & BLOCK_HAVE_DATA) && pindexFirstNeverProcessed == NULL && pindexFirstMissing != NULL) {
            // We HAVE_DATA for this block, have received data for all parents at some point, but we're currently missing data for some parent.
            assert(fHavePruned); // We must have pruned.
            // This block may have entered mapBlocksUnlinked if:
            //  - it has a descendant that at some point had more work than the
            //    tip, and
            //  - we tried switching to that descendant but were missing
            //    data for some intermediate block between chainActive and the
            //    tip.
            // So if this block is itself better than chainActive.Tip() and it wasn't in
            // setBlockIndexCandidates, then it must be in mapBlocksUnlinked.
            if (!CBlockIndexWorkComparator()(pindex, chainActive.Tip()) && setBlockIndexCandidates.count(pindex) == 0) {
                if (pindexFirstInvalid == NULL) {
                    assert(foundInUnlinked);
                }
            }
        }
        // assert(pindex->GetBlockHash() == pindex->GetBlockHeader().GetHash()); // Perhaps too slow
        // End: actual consistency checks.

        // Try descending into the first subnode.
        std::pair<std::multimap<CBlockIndex*,CBlockIndex*>::iterator,std::multimap<CBlockIndex*,CBlockIndex*>::iterator> range = forward.equal_range(pindex);
        if (range.first != range.second) {
            // A subnode was found.
            pindex = range.first->second;
            nHeight++;
            continue;
        }
        // This is a leaf node.
        // Move upwards until we reach a node of which we have not yet visited the last child.
        while (pindex) {
            // We are going to either move to a parent or a sibling of pindex.
            // If pindex was the first with a certain property, unset the corresponding variable.
            if (pindex == pindexFirstInvalid) pindexFirstInvalid = NULL;
            if (pindex == pindexFirstMissing) pindexFirstMissing = NULL;
            if (pindex == pindexFirstNeverProcessed) pindexFirstNeverProcessed = NULL;
            if (pindex == pindexFirstNotTreeValid) pindexFirstNotTreeValid = NULL;
            if (pindex == pindexFirstNotTransactionsValid) pindexFirstNotTransactionsValid = NULL;
            if (pindex == pindexFirstNotChainValid) pindexFirstNotChainValid = NULL;
            if (pindex == pindexFirstNotScriptsValid) pindexFirstNotScriptsValid = NULL;
            // Find our parent.
            CBlockIndex* pindexPar = pindex->pprev;
            // Find which child we just visited.
            std::pair<std::multimap<CBlockIndex*,CBlockIndex*>::iterator,std::multimap<CBlockIndex*,CBlockIndex*>::iterator> rangePar = forward.equal_range(pindexPar);
            while (rangePar.first->second != pindex) {
                assert(rangePar.first != rangePar.second); // Our parent must have at least the node we're coming from as child.
                rangePar.first++;
            }
            // Proceed to the next one.
            rangePar.first++;
            if (rangePar.first != rangePar.second) {
                // Move to the sibling.
                pindex = rangePar.first->second;
                break;
            } else {
                // Move up further.
                pindex = pindexPar;
                nHeight--;
                continue;
            }
        }
    }

    // Check that we actually traversed the entire map.
    assert(nNodes == forward.size());
}

std::string CBlockFileInfo::ToString() const
{
    return strprintf("CBlockFileInfo(blocks=%u, size=%u, heights=%u...%u, time=%s...%s)", nBlocks, nSize, nHeightFirst, nHeightLast, DateTimeStrFormat("%Y-%m-%d", nTimeFirst), DateTimeStrFormat("%Y-%m-%d", nTimeLast));
}

CBlockFileInfo* GetBlockFileInfo(size_t n)
{
    return &vinfoBlockFile.at(n);
}

void SetFullSyncMode(bool state) {
    fFullSyncMode = state;
}

bool isFullSyncMode() {
    return fFullSyncMode;
}

bool StartPartialHeaders(int64_t time, const std::function<void(const CBlockIndex*)>& notifyCallback)
{
    // To ensure that context checks can be done on headers from *time* onwards, a window of
    // at least 576 headers is required. So the youngest block that is before the requested time
    // should have at least 576 headers before it.
    const CBlockIndex* youngestBefore = partialChain.FindYoungest(time, [](int64_t before, const CBlockIndex* index){
        return index->GetBlockTime() < before;
    });

    if (    IsPartialSyncActive()
         && youngestBefore && youngestBefore->nHeight - partialChain.HeightOffset() > 576)
    {
        LogPrintf("Partial sync continues, height offset = %d.\n", partialChain.HeightOffset());
    }
    else {
        if (IsPartialSyncActive()) // IsPartialSyncActive() => above checks for time and/or 576 window failed
        {
            LogPrintf("Partial sync in progress but starting point is too young for requested start. Sync reset.\n");
            pindexBestPartial = nullptr;
            partialChain.SetTip(nullptr);
            partialChain.SetHeightOffset(0);
        }

        // Search checkpoint with at least a 576 block height difference to the checkpoint that is the youngest before
        // the desired starting time. This way when we reach the latter checkpoint there will be enough data to do context checks
        // the largest windows for context checking is the DELTA algorithm, which needs 576 blocks.
        CheckPointEntry entry;
        int youngestHeight = Checkpoints::LastCheckpointAt(time, entry);
        int olderHeight = youngestHeight;
        while (olderHeight > 0 && youngestHeight - olderHeight <= 576)
        {
            olderHeight = Checkpoints::LastCheckpointAt(entry.nTime - 1, entry);
        }
        // if no suitable checkpoint was found use Genesis
        if (olderHeight < 0)
        {
            olderHeight = Checkpoints::LastCheckpointAt(Params().GenesisBlock().GetBlockTime(), entry);
            assert(olderHeight == 0);
        }

        if (olderHeight - 576 >= chainActive.Height() || !isFullSyncMode())
        {
            // inititalize partial sync
            partialChain.SetHeightOffset(olderHeight);
            CBlockIndex* index = InsertBlockIndex(entry.hash);
            index->nHeight = olderHeight;
            index->RaisePartialValidity(BLOCK_PARTIAL_TREE);
            partialChain.SetTip(index);
            pindexBestPartial = index;

            setDirtyBlockIndex.insert(index);

            LogPrintf("Partial headers started from built-in checkpoint with height=%d\n", olderHeight);

            // from now IsPartialSyncActive() == true, as the offset is set and the partial chain has at least one entry
        }
        else
        {
            LogPrintf("Partial headers NOT started started as full chain is ahead of required offset, height=%d offset=%d\n", chainActive.Height(), olderHeight);
            return false;
        }
    }

    headerTipSignal.connect(notifyCallback);
    return true;
}

void SetMaxSPVPruneHeight(int height)
{
    nMaxSPVPruneHeight = height;
}

class CMainCleanup
{
public:
    CMainCleanup() {}
    ~CMainCleanup() {
        // block headers
        BlockMap::iterator it1 = mapBlockIndex.begin();
        for (; it1 != mapBlockIndex.end(); it1++)
            delete (*it1).second;
        mapBlockIndex.clear();
    }
} instance_of_cmaincleanup;
