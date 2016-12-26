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

#include "main.h"

#include "addrman.h"
#include "alert.h"
#include "arith_uint256.h"
#include "blockencodings.h"
#include "chainparams.h"
#include "checkpoints.h"
#include "Gulden/auto_checkpoints.h"
#include "checkqueue.h"
#include "consensus/consensus.h"
#include "consensus/merkle.h"
#include "consensus/validation.h"
#include "hash.h"
#include "init.h"
#include "merkleblock.h"
#include "net.h"
#include "policy/fees.h"
#include "policy/policy.h"
#include "pow.h"
#include <Gulden/Common/diff.h>
#include "primitives/block.h"
#include "primitives/transaction.h"
#include "random.h"
#include "script/script.h"
#include "script/sigcache.h"
#include "script/standard.h"
#include "tinyformat.h"
#include "txdb.h"
#include "txmempool.h"
#include "ui_interface.h"
#include "undo.h"
#include "util.h"
#include "utilmoneystr.h"
#include "utilstrencodings.h"
#include "validationinterface.h"
#include "versionbits.h"
#ifdef ENABLE_WALLET
#include "wallet/wallet.h"
#endif

#include <atomic>
#include <sstream>

#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/math/distributions/poisson.hpp>
#include <boost/thread.hpp>

using namespace std;

#if defined(NDEBUG)
#error "Gulden cannot be compiled without assertions."
#endif

/**
 * Global state
 */

CCriticalSection cs_main;

BlockMap mapBlockIndex;
CChain chainActive;
CBlockIndex* pindexBestHeader = NULL;
int64_t nTimeBestReceived = 0;
CWaitableCriticalSection csBestBlock;
CConditionVariable cvBlockChange;
int nScriptCheckThreads = 0;
bool fImporting = false;
bool fReindex = false;
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

CFeeRate minRelayTxFee = CFeeRate(DEFAULT_MIN_RELAY_TX_FEE);
CAmount maxTxFee = DEFAULT_TRANSACTION_MAXFEE;

CTxMemPool mempool(::minRelayTxFee);
FeeFilterRounder filterRounder(::minRelayTxFee);

struct IteratorComparator {
    template <typename I>
    bool operator()(const I& a, const I& b)
    {
        return &(*a) < &(*b);
    }
};

struct COrphanTx {
    CTransaction tx;
    NodeId fromPeer;
    int64_t nTimeExpire;
};
map<uint256, COrphanTx> mapOrphanTransactions GUARDED_BY(cs_main);
map<COutPoint, set<map<uint256, COrphanTx>::iterator, IteratorComparator> > mapOrphanTransactionsByPrev GUARDED_BY(cs_main);
void EraseOrphansFor(NodeId peer) EXCLUSIVE_LOCKS_REQUIRED(cs_main);

/**
 * Returns true if there are nRequired or more blocks of minVersion or above
 * in the last Consensus::Params::nMajorityWindow blocks, starting at pstart and going backwards.
 */
static bool IsSuperMajority(int minVersion, const CBlockIndex* pstart, unsigned nRequired, const Consensus::Params& consensusParams);
static void CheckBlockIndex(const Consensus::Params& consensusParams);

/** Constant stuff for coinbase transactions we create: */
CScript COINBASE_FLAGS;

int64_t nMinimumInputValue = DUST_HARD_LIMIT;

const string strMessageMagic = "Guldencoin Signed Message:\n";

namespace {

struct CBlockIndexWorkComparator {
    bool operator()(CBlockIndex* pa, CBlockIndex* pb) const
    {

        if (pa->nChainWork > pb->nChainWork)
            return false;
        if (pa->nChainWork < pb->nChainWork)
            return true;

        if (pa->nSequenceId < pb->nSequenceId)
            return false;
        if (pa->nSequenceId > pb->nSequenceId)
            return true;

        if (pa < pb)
            return false;
        if (pa > pb)
            return true;

        return false;
    }
};

CBlockIndex* pindexBestInvalid;

/**
     * The set of all CBlockIndex entries with BLOCK_VALID_TRANSACTIONS (for itself and all ancestors) and
     * as good as our current tip or better. Entries may be failed, though, and pruning nodes may be
     * missing the data for the block.
     */
set<CBlockIndex*, CBlockIndexWorkComparator> setBlockIndexCandidates;
/** Number of nodes with fSyncStarted. */
int nSyncStarted = 0;
/** All pairs A->B, where A (or one of its ancestors) misses transactions, but B has transactions.
     * Pruned nodes may have entries where B is missing data.
     */
multimap<CBlockIndex*, CBlockIndex*> mapBlocksUnlinked;

CCriticalSection cs_LastBlockFile;
std::vector<CBlockFileInfo> vinfoBlockFile;
int nLastBlockFile = 0;
/** Global flag to indicate we should check to see if there are
     *  block/undo files that should be deleted.  Set on startup
     *  or if we allocate more file space when we're in prune mode
     */
bool fCheckForPruning = false;

/**
     * Every received block is assigned a unique and increasing identifier, so we
     * know which one to give priority in case of a fork.
     */
CCriticalSection cs_nBlockSequenceId;
/** Blocks loaded from disk are assigned id 0, so start the counter at 1. */
uint32_t nBlockSequenceId = 1;

/**
     * Sources of received blocks, saved to be able to send them reject
     * messages or ban them when processing happens afterwards. Protected by
     * cs_main.
     */
map<uint256, NodeId> mapBlockSource;

/**
     * Filter for transactions that were recently rejected by
     * AcceptToMemoryPool. These are not rerequested until the chain tip
     * changes, at which point the entire filter is reset. Protected by
     * cs_main.
     *
     * Without this filter we'd be re-requesting txs from each of our peers,
     * increasing bandwidth consumption considerably. For instance, with 100
     * peers, half of which relay a tx we don't accept, that might be a 50x
     * bandwidth increase. A flooding attacker attempting to roll-over the
     * filter using minimum-sized, 60byte, transactions might manage to send
     * 1000/sec if we have fast peers, so we pick 120,000 to give our peers a
     * two minute window to send invs to us.
     *
     * Decreasing the false positive rate is fairly cheap, so we pick one in a
     * million to make it highly unlikely for users to have issues with this
     * filter.
     *
     * Memory used: 1.3 MB
     */
boost::scoped_ptr<CRollingBloomFilter> recentRejects;
uint256 hashRecentRejectsChainTip;

/** Blocks that are in flight, and that are in the queue to be downloaded. Protected by cs_main. */
struct QueuedBlock {
    uint256 hash;
    CBlockIndex* pindex; //!< Optional.
    bool fValidatedHeaders; //!< Whether this block has validated headers at the time of request.
    std::unique_ptr<PartiallyDownloadedBlock> partialBlock; //!< Optional, used for CMPCTBLOCK downloads
};
map<uint256, pair<NodeId, list<QueuedBlock>::iterator> > mapBlocksInFlight;

/** Stack of nodes which we have set to announce using compact blocks */
list<NodeId> lNodesAnnouncingHeaderAndIDs;

/** Number of preferable block download peers. */
int nPreferredDownload = 0;

/** Dirty block index entries. */
set<CBlockIndex*> setDirtyBlockIndex;

/** Dirty block file entries. */
set<int> setDirtyFileInfo;

/** Number of peers from which we're downloading blocks. */
int nPeersWithValidatedDownloads = 0;

/** Relay map, protected by cs_main. */
typedef std::map<uint256, std::shared_ptr<const CTransaction> > MapRelay;
MapRelay mapRelay;
/** Expiration-time ordered list of (expire time, relay map entry) pairs, protected by cs_main). */
std::deque<std::pair<int64_t, MapRelay::iterator> > vRelayExpiration;
} // anon namespace

namespace {

struct CBlockReject {
    unsigned char chRejectCode;
    string strRejectReason;
    uint256 hashBlock;
};

/**
 * Maintain validation-specific state about nodes, protected by cs_main, instead
 * by CNode's own locks. This simplifies asynchronous operation, where
 * processing of incoming data is done after the ProcessMessage call returns,
 * and we're no longer holding the node's locks.
 */
struct CNodeState {

    CService address;

    bool fCurrentlyConnected;

    int nMisbehavior;

    bool fShouldBan;

    std::string name;

    std::vector<CBlockReject> rejects;

    CBlockIndex* pindexBestKnownBlock;

    uint256 hashLastUnknownBlock;

    CBlockIndex* pindexLastCommonBlock;

    CBlockIndex* pindexBestHeaderSent;

    int nUnconnectingHeaders;

    bool fSyncStarted;

    int64_t nStallingSince;
    list<QueuedBlock> vBlocksInFlight;

    int64_t nDownloadingSince;
    int nBlocksInFlight;
    int nBlocksInFlightValidHeaders;

    bool fPreferredDownload;

    bool fPreferHeaders;

    bool fPreferHeaderAndIDs;

    bool fProvidesHeaderAndIDs;

    bool fHaveWitness;

    CNodeState()
    {
        fCurrentlyConnected = false;
        nMisbehavior = 0;
        fShouldBan = false;
        pindexBestKnownBlock = NULL;
        hashLastUnknownBlock.SetNull();
        pindexLastCommonBlock = NULL;
        pindexBestHeaderSent = NULL;
        nUnconnectingHeaders = 0;
        fSyncStarted = false;
        nStallingSince = 0;
        nDownloadingSince = 0;
        nBlocksInFlight = 0;
        nBlocksInFlightValidHeaders = 0;
        fPreferredDownload = false;
        fPreferHeaders = false;
        fPreferHeaderAndIDs = false;
        fProvidesHeaderAndIDs = false;
        fHaveWitness = false;
    }
};

/** Map maintaining per-node state. Requires cs_main. */
map<NodeId, CNodeState> mapNodeState;

CNodeState* State(NodeId pnode)
{
    map<NodeId, CNodeState>::iterator it = mapNodeState.find(pnode);
    if (it == mapNodeState.end())
        return NULL;
    return &it->second;
}

int GetHeight()
{
    LOCK(cs_main);
    return chainActive.Height();
}

void UpdatePreferredDownload(CNode* node, CNodeState* state)
{
    nPreferredDownload -= state->fPreferredDownload;

    state->fPreferredDownload = (!node->fInbound || node->fWhitelisted) && !node->fOneShot && !node->fClient;

    nPreferredDownload += state->fPreferredDownload;
}

void InitializeNode(NodeId nodeid, const CNode* pnode)
{
    LOCK(cs_main);
    CNodeState& state = mapNodeState.insert(std::make_pair(nodeid, CNodeState())).first->second;
    state.name = pnode->addrName;
    state.address = pnode->addr;
}

void FinalizeNode(NodeId nodeid)
{
    LOCK(cs_main);
    CNodeState* state = State(nodeid);

    if (state->fSyncStarted)
        nSyncStarted--;

    if (state->nMisbehavior == 0 && state->fCurrentlyConnected) {
        AddressCurrentlyConnected(state->address);
    }

    BOOST_FOREACH (const QueuedBlock& entry, state->vBlocksInFlight) {
        mapBlocksInFlight.erase(entry.hash);
    }
    EraseOrphansFor(nodeid);
    nPreferredDownload -= state->fPreferredDownload;
    nPeersWithValidatedDownloads -= (state->nBlocksInFlightValidHeaders != 0);
    assert(nPeersWithValidatedDownloads >= 0);

    mapNodeState.erase(nodeid);

    if (mapNodeState.empty()) {

        assert(mapBlocksInFlight.empty());
        assert(nPreferredDownload == 0);
        assert(nPeersWithValidatedDownloads == 0);
    }
}

bool MarkBlockAsReceived(const uint256& hash)
{
    map<uint256, pair<NodeId, list<QueuedBlock>::iterator> >::iterator itInFlight = mapBlocksInFlight.find(hash);
    if (itInFlight != mapBlocksInFlight.end()) {
        CNodeState* state = State(itInFlight->second.first);
        state->nBlocksInFlightValidHeaders -= itInFlight->second.second->fValidatedHeaders;
        if (state->nBlocksInFlightValidHeaders == 0 && itInFlight->second.second->fValidatedHeaders) {

            nPeersWithValidatedDownloads--;
        }
        if (state->vBlocksInFlight.begin() == itInFlight->second.second) {

            state->nDownloadingSince = std::max(state->nDownloadingSince, GetTimeMicros());
        }
        state->vBlocksInFlight.erase(itInFlight->second.second);
        state->nBlocksInFlight--;
        state->nStallingSince = 0;
        mapBlocksInFlight.erase(itInFlight);
        return true;
    }
    return false;
}

bool MarkBlockAsInFlight(NodeId nodeid, const uint256& hash, const Consensus::Params& consensusParams, CBlockIndex* pindex = NULL, list<QueuedBlock>::iterator* *pit = NULL)
{
    CNodeState* state = State(nodeid);
    assert(state != NULL);

    map<uint256, pair<NodeId, list<QueuedBlock>::iterator> >::iterator itInFlight = mapBlocksInFlight.find(hash);
    if (itInFlight != mapBlocksInFlight.end() && itInFlight->second.first == nodeid) {
        *pit = &itInFlight->second.second;
        return false;
    }

    MarkBlockAsReceived(hash);

    list<QueuedBlock>::iterator it = state->vBlocksInFlight.insert(state->vBlocksInFlight.end(),
                                                                   { hash, pindex, pindex != NULL, std::unique_ptr<PartiallyDownloadedBlock>(pit ? new PartiallyDownloadedBlock(&mempool) : NULL) });
    state->nBlocksInFlight++;
    state->nBlocksInFlightValidHeaders += it->fValidatedHeaders;
    if (state->nBlocksInFlight == 1) {

        state->nDownloadingSince = GetTimeMicros();
    }
    if (state->nBlocksInFlightValidHeaders == 1 && pindex != NULL) {
        nPeersWithValidatedDownloads++;
    }
    itInFlight = mapBlocksInFlight.insert(std::make_pair(hash, std::make_pair(nodeid, it))).first;
    if (pit)
        *pit = &itInFlight->second.second;
    return true;
}

/** Check whether the last unknown block a peer advertised is not yet known. */
void ProcessBlockAvailability(NodeId nodeid)
{
    CNodeState* state = State(nodeid);
    assert(state != NULL);

    if (!state->hashLastUnknownBlock.IsNull()) {
        BlockMap::iterator itOld = mapBlockIndex.find(state->hashLastUnknownBlock);
        if (itOld != mapBlockIndex.end() && itOld->second->nChainWork > 0) {
            if (state->pindexBestKnownBlock == NULL || itOld->second->nChainWork >= state->pindexBestKnownBlock->nChainWork)
                state->pindexBestKnownBlock = itOld->second;
            state->hashLastUnknownBlock.SetNull();
        }
    }
}

/** Update tracking information about which blocks a peer is assumed to have. */
void UpdateBlockAvailability(NodeId nodeid, const uint256& hash)
{
    CNodeState* state = State(nodeid);
    assert(state != NULL);

    ProcessBlockAvailability(nodeid);

    BlockMap::iterator it = mapBlockIndex.find(hash);
    if (it != mapBlockIndex.end() && it->second->nChainWork > 0) {

        if (state->pindexBestKnownBlock == NULL || it->second->nChainWork >= state->pindexBestKnownBlock->nChainWork)
            state->pindexBestKnownBlock = it->second;
    } else {

        state->hashLastUnknownBlock = hash;
    }
}

void MaybeSetPeerAsAnnouncingHeaderAndIDs(const CNodeState* nodestate, CNode* pfrom)
{
    if (nLocalServices & NODE_WITNESS) {

        return;
    }
    if (nodestate->fProvidesHeaderAndIDs) {
        BOOST_FOREACH (const NodeId nodeid, lNodesAnnouncingHeaderAndIDs)
            if (nodeid == pfrom->GetId())
                return;
        bool fAnnounceUsingCMPCTBLOCK = false;
        uint64_t nCMPCTBLOCKVersion = 1;
        if (lNodesAnnouncingHeaderAndIDs.size() >= 3) {

            CNode* pnodeStop = FindNode(lNodesAnnouncingHeaderAndIDs.front());
            if (pnodeStop) {
                pnodeStop->PushMessage(NetMsgType::SENDCMPCT, fAnnounceUsingCMPCTBLOCK, nCMPCTBLOCKVersion);
                lNodesAnnouncingHeaderAndIDs.pop_front();
            }
        }
        fAnnounceUsingCMPCTBLOCK = true;
        pfrom->PushMessage(NetMsgType::SENDCMPCT, fAnnounceUsingCMPCTBLOCK, nCMPCTBLOCKVersion);
        lNodesAnnouncingHeaderAndIDs.push_back(pfrom->GetId());
    }
}

bool CanDirectFetch(const Consensus::Params& consensusParams)
{
    return chainActive.Tip()->GetBlockTime() > GetAdjustedTime() - consensusParams.nPowTargetSpacing * 20;
}

bool PeerHasHeader(CNodeState* state, CBlockIndex* pindex)
{
    if (state->pindexBestKnownBlock && pindex == state->pindexBestKnownBlock->GetAncestor(pindex->nHeight))
        return true;
    if (state->pindexBestHeaderSent && pindex == state->pindexBestHeaderSent->GetAncestor(pindex->nHeight))
        return true;
    return false;
}

/** Find the last common ancestor two blocks have.
 *  Both pa and pb must be non-NULL. */
CBlockIndex* LastCommonAncestor(CBlockIndex* pa, CBlockIndex* pb)
{
    if (pa->nHeight > pb->nHeight) {
        pa = pa->GetAncestor(pb->nHeight);
    } else if (pb->nHeight > pa->nHeight) {
        pb = pb->GetAncestor(pa->nHeight);
    }

    while (pa != pb && pa && pb) {
        pa = pa->pprev;
        pb = pb->pprev;
    }

    assert(pa == pb);
    return pa;
}

/** Update pindexLastCommonBlock and add not-in-flight missing successors to vBlocks, until it has
 *  at most count entries. */
void FindNextBlocksToDownload(NodeId nodeid, unsigned int count, std::vector<CBlockIndex*>& vBlocks, NodeId& nodeStaller, const Consensus::Params& consensusParams)
{
    if (count == 0)
        return;

    vBlocks.reserve(vBlocks.size() + count);
    CNodeState* state = State(nodeid);
    assert(state != NULL);

    ProcessBlockAvailability(nodeid);

    if (state->pindexBestKnownBlock == NULL || state->pindexBestKnownBlock->nChainWork < chainActive.Tip()->nChainWork) {

        return;
    }

    if (state->pindexLastCommonBlock == NULL) {

        state->pindexLastCommonBlock = chainActive[std::min(state->pindexBestKnownBlock->nHeight, chainActive.Height())];
    }

    state->pindexLastCommonBlock = LastCommonAncestor(state->pindexLastCommonBlock, state->pindexBestKnownBlock);
    if (state->pindexLastCommonBlock == state->pindexBestKnownBlock)
        return;

    std::vector<CBlockIndex*> vToFetch;
    CBlockIndex* pindexWalk = state->pindexLastCommonBlock;

    int nWindowEnd = state->pindexLastCommonBlock->nHeight + BLOCK_DOWNLOAD_WINDOW;
    int nMaxHeight = std::min<int>(state->pindexBestKnownBlock->nHeight, nWindowEnd + 1);
    NodeId waitingfor = -1;
    while (pindexWalk->nHeight < nMaxHeight) {

        int nToFetch = std::min(nMaxHeight - pindexWalk->nHeight, std::max<int>(count - vBlocks.size(), 128));
        vToFetch.resize(nToFetch);
        pindexWalk = state->pindexBestKnownBlock->GetAncestor(pindexWalk->nHeight + nToFetch);
        vToFetch[nToFetch - 1] = pindexWalk;
        for (unsigned int i = nToFetch - 1; i > 0; i--) {
            vToFetch[i - 1] = vToFetch[i]->pprev;
        }

        BOOST_FOREACH (CBlockIndex* pindex, vToFetch) {
            if (!pindex->IsValid(BLOCK_VALID_TREE)) {

                return;
            }
            if (!State(nodeid)->fHaveWitness && IsWitnessEnabled(pindex->pprev, consensusParams)) {

                return;
            }
            if (pindex->nStatus & BLOCK_HAVE_DATA || chainActive.Contains(pindex)) {
                if (pindex->nChainTx)
                    state->pindexLastCommonBlock = pindex;
            } else if (mapBlocksInFlight.count(pindex->GetBlockHash()) == 0) {

                if (pindex->nHeight > nWindowEnd) {

                    if (vBlocks.size() == 0 && waitingfor != nodeid) {

                        nodeStaller = waitingfor;
                    }
                    return;
                }
                vBlocks.push_back(pindex);
                if (vBlocks.size() == count) {
                    return;
                }
            } else if (waitingfor == -1) {

                waitingfor = mapBlocksInFlight[pindex->GetBlockHash()].first;
            }
        }
    }
}

} // anon namespace

bool GetNodeStateStats(NodeId nodeid, CNodeStateStats& stats)
{
    LOCK(cs_main);
    CNodeState* state = State(nodeid);
    if (state == NULL)
        return false;
    stats.nMisbehavior = state->nMisbehavior;
    stats.nSyncHeight = state->pindexBestKnownBlock ? state->pindexBestKnownBlock->nHeight : -1;
    stats.nCommonHeight = state->pindexLastCommonBlock ? state->pindexLastCommonBlock->nHeight : -1;
    BOOST_FOREACH (const QueuedBlock& queue, state->vBlocksInFlight) {
        if (queue.pindex)
            stats.vHeightInFlight.push_back(queue.pindex->nHeight);
    }
    return true;
}

void RegisterNodeSignals(CNodeSignals& nodeSignals)
{
    nodeSignals.GetHeight.connect(&GetHeight);
    nodeSignals.ProcessMessages.connect(&ProcessMessages);
    nodeSignals.SendMessages.connect(&SendMessages);
    nodeSignals.InitializeNode.connect(&InitializeNode);
    nodeSignals.FinalizeNode.connect(&FinalizeNode);
}

void UnregisterNodeSignals(CNodeSignals& nodeSignals)
{
    nodeSignals.GetHeight.disconnect(&GetHeight);
    nodeSignals.ProcessMessages.disconnect(&ProcessMessages);
    nodeSignals.SendMessages.disconnect(&SendMessages);
    nodeSignals.InitializeNode.disconnect(&InitializeNode);
    nodeSignals.FinalizeNode.disconnect(&FinalizeNode);
}

CBlockIndex* FindForkInGlobalIndex(const CChain& chain, const CBlockLocator& locator)
{

    BOOST_FOREACH (const uint256& hash, locator.vHave) {
        BlockMap::iterator mi = mapBlockIndex.find(hash);
        if (mi != mapBlockIndex.end()) {
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

CCoinsViewCache* pcoinsTip = NULL;
CBlockTreeDB* pblocktree = NULL;

bool AddOrphanTx(const CTransaction& tx, NodeId peer) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    uint256 hash = tx.GetHash();
    if (mapOrphanTransactions.count(hash))
        return false;

    unsigned int sz = GetTransactionWeight(tx);
    if (sz >= MAX_STANDARD_TX_WEIGHT) {
        LogPrint("mempool", "ignoring large orphan tx (size: %u, hash: %s)\n", sz, hash.ToString());
        return false;
    }

    auto ret = mapOrphanTransactions.emplace(hash, COrphanTx{ tx, peer, GetTime() + ORPHAN_TX_EXPIRE_TIME });
    assert(ret.second);
    BOOST_FOREACH (const CTxIn& txin, tx.vin) {
        mapOrphanTransactionsByPrev[txin.prevout].insert(ret.first);
    }

    LogPrint("mempool", "stored orphan tx %s (mapsz %u outsz %u)\n", hash.ToString(),
             mapOrphanTransactions.size(), mapOrphanTransactionsByPrev.size());
    return true;
}

int static EraseOrphanTx(uint256 hash) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    map<uint256, COrphanTx>::iterator it = mapOrphanTransactions.find(hash);
    if (it == mapOrphanTransactions.end())
        return 0;
    BOOST_FOREACH (const CTxIn& txin, it->second.tx.vin) {
        auto itPrev = mapOrphanTransactionsByPrev.find(txin.prevout);
        if (itPrev == mapOrphanTransactionsByPrev.end())
            continue;
        itPrev->second.erase(it);
        if (itPrev->second.empty())
            mapOrphanTransactionsByPrev.erase(itPrev);
    }
    mapOrphanTransactions.erase(it);
    return 1;
}

void EraseOrphansFor(NodeId peer)
{
    int nErased = 0;
    map<uint256, COrphanTx>::iterator iter = mapOrphanTransactions.begin();
    while (iter != mapOrphanTransactions.end()) {
        map<uint256, COrphanTx>::iterator maybeErase = iter++; // increment to avoid iterator becoming invalid
        if (maybeErase->second.fromPeer == peer) {
            nErased += EraseOrphanTx(maybeErase->second.tx.GetHash());
        }
    }
    if (nErased > 0)
        LogPrint("mempool", "Erased %d orphan tx from peer %d\n", nErased, peer);
}

unsigned int LimitOrphanTxSize(unsigned int nMaxOrphans) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    unsigned int nEvicted = 0;
    static int64_t nNextSweep;
    int64_t nNow = GetTime();
    if (nNextSweep <= nNow) {

        int nErased = 0;
        int64_t nMinExpTime = nNow + ORPHAN_TX_EXPIRE_TIME - ORPHAN_TX_EXPIRE_INTERVAL;
        map<uint256, COrphanTx>::iterator iter = mapOrphanTransactions.begin();
        while (iter != mapOrphanTransactions.end()) {
            map<uint256, COrphanTx>::iterator maybeErase = iter++;
            if (maybeErase->second.nTimeExpire <= nNow) {
                nErased += EraseOrphanTx(maybeErase->second.tx.GetHash());
            } else {
                nMinExpTime = std::min(maybeErase->second.nTimeExpire, nMinExpTime);
            }
        }

        nNextSweep = nMinExpTime + ORPHAN_TX_EXPIRE_INTERVAL;
        if (nErased > 0)
            LogPrint("mempool", "Erased %d orphan tx due to expiration\n", nErased);
    }
    while (mapOrphanTransactions.size() > nMaxOrphans) {

        uint256 randomhash = GetRandHash();
        map<uint256, COrphanTx>::iterator it = mapOrphanTransactions.lower_bound(randomhash);
        if (it == mapOrphanTransactions.end())
            it = mapOrphanTransactions.begin();
        EraseOrphanTx(it->first);
        ++nEvicted;
    }
    return nEvicted;
}

bool IsFinalTx(const CTransaction& tx, int nBlockHeight, int64_t nBlockTime)
{
    if (tx.nLockTime == 0)
        return true;
    if ((int64_t)tx.nLockTime < ((int64_t)tx.nLockTime < LOCKTIME_THRESHOLD ? (int64_t)nBlockHeight : nBlockTime))
        return true;
    BOOST_FOREACH (const CTxIn& txin, tx.vin) {
        if (!(txin.nSequence == CTxIn::SEQUENCE_FINAL))
            return false;
    }
    return true;
}

bool CheckFinalTx(const CTransaction& tx, int flags)
{
    AssertLockHeld(cs_main);

    flags = std::max(flags, 0);

    const int nBlockHeight = chainActive.Height() + 1;

    const int64_t nBlockTime = (flags & LOCKTIME_MEDIAN_TIME_PAST)
                                   ? chainActive.Tip()->GetMedianTimePast(chainActive.Height())
                                   : GetAdjustedTime();

    return IsFinalTx(tx, nBlockHeight, nBlockTime);
}

/**
 * Calculates the block height and previous block's median time past at
 * which the transaction will be considered final in the context of BIP 68.
 * Also removes from the vector of input heights any entries which did not
 * correspond to sequence locked inputs as they do not affect the calculation.
 */
static std::pair<int, int64_t> CalculateSequenceLocks(const CTransaction& tx, int flags, std::vector<int>* prevHeights, const CBlockIndex& block)
{
    assert(prevHeights->size() == tx.vin.size());

    int nMinHeight = -1;
    int64_t nMinTime = -1;

    bool fEnforceBIP68 = static_cast<uint32_t>(tx.nVersion) >= 2
                         && flags & LOCKTIME_VERIFY_SEQUENCE;

    if (!fEnforceBIP68) {
        return std::make_pair(nMinHeight, nMinTime);
    }

    for (size_t txinIndex = 0; txinIndex < tx.vin.size(); txinIndex++) {
        const CTxIn& txin = tx.vin[txinIndex];

        if (txin.nSequence & CTxIn::SEQUENCE_LOCKTIME_DISABLE_FLAG) {

            (*prevHeights)[txinIndex] = 0;
            continue;
        }

        int nCoinHeight = (*prevHeights)[txinIndex];

        if (txin.nSequence & CTxIn::SEQUENCE_LOCKTIME_TYPE_FLAG) {
            int64_t nCoinTime = block.GetAncestor(std::max(nCoinHeight - 1, 0))->GetMedianTimePast(block.GetAncestor(std::max(nCoinHeight - 1, 0))->nHeight);

            nMinTime = std::max(nMinTime, nCoinTime + (int64_t)((txin.nSequence & CTxIn::SEQUENCE_LOCKTIME_MASK) << CTxIn::SEQUENCE_LOCKTIME_GRANULARITY) - 1);
        } else {
            nMinHeight = std::max(nMinHeight, nCoinHeight + (int)(txin.nSequence & CTxIn::SEQUENCE_LOCKTIME_MASK) - 1);
        }
    }

    return std::make_pair(nMinHeight, nMinTime);
}

static bool EvaluateSequenceLocks(const CBlockIndex& block, std::pair<int, int64_t> lockPair)
{
    assert(block.pprev);
    int64_t nBlockTime = block.pprev->GetMedianTimePast(block.pprev->nHeight);
    if (lockPair.first >= block.nHeight || lockPair.second >= nBlockTime)
        return false;

    return true;
}

bool SequenceLocks(const CTransaction& tx, int flags, std::vector<int>* prevHeights, const CBlockIndex& block)
{
    return EvaluateSequenceLocks(block, CalculateSequenceLocks(tx, flags, prevHeights, block));
}

bool TestLockPointValidity(const LockPoints* lp)
{
    AssertLockHeld(cs_main);
    assert(lp);

    if (lp->maxInputBlock) {

        if (!chainActive.Contains(lp->maxInputBlock)) {
            return false;
        }
    }

    return true;
}

