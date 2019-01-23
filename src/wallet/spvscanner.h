// Copyright (c) 2018 The Gulden developers
// Authored by: Willem de Jonge (willem@isnapp.nl)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#ifndef SPVSCANNER_H
#define SPVSCANNER_H

#include "../validation/validationinterface.h"
#include <atomic>

class CWallet;

class CSPVScanner
{
public:
    CSPVScanner(CWallet& _wallet);
    ~CSPVScanner();

    bool StartScan();

    const CBlockIndex* LastBlockProcessed() const;

    CSPVScanner(const CSPVScanner&) = delete;
    CSPVScanner& operator=(const CSPVScanner&) = delete;

    // write locator for lastProcessed to db for resuming next session, call sparingly
    void Persist();

    void ResetUnifiedProgressNotification();

    static int getProcessedHeight();

private:
    CWallet& wallet;

    // timestamp to start block requests, older blocks are known to have no
    // transactions for the wallet
    int64_t startTime;

    // SPV scan processed up to this block
    const CBlockIndex* lastProcessed;

    // Blocks (lastProcessed .. requstTip] have been requested and are pending
    const CBlockIndex* requestTip;

    // Session start height for progress reporting
    int startHeight;

    void HeaderTipChanged(const CBlockIndex* pTip);

    // Number of connections for progress reporting
    // (mirrored from net using NotifyNumConnectionsChanged signal)
    int numConnections;
    void OnNumConnectionsChanged(int newNumConnections);

    // Calculate unified progress and trigger
    float lastProgressReported;
    void NotifyUnifiedProgress();

    void RequestBlocks();

    // Update value of lastProcessed to pindex and persist it to the wallet db
    void UpdateLastProcessed(const CBlockIndex* pindex);

    void ProcessPriorityRequest(const std::shared_ptr<const CBlock> &block, const CBlockIndex *pindex);

    // timestamp of peristed last processed block
    int64_t lastPersistedBlockTime;

    // last time when scan progress was persisted to the db
    int64_t lastPersistTime;

    // blocks processed since last persist
    int blocksSincePersist;

    // bookeeping for monitoring
    static std::atomic<int> lastProcessedHeight;
};

#endif // SPVSCANNER_H
