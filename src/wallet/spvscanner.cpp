#include "spvscanner.h"
#include "../net_processing.h"
#include "../validation.h"
#include "wallet.h"

// Maximum number of requests pending. This number should be high enough so that
// the requests can be reasonably distributed over our peers.
const static int nMaxPendingRequests = 512;

CSPVScanner::CSPVScanner(CWallet& _wallet, const CBlockLocator& locator) :
    wallet(_wallet)
{
    LOCK(cs_main);

    lastProcessed = FindForkInGlobalIndex(headerChain, locator);
    requestTip = lastProcessed;

    LogPrint(BCLog::WALLET, "Using %s (height = %d) as last processed SPV block\n",
             lastProcessed->GetBlockHashPoW2().ToString(), lastProcessed->nHeight);

    RegisterValidationInterface(this);
}

CSPVScanner::~CSPVScanner()
{
    UnregisterValidationInterface(this);
}

void CSPVScanner::StartScan()
{
    RequestBlocks();
}

void CSPVScanner::RequestBlocks()
{
    LOCK2(cs_main, wallet.cs_wallet);

    // TODO handle forks, ie might need to revert lastProcessed and/or requestTip

    std::vector<const CBlockIndex*> blocksToRequest;

    // add requests for as long as nMaxPendingRequests is not reached and there are still heigher blocks in headerChain
    while (requestTip->nHeight - lastProcessed->nHeight < nMaxPendingRequests &&
           headerChain.Height() > requestTip->nHeight) {

        requestTip = headerChain.Next(requestTip);
        blocksToRequest.push_back(requestTip);
    }

    AddPriorityDownload(blocksToRequest);

    LogPrint(BCLog::WALLET, "Requested %d blocks for SPV, up to height %d\n", blocksToRequest.size(), requestTip->nHeight);
}

void CSPVScanner::ProcessPriorityRequest(const std::shared_ptr<const CBlock> &block, const CBlockIndex *pindex)
{

    // if this is the block we're waiting for process it and request new block(s)
    if (pindex->pprev == lastProcessed) {
        LOCK2(cs_main, wallet.cs_wallet);

        LogPrint(BCLog::WALLET, "SPV processing block %d\n", pindex->nHeight);

        // TODO handle mempool effects

        std::vector<CTransactionRef> vtxConflicted; // dummy for now
        wallet.BlockConnected(block, pindex, vtxConflicted);

        lastProcessed = pindex;

        CWalletDB walletdb(*wallet.dbw);
        walletdb.WriteLastSPVBlockProcessed(headerChain.GetLocatorPoW2(lastProcessed));

        RequestBlocks();
    }
}

void CSPVScanner::HeaderTipChanged(const CBlockIndex* pTip)
{
    RequestBlocks();
}