bool CheckSequenceLocks(const CTransaction& tx, int flags, LockPoints* lp, bool useExistingLockPoints)
{
    AssertLockHeld(cs_main);
    AssertLockHeld(mempool.cs);

    CBlockIndex* tip = chainActive.Tip();
    CBlockIndex index;
    index.pprev = tip;

    index.nHeight = tip->nHeight + 1;

    std::pair<int, int64_t> lockPair;
    if (useExistingLockPoints) {
        assert(lp);
        lockPair.first = lp->height;
        lockPair.second = lp->time;
    } else {

        CCoinsViewMemPool viewMemPool(pcoinsTip, mempool);
        std::vector<int> prevheights;
        prevheights.resize(tx.vin.size());
        for (size_t txinIndex = 0; txinIndex < tx.vin.size(); txinIndex++) {
            const CTxIn& txin = tx.vin[txinIndex];
            CCoins coins;
            if (!viewMemPool.GetCoins(txin.prevout.hash, coins)) {
                return error("%s: Missing input", __func__);
            }
            if (coins.nHeight == MEMPOOL_HEIGHT) {

                prevheights[txinIndex] = tip->nHeight + 1;
            } else {
                prevheights[txinIndex] = coins.nHeight;
            }
        }
        lockPair = CalculateSequenceLocks(tx, flags, &prevheights, index);
        if (lp) {
            lp->height = lockPair.first;
            lp->time = lockPair.second;

            int maxInputHeight = 0;
            BOOST_FOREACH (int height, prevheights) {

                if (height != tip->nHeight + 1) {
                    maxInputHeight = std::max(maxInputHeight, height);
                }
            }
            lp->maxInputBlock = tip->GetAncestor(maxInputHeight);
        }
    }
    return EvaluateSequenceLocks(index, lockPair);
}

unsigned int GetLegacySigOpCount(const CTransaction& tx)
{
    unsigned int nSigOps = 0;
    BOOST_FOREACH (const CTxIn& txin, tx.vin) {
        nSigOps += txin.scriptSig.GetSigOpCount(false);
    }
    BOOST_FOREACH (const CTxOut& txout, tx.vout) {
        nSigOps += txout.scriptPubKey.GetSigOpCount(false);
    }
    return nSigOps;
}

unsigned int GetP2SHSigOpCount(const CTransaction& tx, const CCoinsViewCache& inputs)
{
    if (tx.IsCoinBase())
        return 0;

    unsigned int nSigOps = 0;
    for (unsigned int i = 0; i < tx.vin.size(); i++) {
        const CTxOut& prevout = inputs.GetOutputFor(tx.vin[i]);
        if (prevout.scriptPubKey.IsPayToScriptHash())
            nSigOps += prevout.scriptPubKey.GetSigOpCount(tx.vin[i].scriptSig);
    }
    return nSigOps;
}

int64_t GetTransactionSigOpCost(const CTransaction& tx, const CCoinsViewCache& inputs, int flags)
{
    int64_t nSigOps = GetLegacySigOpCount(tx) * WITNESS_SCALE_FACTOR;

    if (tx.IsCoinBase())
        return nSigOps;

    if (flags & SCRIPT_VERIFY_P2SH) {
        nSigOps += GetP2SHSigOpCount(tx, inputs) * WITNESS_SCALE_FACTOR;
    }

    for (unsigned int i = 0; i < tx.vin.size(); i++) {
        const CTxOut& prevout = inputs.GetOutputFor(tx.vin[i]);
        nSigOps += CountWitnessSigOps(tx.vin[i].scriptSig, prevout.scriptPubKey, i < tx.wit.vtxinwit.size() ? &tx.wit.vtxinwit[i].scriptWitness : NULL, flags);
    }
    return nSigOps;
}

bool CheckTransaction(const CTransaction& tx, CValidationState& state)
{

    if (tx.vin.empty())
        return state.DoS(10, false, REJECT_INVALID, "bad-txns-vin-empty");
    if (tx.vout.empty())
        return state.DoS(10, false, REJECT_INVALID, "bad-txns-vout-empty");

    if (::GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION | SERIALIZE_TRANSACTION_NO_WITNESS) > MAX_BLOCK_BASE_SIZE)
        return state.DoS(100, false, REJECT_INVALID, "bad-txns-oversize");

    CAmount nValueOut = 0;
    BOOST_FOREACH (const CTxOut& txout, tx.vout) {
        if (txout.nValue < 0)
            return state.DoS(100, false, REJECT_INVALID, "bad-txns-vout-negative");
        if (txout.nValue > MAX_MONEY)
            return state.DoS(100, false, REJECT_INVALID, "bad-txns-vout-toolarge");
        nValueOut += txout.nValue;
        if (!MoneyRange(nValueOut))
            return state.DoS(100, false, REJECT_INVALID, "bad-txns-txouttotal-toolarge");
    }

    set<COutPoint> vInOutPoints;
    BOOST_FOREACH (const CTxIn& txin, tx.vin) {
        if (vInOutPoints.count(txin.prevout))
            return state.DoS(100, false, REJECT_INVALID, "bad-txns-inputs-duplicate");
        vInOutPoints.insert(txin.prevout);
    }

    if (tx.IsCoinBase()) {
        if (tx.vin[0].scriptSig.size() < 2 || tx.vin[0].scriptSig.size() > 100)
            return state.DoS(100, false, REJECT_INVALID, "bad-cb-length");
    } else {
        BOOST_FOREACH (const CTxIn& txin, tx.vin)
            if (txin.prevout.IsNull())
                return state.DoS(10, false, REJECT_INVALID, "bad-txns-prevout-null");
    }

    return true;
}

void LimitMempoolSize(CTxMemPool& pool, size_t limit, unsigned long age)
{
    int expired = pool.Expire(GetTime() - age);
    if (expired != 0)
        LogPrint("mempool", "Expired %i transactions from the memory pool\n", expired);

    std::vector<uint256> vNoSpendsRemaining;
    pool.TrimToSize(limit, &vNoSpendsRemaining);
    BOOST_FOREACH (const uint256& removed, vNoSpendsRemaining)
        pcoinsTip->Uncache(removed);
}

/** Convert CValidationState to a human-readable message for logging */
std::string FormatStateMessage(const CValidationState& state)
{
    return strprintf("%s%s (code %i)",
                     state.GetRejectReason(),
                     state.GetDebugMessage().empty() ? "" : ", " + state.GetDebugMessage(),
                     state.GetRejectCode());
}

bool AcceptToMemoryPoolWorker(CTxMemPool& pool, CValidationState& state, const CTransaction& tx, bool fLimitFree,
                              bool* pfMissingInputs, bool fOverrideMempoolLimit, const CAmount& nAbsurdFee,
                              std::vector<uint256>& vHashTxnToUncache)
{
    const uint256 hash = tx.GetHash();
    AssertLockHeld(cs_main);
    if (pfMissingInputs)
        *pfMissingInputs = false;

    if (!CheckTransaction(tx, state))
        return false; // state filled in by CheckTransaction

    if (tx.IsCoinBase())
        return state.DoS(100, false, REJECT_INVALID, "coinbase");

    const CChainParams& chainparams = Params();
    if (fRequireStandard && tx.nVersion >= 2 && VersionBitsTipState(chainparams.GetConsensus(), Consensus::DEPLOYMENT_CSV) != THRESHOLD_ACTIVE) {
        return state.DoS(0, false, REJECT_NONSTANDARD, "premature-version2-tx");
    }

    bool witnessEnabled = IsWitnessEnabled(chainActive.Tip(), Params().GetConsensus());
    if (!GetBoolArg("-prematurewitness", false) && !tx.wit.IsNull() && !witnessEnabled) {
        return state.DoS(0, false, REJECT_NONSTANDARD, "no-witness-yet", true);
    }

    string reason;
    if (fRequireStandard && !IsStandardTx(tx, reason, witnessEnabled))
        return state.DoS(0, false, REJECT_NONSTANDARD, reason);

    if (!CheckFinalTx(tx, STANDARD_LOCKTIME_VERIFY_FLAGS))
        return state.DoS(0, false, REJECT_NONSTANDARD, "non-final");

    if (pool.exists(hash))
        return state.Invalid(false, REJECT_ALREADY_KNOWN, "txn-already-in-mempool");

    set<uint256> setConflicts;
    {
        LOCK(pool.cs); // protect pool.mapNextTx
        BOOST_FOREACH (const CTxIn& txin, tx.vin) {
            auto itConflicting = pool.mapNextTx.find(txin.prevout);
            if (itConflicting != pool.mapNextTx.end()) {
                const CTransaction* ptxConflicting = itConflicting->second;
                if (!setConflicts.count(ptxConflicting->GetHash())) {

                    bool fReplacementOptOut = true;
                    if (fEnableReplacement) {
                        BOOST_FOREACH (const CTxIn& txin, ptxConflicting->vin) {
                            if (txin.nSequence < std::numeric_limits<unsigned int>::max() - 1) {
                                fReplacementOptOut = false;
                                break;
                            }
                        }
                    }
                    if (fReplacementOptOut)
                        return state.Invalid(false, REJECT_CONFLICT, "txn-mempool-conflict");

                    setConflicts.insert(ptxConflicting->GetHash());
                }
            }
        }
    }

    {
        CCoinsView dummy;
        CCoinsViewCache view(&dummy);

        CAmount nValueIn = 0;
        LockPoints lp;
        {
            LOCK(pool.cs);
            CCoinsViewMemPool viewMemPool(pcoinsTip, pool);
            view.SetBackend(viewMemPool);

            bool fHadTxInCache = pcoinsTip->HaveCoinsInCache(hash);
            if (view.HaveCoins(hash)) {
                if (!fHadTxInCache)
                    vHashTxnToUncache.push_back(hash);
                return state.Invalid(false, REJECT_ALREADY_KNOWN, "txn-already-known");
            }

            BOOST_FOREACH (const CTxIn txin, tx.vin) {
                if (!pcoinsTip->HaveCoinsInCache(txin.prevout.hash))
                    vHashTxnToUncache.push_back(txin.prevout.hash);
                if (!view.HaveCoins(txin.prevout.hash)) {
                    if (pfMissingInputs)
                        *pfMissingInputs = true;
                    return false; // fMissingInputs and !state.IsInvalid() is used to detect this condition, don't set state.Invalid()
                }
            }

            if (!view.HaveInputs(tx))
                return state.Invalid(false, REJECT_DUPLICATE, "bad-txns-inputs-spent");

            view.GetBestBlock();

            nValueIn = view.GetValueIn(tx);

            view.SetBackend(dummy);

            if (!CheckSequenceLocks(tx, STANDARD_LOCKTIME_VERIFY_FLAGS, &lp))
                return state.DoS(0, false, REJECT_NONSTANDARD, "non-BIP68-final");
        }

        if (fRequireStandard && !AreInputsStandard(tx, view))
            return state.Invalid(false, REJECT_NONSTANDARD, "bad-txns-nonstandard-inputs");

        int64_t nSigOpsCost = GetTransactionSigOpCost(tx, view, STANDARD_SCRIPT_VERIFY_FLAGS);

        CAmount nValueOut = tx.GetValueOut();
        CAmount nFees = nValueIn - nValueOut;

        CAmount nModifiedFees = nFees;
        double nPriorityDummy = 0;
        pool.ApplyDeltas(hash, nPriorityDummy, nModifiedFees);

        CAmount inChainInputValue;
        double dPriority = view.GetPriority(tx, chainActive.Height(), inChainInputValue);

        bool fSpendsCoinbase = false;
        BOOST_FOREACH (const CTxIn& txin, tx.vin) {
            const CCoins* coins = view.AccessCoins(txin.prevout.hash);
            if (coins->IsCoinBase()) {
                fSpendsCoinbase = true;
                break;
            }
        }

        CTxMemPoolEntry entry(tx, nFees, GetTime(), dPriority, chainActive.Height(), pool.HasNoInputsOf(tx), inChainInputValue, fSpendsCoinbase, nSigOpsCost, lp);
        unsigned int nSize = entry.GetTxSize();

        if (nSigOpsCost > MAX_STANDARD_TX_SIGOPS_COST)
            return state.DoS(0, false, REJECT_NONSTANDARD, "bad-txns-too-many-sigops", false,
                             strprintf("%d", nSigOpsCost));

        CAmount mempoolRejectFee = pool.GetMinFee(GetArg("-maxmempool", DEFAULT_MAX_MEMPOOL_SIZE) * 1000000).GetFee(nSize);
        if (mempoolRejectFee > 0 && nModifiedFees < mempoolRejectFee) {
            return state.DoS(0, false, REJECT_INSUFFICIENTFEE, "mempool min fee not met", false, strprintf("%d < %d", nFees, mempoolRejectFee));
        } else if (GetBoolArg("-relaypriority", DEFAULT_RELAYPRIORITY) && nModifiedFees < ::minRelayTxFee.GetFee(nSize) && !AllowFree(entry.GetPriority(chainActive.Height() + 1))) {

            return state.DoS(0, false, REJECT_INSUFFICIENTFEE, "insufficient priority");
        }

        if (fLimitFree && nModifiedFees < ::minRelayTxFee.GetFee(nSize)) {
            static CCriticalSection csFreeLimiter;
            static double dFreeCount;
            static int64_t nLastTime;
            int64_t nNow = GetTime();

            LOCK(csFreeLimiter);

            dFreeCount *= pow(1.0 - 1.0 / 600.0, (double)(nNow - nLastTime));
            nLastTime = nNow;

            if (dFreeCount + nSize >= GetArg("-limitfreerelay", DEFAULT_LIMITFREERELAY) * 10 * 1000)
                return state.DoS(0, false, REJECT_INSUFFICIENTFEE, "rate limited free transaction");
            LogPrint("mempool", "Rate limit dFreeCount: %g => %g\n", dFreeCount, dFreeCount + nSize);
            dFreeCount += nSize;
        }

        if (nAbsurdFee && nFees > nAbsurdFee)
            return state.Invalid(false,
                                 REJECT_HIGHFEE, "absurdly-high-fee",
                                 strprintf("%d > %d", nFees, nAbsurdFee));

        CTxMemPool::setEntries setAncestors;
        size_t nLimitAncestors = GetArg("-limitancestorcount", DEFAULT_ANCESTOR_LIMIT);
        size_t nLimitAncestorSize = GetArg("-limitancestorsize", DEFAULT_ANCESTOR_SIZE_LIMIT) * 1000;
        size_t nLimitDescendants = GetArg("-limitdescendantcount", DEFAULT_DESCENDANT_LIMIT);
        size_t nLimitDescendantSize = GetArg("-limitdescendantsize", DEFAULT_DESCENDANT_SIZE_LIMIT) * 1000;
        std::string errString;
        if (!pool.CalculateMemPoolAncestors(entry, setAncestors, nLimitAncestors, nLimitAncestorSize, nLimitDescendants, nLimitDescendantSize, errString)) {
            return state.DoS(0, false, REJECT_NONSTANDARD, "too-long-mempool-chain", false, errString);
        }

        BOOST_FOREACH (CTxMemPool::txiter ancestorIt, setAncestors) {
            const uint256& hashAncestor = ancestorIt->GetTx().GetHash();
            if (setConflicts.count(hashAncestor)) {
                return state.DoS(10, false,
                                 REJECT_INVALID, "bad-txns-spends-conflicting-tx", false,
                                 strprintf("%s spends conflicting transaction %s",
                                           hash.ToString(),
                                           hashAncestor.ToString()));
            }
        }

        CAmount nConflictingFees = 0;
        size_t nConflictingSize = 0;
        uint64_t nConflictingCount = 0;
        CTxMemPool::setEntries allConflicting;

        LOCK(pool.cs);
        if (setConflicts.size()) {
            CFeeRate newFeeRate(nModifiedFees, nSize);
            set<uint256> setConflictsParents;
            const int maxDescendantsToVisit = 100;
            CTxMemPool::setEntries setIterConflicting;
            BOOST_FOREACH (const uint256& hashConflicting, setConflicts) {
                CTxMemPool::txiter mi = pool.mapTx.find(hashConflicting);
                if (mi == pool.mapTx.end())
                    continue;

                setIterConflicting.insert(mi);

                CFeeRate oldFeeRate(mi->GetModifiedFee(), mi->GetTxSize());
                if (newFeeRate <= oldFeeRate) {
                    return state.DoS(0, false,
                                     REJECT_INSUFFICIENTFEE, "insufficient fee", false,
                                     strprintf("rejecting replacement %s; new feerate %s <= old feerate %s",
                                               hash.ToString(),
                                               newFeeRate.ToString(),
                                               oldFeeRate.ToString()));
                }

                BOOST_FOREACH (const CTxIn& txin, mi->GetTx().vin) {
                    setConflictsParents.insert(txin.prevout.hash);
                }

                nConflictingCount += mi->GetCountWithDescendants();
            }

            if (nConflictingCount <= maxDescendantsToVisit) {

                BOOST_FOREACH (CTxMemPool::txiter it, setIterConflicting) {
                    pool.CalculateDescendants(it, allConflicting);
                }
                BOOST_FOREACH (CTxMemPool::txiter it, allConflicting) {
                    nConflictingFees += it->GetModifiedFee();
                    nConflictingSize += it->GetTxSize();
                }
            } else {
                return state.DoS(0, false,
                                 REJECT_NONSTANDARD, "too many potential replacements", false,
                                 strprintf("rejecting replacement %s; too many potential replacements (%d > %d)\n",
                                           hash.ToString(),
                                           nConflictingCount,
                                           maxDescendantsToVisit));
            }

            for (unsigned int j = 0; j < tx.vin.size(); j++) {

                if (!setConflictsParents.count(tx.vin[j].prevout.hash)) {

                    if (pool.mapTx.find(tx.vin[j].prevout.hash) != pool.mapTx.end())
                        return state.DoS(0, false,
                                         REJECT_NONSTANDARD, "replacement-adds-unconfirmed", false,
                                         strprintf("replacement %s adds unconfirmed input, idx %d",
                                                   hash.ToString(), j));
                }
            }

            if (nModifiedFees < nConflictingFees) {
                return state.DoS(0, false,
                                 REJECT_INSUFFICIENTFEE, "insufficient fee", false,
                                 strprintf("rejecting replacement %s, less fees than conflicting txs; %s < %s",
                                           hash.ToString(), FormatMoney(nModifiedFees), FormatMoney(nConflictingFees)));
            }

            CAmount nDeltaFees = nModifiedFees - nConflictingFees;
            if (nDeltaFees < ::minRelayTxFee.GetFee(nSize)) {
                return state.DoS(0, false,
                                 REJECT_INSUFFICIENTFEE, "insufficient fee", false,
                                 strprintf("rejecting replacement %s, not enough additional fees to relay; %s < %s",
                                           hash.ToString(),
                                           FormatMoney(nDeltaFees),
                                           FormatMoney(::minRelayTxFee.GetFee(nSize))));
            }
        }

        unsigned int scriptVerifyFlags = STANDARD_SCRIPT_VERIFY_FLAGS;
        if (!Params().RequireStandard()) {
            scriptVerifyFlags = GetArg("-promiscuousmempoolflags", scriptVerifyFlags);
        }

        PrecomputedTransactionData txdata(tx);
        if (!CheckInputs(tx, state, view, true, scriptVerifyFlags, true, txdata)) {

            if (CheckInputs(tx, state, view, true, scriptVerifyFlags & ~(SCRIPT_VERIFY_WITNESS | SCRIPT_VERIFY_CLEANSTACK), true, txdata) && !CheckInputs(tx, state, view, true, scriptVerifyFlags & ~SCRIPT_VERIFY_CLEANSTACK, true, txdata)) {

                state.SetCorruptionPossible();
            }
            return false;
        }

        if (!CheckInputs(tx, state, view, true, MANDATORY_SCRIPT_VERIFY_FLAGS, true, txdata)) {
            return error("%s: BUG! PLEASE REPORT THIS! ConnectInputs failed against MANDATORY but not STANDARD flags %s, %s",
                         __func__, hash.ToString(), FormatStateMessage(state));
        }

        BOOST_FOREACH (const CTxMemPool::txiter it, allConflicting) {
            LogPrint("mempool", "replacing tx %s with %s for %s BTC additional fees, %d delta bytes\n",
                     it->GetTx().GetHash().ToString(),
                     hash.ToString(),
                     FormatMoney(nModifiedFees - nConflictingFees),
                     (int)nSize - (int)nConflictingSize);
        }
        pool.RemoveStaged(allConflicting, false);

        pool.addUnchecked(hash, entry, setAncestors, !IsInitialBlockDownload());

        if (!fOverrideMempoolLimit) {
            LimitMempoolSize(pool, GetArg("-maxmempool", DEFAULT_MAX_MEMPOOL_SIZE) * 1000000, GetArg("-mempoolexpiry", DEFAULT_MEMPOOL_EXPIRY) * 60 * 60);
            if (!pool.exists(hash))
                return state.DoS(0, false, REJECT_INSUFFICIENTFEE, "mempool full");
        }
    }

    SyncWithWallets(tx, NULL, NULL);

    return true;
}

bool AcceptToMemoryPool(CTxMemPool& pool, CValidationState& state, const CTransaction& tx, bool fLimitFree,
                        bool* pfMissingInputs, bool fOverrideMempoolLimit, const CAmount nAbsurdFee)
{
    std::vector<uint256> vHashTxToUncache;
    bool res = AcceptToMemoryPoolWorker(pool, state, tx, fLimitFree, pfMissingInputs, fOverrideMempoolLimit, nAbsurdFee, vHashTxToUncache);
    if (!res) {
        BOOST_FOREACH (const uint256& hashTx, vHashTxToUncache)
            pcoinsTip->Uncache(hashTx);
    }
    return res;
}

/** Return transaction in tx, and if it was found inside a block, its hash is placed in hashBlock */
bool GetTransaction(const uint256& hash, CTransaction& txOut, const Consensus::Params& consensusParams, uint256& hashBlock, bool fAllowSlow)
{
    CBlockIndex* pindexSlow = NULL;

    LOCK(cs_main);

    std::shared_ptr<const CTransaction> ptx = mempool.get(hash);
    if (ptx) {
        txOut = *ptx;
        return true;
    }

    if (fTxIndex) {
        CDiskTxPos postx;
        if (pblocktree->ReadTxIndex(hash, postx)) {
            CAutoFile file(OpenBlockFile(postx, true), SER_DISK, CLIENT_VERSION);
            if (file.IsNull())
                return error("%s: OpenBlockFile failed", __func__);
            CBlockHeader header;
            try {
                file >> header;
                fseek(file.Get(), postx.nTxOffset, SEEK_CUR);
                file >> txOut;
            }
            catch (const std::exception& e) {
                return error("%s: Deserialize or I/O error - %s", __func__, e.what());
            }
            hashBlock = header.GetHash();
            if (txOut.GetHash() != hash)
                return error("%s: txid mismatch", __func__);
            return true;
        }
    }

    if (fAllowSlow) { // use coin database to locate block that contains transaction, and scan it
        int nHeight = -1;
        {
            const CCoinsViewCache& view = *pcoinsTip;
            const CCoins* coins = view.AccessCoins(hash);
            if (coins)
                nHeight = coins->nHeight;
        }
        if (nHeight > 0)
            pindexSlow = chainActive[nHeight];
    }

    if (pindexSlow) {
        CBlock block;
        if (ReadBlockFromDisk(block, pindexSlow, consensusParams)) {
            BOOST_FOREACH (const CTransaction& tx, block.vtx) {
                if (tx.GetHash() == hash) {
                    txOut = tx;
                    hashBlock = pindexSlow->GetBlockHash();
                    return true;
                }
            }
        }
    }

    return false;
}

bool WriteBlockToDisk(const CBlock& block, CDiskBlockPos& pos, const CMessageHeader::MessageStartChars& messageStart)
{

    CAutoFile fileout(OpenBlockFile(pos), SER_DISK, CLIENT_VERSION);
    if (fileout.IsNull())
        return error("WriteBlockToDisk: OpenBlockFile failed");

    unsigned int nSize = fileout.GetSerializeSize(block);
    fileout << FLATDATA(messageStart) << nSize;

    long fileOutPos = ftell(fileout.Get());
    if (fileOutPos < 0)
        return error("WriteBlockToDisk: ftell failed");
    pos.nPos = (unsigned int)fileOutPos;
    fileout << block;

    return true;
}

bool ReadBlockFromDisk(CBlock& block, const CDiskBlockPos& pos, const Consensus::Params& consensusParams)
{
    block.SetNull();

    CAutoFile filein(OpenBlockFile(pos, true), SER_DISK, CLIENT_VERSION);
    if (filein.IsNull())
        return error("ReadBlockFromDisk: OpenBlockFile failed for %s", pos.ToString());

    try {
        filein >> block;
    }
    catch (const std::exception& e) {
        return error("%s: Deserialize or I/O error - %s at %s", __func__, e.what(), pos.ToString());
    }

    if (!CheckProofOfWork(block.GetPoWHash(), block.nBits, consensusParams))
        return error("ReadBlockFromDisk: Errors in block header at %s", pos.ToString());

    return true;
}

bool ReadBlockFromDisk(CBlock& block, const CBlockIndex* pindex, const Consensus::Params& consensusParams)
{
    if (!ReadBlockFromDisk(block, pindex->GetBlockPos(), consensusParams))
        return false;
    if (block.GetHash() != pindex->GetBlockHash())
        return error("ReadBlockFromDisk(CBlock&, CBlockIndex*): GetHash() doesn't match index for %s at %s",
                     pindex->ToString(), pindex->GetBlockPos().ToString());
    return true;
}

CAmount GetBlockSubsidy(int nHeight, const Consensus::Params& consensusParams)
{
    CAmount nSubsidy = 0;
    if (nHeight == 1) //First block premine
    {
        nSubsidy = 170000000 * COIN;
    } else if (nHeight <= 250000) // 1000 Gulden per block for first 250k blocks
    {
        nSubsidy = 1000 * COIN;
    } else if (nHeight <= 12850000) // Switch to fixed reward of 100 Gulden per block (no halving) - to continue until original coin target is met.
    {
        nSubsidy = 100 * COIN;
    }

    return nSubsidy;
}

bool IsInitialBlockDownload()
{
    const CChainParams& chainParams = Params();

    static std::atomic<bool> latchToFalse{ false };

    if (latchToFalse.load(std::memory_order_relaxed))
        return false;

    LOCK(cs_main);
    if (latchToFalse.load(std::memory_order_relaxed))
        return false;
    if (fImporting || fReindex)
        return true;
    if (fCheckpointsEnabled && chainActive.Height() < Checkpoints::GetTotalBlocksEstimate(chainParams.Checkpoints()))
        return true;
    bool state = (chainActive.Height() < pindexBestHeader->nHeight - 24 * 6 || std::max(chainActive.Tip()->GetBlockTime(), pindexBestHeader->GetBlockTime()) < GetTime() - nMaxTipAge);
    if (!state)
        latchToFalse.store(true, std::memory_order_relaxed);
    return state;
}

bool fLargeWorkForkFound = false;
bool fLargeWorkInvalidChainFound = false;
CBlockIndex* pindexBestForkTip = NULL, *pindexBestForkBase = NULL;
#if 0
static void AlertNotify(const std::string& strMessage)
{
    uiInterface.NotifyAlertChanged();
    std::string strCmd = GetArg("-alertnotify", "");
    if (strCmd.empty()) return;




    std::string singleQuote("'");
    std::string safeStatus = SanitizeString(strMessage);
    safeStatus = singleQuote+safeStatus+singleQuote;
    boost::replace_all(strCmd, "%s", safeStatus);

    boost::thread t(runCommand, strCmd); // thread runs free
}
#endif
void CheckForkWarningConditions()
{
    AssertLockHeld(cs_main);

    if (IsInitialBlockDownload())
        return;

    if (pindexBestForkTip && chainActive.Height() - pindexBestForkTip->nHeight >= 72)
        pindexBestForkTip = NULL;

    if (pindexBestForkTip || (pindexBestInvalid && pindexBestInvalid->nChainWork > chainActive.Tip()->nChainWork + (GetBlockProof(*chainActive.Tip()) * 6))) {
        if (!fLargeWorkForkFound && pindexBestForkBase) {
            std::string warning = std::string("'Warning: Large-work fork detected, forking after block ") + pindexBestForkBase->phashBlock->ToString() + std::string("'");
            CAlert::Notify(warning, true);
        }
        if (pindexBestForkTip && pindexBestForkBase) {
            LogPrintf("%s: Warning: Large valid fork found\n  forking the chain at height %d (%s)\n  lasting to height %d (%s).\nChain state database corruption likely.\n", __func__,
                      pindexBestForkBase->nHeight, pindexBestForkBase->phashBlock->ToString(),
                      pindexBestForkTip->nHeight, pindexBestForkTip->phashBlock->ToString());
            fLargeWorkForkFound = true;
        } else {
            LogPrintf("%s: Warning: Found invalid chain at least ~6 blocks longer than our best chain.\nChain state database corruption likely.\n", __func__);
            fLargeWorkInvalidChainFound = true;
        }
    } else {
        fLargeWorkForkFound = false;
        fLargeWorkInvalidChainFound = false;
    }
}

void CheckForkWarningConditionsOnNewFork(CBlockIndex* pindexNewForkTip)
{
    AssertLockHeld(cs_main);

    CBlockIndex* pfork = pindexNewForkTip;
    CBlockIndex* plonger = chainActive.Tip();
    while (pfork && pfork != plonger) {
        while (plonger && plonger->nHeight > pfork->nHeight)
            plonger = plonger->pprev;
        if (pfork == plonger)
            break;
        pfork = pfork->pprev;
    }

    if (pfork && (!pindexBestForkTip || (pindexBestForkTip && pindexNewForkTip->nHeight > pindexBestForkTip->nHeight)) && pindexNewForkTip->nChainWork - pfork->nChainWork > (GetBlockProof(*pfork) * 7) && chainActive.Height() - pindexNewForkTip->nHeight < 72) {
        pindexBestForkTip = pindexNewForkTip;
        pindexBestForkBase = pfork;
    }

    CheckForkWarningConditions();
}

void Misbehaving(NodeId pnode, int howmuch)
{
    if (howmuch == 0)
        return;

    CNodeState* state = State(pnode);
    if (state == NULL)
        return;

    state->nMisbehavior += howmuch;
    int banscore = GetArg("-banscore", DEFAULT_BANSCORE_THRESHOLD);
    if (state->nMisbehavior >= banscore && state->nMisbehavior - howmuch < banscore) {
        LogPrintf("%s: %s (%d -> %d) BAN THRESHOLD EXCEEDED\n", __func__, state->name, state->nMisbehavior - howmuch, state->nMisbehavior);
        state->fShouldBan = true;
    } else
        LogPrintf("%s: %s (%d -> %d)\n", __func__, state->name, state->nMisbehavior - howmuch, state->nMisbehavior);
}

void static InvalidChainFound(CBlockIndex* pindexNew)
{
    if (!pindexBestInvalid || pindexNew->nChainWork > pindexBestInvalid->nChainWork)
        pindexBestInvalid = pindexNew;

    LogPrintf("%s: invalid block=%s  height=%d  log2_work=%.8g  date=%s\n", __func__,
              pindexNew->GetBlockHash().ToString(), pindexNew->nHeight,
              log(pindexNew->nChainWork.getdouble()) / log(2.0), DateTimeStrFormat("%Y-%m-%d %H:%M:%S",
                                                                                   pindexNew->GetBlockTime()));
    CBlockIndex* tip = chainActive.Tip();
    assert(tip);
    LogPrintf("%s:  current best=%s  height=%d  log2_work=%.8g  date=%s\n", __func__,
              tip->GetBlockHash().ToString(), chainActive.Height(), log(tip->nChainWork.getdouble()) / log(2.0),
              DateTimeStrFormat("%Y-%m-%d %H:%M:%S", tip->GetBlockTime()));
    CheckForkWarningConditions();
}

