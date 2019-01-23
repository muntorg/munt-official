// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// File contains modifications by: The Gulden developers
// All modifications:
// Copyright (c) 2017-2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#ifndef GULDEN_VALIDATIONINTERFACE_H
#define GULDEN_VALIDATIONINTERFACE_H

#if defined(HAVE_CONFIG_H)
#include "config/gulden-config.h"
#endif

#include <boost/signals2/signal.hpp>
#include <memory>

#include "generation/generation.h"

#include "primitives/transaction.h" // CTransaction(Ref)

class CBlock;
class CBlockIndex;
struct CBlockLocator;
class CBlockIndex;
class CConnman;
class CValidationInterface;
class CValidationState;
class uint256;
class CAccount;
enum class MemPoolRemovalReason;

// These functions dispatch to one or all registered wallets

/** Register a wallet to receive updates from core */
void RegisterValidationInterface(CValidationInterface* pwalletIn);
/** Unregister a wallet from core */
void UnregisterValidationInterface(CValidationInterface* pwalletIn);
/** Unregister all wallets from core */
void UnregisterAllValidationInterfaces();

class CValidationInterface {
public:
    virtual ~CValidationInterface() {}
protected:
    virtual void StalledWitness([[maybe_unused]] const CBlockIndex* pBlock, [[maybe_unused]] uint64_t nSeconds) {}
    virtual void UpdatedBlockTip([[maybe_unused]] const CBlockIndex *pindexNew, [[maybe_unused]] const CBlockIndex *pindexFork, [[maybe_unused]] bool fInitialDownload) {}
    virtual void TransactionAddedToMempool([[maybe_unused]] const CTransactionRef &ptxn) {}
    virtual void TransactionDeletedFromMempool([[maybe_unused]] const uint256 &hash, [[maybe_unused]] MemPoolRemovalReason reason) {}
    virtual void BlockConnected([[maybe_unused]] const std::shared_ptr<const CBlock> &block, [[maybe_unused]] const CBlockIndex *pindex, [[maybe_unused]] const std::vector<CTransactionRef> &txnConflicted) {}
    virtual void BlockDisconnected([[maybe_unused]] const std::shared_ptr<const CBlock> &block) {}
    virtual void SetBestChain([[maybe_unused]] const CBlockLocator &locator) {}
    virtual void ResendWalletTransactions([[maybe_unused]] int64_t nBestBlockTime, [[maybe_unused]] CConnman* connman) {}
    virtual void BlockChecked([[maybe_unused]] const CBlock&, [[maybe_unused]] const CValidationState&) {}
    virtual void GetScriptForMining([[maybe_unused]] std::shared_ptr<CReserveKeyOrScript>&, [[maybe_unused]] CAccount* forAccount) {};
    virtual void GetScriptForWitnessing([[maybe_unused]] std::shared_ptr<CReserveKeyOrScript>&, [[maybe_unused]] CAccount* forAccount) {};
    virtual void NewPoWValidBlock([[maybe_unused]] const CBlockIndex *pindex, [[maybe_unused]] const std::shared_ptr<const CBlock>& block) {};
    friend void ::RegisterValidationInterface([[maybe_unused]] CValidationInterface* interface);
    friend void ::UnregisterValidationInterface([[maybe_unused]] CValidationInterface* interface);
    friend void ::UnregisterAllValidationInterfaces();
};

struct CMainSignals {
    //! Notifies listeners of a stalled witness at tip of chain
    boost::signals2::signal<void (const CBlockIndex *, uint64_t nSeconds)> StalledWitness;

    /** Notifies listeners of updated block chain tip */
    boost::signals2::signal<void (const CBlockIndex *, const CBlockIndex *, bool fInitialDownload)> UpdatedBlockTip;
    /** Notifies listeners of a transaction having been added to mempool. */
    boost::signals2::signal<void (const CTransactionRef &)> TransactionAddedToMempool;
    /** Notifies listeners of a transaction having been removed from mempool. Currently only triggered with MemPoolRemovalReason::EXPIRY. */
    boost::signals2::signal<void (const uint256& hash, MemPoolRemovalReason reason)> TransactionDeletedFromMempool;
    /**
     * Notifies listeners of a block being connected.
     * Provides a vector of transactions evicted from the mempool as a result.
     */
    boost::signals2::signal<void (const std::shared_ptr<const CBlock> &, const CBlockIndex *pindex, const std::vector<CTransactionRef> &)> BlockConnected;
    /** Notifies listeners of a block being disconnected */
    boost::signals2::signal<void (const std::shared_ptr<const CBlock> &)> BlockDisconnected;
    /** Notifies listeners of a new active block chain. */
    boost::signals2::signal<void (const CBlockLocator &)> SetBestChain;
    /** Tells listeners to broadcast their data. */
    boost::signals2::signal<void (int64_t nBestBlockTime, CConnman* connman)> Broadcast;
    /**
     * Notifies listeners of a block validation result.
     * If the provided CValidationState IsValid, the provided block
     * is guaranteed to be the current best block at the time the
     * callback was generated (not necessarily now)
     */
    boost::signals2::signal<void (const CBlock&, const CValidationState&)> BlockChecked;
    /** Notifies listeners that a key for mining is required (coinbase) */
    boost::signals2::signal<void (std::shared_ptr<CReserveKeyOrScript>&, CAccount* forAccount)> ScriptForMining;

    //! Notifies listeners that a key for witnessing is required (to generate non-compound coinbase outputs)
    boost::signals2::signal<void (std::shared_ptr<CReserveKeyOrScript>&, CAccount* forAccount)> ScriptForWitnessing;

    /**
     * Notifies listeners that a block which builds directly on our current tip
     * has been received and connected to the headers tree, though not validated yet */
    boost::signals2::signal<void (const CBlockIndex *, const std::shared_ptr<const CBlock>&)> NewPoWValidBlock;
};

CMainSignals& GetMainSignals();

#endif // GULDEN_VALIDATIONINTERFACE_H
