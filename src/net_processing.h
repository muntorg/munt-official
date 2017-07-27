// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// File contains modifications by: The Gulden developers
// All modifications:
// Copyright (c) 2018 The Gulden developers
// Authored by: Willem de Jonge (willem@isnapp.nl)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#ifndef GULDEN_NET_PROCESSING_H
#define GULDEN_NET_PROCESSING_H

#include "net.h"
#include "validation/validationinterface.h"

/** if disabled, blocks will not be requested automatically, useful for low-resources-available mode */
static const bool DEFAULT_AUTOMATIC_BLOCK_REQUESTS = true;
/** Default for -maxorphantx, maximum number of orphan transactions kept in memory */
static const unsigned int DEFAULT_MAX_ORPHAN_TRANSACTIONS = 100;
/** Expiration time for orphan transactions in seconds */
static const int64_t ORPHAN_TX_EXPIRE_TIME = 20 * 60;
/** Minimum time between orphan transactions expire time checks in seconds */
static const int64_t ORPHAN_TX_EXPIRE_INTERVAL = 5 * 60;
/** Default number of orphan+recently-replaced txn to keep around for block reconstruction */
static const unsigned int DEFAULT_BLOCK_RECONSTRUCTION_EXTRA_TXN = 100;
/** Headers download timeout expressed in microseconds
 *  Timeout = base + per_header * (expected number of headers) */
static constexpr int64_t HEADERS_DOWNLOAD_TIMEOUT_BASE = 15 * 60 * 1000000; // 15 minutes
static constexpr int64_t HEADERS_DOWNLOAD_TIMEOUT_PER_HEADER = 1000; // 1ms/header
/** Reverse headers download timeout expressed in microseconds
 *  Timeout = base + per_header * (expected number of headers)
 *  Using a model of bits/sec throughput to find a suitable number.
 *    Tb = througput in bits/sec, assume 2Mbit quite slow for current standards, however roundtrip latency also
 *         takes a big part in throughput
 *    Hb = Headersize in bits = 80 * 8 = 640
 *    Hb / Tb = 305 usec/header
 *    So for example when reverse downloading 700K headers it is expected to complete in 700K * 30usec + 1min which
 *    is about 4.5min (this is really very slow, in practice we should see most reverse headers sync complete well under a minute).
*/
static constexpr int64_t RHEADERS_DOWNLOAD_TIMEOUT_BASE = 1 * 60 * 1000000; // 1 minute
static constexpr int64_t RHEADERS_DOWNLOAD_TIMEOUT_PER_HEADER = 305; // 305usec/header

/** Register with a network node to receive its signals */
void RegisterNodeSignals(CNodeSignals& nodeSignals);
/** Unregister a network node */
void UnregisterNodeSignals(CNodeSignals& nodeSignals);

class PeerLogicValidation : public CValidationInterface {
private:
    CConnman* connman;

public:
    PeerLogicValidation(CConnman* connmanIn);

    void BlockConnected(const std::shared_ptr<const CBlock>& pblock, const CBlockIndex* pindexConnected, const std::vector<CTransactionRef>& vtxConflicted) override;
    void UpdatedBlockTip(const CBlockIndex *pindexNew, const CBlockIndex *pindexFork, bool fInitialDownload) override;
    void BlockChecked(const CBlock& block, const CValidationState& state) override;
    void NewPoWValidBlock(const CBlockIndex *pindex, const std::shared_ptr<const CBlock>& pblock) override;
};

struct CNodeStateStats {
    int nMisbehavior;
    int nSyncHeight;
    int nCommonHeight;
    std::vector<int> vHeightInFlight;
};

/** Get statistics from node state */
bool GetNodeStateStats(NodeId nodeid, CNodeStateStats &stats);
/** Increase a node's misbehavior score. */
void Misbehaving(NodeId nodeid, int howmuch);

/** Process protocol messages received from a given node */
bool ProcessMessages(CNode* pfrom, CConnman& connman, const std::atomic<bool>& interrupt);
/**
 * Send queued protocol messages to be sent to a give node.
 *
 * @param[in]   pto             The node which we are sending messages to.
 * @param[in]   connman         The connection manager for that node.
 * @param[in]   interrupt       Interrupt condition for processing threads
 * @return                      True if there is more work to be done
 */
bool SendMessages(CNode* pto, CConnman& connman, const std::atomic<bool>& interrupt);

void SetAutoRequestBlocks(bool state);
bool isAutoRequestingBlocks();

/**
 * Prioritize a block for downloading
 * Blocks requested with priority will be downloaded and processed first
 * Downloaded blocks will not trigger ActivateBestChain
 */
void AddPriorityDownload(const std::vector<const CBlockIndex*>& blocksToDownload);
void ProcessPriorityRequests(const std::shared_ptr<CBlock> block);
#endif // GULDEN_NET_PROCESSING_H
