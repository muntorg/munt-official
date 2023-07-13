// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// File contains modifications by: The Centure developers
// All modifications:
// Copyright (c) 2017-2022 The Centure developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the GNU Lesser General Public License v3, see the accompanying
// file COPYING

#include "validation/validationinterface.h"

#include <scheduler.h>

#include <future>
#include <unordered_map>
#include <utility>

#include "consensus/validation.h"
#include "validation/validation.h"
#include "wallet/wallet.h"

//! The MainSignalsInstance manages a list of shared_ptr<CValidationInterface>
//! callbacks.
//!
//! A std::unordered_map is used to track what callbacks are currently
//! registered, and a std::list is to used to store the callbacks that are
//! currently registered as well as any callbacks that are just unregistered
//! and about to be deleted when they are done executing.
struct MainSignalsInstance {
private:
    Mutex m_mutex;
    //! List entries consist of a callback pointer and reference count. The
    //! count is equal to the number of current executions of that entry, plus 1
    //! if it's registered. It cannot be 0 because that would imply it is
    //! unregistered and also not being executed (so shouldn't exist).
    struct ListEntry { std::shared_ptr<CValidationInterface> callbacks; int count = 1; };
    std::list<ListEntry> m_list GUARDED_BY(m_mutex);
    std::unordered_map<CValidationInterface*, std::list<ListEntry>::iterator> m_map GUARDED_BY(m_mutex);

public:
    // We are not allowed to assume the scheduler only runs in one thread,
    // but must ensure all callbacks happen in-order, so we end up creating
    // our own queue here :(
    SingleThreadedSchedulerClient m_schedulerClient;

    explicit MainSignalsInstance(CScheduler *pscheduler) : m_schedulerClient(pscheduler) {}

    void Register(std::shared_ptr<CValidationInterface> callbacks)
    {
        LOCK(m_mutex);
        auto inserted = m_map.emplace(callbacks.get(), m_list.end());
        if (inserted.second) inserted.first->second = m_list.emplace(m_list.end());
        inserted.first->second->callbacks = std::move(callbacks);
    }

    void Unregister(CValidationInterface* callbacks)
    {
        LOCK(m_mutex);
        auto it = m_map.find(callbacks);
        if (it != m_map.end()) {
            if (!--it->second->count) m_list.erase(it->second);
            m_map.erase(it);
        }
    }

    //! Clear unregisters every previously registered callback, erasing every
    //! map entry. After this call, the list may still contain callbacks that
    //! are currently executing, but it will be cleared when they are done
    //! executing.
    void Clear()
    {
        LOCK(m_mutex);
        for (const auto& entry : m_map) {
            if (!--entry.second->count) m_list.erase(entry.second);
        }
        m_map.clear();
    }

    template<typename F> void Iterate(F&& f)
    {
        WAIT_LOCK(m_mutex, lock);
        for (auto it = m_list.begin(); it != m_list.end();) {
            ++it->count;
            {
                REVERSE_LOCK(lock);
                f(*it->callbacks);
            }
            it = --it->count ? std::next(it) : m_list.erase(it);
        }
    }
};

static CMainSignals g_signals;

void CMainSignals::RegisterBackgroundSignalScheduler(CScheduler& scheduler)
{
    assert(!m_internals);
    m_internals.reset(new MainSignalsInstance(&scheduler));
}

void CMainSignals::UnregisterBackgroundSignalScheduler()
{
    m_internals.reset(nullptr);
}

void CMainSignals::FlushBackgroundCallbacks()
{
    if (m_internals) {
        m_internals->m_schedulerClient.EmptyQueue();
    }
}

size_t CMainSignals::CallbacksPending()
{
    if (!m_internals) return 0;
    return m_internals->m_schedulerClient.CallbacksPending();
}

CMainSignals& GetMainSignals()
{
    return g_signals;
}

void RegisterSharedValidationInterface(std::shared_ptr<CValidationInterface> callbacks)
{
    // Each connection captures the shared_ptr to ensure that each callback is
    // executed before the subscriber is destroyed. For more details see #18338.
    g_signals.m_internals->Register(std::move(callbacks));
}

void RegisterValidationInterface(CValidationInterface* callbacks)
{
    // Create a shared_ptr with a no-op deleter - CValidationInterface lifecycle
    // is managed by the caller.
    RegisterSharedValidationInterface({callbacks, [](CValidationInterface*){}});
}

