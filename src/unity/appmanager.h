// Copyright (c) 2016-2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#ifndef GULDEN_UNITY_APP_MANAGER_H
#define GULDEN_UNITY_APP_MANAGER_H

#include "signals.h"
#include "scheduler.h"
#include "base58.h"
#include "support/allocators/secure.h"
#include <atomic>
#include <string>
#include <thread>
#include <boost/signals2/signal.hpp>
#include <boost/thread.hpp>
#include <condition_variable>


/** Class encapsulating Gulden startup and shutdown.
 * Allows running startup and shutdown in a different thread from the UI thread.
 */
class GuldenAppManager
{
public:
    //! NB! Only initialise once, afterwards refer to by gApp static instance.
    GuldenAppManager();
    ~GuldenAppManager();
    static GuldenAppManager* gApp;

    //! NB! This runs in a detached thread
    void initialize();

    //! NB! This signals, in a sigterm safe way, to shutdownThread that it should start the shutdown process.
    //! All actual work takes places inside shutdownThread which is a detached thread
    void shutdown();

    //! Explicitly wait for shutdown to complete
    void waitForShutDown();

    //! This places the app in a daemonised state.
    //! NB! Always call this before calling initialise.
    bool daemonise();

    std::atomic<bool> fShutDownHasBeenInitiated;

    //NB! The below signals are -not- from UI thread, if the UI handles them it should take this into account.
    boost::signals2::signal<void (bool initializeResult)> signalAppInitializeResult;
    boost::signals2::signal<bool (), BooleanAndAllReturnValues> signalAboutToInitMain;
    boost::signals2::signal<void ()> signalAppShutdownStarted;
    boost::signals2::signal<void ()> signalAppShutdownAlertUser;
    boost::signals2::signal<void ()> signalAppShutdownCoreInterrupted;
    boost::signals2::signal<void ()> signalAppShutdownFinished;
    boost::signals2::signal<void (std::string exceptionMessage)> signalRunawayException;
private:
    std::mutex shutdownFinishMutex;
    std::condition_variable shutdownFinishCondition;
    bool shutdownDidFinish;

    std::mutex appManagerInitShutDownMutex;
    #ifdef WIN32
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
    // Set to true if we are busy starting a new wallet via "recovery phrase"
    bool isRecovery = false;
    // Set to true if we are busy starting a new wallet via "link"
    bool isLink = false;

    // fixme: (SPV) move these recovery helpers to a better place
    int getRecoveryBirth() const;
    int64_t getRecoveryBirthTime() const;
    void setRecoveryBirthNumber(int _recoveryBirth);
    void setRecoveryBirthTime(int64_t birthTime);
    SecureString getCombinedRecoveryPhrase() const;
    static SecureString composeRecoveryPhrase(const SecureString& phrase, int64_t birthTime);
    void setCombinedRecoveryPhrase(const SecureString& combinedPhrase);
    static void splitRecoveryPhraseAndBirth(const SecureString& input, SecureString& phrase, int& birthNumber);
    void setLinkKey(CGuldenSecretExt<CExtKey> _linkKey);
    int64_t getLinkedBirthTime() const;
    CGuldenSecretExt<CExtKey> getLinkedKey() const;
private:
    CGuldenSecretExt<CExtKey> linkKey;
    SecureString recoveryPhrase;
    int recoveryBirthNumber;

    // Passed on to the rest of the app but not used internally by GuldenAppManager.
    boost::thread_group threadGroup;
    CScheduler scheduler;
};

bool ShutdownRequested();

#endif
