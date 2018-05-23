// Copyright (c) 2016-2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#include "appmanager.h"
#include "util.h"
#include "init.h"
#include "warnings.h"

// Below  includes is for the socketpair that controls shutdown.
#ifdef WIN32
#include <sys/types.h>
#endif



GuldenAppManager* GuldenAppManager::gApp = nullptr;

GuldenAppManager::GuldenAppManager()
: fShutDownHasBeenInitiated(false)
{
    if (gApp)
        assert(0);

    gApp = this;

    // Create the shutdown handler thread.
    shutdownThread();
}

GuldenAppManager::~GuldenAppManager()
{
    // Refuse to close if initialize() or shutdown() is still busy.
    std::lock_guard<std::mutex> lock(appManagerInitShutDownMutex);
}

void GuldenAppManager::handleRunawayException(const std::exception *e)
{
    PrintExceptionContinue(e, "Runaway exception");
    signalRunawayException((GetWarnings("gui")));
}

void GuldenAppManager::initialize()
{
    std::thread([=]
    {
        RenameThread("Gulden-initialise");
        std::lock_guard<std::mutex> lock(appManagerInitShutDownMutex);
        try
        {
            LogPrintf("%s: Running initialization in thread\n", __func__);
            if (fShutDownHasBeenInitiated)
                return;
            if (!AppInitBasicSetup())
            {
                signalAppInitializeResult(false);
                return;
            }
            if (fShutDownHasBeenInitiated)
                return;
            if (!AppInitParameterInteraction())
            {
                signalAppInitializeResult(false);
                return;
            }
            if (fShutDownHasBeenInitiated)
                return;
            if (!AppInitSanityChecks())
            {
                signalAppInitializeResult(false);
                return;
            }
            if (fShutDownHasBeenInitiated)
                return;

            //fixme: (UNITY) - We handle only the last slot return here - this is fine for now as there -is- only one.
            //However we should just use a custom combiner and boolean && the results to be future safe for other ports.
            if (!signalAboutToInitMain())
            {
                //Start shutdown process.
                shutdown();
                return;
            }

            if (fShutDownHasBeenInitiated)
                return;
            bool rv = AppInitMain(threadGroup, scheduler);
            signalAppInitializeResult(rv);
        }
        catch (const std::exception& e)
        {
            handleRunawayException(&e);
        }
        catch (...)
        {
            handleRunawayException(NULL);
        }
    }).detach();
}

// We use a socket here to signal shutdown to the main app.
// As we (may) have been called from sigterm it is not safe to do anything else here.
// See http://doc.qt.io/qt-5/unix-signals.html for more information.
void GuldenAppManager::shutdown()
{
    // Let the core know that we are in the early process of shutting down.
    // Do this before the mutex so that if we are still in init we can abandon the init.
    fShutDownHasBeenInitiated = true;

    #ifdef WIN32
    sigtermCv.notify_one();
    #else
    char signalClose = 1;
    ::write(sigtermFd[0], &signalClose, sizeof(signalClose));
    #endif
}

void GuldenAppManager::shutdownThread()
{
    #ifndef WIN32
    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, sigtermFd))
       assert(0);
    #endif

    std::thread([=]
    {
        RenameThread("Gulden-shutdown");

        // Block until we are signalled to commence
        #ifdef WIN32
        std::unique_lock<std::mutex> lk(appManagerInitShutDownMutex);
        sigtermCv.wait(lk, [this]{ return fShutDownHasBeenInitiated == true; });
        #else
        char signalClose = 0;
        ::read(sigtermFd[1], &signalClose, sizeof(signalClose));
        LogPrintf("%s: App shutdown requested\n", __func__);
        std::lock_guard<std::mutex> lock(appManagerInitShutDownMutex);
        #endif

        LogPrintf("%s: Running App Shutdown in thread\n", __func__);

        try
        {
            // Allow UI to visually alert start of shutdown progress.
            signalAppShutdownStarted();

            // Notify all core and network threads to start "wrapping up".
            CoreInterrupt(threadGroup);

            // Notify UI that core shutdown has begun and that it should start disconnecting the various models/signals.
            signalAppShutdownCoreInterrupted();

            // Terminate all core threads.
            CoreShutdown(threadGroup);

            LogPrintf("%s: App shutdown finished\n", __func__);
            signalAppShutdownFinished();
        }
        catch (const std::exception& e)
        {
            LogPrintf("%s: App shutdown exception [%s]\n", __func__, e.what());
            handleRunawayException(&e);
        }
        catch (...)
        {
            LogPrintf("%s: App shutdown exception\n", __func__);
            handleRunawayException(NULL);
        }
    }).detach();
}

void GuldenAppManager::setRecoveryPhrase(const SecureString& recoveryPhrase_)
{
    recoveryPhrase = recoveryPhrase_;
}

SecureString GuldenAppManager::getRecoveryPhrase()
{
    return recoveryPhrase;
}

void GuldenAppManager::BurnRecoveryPhrase()
{
    // The below is a 'SecureString' - so no memory burn necessary, it should burn itself.
    recoveryPhrase = "";
}

bool ShutdownRequested()
{
    return GuldenAppManager::gApp->fShutDownHasBeenInitiated;
}
