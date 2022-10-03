// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// File contains modifications by: The Centure developers
// All modifications:
// Copyright (c) 2016-2022 The Centure developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the Libre Chain License, see the accompanying
// file COPYING

#if defined(HAVE_CONFIG_H)
#include "config/build-config.h"
#endif

#include "libinit.h"
#include "chainparams.h"
#include "clientversion.h"
#include "compat.h"
#include "fs.h"
#include "init.h"
#include "scheduler.h"
#include "util.h"
#include "util/strencodings.h"
#include "net.h"
#include "util/thread.h"
#include "wallet/wallet.h"
#include <unity/appmanager.h>

#include <boost/thread.hpp>
#include <boost/interprocess/sync/file_lock.hpp>


#include <stdio.h>

#define _(x) std::string(x)/* Keep the _() around in case gettext or such will be used later to translate non-UI */


bool shutDownFinalised = false;

extern void terminateUnityFrontend();
static void handleFinalShutdown()
{
    terminateUnityFrontend();
    shutDownFinalised = true;
}

extern void handlePostInitMain();
static void handleAppInitResult(bool bResult)
{
    if (!bResult)
    {
        // InitError will have been called with detailed error, which ends up on console
        AppLifecycleManager::gApp->shutdown();
        return;
    }
    handlePostInitMain();
}

static bool handlePreInitMain()
{
    return true;
}



extern void handleInitWithExistingWallet();
extern void handleInitWithoutExistingWallet();
extern bool unityMessageBox(const std::string& message, const std::string& caption, unsigned int style);

static bool unityThreadSafeQuestion(const std::string& /* ignored interactive message */, const std::string& message, const std::string& caption, unsigned int style)
{
    //fixme: (UNITY) (HIGH)
    //Implement...
    return false;
}

//fixme: See if we can remove this, looks like it may no longer be used.
static void unityInitMessage(const std::string& message)
{
    LogPrintf("init message: %s\n", message);
}

#ifdef ENABLE_WALLET
extern void NotifyRequestUnlockS(CWallet* wallet, std::string reason);
extern void NotifyRequestUnlockWithCallbackS(CWallet* wallet, std::string reason, std::function<void (void)> successCallback);
#endif

void connectUIInterface()
{
    // Connect signal handlers
    uiInterface.ThreadSafeMessageBox.connect(unityMessageBox);
    uiInterface.ThreadSafeQuestion.connect(unityThreadSafeQuestion);
    uiInterface.InitMessage.connect(unityInitMessage);

    #ifdef ENABLE_WALLET
    uiInterface.RequestUnlock.connect(boost::bind(NotifyRequestUnlockS,  boost::placeholders::_1,  boost::placeholders::_2));
    uiInterface.RequestUnlockWithCallback.connect(boost::bind(NotifyRequestUnlockWithCallbackS,  boost::placeholders::_1,  boost::placeholders::_2,  boost::placeholders::_3));
    #endif
}

