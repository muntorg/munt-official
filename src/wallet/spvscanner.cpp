// Copyright (c) 2018 The Gulden developers
// Authored by: Willem de Jonge (willem@isnapp.nl)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#include "spvscanner.h"
#include "../net_processing.h"
#include "timedata.h"
#include "../validation/validation.h"
#include "wallet.h"

#include <algorithm>

// Maximum number of requests pending. This number should be high enough so that
// the requests can be reasonably distributed over our peers.
const int MAX_PENDING_REQUESTS = 512;

// Duration (seconds) of longest fork that can be handled
const int64_t MAX_FORK_DURATION = 1 * 12 * 3600;

// Persisting wallet db updates are limited as they are very expensive
// and updating at every processed block would give very poor performance.
// Therefore the state is only written to the db when a minimum time interval has passed,
// or a certain of number of blocks have been processed (whichever comes first).
// Note that due to this some blocks might be re-processed in case of abnormal termination,
// this is fine.

// Interval in seconds for writing the scan progress to the wallet db
const int64_t PERSIST_INTERVAL_SEC = 5;

// Blocks count after which scan processing state is written to the db.
const int PERSIST_BLOCK_COUNT = 500;

// limit UI update notifications (except when catched up)
const int UI_UPDATE_LIMIT = 50;

CSPVScanner::CSPVScanner(CWallet& _wallet) :
    wallet(_wallet),
    startTime(0),
    lastProcessed(nullptr),
    numConnections(0),
    probableHeight(0),
    lastProgressReported(-1.0f),
    lastPersistTime(0)
{
    LOCK(cs_main);

    // init scan starting time to birth of first key
    startTime =  wallet.nTimeFirstKey;

    // forward scan starting time to last block processed if available
    CWalletDB walletdb(*wallet.dbw);
    CBlockLocator locator;
    int64_t lastBlockTime;
    if (walletdb.ReadLastSPVBlockProcessed(locator, lastBlockTime))
        startTime = lastBlockTime;

    // rewind scan starting time by maximum handled fork duration
    startTime = std::max(Params().GenesisBlock().GetBlockTime(), startTime - MAX_FORK_DURATION);
}

CSPVScanner::~CSPVScanner()
{
    uiInterface.NotifyNumConnectionsChanged.disconnect(boost::bind(&CSPVScanner::OnNumConnectionsChanged, this, _1));
    uiInterface.NotifyHeaderProgress.disconnect(boost::bind(&CSPVScanner::OnHeaderProgressChanged, this, _1, _2, _3, _4));
}

bool CSPVScanner::StartScan()
{
    if (StartPartialHeaders(startTime, std::bind(&CSPVScanner::HeaderTipChanged, this, std::placeholders::_1)))
    {
        uiInterface.NotifyNumConnectionsChanged.connect(boost::bind(&CSPVScanner::OnNumConnectionsChanged, this, _1));
        uiInterface.NotifyHeaderProgress.connect(boost::bind(&CSPVScanner::OnHeaderProgressChanged, this, _1, _2, _3, _4));
        HeaderTipChanged(partialChain.Tip());
        NotifyUnifiedProgress();
        return true;
    }
    else
        return false;
}

const CBlockIndex* CSPVScanner::LastBlockProcessed() const
{
    return lastProcessed;
}

void CSPVScanner::RequestBlocks()
{
    LOCK2(cs_main, wallet.cs_wallet);

    // put lastProcessed and/or requestTip back on chain if forked
    while (!partialChain.Contains(requestTip)) {
        if (requestTip->nHeight > lastProcessed->nHeight) {
            CancelPriorityDownload(requestTip, std::bind(&CSPVScanner::ProcessPriorityRequest, this, std::placeholders::_1, std::placeholders::_2));
            requestTip = requestTip->pprev;
        }
        else { // so here requestTip == lastProcessed
            std::shared_ptr<CBlock> pblock = std::make_shared<CBlock>();
            if (ReadBlockFromDisk(*pblock, lastProcessed, Params())) {
                wallet.BlockDisconnected(pblock);
            }
            else {
                // This block must be on disk, it was processed before.
                // So pruning has to keep at least as many blocks back as the longest fork we are willing to handle.
                assert(false);
            }
            UpdateLastProcessed(lastProcessed->pprev);
            requestTip = lastProcessed;
        }
    }

    // skip blocks that are before startTime
    const CBlockIndex* skip = lastProcessed;
    while (skip->GetBlockTime() < startTime && partialChain.Height() > skip->nHeight)
        skip = partialChain.Next(skip);
    if (skip != lastProcessed) {
        LogPrint(BCLog::WALLET, "Skipping %d old blocks for SPV scan, up to height %d\n", skip->nHeight - lastProcessed->nHeight, skip->nHeight);
        UpdateLastProcessed(skip);
        if (lastProcessed->nHeight > requestTip->nHeight) {
            requestTip = lastProcessed;
        }
    }

    std::vector<const CBlockIndex*> blocksToRequest;

    // add requests for as long as nMaxPendingRequests is not reached and there are still heigher blocks in headerChain
    while (requestTip->nHeight - lastProcessed->nHeight < MAX_PENDING_REQUESTS &&
           partialChain.Height() > requestTip->nHeight) {

        requestTip = partialChain.Next(requestTip);
        blocksToRequest.push_back(requestTip);
    }

    if (!blocksToRequest.empty()) {
        LogPrint(BCLog::WALLET, "Requesting %d blocks for SPV, up to height %d\n", blocksToRequest.size(), requestTip->nHeight);
        AddPriorityDownload(blocksToRequest, std::bind(&CSPVScanner::ProcessPriorityRequest, this, std::placeholders::_1, std::placeholders::_2));
    }
}

