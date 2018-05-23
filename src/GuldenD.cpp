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

#include "chainparams.h"
#include "clientversion.h"
#include "compat.h"
#include "fs.h"
#include "rpc/server.h"
#include "init.h"
#include "noui.h"
#include "scheduler.h"
#include "util.h"
#include "httpserver.h"
#include "httprpc.h"
#include "utilstrencodings.h"
#include "net.h"
#include <unity/appmanager.h>

#include <boost/thread.hpp>
#include <boost/interprocess/sync/file_lock.hpp>


#include <stdio.h>

#define _(x) std::string(x)/* Keep the _() around in case gettext or such will be used later to translate non-UI */


/* Introduction text for doxygen: */

/*! \mainpage Developer documentation
 *
 * \section intro_sec Introduction
 *
 * This is the developer documentation of the reference client for an experimental new digital currency called Gulden (https://www.gulden.com),
 * which enables instant payments to anyone, anywhere in the world. Gulden uses peer-to-peer technology to operate
 * with no central authority: managing transactions and issuing money are carried out collectively by the network.
 *
 * The software is a community-driven open source project, released under the MIT license.
 *
 * \section Navigation
 * Use the buttons <code>Namespaces</code>, <code>Classes</code> or <code>Files</code> at the top of the page to start navigating the code.
 */

int exitStatus = EXIT_SUCCESS;
bool shutDownFinalised = false;

void handleFinalShutdown()
{
    shutDownFinalised = true;
}

void WaitForShutdown()
{
    while (!shutDownFinalised)
    {
        MilliSleep(200);
    }
}

void handlePostInitMain()
{
    //fixme: (UNITY) - This is now duplicated, factor this out into a common helper.
    //Also shouldn't this happen earlier in the init process?
    // Make sure only a single Gulden process is using the data directory.
    fs::path pathLockFile = GetDataDir() / ".lock";
    FILE* file = fopen(pathLockFile.string().c_str(), "a"); // empty lock file; created if it doesn't exist.
    if (file) fclose(file);

    try
    {
        static boost::interprocess::file_lock lock(pathLockFile.string().c_str());
        if (!lock.try_lock())
        {
            fprintf(stderr, _("Cannot obtain a lock on data directory %s. %s is probably already running.").c_str(), GetDataDir().string().c_str(), _(PACKAGE_NAME).c_str());
            exitStatus = EXIT_FAILURE;
            GuldenAppManager::gApp->shutdown();
            return;
        }
    }
    catch(const boost::interprocess::interprocess_exception& e)
    {
        fprintf(stderr, _("Cannot obtain a lock on data directory %s. %s is probably already running.").c_str(), GetDataDir().string().c_str(), _(PACKAGE_NAME).c_str());
        exitStatus = EXIT_FAILURE;
        GuldenAppManager::gApp->shutdown();
        return;
    }
}

void handleAppInitResult(bool bResult)
{
    if (!bResult)
    {
        // InitError will have been called with detailed error, which ends up on console
        exitStatus = EXIT_FAILURE;
        GuldenAppManager::gApp->shutdown();
        return;
    }
    handlePostInitMain();
}

bool handlePreInitMain()
{
    if (GetBoolArg("-daemon", false))
    {
        #if HAVE_DECL_DAEMON
        {
            fprintf(stdout, "Gulden server starting\n");

            // Daemonize
            // don't chdir (1), do close FDs (0)
            if (daemon(1, 0))
            {
                fprintf(stderr, "Error: daemon() failed: %s\n", strerror(errno));
                exitStatus = EXIT_FAILURE;
                return false;
            }
        }
        #else
        {
            fprintf(stderr, "Error: -daemon is not supported on this operating system\n");
            exitStatus = EXIT_FAILURE;
            return false;
        }
        #endif // HAVE_DECL_DAEMON
    }
}

//////////////////////////////////////////////////////////////////////////////
//
// Start
//
void AppInit(int argc, char* argv[])
{
    GuldenAppManager appManager; 
    appManager.signalAppInitializeResult.connect(boost::bind(handleAppInitResult, _1));
    appManager.signalAboutToInitMain.connect(&handlePreInitMain);
    appManager.signalAppShutdownFinished.connect(&handleFinalShutdown);

    //fixme: (UNITY) - refactor this some more so that init is taken care of inside the app manager (like with qt app)

    //
    // Parameters
    //
    // If Qt is used, parameters/Gulden.conf are parsed in qt/Gulden.cpp's main()
    ParseParameters(argc, argv);

    // Process help and version before taking care about datadir
    if (IsArgSet("-?") || IsArgSet("-h") ||  IsArgSet("-help") || IsArgSet("-version"))
    {
        std::string strUsage = strprintf(_("%s Daemon"), _(PACKAGE_NAME)) + " " + _("version") + " " + FormatFullVersion() + "\n";

        if (IsArgSet("-version"))
        {
            strUsage += FormatParagraph(LicenseInfo());
        }
        else
        {
            strUsage += "\n" + _("Usage:") + "\n" + "  GuldenD [options]                     " + strprintf(_("Start %s Daemon"), _(PACKAGE_NAME)) + "\n";
            strUsage += "\n" + HelpMessage(HMM_GULDEND);
        }
        fprintf(stdout, "%s", strUsage.c_str());
        return;
    }

    //NB! Must be set before AppInitMain.
    fNoUI = true;

    try
    {
        if (!fs::is_directory(GetDataDir(false)))
        {
            fprintf(stderr, "Error: Specified data directory \"%s\" does not exist.\n", GetArg("-datadir", "").c_str());
            exitStatus = EXIT_FAILURE;
            return;
        }
        try
        {
            ReadConfigFile(GetArg("-conf", GULDEN_CONF_FILENAME));
        }
        catch (const std::exception& e)
        {
            fprintf(stderr,"Error reading configuration file: %s\n", e.what());
            exitStatus = EXIT_FAILURE;
            return;
        }
        // Check for -testnet or -regtest parameter (Params() calls are only valid after this clause)
        try
        {
            SelectParams(ChainNameFromCommandLine());
        }
        catch (const std::exception& e)
        {
            fprintf(stderr, "Error: %s\n", e.what());
            exitStatus = EXIT_FAILURE;
            return;
        }

        // Error out when loose non-argument tokens are encountered on command line
        for (int i = 1; i < argc; i++)
        {
            if (!IsSwitchChar(argv[i][0]))
            {
                fprintf(stderr, "Error: Command line contains unexpected token '%s', see GuldenD -h for a list of options.\n", argv[i]);
                exitStatus = EXIT_FAILURE;
                return;
            }
        }

        // -server defaults to true for GuldenD but not for the GUI so do this here
        SoftSetBoolArg("-server", true);
        // Set this early so that parameter interactions go to console
        InitLogging();
        InitParameterInteraction();

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
}

int main(int argc, char* argv[])
{
    SetupEnvironment();

    // Connect GuldenD signal handlers
    noui_connect();

    AppInit(argc, argv);

    return exitStatus;
}