void UnregisterSharedValidationInterface(std::shared_ptr<CValidationInterface> callbacks)
{
    UnregisterValidationInterface(callbacks.get());
}

void UnregisterValidationInterface(CValidationInterface* callbacks)
{
    if (g_signals.m_internals) {
        g_signals.m_internals->Unregister(callbacks);
    }
}

void UnregisterAllValidationInterfaces()
{
    if (!g_signals.m_internals) {
        return;
    }
    g_signals.m_internals->Clear();
}

void CallFunctionInValidationInterfaceQueue(std::function<void()> func)
{
    g_signals.m_internals->m_schedulerClient.AddToProcessQueue(std::move(func));
}

void SyncWithValidationInterfaceQueue()
{
    AssertLockNotHeld(cs_main);
    // Block until the validation queue drains
    std::promise<void> promise;
    CallFunctionInValidationInterfaceQueue([&promise] {
        promise.set_value();
    });
    promise.get_future().wait();
}

// Use a macro instead of a function for conditional logging to prevent
// evaluating arguments when logging is not enabled.
//
// NOTE: The lambda captures all local variables by value.
#define ENQUEUE_AND_LOG_EVENT(event, fmt, name, ...)           \
    do {                                                       \
        auto local_name = (name);                              \
        LOG_EVENT("Enqueuing " fmt, local_name, __VA_ARGS__);  \
        m_internals->m_schedulerClient.AddToProcessQueue([=] { \
            LOG_EVENT(fmt, local_name, __VA_ARGS__);           \
            event();                                           \
        });                                                    \
    } while (0)

#define LOG_EVENT(fmt, ...) \
    LogPrint(BCLog::VALIDATION, fmt "\n", __VA_ARGS__)

void CMainSignals::UpdatedBlockTip(const CBlockIndex *pindexNew, const CBlockIndex *pindexFork, bool fInitialDownload) {
    // Dependencies exist that require UpdatedBlockTip events to be delivered in the order in which
    // the chain actually updates. One way to ensure this is for the caller to invoke this signal
    // in the same critical section where the chain is updated

    auto event = [pindexNew, pindexFork, fInitialDownload, this] {
        m_internals->Iterate([&](CValidationInterface& callbacks) { callbacks.UpdatedBlockTip(pindexNew, pindexFork, fInitialDownload); });
    };
    ENQUEUE_AND_LOG_EVENT(event, "%s: new block hash=%s fork block hash=%s (in IBD=%s)", __func__,
                          pindexNew->GetBlockHashPoW2().ToString(),
                          pindexFork ? pindexFork->GetBlockHashPoW2().ToString() : "null",
                          fInitialDownload);
}

void CMainSignals::TransactionAddedToMempool(const CTransactionRef& tx) {
    auto event = [tx, this] {
        m_internals->Iterate([&](CValidationInterface& callbacks) { callbacks.TransactionAddedToMempool(tx); });
    };
    ENQUEUE_AND_LOG_EVENT(event, "%s: txid=%s wtxid=%s", __func__,
                          tx->GetHash().ToString(),
                          tx->GetWitnessHash().ToString());
}

void CMainSignals::TransactionRemovedFromMempool(const uint256 &transactionHash, MemPoolRemovalReason reason)
{
    auto event = [transactionHash, reason, this] {
        m_internals->Iterate([&](CValidationInterface& callbacks) { callbacks.TransactionRemovedFromMempool(transactionHash, reason); });
    };
    ENQUEUE_AND_LOG_EVENT(event, "%s: txid=%s", __func__,
                          transactionHash.ToString());
}

void CMainSignals::BlockConnected(const std::shared_ptr<const CBlock>& pblock, const CBlockIndex* pindex, const std::vector<CTransactionRef>& txnConflicted) {
    auto event = [pblock, pindex, txnConflicted, this] {
        m_internals->Iterate([&](CValidationInterface& callbacks) { callbacks.BlockConnected(pblock, pindex, txnConflicted); });
    };
    ENQUEUE_AND_LOG_EVENT(event, "%s: block hash=%s, block height=%d", __func__,
                          pblock->GetHashPoW2().ToString(), pindex->nHeight);
}