void CSPVScanner::ProcessPriorityRequest(const std::shared_ptr<const CBlock> &block, const CBlockIndex *pindex)
{
    LOCK2(cs_main, wallet.cs_wallet);

    // if chainActive is up-to-date no SPV blocks need to be requested, we can update SPV to the activeChain
    if (chainActive.Tip() == partialChain.Tip()) {
        LogPrint(BCLog::WALLET, "chainActive is up-to-date, skipping SPV processing block %d\n", pindex->nHeight);
        if (lastProcessed != partialChain.Tip()) {
            UpdateLastProcessed(chainActive.Tip());
            requestTip = lastProcessed;
        }
    }

    // if this is the block we're waiting for process it and request new block(s)
    if (pindex->pprev == lastProcessed) {
        LogPrint(BCLog::WALLET, "SPV processing block %d\n", pindex->nHeight);

        // TODO handle mempool effects

        std::vector<CTransactionRef> vtxConflicted; // dummy for now
        wallet.BlockConnected(block, pindex, vtxConflicted);

        UpdateLastProcessed(pindex);

        RequestBlocks();

        if (partialChain.Height() == pindex->nHeight || pindex->nHeight % UI_UPDATE_LIMIT == 0)
            uiInterface.NotifySPVProgress(startHeight, pindex->nHeight, partialChain.Height());

        NotifyUnifiedProgress();

        blocksSincePersist++;
    }
}

void CSPVScanner::HeaderTipChanged(const CBlockIndex* pTip)
{
    if (pTip)
    {
        // initialization on the first header tip notification
        if (lastProcessed == nullptr)
        {
            if (partialChain.Height() >= partialChain.HeightOffset()
                    && partialChain[partialChain.HeightOffset()]->GetBlockTime() <= startTime)
            {
                // use start of partial chain to init lastProcessed
                // forks are handled when requesting blocks which will also fast-forward to startTime
                // should the headerChain be very early
                lastProcessed = partialChain[partialChain.HeightOffset()];
                requestTip = lastProcessed;
                startHeight = lastProcessed->nHeight;

                LogPrint(BCLog::WALLET, "SPV init using %s (height = %d) as last processed block\n",
                         lastProcessed->GetBlockHashPoW2().ToString(), lastProcessed->nHeight);
            }
            else
            {
                // headerChain not usable, it does not start early enough or has no data.
                // This should not happen, as StartPartialHeaders was explicitly given the startTime
                // so if this occurs it's a bug.
                throw std::runtime_error("partialChain not usable, starting late or too little data");
            }
        }

        RequestBlocks();

        NotifyUnifiedProgress();
    }
    else // pTip == nullptr => partial sync stopped
    {
        CancelAllPriorityDownloads();
        Persist();
    }
}

void CSPVScanner::OnNumConnectionsChanged(int newNumConnections)
{
    numConnections = newNumConnections;
    NotifyUnifiedProgress();
}

void CSPVScanner::OnHeaderProgressChanged(int, int _probableHeight, int, int64_t)
{
    probableHeight = _probableHeight;
}

void CSPVScanner::ResetUnifiedProgressNotification()
{
    lastProgressReported = -1.0f;
    if (lastProcessed)
        startHeight = lastProcessed->nHeight;
    NotifyUnifiedProgress();
}

void CSPVScanner::NotifyUnifiedProgress()
{
    const float CONNECTION_WEIGHT = 0.05f;
    const float MIN_REPORTING_DELTA = 0.002f;
    const float ALWAYS_REPORT_THRESHOLD = 0.9995f;

    float newProgress = 0.0f;

    if (numConnections>0)
        newProgress += CONNECTION_WEIGHT;

    if (probableHeight > 0 && startHeight > 0 && lastProcessed != nullptr && lastProcessed->nHeight > 0) {
        float pgs = (lastProcessed->nHeight - startHeight)/float(probableHeight - startHeight);
        newProgress += (1.0f - CONNECTION_WEIGHT) * pgs;
    }

    // silently ignore progress decrease, this can occur if the chain grew
    // faster then the synchronisation (this would be a very short live situation)
    // and reporting will continue normally when we're catching up again
    if (newProgress <= lastProgressReported)
        return;

    // limit processing overhead and only report if a reasonable amount of progress was made since last report
    if (newProgress - lastProgressReported <= MIN_REPORTING_DELTA && newProgress < ALWAYS_REPORT_THRESHOLD)
        return;

    uiInterface.NotifyUnifiedProgress(newProgress);

    LogPrintf("====== progress = %10.7f\n", newProgress);

    lastProgressReported = newProgress;
}

void CSPVScanner::UpdateLastProcessed(const CBlockIndex* pindex)
{
    lastProcessed = pindex;

    int64_t now = GetAdjustedTime();
    if (now - lastPersistTime > PERSIST_INTERVAL_SEC || blocksSincePersist >= PERSIST_BLOCK_COUNT)
        Persist();
}

void CSPVScanner::Persist()
{
    if (lastProcessed != nullptr)
    {
        {
            // scoped here to ensure that the walletdb is flushed before assuming a new prune height
            CWalletDB walletdb(*wallet.dbw);
            walletdb.WriteLastSPVBlockProcessed(partialChain.GetLocatorPoW2(lastProcessed), lastProcessed->GetBlockTime());
        }
        int maxPruneHeight = lastProcessed->nHeight - PARTIAL_SYNC_PRUNE_HEIGHT;
        if (maxPruneHeight > 0)
            SetMaxSPVPruneHeight(maxPruneHeight);

        lastPersistTime = GetAdjustedTime();
        blocksSincePersist = 0;

        PersistAndPruneForPartialSync(true);
    }
}