//////////////////////////////////////////////////////////////////////////////
//
// Start
//
int InitUnity()
{
    SetupEnvironment();

    // Connect signal handlers
    connectUIInterface();

    AppLifecycleManager appManager; 
    appManager.signalAppInitializeResult.connect(boost::bind(handleAppInitResult, boost::placeholders::_1));
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
            ReadConfigFile(GetArg("-conf", DEFAULT_CONF_FILENAME));
        }
        catch (const std::exception& e)
        {
            fprintf(stderr,"Error reading configuration file: %s\n", e.what());
            return EXIT_FAILURE;
        }

        InitAppSpecificConfigParamaters();

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

        std::string walletFile = GetArg("-wallet", DEFAULT_WALLET_DAT);
        if (!fs::exists(GetDataDir() / walletFile))
        {
            //Temporary - migrate old 'Gulden' wallets to new 'Munt' wallets.
            try
            {
                std::string newPathString = GetDataDir().string();
                std::string oldPathString = newPathString;

                boost::replace_all(oldPathString, "Munt", "Gulden");
                boost::replace_all(oldPathString, "munt", "Gulden");
                boost::filesystem::path oldPath(oldPathString);
                if (fs::exists( (oldPath / walletFile).string() ))
                {
                    for(auto& entry : boost::make_iterator_range(boost::filesystem::directory_iterator(oldPath), {}))
                    {
                        try { fs::rename((oldPath / entry.path().filename()).string(), (GetDataDir() / entry.path().filename()).string() ); } catch(...) {}
                    }
                    if (fs::exists(oldPath / "gulden.conf"))
                        fs::rename((oldPath / "gulden.conf").string(), (GetDataDir() / "munt.conf").string());
                    if (fs::exists(oldPath / "Gulden.conf"))
                        fs::rename((oldPath / "Gulden.conf").string(), (GetDataDir() / "munt.conf").string());
                }
                else
                {
                    std::string newPathString = GetDataDir().string();
                    std::string oldPathString = newPathString;
                    boost::replace_all(oldPathString, "Munt", "gulden");
                    boost::replace_all(oldPathString, "munt", "gulden");
                    oldPath = boost::filesystem::path(boost::filesystem::path(oldPathString));
                    if (fs::exists( (oldPath / walletFile).string() ))
                    {
                        for(auto& entry : boost::make_iterator_range(boost::filesystem::directory_iterator(oldPath), {}))
                        {
       	                    try { fs::rename((oldPath / entry.path().filename()).string(), (GetDataDir() / entry.path().filename()).string() ); } catch(...) {}
                        }
                        if (fs::exists(oldPath / "gulden.conf"))
	                    fs::rename((oldPath / "gulden.conf").string(), (GetDataDir() / "munt.conf").string());
                        if (fs::exists(oldPath / "Gulden.conf"))
                            fs::rename((oldPath / "Gulden.conf").string(), (GetDataDir() / "munt.conf").string());
                    }
                }
            }
            catch(...){}
        }

        // Set this early so that parameter interactions go to console
        InitLogging();
        InitParameterInteraction();

        //fixme: (UNITY) - This is now duplicated, factor this out into a common helper.
        // NB! This has to happen before we deamonise
        // Make sure only a single Munt process is using the data directory.
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
                    AppLifecycleManager::gApp->shutdown();
                    return EXIT_FAILURE;
                }
            }
            catch(const boost::interprocess::interprocess_exception& e)
            {
                fprintf(stderr, "ERROR: Cannot obtain a lock on data directory %s. %s is probably already running.", GetDataDir().string().c_str(), _(PACKAGE_NAME).c_str());
                AppLifecycleManager::gApp->shutdown();
                return EXIT_FAILURE;
            }
        }

        bool havePrimary = false;
        if (fs::exists(GetDataDir() / walletFile))
        {
            CWalletDBWrapper dbw(&bitdb, walletFile);
            CDB db(dbw);
            std::string primaryUUID;
            havePrimary = db.Read(std::string("primaryaccount"), primaryUUID);
            if (havePrimary)
                fprintf(stderr, "Existing wallet file has primary account %s", primaryUUID.c_str());
            else
                fprintf(stderr, "Wallet file exists but hase no primary account (erased)");
        }

        if (havePrimary)
        {
            handleInitWithExistingWallet();
        }
        else
        {
            handleInitWithoutExistingWallet();
        }
    }
    catch (const std::exception& e)
    {
        PrintExceptionContinue(&e, "AppInit()");
    }
    catch (...)
    {
        PrintExceptionContinue(NULL, "AppInit()");
    }

    AppLifecycleManager::gApp->waitForShutDown();

    return EXIT_SUCCESS;
}

//fixme: (HIGH)
//Super gross workaround - for some reason our macos build keeps failing to provide '___cpu_model' symbol, so we just define it ourselves as a workaround until we can fix the issue.
#ifdef MAC_OSX
#include "llvm-cpumodel-hack.cpp"
#endif