void static InvalidBlockFound(CBlockIndex* pindex, const CValidationState& state)
{
    int nDoS = 0;
    if (state.IsInvalid(nDoS)) {
        std::map<uint256, NodeId>::iterator it = mapBlockSource.find(pindex->GetBlockHash());
        if (it != mapBlockSource.end() && State(it->second)) {
            assert(state.GetRejectCode() < REJECT_INTERNAL); // Blocks are never rejected with internal reject codes
            CBlockReject reject = {(unsigned char)state.GetRejectCode(), state.GetRejectReason().substr(0, MAX_REJECT_MESSAGE_LENGTH), pindex->GetBlockHash() };
            State(it->second)->rejects.push_back(reject);
            if (nDoS > 0)
                Misbehaving(it->second, nDoS);
        }
    }
    if (!state.CorruptionPossible()) {
        pindex->nStatus |= BLOCK_FAILED_VALID;
        setDirtyBlockIndex.insert(pindex);
        setBlockIndexCandidates.erase(pindex);
        InvalidChainFound(pindex);
    }
}

void UpdateCoins(const CTransaction& tx, CCoinsViewCache& inputs, CTxUndo& txundo, int nHeight)
{

    if (!tx.IsCoinBase()) {
        txundo.vprevout.reserve(tx.vin.size());
        BOOST_FOREACH (const CTxIn& txin, tx.vin) {
            CCoinsModifier coins = inputs.ModifyCoins(txin.prevout.hash);
            unsigned nPos = txin.prevout.n;

            if (nPos >= coins->vout.size() || coins->vout[nPos].IsNull())
                assert(false);

            txundo.vprevout.push_back(CTxInUndo(coins->vout[nPos]));
            coins->Spend(nPos);
            if (coins->vout.size() == 0) {
                CTxInUndo& undo = txundo.vprevout.back();
                undo.nHeight = coins->nHeight;
                undo.fCoinBase = coins->fCoinBase;
                undo.nVersion = coins->nVersion;
            }
        }
    }

    inputs.ModifyNewCoins(tx.GetHash(), tx.IsCoinBase())->FromTx(tx, nHeight);
}

void UpdateCoins(const CTransaction& tx, CCoinsViewCache& inputs, int nHeight)
{
    CTxUndo txundo;
    UpdateCoins(tx, inputs, txundo, nHeight);
}

bool CScriptCheck::operator()()
{
    const CScript& scriptSig = ptxTo->vin[nIn].scriptSig;
    const CScriptWitness* witness = (nIn < ptxTo->wit.vtxinwit.size()) ? &ptxTo->wit.vtxinwit[nIn].scriptWitness : NULL;
    if (!VerifyScript(scriptSig, scriptPubKey, witness, nFlags, CachingTransactionSignatureChecker(ptxTo, nIn, amount, cacheStore, *txdata), &error)) {
        return false;
    }
    return true;
}

int GetSpendHeight(const CCoinsViewCache& inputs)
{
    LOCK(cs_main);
    CBlockIndex* pindexPrev = mapBlockIndex.find(inputs.GetBestBlock())->second;
    return pindexPrev->nHeight + 1;
}

namespace Consensus {
bool CheckTxInputs(const CTransaction& tx, CValidationState& state, const CCoinsViewCache& inputs, int nSpendHeight)
{

    if (!inputs.HaveInputs(tx))
        return state.Invalid(false, 0, "", "Inputs unavailable");

    CAmount nValueIn = 0;
    CAmount nFees = 0;
    for (unsigned int i = 0; i < tx.vin.size(); i++) {
        const COutPoint& prevout = tx.vin[i].prevout;
        const CCoins* coins = inputs.AccessCoins(prevout.hash);
        assert(coins);

        if (coins->IsCoinBase()) {
            if (nSpendHeight - coins->nHeight < COINBASE_MATURITY)
                return state.Invalid(false,
                                     REJECT_INVALID, "bad-txns-premature-spend-of-coinbase",
                                     strprintf("tried to spend coinbase at depth %d", nSpendHeight - coins->nHeight));
        }

        nValueIn += coins->vout[prevout.n].nValue;
        if (!MoneyRange(coins->vout[prevout.n].nValue) || !MoneyRange(nValueIn))
            return state.DoS(100, false, REJECT_INVALID, "bad-txns-inputvalues-outofrange");
    }

    if (nValueIn < tx.GetValueOut())
        return state.DoS(100, false, REJECT_INVALID, "bad-txns-in-belowout", false,
                         strprintf("value in (%s) < value out (%s)", FormatMoney(nValueIn), FormatMoney(tx.GetValueOut())));

    CAmount nTxFee = nValueIn - tx.GetValueOut();
    if (nTxFee < 0)
        return state.DoS(100, false, REJECT_INVALID, "bad-txns-fee-negative");
    nFees += nTxFee;
    if (!MoneyRange(nFees))
        return state.DoS(100, false, REJECT_INVALID, "bad-txns-fee-outofrange");
    return true;
}
} // namespace Consensus

bool CheckInputs(const CTransaction& tx, CValidationState& state, const CCoinsViewCache& inputs, bool fScriptChecks, unsigned int flags, bool cacheStore, PrecomputedTransactionData& txdata, std::vector<CScriptCheck>* pvChecks)
{
    if (!tx.IsCoinBase()) {
        if (!Consensus::CheckTxInputs(tx, state, inputs, GetSpendHeight(inputs)))
            return false;

        if (pvChecks)
            pvChecks->reserve(tx.vin.size());

        if (fScriptChecks) {
            for (unsigned int i = 0; i < tx.vin.size(); i++) {
                const COutPoint& prevout = tx.vin[i].prevout;
                const CCoins* coins = inputs.AccessCoins(prevout.hash);
                assert(coins);

                CScriptCheck check(*coins, tx, i, flags, cacheStore, &txdata);
                if (pvChecks) {
                    pvChecks->push_back(CScriptCheck());
                    check.swap(pvChecks->back());
                } else if (!check()) {
                    if (flags & STANDARD_NOT_MANDATORY_VERIFY_FLAGS) {

                        CScriptCheck check2(*coins, tx, i,
                                            flags & ~STANDARD_NOT_MANDATORY_VERIFY_FLAGS, cacheStore, &txdata);
                        if (check2())
                            return state.Invalid(false, REJECT_NONSTANDARD, strprintf("non-mandatory-script-verify-flag (%s)", ScriptErrorString(check.GetScriptError())));
                    }

                    return state.DoS(100, false, REJECT_INVALID, strprintf("mandatory-script-verify-flag-failed (%s)", ScriptErrorString(check.GetScriptError())));
                }
            }
        }
    }

    return true;
}

namespace {

bool UndoWriteToDisk(const CBlockUndo& blockundo, CDiskBlockPos& pos, const uint256& hashBlock, const CMessageHeader::MessageStartChars& messageStart)
{

    CAutoFile fileout(OpenUndoFile(pos), SER_DISK, CLIENT_VERSION);
    if (fileout.IsNull())
        return error("%s: OpenUndoFile failed", __func__);

    unsigned int nSize = fileout.GetSerializeSize(blockundo);
    fileout << FLATDATA(messageStart) << nSize;

    long fileOutPos = ftell(fileout.Get());
    if (fileOutPos < 0)
        return error("%s: ftell failed", __func__);
    pos.nPos = (unsigned int)fileOutPos;
    fileout << blockundo;

    CHashWriter hasher(SER_GETHASH, PROTOCOL_VERSION);
    hasher << hashBlock;
    hasher << blockundo;
    fileout << hasher.GetHash();

    return true;
}

bool UndoReadFromDisk(CBlockUndo& blockundo, const CDiskBlockPos& pos, const uint256& hashBlock)
{

    CAutoFile filein(OpenUndoFile(pos, true), SER_DISK, CLIENT_VERSION);
    if (filein.IsNull())
        return error("%s: OpenUndoFile failed", __func__);

    uint256 hashChecksum;
    try {
        filein >> blockundo;
        filein >> hashChecksum;
    }
    catch (const std::exception& e) {
        return error("%s: Deserialize or I/O error - %s", __func__, e.what());
    }

    CHashWriter hasher(SER_GETHASH, PROTOCOL_VERSION);
    hasher << hashBlock;
    hasher << blockundo;
    if (hashChecksum != hasher.GetHash())
        return error("%s: Checksum mismatch", __func__);

    return true;
}

/** Abort with a message */
bool AbortNode(const std::string& strMessage, const std::string& userMessage = "")
{
    strMiscWarning = strMessage;
    LogPrintf("*** %s\n", strMessage);
    uiInterface.ThreadSafeMessageBox(
        userMessage.empty() ? _("Error: A fatal internal error occurred, see debug.log for details") : userMessage,
        "", CClientUIInterface::MSG_ERROR);
    StartShutdown();
    return false;
}

bool AbortNode(CValidationState& state, const std::string& strMessage, const std::string& userMessage = "")
{
    AbortNode(strMessage, userMessage);
    return state.Error(strMessage);
}

} // anon namespace

/**
 * Apply the undo operation of a CTxInUndo to the given chain state.
 * @param undo The undo object.
 * @param view The coins view to which to apply the changes.
 * @param out The out point that corresponds to the tx input.
 * @return True on success.
 */
static bool ApplyTxInUndo(const CTxInUndo& undo, CCoinsViewCache& view, const COutPoint& out)
{
    bool fClean = true;

    CCoinsModifier coins = view.ModifyCoins(out.hash);
    if (undo.nHeight != 0) {

        if (!coins->IsPruned())
            fClean = fClean && error("%s: undo data overwriting existing transaction", __func__);
        coins->Clear();
        coins->fCoinBase = undo.fCoinBase;
        coins->nHeight = undo.nHeight;
        coins->nVersion = undo.nVersion;
    } else {
        if (coins->IsPruned())
            fClean = fClean && error("%s: undo data adding output to missing transaction", __func__);
    }
    if (coins->IsAvailable(out.n))
        fClean = fClean && error("%s: undo data overwriting existing output", __func__);
    if (coins->vout.size() < out.n + 1)
        coins->vout.resize(out.n + 1);
    coins->vout[out.n] = undo.txout;

    return fClean;
}

bool DisconnectBlock(const CBlock& block, CValidationState& state, const CBlockIndex* pindex, CCoinsViewCache& view, bool* pfClean)
{
    assert(pindex->GetBlockHash() == view.GetBestBlock());

    if (pfClean)
        *pfClean = false;

    bool fClean = true;

    CBlockUndo blockUndo;
    CDiskBlockPos pos = pindex->GetUndoPos();
    if (pos.IsNull())
        return error("DisconnectBlock(): no undo data available");
    if (!UndoReadFromDisk(blockUndo, pos, pindex->pprev->GetBlockHash()))
        return error("DisconnectBlock(): failure reading undo data");

    if (blockUndo.vtxundo.size() + 1 != block.vtx.size())
        return error("DisconnectBlock(): block and undo data inconsistent");

    for (int i = block.vtx.size() - 1; i >= 0; i--) {
        const CTransaction& tx = block.vtx[i];
        uint256 hash = tx.GetHash();

        {
            CCoinsModifier outs = view.ModifyCoins(hash);
            outs->ClearUnspendable();

            CCoins outsBlock(tx, pindex->nHeight);

            if (outsBlock.nVersion < 0)
                outs->nVersion = outsBlock.nVersion;
            if (*outs != outsBlock)
                fClean = fClean && error("DisconnectBlock(): added transaction mismatch? database corrupted");

            outs->Clear();
        }

        if (i > 0) { // not coinbases
            const CTxUndo& txundo = blockUndo.vtxundo[i - 1];
            if (txundo.vprevout.size() != tx.vin.size())
                return error("DisconnectBlock(): transaction and undo data inconsistent");
            for (unsigned int j = tx.vin.size(); j-- > 0;) {
                const COutPoint& out = tx.vin[j].prevout;
                const CTxInUndo& undo = txundo.vprevout[j];
                if (!ApplyTxInUndo(undo, view, out))
                    fClean = false;
            }
        }
    }

    view.SetBestBlock(pindex->pprev->GetBlockHash());

    if (pfClean) {
        *pfClean = fClean;
        return true;
    }

    return fClean;
}

void static FlushBlockFile(bool fFinalize = false)
{
    LOCK(cs_LastBlockFile);

    CDiskBlockPos posOld(nLastBlockFile, 0);

    FILE* fileOld = OpenBlockFile(posOld);
    if (fileOld) {
        if (fFinalize)
            TruncateFile(fileOld, vinfoBlockFile[nLastBlockFile].nSize);
        FileCommit(fileOld);
        fclose(fileOld);
    }

    fileOld = OpenUndoFile(posOld);
    if (fileOld) {
        if (fFinalize)
            TruncateFile(fileOld, vinfoBlockFile[nLastBlockFile].nUndoSize);
        FileCommit(fileOld);
        fclose(fileOld);
    }
}

bool FindUndoPos(CValidationState& state, int nFile, CDiskBlockPos& pos, unsigned int nAddSize);

static CCheckQueue<CScriptCheck> scriptcheckqueue(128);

void ThreadScriptCheck()
{
    RenameThread("Gulden-scriptch");
    scriptcheckqueue.Thread();
}

VersionBitsCache versionbitscache;

int32_t ComputeBlockVersion(const CBlockIndex* pindexPrev, const Consensus::Params& params)
{
    LOCK(cs_main);
    int32_t nVersion = VERSIONBITS_TOP_BITS;

    for (int i = 0; i < (int)Consensus::MAX_VERSION_BITS_DEPLOYMENTS; i++) {
        ThresholdState state = VersionBitsState(pindexPrev, params, (Consensus::DeploymentPos)i, versionbitscache);
        if (state == THRESHOLD_LOCKED_IN || state == THRESHOLD_STARTED) {
            nVersion |= VersionBitsMask(params, (Consensus::DeploymentPos)i);
        }
    }

    return nVersion;
}

/**
 * Threshold condition checker that triggers when unknown versionbits are seen on the network.
 */
class WarningBitsConditionChecker : public AbstractThresholdConditionChecker {
private:
    int bit;

public:
    WarningBitsConditionChecker(int bitIn)
        : bit(bitIn)
    {
    }

    int64_t BeginTime(const Consensus::Params& params) const { return 0; }
    int64_t EndTime(const Consensus::Params& params) const { return std::numeric_limits<int64_t>::max(); }
    int Period(const Consensus::Params& params) const { return params.nMinerConfirmationWindow; }
    int Threshold(const Consensus::Params& params) const { return params.nRuleChangeActivationThreshold; }

    bool Condition(const CBlockIndex* pindex, const Consensus::Params& params) const
    {
        return ((pindex->nVersion & VERSIONBITS_TOP_MASK) == VERSIONBITS_TOP_BITS) && ((pindex->nVersion >> bit) & 1) != 0 && ((ComputeBlockVersion(pindex->pprev, params) >> bit) & 1) == 0;
    }
};

static ThresholdConditionCache warningcache[VERSIONBITS_NUM_BITS];

static int64_t nTimeCheck = 0;
static int64_t nTimeForks = 0;
static int64_t nTimeVerify = 0;
static int64_t nTimeConnect = 0;
static int64_t nTimeIndex = 0;
static int64_t nTimeCallbacks = 0;
static int64_t nTimeTotal = 0;

bool ConnectBlock(const CBlock& block, CValidationState& state, CBlockIndex* pindex,
                  CCoinsViewCache& view, const CChainParams& chainparams, bool fJustCheck)
{
    AssertLockHeld(cs_main);

    int64_t nTimeStart = GetTimeMicros();

    if (!CheckBlock(block, state, chainparams.GetConsensus(), !fJustCheck, !fJustCheck))
        return error("%s: Consensus::CheckBlock: %s", __func__, FormatStateMessage(state));

    uint256 hashPrevBlock = pindex->pprev == NULL ? uint256() : pindex->pprev->GetBlockHash();
    assert(hashPrevBlock == view.GetBestBlock());

    if (block.GetHash() == chainparams.GetConsensus().hashGenesisBlock) {
        if (!fJustCheck)
            view.SetBestBlock(pindex->GetBlockHash());
        return true;
    }

    bool fScriptChecks = true;
    if (fCheckpointsEnabled) {
        CBlockIndex* pindexLastCheckpoint = Checkpoints::GetLastCheckpoint(chainparams.Checkpoints());
        if (pindexLastCheckpoint && pindexLastCheckpoint->GetAncestor(pindex->nHeight) == pindex) {

            fScriptChecks = false;
        }
    }

    int64_t nTime1 = GetTimeMicros();
    nTimeCheck += nTime1 - nTimeStart;
    LogPrint("bench", "    - Sanity checks: %.2fms [%.2fs]\n", 0.001 * (nTime1 - nTimeStart), nTimeCheck * 0.000001);

    bool fEnforceBIP30 = true;

    CBlockIndex* pindexBIP34height = pindex->pprev->GetAncestor(chainparams.GetConsensus().BIP34Height);

    fEnforceBIP30 = fEnforceBIP30 && (!pindexBIP34height || !(pindexBIP34height->GetBlockHash() == chainparams.GetConsensus().BIP34Hash));

    if (fEnforceBIP30) {
        BOOST_FOREACH (const CTransaction& tx, block.vtx) {
            const CCoins* coins = view.AccessCoins(tx.GetHash());
            if (coins && !coins->IsPruned())
                return state.DoS(100, error("ConnectBlock(): tried to overwrite transaction"),
                                 REJECT_INVALID, "bad-txns-BIP30");
        }
    }

    int64_t nBIP16SwitchTime = 1349049600;
    bool fStrictPayToScriptHash = (pindex->GetBlockTime() >= nBIP16SwitchTime);

    unsigned int flags = fStrictPayToScriptHash ? SCRIPT_VERIFY_P2SH : SCRIPT_VERIFY_NONE;

    if (block.nVersion >= 3 && IsSuperMajority(3, pindex->pprev, chainparams.GetConsensus().nMajorityEnforceBlockUpgrade, chainparams.GetConsensus())) {
        flags |= SCRIPT_VERIFY_DERSIG;
    }

    if (block.nVersion >= 4 && IsSuperMajority(4, pindex->pprev, chainparams.GetConsensus().nMajorityEnforceBlockUpgrade, chainparams.GetConsensus())) {
        flags |= SCRIPT_VERIFY_CHECKLOCKTIMEVERIFY;
    }

    int nLockTimeFlags = 0;
    if (VersionBitsState(pindex->pprev, chainparams.GetConsensus(), Consensus::DEPLOYMENT_CSV, versionbitscache) == THRESHOLD_ACTIVE) {
        flags |= SCRIPT_VERIFY_CHECKSEQUENCEVERIFY;
        nLockTimeFlags |= LOCKTIME_VERIFY_SEQUENCE;
    }

    if (IsWitnessEnabled(pindex->pprev, chainparams.GetConsensus())) {
        flags |= SCRIPT_VERIFY_WITNESS;
    }

    int64_t nTime2 = GetTimeMicros();
    nTimeForks += nTime2 - nTime1;
    LogPrint("bench", "    - Fork checks: %.2fms [%.2fs]\n", 0.001 * (nTime2 - nTime1), nTimeForks * 0.000001);

    CBlockUndo blockundo;

    CCheckQueueControl<CScriptCheck> control(fScriptChecks && nScriptCheckThreads ? &scriptcheckqueue : NULL);

    std::vector<uint256> vOrphanErase;
    std::vector<int> prevheights;
    CAmount nFees = 0;
    int nInputs = 0;
    int64_t nSigOpsCost = 0;
    CDiskTxPos pos(pindex->GetBlockPos(), GetSizeOfCompactSize(block.vtx.size()));
    std::vector<std::pair<uint256, CDiskTxPos> > vPos;
    vPos.reserve(block.vtx.size());
    blockundo.vtxundo.reserve(block.vtx.size() - 1);
    std::vector<PrecomputedTransactionData> txdata;
    txdata.reserve(block.vtx.size()); // Required so that pointers to individual PrecomputedTransactionData don't get invalidated
    for (unsigned int i = 0; i < block.vtx.size(); i++) {
        const CTransaction& tx = block.vtx[i];

        nInputs += tx.vin.size();

        if (!tx.IsCoinBase()) {
            if (!view.HaveInputs(tx))
                return state.DoS(100, error("ConnectBlock(): inputs missing/spent"),
                                 REJECT_INVALID, "bad-txns-inputs-missingorspent");

            prevheights.resize(tx.vin.size());
            for (size_t j = 0; j < tx.vin.size(); j++) {
                prevheights[j] = view.AccessCoins(tx.vin[j].prevout.hash)->nHeight;
            }

            for (size_t j = 0; j < tx.vin.size(); j++) {
                auto itByPrev = mapOrphanTransactionsByPrev.find(tx.vin[j].prevout);
                if (itByPrev == mapOrphanTransactionsByPrev.end())
                    continue;
                for (auto mi = itByPrev->second.begin(); mi != itByPrev->second.end(); ++mi) {
                    const CTransaction& orphanTx = (*mi)->second.tx;
                    const uint256& orphanHash = orphanTx.GetHash();
                    vOrphanErase.push_back(orphanHash);
                }
            }

            if (!SequenceLocks(tx, nLockTimeFlags, &prevheights, *pindex)) {
                return state.DoS(100, error("%s: contains a non-BIP68-final transaction", __func__),
                                 REJECT_INVALID, "bad-txns-nonfinal");
            }
        }

        nSigOpsCost += GetTransactionSigOpCost(tx, view, flags);
        if (nSigOpsCost > MAX_BLOCK_SIGOPS_COST)
            return state.DoS(100, error("ConnectBlock(): too many sigops"),
                             REJECT_INVALID, "bad-blk-sigops");

        txdata.emplace_back(tx);
        if (!tx.IsCoinBase()) {
            nFees += view.GetValueIn(tx) - tx.GetValueOut();

            std::vector<CScriptCheck> vChecks;
            bool fCacheResults = fJustCheck; /* Don't cache results if we're actually connecting blocks (still consult the cache, though) */
            if (!CheckInputs(tx, state, view, fScriptChecks, flags, fCacheResults, txdata[i], nScriptCheckThreads ? &vChecks : NULL))
                return error("ConnectBlock(): CheckInputs on %s failed with %s",
                             tx.GetHash().ToString(), FormatStateMessage(state));
            control.Add(vChecks);
        }

        CTxUndo undoDummy;
        if (i > 0) {
            blockundo.vtxundo.push_back(CTxUndo());
        }
        UpdateCoins(tx, view, i == 0 ? undoDummy : blockundo.vtxundo.back(), pindex->nHeight);

        vPos.push_back(std::make_pair(tx.GetHash(), pos));
        pos.nTxOffset += ::GetSerializeSize(tx, SER_DISK, CLIENT_VERSION);
    }
    int64_t nTime3 = GetTimeMicros();
    nTimeConnect += nTime3 - nTime2;
    LogPrint("bench", "      - Connect %u transactions: %.2fms (%.3fms/tx, %.3fms/txin) [%.2fs]\n", (unsigned)block.vtx.size(), 0.001 * (nTime3 - nTime2), 0.001 * (nTime3 - nTime2) / block.vtx.size(), nInputs <= 1 ? 0 : 0.001 * (nTime3 - nTime2) / (nInputs - 1), nTimeConnect * 0.000001);

    CAmount blockReward = nFees + GetBlockSubsidy(pindex->nHeight, chainparams.GetConsensus());
    if (block.vtx[0].GetValueOut() > blockReward)
        return state.DoS(100,
                         error("ConnectBlock(): coinbase pays too much (actual=%d vs limit=%d)",
                               block.vtx[0].GetValueOut(), blockReward),
                         REJECT_INVALID, "bad-cb-amount");

    if (!control.Wait())
        return state.DoS(100, false);
    int64_t nTime4 = GetTimeMicros();
    nTimeVerify += nTime4 - nTime2;
    LogPrint("bench", "    - Verify %u txins: %.2fms (%.3fms/txin) [%.2fs]\n", nInputs - 1, 0.001 * (nTime4 - nTime2), nInputs <= 1 ? 0 : 0.001 * (nTime4 - nTime2) / (nInputs - 1), nTimeVerify * 0.000001);

    if (fJustCheck)
        return true;

    if (pindex->GetUndoPos().IsNull() || !pindex->IsValid(BLOCK_VALID_SCRIPTS)) {
        if (pindex->GetUndoPos().IsNull()) {
            CDiskBlockPos pos;
            if (!FindUndoPos(state, pindex->nFile, pos, ::GetSerializeSize(blockundo, SER_DISK, CLIENT_VERSION) + 40))
                return error("ConnectBlock(): FindUndoPos failed");
            if (!UndoWriteToDisk(blockundo, pos, pindex->pprev->GetBlockHash(), chainparams.MessageStart()))
                return AbortNode(state, "Failed to write undo data");

            pindex->nUndoPos = pos.nPos;
            pindex->nStatus |= BLOCK_HAVE_UNDO;
        }

        pindex->RaiseValidity(BLOCK_VALID_SCRIPTS);
        setDirtyBlockIndex.insert(pindex);
    }

    if (fTxIndex)
        if (!pblocktree->WriteTxIndex(vPos))
            return AbortNode(state, "Failed to write transaction index");

    view.SetBestBlock(pindex->GetBlockHash());

    int64_t nTime5 = GetTimeMicros();
    nTimeIndex += nTime5 - nTime4;
    LogPrint("bench", "    - Index writing: %.2fms [%.2fs]\n", 0.001 * (nTime5 - nTime4), nTimeIndex * 0.000001);

    static uint256 hashPrevBestCoinBase;
    GetMainSignals().UpdatedTransaction(hashPrevBestCoinBase);
    hashPrevBestCoinBase = block.vtx[0].GetHash();

    if (vOrphanErase.size()) {
        int nErased = 0;
        BOOST_FOREACH (uint256& orphanHash, vOrphanErase) {
            nErased += EraseOrphanTx(orphanHash);
        }
        LogPrint("mempool", "Erased %d orphan tx included or conflicted by block\n", nErased);
    }

    int64_t nTime6 = GetTimeMicros();
    nTimeCallbacks += nTime6 - nTime5;
    LogPrint("bench", "    - Callbacks: %.2fms [%.2fs]\n", 0.001 * (nTime6 - nTime5), nTimeCallbacks * 0.000001);

    return true;
}

enum FlushStateMode {
    FLUSH_STATE_NONE,
    FLUSH_STATE_IF_NEEDED,
    FLUSH_STATE_PERIODIC,
    FLUSH_STATE_ALWAYS
};

/**
 * Update the on-disk chain state.
 * The caches and indexes are flushed depending on the mode we're called with
 * if they're too large, if it's been a while since the last write,
 * or always and in all cases if we're in prune mode and are deleting files.
 */
bool static FlushStateToDisk(CValidationState& state, FlushStateMode mode)
{
    const CChainParams& chainparams = Params();
    LOCK2(cs_main, cs_LastBlockFile);
    static int64_t nLastWrite = 0;
    static int64_t nLastFlush = 0;
    static int64_t nLastSetChain = 0;
    std::set<int> setFilesToPrune;
    bool fFlushForPrune = false;
    try {
        if (fPruneMode && fCheckForPruning && !fReindex) {
            FindFilesToPrune(setFilesToPrune, chainparams.PruneAfterHeight());
            fCheckForPruning = false;
            if (!setFilesToPrune.empty()) {
                fFlushForPrune = true;
                if (!fHavePruned) {
                    pblocktree->WriteFlag("prunedblockfiles", true);
                    fHavePruned = true;
                }
            }
        }
        int64_t nNow = GetTimeMicros();

        if (nLastWrite == 0) {
            nLastWrite = nNow;
        }
        if (nLastFlush == 0) {
            nLastFlush = nNow;
        }
        if (nLastSetChain == 0) {
            nLastSetChain = nNow;
        }
        size_t cacheSize = pcoinsTip->DynamicMemoryUsage();

        bool fCacheLarge = mode == FLUSH_STATE_PERIODIC && cacheSize * (10.0 / 9) > nCoinCacheUsage;

        bool fCacheCritical = mode == FLUSH_STATE_IF_NEEDED && cacheSize > nCoinCacheUsage;

        bool fPeriodicWrite = mode == FLUSH_STATE_PERIODIC && nNow > nLastWrite + (int64_t)DATABASE_WRITE_INTERVAL * 1000000;

        bool fPeriodicFlush = mode == FLUSH_STATE_PERIODIC && nNow > nLastFlush + (int64_t)DATABASE_FLUSH_INTERVAL * 1000000;

        bool fDoFullFlush = (mode == FLUSH_STATE_ALWAYS) || fCacheLarge || fCacheCritical || fPeriodicFlush || fFlushForPrune;

        if (fDoFullFlush || fPeriodicWrite) {

            if (!CheckDiskSpace(0))
                return state.Error("out of disk space");

            FlushBlockFile();

            {
                std::vector<std::pair<int, const CBlockFileInfo*> > vFiles;
                vFiles.reserve(setDirtyFileInfo.size());
                for (set<int>::iterator it = setDirtyFileInfo.begin(); it != setDirtyFileInfo.end();) {
                    vFiles.push_back(make_pair(*it, &vinfoBlockFile[*it]));
                    setDirtyFileInfo.erase(it++);
                }
                std::vector<const CBlockIndex*> vBlocks;
                vBlocks.reserve(setDirtyBlockIndex.size());
                for (set<CBlockIndex*>::iterator it = setDirtyBlockIndex.begin(); it != setDirtyBlockIndex.end();) {
                    vBlocks.push_back(*it);
                    setDirtyBlockIndex.erase(it++);
                }
                if (!pblocktree->WriteBatchSync(vFiles, nLastBlockFile, vBlocks)) {
                    return AbortNode(state, "Files to write to block index database");
                }
            }

            if (fFlushForPrune)
                UnlinkPrunedFiles(setFilesToPrune);
            nLastWrite = nNow;
        }

        if (fDoFullFlush) {

            if (!CheckDiskSpace(128 * 2 * 2 * pcoinsTip->GetCacheSize()))
                return state.Error("out of disk space");

            if (!pcoinsTip->Flush())
                return AbortNode(state, "Failed to write to coin database");
            nLastFlush = nNow;
        }
        if (fDoFullFlush || ((mode == FLUSH_STATE_ALWAYS || mode == FLUSH_STATE_PERIODIC) && nNow > nLastSetChain + (int64_t)DATABASE_WRITE_INTERVAL * 1000000)) {

            GetMainSignals().SetBestChain(chainActive.GetLocator());
            nLastSetChain = nNow;
        }
    }
    catch (const std::runtime_error& e) {
        return AbortNode(state, std::string("System error while flushing: ") + e.what());
    }
    return true;
}

void FlushStateToDisk()
{
    CValidationState state;
    FlushStateToDisk(state, FLUSH_STATE_ALWAYS);
}

void PruneAndFlush()
{
    CValidationState state;
    fCheckForPruning = true;
    FlushStateToDisk(state, FLUSH_STATE_NONE);
}

