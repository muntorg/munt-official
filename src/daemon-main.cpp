// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// File contains modifications by: The Centure developers
// All modifications:
// Copyright (c) 2016-2022 The Centure developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the GNU Lesser General Public License v3, see the accompanying
// file COPYING

#if defined(HAVE_CONFIG_H)
#include "config/build-config.h"
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
#include "util/thread.h"
#include "httpserver.h"
#include "httprpc.h"
#include "util/strencodings.h"
#include "net.h"
#include <unity/appmanager.h>

#include <boost/bind/bind.hpp>
using namespace boost::placeholders;
#include <boost/thread.hpp>
#include <boost/interprocess/sync/file_lock.hpp>


#include <stdio.h>

#define _(x) std::string(x)/* Keep the _() around in case gettext or such will be used later to translate non-UI */


/* Introduction text for doxygen: */

/*! \mainpage Developer documentation
 *
 * \section intro_sec Introduction
 *
 * This is the developer documentation of the reference client for an experimental new digital currency called Munt (https://www.munt.org),
 * which enables instant payments to anyone, anywhere in the world. Munt uses peer-to-peer technology to operate
 * with no central authority: managing transactions and issuing money are carried out collectively by the network.
 *
 * The software is a community-driven open source project, released under the GNU Lesser General Public License v3.
 *
 * \section Navigation
 * Use the buttons <code>Namespaces</code>, <code>Classes</code> or <code>Files</code> at the top of the page to start navigating the code.
 */

int exitStatus = EXIT_SUCCESS;
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

void handlePostInitMain()
{
}

static void handleAppInitResult(bool bResult)
{
    if (!bResult)
    {
        // InitError will have been called with detailed error, which ends up on console
        exitStatus = EXIT_FAILURE;
        AppLifecycleManager::gApp->shutdown();
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
static void AppInit(int argc, char* argv[])
{
    AppLifecycleManager appManager; 
    appManager.signalAppInitializeResult.connect(boost::bind(handleAppInitResult, _1));
    appManager.signalAboutToInitMain.connect(&handlePreInitMain);
    appManager.signalAppShutdownFinished.connect(&handleFinalShutdown);

    //fixme: (UNITY) - refactor this some more so that init is taken care of inside the app manager (like with qt app)

    //
    // Parameters
    //
    // If Qt is used, parameters/.conf are parsed in the qt clients main() implementation
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
            strUsage += "\n" + _("Usage:") + "\n" + "  " + DAEMON_NAME + " [options]                     " + strprintf(_("Start %s Daemon"), _(PACKAGE_NAME)) + "\n";
            strUsage += "\n" + HelpMessage(HMM_DAEMON);
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
            ReadConfigFile(GetArg("-conf", DEFAULT_CONF_FILENAME));
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
                fprintf(stderr, "Error: Command line contains unexpected token '%s', see " DAEMON_NAME " -h for a list of options.\n", argv[i]);
                exitStatus = EXIT_FAILURE;
                return;
            }
        }

        // -server defaults to true for daemon but not for the GUI so do this here
        SoftSetBoolArg("-server", true);
        // Set this early so that parameter interactions go to console
        InitLogging();
        InitParameterInteraction();

        //fixme: (UNITY) - This is now duplicated, factor this out into a common helper.
        // NB! This has to happen before we deamonise
        // Make sure only a single process is using the data directory.
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
                    fprintf(stderr, "ERROR: Cannot obtain a lock on data directory %s. %s is probably already running.", GetDataDir().string().c_str(), _(PACKAGE_NAME).c_str());
                    exitStatus = EXIT_FAILURE;
                    AppLifecycleManager::gApp->shutdown();
                    return;
                }
            }
            catch(const boost::interprocess::interprocess_exception& e)
            {
                fprintf(stderr, "ERROR: Cannot obtain a lock on data directory %s. %s is probably already running.", GetDataDir().string().c_str(), _(PACKAGE_NAME).c_str());
                exitStatus = EXIT_FAILURE;
                AppLifecycleManager::gApp->shutdown();
                return;
            }
        }

        if (GetBoolArg("-daemon", false))
        {
            fprintf(stdout, "%s server starting\n", _(PACKAGE_NAME).c_str());
            if (!AppLifecycleManager::gApp->daemonise())
            {
                LogPrintf("Failed to daemonise\n");
                exitStatus = EXIT_FAILURE;
                return;
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
}

int main(int argc, char* argv[])
{
    SetupEnvironment();

    // Connect daemon signal handlers
    noui_connect();

    AppInit(argc, argv);

    return exitStatus;
}

//fixme: (HIGH)
//Super gross workaround - for some reason our macos build keeps failing to provide '___cpu_model' symbol, so we just define it ourselves as a workaround until we can fix the issue.
#ifdef MAC_OSX
#include "llvm-cpumodel-hack.cpp"
#endif
