// Copyright (c) 2018-2022 The Centure developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the GNU Lesser General Public License v3, see the accompanying
// file COPYING

#ifndef UNITY_IMPL
#define UNITY_IMPL

#include "i_library_controller.hpp"
#include "wallet/wallet.h"
#include "transaction_record.hpp"

const int RECOMMENDED_CONFIRMATIONS = 3;
const int BALANCE_NOTIFY_THRESHOLD_MS = 4000;
const int NEW_MUTATIONS_NOTIFY_THRESHOLD_MS = 10000;

extern std::shared_ptr<ILibraryListener> signalHandler;

extern TransactionRecord calculateTransactionRecordForWalletTransaction(const CWalletTx& wtx, std::vector<CAccount*>& forAccounts, bool& anyInputsOrOutputsAreMine);
extern std::vector<TransactionRecord> getTransactionHistoryForAccount(CAccount* forAccount);
extern std::vector<MutationRecord> getMutationHistoryForAccount(CAccount* forAccount);
extern void addMutationsForTransaction(const CWalletTx* wtx, std::vector<MutationRecord>& mutations, CAccount* forAccount);

extern boost::asio::io_context ioctx;
extern boost::asio::executor_work_guard<boost::asio::io_context::executor_type> work;
extern boost::thread run_thread;

/* Helper for rate limiting notifications.
 * This helper is agnostic of state like chain sync etc.
 * It is limited by a minimum time that has to pass between notifications. Any updates that come in too fast will
 * be absorbed. Finally the last update is guaranteed to be delivered and will take the latest data.
 * So for example with a rate limit of 3 seconds we get this scenario:
 * trigger(1)
 * trigger(2)
 * wait of 1 seconds
 * trigger(3)
 * trigger(4)
 * wait of 4 seconds
 * trigger(5)
 * trigger(6)
 * will see trigger 1 (or 2 depending on thread scheduling) delivered at t=0s, 4 at t=3s and finally 6 at t=6s.
 */
template <typename EventData> class CRateLimit
{
public:

    CRateLimit(std::function<void (const EventData&)> _notifier, const std::chrono::milliseconds& _timeLimit):
        notifier(_notifier),
        timeLimit(_timeLimit),
        pending(false),
        lastTimeHandled(std::chrono::steady_clock::now() - timeLimit),
        timer(ioctx) {}

    virtual ~CRateLimit()
    {
        timer.cancel();
    }

    void trigger(const EventData& data)
    {
        std::scoped_lock lock(mtx);
        mostRecentData = data;
        if (!pending) {
            pending = true;
            timer.expires_at(lastTimeHandled + timeLimit);
            timer.async_wait(boost::bind(&CRateLimit::handler, this));
        }
    }

    void handler() {
        EventData data;
        {
            std::scoped_lock lock(mtx);
            data = mostRecentData;
            pending = false;
            lastTimeHandled = std::chrono::steady_clock::now();

        }
        notifier(data); // notify using a copy of event data such that the notifier will not block new triggers
    }

private:
    std::function<void (const EventData&)> notifier;
    boost::asio::steady_timer::duration timeLimit;

    std::mutex mtx;
    bool pending;
    EventData mostRecentData;
    std::chrono::time_point<std::chrono::steady_clock> lastTimeHandled;
    boost::asio::steady_timer timer;
};

#endif
