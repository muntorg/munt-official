// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// File contains modifications by: The Gulden developers
// All modifications:
// Copyright (c) 2016 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#if defined(HAVE_CONFIG_H)
#include "config/bitcoin-config.h"
#endif

#include "chainparams.h"
#include "clientversion.h"
#include "rpc/server.h"
#include "init.h"
#include "noui.h"
#include "scheduler.h"
#include "util.h"
#include "httpserver.h"
#include "httprpc.h"
#include "utilstrencodings.h"

#include <boost/algorithm/string/predicate.hpp>
#include <boost/filesystem.hpp>
#include <boost/thread.hpp>
#include <boost/interprocess/sync/file_lock.hpp>

#include <Gulden/translate.h>
#include <stdio.h>

#define _(x) gulden_translate(x) /* Keep the _() around in case gettext or such will be used later to translate non-UI */

/* Introduction text for doxygen: */

/*! \mainpage Developer documentation
 *
 * \section intro_sec Introduction
 *
 * This is the developer documentation of the reference client for an experimental new digital currency called Bitcoin (https://www.gulden.com),
 * which enables instant payments to anyone, anywhere in the world. Bitcoin uses peer-to-peer technology to operate
 * with no central authority: managing transactions and issuing money are carried out collectively by the network.
 *
 * The software is a community-driven open source project, released under the MIT license.
 *
 * \section Navigation
 * Use the buttons <code>Namespaces</code>, <code>Classes</code> or <code>Files</code> at the top of the page to start navigating the code.
 */

static bool fDaemon;
bool fBitcoinD;

void WaitForShutdown(boost::thread_group* threadGroup)
{
    bool fShutdown = ShutdownRequested();

    while (!fShutdown) {
        MilliSleep(200);
        fShutdown = ShutdownRequested();
    }
    if (threadGroup) {
        Interrupt(*threadGroup);
        threadGroup->join_all();
    }
}

bool AppInit(int argc, char* argv[])
{
    boost::thread_group threadGroup;
    CScheduler scheduler;

    bool fRet = false;

    ParseParameters(argc, argv);

    if (mapArgs.count("-?") || mapArgs.count("-h") || mapArgs.count("-help") || mapArgs.count("-version")) {
        std::string strUsage = strprintf(_("%s Daemon"), _(PACKAGE_NAME)) + " " + _("version") + " " + FormatFullVersion() + "\n";

        if (mapArgs.count("-version")) {
            strUsage += FormatParagraph(LicenseInfo());
        } else {
            strUsage += "\n" + _("Usage:") + "\n" + "  GuldenD [options]                     " + strprintf(_("Start %s Daemon"), _(PACKAGE_NAME)) + "\n";

            strUsage += "\n" + HelpMessage(HMM_BITCOIND);
        }

        fprintf(stdout, "%s", strUsage.c_str());
        return false;
    }

    try {
        if (!boost::filesystem::is_directory(GetDataDir(false))) {
            fprintf(stderr, "Error: Specified data directory \"%s\" does not exist.\n", mapArgs["-datadir"].c_str());
            return false;
        }
        try {
            ReadConfigFile(mapArgs, mapMultiArgs);
        }
        catch (const std::exception& e) {
            fprintf(stderr, "Error reading configuration file: %s\n", e.what());
            return false;
        }

        try {
            SelectParams(ChainNameFromCommandLine());
        }
        catch (const std::exception& e) {
            fprintf(stderr, "Error: %s\n", e.what());
            return false;
        }

        bool fCommandLine = false;
        for (int i = 1; i < argc; i++)
            if (!IsSwitchChar(argv[i][0]) && !boost::algorithm::istarts_with(argv[i], "Gulden:"))
                fCommandLine = true;

        if (fCommandLine) {
            fprintf(stderr, "Error: There is no RPC client functionality in GuldenD anymore. Use the Gulden-cli utility instead.\n");
            exit(1);
        }
#ifndef WIN32
        fDaemon = GetBoolArg("-daemon", false);
        if (fDaemon) {
            fprintf(stdout, "Gulden server starting\n");

            pid_t pid = fork();
            if (pid < 0) {
                fprintf(stderr, "Error: fork() returned %d errno %d\n", pid, errno);
                return false;
            }
            if (pid > 0) // Parent process, pid is child process id
            {
                return true;
            }

            pid_t sid = setsid();
            if (sid < 0)
                fprintf(stderr, "Error: setsid() returned %d errno %d\n", sid, errno);
        }
#endif
        SoftSetBoolArg("-server", true);

        fNoUI = true;

        InitLogging();
        InitParameterInteraction();

        boost::filesystem::path pathLockFile = GetDataDir() / ".lock";
        FILE* file = fopen(pathLockFile.string().c_str(), "a"); // empty lock file; created if it doesn't exist.
        if (file)
            fclose(file);

        try {
            static boost::interprocess::file_lock lock(pathLockFile.string().c_str());
            if (!lock.try_lock()) {
                fprintf(stderr, _("Cannot obtain a lock on data directory %s. %s is probably already running.").c_str(), GetDataDir().string().c_str(), _(PACKAGE_NAME).c_str());
                return 1;
            }
        }
        catch (const boost::interprocess::interprocess_exception& e) {
            fprintf(stderr, _("Cannot obtain a lock on data directory %s. %s is probably already running.").c_str(), GetDataDir().string().c_str(), _(PACKAGE_NAME).c_str());
            return 1;
        }

        fRet = AppInit2(threadGroup, scheduler);
    }
    catch (const std::exception& e) {
        PrintExceptionContinue(&e, "AppInit()");
    }
    catch (...) {
        PrintExceptionContinue(NULL, "AppInit()");
    }

    if (!fRet) {
        Interrupt(threadGroup);

    } else {
        WaitForShutdown(&threadGroup);
    }
    Shutdown();

    return fRet;
}

int main(int argc, char* argv[])
{
    SetupEnvironment();

    noui_connect();

    return (AppInit(argc, argv) ? 0 : 1);
}