void CMainSignals::BlockDisconnected(const std::shared_ptr<const CBlock>& pblock)
{
    auto event = [pblock, this] {
        m_internals->Iterate([&](CValidationInterface& callbacks) { callbacks.BlockDisconnected(pblock); });
    };
    ENQUEUE_AND_LOG_EVENT(event, "%s: block hash=%s", __func__,
                          pblock->GetHashPoW2().ToString());
}

void CMainSignals::BlockChecked(const CBlock& block, const CValidationState& state) {
    LOG_EVENT("%s: block hash=%s state=%s", __func__,
              block.GetHashPoW2().ToString(), state.ToString());
    m_internals->Iterate([&](CValidationInterface& callbacks) { callbacks.BlockChecked(block, state); });
}

void CMainSignals::NewPoWValidBlock(const CBlockIndex *pindex, const std::shared_ptr<const CBlock> &block) {
    LOG_EVENT("%s: block hash=%s", __func__, block->GetHashPoW2().ToString());
    m_internals->Iterate([&](CValidationInterface& callbacks) { callbacks.NewPoWValidBlock(pindex, block); });
}

void CMainSignals::PruningConflictingBlock (const uint256& orphanBlockHash)
{
    LOG_EVENT("%s: block hash=%s", __func__, orphanBlockHash.ToString());
    m_internals->Iterate([&](CValidationInterface& callbacks) { callbacks.PruningConflictingBlock(orphanBlockHash); });
}

void CMainSignals::Broadcast (int64_t nBestBlockTime, CConnman* connman)
{
    LOG_EVENT("%s: best block time=%d", __func__, nBestBlockTime);
    m_internals->Iterate([&](CValidationInterface& callbacks) { callbacks.Broadcast(nBestBlockTime, connman); });
}

void CMainSignals::ScriptForMining(std::shared_ptr<CReserveKeyOrScript>& scriptForMining, CAccount* forAccount)
{
    LOG_EVENT("%s", __func__);
    m_internals->Iterate([&](CValidationInterface& callbacks) { callbacks.ScriptForMining(scriptForMining, forAccount); });
}

void CMainSignals::ScriptForWitnessing(std::shared_ptr<CReserveKeyOrScript>& scriptForWitnessing, CAccount* forAccount)
{
    LOG_EVENT("%s", __func__);
    m_internals->Iterate([&](CValidationInterface& callbacks) { callbacks.ScriptForWitnessing(scriptForWitnessing, forAccount); });
}

void CMainSignals::SetBestChain(const CBlockLocator& locator)
{
    LOG_EVENT("%s: best block time=%d", __func__, locator.IsNull() ? "null" : locator.vHave.front().ToString());
    m_internals->Iterate([&](CValidationInterface& callbacks) { callbacks.SetBestChain(locator); });
}

void CMainSignals::StalledWitness(const CBlockIndex* pBlockIndex, uint64_t nSeconds)
{
    LOG_EVENT("%s: block hash=%s seconds=%d", __func__, pBlockIndex->GetBlockHashPoW2().ToString(), nSeconds);
    m_internals->Iterate([&](CValidationInterface& callbacks) { callbacks.StalledWitness(pBlockIndex, nSeconds); });
}
    
    
void CMainSignals::WalletTransactionAdded(CWallet* const pWallet, const CWalletTx& wtx)
{
    LOG_EVENT("%s: wallet=%s transactionHash=%d", __func__, pWallet->GetName(), wtx.GetHash().ToString());
    m_internals->Iterate([&](CValidationInterface& callbacks) { callbacks.WalletTransactionAdded(pWallet, wtx); });
}