/** Update chainActive and related internal data structures. */
void static UpdateTip(CBlockIndex* pindexNew, const CChainParams& chainParams)
{
    chainActive.SetTip(pindexNew);

    nTimeBestReceived = GetTime();
    mempool.AddTransactionsUpdated(1);

    cvBlockChange.notify_all();

    static bool fWarned = false;
    std::vector<std::string> warningMessages;
    if (!IsInitialBlockDownload()) {
        int nUpgraded = 0;
        const CBlockIndex* pindex = chainActive.Tip();
        for (int bit = 0; bit < VERSIONBITS_NUM_BITS; bit++) {
            WarningBitsConditionChecker checker(bit);
            ThresholdState state = checker.GetStateFor(pindex, chainParams.GetConsensus(), warningcache[bit]);
            if (state == THRESHOLD_ACTIVE || state == THRESHOLD_LOCKED_IN) {
                if (state == THRESHOLD_ACTIVE) {
                    strMiscWarning = strprintf(_("Warning: unknown new rules activated (versionbit %i)"), bit);
                    if (!fWarned) {
                        CAlert::Notify(strMiscWarning, true);
                        fWarned = true;
                    }
                } else {
                    warningMessages.push_back(strprintf("unknown new rules are about to activate (versionbit %i)", bit));
                }
            }
        }

        for (int i = 0; i < 100 && pindex != NULL; i++) {
            int32_t nExpectedVersion = ComputeBlockVersion(pindex->pprev, chainParams.GetConsensus());
            if (pindex->nVersion > VERSIONBITS_LAST_OLD_BLOCK_VERSION && (pindex->nVersion & ~nExpectedVersion) != 0)
                ++nUpgraded;
            pindex = pindex->pprev;
        }
        if (nUpgraded > 0)
            warningMessages.push_back(strprintf("%d of last 100 blocks have unexpected version", nUpgraded));
        if (nUpgraded > 100 / 2) {

            strMiscWarning = _("Warning: Unknown block versions being mined! It's possible unknown rules are in effect");
            if (!fWarned) {
                CAlert::Notify(strMiscWarning, true);
                fWarned = true;
            }
        }
    }
    LogPrintf("%s: new best=%s height=%d version=0x%08x log2_work=%.8g tx=%lu date='%s' progress=%f cache=%.1fMiB(%utx)", __func__,
              chainActive.Tip()->GetBlockHash().ToString(), chainActive.Height(), chainActive.Tip()->nVersion,
              log(chainActive.Tip()->nChainWork.getdouble()) / log(2.0), (unsigned long)chainActive.Tip()->nChainTx,
              DateTimeStrFormat("%Y-%m-%d %H:%M:%S", chainActive.Tip()->GetBlockTime()),
              Checkpoints::GuessVerificationProgress(chainParams.Checkpoints(), chainActive.Tip()), pcoinsTip->DynamicMemoryUsage() * (1.0 / (1 << 20)), pcoinsTip->GetCacheSize());
    if (!warningMessages.empty())
        LogPrintf(" warning='%s'", boost::algorithm::join(warningMessages, ", "));
    LogPrintf("\n");
}

/** Disconnect chainActive's tip. You probably want to call mempool.removeForReorg and manually re-limit mempool size after this, with cs_main held. */
bool static DisconnectTip(CValidationState& state, const CChainParams& chainparams, bool fBare = false)
{
    CBlockIndex* pindexDelete = chainActive.Tip();
    assert(pindexDelete);

    CBlock block;
    if (!ReadBlockFromDisk(block, pindexDelete, chainparams.GetConsensus()))
        return AbortNode(state, "Failed to read block");

    int64_t nStart = GetTimeMicros();
    {
        CCoinsViewCache view(pcoinsTip);
        if (!DisconnectBlock(block, state, pindexDelete, view))
            return error("DisconnectTip(): DisconnectBlock %s failed", pindexDelete->GetBlockHash().ToString());
        assert(view.Flush());
    }
    LogPrint("bench", "- Disconnect block: %.2fms\n", (GetTimeMicros() - nStart) * 0.001);

    if (!FlushStateToDisk(state, FLUSH_STATE_IF_NEEDED))
        return false;

    if (!fBare) {

        std::vector<uint256> vHashUpdate;
        BOOST_FOREACH (const CTransaction& tx, block.vtx) {

            list<CTransaction> removed;
            CValidationState stateDummy;
            if (tx.IsCoinBase() || !AcceptToMemoryPool(mempool, stateDummy, tx, false, NULL, true)) {
                mempool.removeRecursive(tx, removed);
            } else if (mempool.exists(tx.GetHash())) {
                vHashUpdate.push_back(tx.GetHash());
            }
        }

        mempool.UpdateTransactionsFromBlock(vHashUpdate);
    }

    UpdateTip(pindexDelete->pprev, chainparams);

    BOOST_FOREACH (const CTransaction& tx, block.vtx) {
        SyncWithWallets(tx, pindexDelete->pprev, NULL);
    }
    return true;
}

static int64_t nTimeReadFromDisk = 0;
static int64_t nTimeConnectTotal = 0;
static int64_t nTimeFlush = 0;
static int64_t nTimeChainState = 0;
static int64_t nTimePostConnect = 0;

/**
 * Connect a new block to chainActive. pblock is either NULL or a pointer to a CBlock
 * corresponding to pindexNew, to bypass loading it again from disk.
 */
bool static ConnectTip(CValidationState& state, const CChainParams& chainparams, CBlockIndex* pindexNew, const CBlock* pblock)
{
    assert(pindexNew->pprev == chainActive.Tip());

    int64_t nTime1 = GetTimeMicros();
    CBlock block;
    if (!pblock) {
        if (!ReadBlockFromDisk(block, pindexNew, chainparams.GetConsensus()))
            return AbortNode(state, "Failed to read block");
        pblock = &block;
    }

    int64_t nTime2 = GetTimeMicros();
    nTimeReadFromDisk += nTime2 - nTime1;
    int64_t nTime3;
    LogPrint("bench", "  - Load block from disk: %.2fms [%.2fs]\n", (nTime2 - nTime1) * 0.001, nTimeReadFromDisk * 0.000001);
    {
        CCoinsViewCache view(pcoinsTip);
        bool rv = ConnectBlock(*pblock, state, pindexNew, view, chainparams);
        GetMainSignals().BlockChecked(*pblock, state);
        if (!rv) {
            if (state.IsInvalid())
                InvalidBlockFound(pindexNew, state);
            return error("ConnectTip(): ConnectBlock %s failed", pindexNew->GetBlockHash().ToString());
        }
        mapBlockSource.erase(pindexNew->GetBlockHash());
        nTime3 = GetTimeMicros();
        nTimeConnectTotal += nTime3 - nTime2;
        LogPrint("bench", "  - Connect total: %.2fms [%.2fs]\n", (nTime3 - nTime2) * 0.001, nTimeConnectTotal * 0.000001);
        assert(view.Flush());
    }
    int64_t nTime4 = GetTimeMicros();
    nTimeFlush += nTime4 - nTime3;
    LogPrint("bench", "  - Flush: %.2fms [%.2fs]\n", (nTime4 - nTime3) * 0.001, nTimeFlush * 0.000001);

    if (!FlushStateToDisk(state, FLUSH_STATE_IF_NEEDED))
        return false;
    int64_t nTime5 = GetTimeMicros();
    nTimeChainState += nTime5 - nTime4;
    LogPrint("bench", "  - Writing chainstate: %.2fms [%.2fs]\n", (nTime5 - nTime4) * 0.001, nTimeChainState * 0.000001);

    list<CTransaction> txConflicted;
    mempool.removeForBlock(pblock->vtx, pindexNew->nHeight, txConflicted, !IsInitialBlockDownload());

    UpdateTip(pindexNew, chainparams);

    BOOST_FOREACH (const CTransaction& tx, txConflicted) {
        SyncWithWallets(tx, pindexNew, NULL);
    }

    BOOST_FOREACH (const CTransaction& tx, pblock->vtx) {
        SyncWithWallets(tx, pindexNew, pblock);
    }

    int64_t nTime6 = GetTimeMicros();
    nTimePostConnect += nTime6 - nTime5;
    nTimeTotal += nTime6 - nTime1;
    LogPrint("bench", "  - Connect postprocess: %.2fms [%.2fs]\n", (nTime6 - nTime5) * 0.001, nTimePostConnect * 0.000001);
    LogPrint("bench", "- Connect block: %.2fms [%.2fs]\n", (nTime6 - nTime1) * 0.001, nTimeTotal * 0.000001);
    return true;
}

/**
 * Return the tip of the chain with the most work in it, that isn't
 * known to be invalid (it's however far from certain to be valid).
 */
static CBlockIndex* FindMostWorkChain()
{
    do {
        CBlockIndex* pindexNew = NULL;

        {
            std::set<CBlockIndex*, CBlockIndexWorkComparator>::reverse_iterator it = setBlockIndexCandidates.rbegin();
            if (it == setBlockIndexCandidates.rend())
                return NULL;
            pindexNew = *it;
        }

        CBlockIndex* pindexTest = pindexNew;
        bool fInvalidAncestor = false;
        while (pindexTest && !chainActive.Contains(pindexTest)) {
            assert(pindexTest->nChainTx || pindexTest->nHeight == 0);

            bool fFailedChain = pindexTest->nStatus & BLOCK_FAILED_MASK;
            bool fMissingData = !(pindexTest->nStatus & BLOCK_HAVE_DATA);
            if (fFailedChain || fMissingData) {

                if (fFailedChain && (pindexBestInvalid == NULL || pindexNew->nChainWork > pindexBestInvalid->nChainWork))
                    pindexBestInvalid = pindexNew;
                CBlockIndex* pindexFailed = pindexNew;

                while (pindexTest != pindexFailed) {
                    if (fFailedChain) {
                        pindexFailed->nStatus |= BLOCK_FAILED_CHILD;
                    } else if (fMissingData) {

                        mapBlocksUnlinked.insert(std::make_pair(pindexFailed->pprev, pindexFailed));
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
    } while (true);
}

/** Delete all entries in setBlockIndexCandidates that are worse than the current tip. */
static void PruneBlockIndexCandidates()
{

    std::set<CBlockIndex*, CBlockIndexWorkComparator>::iterator it = setBlockIndexCandidates.begin();
    while (it != setBlockIndexCandidates.end() && setBlockIndexCandidates.value_comp()(*it, chainActive.Tip())) {
        setBlockIndexCandidates.erase(it++);
    }

    assert(!setBlockIndexCandidates.empty());
}

/**
 * Try to make some progress towards making pindexMostWork the active block.
 * pblock is either NULL or a pointer to a CBlock corresponding to pindexMostWork.
 */
static bool ActivateBestChainStep(CValidationState& state, const CChainParams& chainparams, CBlockIndex* pindexMostWork, const CBlock* pblock, bool& fInvalidFound)
{
    AssertLockHeld(cs_main);
    const CBlockIndex* pindexOldTip = chainActive.Tip();
    const CBlockIndex* pindexFork = chainActive.FindFork(pindexMostWork);

    bool fBlocksDisconnected = false;
    while (chainActive.Tip() && chainActive.Tip() != pindexFork) {
        if (!DisconnectTip(state, chainparams))
            return false;
        fBlocksDisconnected = true;
    }

    std::vector<CBlockIndex*> vpindexToConnect;
    bool fContinue = true;
    int nHeight = pindexFork ? pindexFork->nHeight : -1;
    while (fContinue && nHeight != pindexMostWork->nHeight) {

        int nTargetHeight = std::min(nHeight + 32, pindexMostWork->nHeight);
        vpindexToConnect.clear();
        vpindexToConnect.reserve(nTargetHeight - nHeight);
        CBlockIndex* pindexIter = pindexMostWork->GetAncestor(nTargetHeight);
        while (pindexIter && pindexIter->nHeight != nHeight) {
            vpindexToConnect.push_back(pindexIter);
            pindexIter = pindexIter->pprev;
        }
        nHeight = nTargetHeight;

        BOOST_REVERSE_FOREACH(CBlockIndex * pindexConnect, vpindexToConnect)
        {
            if (!ConnectTip(state, chainparams, pindexConnect, pindexConnect == pindexMostWork ? pblock : NULL)) {
                if (state.IsInvalid()) {

                    if (!state.CorruptionPossible())
                        InvalidChainFound(vpindexToConnect.back());
                    state = CValidationState();
                    fInvalidFound = true;
                    fContinue = false;
                    break;
                } else {

                    return false;
                }
            } else {
                PruneBlockIndexCandidates();
                if (!pindexOldTip || chainActive.Tip()->nChainWork > pindexOldTip->nChainWork) {

                    fContinue = false;
                    break;
                }
            }
        }
    }

    if (fBlocksDisconnected) {
        mempool.removeForReorg(pcoinsTip, chainActive.Tip()->nHeight + 1, STANDARD_LOCKTIME_VERIFY_FLAGS);
        LimitMempoolSize(mempool, GetArg("-maxmempool", DEFAULT_MAX_MEMPOOL_SIZE) * 1000000, GetArg("-mempoolexpiry", DEFAULT_MEMPOOL_EXPIRY) * 60 * 60);
    }
    mempool.check(pcoinsTip);

    if (fInvalidFound)
        CheckForkWarningConditionsOnNewFork(vpindexToConnect.back());
    else
        CheckForkWarningConditions();

    return true;
}

static void NotifyHeaderTip()
{
    bool fNotify = false;
    bool fInitialBlockDownload = false;
    static CBlockIndex* pindexHeaderOld = NULL;
    CBlockIndex* pindexHeader = NULL;
    {
        LOCK(cs_main);
        if (!setBlockIndexCandidates.empty()) {
            pindexHeader = *setBlockIndexCandidates.rbegin();
        }
        if (pindexHeader != pindexHeaderOld) {
            fNotify = true;
            fInitialBlockDownload = IsInitialBlockDownload();
            pindexHeaderOld = pindexHeader;
        }
    }

    if (fNotify) {
        uiInterface.NotifyHeaderTip(fInitialBlockDownload, pindexHeader);
    }
}

/**
 * Make the best chain active, in multiple steps. The result is either failure
 * or an activated best chain. pblock is either NULL or a pointer to a block
 * that is already loaded (to avoid loading it again from disk).
 */
bool ActivateBestChain(CValidationState& state, const CChainParams& chainparams, const CBlock* pblock)
{
    CBlockIndex* pindexMostWork = NULL;
    CBlockIndex* pindexNewTip = NULL;
    do {
        boost::this_thread::interruption_point();
        if (ShutdownRequested())
            break;

        const CBlockIndex* pindexFork;
        bool fInitialDownload;
        int nNewHeight;
        {
            LOCK(cs_main);
            CBlockIndex* pindexOldTip = chainActive.Tip();
            if (pindexMostWork == NULL) {
                pindexMostWork = FindMostWorkChain();
            }

            if (pindexMostWork == NULL || pindexMostWork == chainActive.Tip())
                return true;

            bool fInvalidFound = false;
            if (!ActivateBestChainStep(state, chainparams, pindexMostWork, pblock && pblock->GetHash() == pindexMostWork->GetBlockHash() ? pblock : NULL, fInvalidFound))
                return false;

            if (fInvalidFound) {

                pindexMostWork = NULL;
            }
            pindexNewTip = chainActive.Tip();
            pindexFork = chainActive.FindFork(pindexOldTip);
            fInitialDownload = IsInitialBlockDownload();
            nNewHeight = chainActive.Height();
        }

        if (pindexFork != pindexNewTip) {
            uiInterface.NotifyBlockTip(fInitialDownload, pindexNewTip);

            if (!fInitialDownload) {

                std::vector<uint256> vHashes;
                CBlockIndex* pindexToAnnounce = pindexNewTip;
                while (pindexToAnnounce != pindexFork) {
                    vHashes.push_back(pindexToAnnounce->GetBlockHash());
                    pindexToAnnounce = pindexToAnnounce->pprev;
                    if (vHashes.size() == MAX_BLOCKS_TO_ANNOUNCE) {

                        break;
                    }
                }

                int nBlockEstimate = 0;
                if (fCheckpointsEnabled)
                    nBlockEstimate = Checkpoints::GetTotalBlocksEstimate(chainparams.Checkpoints());
                {
                    LOCK(cs_vNodes);
                    BOOST_FOREACH (CNode* pnode, vNodes) {
                        if (nNewHeight > (pnode->nStartingHeight != -1 ? pnode->nStartingHeight - 2000 : nBlockEstimate)) {
                            BOOST_REVERSE_FOREACH(const uint256& hash, vHashes)
                            {
                                pnode->PushBlockHash(hash);
                            }
                        }
                    }
                }

                if (!vHashes.empty()) {
                    GetMainSignals().UpdatedBlockTip(pindexNewTip);
                }
            }
        }
    } while (pindexNewTip != pindexMostWork);
    CheckBlockIndex(chainparams.GetConsensus());

    if (!FlushStateToDisk(state, FLUSH_STATE_PERIODIC)) {
        return false;
    }

    return true;
}

bool InvalidateBlock(CValidationState& state, const CChainParams& chainparams, CBlockIndex* pindex)
{
    AssertLockHeld(cs_main);

    pindex->nStatus |= BLOCK_FAILED_VALID;
    setDirtyBlockIndex.insert(pindex);
    setBlockIndexCandidates.erase(pindex);

    while (chainActive.Contains(pindex)) {
        CBlockIndex* pindexWalk = chainActive.Tip();
        pindexWalk->nStatus |= BLOCK_FAILED_CHILD;
        setDirtyBlockIndex.insert(pindexWalk);
        setBlockIndexCandidates.erase(pindexWalk);

        if (!DisconnectTip(state, chainparams)) {
            mempool.removeForReorg(pcoinsTip, chainActive.Tip()->nHeight + 1, STANDARD_LOCKTIME_VERIFY_FLAGS);
            return false;
        }
    }

    LimitMempoolSize(mempool, GetArg("-maxmempool", DEFAULT_MAX_MEMPOOL_SIZE) * 1000000, GetArg("-mempoolexpiry", DEFAULT_MEMPOOL_EXPIRY) * 60 * 60);

    BlockMap::iterator it = mapBlockIndex.begin();
    while (it != mapBlockIndex.end()) {
        if (it->second->IsValid(BLOCK_VALID_TRANSACTIONS) && it->second->nChainTx && !setBlockIndexCandidates.value_comp()(it->second, chainActive.Tip())) {
            setBlockIndexCandidates.insert(it->second);
        }
        it++;
    }

    InvalidChainFound(pindex);
    mempool.removeForReorg(pcoinsTip, chainActive.Tip()->nHeight + 1, STANDARD_LOCKTIME_VERIFY_FLAGS);
    return true;
}

bool ResetBlockFailureFlags(CBlockIndex* pindex)
{
    AssertLockHeld(cs_main);

    int nHeight = pindex->nHeight;

    BlockMap::iterator it = mapBlockIndex.begin();
    while (it != mapBlockIndex.end()) {
        if (!it->second->IsValid() && it->second->GetAncestor(nHeight) == pindex) {
            it->second->nStatus &= ~BLOCK_FAILED_MASK;
            setDirtyBlockIndex.insert(it->second);
            if (it->second->IsValid(BLOCK_VALID_TRANSACTIONS) && it->second->nChainTx && setBlockIndexCandidates.value_comp()(chainActive.Tip(), it->second)) {
                setBlockIndexCandidates.insert(it->second);
            }
            if (it->second == pindexBestInvalid) {

                pindexBestInvalid = NULL;
            }
        }
        it++;
    }

    while (pindex != NULL) {
        if (pindex->nStatus & BLOCK_FAILED_MASK) {
            pindex->nStatus &= ~BLOCK_FAILED_MASK;
            setDirtyBlockIndex.insert(pindex);
        }
        pindex = pindex->pprev;
    }
    return true;
}

CBlockIndex* AddToBlockIndex(const CBlockHeader& block)
{

    uint256 hash = block.GetHash();
    BlockMap::iterator it = mapBlockIndex.find(hash);
    if (it != mapBlockIndex.end())
        return it->second;

    CBlockIndex* pindexNew = new CBlockIndex(block);
    assert(pindexNew);

    pindexNew->nSequenceId = 0;
    BlockMap::iterator mi = mapBlockIndex.insert(make_pair(hash, pindexNew)).first;
    pindexNew->phashBlock = &((*mi).first);
    BlockMap::iterator miPrev = mapBlockIndex.find(block.hashPrevBlock);
    if (miPrev != mapBlockIndex.end()) {
        pindexNew->pprev = (*miPrev).second;
        pindexNew->nHeight = pindexNew->pprev->nHeight + 1;
        pindexNew->BuildSkip();
    }
    pindexNew->nChainWork = (pindexNew->pprev ? pindexNew->pprev->nChainWork : 0) + GetBlockProof(*pindexNew);
    pindexNew->RaiseValidity(BLOCK_VALID_TREE);
    if (pindexBestHeader == NULL || pindexBestHeader->nChainWork < pindexNew->nChainWork)
        pindexBestHeader = pindexNew;

    setDirtyBlockIndex.insert(pindexNew);

    return pindexNew;
}

/** Mark a block as having its data received and checked (up to BLOCK_VALID_TRANSACTIONS). */
bool ReceivedBlockTransactions(const CBlock& block, CValidationState& state, CBlockIndex* pindexNew, const CDiskBlockPos& pos)
{
    pindexNew->nTx = block.vtx.size();
    pindexNew->nChainTx = 0;
    pindexNew->nFile = pos.nFile;
    pindexNew->nDataPos = pos.nPos;
    pindexNew->nUndoPos = 0;
    pindexNew->nStatus |= BLOCK_HAVE_DATA;
    if (IsWitnessEnabled(pindexNew->pprev, Params().GetConsensus())) {
        pindexNew->nStatus |= BLOCK_OPT_WITNESS;
    }
    pindexNew->RaiseValidity(BLOCK_VALID_TRANSACTIONS);
    setDirtyBlockIndex.insert(pindexNew);

    if (pindexNew->pprev == NULL || pindexNew->pprev->nChainTx) {

        deque<CBlockIndex*> queue;
        queue.push_back(pindexNew);

        while (!queue.empty()) {
            CBlockIndex* pindex = queue.front();
            queue.pop_front();
            pindex->nChainTx = (pindex->pprev ? pindex->pprev->nChainTx : 0) + pindex->nTx;
            {
                LOCK(cs_nBlockSequenceId);
                pindex->nSequenceId = nBlockSequenceId++;
            }
            if (chainActive.Tip() == NULL || !setBlockIndexCandidates.value_comp()(pindex, chainActive.Tip())) {
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
            mapBlocksUnlinked.insert(std::make_pair(pindexNew->pprev, pindexNew));
        }
    }

    return true;
}

bool FindBlockPos(CValidationState& state, CDiskBlockPos& pos, unsigned int nAddSize, unsigned int nHeight, uint64_t nTime, bool fKnown = false)
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
                FILE* file = OpenBlockFile(pos);
                if (file) {
                    LogPrintf("Pre-allocating up to position 0x%x in blk%05u.dat\n", nNewChunks * BLOCKFILE_CHUNK_SIZE, pos.nFile);
                    AllocateFileRange(file, pos.nPos, nNewChunks * BLOCKFILE_CHUNK_SIZE - pos.nPos);
                    fclose(file);
                }
            } else
                return state.Error("out of disk space");
        }
    }

    setDirtyFileInfo.insert(nFile);
    return true;
}

bool FindUndoPos(CValidationState& state, int nFile, CDiskBlockPos& pos, unsigned int nAddSize)
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
            FILE* file = OpenUndoFile(pos);
            if (file) {
                LogPrintf("Pre-allocating up to position 0x%x in rev%05u.dat\n", nNewChunks * UNDOFILE_CHUNK_SIZE, pos.nFile);
                AllocateFileRange(file, pos.nPos, nNewChunks * UNDOFILE_CHUNK_SIZE - pos.nPos);
                fclose(file);
            }
        } else
            return state.Error("out of disk space");
    }

    return true;
}

bool CheckBlockHeader(const CBlock& block, CValidationState& state, const Consensus::Params& consensusParams, bool fCheckPOW)
{

    if (fCheckPOW && !CheckProofOfWork(block.GetPoWHash(), block.nBits, consensusParams))
        return state.DoS(50, false, REJECT_INVALID, "high-hash", false, "proof of work failed");

    return true;
}

bool CheckBlock(const CBlock& block, CValidationState& state, const Consensus::Params& consensusParams, bool fCheckPOW, bool fCheckMerkleRoot)
{

    if (block.fChecked)
        return true;

    if (!CheckBlockHeader(block, state, consensusParams, fCheckPOW))
        return false;

    if (fCheckMerkleRoot) {
        bool mutated;
        uint256 hashMerkleRoot2 = BlockMerkleRoot(block, &mutated);
        if (block.hashMerkleRoot != hashMerkleRoot2)
            return state.DoS(100, false, REJECT_INVALID, "bad-txnmrklroot", true, "hashMerkleRoot mismatch");

        if (mutated)
            return state.DoS(100, false, REJECT_INVALID, "bad-txns-duplicate", true, "duplicate transaction");
    }

    if (block.vtx.empty() || block.vtx.size() > MAX_BLOCK_BASE_SIZE || ::GetSerializeSize(block, SER_NETWORK, PROTOCOL_VERSION | SERIALIZE_TRANSACTION_NO_WITNESS) > MAX_BLOCK_BASE_SIZE)
        return state.DoS(100, false, REJECT_INVALID, "bad-blk-length", false, "size limits failed");

    if (block.vtx.empty() || !block.vtx[0].IsCoinBase())
        return state.DoS(100, false, REJECT_INVALID, "bad-cb-missing", false, "first tx is not coinbase");
    for (unsigned int i = 1; i < block.vtx.size(); i++)
        if (block.vtx[i].IsCoinBase())
            return state.DoS(100, false, REJECT_INVALID, "bad-cb-multiple", false, "more than one coinbase");

    BOOST_FOREACH (const CTransaction& tx, block.vtx)
        if (!CheckTransaction(tx, state))
            return state.Invalid(false, state.GetRejectCode(), state.GetRejectReason(),
                                 strprintf("Transaction check failed (tx hash %s) %s", tx.GetHash().ToString(), state.GetDebugMessage()));

    unsigned int nSigOps = 0;
    BOOST_FOREACH (const CTransaction& tx, block.vtx) {
        nSigOps += GetLegacySigOpCount(tx);
    }
    if (nSigOps * WITNESS_SCALE_FACTOR > MAX_BLOCK_SIGOPS_COST)
        return state.DoS(100, false, REJECT_INVALID, "bad-blk-sigops", false, "out-of-bounds SigOpCount");

    if (fCheckPOW && fCheckMerkleRoot)
        block.fChecked = true;

    return true;
}

static bool CheckIndexAgainstCheckpoint(const CBlockIndex* pindexPrev, CValidationState& state, const CChainParams& chainparams, const uint256& hash)
{
    if (*pindexPrev->phashBlock == chainparams.GetConsensus().hashGenesisBlock)
        return true;

    int nHeight = pindexPrev->nHeight + 1;

    if (!Checkpoints::CheckSync(hash, pindexPrev))
        return error("AcceptBlock() : rejected by synchronized checkpoint");

    CBlockIndex* pcheckpoint = Checkpoints::GetLastCheckpoint(chainparams.Checkpoints());
    if (pcheckpoint && nHeight < pcheckpoint->nHeight)
        return state.DoS(100, error("%s: forked chain older than last checkpoint (height %d)", __func__, nHeight));

    return true;
}

bool IsWitnessEnabled(const CBlockIndex* pindexPrev, const Consensus::Params& params)
{
    LOCK(cs_main);
    return (VersionBitsState(pindexPrev, params, Consensus::DEPLOYMENT_SEGWIT, versionbitscache) == THRESHOLD_ACTIVE);
}

static int GetWitnessCommitmentIndex(const CBlock& block)
{
    int commitpos = -1;
    for (size_t o = 0; o < block.vtx[0].vout.size(); o++) {
        if (block.vtx[0].vout[o].scriptPubKey.size() >= 38 && block.vtx[0].vout[o].scriptPubKey[0] == OP_RETURN && block.vtx[0].vout[o].scriptPubKey[1] == 0x24 && block.vtx[0].vout[o].scriptPubKey[2] == 0xaa && block.vtx[0].vout[o].scriptPubKey[3] == 0x21 && block.vtx[0].vout[o].scriptPubKey[4] == 0xa9 && block.vtx[0].vout[o].scriptPubKey[5] == 0xed) {
            commitpos = o;
        }
    }
    return commitpos;
}

void UpdateUncommittedBlockStructures(CBlock& block, const CBlockIndex* pindexPrev, const Consensus::Params& consensusParams)
{
    int commitpos = GetWitnessCommitmentIndex(block);
    static const std::vector<unsigned char> nonce(32, 0x00);
    if (commitpos != -1 && IsWitnessEnabled(pindexPrev, consensusParams) && block.vtx[0].wit.IsEmpty()) {
        block.vtx[0].wit.vtxinwit.resize(1);
        block.vtx[0].wit.vtxinwit[0].scriptWitness.stack.resize(1);
        block.vtx[0].wit.vtxinwit[0].scriptWitness.stack[0] = nonce;
    }
}

std::vector<unsigned char> GenerateCoinbaseCommitment(CBlock& block, const CBlockIndex* pindexPrev, const Consensus::Params& consensusParams)
{
    std::vector<unsigned char> commitment;
    int commitpos = GetWitnessCommitmentIndex(block);
    bool fHaveWitness = false;
    for (size_t t = 1; t < block.vtx.size(); t++) {
        if (!block.vtx[t].wit.IsNull()) {
            fHaveWitness = true;
            break;
        }
    }
    std::vector<unsigned char> ret(32, 0x00);
    if (fHaveWitness && IsWitnessEnabled(pindexPrev, consensusParams)) {
        if (commitpos == -1) {
            uint256 witnessroot = BlockWitnessMerkleRoot(block, NULL);
            CHash256().Write(witnessroot.begin(), 32).Write(&ret[0], 32).Finalize(witnessroot.begin());
            CTxOut out;
            out.nValue = 0;
            out.scriptPubKey.resize(38);
            out.scriptPubKey[0] = OP_RETURN;
            out.scriptPubKey[1] = 0x24;
            out.scriptPubKey[2] = 0xaa;
            out.scriptPubKey[3] = 0x21;
            out.scriptPubKey[4] = 0xa9;
            out.scriptPubKey[5] = 0xed;
            memcpy(&out.scriptPubKey[6], witnessroot.begin(), 32);
            commitment = std::vector<unsigned char>(out.scriptPubKey.begin(), out.scriptPubKey.end());
            const_cast<std::vector<CTxOut>*>(&block.vtx[0].vout)->push_back(out);
            block.vtx[0].UpdateHash();
        }
    }
    UpdateUncommittedBlockStructures(block, pindexPrev, consensusParams);
    return commitment;
}

