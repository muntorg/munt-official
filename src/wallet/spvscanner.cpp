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
const int64_t MAX_FORK_DURATION = 7 * 24 * 3600;

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
    lastPersistTime(0)
{
    LOCK(cs_main);

    int64_t seedTime = wallet.nTimeFirstKey;
    startTime =  std::max(int64_t(0), seedTime - MAX_FORK_DURATION);
}

CSPVScanner::~CSPVScanner()
{
}

void CSPVScanner::StartScan()
{
    CWalletDB walletdb(*wallet.dbw);
    CBlockLocator locator;
    int64_t headerStartTime;
    if (walletdb.ReadLastSPVBlockProcessed(locator, headerStartTime))
        headerStartTime = headerStartTime - MAX_FORK_DURATION;
    else
        headerStartTime = startTime;

    StartPartialHeaders(headerStartTime, std::bind(&CSPVScanner::HeaderTipChanged, this, std::placeholders::_1));
}

const CBlockIndex* CSPVScanner::LastBlockProcessed() const
{
    return lastProcessed;
}

void CSPVScanner::RequestBlocks()
{
    LOCK2(cs_main, wallet.cs_wallet);

    // put lastProcessed and/or requestTip back on chain if forked
    while (!headerChain.Contains(requestTip)) {
        if (requestTip->nHeight > lastProcessed->nHeight) {
            CancelPriorityDownload(requestTip, std::bind(&CSPVScanner::ProcessPriorityRequest, this, std::placeholders::_1, std::placeholders::_2));
            requestTip = requestTip->pprev;
        }
        else { // so here requestTip == lastProcessed
            std::shared_ptr<CBlock> pblock = std::make_shared<CBlock>();
            if (!ReadBlockFromDisk(*pblock, lastProcessed, Params())) {
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
    while (skip->GetBlockTime() < startTime && headerChain.Height() > skip->nHeight)
        skip = headerChain.Next(skip);
    if (skip != lastProcessed) {
        LogPrint(BCLog::WALLET, "Skipping %d old blocks for SPV scan, up to height %d\n", skip->nHeight - lastProcessed->nHeight, skip->nHeight);
        UpdateLastProcessed(skip);
        if (lastProcessed->nHeight > requestTip->nHeight) {
            requestTip = lastProcessed;
            startHeight = lastProcessed->nHeight;
        }
    }

    std::vector<const CBlockIndex*> blocksToRequest;

    // add requests for as long as nMaxPendingRequests is not reached and there are still heigher blocks in headerChain
    while (requestTip->nHeight - lastProcessed->nHeight < MAX_PENDING_REQUESTS &&
           headerChain.Height() > requestTip->nHeight) {

        requestTip = headerChain.Next(requestTip);
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
    if (chainActive.Tip() == headerChain.Tip()) {
        LogPrint(BCLog::WALLET, "chainActive is up-to-date, skipping SPV processing block %d\n", pindex->nHeight);
        if (lastProcessed != headerChain.Tip()) {
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

        if (headerChain.Height() == pindex->nHeight || pindex->nHeight % UI_UPDATE_LIMIT == 0)
            uiInterface.NotifySPVProgress(startHeight, pindex->nHeight, headerChain.Height());

        blocksSincePersist++;
    }
}

void CSPVScanner::HeaderTipChanged(const CBlockIndex* pTip)
{
    // initialization on the first header tip notification
    if (lastProcessed == nullptr)
    {
        CWalletDB walletdb(*wallet.dbw);
        CBlockLocator locator;
        int64_t time;
        if (walletdb.ReadLastSPVBlockProcessed(locator, time)) {
            // TODO search fork up to start of partial chain and ensure that stat of partial chain is early enough
            // so we can safely use it is our lastProcessed
            lastProcessed = FindForkInGlobalIndex(headerChain, locator);
        }
        else
        {
            // TODO use start of partial chain to init lastProcessed, check that it is early enough
        }

        requestTip = lastProcessed;
        startHeight = lastProcessed->nHeight;

        LogPrint(BCLog::WALLET, "SPV init using %s (height = %d) as last processed block\n",
                 lastProcessed->GetBlockHashPoW2().ToString(), lastProcessed->nHeight);
    }

    RequestBlocks();
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
    CWalletDB walletdb(*wallet.dbw);
    walletdb.WriteLastSPVBlockProcessed(headerChain.GetLocatorPoW2(lastProcessed), lastProcessed->GetBlockTime());

    lastPersistTime = GetAdjustedTime();
    blocksSincePersist = 0;
}
