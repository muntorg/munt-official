// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// File contains modifications by: The Gulden developers
// All modifications:
// Copyright (c) 2016-2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#if defined(HAVE_CONFIG_H)
#include "config/gulden-config.h"
#endif

#include "libinit.h"
#include "chainparams.h"
#include "clientversion.h"
#include "compat.h"
#include "fs.h"
#include "init.h"
#include "noui.h"
#include "scheduler.h"
#include "util.h"
#include "utilstrencodings.h"
#include "net.h"
#include <unity/appmanager.h>

#include <boost/thread.hpp>
#include <boost/interprocess/sync/file_lock.hpp>


#include <stdio.h>

#define _(x) std::string(x)/* Keep the _() around in case gettext or such will be used later to translate non-UI */


bool shutDownFinalised = false;

static void handleFinalShutdown()
{
    shutDownFinalised = true;
}

static void WaitForShutdown()
{
    while (!shutDownFinalised)
    {
        MilliSleep(200);
    }
}

extern void handlePostInitMain();
static void handleAppInitResult(bool bResult)
{
    if (!bResult)
    {
        // InitError will have been called with detailed error, which ends up on console
        GuldenAppManager::gApp->shutdown();
        return;
    }
    handlePostInitMain();
}

static bool handlePreInitMain()
{
    return true;
}

//////////////////////////////////////////////////////////////////////////////
//
// Start
//
int InitUnity()
{
    SetupEnvironment();

    // Connect signal handlers
    noui_connect();

    GuldenAppManager appManager; 
    appManager.signalAppInitializeResult.connect(boost::bind(handleAppInitResult, _1));
    appManager.signalAboutToInitMain.connect(&handlePreInitMain);
    appManager.signalAppShutdownFinished.connect(&handleFinalShutdown);

    //NB! Must be set before AppInitMain.
    fNoUI = true;

    try
    {
        if (!fs::is_directory(GetDataDir(false)))
        {
            fprintf(stderr, "Error: Specified data directory \"%s\" does not exist.\n", GetArg("-datadir", "").c_str());
            return EXIT_FAILURE;
        }
        try
        {
            ReadConfigFile(GetArg("-conf", GULDEN_CONF_FILENAME));
        }
        catch (const std::exception& e)
        {
            fprintf(stderr,"Error reading configuration file: %s\n", e.what());
            return EXIT_FAILURE;
        }

        // Check for -testnet or -regtest parameter (Params() calls are only valid after this clause)
        try
        {
            SelectParams(ChainNameFromCommandLine());
        }
        catch (const std::exception& e)
        {
            fprintf(stderr, "Error: %s\n", e.what());
            return EXIT_FAILURE;
        }

        // Set this early so that parameter interactions go to console
        InitLogging();
        InitParameterInteraction();

        //fixme: (UNITY) - This is now duplicated, factor this out into a common helper.
        // NB! This has to happen before we deamonise
        // Make sure only a single Gulden process is using the data directory.
        {
            fs::path pathLockFile = GetDataDir() / ".lock";
            FILE* file = fopen(pathLockFile.string().c_str(), "a"); // empty lock file; created if it doesn't exist.
            if (file)
                fclose(file);

            try
            {
                static boost::interprocess::file_lock lock(pathLockFile.string().c_str());
                if (!lock.try_lock())
                {
                    fprintf(stderr, "Cannot obtain a lock on data directory %s. %s is probably already running.", GetDataDir().string().c_str(), _(PACKAGE_NAME).c_str());
                    GuldenAppManager::gApp->shutdown();
                    return EXIT_FAILURE;
                }
            }
            catch(const boost::interprocess::interprocess_exception& e)
            {
                fprintf(stderr, "Cannot obtain a lock on data directory %s. %s is probably already running.", GetDataDir().string().c_str(), _(PACKAGE_NAME).c_str());
                GuldenAppManager::gApp->shutdown();
                return EXIT_FAILURE;
            }
        }

        appManager.initialize();
    }
    catch (const std::exception& e)
    {
        PrintExceptionContinue(&e, "AppInit()");
    }
    catch (...)
    {
        PrintExceptionContinue(NULL, "AppInit()");
    }

    //fixme: (UNITY) - It would be much better to wait on a condition variable here.
    // Busy poll for shutdown and allow app to exit when we reach there.
    WaitForShutdown();

    return EXIT_SUCCESS;
}