bool ContextualCheckBlockHeader(const CBlockHeader& block, CValidationState& state, const Consensus::Params& consensusParams, CBlockIndex* const pindexPrev, int64_t nAdjustedTime)
{

    if (block.nBits != GetNextWorkRequired(pindexPrev, &block, consensusParams))
        return state.DoS(100, false, REJECT_INVALID, "bad-diffbits", false, "incorrect proof of work");

    if (block.GetBlockTime() <= pindexPrev->GetMedianTimePast(pindexPrev->nHeight))
        return state.Invalid(false, REJECT_INVALID, "time-too-old", "block's timestamp is too early");

    if (pindexPrev->nHeight > (GetBoolArg("-testnet", false) ? 446500 : 437500)) {
        if (block.GetBlockTime() > nAdjustedTime + 60)
            return state.Invalid(false, REJECT_INVALID, "time-too-new", strprintf("block timestamp too far in the future block:%u adjusted:%u system:%u offset:%u", block.GetBlockTime(), nAdjustedTime, GetTime(), GetTimeOffset()));
    } else {
        if (block.GetBlockTime() > nAdjustedTime + 15 * 60)
            return state.Invalid(false, REJECT_INVALID, "time-too-new", "block timestamp too far in the future");
    }

    for (int32_t version = 2; version < 5; ++version) // check for version 2, 3 and 4 upgrades
        if (block.nVersion < version && IsSuperMajority(version, pindexPrev, consensusParams.nMajorityRejectBlockOutdated, consensusParams))
            return state.Invalid(false, REJECT_OBSOLETE, strprintf("bad-version(0x%08x)", version - 1),
                                 strprintf("rejected nVersion=0x%08x block", version - 1));

    return true;
}

bool ContextualCheckBlock(const CBlock& block, CValidationState& state, CBlockIndex* const pindexPrev)
{
    const int nHeight = pindexPrev == NULL ? 0 : pindexPrev->nHeight + 1;
    const Consensus::Params& consensusParams = Params().GetConsensus();

    int nLockTimeFlags = 0;
    if (VersionBitsState(pindexPrev, consensusParams, Consensus::DEPLOYMENT_CSV, versionbitscache) == THRESHOLD_ACTIVE) {
        nLockTimeFlags |= LOCKTIME_MEDIAN_TIME_PAST;
    }

    int64_t nLockTimeCutoff = (nLockTimeFlags & LOCKTIME_MEDIAN_TIME_PAST)
                                  ? pindexPrev->GetMedianTimePast(pindexPrev->nHeight)
                                  : block.GetBlockTime();

    BOOST_FOREACH (const CTransaction& tx, block.vtx) {
        if (!IsFinalTx(tx, nHeight, nLockTimeCutoff)) {
            return state.DoS(10, false, REJECT_INVALID, "bad-txns-nonfinal", false, "non-final transaction");
        }
    }

    if (block.nVersion >= 2 && IsSuperMajority(2, pindexPrev, consensusParams.nMajorityEnforceBlockUpgrade, consensusParams)) {
        CScript expect = CScript() << nHeight;
        if (block.vtx[0].vin[0].scriptSig.size() < expect.size() || !std::equal(expect.begin(), expect.end(), block.vtx[0].vin[0].scriptSig.begin())) {
            return state.DoS(100, false, REJECT_INVALID, "bad-cb-height", false, "block height mismatch in coinbase");
        }
    }

    bool fHaveWitness = false;
    if (IsWitnessEnabled(pindexPrev, consensusParams)) {
        int commitpos = GetWitnessCommitmentIndex(block);
        if (commitpos != -1) {
            bool malleated = false;
            uint256 hashWitness = BlockWitnessMerkleRoot(block, &malleated);

            if (block.vtx[0].wit.vtxinwit.size() != 1 || block.vtx[0].wit.vtxinwit[0].scriptWitness.stack.size() != 1 || block.vtx[0].wit.vtxinwit[0].scriptWitness.stack[0].size() != 32) {
                return state.DoS(100, error("%s : invalid witness nonce size", __func__), REJECT_INVALID, "bad-witness-nonce-size", true);
            }
            CHash256().Write(hashWitness.begin(), 32).Write(&block.vtx[0].wit.vtxinwit[0].scriptWitness.stack[0][0], 32).Finalize(hashWitness.begin());
            if (memcmp(hashWitness.begin(), &block.vtx[0].vout[commitpos].scriptPubKey[6], 32)) {
                return state.DoS(100, error("%s : witness merkle commitment mismatch", __func__), REJECT_INVALID, "bad-witness-merkle-match", true);
            }
            fHaveWitness = true;
        }
    }

    if (!fHaveWitness) {
        for (size_t i = 0; i < block.vtx.size(); i++) {
            if (!block.vtx[i].wit.IsNull()) {
                return state.DoS(100, error("%s : unexpected witness data found", __func__), REJECT_INVALID, "unexpected-witness", true);
            }
        }
    }

    if (GetBlockWeight(block) > MAX_BLOCK_WEIGHT) {
        return state.DoS(100, error("ContextualCheckBlock(): weight limit failed"), REJECT_INVALID, "bad-blk-weight");
    }

    return true;
}

static bool AcceptBlockHeader(const CBlockHeader& block, CValidationState& state, const CChainParams& chainparams, CBlockIndex** ppindex = NULL)
{
    AssertLockHeld(cs_main);

    uint256 hash = block.GetHash();
    BlockMap::iterator miSelf = mapBlockIndex.find(hash);
    CBlockIndex* pindex = NULL;
    if (hash != chainparams.GetConsensus().hashGenesisBlock) {

        if (miSelf != mapBlockIndex.end()) {

            pindex = miSelf->second;
            if (ppindex)
                *ppindex = pindex;
            if (pindex->nStatus & BLOCK_FAILED_MASK)
                return state.Invalid(error("%s: block %s is marked invalid", __func__, hash.ToString()), 0, "duplicate");
            return true;
        }

        if (!CheckBlockHeader(block, state, chainparams.GetConsensus()))
            return error("%s: Consensus::CheckBlockHeader: %s, %s", __func__, hash.ToString(), FormatStateMessage(state));

        CBlockIndex* pindexPrev = NULL;
        BlockMap::iterator mi = mapBlockIndex.find(block.hashPrevBlock);
        if (mi == mapBlockIndex.end())
            return state.DoS(10, error("%s: prev block not found", __func__), 0, "bad-prevblk");
        pindexPrev = (*mi).second;
        if (pindexPrev->nStatus & BLOCK_FAILED_MASK)
            return state.DoS(100, error("%s: prev block invalid", __func__), REJECT_INVALID, "bad-prevblk");

        assert(pindexPrev);
        if (fCheckpointsEnabled && !CheckIndexAgainstCheckpoint(pindexPrev, state, chainparams, hash))
            return error("%s: CheckIndexAgainstCheckpoint(): %s", __func__, FormatStateMessage(state));

        if (!ContextualCheckBlockHeader(block, state, chainparams.GetConsensus(), pindexPrev, GetAdjustedTime()))
            return error("%s: Consensus::ContextualCheckBlockHeader: %s, %s", __func__, hash.ToString(), FormatStateMessage(state));
    }
    if (pindex == NULL)
        pindex = AddToBlockIndex(block);

    if (ppindex)
        *ppindex = pindex;

    return true;
}

/** Store block on disk. If dbp is non-NULL, the file is known to already reside on disk */
static bool AcceptBlock(const CBlock& block, CValidationState& state, const CChainParams& chainparams, CBlockIndex** ppindex, bool fRequested, const CDiskBlockPos* dbp, bool* fNewBlock)
{
    if (fNewBlock)
        *fNewBlock = false;
    AssertLockHeld(cs_main);

    CBlockIndex* pindexDummy = NULL;
    CBlockIndex*& pindex = ppindex ? *ppindex : pindexDummy;

    if (!AcceptBlockHeader(block, state, chainparams, &pindex))
        return false;

    bool fAlreadyHave = pindex->nStatus & BLOCK_HAVE_DATA;
    bool fHasMoreWork = (chainActive.Tip() ? pindex->nChainWork > chainActive.Tip()->nChainWork : true);

    bool fTooFarAhead = (pindex->nHeight > int(chainActive.Height() + MIN_BLOCKS_TO_KEEP));

    if (fAlreadyHave)
        return true;
    if (!fRequested) { // If we didn't ask for it:
        if (pindex->nTx != 0)
            return true; // This is a previously-processed block that was pruned
        if (!fHasMoreWork)
            return true; // Don't process less-work chains
        if (fTooFarAhead)
            return true; // Block height is too high
    }
    if (fNewBlock)
        *fNewBlock = true;

    if ((!CheckBlock(block, state, chainparams.GetConsensus(), GetAdjustedTime())) || !ContextualCheckBlock(block, state, pindex->pprev)) {
        if (state.IsInvalid() && !state.CorruptionPossible()) {
            pindex->nStatus |= BLOCK_FAILED_VALID;
            setDirtyBlockIndex.insert(pindex);
        }
        return error("%s: %s", __func__, FormatStateMessage(state));
    }

    int nHeight = pindex->nHeight;

    try {
        unsigned int nBlockSize = ::GetSerializeSize(block, SER_DISK, CLIENT_VERSION);
        CDiskBlockPos blockPos;
        if (dbp != NULL)
            blockPos = *dbp;
        if (!FindBlockPos(state, blockPos, nBlockSize + 8, nHeight, block.GetBlockTime(), dbp != NULL))
            return error("AcceptBlock(): FindBlockPos failed");
        if (dbp == NULL)
            if (!WriteBlockToDisk(block, blockPos, chainparams.MessageStart()))
                AbortNode(state, "Failed to write block");
        if (!ReceivedBlockTransactions(block, state, pindex, blockPos))
            return error("AcceptBlock(): ReceivedBlockTransactions failed");
    }
    catch (const std::runtime_error& e) {
        return AbortNode(state, std::string("System error: ") + e.what());
    }

    if (fCheckForPruning)
        FlushStateToDisk(state, FLUSH_STATE_NONE); // we just allocated more disk space for block files

    return true;
}

static bool IsSuperMajority(int minVersion, const CBlockIndex* pstart, unsigned nRequired, const Consensus::Params& consensusParams)
{
    if (pstart->nHeight < (GetBoolArg("-testnet", false) ? 446500 : 434500))
        return false;

    unsigned int nFound = 0;
    for (int i = 0; i < consensusParams.nMajorityWindow && nFound < nRequired && pstart != NULL; i++) {
        if (pstart->nVersion >= minVersion)
            ++nFound;
        pstart = pstart->pprev;
    }
    return (nFound >= nRequired);
}

bool ProcessNewBlock(CValidationState& state, const CChainParams& chainparams, CNode* pfrom, const CBlock* pblock, bool fForceProcessing, const CDiskBlockPos* dbp)
{
    {
        LOCK(cs_main);
        bool fRequested = MarkBlockAsReceived(pblock->GetHash());
        fRequested |= fForceProcessing;

        CBlockIndex* pindex = NULL;
        bool fNewBlock = false;
        bool ret = AcceptBlock(*pblock, state, chainparams, &pindex, fRequested, dbp, &fNewBlock);
        if (pindex && pfrom) {
            mapBlockSource[pindex->GetBlockHash()] = pfrom->GetId();
            if (fNewBlock)
                pfrom->nLastBlockTime = GetTime();
        }
        CheckBlockIndex(chainparams.GetConsensus());
        if (!ret)
            return error("%s: AcceptBlock FAILED", __func__);
    }

    NotifyHeaderTip();

    if (!ActivateBestChain(state, chainparams, pblock))
        return error("%s: ActivateBestChain failed", __func__);

    if (!IsInitialBlockDownload()) {

        if (!CSyncCheckpoint::strMasterPrivKey.empty())
            Checkpoints::SendSyncCheckpoint(Checkpoints::AutoSelectSyncCheckpoint(), chainparams);
    }

    Checkpoints::AcceptPendingSyncCheckpoint(chainparams);

    return true;
}

bool TestBlockValidity(CValidationState& state, const CChainParams& chainparams, const CBlock& block, CBlockIndex* pindexPrev, bool fCheckPOW, bool fCheckMerkleRoot)
{
    AssertLockHeld(cs_main);
    assert(pindexPrev && pindexPrev == chainActive.Tip());
    if (fCheckpointsEnabled && !CheckIndexAgainstCheckpoint(pindexPrev, state, chainparams, block.GetHash()))
        return error("%s: CheckIndexAgainstCheckpoint(): %s", __func__, state.GetRejectReason().c_str());

    CCoinsViewCache viewNew(pcoinsTip);
    CBlockIndex indexDummy(block);
    indexDummy.pprev = pindexPrev;
    indexDummy.nHeight = pindexPrev->nHeight + 1;

    if (!ContextualCheckBlockHeader(block, state, chainparams.GetConsensus(), pindexPrev, GetAdjustedTime()))
        return error("%s: Consensus::ContextualCheckBlockHeader: %s", __func__, FormatStateMessage(state));
    if (!CheckBlock(block, state, chainparams.GetConsensus(), fCheckPOW, fCheckMerkleRoot))
        return error("%s: Consensus::CheckBlock: %s", __func__, FormatStateMessage(state));
    if (!ContextualCheckBlock(block, state, pindexPrev))
        return error("%s: Consensus::ContextualCheckBlock: %s", __func__, FormatStateMessage(state));
    if (!ConnectBlock(block, state, &indexDummy, viewNew, chainparams, true))
        return false;
    assert(state.IsValid());

    return true;
}

/**
 * BLOCK PRUNING CODE
 */

/* Calculate the amount of disk space the block & undo files currently use */
uint64_t CalculateCurrentUsage()
{
    uint64_t retval = 0;
    BOOST_FOREACH (const CBlockFileInfo& file, vinfoBlockFile) {
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

            std::pair<std::multimap<CBlockIndex*, CBlockIndex*>::iterator, std::multimap<CBlockIndex*, CBlockIndex*>::iterator> range = mapBlocksUnlinked.equal_range(pindex->pprev);
            while (range.first != range.second) {
                std::multimap<CBlockIndex*, CBlockIndex*>::iterator it = range.first;
                range.first++;
                if (it->second == pindex) {
                    mapBlocksUnlinked.erase(it);
                }
            }
        }
    }

    vinfoBlockFile[fileNumber].SetNull();
    setDirtyFileInfo.insert(fileNumber);
}

void UnlinkPrunedFiles(std::set<int>& setFilesToPrune)
{
    for (set<int>::iterator it = setFilesToPrune.begin(); it != setFilesToPrune.end(); ++it) {
        CDiskBlockPos pos(*it, 0);
        boost::filesystem::remove(GetBlockPosFilename(pos, "blk"));
        boost::filesystem::remove(GetBlockPosFilename(pos, "rev"));
        LogPrintf("Prune: %s deleted blk/rev (%05u)\n", __func__, *it);
    }
}

/* Calculate the block/rev files that should be deleted to remain under target*/
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

    uint64_t nBuffer = BLOCKFILE_CHUNK_SIZE + UNDOFILE_CHUNK_SIZE;
    uint64_t nBytesToPrune;
    int count = 0;

    if (nCurrentUsage + nBuffer >= nPruneTarget) {
        for (int fileNumber = 0; fileNumber < nLastBlockFile; fileNumber++) {
            nBytesToPrune = vinfoBlockFile[fileNumber].nSize + vinfoBlockFile[fileNumber].nUndoSize;

            if (vinfoBlockFile[fileNumber].nSize == 0)
                continue;

            if (nCurrentUsage + nBuffer < nPruneTarget) // are we below our target?
                break;

            if (vinfoBlockFile[fileNumber].nHeightLast > nLastBlockWeCanPrune)
                continue;

            PruneOneBlockFile(fileNumber);

            setFilesToPrune.insert(fileNumber);
            nCurrentUsage -= nBytesToPrune;
            count++;
        }
    }

    LogPrint("prune", "Prune: target=%dMiB actual=%dMiB diff=%dMiB max_prune_height=%d removed %d blk/rev pairs\n",
             nPruneTarget / 1024 / 1024, nCurrentUsage / 1024 / 1024,
             ((int64_t)nPruneTarget - (int64_t)nCurrentUsage) / 1024 / 1024,
             nLastBlockWeCanPrune, count);
}

bool CheckDiskSpace(uint64_t nAdditionalBytes)
{
    uint64_t nFreeBytesAvailable = boost::filesystem::space(GetDataDir()).available;

    if (nFreeBytesAvailable < nMinDiskSpace + nAdditionalBytes)
        return AbortNode("Disk space is low!", _("Error: Disk space is low!"));

    return true;
}

FILE* OpenDiskFile(const CDiskBlockPos& pos, const char* prefix, bool fReadOnly)
{
    if (pos.IsNull())
        return NULL;
    boost::filesystem::path path = GetBlockPosFilename(pos, prefix);
    boost::filesystem::create_directories(path.parent_path());
    FILE* file = fopen(path.string().c_str(), "rb+");
    if (!file && !fReadOnly)
        file = fopen(path.string().c_str(), "wb+");
    if (!file) {
        LogPrintf("Unable to open file %s\n", path.string());
        return NULL;
    }
    if (pos.nPos) {
        if (fseek(file, pos.nPos, SEEK_SET)) {
            LogPrintf("Unable to seek to position %u of %s\n", pos.nPos, path.string());
            fclose(file);
            return NULL;
        }
    }
    return file;
}

FILE* OpenBlockFile(const CDiskBlockPos& pos, bool fReadOnly)
{
    return OpenDiskFile(pos, "blk", fReadOnly);
}

FILE* OpenUndoFile(const CDiskBlockPos& pos, bool fReadOnly)
{
    return OpenDiskFile(pos, "rev", fReadOnly);
}

boost::filesystem::path GetBlockPosFilename(const CDiskBlockPos& pos, const char* prefix)
{
    return GetDataDir() / "blocks" / strprintf("%s%05u.dat", prefix, pos.nFile);
}

CBlockIndex* InsertBlockIndex(uint256 hash)
{
    if (hash.IsNull())
        return NULL;

    BlockMap::iterator mi = mapBlockIndex.find(hash);
    if (mi != mapBlockIndex.end())
        return (*mi).second;

    CBlockIndex* pindexNew = new CBlockIndex();
    if (!pindexNew)
        throw runtime_error(std::string(__func__) + ": new CBlockIndex failed");
    mi = mapBlockIndex.insert(make_pair(hash, pindexNew)).first;
    pindexNew->phashBlock = &((*mi).first);

    return pindexNew;
}

