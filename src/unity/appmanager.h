// Copyright (c) 2016-2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#ifndef GULDEN_UNITY_APP_MANAGER_H
#define GULDEN_UNITY_APP_MANAGER_H

#include "signals.h"
#include "scheduler.h"
#include "support/allocators/secure.h"
#include <atomic>
#include <string>
#include <thread>
#include <boost/signals2/signal.hpp>
#include <boost/thread.hpp>

#ifdef WIN32
#include <condition_variable>
#endif

/** Class encapsulating Gulden startup and shutdown.
 * Allows running startup and shutdown in a different thread from the UI thread.
 */
class GuldenAppManager
{
public:
    //NB! Only initialise once, afterwards refer to by gApp static instance.
    GuldenAppManager();
    ~GuldenAppManager();
    static GuldenAppManager* gApp;

    //NB! This runs in a detached thread
    void initialize();

    //NB! This signals, in a sigterm safe way, to shutdownThread that it should start the shutdown process.
    //All actual work takes places inside shutdownThread which is a detached thread
    void shutdown();

    std::atomic<bool> fShutDownHasBeenInitiated;

    //NB! The below signals are -not- from UI thread, if the UI handles them it should take this into account.
    boost::signals2::signal<void (bool initializeResult)> signalAppInitializeResult;
    boost::signals2::signal<bool (), BooleanAndAllReturnValues> signalAboutToInitMain;
    boost::signals2::signal<void ()> signalAppShutdownFinished;
    boost::signals2::signal<void ()> signalAppShutdownStarted;
    boost::signals2::signal<void ()> signalAppShutdownCoreInterrupted;
    boost::signals2::signal<void (std::string exceptionMessage)> signalRunawayException;
private:
    std::mutex appManagerInitShutDownMutex;
    #ifdef WIN32
    bool shouldTerminate = false;
    std::condition_variable sigtermCv;
    #else
    int sigtermFd[2];
    #endif
    void handleRunawayException(const std::exception *e);
    void shutdownThread();

    // App globals, not used internally by GuldenAppManager.
public:
    void setRecoveryPhrase(const SecureString& recoveryPhrase);
    SecureString getRecoveryPhrase();
    void BurnRecoveryPhrase();
    bool isRecovery = false;
private:
    SecureString recoveryPhrase;

    // Passed on to the rest of the app but not used internally by GuldenAppManager.
private:
    boost::thread_group threadGroup;
    CScheduler scheduler;
};

bool ShutdownRequested();

#endif