/*/
void RegisterValidationInterface(CValidationInterface* pwalletIn)
{
    g_signals.StalledWitness.connect(boost::bind(&CValidationInterface::StalledWitness, pwalletIn, _1, _2));
    g_signals.UpdatedBlockTip.connect(boost::bind(&CValidationInterface::UpdatedBlockTip, pwalletIn, _1, _2, _3));
    g_signals.TransactionAddedToMempool.connect(boost::bind(&CValidationInterface::TransactionAddedToMempool, pwalletIn, _1));
    g_signals.TransactionRemovedFromMempool.connect(boost::bind(&CValidationInterface::TransactionRemovedFromMempool, pwalletIn, _1, _2));
    #ifdef ENABLE_WALLET
    g_signals.WalletTransactionAdded.connect(boost::bind(&CValidationInterface::WalletTransactionAdded, pwalletIn, _1, _2));
    #endif
    g_signals.BlockConnected.connect(boost::bind(&CValidationInterface::BlockConnected, pwalletIn, _1, _2, _3));
    g_signals.BlockDisconnected.connect(boost::bind(&CValidationInterface::BlockDisconnected, pwalletIn, _1));
    g_signals.SetBestChain.connect(boost::bind(&CValidationInterface::SetBestChain, pwalletIn, _1));
    g_signals.Broadcast.connect(boost::bind(&CValidationInterface::ResendWalletTransactions, pwalletIn, _1, _2));
    g_signals.BlockChecked.connect(boost::bind(&CValidationInterface::BlockChecked, pwalletIn, _1, _2));
    g_signals.ScriptForMining.connect(boost::bind(&CValidationInterface::GetScriptForMining, pwalletIn, _1, _2));
    g_signals.ScriptForWitnessing.connect(boost::bind(&CValidationInterface::GetScriptForWitnessing, pwalletIn, _1, _2));
    g_signals.NewPoWValidBlock.connect(boost::bind(&CValidationInterface::NewPoWValidBlock, pwalletIn, _1, _2));
    g_signals.PruningConflictingBlock.connect(boost::bind(&CValidationInterface::PruningConflictingBlock, pwalletIn, _1));
}

void UnregisterValidationInterface(CValidationInterface* pwalletIn)
{
    g_signals.NewPoWValidBlock.disconnect(boost::bind(&CValidationInterface::NewPoWValidBlock, pwalletIn, _1, _2));
    g_signals.ScriptForWitnessing.disconnect(boost::bind(&CValidationInterface::GetScriptForWitnessing, pwalletIn, _1, _2));
    g_signals.ScriptForMining.disconnect(boost::bind(&CValidationInterface::GetScriptForMining, pwalletIn, _1, _2));
    g_signals.BlockChecked.disconnect(boost::bind(&CValidationInterface::BlockChecked, pwalletIn, _1, _2));
    g_signals.Broadcast.disconnect(boost::bind(&CValidationInterface::ResendWalletTransactions, pwalletIn, _1, _2));
    g_signals.SetBestChain.disconnect(boost::bind(&CValidationInterface::SetBestChain, pwalletIn, _1));
    g_signals.BlockDisconnected.disconnect(boost::bind(&CValidationInterface::BlockDisconnected, pwalletIn, _1));
    g_signals.BlockConnected.disconnect(boost::bind(&CValidationInterface::BlockConnected, pwalletIn, _1, _2, _3));
    g_signals.TransactionAddedToMempool.disconnect(boost::bind(&CValidationInterface::TransactionAddedToMempool, pwalletIn, _1));
    g_signals.TransactionRemovedFromMempool.disconnect(boost::bind(&CValidationInterface::TransactionRemovedFromMempool, pwalletIn, _1, _2));
    #ifdef ENABLE_WALLET
    g_signals.WalletTransactionAdded.disconnect(boost::bind(&CValidationInterface::WalletTransactionAdded, pwalletIn, _1, _2));
    #endif
    g_signals.UpdatedBlockTip.disconnect(boost::bind(&CValidationInterface::UpdatedBlockTip, pwalletIn, _1, _2, _3));
    g_signals.StalledWitness.disconnect(boost::bind(&CValidationInterface::StalledWitness, pwalletIn, _1, _2));
    g_signals.PruningConflictingBlock.disconnect(boost::bind(&CValidationInterface::PruningConflictingBlock, pwalletIn, _1));
}

void UnregisterAllValidationInterfaces()
{
    g_signals.ScriptForWitnessing.disconnect_all_slots();
    g_signals.ScriptForMining.disconnect_all_slots();
    g_signals.BlockChecked.disconnect_all_slots();
    g_signals.Broadcast.disconnect_all_slots();
    g_signals.SetBestChain.disconnect_all_slots();
    g_signals.TransactionAddedToMempool.disconnect_all_slots();
    g_signals.TransactionRemovedFromMempool.disconnect_all_slots();
    g_signals.BlockConnected.disconnect_all_slots();
    g_signals.BlockDisconnected.disconnect_all_slots();
    g_signals.UpdatedBlockTip.disconnect_all_slots();
    g_signals.NewPoWValidBlock.disconnect_all_slots();
    g_signals.StalledWitness.disconnect_all_slots();
    g_signals.PruningConflictingBlock.disconnect_all_slots();
}*/