bool static LoadBlockIndexDB()
{
    const CChainParams& chainparams = Params();
    if (!pblocktree->LoadBlockIndexGuts(InsertBlockIndex))
        return false;

    boost::this_thread::interruption_point();

    vector<pair<int, CBlockIndex*> > vSortedByHeight;
    vSortedByHeight.reserve(mapBlockIndex.size());
    BOOST_FOREACH (const PAIRTYPE(uint256, CBlockIndex*)&item, mapBlockIndex) {
        CBlockIndex* pindex = item.second;
        vSortedByHeight.push_back(make_pair(pindex->nHeight, pindex));
    }
    sort(vSortedByHeight.begin(), vSortedByHeight.end());
    BOOST_FOREACH (const PAIRTYPE(int, CBlockIndex*)&item, vSortedByHeight) {
        CBlockIndex* pindex = item.second;
        pindex->nChainWork = (pindex->pprev ? pindex->pprev->nChainWork : 0) + GetBlockProof(*pindex);

        if (pindex->nTx > 0) {
            if (pindex->pprev) {
                if (pindex->pprev->nChainTx) {
                    pindex->nChainTx = pindex->pprev->nChainTx + pindex->nTx;
                } else {
                    pindex->nChainTx = 0;
                    mapBlocksUnlinked.insert(std::make_pair(pindex->pprev, pindex));
                }
            } else {
                pindex->nChainTx = pindex->nTx;
            }
        }
        if (pindex->IsValid(BLOCK_VALID_TRANSACTIONS) && (pindex->nChainTx || pindex->pprev == NULL))
            setBlockIndexCandidates.insert(pindex);
        if (pindex->nStatus & BLOCK_FAILED_MASK && (!pindexBestInvalid || pindex->nChainWork > pindexBestInvalid->nChainWork))
            pindexBestInvalid = pindex;
        if (pindex->pprev)
            pindex->BuildSkip();
        if (pindex->IsValid(BLOCK_VALID_TREE) && (pindexBestHeader == NULL || CBlockIndexWorkComparator()(pindexBestHeader, pindex)))
            pindexBestHeader = pindex;
    }

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

    LogPrintf("Checking all blk files are present...\n");
    set<int> setBlkDataFiles;
    BOOST_FOREACH (const PAIRTYPE(uint256, CBlockIndex*)&item, mapBlockIndex) {
        CBlockIndex* pindex = item.second;
        if (pindex->nStatus & BLOCK_HAVE_DATA) {
            setBlkDataFiles.insert(pindex->nFile);
        }
    }
    for (std::set<int>::iterator it = setBlkDataFiles.begin(); it != setBlkDataFiles.end(); it++) {
        CDiskBlockPos pos(*it, 0);
        if (CAutoFile(OpenBlockFile(pos, true), SER_DISK, CLIENT_VERSION).IsNull()) {
            return false;
        }
    }

    pblocktree->ReadFlag("prunedblockfiles", fHavePruned);
    if (fHavePruned)
        LogPrintf("LoadBlockIndexDB(): Block files have previously been pruned\n");

    bool fReindexing = false;
    pblocktree->ReadReindexing(fReindexing);
    fReindex |= fReindexing;

    pblocktree->ReadFlag("txindex", fTxIndex);
    LogPrintf("%s: transaction index %s\n", __func__, fTxIndex ? "enabled" : "disabled");

    BlockMap::iterator it = mapBlockIndex.find(pcoinsTip->GetBestBlock());
    if (it == mapBlockIndex.end())
        return true;
    chainActive.SetTip(it->second);

    PruneBlockIndexCandidates();

    if (boost::filesystem::exists(GetDataDir() / "checkpoints")) {
        boost::filesystem::remove_all(GetDataDir() / "checkpoints");
    }

    Checkpoints::ReadSyncCheckpoint(Checkpoints::hashSyncCheckpoint);
    LogPrintf("LoadBlockIndexDB(): using synchronized checkpoint %s\n", Checkpoints::hashSyncCheckpoint.ToString().c_str());

    LogPrintf("%s: hashBestChain=%s height=%d date=%s progress=%f\n", __func__,
              chainActive.Tip()->GetBlockHash().ToString(), chainActive.Height(),
              DateTimeStrFormat("%Y-%m-%d %H:%M:%S", chainActive.Tip()->GetBlockTime()),
              Checkpoints::GuessVerificationProgress(chainparams.Checkpoints(), chainActive.Tip()));

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

bool CVerifyDB::VerifyDB(const CChainParams& chainparams, CCoinsView* coinsview, int nCheckLevel, int nCheckDepth)
{
    LOCK(cs_main);
    if (chainActive.Tip() == NULL || chainActive.Tip()->pprev == NULL)
        return true;

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
    LogPrintf("[0%]...");
    for (CBlockIndex* pindex = chainActive.Tip(); pindex && pindex->pprev; pindex = pindex->pprev) {
        boost::this_thread::interruption_point();
        int percentageDone = std::max(1, std::min(99, (int)(((double)(chainActive.Height() - pindex->nHeight)) / (double)nCheckDepth * (nCheckLevel >= 4 ? 50 : 100))));
        if (reportDone < percentageDone / 10) {

            LogPrintf("[%d%%]...", percentageDone);
            reportDone = percentageDone / 10;
        }
        uiInterface.ShowProgress(_("Verifying blocks..."), percentageDone);
        if (pindex->nHeight < chainActive.Height() - nCheckDepth)
            break;
        if (fPruneMode && !(pindex->nStatus & BLOCK_HAVE_DATA)) {

            LogPrintf("VerifyDB(): block verification stopping at height %d (pruning, no data)\n", pindex->nHeight);
            break;
        }
        CBlock block;

        if (!ReadBlockFromDisk(block, pindex, chainparams.GetConsensus()))
            return error("VerifyDB(): *** ReadBlockFromDisk failed at %d, hash=%s", pindex->nHeight, pindex->GetBlockHash().ToString());

        if (nCheckLevel >= 1 && !CheckBlock(block, state, chainparams.GetConsensus()))
            return error("%s: *** found bad block at %d, hash=%s (%s)\n", __func__,
                         pindex->nHeight, pindex->GetBlockHash().ToString(), FormatStateMessage(state));

        if (nCheckLevel >= 2 && pindex) {
            CBlockUndo undo;
            CDiskBlockPos pos = pindex->GetUndoPos();
            if (!pos.IsNull()) {
                if (!UndoReadFromDisk(undo, pos, pindex->pprev->GetBlockHash()))
                    return error("VerifyDB(): *** found bad undo data at %d, hash=%s\n", pindex->nHeight, pindex->GetBlockHash().ToString());
            }
        }

        if (nCheckLevel >= 3 && pindex == pindexState && (coins.DynamicMemoryUsage() + pcoinsTip->DynamicMemoryUsage()) <= nCoinCacheUsage) {
            bool fClean = true;
            if (!DisconnectBlock(block, state, pindex, coins, &fClean))
                return error("VerifyDB(): *** irrecoverable inconsistency in block data at %d, hash=%s", pindex->nHeight, pindex->GetBlockHash().ToString());
            pindexState = pindex->pprev;
            if (!fClean) {
                nGoodTransactions = 0;
                pindexFailure = pindex;
            } else
                nGoodTransactions += block.vtx.size();
        }
        if (ShutdownRequested())
            return true;
    }
    if (pindexFailure)
        return error("VerifyDB(): *** coin database inconsistencies found (last %i blocks, %i good transactions before that)\n", chainActive.Height() - pindexFailure->nHeight + 1, nGoodTransactions);

    if (nCheckLevel >= 4) {
        CBlockIndex* pindex = pindexState;
        while (pindex != chainActive.Tip()) {
            boost::this_thread::interruption_point();
            uiInterface.ShowProgress(_("Verifying blocks..."), std::max(1, std::min(99, 100 - (int)(((double)(chainActive.Height() - pindex->nHeight)) / (double)nCheckDepth * 50))));
            pindex = chainActive.Next(pindex);
            CBlock block;
            if (!ReadBlockFromDisk(block, pindex, chainparams.GetConsensus()))
                return error("VerifyDB(): *** ReadBlockFromDisk failed at %d, hash=%s", pindex->nHeight, pindex->GetBlockHash().ToString());
            if (!ConnectBlock(block, state, pindex, coins, chainparams))
                return error("VerifyDB(): *** found unconnectable block at %d, hash=%s", pindex->nHeight, pindex->GetBlockHash().ToString());
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
    while (nHeight <= chainActive.Height()) {
        if (IsWitnessEnabled(chainActive[nHeight - 1], params.GetConsensus()) && !(chainActive[nHeight]->nStatus & BLOCK_OPT_WITNESS)) {
            break;
        }
        nHeight++;
    }

    CValidationState state;
    CBlockIndex* pindex = chainActive.Tip();
    while (chainActive.Height() >= nHeight) {
        if (fPruneMode && !(chainActive.Tip()->nStatus & BLOCK_HAVE_DATA)) {

            break;
        }
        if (!DisconnectTip(state, params, true)) {
            return error("RewindBlockIndex: unable to disconnect block at height %i", pindex->nHeight);
        }

        if (!FlushStateToDisk(state, FLUSH_STATE_PERIODIC))
            return false;
    }

    for (BlockMap::iterator it = mapBlockIndex.begin(); it != mapBlockIndex.end(); it++) {
        CBlockIndex* pindexIter = it->second;

        if (IsWitnessEnabled(pindexIter->pprev, params.GetConsensus()) && !(pindexIter->nStatus & BLOCK_OPT_WITNESS) && !chainActive.Contains(pindexIter)) {

            pindexIter->nStatus = std::min<unsigned int>(pindexIter->nStatus & BLOCK_VALID_MASK, BLOCK_VALID_TREE) | (pindexIter->nStatus & ~BLOCK_VALID_MASK);

            pindexIter->nStatus &= ~(BLOCK_HAVE_DATA | BLOCK_HAVE_UNDO);

            pindexIter->nFile = 0;
            pindexIter->nDataPos = 0;
            pindexIter->nUndoPos = 0;

            pindexIter->nTx = 0;
            pindexIter->nChainTx = 0;
            pindexIter->nSequenceId = 0;

            setDirtyBlockIndex.insert(pindexIter);

            setBlockIndexCandidates.erase(pindexIter);
            std::pair<std::multimap<CBlockIndex*, CBlockIndex*>::iterator, std::multimap<CBlockIndex*, CBlockIndex*>::iterator> ret = mapBlocksUnlinked.equal_range(pindexIter->pprev);
            while (ret.first != ret.second) {
                if (ret.first->second == pindexIter) {
                    mapBlocksUnlinked.erase(ret.first++);
                } else {
                    ++ret.first;
                }
            }
        } else if (pindexIter->IsValid(BLOCK_VALID_TRANSACTIONS) && pindexIter->nChainTx) {
            setBlockIndexCandidates.insert(pindexIter);
        }
    }

    PruneBlockIndexCandidates();

    CheckBlockIndex(params.GetConsensus());

    if (!FlushStateToDisk(state, FLUSH_STATE_ALWAYS)) {
        return false;
    }

    return true;
}

void UnloadBlockIndex()
{
    LOCK(cs_main);
    setBlockIndexCandidates.clear();
    chainActive.SetTip(NULL);
    pindexBestInvalid = NULL;
    pindexBestHeader = NULL;
    mempool.clear();
    mapOrphanTransactions.clear();
    mapOrphanTransactionsByPrev.clear();
    nSyncStarted = 0;
    mapBlocksUnlinked.clear();
    vinfoBlockFile.clear();
    nLastBlockFile = 0;
    nBlockSequenceId = 1;
    mapBlockSource.clear();
    mapBlocksInFlight.clear();
    nPreferredDownload = 0;
    setDirtyBlockIndex.clear();
    setDirtyFileInfo.clear();
    mapNodeState.clear();
    recentRejects.reset(NULL);
    versionbitscache.Clear();
    for (int b = 0; b < VERSIONBITS_NUM_BITS; b++) {
        warningcache[b].clear();
    }

    BOOST_FOREACH (BlockMap::value_type& entry, mapBlockIndex) {
        delete entry.second;
    }
    mapBlockIndex.clear();
    fHavePruned = false;
}

bool LoadBlockIndex()
{

    if (!fReindex && !LoadBlockIndexDB())
        return false;
    return true;
}

bool InitBlockIndex(const CChainParams& chainparams)
{
    LOCK(cs_main);

    recentRejects.reset(new CRollingBloomFilter(120000, 0.000001));

    if (chainActive.Genesis() != NULL)
        return true;

    LogPrintf("Wrote sync checkpoint...\n");

    fTxIndex = GetBoolArg("-txindex", DEFAULT_TXINDEX);
    pblocktree->WriteFlag("txindex", fTxIndex);
    LogPrintf("Initializing databases...\n");

    if (!fReindex) {
        try {
            CBlock& block = const_cast<CBlock&>(chainparams.GenesisBlock());

            unsigned int nBlockSize = ::GetSerializeSize(block, SER_DISK, CLIENT_VERSION);
            CDiskBlockPos blockPos;
            CValidationState state;
            if (!FindBlockPos(state, blockPos, nBlockSize + 8, 0, block.GetBlockTime()))
                return error("LoadBlockIndex(): FindBlockPos failed");
            if (!WriteBlockToDisk(block, blockPos, chainparams.MessageStart()))
                return error("LoadBlockIndex(): writing genesis block to disk failed");
            CBlockIndex* pindex = AddToBlockIndex(block);
            if (!ReceivedBlockTransactions(block, state, pindex, blockPos))
                return error("LoadBlockIndex(): genesis block not accepted");

            if (!Checkpoints::WriteSyncCheckpoint(Params().GenesisBlock().GetHash()))
                return error("LoadBlockIndex() : failed to init sync checkpoint");
            std::string strPubKey;
            std::string strPubKeyComp = GetBoolArg("-testnet", false) ? CSyncCheckpoint::strMasterPubKeyTestnet : CSyncCheckpoint::strMasterPubKey;
            if (!Checkpoints::ReadCheckpointPubKey(strPubKey) || strPubKey != strPubKeyComp) {

                if (!Checkpoints::WriteCheckpointPubKey(strPubKeyComp))
                    return error("LoadBlockIndex() : failed to write new checkpoint master key to db");
                if (!Checkpoints::ResetSyncCheckpoint(chainparams))
                    return error("LoadBlockIndex() : failed to reset sync-checkpoint");
            }

            return FlushStateToDisk(state, FLUSH_STATE_ALWAYS);
        }
        catch (const std::runtime_error& e) {
            return error("LoadBlockIndex(): failed to initialize block database: %s", e.what());
        }
    }

    return true;
}

bool LoadExternalBlockFile(const CChainParams& chainparams, FILE* fileIn, CDiskBlockPos* dbp)
{

    static std::multimap<uint256, CDiskBlockPos> mapBlocksUnknownParent;
    int64_t nStart = GetTimeMillis();

    int nLoaded = 0;
    try {

        CBufferedFile blkdat(fileIn, 2 * MAX_BLOCK_SERIALIZED_SIZE, MAX_BLOCK_SERIALIZED_SIZE + 8, SER_DISK, CLIENT_VERSION);
        uint64_t nRewind = blkdat.GetPos();
        while (!blkdat.eof()) {
            boost::this_thread::interruption_point();

            blkdat.SetPos(nRewind);
            nRewind++; // start one byte further next time, in case of failure
            blkdat.SetLimit(); // remove former limit
            unsigned int nSize = 0;
            try {

                unsigned char buf[MESSAGE_START_SIZE];
                blkdat.FindByte(chainparams.MessageStart()[0]);
                nRewind = blkdat.GetPos() + 1;
                blkdat >> FLATDATA(buf);
                if (memcmp(buf, chainparams.MessageStart(), MESSAGE_START_SIZE))
                    continue;

                blkdat >> nSize;
                if (nSize < 80 || nSize > MAX_BLOCK_SERIALIZED_SIZE)
                    continue;
            }
            catch (const std::exception&) {

                break;
            }
            try {

                uint64_t nBlockPos = blkdat.GetPos();
                if (dbp)
                    dbp->nPos = nBlockPos;
                blkdat.SetLimit(nBlockPos + nSize);
                blkdat.SetPos(nBlockPos);
                CBlock block;
                blkdat >> block;
                nRewind = blkdat.GetPos();

                uint256 hash = block.GetHash();
                if (hash != chainparams.GetConsensus().hashGenesisBlock && mapBlockIndex.find(block.hashPrevBlock) == mapBlockIndex.end()) {
                    LogPrint("reindex", "%s: Out of order block %s, parent %s not known\n", __func__, hash.ToString(),
                             block.hashPrevBlock.ToString());
                    if (dbp)
                        mapBlocksUnknownParent.insert(std::make_pair(block.hashPrevBlock, *dbp));
                    continue;
                }

                if (mapBlockIndex.count(hash) == 0 || (mapBlockIndex[hash]->nStatus & BLOCK_HAVE_DATA) == 0) {
                    LOCK(cs_main);
                    CValidationState state;
                    if (AcceptBlock(block, state, chainparams, NULL, true, dbp, NULL))
                        nLoaded++;
                    if (state.IsError())
                        break;
                } else if (hash != chainparams.GetConsensus().hashGenesisBlock && mapBlockIndex[hash]->nHeight % 1000 == 0) {
                    LogPrint("reindex", "Block Import: already had block %s at height %d\n", hash.ToString(), mapBlockIndex[hash]->nHeight);
                }

                if (hash == chainparams.GetConsensus().hashGenesisBlock) {
                    CValidationState state;
                    if (!ActivateBestChain(state, chainparams)) {
                        break;
                    }
                }

                NotifyHeaderTip();

                deque<uint256> queue;
                queue.push_back(hash);
                while (!queue.empty()) {
                    uint256 head = queue.front();
                    queue.pop_front();
                    std::pair<std::multimap<uint256, CDiskBlockPos>::iterator, std::multimap<uint256, CDiskBlockPos>::iterator> range = mapBlocksUnknownParent.equal_range(head);
                    while (range.first != range.second) {
                        std::multimap<uint256, CDiskBlockPos>::iterator it = range.first;
                        if (ReadBlockFromDisk(block, it->second, chainparams.GetConsensus())) {
                            LogPrint("reindex", "%s: Processing out of order child %s of %s\n", __func__, block.GetHash().ToString(),
                                     head.ToString());
                            LOCK(cs_main);
                            CValidationState dummy;
                            if (AcceptBlock(block, dummy, chainparams, NULL, true, &it->second, NULL)) {
                                nLoaded++;
                                queue.push_back(block.GetHash());
                            }
                        }
                        range.first++;
                        mapBlocksUnknownParent.erase(it);
                        NotifyHeaderTip();
                    }
                }
            }
            catch (const std::exception& e) {
                LogPrintf("%s: Deserialize or I/O error - %s\n", __func__, e.what());
            }
        }
    }
    catch (const std::runtime_error& e) {
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

    if (chainActive.Height() < 0) {
        assert(mapBlockIndex.size() <= 1);
        return;
    }

    std::multimap<CBlockIndex*, CBlockIndex*> forward;
    for (BlockMap::iterator it = mapBlockIndex.begin(); it != mapBlockIndex.end(); it++) {
        forward.insert(std::make_pair(it->second->pprev, it->second));
    }

    assert(forward.size() == mapBlockIndex.size());

    std::pair<std::multimap<CBlockIndex*, CBlockIndex*>::iterator, std::multimap<CBlockIndex*, CBlockIndex*>::iterator> rangeGenesis = forward.equal_range(NULL);
    CBlockIndex* pindex = rangeGenesis.first->second;
    rangeGenesis.first++;
    assert(rangeGenesis.first == rangeGenesis.second); // There is only one index entry with parent NULL.

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
        if (pindexFirstInvalid == NULL && pindex->nStatus & BLOCK_FAILED_VALID)
            pindexFirstInvalid = pindex;
        if (pindexFirstMissing == NULL && !(pindex->nStatus & BLOCK_HAVE_DATA))
            pindexFirstMissing = pindex;
        if (pindexFirstNeverProcessed == NULL && pindex->nTx == 0)
            pindexFirstNeverProcessed = pindex;
        if (pindex->pprev != NULL && pindexFirstNotTreeValid == NULL && (pindex->nStatus & BLOCK_VALID_MASK) < BLOCK_VALID_TREE)
            pindexFirstNotTreeValid = pindex;
        if (pindex->pprev != NULL && pindexFirstNotTransactionsValid == NULL && (pindex->nStatus & BLOCK_VALID_MASK) < BLOCK_VALID_TRANSACTIONS)
            pindexFirstNotTransactionsValid = pindex;
        if (pindex->pprev != NULL && pindexFirstNotChainValid == NULL && (pindex->nStatus & BLOCK_VALID_MASK) < BLOCK_VALID_CHAIN)
            pindexFirstNotChainValid = pindex;
        if (pindex->pprev != NULL && pindexFirstNotScriptsValid == NULL && (pindex->nStatus & BLOCK_VALID_MASK) < BLOCK_VALID_SCRIPTS)
            pindexFirstNotScriptsValid = pindex;

        if (pindex->pprev == NULL) {

            assert(pindex->GetBlockHash() == consensusParams.hashGenesisBlock); // Genesis block's hash must match.
            assert(pindex == chainActive.Genesis()); // The current active chain's genesis block must be this block.
        }
        if (pindex->nChainTx == 0)
            assert(pindex->nSequenceId == 0); // nSequenceId can't be set for blocks that aren't linked

        if (!fHavePruned) {

            assert(!(pindex->nStatus & BLOCK_HAVE_DATA) == (pindex->nTx == 0));
            assert(pindexFirstMissing == pindexFirstNeverProcessed);
        } else {

            if (pindex->nStatus & BLOCK_HAVE_DATA)
                assert(pindex->nTx > 0);
        }
        if (pindex->nStatus & BLOCK_HAVE_UNDO)
            assert(pindex->nStatus & BLOCK_HAVE_DATA);
        assert(((pindex->nStatus & BLOCK_VALID_MASK) >= BLOCK_VALID_TRANSACTIONS) == (pindex->nTx > 0)); // This is pruning-independent.

        assert((pindexFirstNeverProcessed != NULL) == (pindex->nChainTx == 0)); // nChainTx != 0 is used to signal that all parent blocks have been processed (but may have been pruned).
        assert((pindexFirstNotTransactionsValid != NULL) == (pindex->nChainTx == 0));
        assert(pindex->nHeight == nHeight); // nHeight must be consistent.
        assert(pindex->pprev == NULL || pindex->nChainWork >= pindex->pprev->nChainWork); // For every block except the genesis block, the chainwork must be larger than the parent's.
        assert(nHeight < 2 || (pindex->pskip && (pindex->pskip->nHeight < nHeight))); // The pskip pointer must point back for all but the first 2 blocks.
        assert(pindexFirstNotTreeValid == NULL); // All mapBlockIndex entries must at least be TREE valid
        if ((pindex->nStatus & BLOCK_VALID_MASK) >= BLOCK_VALID_TREE)
            assert(pindexFirstNotTreeValid == NULL); // TREE valid implies all parents are TREE valid
        if ((pindex->nStatus & BLOCK_VALID_MASK) >= BLOCK_VALID_CHAIN)
            assert(pindexFirstNotChainValid == NULL); // CHAIN valid implies all parents are CHAIN valid
        if ((pindex->nStatus & BLOCK_VALID_MASK) >= BLOCK_VALID_SCRIPTS)
            assert(pindexFirstNotScriptsValid == NULL); // SCRIPTS valid implies all parents are SCRIPTS valid
        if (pindexFirstInvalid == NULL) {

            assert((pindex->nStatus & BLOCK_FAILED_MASK) == 0); // The failed mask cannot be set for blocks without invalid parents.
        }
        if (!CBlockIndexWorkComparator()(pindex, chainActive.Tip()) && pindexFirstNeverProcessed == NULL) {
            if (pindexFirstInvalid == NULL) {

                if (pindexFirstMissing == NULL || pindex == chainActive.Tip()) {
                    assert(setBlockIndexCandidates.count(pindex));
                }
            }
        } else { // If this block sorts worse than the current tip or some ancestor's block has never been seen, it cannot be in setBlockIndexCandidates.
            assert(setBlockIndexCandidates.count(pindex) == 0);
        }

        std::pair<std::multimap<CBlockIndex*, CBlockIndex*>::iterator, std::multimap<CBlockIndex*, CBlockIndex*>::iterator> rangeUnlinked = mapBlocksUnlinked.equal_range(pindex->pprev);
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

            assert(foundInUnlinked);
        }
        if (!(pindex->nStatus & BLOCK_HAVE_DATA))
            assert(!foundInUnlinked); // Can't be in mapBlocksUnlinked if we don't HAVE_DATA
        if (pindexFirstMissing == NULL)
            assert(!foundInUnlinked); // We aren't missing data for any parent -- cannot be in mapBlocksUnlinked.
        if (pindex->pprev && (pindex->nStatus & BLOCK_HAVE_DATA) && pindexFirstNeverProcessed == NULL && pindexFirstMissing != NULL) {

            assert(fHavePruned); // We must have pruned.

            if (!CBlockIndexWorkComparator()(pindex, chainActive.Tip()) && setBlockIndexCandidates.count(pindex) == 0) {
                if (pindexFirstInvalid == NULL) {
                    assert(foundInUnlinked);
                }
            }
        }

        std::pair<std::multimap<CBlockIndex*, CBlockIndex*>::iterator, std::multimap<CBlockIndex*, CBlockIndex*>::iterator> range = forward.equal_range(pindex);
        if (range.first != range.second) {

            pindex = range.first->second;
            nHeight++;
            continue;
        }

        while (pindex) {

            if (pindex == pindexFirstInvalid)
                pindexFirstInvalid = NULL;
            if (pindex == pindexFirstMissing)
                pindexFirstMissing = NULL;
            if (pindex == pindexFirstNeverProcessed)
                pindexFirstNeverProcessed = NULL;
            if (pindex == pindexFirstNotTreeValid)
                pindexFirstNotTreeValid = NULL;
            if (pindex == pindexFirstNotTransactionsValid)
                pindexFirstNotTransactionsValid = NULL;
            if (pindex == pindexFirstNotChainValid)
                pindexFirstNotChainValid = NULL;
            if (pindex == pindexFirstNotScriptsValid)
                pindexFirstNotScriptsValid = NULL;

            CBlockIndex* pindexPar = pindex->pprev;

            std::pair<std::multimap<CBlockIndex*, CBlockIndex*>::iterator, std::multimap<CBlockIndex*, CBlockIndex*>::iterator> rangePar = forward.equal_range(pindexPar);
            while (rangePar.first->second != pindex) {
                assert(rangePar.first != rangePar.second); // Our parent must have at least the node we're coming from as child.
                rangePar.first++;
            }

            rangePar.first++;
            if (rangePar.first != rangePar.second) {

                pindex = rangePar.first->second;
                break;
            } else {

                pindex = pindexPar;
                nHeight--;
                continue;
            }
        }
    }

    assert(nNodes == forward.size());
}

std::string GetWarnings(const std::string& strFor)
{
    int nPriority = 0;
    string strStatusBar;
    string strRPC;
    string strGUI;
    const string uiAlertSeperator = "<hr />";

    if (!CLIENT_VERSION_IS_RELEASE) {
        strStatusBar = "This is a pre-release test build - use at your own risk - do not use for mining or merchant applications";
        strGUI = _("This is a pre-release test build - use at your own risk - do not use for mining or merchant applications");
    }

    if (GetBoolArg("-testsafemode", DEFAULT_TESTSAFEMODE))
        strStatusBar = strRPC = strGUI = "testsafemode enabled";

    if (strMiscWarning != "") {
        nPriority = 1000;
        strStatusBar = strMiscWarning;
        strGUI += (strGUI.empty() ? "" : uiAlertSeperator) + strMiscWarning;
    }

    if (fLargeWorkForkFound) {
        nPriority = 2000;
        strStatusBar = strRPC = "Warning: The network does not appear to fully agree! Some miners appear to be experiencing issues.";
        strGUI += (strGUI.empty() ? "" : uiAlertSeperator) + _("Warning: The network does not appear to fully agree! Some miners appear to be experiencing issues.");
    } else if (fLargeWorkInvalidChainFound) {
        nPriority = 2000;
        strStatusBar = strRPC = "Warning: We do not appear to fully agree with our peers! You may need to upgrade, or other nodes may need to upgrade.";
        strGUI += (strGUI.empty() ? "" : uiAlertSeperator) + _("Warning: We do not appear to fully agree with our peers! You may need to upgrade, or other nodes may need to upgrade.");
    }

    if (Checkpoints::IsSyncCheckpointTooOld(2 * 60 * 60)) {
        nPriority = 100;
        strStatusBar = "WARNING: Checkpoint is too old, please wait for a new checkpoint to arrive before engaging in any transactions.";
    }

    if (Checkpoints::hashInvalidCheckpoint != uint256()) {
        nPriority = 3000;
        strStatusBar = strRPC = "WARNING: Invalid checkpoint found! Displayed transactions may not be correct! You may need to upgrade, or notify developers of the issue.";
    }

    {
        LOCK(cs_mapAlerts);
        BOOST_FOREACH (PAIRTYPE(const uint256, CAlert) & item, mapAlerts) {
            const CAlert& alert = item.second;
            if (alert.AppliesToMe() && alert.nPriority > nPriority) {
                nPriority = alert.nPriority;
                strStatusBar = alert.strStatusBar;
            }
        }
    }

    if (strFor == "gui")
        return strGUI;
    else if (strFor == "statusbar")
        return strStatusBar;
    else if (strFor == "rpc")
        return strRPC;
    assert(!"GetWarnings(): invalid parameter");
    return "error";
}

bool static AlreadyHave(const CInv& inv) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    switch (inv.type) {
    case MSG_TX:
    case MSG_WITNESS_TX: {
        assert(recentRejects);
        if (chainActive.Tip()->GetBlockHash() != hashRecentRejectsChainTip) {

            hashRecentRejectsChainTip = chainActive.Tip()->GetBlockHash();
            recentRejects->reset();
        }

        return recentRejects->contains(inv.hash) || mempool.exists(inv.hash) || mapOrphanTransactions.count(inv.hash) || pcoinsTip->HaveCoinsInCache(inv.hash);
    }
    case MSG_BLOCK:
    case MSG_WITNESS_BLOCK:
        return mapBlockIndex.count(inv.hash);
    }

    return true;
}

void static ProcessGetData(CNode* pfrom, const Consensus::Params& consensusParams)
{
    std::deque<CInv>::iterator it = pfrom->vRecvGetData.begin();

    vector<CInv> vNotFound;

    LOCK(cs_main);

    while (it != pfrom->vRecvGetData.end()) {

        if (pfrom->nSendSize >= SendBufferSize())
            break;

        const CInv& inv = *it;
        {
            boost::this_thread::interruption_point();
            it++;

            if (inv.type == MSG_BLOCK || inv.type == MSG_FILTERED_BLOCK || inv.type == MSG_CMPCT_BLOCK || inv.type == MSG_WITNESS_BLOCK) {
                bool send = false;
                BlockMap::iterator mi = mapBlockIndex.find(inv.hash);
                if (mi != mapBlockIndex.end()) {
                    if (chainActive.Contains(mi->second)) {
                        send = true;
                    } else {
                        static const int nOneMonth = 30 * 24 * 60 * 60;

                        send = mi->second->IsValid(BLOCK_VALID_SCRIPTS) && (pindexBestHeader != NULL) && (pindexBestHeader->GetBlockTime() - mi->second->GetBlockTime() < nOneMonth) && (GetBlockProofEquivalentTime(*pindexBestHeader, *mi->second, *pindexBestHeader, consensusParams) < nOneMonth);
                        if (!send) {
                            LogPrintf("%s: ignoring request from peer=%i for old block that isn't in the main chain\n", __func__, pfrom->GetId());
                        }
                    }
                }

                static const int nOneWeek = 7 * 24 * 60 * 60; // assume > 1 week = historical
                if (send && CNode::OutboundTargetReached(true) && (((pindexBestHeader != NULL) && (pindexBestHeader->GetBlockTime() - mi->second->GetBlockTime() > nOneWeek)) || inv.type == MSG_FILTERED_BLOCK) && !pfrom->fWhitelisted) {
                    LogPrint("net", "historical block serving limit reached, disconnect peer=%d\n", pfrom->GetId());

                    pfrom->fDisconnect = true;
                    send = false;
                }

                if (send && (mi->second->nStatus & BLOCK_HAVE_DATA)) {

                    CBlock block;
                    if (!ReadBlockFromDisk(block, (*mi).second, consensusParams))
                        assert(!"cannot load block from disk");
                    if (inv.type == MSG_BLOCK)
                        pfrom->PushMessageWithFlag(SERIALIZE_TRANSACTION_NO_WITNESS, NetMsgType::BLOCK, block);
                    else if (inv.type == MSG_WITNESS_BLOCK)
                        pfrom->PushMessage(NetMsgType::BLOCK, block);
                    else if (inv.type == MSG_FILTERED_BLOCK) {
                        bool send = false;
                        CMerkleBlock merkleBlock;
                        {
                            LOCK(pfrom->cs_filter);
                            if (pfrom->pfilter) {
                                send = true;
                                merkleBlock = CMerkleBlock(block, *pfrom->pfilter);
                            }
                        }
                        if (send) {
                            pfrom->PushMessage(NetMsgType::MERKLEBLOCK, merkleBlock);

                            typedef std::pair<unsigned int, uint256> PairType;
                            BOOST_FOREACH (PairType& pair, merkleBlock.vMatchedTxn)
                                pfrom->PushMessageWithFlag(SERIALIZE_TRANSACTION_NO_WITNESS, NetMsgType::TX, block.vtx[pair.first]);
                        }

                    } else if (inv.type == MSG_CMPCT_BLOCK) {

                        if (mi->second->nHeight >= chainActive.Height() - 10) {
                            CBlockHeaderAndShortTxIDs cmpctblock(block);
                            pfrom->PushMessageWithFlag(SERIALIZE_TRANSACTION_NO_WITNESS, NetMsgType::CMPCTBLOCK, cmpctblock);
                        } else
                            pfrom->PushMessageWithFlag(SERIALIZE_TRANSACTION_NO_WITNESS, NetMsgType::BLOCK, block);
                    }

                    if (inv.hash == pfrom->hashContinue) {

                        vector<CInv> vInv;
                        vInv.push_back(CInv(MSG_BLOCK, chainActive.Tip()->GetBlockHash()));
                        pfrom->PushMessage(NetMsgType::INV, vInv);
                        pfrom->hashContinue.SetNull();
                    }
                }
            } else if (inv.type == MSG_TX || inv.type == MSG_WITNESS_TX) {

                bool push = false;
                auto mi = mapRelay.find(inv.hash);
                if (mi != mapRelay.end()) {
                    pfrom->PushMessageWithFlag(inv.type == MSG_TX ? SERIALIZE_TRANSACTION_NO_WITNESS : 0, NetMsgType::TX, *mi->second);
                    push = true;
                } else if (pfrom->timeLastMempoolReq) {
                    auto txinfo = mempool.info(inv.hash);

                    if (txinfo.tx && txinfo.nTime <= pfrom->timeLastMempoolReq) {
                        pfrom->PushMessageWithFlag(inv.type == MSG_TX ? SERIALIZE_TRANSACTION_NO_WITNESS : 0, NetMsgType::TX, *txinfo.tx);
                        push = true;
                    }
                }
                if (!push) {
                    vNotFound.push_back(inv);
                }
            }

            GetMainSignals().Inventory(inv.hash);

            if (inv.type == MSG_BLOCK || inv.type == MSG_FILTERED_BLOCK || inv.type == MSG_CMPCT_BLOCK || inv.type == MSG_WITNESS_BLOCK)
                break;
        }
    }

    pfrom->vRecvGetData.erase(pfrom->vRecvGetData.begin(), it);

    if (!vNotFound.empty()) {

        pfrom->PushMessage(NetMsgType::NOTFOUND, vNotFound);
    }
}

uint32_t GetFetchFlags(CNode* pfrom, CBlockIndex* pprev, const Consensus::Params& chainparams)
{
    uint32_t nFetchFlags = 0;
    if (IsWitnessEnabled(pprev, chainparams) && State(pfrom->GetId())->fHaveWitness) {
        nFetchFlags |= MSG_WITNESS_FLAG;
    }
    return nFetchFlags;
}

bool static ProcessMessage(CNode* pfrom, string strCommand, CDataStream& vRecv, int64_t nTimeReceived, const CChainParams& chainparams)
{
    LogPrint("net", "received: %s (%u bytes) peer=%d\n", SanitizeString(strCommand), vRecv.size(), pfrom->id);
    if (mapArgs.count("-dropmessagestest") && GetRand(atoi(mapArgs["-dropmessagestest"])) == 0) {
        LogPrintf("dropmessagestest DROPPING RECV MESSAGE\n");
        return true;
    }

    if (!(nLocalServices & NODE_BLOOM) && (strCommand == NetMsgType::FILTERLOAD || strCommand == NetMsgType::FILTERADD || strCommand == NetMsgType::FILTERCLEAR)) {
        if (pfrom->nVersion >= NO_BLOOM_VERSION) {
            LOCK(cs_main);
            Misbehaving(pfrom->GetId(), 100);
            return false;
        } else {
            pfrom->fDisconnect = true;
            return false;
        }
    }

    if (strCommand == NetMsgType::VERSION) {

        if (pfrom->fFeeler) {
            assert(pfrom->fInbound == false);
            pfrom->fDisconnect = true;
        }

        if (pfrom->nVersion != 0) {
            pfrom->PushMessage(NetMsgType::REJECT, strCommand, REJECT_DUPLICATE, string("Duplicate version message"));
            LOCK(cs_main);
            Misbehaving(pfrom->GetId(), 1);
            return false;
        }

        int64_t nTime;
        CAddress addrMe;
        CAddress addrFrom;
        uint64_t nNonce = 1;
        uint64_t nServiceInt;
        vRecv >> pfrom->nVersion >> nServiceInt >> nTime >> addrMe;
        pfrom->nServices = ServiceFlags(nServiceInt);
        if (!pfrom->fInbound) {
            addrman.SetServices(pfrom->addr, pfrom->nServices);
        }
        if (pfrom->nServicesExpected & ~pfrom->nServices) {
            LogPrint("net", "peer=%d does not offer the expected services (%08x offered, %08x expected); disconnecting\n", pfrom->id, pfrom->nServices, pfrom->nServicesExpected);
            pfrom->PushMessage(NetMsgType::REJECT, strCommand, REJECT_NONSTANDARD,
                               strprintf("Expected to offer services %08x", pfrom->nServicesExpected));
            pfrom->fDisconnect = true;
            return false;
        }

        if (pfrom->nVersion < MIN_PEER_PROTO_VERSION) {

            LogPrintf("peer=%d using obsolete version %i; disconnecting\n", pfrom->id, pfrom->nVersion);
            pfrom->PushMessage(NetMsgType::REJECT, strCommand, REJECT_OBSOLETE,
                               strprintf("Version must be %d or greater", MIN_PEER_PROTO_VERSION));
            pfrom->fDisconnect = true;
            return false;
        }

        if (pfrom->nVersion == 10300)
            pfrom->nVersion = 300;
        if (!vRecv.empty())
            vRecv >> addrFrom >> nNonce;
        if (!vRecv.empty()) {
            vRecv >> LIMITED_STRING(pfrom->strSubVer, MAX_SUBVERSION_LENGTH);
            pfrom->cleanSubVer = SanitizeString(pfrom->strSubVer);
        }
        if (!vRecv.empty()) {
            vRecv >> pfrom->nStartingHeight;
        }
        {
            LOCK(pfrom->cs_filter);
            if (!vRecv.empty())
                vRecv >> pfrom->fRelayTxes; // set to true after we get the first filter* message
            else
                pfrom->fRelayTxes = true;
        }

        if (pfrom->cleanSubVer == "/Guldencoin:1.3.1/" || pfrom->cleanSubVer == "/Guldencoin:1.4.0/" /* || pfrom->cleanSubVer=="/Guldencoin:1.5.0/"*/) {

            LogPrintf("peer=%d using obsolete version %i; disconnecting\n", pfrom->id, pfrom->nVersion);
            pfrom->PushMessage("reject", strCommand, REJECT_OBSOLETE, strprintf("Client version must be 1.5.1 or greater [%s]", pfrom->cleanSubVer));
            pfrom->fDisconnect = true;
            return false;
        }

        if (nNonce == nLocalHostNonce && nNonce > 1) {
            LogPrintf("connected to self at %s, disconnecting\n", pfrom->addr.ToString());
            pfrom->fDisconnect = true;
            return true;
        }

        pfrom->addrLocal = addrMe;
        if (pfrom->fInbound && addrMe.IsRoutable()) {
            SeenLocal(addrMe);
        }

        if (pfrom->fInbound)
            pfrom->PushVersion();

        pfrom->fClient = !(pfrom->nServices & NODE_NETWORK);

        if ((pfrom->nServices & NODE_WITNESS)) {
            LOCK(cs_main);
            State(pfrom->GetId())->fHaveWitness = true;
        }

        {
            LOCK(cs_main);
            UpdatePreferredDownload(pfrom, State(pfrom->GetId()));
        }

        pfrom->PushMessage(NetMsgType::VERACK);
        pfrom->ssSend.SetVersion(min(pfrom->nVersion, PROTOCOL_VERSION));

        if (!pfrom->fInbound) {

            if (fListen && !IsInitialBlockDownload()) {
                CAddress addr = GetLocalAddress(&pfrom->addr);
                if (addr.IsRoutable()) {
                    LogPrintf("ProcessMessages: advertising address %s\n", addr.ToString());
                    pfrom->PushAddress(addr);
                } else if (IsPeerAddrLocalGood(pfrom)) {
                    addr.SetIP(pfrom->addrLocal);
                    LogPrintf("ProcessMessages: advertising address %s\n", addr.ToString());
                    pfrom->PushAddress(addr);
                }
            }

            if (pfrom->fOneShot || pfrom->nVersion >= CADDR_TIME_VERSION || addrman.size() < 1000) {
                pfrom->PushMessage(NetMsgType::GETADDR);
                pfrom->fGetAddr = true;
            }
            addrman.Good(pfrom->addr);
        }

        {
            LOCK(cs_mapAlerts);
            BOOST_FOREACH (PAIRTYPE(const uint256, CAlert) & item, mapAlerts)
                item.second.RelayTo(pfrom);
        }

        pfrom->fSuccessfullyConnected = true;

        string remoteAddr;
        if (fLogIPs)
            remoteAddr = ", peeraddr=" + pfrom->addr.ToString();

        LogPrintf("receive version message: %s: version %d, blocks=%d, us=%s, peer=%d%s\n",
                  pfrom->cleanSubVer, pfrom->nVersion,
                  pfrom->nStartingHeight, addrMe.ToString(), pfrom->id,
                  remoteAddr);

        int64_t nTimeOffset = nTime - GetTime();
        pfrom->nTimeOffset = nTimeOffset;
        AddTimeData(pfrom->addr, nTimeOffset);
    }

    else if (pfrom->nVersion == 0) {

        LOCK(cs_main);
        Misbehaving(pfrom->GetId(), 1);
        return false;
    }

    else if (strCommand == NetMsgType::VERACK) {
        pfrom->SetRecvVersion(min(pfrom->nVersion, PROTOCOL_VERSION));

        if (pfrom->fNetworkNode) {
            LOCK(cs_main);
            State(pfrom->GetId())->fCurrentlyConnected = true;
        }

        if (pfrom->nVersion >= SENDHEADERS_VERSION) {

            pfrom->PushMessage(NetMsgType::SENDHEADERS);
        }
        if (pfrom->nVersion >= SHORT_IDS_BLOCKS_VERSION) {

            bool fAnnounceUsingCMPCTBLOCK = false;
            uint64_t nCMPCTBLOCKVersion = 1;
            pfrom->PushMessage(NetMsgType::SENDCMPCT, fAnnounceUsingCMPCTBLOCK, nCMPCTBLOCKVersion);
        }
    }

    else if (strCommand == NetMsgType::ADDR) {
        vector<CAddress> vAddr;
        vRecv >> vAddr;

        if (pfrom->nVersion < CADDR_TIME_VERSION && addrman.size() > 1000)
            return true;
        if (vAddr.size() > 1000) {
            LOCK(cs_main);
            Misbehaving(pfrom->GetId(), 20);
            return error("message addr size() = %u", vAddr.size());
        }

        vector<CAddress> vAddrOk;
        int64_t nNow = GetAdjustedTime();
        int64_t nSince = nNow - 10 * 60;
        BOOST_FOREACH (CAddress& addr, vAddr) {
            boost::this_thread::interruption_point();

            if ((addr.nServices & REQUIRED_SERVICES) != REQUIRED_SERVICES)
                continue;

            if (addr.nTime <= 100000000 || addr.nTime > nNow + 10 * 60)
                addr.nTime = nNow - 5 * 24 * 60 * 60;
            pfrom->AddAddressKnown(addr);
            bool fReachable = IsReachable(addr);
            if (addr.nTime > nSince && !pfrom->fGetAddr && vAddr.size() <= 10 && addr.IsRoutable()) {

                {
                    LOCK(cs_vNodes);

                    static const uint64_t salt0 = GetRand(std::numeric_limits<uint64_t>::max());
                    static const uint64_t salt1 = GetRand(std::numeric_limits<uint64_t>::max());
                    uint64_t hashAddr = addr.GetHash();
                    multimap<uint64_t, CNode*> mapMix;
                    const CSipHasher hasher = CSipHasher(salt0, salt1).Write(hashAddr << 32).Write((GetTime() + hashAddr) / (24 * 60 * 60));
                    BOOST_FOREACH (CNode* pnode, vNodes) {
                        if (pnode->nVersion < CADDR_TIME_VERSION)
                            continue;
                        uint64_t hashKey = CSipHasher(hasher).Write(pnode->id).Finalize();
                        mapMix.insert(make_pair(hashKey, pnode));
                    }
                    int nRelayNodes = fReachable ? 2 : 1; // limited relaying of addresses outside our network(s)
                    for (multimap<uint64_t, CNode*>::iterator mi = mapMix.begin(); mi != mapMix.end() && nRelayNodes-- > 0; ++mi)
                        ((*mi).second)->PushAddress(addr);
                }
            }

            if (fReachable)
                vAddrOk.push_back(addr);
        }
        addrman.Add(vAddrOk, pfrom->addr, 2 * 60 * 60);
        if (vAddr.size() < 1000)
            pfrom->fGetAddr = false;
        if (pfrom->fOneShot)
            pfrom->fDisconnect = true;
    }

    else if (strCommand == NetMsgType::SENDHEADERS) {
        LOCK(cs_main);
        State(pfrom->GetId())->fPreferHeaders = true;
    }

    else if (strCommand == NetMsgType::SENDCMPCT) {
        bool fAnnounceUsingCMPCTBLOCK = false;
        uint64_t nCMPCTBLOCKVersion = 1;
        vRecv >> fAnnounceUsingCMPCTBLOCK >> nCMPCTBLOCKVersion;
        if (nCMPCTBLOCKVersion == 1) {
            LOCK(cs_main);
            State(pfrom->GetId())->fProvidesHeaderAndIDs = true;
            State(pfrom->GetId())->fPreferHeaderAndIDs = fAnnounceUsingCMPCTBLOCK;
        }
    }

    else if (strCommand == NetMsgType::INV) {
        vector<CInv> vInv;
        vRecv >> vInv;
        if (vInv.size() > MAX_INV_SZ) {
            LOCK(cs_main);
            Misbehaving(pfrom->GetId(), 20);
            return error("message inv size() = %u", vInv.size());
        }

        bool fBlocksOnly = !fRelayTxes;

        if (pfrom->fWhitelisted && GetBoolArg("-whitelistrelay", DEFAULT_WHITELISTRELAY))
            fBlocksOnly = false;

        LOCK(cs_main);

        uint32_t nFetchFlags = GetFetchFlags(pfrom, chainActive.Tip(), chainparams.GetConsensus());

        std::vector<CInv> vToFetch;

        for (unsigned int nInv = 0; nInv < vInv.size(); nInv++) {
            CInv& inv = vInv[nInv];

            boost::this_thread::interruption_point();

            bool fAlreadyHave = AlreadyHave(inv);
            LogPrint("net", "got inv: %s  %s peer=%d\n", inv.ToString(), fAlreadyHave ? "have" : "new", pfrom->id);

            if (inv.type == MSG_TX) {
                inv.type |= nFetchFlags;
            }

            if (inv.type == MSG_BLOCK) {
                UpdateBlockAvailability(pfrom->GetId(), inv.hash);
                if (!fAlreadyHave && !fImporting && !fReindex && !mapBlocksInFlight.count(inv.hash)) {

                    pfrom->PushMessage(NetMsgType::GETHEADERS, chainActive.GetLocator(pindexBestHeader), inv.hash);
                    CNodeState* nodestate = State(pfrom->GetId());
                    if (CanDirectFetch(chainparams.GetConsensus()) && nodestate->nBlocksInFlight < MAX_BLOCKS_IN_TRANSIT_PER_PEER && (!IsWitnessEnabled(chainActive.Tip(), chainparams.GetConsensus()) || State(pfrom->GetId())->fHaveWitness)) {
                        inv.type |= nFetchFlags;
                        if (nodestate->fProvidesHeaderAndIDs && !(nLocalServices & NODE_WITNESS))
                            vToFetch.push_back(CInv(MSG_CMPCT_BLOCK, inv.hash));
                        else
                            vToFetch.push_back(inv);

                        MarkBlockAsInFlight(pfrom->GetId(), inv.hash, chainparams.GetConsensus());
                    }
                    LogPrint("net", "getheaders (%d) %s to peer=%d\n", pindexBestHeader->nHeight, inv.hash.ToString(), pfrom->id);
                }
            } else {
                pfrom->AddInventoryKnown(inv);
                if (fBlocksOnly)
                    LogPrint("net", "transaction (%s) inv sent in violation of protocol peer=%d\n", inv.hash.ToString(), pfrom->id);
                else if (!fAlreadyHave && !fImporting && !fReindex && !IsInitialBlockDownload())
                    pfrom->AskFor(inv);
            }

            GetMainSignals().Inventory(inv.hash);

            if (pfrom->nSendSize > (SendBufferSize() * 2)) {
                Misbehaving(pfrom->GetId(), 50);
                return error("send buffer size() = %u", pfrom->nSendSize);
            }
        }

        if (!vToFetch.empty())
            pfrom->PushMessage(NetMsgType::GETDATA, vToFetch);
    }

    else if (strCommand == NetMsgType::GETDATA) {
        vector<CInv> vInv;
        vRecv >> vInv;
        if (vInv.size() > MAX_INV_SZ) {
            LOCK(cs_main);
            Misbehaving(pfrom->GetId(), 20);
            return error("message getdata size() = %u", vInv.size());
        }

        if (fDebug || (vInv.size() != 1))
            LogPrint("net", "received getdata (%u invsz) peer=%d\n", vInv.size(), pfrom->id);

        if ((fDebug && vInv.size() > 0) || (vInv.size() == 1))
            LogPrint("net", "received getdata for: %s peer=%d\n", vInv[0].ToString(), pfrom->id);

        pfrom->vRecvGetData.insert(pfrom->vRecvGetData.end(), vInv.begin(), vInv.end());
        ProcessGetData(pfrom, chainparams.GetConsensus());
    }

    else if (strCommand == NetMsgType::GETBLOCKS) {
        CBlockLocator locator;
        uint256 hashStop;
        vRecv >> locator >> hashStop;

        LOCK(cs_main);

        CBlockIndex* pindex = FindForkInGlobalIndex(chainActive, locator);

        if (pindex)
            pindex = chainActive.Next(pindex);
        int nLimit = 500;
        LogPrint("net", "getblocks %d to %s limit %d from peer=%d\n", (pindex ? pindex->nHeight : -1), hashStop.IsNull() ? "end" : hashStop.ToString(), nLimit, pfrom->id);
        for (; pindex; pindex = chainActive.Next(pindex)) {
            if (pindex->GetBlockHash() == hashStop) {
                LogPrint("net", "  getblocks stopping at %d %s\n", pindex->nHeight, pindex->GetBlockHash().ToString());
                break;
            }

            const int nPrunedBlocksLikelyToHave = MIN_BLOCKS_TO_KEEP - 3600 / chainparams.GetConsensus().nPowTargetSpacing;
            if (fPruneMode && (!(pindex->nStatus & BLOCK_HAVE_DATA) || pindex->nHeight <= chainActive.Tip()->nHeight - nPrunedBlocksLikelyToHave)) {
                LogPrint("net", " getblocks stopping, pruned or too old block at %d %s\n", pindex->nHeight, pindex->GetBlockHash().ToString());
                break;
            }
            pfrom->PushInventory(CInv(MSG_BLOCK, pindex->GetBlockHash()));
            if (--nLimit <= 0) {

                LogPrint("net", "  getblocks stopping at limit %d %s\n", pindex->nHeight, pindex->GetBlockHash().ToString());
                pfrom->hashContinue = pindex->GetBlockHash();
                break;
            }
        }
    }

    else if (strCommand == NetMsgType::GETBLOCKTXN) {
        BlockTransactionsRequest req;
        vRecv >> req;

        BlockMap::iterator it = mapBlockIndex.find(req.blockhash);
        if (it == mapBlockIndex.end() || !(it->second->nStatus & BLOCK_HAVE_DATA)) {
            LogPrintf("Peer %d sent us a getblocktxn for a block we don't have", pfrom->id);
            return true;
        }

        if (it->second->nHeight < chainActive.Height() - 15) {
            LogPrint("net", "Peer %d sent us a getblocktxn for a block > 15 deep", pfrom->id);
            return true;
        }

        CBlock block;
        assert(ReadBlockFromDisk(block, it->second, chainparams.GetConsensus()));

        BlockTransactions resp(req);
        for (size_t i = 0; i < req.indexes.size(); i++) {
            if (req.indexes[i] >= block.vtx.size()) {
                Misbehaving(pfrom->GetId(), 100);
                LogPrintf("Peer %d sent us a getblocktxn with out-of-bounds tx indices", pfrom->id);
                return true;
            }
            resp.txn[i] = block.vtx[req.indexes[i]];
        }
        pfrom->PushMessageWithFlag(SERIALIZE_TRANSACTION_NO_WITNESS, NetMsgType::BLOCKTXN, resp);
    }

    else if (strCommand == NetMsgType::GETHEADERS) {
        CBlockLocator locator;
        uint256 hashStop;
        vRecv >> locator >> hashStop;

        LOCK(cs_main);
        if (IsInitialBlockDownload() && !pfrom->fWhitelisted) {
            LogPrint("net", "Ignoring getheaders from peer=%d because node is in initial block download\n", pfrom->id);
            return true;
        }

        CNodeState* nodestate = State(pfrom->GetId());
        CBlockIndex* pindex = NULL;
        if (locator.IsNull()) {

            BlockMap::iterator mi = mapBlockIndex.find(hashStop);
            if (mi == mapBlockIndex.end())
                return true;
            pindex = (*mi).second;
        } else {

            pindex = FindForkInGlobalIndex(chainActive, locator);
            if (pindex)
                pindex = chainActive.Next(pindex);
        }

        vector<CBlock> vHeaders;
        int nLimit = MAX_HEADERS_RESULTS;
        LogPrint("net", "getheaders %d to %s from peer=%d\n", (pindex ? pindex->nHeight : -1), hashStop.ToString(), pfrom->id);
        for (; pindex; pindex = chainActive.Next(pindex)) {
            vHeaders.push_back(pindex->GetBlockHeader());
            if (--nLimit <= 0 || pindex->GetBlockHash() == hashStop)
                break;
        }

        nodestate->pindexBestHeaderSent = pindex ? pindex : chainActive.Tip();
        pfrom->PushMessage(NetMsgType::HEADERS, vHeaders);
    }

    else if (strCommand == NetMsgType::TX) {

        if (!fRelayTxes && (!pfrom->fWhitelisted || !GetBoolArg("-whitelistrelay", DEFAULT_WHITELISTRELAY))) {
            LogPrint("net", "transaction sent in violation of protocol peer=%d\n", pfrom->id);
            return true;
        }

        deque<COutPoint> vWorkQueue;
        vector<uint256> vEraseQueue;
        CTransaction tx;
        vRecv >> tx;

        CInv inv(MSG_TX, tx.GetHash());
        pfrom->AddInventoryKnown(inv);

        LOCK(cs_main);

        bool fMissingInputs = false;
        CValidationState state;

        pfrom->setAskFor.erase(inv.hash);
        mapAlreadyAskedFor.erase(inv.hash);

        if (!AlreadyHave(inv) && AcceptToMemoryPool(mempool, state, tx, true, &fMissingInputs)) {
            mempool.check(pcoinsTip);
            RelayTransaction(tx);
            for (unsigned int i = 0; i < tx.vout.size(); i++) {
                vWorkQueue.emplace_back(inv.hash, i);
            }

            pfrom->nLastTXTime = GetTime();

            LogPrint("mempool", "AcceptToMemoryPool: peer=%d: accepted %s (poolsz %u txn, %u kB)\n",
                     pfrom->id,
                     tx.GetHash().ToString(),
                     mempool.size(), mempool.DynamicMemoryUsage() / 1000);

            set<NodeId> setMisbehaving;
            while (!vWorkQueue.empty()) {
                auto itByPrev = mapOrphanTransactionsByPrev.find(vWorkQueue.front());
                vWorkQueue.pop_front();
                if (itByPrev == mapOrphanTransactionsByPrev.end())
                    continue;
                for (auto mi = itByPrev->second.begin();
                     mi != itByPrev->second.end();
                     ++mi) {
                    const CTransaction& orphanTx = (*mi)->second.tx;
                    const uint256& orphanHash = orphanTx.GetHash();
                    NodeId fromPeer = (*mi)->second.fromPeer;
                    bool fMissingInputs2 = false;

                    CValidationState stateDummy;

                    if (setMisbehaving.count(fromPeer))
                        continue;
                    if (AcceptToMemoryPool(mempool, stateDummy, orphanTx, true, &fMissingInputs2)) {
                        LogPrint("mempool", "   accepted orphan tx %s\n", orphanHash.ToString());
                        RelayTransaction(orphanTx);
                        for (unsigned int i = 0; i < orphanTx.vout.size(); i++) {
                            vWorkQueue.emplace_back(orphanHash, i);
                        }
                        vEraseQueue.push_back(orphanHash);
                    } else if (!fMissingInputs2) {
                        int nDos = 0;
                        if (stateDummy.IsInvalid(nDos) && nDos > 0 && (!state.CorruptionPossible() || State(fromPeer)->fHaveWitness)) {

                            Misbehaving(fromPeer, nDos);
                            setMisbehaving.insert(fromPeer);
                            LogPrint("mempool", "   invalid orphan tx %s\n", orphanHash.ToString());
                        }

                        LogPrint("mempool", "   removed orphan tx %s\n", orphanHash.ToString());
                        vEraseQueue.push_back(orphanHash);
                        if (!stateDummy.CorruptionPossible()) {
                            assert(recentRejects);
                            recentRejects->insert(orphanHash);
                        }
                    }
                    mempool.check(pcoinsTip);
                }
            }

            BOOST_FOREACH (uint256 hash, vEraseQueue)
                EraseOrphanTx(hash);
        } else if (fMissingInputs) {
            bool fRejectedParents = false; // It may be the case that the orphans parents have all been rejected
            BOOST_FOREACH (const CTxIn& txin, tx.vin) {
                if (recentRejects->contains(txin.prevout.hash)) {
                    fRejectedParents = true;
                    break;
                }
            }
            if (!fRejectedParents) {
                BOOST_FOREACH (const CTxIn& txin, tx.vin) {
                    CInv inv(MSG_TX, txin.prevout.hash);
                    pfrom->AddInventoryKnown(inv);
                    if (!AlreadyHave(inv))
                        pfrom->AskFor(inv);
                }
                AddOrphanTx(tx, pfrom->GetId());

                unsigned int nMaxOrphanTx = (unsigned int)std::max((int64_t)0, GetArg("-maxorphantx", DEFAULT_MAX_ORPHAN_TRANSACTIONS));
                unsigned int nEvicted = LimitOrphanTxSize(nMaxOrphanTx);
                if (nEvicted > 0)
                    LogPrint("mempool", "mapOrphan overflow, removed %u tx\n", nEvicted);
            } else {
                LogPrint("mempool", "not keeping orphan with rejected parents %s\n", tx.GetHash().ToString());
            }
        } else {
            if (!state.CorruptionPossible()) {
                assert(recentRejects);
                recentRejects->insert(tx.GetHash());
            }

            if (pfrom->fWhitelisted && GetBoolArg("-whitelistforcerelay", DEFAULT_WHITELISTFORCERELAY)) {

                int nDoS = 0;
                if (!state.IsInvalid(nDoS) || nDoS == 0) {
                    LogPrintf("Force relaying tx %s from whitelisted peer=%d\n", tx.GetHash().ToString(), pfrom->id);
                    RelayTransaction(tx);
                } else {
                    LogPrintf("Not relaying invalid transaction %s from whitelisted peer=%d (%s)\n", tx.GetHash().ToString(), pfrom->id, FormatStateMessage(state));
                }
            }
        }
        int nDoS = 0;
        if (state.IsInvalid(nDoS)) {
            LogPrint("mempoolrej", "%s from peer=%d was not accepted: %s\n", tx.GetHash().ToString(),
                     pfrom->id,
                     FormatStateMessage(state));
            if (state.GetRejectCode() < REJECT_INTERNAL) // Never send AcceptToMemoryPool's internal codes over P2P
                pfrom->PushMessage(NetMsgType::REJECT, strCommand, (unsigned char)state.GetRejectCode(),
                                   state.GetRejectReason().substr(0, MAX_REJECT_MESSAGE_LENGTH), inv.hash);
            if (nDoS > 0 && (!state.CorruptionPossible() || State(pfrom->id)->fHaveWitness)) {

                Misbehaving(pfrom->GetId(), nDoS);
            }
        }
        FlushStateToDisk(state, FLUSH_STATE_PERIODIC);
    }

    else if (strCommand == NetMsgType::CMPCTBLOCK && !fImporting && !fReindex) // Ignore blocks received while importing
    {
        CBlockHeaderAndShortTxIDs cmpctblock;
        vRecv >> cmpctblock;

        LOCK(cs_main);

        if (mapBlockIndex.find(cmpctblock.header.hashPrevBlock) == mapBlockIndex.end()) {

            if (!IsInitialBlockDownload())
                pfrom->PushMessage(NetMsgType::GETHEADERS, chainActive.GetLocator(pindexBestHeader), uint256());
            return true;
        }

        CBlockIndex* pindex = NULL;
        CValidationState state;
        if (!AcceptBlockHeader(cmpctblock.header, state, chainparams, &pindex)) {
            int nDoS;
            if (state.IsInvalid(nDoS)) {
                if (nDoS > 0)
                    Misbehaving(pfrom->GetId(), nDoS);
                LogPrintf("Peer %d sent us invalid header via cmpctblock\n", pfrom->id);
                return true;
            }
        }

        assert(pindex);
        UpdateBlockAvailability(pfrom->GetId(), pindex->GetBlockHash());

        std::map<uint256, pair<NodeId, list<QueuedBlock>::iterator> >::iterator blockInFlightIt = mapBlocksInFlight.find(pindex->GetBlockHash());
        bool fAlreadyInFlight = blockInFlightIt != mapBlocksInFlight.end();

        if (pindex->nStatus & BLOCK_HAVE_DATA) // Nothing to do here
            return true;

        if (pindex->nChainWork <= chainActive.Tip()->nChainWork || // We know something better
            pindex->nTx != 0) { // We had this block at some point, but pruned it
            if (fAlreadyInFlight) {

                std::vector<CInv> vInv(1);
                vInv[0] = CInv(MSG_BLOCK, cmpctblock.header.GetHash());
                pfrom->PushMessage(NetMsgType::GETDATA, vInv);
            }
            return true;
        }

        if (!fAlreadyInFlight && !CanDirectFetch(chainparams.GetConsensus()))
            return true;

        CNodeState* nodestate = State(pfrom->GetId());

        if (pindex->nHeight <= chainActive.Height() + 2) {
            if ((!fAlreadyInFlight && nodestate->nBlocksInFlight < MAX_BLOCKS_IN_TRANSIT_PER_PEER) || (fAlreadyInFlight && blockInFlightIt->second.first == pfrom->GetId())) {
                list<QueuedBlock>::iterator* queuedBlockIt = NULL;
                if (!MarkBlockAsInFlight(pfrom->GetId(), pindex->GetBlockHash(), chainparams.GetConsensus(), pindex, &queuedBlockIt)) {
                    if (!(*queuedBlockIt)->partialBlock)
                        (*queuedBlockIt)->partialBlock.reset(new PartiallyDownloadedBlock(&mempool));
                    else {

                        LogPrint("net", "Peer sent us compact block we were already syncing!\n");
                        return true;
                    }
                }

                PartiallyDownloadedBlock& partialBlock = *(*queuedBlockIt)->partialBlock;
                ReadStatus status = partialBlock.InitData(cmpctblock);
                if (status == READ_STATUS_INVALID) {
                    MarkBlockAsReceived(pindex->GetBlockHash()); // Reset in-flight state in case of whitelist
                    Misbehaving(pfrom->GetId(), 100);
                    LogPrintf("Peer %d sent us invalid compact block\n", pfrom->id);
                    return true;
                } else if (status == READ_STATUS_FAILED) {

                    std::vector<CInv> vInv(1);
                    vInv[0] = CInv(MSG_BLOCK, cmpctblock.header.GetHash());
                    pfrom->PushMessage(NetMsgType::GETDATA, vInv);
                    return true;
                }

                BlockTransactionsRequest req;
                for (size_t i = 0; i < cmpctblock.BlockTxCount(); i++) {
                    if (!partialBlock.IsTxAvailable(i))
                        req.indexes.push_back(i);
                }
                if (req.indexes.empty()) {

                    BlockTransactions txn;
                    txn.blockhash = cmpctblock.header.GetHash();
                    CDataStream blockTxnMsg(SER_NETWORK, PROTOCOL_VERSION);
                    blockTxnMsg << txn;
                    return ProcessMessage(pfrom, NetMsgType::BLOCKTXN, blockTxnMsg, nTimeReceived, chainparams);
                } else {
                    req.blockhash = pindex->GetBlockHash();
                    pfrom->PushMessage(NetMsgType::GETBLOCKTXN, req);
                }
            }
        } else {
            if (fAlreadyInFlight) {

                std::vector<CInv> vInv(1);
                vInv[0] = CInv(MSG_BLOCK, cmpctblock.header.GetHash());
                pfrom->PushMessage(NetMsgType::GETDATA, vInv);
                return true;
            } else {

                std::vector<CBlock> headers;
                headers.push_back(cmpctblock.header);
                CDataStream vHeadersMsg(SER_NETWORK, PROTOCOL_VERSION);
                vHeadersMsg << headers;
                return ProcessMessage(pfrom, NetMsgType::HEADERS, vHeadersMsg, nTimeReceived, chainparams);
            }
        }

        CheckBlockIndex(chainparams.GetConsensus());
    }

    else if (strCommand == NetMsgType::BLOCKTXN && !fImporting && !fReindex) // Ignore blocks received while importing
    {
        BlockTransactions resp;
        vRecv >> resp;

        LOCK(cs_main);

        map<uint256, pair<NodeId, list<QueuedBlock>::iterator> >::iterator it = mapBlocksInFlight.find(resp.blockhash);
        if (it == mapBlocksInFlight.end() || !it->second.second->partialBlock || it->second.first != pfrom->GetId()) {
            LogPrint("net", "Peer %d sent us block transactions for block we weren't expecting\n", pfrom->id);
            return true;
        }

        PartiallyDownloadedBlock& partialBlock = *it->second.second->partialBlock;
        CBlock block;
        ReadStatus status = partialBlock.FillBlock(block, resp.txn);
        if (status == READ_STATUS_INVALID) {
            MarkBlockAsReceived(resp.blockhash); // Reset in-flight state in case of whitelist
            Misbehaving(pfrom->GetId(), 100);
            LogPrintf("Peer %d sent us invalid compact block/non-matching block transactions\n", pfrom->id);
            return true;
        } else if (status == READ_STATUS_FAILED) {

            std::vector<CInv> invs;
            invs.push_back(CInv(MSG_BLOCK, resp.blockhash));
            pfrom->PushMessage(NetMsgType::GETDATA, invs);
        } else {
            CValidationState state;
            ProcessNewBlock(state, chainparams, pfrom, &block, false, NULL);
            int nDoS;
            if (state.IsInvalid(nDoS)) {
                assert(state.GetRejectCode() < REJECT_INTERNAL); // Blocks are never rejected with internal reject codes
                pfrom->PushMessage(NetMsgType::REJECT, strCommand, (unsigned char)state.GetRejectCode(),
                                   state.GetRejectReason().substr(0, MAX_REJECT_MESSAGE_LENGTH), block.GetHash());
                if (nDoS > 0) {
                    LOCK(cs_main);
                    Misbehaving(pfrom->GetId(), nDoS);
                }
            }
        }
    }

    else if (strCommand == NetMsgType::HEADERS && !fImporting && !fReindex) // Ignore headers received while importing
    {
        std::vector<CBlockHeader> headers;

        unsigned int nCount = ReadCompactSize(vRecv);
        if (nCount > MAX_HEADERS_RESULTS) {
            LOCK(cs_main);
            Misbehaving(pfrom->GetId(), 20);
            return error("headers message size = %u", nCount);
        }
        headers.resize(nCount);
        for (unsigned int n = 0; n < nCount; n++) {
            vRecv >> headers[n];
            ReadCompactSize(vRecv); // ignore tx count; assume it is 0.
        }

        {
            LOCK(cs_main);

            if (nCount == 0) {

                return true;
            }

            CNodeState* nodestate = State(pfrom->GetId());

            if (mapBlockIndex.find(headers[0].hashPrevBlock) == mapBlockIndex.end() && nCount < MAX_BLOCKS_TO_ANNOUNCE) {
                nodestate->nUnconnectingHeaders++;
                pfrom->PushMessage(NetMsgType::GETHEADERS, chainActive.GetLocator(pindexBestHeader), uint256());
                LogPrint("net", "received header %s: missing prev block %s, sending getheaders (%d) to end (peer=%d, nUnconnectingHeaders=%d)\n",
                         headers[0].GetHash().ToString(),
                         headers[0].hashPrevBlock.ToString(),
                         pindexBestHeader->nHeight,
                         pfrom->id, nodestate->nUnconnectingHeaders);

                UpdateBlockAvailability(pfrom->GetId(), headers.back().GetHash());

                if (nodestate->nUnconnectingHeaders % MAX_UNCONNECTING_HEADERS == 0) {
                    Misbehaving(pfrom->GetId(), 20);
                }
                return true;
            }

            CBlockIndex* pindexLast = NULL;
            BOOST_FOREACH (const CBlockHeader& header, headers) {
                CValidationState state;
                if (pindexLast != NULL && header.hashPrevBlock != pindexLast->GetBlockHash()) {
                    Misbehaving(pfrom->GetId(), 20);
                    return error("non-continuous headers sequence");
                }
                if (!AcceptBlockHeader(header, state, chainparams, &pindexLast)) {
                    int nDoS;
                    if (state.IsInvalid(nDoS)) {
                        if (nDoS > 0)
                            Misbehaving(pfrom->GetId(), nDoS);
                        return error("invalid header received");
                    }
                }
            }

            if (nodestate->nUnconnectingHeaders > 0) {
                LogPrint("net", "peer=%d: resetting nUnconnectingHeaders (%d -> 0)\n", pfrom->id, nodestate->nUnconnectingHeaders);
            }
            nodestate->nUnconnectingHeaders = 0;

            assert(pindexLast);
            UpdateBlockAvailability(pfrom->GetId(), pindexLast->GetBlockHash());

            if (nCount == MAX_HEADERS_RESULTS) {

                LogPrint("net", "more getheaders (%d) to end to peer=%d (startheight:%d)\n", pindexLast->nHeight, pfrom->id, pfrom->nStartingHeight);
                pfrom->PushMessage(NetMsgType::GETHEADERS, chainActive.GetLocator(pindexLast), uint256());
            }

            bool fCanDirectFetch = CanDirectFetch(chainparams.GetConsensus());

            if (fCanDirectFetch && pindexLast->IsValid(BLOCK_VALID_TREE) && chainActive.Tip()->nChainWork <= pindexLast->nChainWork) {
                vector<CBlockIndex*> vToFetch;
                CBlockIndex* pindexWalk = pindexLast;

                while (pindexWalk && !chainActive.Contains(pindexWalk) && vToFetch.size() <= MAX_BLOCKS_IN_TRANSIT_PER_PEER) {
                    if (!(pindexWalk->nStatus & BLOCK_HAVE_DATA) && !mapBlocksInFlight.count(pindexWalk->GetBlockHash()) && (!IsWitnessEnabled(pindexWalk->pprev, chainparams.GetConsensus()) || State(pfrom->GetId())->fHaveWitness)) {

                        vToFetch.push_back(pindexWalk);
                    }
                    pindexWalk = pindexWalk->pprev;
                }

                if (!chainActive.Contains(pindexWalk)) {
                    LogPrint("net", "Large reorg, won't direct fetch to %s (%d)\n",
                             pindexLast->GetBlockHash().ToString(),
                             pindexLast->nHeight);
                } else {
                    vector<CInv> vGetData;

                    BOOST_REVERSE_FOREACH(CBlockIndex * pindex, vToFetch)
                    {
                        if (nodestate->nBlocksInFlight >= MAX_BLOCKS_IN_TRANSIT_PER_PEER) {

                            break;
                        }
                        uint32_t nFetchFlags = GetFetchFlags(pfrom, pindex->pprev, chainparams.GetConsensus());
                        vGetData.push_back(CInv(MSG_BLOCK | nFetchFlags, pindex->GetBlockHash()));
                        MarkBlockAsInFlight(pfrom->GetId(), pindex->GetBlockHash(), chainparams.GetConsensus(), pindex);
                        LogPrint("net", "Requesting block %s from  peer=%d\n",
                                 pindex->GetBlockHash().ToString(), pfrom->id);
                    }
                    if (vGetData.size() > 1) {
                        LogPrint("net", "Downloading blocks toward %s (%d) via headers direct fetch\n",
                                 pindexLast->GetBlockHash().ToString(), pindexLast->nHeight);
                    }
                    if (vGetData.size() > 0) {
                        if (nodestate->fProvidesHeaderAndIDs && vGetData.size() == 1 && mapBlocksInFlight.size() == 1 && pindexLast->pprev->IsValid(BLOCK_VALID_CHAIN) && !(nLocalServices & NODE_WITNESS)) {

                            MaybeSetPeerAsAnnouncingHeaderAndIDs(nodestate, pfrom);

                            vGetData[0] = CInv(MSG_CMPCT_BLOCK, vGetData[0].hash);
                        }
                        pfrom->PushMessage(NetMsgType::GETDATA, vGetData);
                    }
                }
            }

            CheckBlockIndex(chainparams.GetConsensus());
        }

        NotifyHeaderTip();
    }

    else if (strCommand == NetMsgType::BLOCK && !fImporting && !fReindex) // Ignore blocks received while importing
    {
        CBlock block;
        vRecv >> block;

        LogPrint("net", "received block %s peer=%d\n", block.GetHash().ToString(), pfrom->id);

        CValidationState state;

        bool forceProcessing = pfrom->fWhitelisted && !IsInitialBlockDownload();
        ProcessNewBlock(state, chainparams, pfrom, &block, forceProcessing, NULL);
        int nDoS;
        if (state.IsInvalid(nDoS)) {
            assert(state.GetRejectCode() < REJECT_INTERNAL); // Blocks are never rejected with internal reject codes
            pfrom->PushMessage(NetMsgType::REJECT, strCommand, (unsigned char)state.GetRejectCode(),
                               state.GetRejectReason().substr(0, MAX_REJECT_MESSAGE_LENGTH), block.GetHash());
            if (nDoS > 0) {
                LOCK(cs_main);
                Misbehaving(pfrom->GetId(), nDoS);
            }
        }

    }

    else if (strCommand == NetMsgType::GETADDR) {

        if (!pfrom->fInbound) {
            LogPrint("net", "Ignoring \"getaddr\" from outbound connection. peer=%d\n", pfrom->id);
            return true;
        }

        if (pfrom->fSentAddr) {
            LogPrint("net", "Ignoring repeated \"getaddr\". peer=%d\n", pfrom->id);
            return true;
        }
        pfrom->fSentAddr = true;

        pfrom->vAddrToSend.clear();
        vector<CAddress> vAddr = addrman.GetAddr();
        BOOST_FOREACH (const CAddress& addr, vAddr)
            pfrom->PushAddress(addr);
    }

    else if (strCommand == NetMsgType::MEMPOOL) {
        if (!(nLocalServices & NODE_BLOOM) && !pfrom->fWhitelisted) {
            LogPrint("net", "mempool request with bloom filters disabled, disconnect peer=%d\n", pfrom->GetId());
            pfrom->fDisconnect = true;
            return true;
        }

        if (CNode::OutboundTargetReached(false) && !pfrom->fWhitelisted) {
            LogPrint("net", "mempool request with bandwidth limit reached, disconnect peer=%d\n", pfrom->GetId());
            pfrom->fDisconnect = true;
            return true;
        }

        LOCK(pfrom->cs_inventory);
        pfrom->fSendMempool = true;
    }

    else if (strCommand == NetMsgType::PING) {
        if (pfrom->nVersion > BIP0031_VERSION) {
            uint64_t nonce = 0;
            vRecv >> nonce;

            pfrom->PushMessage(NetMsgType::PONG, nonce);
        }
    }

    else if (strCommand == NetMsgType::PONG) {
        int64_t pingUsecEnd = nTimeReceived;
        uint64_t nonce = 0;
        size_t nAvail = vRecv.in_avail();
        bool bPingFinished = false;
        std::string sProblem;

        if (nAvail >= sizeof(nonce)) {
            vRecv >> nonce;

            if (pfrom->nPingNonceSent != 0) {
                if (nonce == pfrom->nPingNonceSent) {

                    bPingFinished = true;
                    int64_t pingUsecTime = pingUsecEnd - pfrom->nPingUsecStart;
                    if (pingUsecTime > 0) {

                        pfrom->nPingUsecTime = pingUsecTime;
                        pfrom->nMinPingUsecTime = std::min(pfrom->nMinPingUsecTime, pingUsecTime);
                    } else {

                        sProblem = "Timing mishap";
                    }
                } else {

                    sProblem = "Nonce mismatch";
                    if (nonce == 0) {

                        bPingFinished = true;
                        sProblem = "Nonce zero";
                    }
                }
            } else {
                sProblem = "Unsolicited pong without ping";
            }
        } else {

            bPingFinished = true;
            sProblem = "Short payload";
        }

        if (!(sProblem.empty())) {
            LogPrint("net", "pong peer=%d: %s, %x expected, %x received, %u bytes\n",
                     pfrom->id,
                     sProblem,
                     pfrom->nPingNonceSent,
                     nonce,
                     nAvail);
        }
        if (bPingFinished) {
            pfrom->nPingNonceSent = 0;
        }
    }

    else if (fAlerts && strCommand == NetMsgType::ALERT) {
        CAlert alert;
        vRecv >> alert;

        uint256 alertHash = alert.GetHash();
        if (pfrom->setKnown.count(alertHash) == 0) {
            if (alert.ProcessAlert(chainparams.AlertKey())) {

                pfrom->setKnown.insert(alertHash);
                {
                    LOCK(cs_vNodes);
                    BOOST_FOREACH (CNode* pnode, vNodes)
                        alert.RelayTo(pnode);
                }
            } else {

                Misbehaving(pfrom->GetId(), 10);
            }
        }
    }

    else if (strCommand == "checkpoint") {
        CSyncCheckpoint checkpoint;
        vRecv >> checkpoint;

        if (checkpoint.ProcessSyncCheckpoint(pfrom, chainparams)) {

            pfrom->hashCheckpointKnown = checkpoint.hashCheckpoint;
            LOCK(cs_vNodes);
            BOOST_FOREACH (CNode* pnode, vNodes)
                checkpoint.RelayTo(pnode);
        }
    }

    else if (strCommand == NetMsgType::FILTERLOAD) {
        CBloomFilter filter;
        vRecv >> filter;

        if (!filter.IsWithinSizeConstraints()) {

            LOCK(cs_main);
            Misbehaving(pfrom->GetId(), 100);
        } else {
            LOCK(pfrom->cs_filter);
            delete pfrom->pfilter;
            pfrom->pfilter = new CBloomFilter(filter);
            pfrom->pfilter->UpdateEmptyFull();
            pfrom->fRelayTxes = true;
        }
    }

    else if (strCommand == NetMsgType::FILTERADD) {
        vector<unsigned char> vData;
        vRecv >> vData;

        bool bad = false;
        if (vData.size() > MAX_SCRIPT_ELEMENT_SIZE) {
            bad = true;
        } else {
            LOCK(pfrom->cs_filter);
            if (pfrom->pfilter) {
                pfrom->pfilter->insert(vData);
            } else {
                bad = true;
            }
        }
        if (bad) {
            LOCK(cs_main);
            Misbehaving(pfrom->GetId(), 100);
        }
    }

    else if (strCommand == NetMsgType::FILTERCLEAR) {
        LOCK(pfrom->cs_filter);
        delete pfrom->pfilter;
        pfrom->pfilter = new CBloomFilter();
        pfrom->fRelayTxes = true;
    }

    else if (strCommand == NetMsgType::REJECT) {
        if (fDebug) {
            try {
                string strMsg;
                unsigned char ccode;
                string strReason;
                vRecv >> LIMITED_STRING(strMsg, CMessageHeader::COMMAND_SIZE) >> ccode >> LIMITED_STRING(strReason, MAX_REJECT_MESSAGE_LENGTH);

                ostringstream ss;
                ss << strMsg << " code " << itostr(ccode) << ": " << strReason;

                if (strMsg == NetMsgType::BLOCK || strMsg == NetMsgType::TX) {
                    uint256 hash;
                    vRecv >> hash;
                    ss << ": hash " << hash.ToString();
                }
                LogPrint("net", "Reject %s\n", SanitizeString(ss.str()));
            }
            catch (const std::ios_base::failure&) {

                LogPrint("net", "Unparseable reject message received\n");
            }
        }
    }

    else if (strCommand == NetMsgType::FEEFILTER) {
        CAmount newFeeFilter = 0;
        vRecv >> newFeeFilter;
        if (MoneyRange(newFeeFilter)) {
            {
                LOCK(pfrom->cs_feeFilter);
                pfrom->minFeeFilter = newFeeFilter;
            }
            LogPrint("net", "received: feefilter of %s from peer=%d\n", CFeeRate(newFeeFilter).ToString(), pfrom->id);
        }
    }

    else if (strCommand == NetMsgType::NOTFOUND) {

    }

    else {

        LogPrint("net", "Unknown command \"%s\" from peer=%d\n", SanitizeString(strCommand), pfrom->id);
    }

    return true;
}

bool ProcessMessages(CNode* pfrom)
{
    const CChainParams& chainparams = Params();

    bool fOk = true;

    if (!pfrom->vRecvGetData.empty())
        ProcessGetData(pfrom, chainparams.GetConsensus());

    if (!pfrom->vRecvGetData.empty())
        return fOk;

    std::deque<CNetMessage>::iterator it = pfrom->vRecvMsg.begin();
    while (!pfrom->fDisconnect && it != pfrom->vRecvMsg.end()) {

        if (pfrom->nSendSize >= SendBufferSize())
            break;

        CNetMessage& msg = *it;

        if (!msg.complete())
            break;

        it++;

        if (memcmp(msg.hdr.pchMessageStart, chainparams.MessageStart(), MESSAGE_START_SIZE) != 0) {
            LogPrintf("PROCESSMESSAGE: INVALID MESSAGESTART %s peer=%d\n", SanitizeString(msg.hdr.GetCommand()), pfrom->id);
            fOk = false;
            break;
        }

        CMessageHeader& hdr = msg.hdr;
        if (!hdr.IsValid(chainparams.MessageStart())) {
            LogPrintf("PROCESSMESSAGE: ERRORS IN HEADER %s peer=%d\n", SanitizeString(hdr.GetCommand()), pfrom->id);
            continue;
        }
        string strCommand = hdr.GetCommand();

        unsigned int nMessageSize = hdr.nMessageSize;

        CDataStream& vRecv = msg.vRecv;
        uint256 hash = Hash(vRecv.begin(), vRecv.begin() + nMessageSize);
        unsigned int nChecksum = ReadLE32((unsigned char*)&hash);
        if (nChecksum != hdr.nChecksum) {
            LogPrintf("%s(%s, %u bytes): CHECKSUM ERROR nChecksum=%08x hdr.nChecksum=%08x\n", __func__,
                      SanitizeString(strCommand), nMessageSize, nChecksum, hdr.nChecksum);
            continue;
        }

        bool fRet = false;
        try {
            fRet = ProcessMessage(pfrom, strCommand, vRecv, msg.nTime, chainparams);
            boost::this_thread::interruption_point();
        }
        catch (const std::ios_base::failure& e) {
            pfrom->PushMessage(NetMsgType::REJECT, strCommand, REJECT_MALFORMED, string("error parsing message"));
            if (strstr(e.what(), "end of data")) {

                LogPrintf("%s(%s, %u bytes): Exception '%s' caught, normally caused by a message being shorter than its stated length\n", __func__, SanitizeString(strCommand), nMessageSize, e.what());
            } else if (strstr(e.what(), "size too large")) {

                LogPrintf("%s(%s, %u bytes): Exception '%s' caught\n", __func__, SanitizeString(strCommand), nMessageSize, e.what());
            } else if (strstr(e.what(), "non-canonical ReadCompactSize()")) {

                LogPrintf("%s(%s, %u bytes): Exception '%s' caught\n", __func__, SanitizeString(strCommand), nMessageSize, e.what());
            } else {
                PrintExceptionContinue(&e, "ProcessMessages()");
            }
        }
        catch (const boost::thread_interrupted&) {
            throw;
        }
        catch (const std::exception& e) {
            PrintExceptionContinue(&e, "ProcessMessages()");
        }
        catch (...) {
            PrintExceptionContinue(NULL, "ProcessMessages()");
        }

        if (!fRet)
            LogPrintf("%s(%s, %u bytes) FAILED peer=%d\n", __func__, SanitizeString(strCommand), nMessageSize, pfrom->id);

        break;
    }

    if (!pfrom->fDisconnect)
        pfrom->vRecvMsg.erase(pfrom->vRecvMsg.begin(), it);

    return fOk;
}

class CompareInvMempoolOrder {
    CTxMemPool* mp;

public:
    CompareInvMempoolOrder(CTxMemPool* mempool)
    {
        mp = mempool;
    }

    bool operator()(std::set<uint256>::iterator a, std::set<uint256>::iterator b)
    {
        /* As std::make_heap produces a max-heap, we want the entries with the
         * fewest ancestors/highest fee to sort later. */
        return mp->CompareDepthAndScore(*b, *a);
    }
};

bool SendMessages(CNode* pto)
{
    const Consensus::Params& consensusParams = Params().GetConsensus();
    {

        if (pto->nVersion == 0)
            return true;

        bool pingSend = false;
        if (pto->fPingQueued) {

            pingSend = true;
        }
        if (pto->nPingNonceSent == 0 && pto->nPingUsecStart + PING_INTERVAL * 1000000 < GetTimeMicros()) {

            pingSend = true;
        }
        if (pingSend) {
            uint64_t nonce = 0;
            while (nonce == 0) {
                GetRandBytes((unsigned char*)&nonce, sizeof(nonce));
            }
            pto->fPingQueued = false;
            pto->nPingUsecStart = GetTimeMicros();
            if (pto->nVersion > BIP0031_VERSION) {
                pto->nPingNonceSent = nonce;
                pto->PushMessage(NetMsgType::PING, nonce);
            } else {

                pto->nPingNonceSent = 0;
                pto->PushMessage(NetMsgType::PING);
            }
        }

        TRY_LOCK(cs_main, lockMain); // Acquire cs_main for IsInitialBlockDownload() and CNodeState()
        if (!lockMain)
            return true;

        int64_t nNow = GetTimeMicros();
        if (!IsInitialBlockDownload() && pto->nNextLocalAddrSend < nNow) {
            AdvertiseLocal(pto);
            pto->nNextLocalAddrSend = PoissonNextSend(nNow, AVG_LOCAL_ADDRESS_BROADCAST_INTERVAL);
        }

        if (pto->nNextAddrSend < nNow) {
            pto->nNextAddrSend = PoissonNextSend(nNow, AVG_ADDRESS_BROADCAST_INTERVAL);
            vector<CAddress> vAddr;
            vAddr.reserve(pto->vAddrToSend.size());
            BOOST_FOREACH (const CAddress& addr, pto->vAddrToSend) {
                if (!pto->addrKnown.contains(addr.GetKey())) {
                    pto->addrKnown.insert(addr.GetKey());
                    vAddr.push_back(addr);

                    if (vAddr.size() >= 1000) {
                        pto->PushMessage(NetMsgType::ADDR, vAddr);
                        vAddr.clear();
                    }
                }
            }
            pto->vAddrToSend.clear();
            if (!vAddr.empty())
                pto->PushMessage(NetMsgType::ADDR, vAddr);

            if (pto->vAddrToSend.capacity() > 40)
                pto->vAddrToSend.shrink_to_fit();
        }

        CNodeState& state = *State(pto->GetId());
        if (state.fShouldBan) {
            if (pto->fWhitelisted)
                LogPrintf("Warning: not punishing whitelisted peer %s!\n", pto->addr.ToString());
            else {
                pto->fDisconnect = true;
                if (pto->addr.IsLocal())
                    LogPrintf("Warning: not banning local peer %s!\n", pto->addr.ToString());
                else {
                    CNode::Ban(pto->addr, BanReasonNodeMisbehaving);
                }
            }
            state.fShouldBan = false;
        }

        BOOST_FOREACH (const CBlockReject& reject, state.rejects)
            pto->PushMessage(NetMsgType::REJECT, (string)NetMsgType::BLOCK, reject.chRejectCode, reject.strRejectReason, reject.hashBlock);
        state.rejects.clear();

        if (pindexBestHeader == NULL)
            pindexBestHeader = chainActive.Tip();
        bool fFetch = state.fPreferredDownload || (nPreferredDownload == 0 && !pto->fClient && !pto->fOneShot); // Download if this is a nice peer, or we have no nice peers and this one might do.
        if (!state.fSyncStarted && !pto->fClient && !fImporting && !fReindex) {

            if ((nSyncStarted == 0 && fFetch) || pindexBestHeader->GetBlockTime() > GetAdjustedTime() - 24 * 60 * 60) {
                state.fSyncStarted = true;
                nSyncStarted++;
                const CBlockIndex* pindexStart = pindexBestHeader;
                /* If possible, start at the block preceding the currently
                   best known header.  This ensures that we always get a
                   non-empty list of headers back as long as the peer
                   is up-to-date.  With a non-empty response, we can initialise
                   the peer's known best block.  This wouldn't be possible
                   if we requested starting at pindexBestHeader and
                   got back an empty response.  */
                if (pindexStart->pprev)
                    pindexStart = pindexStart->pprev;
                LogPrint("net", "initial getheaders (%d) to peer=%d (startheight:%d)\n", pindexStart->nHeight, pto->id, pto->nStartingHeight);
                pto->PushMessage(NetMsgType::GETHEADERS, chainActive.GetLocator(pindexStart), uint256());
            }
        }

        if (!fReindex && !fImporting && !IsInitialBlockDownload()) {
            GetMainSignals().Broadcast(nTimeBestReceived);
        }

        {

            LOCK(pto->cs_inventory);
            vector<CBlock> vHeaders;
            bool fRevertToInv = ((!state.fPreferHeaders && (!state.fPreferHeaderAndIDs || pto->vBlockHashesToAnnounce.size() > 1)) || pto->vBlockHashesToAnnounce.size() > MAX_BLOCKS_TO_ANNOUNCE);
            CBlockIndex* pBestIndex = NULL; // last header queued for delivery
            ProcessBlockAvailability(pto->id); // ensure pindexBestKnownBlock is up-to-date

            if (!fRevertToInv) {
                bool fFoundStartingHeader = false;

                BOOST_FOREACH (const uint256& hash, pto->vBlockHashesToAnnounce) {
                    BlockMap::iterator mi = mapBlockIndex.find(hash);
                    assert(mi != mapBlockIndex.end());
                    CBlockIndex* pindex = mi->second;
                    if (chainActive[pindex->nHeight] != pindex) {

                        fRevertToInv = true;
                        break;
                    }
                    if (pBestIndex != NULL && pindex->pprev != pBestIndex) {

                        fRevertToInv = true;
                        break;
                    }
                    pBestIndex = pindex;
                    if (fFoundStartingHeader) {

                        vHeaders.push_back(pindex->GetBlockHeader());
                    } else if (PeerHasHeader(&state, pindex)) {
                        continue; // keep looking for the first new block
                    } else if (pindex->pprev == NULL || PeerHasHeader(&state, pindex->pprev)) {

                        fFoundStartingHeader = true;
                        vHeaders.push_back(pindex->GetBlockHeader());
                    } else {

                        fRevertToInv = true;
                        break;
                    }
                }
            }
            if (!fRevertToInv && !vHeaders.empty()) {
                if (vHeaders.size() == 1 && state.fPreferHeaderAndIDs) {

                    LogPrint("net", "%s sending header-and-ids %s to peer %d\n", __func__,
                             vHeaders.front().GetHash().ToString(), pto->id);

                    CBlock block;
                    assert(ReadBlockFromDisk(block, pBestIndex, consensusParams));
                    CBlockHeaderAndShortTxIDs cmpctblock(block);
                    pto->PushMessageWithFlag(SERIALIZE_TRANSACTION_NO_WITNESS, NetMsgType::CMPCTBLOCK, cmpctblock);
                    state.pindexBestHeaderSent = pBestIndex;
                } else if (state.fPreferHeaders) {
                    if (vHeaders.size() > 1) {
                        LogPrint("net", "%s: %u headers, range (%s, %s), to peer=%d\n", __func__,
                                 vHeaders.size(),
                                 vHeaders.front().GetHash().ToString(),
                                 vHeaders.back().GetHash().ToString(), pto->id);
                    } else {
                        LogPrint("net", "%s: sending header %s to peer=%d\n", __func__,
                                 vHeaders.front().GetHash().ToString(), pto->id);
                    }
                    pto->PushMessage(NetMsgType::HEADERS, vHeaders);
                    state.pindexBestHeaderSent = pBestIndex;
                } else
                    fRevertToInv = true;
            }
            if (fRevertToInv) {

                if (!pto->vBlockHashesToAnnounce.empty()) {
                    const uint256& hashToAnnounce = pto->vBlockHashesToAnnounce.back();
                    BlockMap::iterator mi = mapBlockIndex.find(hashToAnnounce);
                    assert(mi != mapBlockIndex.end());
                    CBlockIndex* pindex = mi->second;

                    if (chainActive[pindex->nHeight] != pindex) {
                        LogPrint("net", "Announcing block %s not on main chain (tip=%s)\n",
                                 hashToAnnounce.ToString(), chainActive.Tip()->GetBlockHash().ToString());
                    }

                    if (!PeerHasHeader(&state, pindex)) {
                        pto->PushInventory(CInv(MSG_BLOCK, hashToAnnounce));
                        LogPrint("net", "%s: sending inv peer=%d hash=%s\n", __func__,
                                 pto->id, hashToAnnounce.ToString());
                    }
                }
            }
            pto->vBlockHashesToAnnounce.clear();
        }

        vector<CInv> vInv;
        {
            LOCK(pto->cs_inventory);
            vInv.reserve(std::max<size_t>(pto->vInventoryBlockToSend.size(), INVENTORY_BROADCAST_MAX));

            BOOST_FOREACH (const uint256& hash, pto->vInventoryBlockToSend) {
                vInv.push_back(CInv(MSG_BLOCK, hash));
                if (vInv.size() == MAX_INV_SZ) {
                    pto->PushMessage(NetMsgType::INV, vInv);
                    vInv.clear();
                }
            }
            pto->vInventoryBlockToSend.clear();

            bool fSendTrickle = pto->fWhitelisted;
            if (pto->nNextInvSend < nNow) {
                fSendTrickle = true;

                pto->nNextInvSend = PoissonNextSend(nNow, INVENTORY_BROADCAST_INTERVAL >> !pto->fInbound);
            }

            if (fSendTrickle) {
                LOCK(pto->cs_filter);
                if (!pto->fRelayTxes)
                    pto->setInventoryTxToSend.clear();
            }

            if (fSendTrickle && pto->fSendMempool) {
                auto vtxinfo = mempool.infoAll();
                pto->fSendMempool = false;
                CAmount filterrate = 0;
                {
                    LOCK(pto->cs_feeFilter);
                    filterrate = pto->minFeeFilter;
                }

                LOCK(pto->cs_filter);

                for (const auto& txinfo : vtxinfo) {
                    const uint256& hash = txinfo.tx->GetHash();
                    CInv inv(MSG_TX, hash);
                    pto->setInventoryTxToSend.erase(hash);
                    if (filterrate) {
                        if (txinfo.feeRate.GetFeePerK() < filterrate)
                            continue;
                    }
                    if (pto->pfilter) {
                        if (!pto->pfilter->IsRelevantAndUpdate(*txinfo.tx))
                            continue;
                    }
                    pto->filterInventoryKnown.insert(hash);
                    vInv.push_back(inv);
                    if (vInv.size() == MAX_INV_SZ) {
                        pto->PushMessage(NetMsgType::INV, vInv);
                        vInv.clear();
                    }
                }
                pto->timeLastMempoolReq = GetTime();
            }

            if (fSendTrickle) {

                vector<std::set<uint256>::iterator> vInvTx;
                vInvTx.reserve(pto->setInventoryTxToSend.size());
                for (std::set<uint256>::iterator it = pto->setInventoryTxToSend.begin(); it != pto->setInventoryTxToSend.end(); it++) {
                    vInvTx.push_back(it);
                }
                CAmount filterrate = 0;
                {
                    LOCK(pto->cs_feeFilter);
                    filterrate = pto->minFeeFilter;
                }

                CompareInvMempoolOrder compareInvMempoolOrder(&mempool);
                std::make_heap(vInvTx.begin(), vInvTx.end(), compareInvMempoolOrder);

                unsigned int nRelayedTransactions = 0;
                LOCK(pto->cs_filter);
                while (!vInvTx.empty() && nRelayedTransactions < INVENTORY_BROADCAST_MAX) {

                    std::pop_heap(vInvTx.begin(), vInvTx.end(), compareInvMempoolOrder);
                    std::set<uint256>::iterator it = vInvTx.back();
                    vInvTx.pop_back();
                    uint256 hash = *it;

                    pto->setInventoryTxToSend.erase(it);

                    if (pto->filterInventoryKnown.contains(hash)) {
                        continue;
                    }

                    auto txinfo = mempool.info(hash);
                    if (!txinfo.tx) {
                        continue;
                    }
                    if (filterrate && txinfo.feeRate.GetFeePerK() < filterrate) {
                        continue;
                    }
                    if (pto->pfilter && !pto->pfilter->IsRelevantAndUpdate(*txinfo.tx))
                        continue;

                    vInv.push_back(CInv(MSG_TX, hash));
                    nRelayedTransactions++;
                    {

                        while (!vRelayExpiration.empty() && vRelayExpiration.front().first < nNow) {
                            mapRelay.erase(vRelayExpiration.front().second);
                            vRelayExpiration.pop_front();
                        }

                        auto ret = mapRelay.insert(std::make_pair(hash, std::move(txinfo.tx)));
                        if (ret.second) {
                            vRelayExpiration.push_back(std::make_pair(nNow + 15 * 60 * 1000000, ret.first));
                        }
                    }
                    if (vInv.size() == MAX_INV_SZ) {
                        pto->PushMessage(NetMsgType::INV, vInv);
                        vInv.clear();
                    }
                    pto->filterInventoryKnown.insert(hash);
                }
            }
        }
        if (!vInv.empty())
            pto->PushMessage(NetMsgType::INV, vInv);

        nNow = GetTimeMicros();
        if (!pto->fDisconnect && state.nStallingSince && state.nStallingSince < nNow - 1000000 * BLOCK_STALLING_TIMEOUT) {

            LogPrintf("Peer=%d is stalling block download, disconnecting\n", pto->id);
            pto->fDisconnect = true;
        }

        if (!pto->fDisconnect && state.vBlocksInFlight.size() > 0) {
            QueuedBlock& queuedBlock = state.vBlocksInFlight.front();
            int nOtherPeersWithValidatedDownloads = nPeersWithValidatedDownloads - (state.nBlocksInFlightValidHeaders > 0);
            if (nNow > state.nDownloadingSince + consensusParams.nPowTargetSpacing * (BLOCK_DOWNLOAD_TIMEOUT_BASE + BLOCK_DOWNLOAD_TIMEOUT_PER_PEER * nOtherPeersWithValidatedDownloads)) {
                LogPrintf("Timeout downloading block %s from peer=%d, disconnecting\n", queuedBlock.hash.ToString(), pto->id);
                pto->fDisconnect = true;
            }
        }

        vector<CInv> vGetData;
        if (!pto->fDisconnect && !pto->fClient && (fFetch || !IsInitialBlockDownload()) && state.nBlocksInFlight < MAX_BLOCKS_IN_TRANSIT_PER_PEER) {
            vector<CBlockIndex*> vToDownload;
            NodeId staller = -1;
            FindNextBlocksToDownload(pto->GetId(), MAX_BLOCKS_IN_TRANSIT_PER_PEER - state.nBlocksInFlight, vToDownload, staller, consensusParams);
            BOOST_FOREACH (CBlockIndex* pindex, vToDownload) {
                uint32_t nFetchFlags = GetFetchFlags(pto, pindex->pprev, consensusParams);
                vGetData.push_back(CInv(MSG_BLOCK | nFetchFlags, pindex->GetBlockHash()));
                MarkBlockAsInFlight(pto->GetId(), pindex->GetBlockHash(), consensusParams, pindex);
                LogPrint("net", "Requesting block %s (%d) peer=%d\n", pindex->GetBlockHash().ToString(),
                         pindex->nHeight, pto->id);
            }
            if (state.nBlocksInFlight == 0 && staller != -1) {
                if (State(staller)->nStallingSince == 0) {
                    State(staller)->nStallingSince = nNow;
                    LogPrint("net", "Stall started peer=%d\n", staller);
                }
            }
        }

        while (!pto->fDisconnect && !pto->mapAskFor.empty() && (*pto->mapAskFor.begin()).first <= nNow) {
            const CInv& inv = (*pto->mapAskFor.begin()).second;
            if (!AlreadyHave(inv)) {
                if (fDebug)
                    LogPrint("net", "Requesting %s peer=%d\n", inv.ToString(), pto->id);
                vGetData.push_back(inv);
                if (vGetData.size() >= 1000) {
                    pto->PushMessage(NetMsgType::GETDATA, vGetData);
                    vGetData.clear();
                }
            } else {

                pto->setAskFor.erase(inv.hash);
            }
            pto->mapAskFor.erase(pto->mapAskFor.begin());
        }
        if (!vGetData.empty())
            pto->PushMessage(NetMsgType::GETDATA, vGetData);

        if (pto->nVersion >= FEEFILTER_VERSION && GetBoolArg("-feefilter", DEFAULT_FEEFILTER) && !(pto->fWhitelisted && GetBoolArg("-whitelistforcerelay", DEFAULT_WHITELISTFORCERELAY))) {
            CAmount currentFilter = mempool.GetMinFee(GetArg("-maxmempool", DEFAULT_MAX_MEMPOOL_SIZE) * 1000000).GetFeePerK();
            int64_t timeNow = GetTimeMicros();
            if (timeNow > pto->nextSendTimeFeeFilter) {
                CAmount filterToSend = filterRounder.round(currentFilter);
                if (filterToSend != pto->lastSentFeeFilter) {
                    pto->PushMessage(NetMsgType::FEEFILTER, filterToSend);
                    pto->lastSentFeeFilter = filterToSend;
                }
                pto->nextSendTimeFeeFilter = PoissonNextSend(timeNow, AVG_FEEFILTER_BROADCAST_INTERVAL);
            }

            else if (timeNow + MAX_FEEFILTER_CHANGE_DELAY * 1000000 < pto->nextSendTimeFeeFilter && (currentFilter < 3 * pto->lastSentFeeFilter / 4 || currentFilter > 4 * pto->lastSentFeeFilter / 3)) {
                pto->nextSendTimeFeeFilter = timeNow + (insecure_rand() % MAX_FEEFILTER_CHANGE_DELAY) * 1000000;
            }
        }
    }
    return true;
}

std::string CBlockFileInfo::ToString() const
{
    return strprintf("CBlockFileInfo(blocks=%u, size=%u, heights=%u...%u, time=%s...%s)", nBlocks, nSize, nHeightFirst, nHeightLast, DateTimeStrFormat("%Y-%m-%d", nTimeFirst), DateTimeStrFormat("%Y-%m-%d", nTimeLast));
}

ThresholdState VersionBitsTipState(const Consensus::Params& params, Consensus::DeploymentPos pos)
{
    LOCK(cs_main);
    return VersionBitsState(chainActive.Tip(), params, pos, versionbitscache);
}

class CMainCleanup {
public:
    CMainCleanup() {}
    ~CMainCleanup()
    {

        BlockMap::iterator it1 = mapBlockIndex.begin();
        for (; it1 != mapBlockIndex.end(); it1++)
            delete (*it1).second;
        mapBlockIndex.clear();

        mapOrphanTransactions.clear();
        mapOrphanTransactionsByPrev.clear();
    }
} instance_of_cmaincleanup;
