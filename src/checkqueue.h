// Copyright (c) 2012-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CHECKQUEUE_H
#define BITCOIN_CHECKQUEUE_H

#include <algorithm>
#include <vector>

#include <boost/foreach.hpp>
#include <boost/thread/condition_variable.hpp>
#include <boost/thread/locks.hpp>
#include <boost/thread/mutex.hpp>

template <typename T>
class CCheckQueueControl;

/** 
 * Queue for verifications that have to be performed.
  * The verifications are represented by a type T, which must provide an
  * operator(), returning a bool.
  *
  * One thread (the master) is assumed to push batches of verifications
  * onto the queue, where they are processed by N-1 worker threads. When
  * the master is done adding work, it temporarily joins the worker pool
  * as an N'th worker, until all jobs are done.
  */
template <typename T>
class CCheckQueue {
private:
    boost::mutex mutex;

    boost::condition_variable condWorker;

    boost::condition_variable condMaster;

    std::vector<T> queue;

    int nIdle;

    int nTotal;

    bool fAllOk;

    /**
     * Number of verifications that haven't completed yet.
     * This includes elements that are no longer queued, but still in the
     * worker's own batches.
     */
    unsigned int nTodo;

    bool fQuit;

    unsigned int nBatchSize;

    /** Internal function that does bulk of the verification work. */
    bool Loop(bool fMaster = false)
    {
        boost::condition_variable& cond = fMaster ? condMaster : condWorker;
        std::vector<T> vChecks;
        vChecks.reserve(nBatchSize);
        unsigned int nNow = 0;
        bool fOk = true;
        do {
            {
                boost::unique_lock<boost::mutex> lock(mutex);

                if (nNow) {
                    fAllOk &= fOk;
                    nTodo -= nNow;
                    if (nTodo == 0 && !fMaster)

                        condMaster.notify_one();
                } else {

                    nTotal++;
                }

                while (queue.empty()) {
                    if ((fMaster || fQuit) && nTodo == 0) {
                        nTotal--;
                        bool fRet = fAllOk;

                        if (fMaster)
                            fAllOk = true;

                        return fRet;
                    }
                    nIdle++;
                    cond.wait(lock); // wait
                    nIdle--;
                }

                nNow = std::max(1U, std::min(nBatchSize, (unsigned int)queue.size() / (nTotal + nIdle + 1)));
                vChecks.resize(nNow);
                for (unsigned int i = 0; i < nNow; i++) {

                    vChecks[i].swap(queue.back());
                    queue.pop_back();
                }

                fOk = fAllOk;
            }

            BOOST_FOREACH (T& check, vChecks)
                if (fOk)
                    fOk = check();
            vChecks.clear();
        } while (true);
    }

public:
    CCheckQueue(unsigned int nBatchSizeIn)
        : nIdle(0)
        , nTotal(0)
        , fAllOk(true)
        , nTodo(0)
        , fQuit(false)
        , nBatchSize(nBatchSizeIn)
    {
    }

    void Thread()
    {
        Loop();
    }

    bool Wait()
    {
        return Loop(true);
    }

    void Add(std::vector<T>& vChecks)
    {
        boost::unique_lock<boost::mutex> lock(mutex);
        BOOST_FOREACH (T& check, vChecks) {
            queue.push_back(T());
            check.swap(queue.back());
        }
        nTodo += vChecks.size();
        if (vChecks.size() == 1)
            condWorker.notify_one();
        else if (vChecks.size() > 1)
            condWorker.notify_all();
    }

    ~CCheckQueue()
    {
    }

    bool IsIdle()
    {
        boost::unique_lock<boost::mutex> lock(mutex);
        return (nTotal == nIdle && nTodo == 0 && fAllOk == true);
    }
};

/** 
 * RAII-style controller object for a CCheckQueue that guarantees the passed
 * queue is finished before continuing.
 */
template <typename T>
class CCheckQueueControl {
private:
    CCheckQueue<T>* pqueue;
    bool fDone;

public:
    CCheckQueueControl(CCheckQueue<T>* pqueueIn)
        : pqueue(pqueueIn)
        , fDone(false)
    {

        if (pqueue != NULL) {
            bool isIdle = pqueue->IsIdle();
            assert(isIdle);
        }
    }

    bool Wait()
    {
        if (pqueue == NULL)
            return true;
        bool fRet = pqueue->Wait();
        fDone = true;
        return fRet;
    }

    void Add(std::vector<T>& vChecks)
    {
        if (pqueue != NULL)
            pqueue->Add(vChecks);
    }

    ~CCheckQueueControl()
    {
        if (!fDone)
            Wait();
    }
};

#endif // BITCOIN_CHECKQUEUE_H
