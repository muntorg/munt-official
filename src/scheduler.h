// Copyright (c) 2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_SCHEDULER_H
#define BITCOIN_SCHEDULER_H

//
// NOTE:
// boost::thread / boost::function / boost::chrono should be ported to
// std::thread / std::function / std::chrono when we support C++11.
//
#include <boost/function.hpp>
#include <boost/chrono/chrono.hpp>
#include <boost/thread.hpp>
#include <map>

//
// Simple class for background tasks that should be run
// periodically or once "after a while"
//
// Usage:
//
// CScheduler* s = new CScheduler();

class CScheduler {
public:
    CScheduler();
    ~CScheduler();

    typedef boost::function<void(void)> Function;

    void schedule(Function f, boost::chrono::system_clock::time_point t);

    void scheduleFromNow(Function f, int64_t deltaSeconds);

    void scheduleEvery(Function f, int64_t deltaSeconds);

    void serviceQueue();

    void stop(bool drain = false);

    size_t getQueueInfo(boost::chrono::system_clock::time_point& first,
                        boost::chrono::system_clock::time_point& last) const;

private:
    std::multimap<boost::chrono::system_clock::time_point, Function> taskQueue;
    boost::condition_variable newTaskScheduled;
    mutable boost::mutex newTaskMutex;
    int nThreadsServicingQueue;
    bool stopRequested;
    bool stopWhenEmpty;
    bool shouldStop() { return stopRequested || (stopWhenEmpty && taskQueue.empty()); }
};

#endif
