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

#include "init.h"
#include <unity/appmanager.h>

#include "addrman.h"
#include "amount.h"
#include "blockstore.h"
#include "chain.h"
#include "chainparams.h"
#include "checkpoints.h"
#include <Gulden/auto_checkpoints.h>
#include "compat/sanity.h"
#include "consensus/validation.h"
#include "validation/validation.h"
#include "validation/witnessvalidation.h"
#include "validation/validationinterface.h"
#include "validation/versionbitsvalidation.h"
#include "fs.h"
#include "httpserver.h"
#include "httprpc.h"
#include "key.h"
#include "generation/miner.h"
#include "generation/witness.h"
#include "netbase.h"
#include "net.h"
#include "net_processing.h"
#include "policy/feerate.h"
#include "policy/fees.h"
#include "policy/policy.h"
#include "rpc/server.h"
#include "rpc/register.h"
#include "rpc/blockchain.h"
#include "script/standard.h"
#include "script/sigcache.h"
#include "scheduler.h"
#include "timedata.h"
#include "txdb.h"
#include "txmempool.h"
#include "torcontrol.h"
#include "ui_interface.h"
#include "util.h"
#include "utilmoneystr.h"
#ifdef ENABLE_WALLET
#include "wallet/wallet.h"
#include "wallet/walletdb.h"
#endif
#include "warnings.h"
#include <stdint.h>
#include <stdio.h>
#include <memory>

#ifndef WIN32
#include <signal.h>
#include <sys/resource.h>
#endif

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/bind.hpp>
#include <boost/interprocess/sync/file_lock.hpp>
#include <boost/thread.hpp>
#include <openssl/crypto.h>

#if ENABLE_ZMQ
#include "zmq/zmqnotificationinterface.h"
#endif

#include "util.h"

bool fFeeEstimatesInitialized = false;


std::unique_ptr<CConnman> g_connman;
std::unique_ptr<PeerLogicValidation> peerLogic;

#if ENABLE_ZMQ
static CZMQNotificationInterface* pzmqNotificationInterface = NULL;
#endif

#ifdef WIN32
// Win32 LevelDB doesn't use filedescriptors, and the ones used for
// accessing block files don't count towards the fd_set size limit
// anyway.
#define MIN_CORE_FILEDESCRIPTORS 0
#else
#define MIN_CORE_FILEDESCRIPTORS 150
#endif

/** Used to pass flags to the Bind() function */
enum BindFlags {
    BF_NONE         = 0,
    BF_EXPLICIT     = (1U << 0),
    BF_REPORT_ERROR = (1U << 1),
    BF_WHITELIST    = (1U << 2),
};

static const char* FEE_ESTIMATES_FILENAME="fee_estimates.dat";

//////////////////////////////////////////////////////////////////////////////
//
// Shutdown
//

//
// Thread management and startup/shutdown:
//
// The network-processing threads are all part of a thread group
// created by AppInit() or the Qt main() function.
//
// A clean exit happens when qt or the SIGTERM signal handler call GuldenAppManager::shutdown()
// This signals to the shutdown thread that shutdown can commence.
// The shutdown thread systematically shuts down the application:
// 1 - Alerts the UI to perform basic pre shutdown preparation (hide UI, show an exit message etc.)
// 2 - Interrupts the thread groups.
// 3 - Notifies the UI to disconnect from signals.
// 4 - Stops all the thread groups, syncs all files to disk etc.
// 5 - Notifies the App/UI to close themeselves (or in the case of GuldenD to simply exit).


std::atomic<bool> fDumpMempoolLater(false);

/**
 * This is a minimally invasive approach to shutdown on LevelDB read errors from the
 * chainstate, while keeping user interface out of the common library, which is shared
 * between GuldenD, and Gulden (qt) and non-server tools.
*/
class CCoinsViewErrorCatcher : public CCoinsViewBacked
{
public:
    CCoinsViewErrorCatcher(CCoinsView* view) : CCoinsViewBacked(view) {}
    bool GetCoin(const COutPoint &outpoint, Coin &coin) const override {
        try {
            return CCoinsViewBacked::GetCoin(outpoint, coin);
        } catch(const std::runtime_error& e) {
            uiInterface.ThreadSafeMessageBox(_("Error reading from database, shutting down."), "", CClientUIInterface::MSG_ERROR);
            LogPrintf("Error reading from database: %s\n", e.what());
            // Starting the shutdown sequence and returning false to the caller would be
            // interpreted as 'entry not found' (as opposed to unable to read data), and
            // could lead to invalid interpretation. Just exit immediately, as we can't
            // continue anyway, and all writes should be atomic.
            abort();
        }
    }
    // Writes do not need similar protection, as failure to write is handled by the caller.
};

static CCoinsViewErrorCatcher *pcoinscatcher = NULL;
static std::unique_ptr<ECCVerifyHandle> globalVerifyHandle;

static CCoinsViewErrorCatcher *ppow2witcatcher = NULL;

extern void ServerInterrupt(boost::thread_group& threadGroup);
void CoreInterrupt(boost::thread_group& threadGroup)
{
    LogPrintf("Core interrupt: commence core interrupt\n");
    PoWMineGulden(false, 0, Params());
    if (g_connman)
        g_connman->Interrupt();
    ServerInterrupt(threadGroup);
    threadGroup.interrupt_all();
    LogPrintf("Core interrupt: done.\n");
}

extern void ServerShutdown(boost::thread_group& threadGroup);
void CoreShutdown(boost::thread_group& threadGroup)
{
    LogPrintf("Core shutdown: commence core shutdown\n");
    static CCriticalSection cs_Shutdown;

    TRY_LOCK(cs_Shutdown, lockShutdown);
    if (!lockShutdown)
        return;

    /// Note: Shutdown() must be able to handle cases in which initialization failed part of the way,
    /// for example if the data directory was found to be locked.
    /// Be sure that anything that writes files or flushes caches only does this if the respective
    /// module was initialized.
    mempool.AddTransactionsUpdated(1);

    LogPrintf("Core shutdown: stop network threads.\n");
    if (g_connman)
        g_connman->Stop();
    MilliSleep(20); //Allow other threads (UI etc. a chance to cleanup as well)


    LogPrintf("Core shutdown: stop remaining worker threads.\n");
    ServerShutdown(threadGroup);
    threadGroup.join_all();
    MilliSleep(20); //Allow other threads (UI etc. a chance to cleanup as well)

    #ifdef ENABLE_WALLET
    LogPrintf("Core shutdown: final flush wallets.\n");
    for (CWalletRef pwallet : vpwallets) {
        pwallet->Flush(false);
    }
    MilliSleep(20); //Allow other threads (UI etc. a chance to cleanup as well)
    #endif

    LogPrintf("Core shutdown: delete network threads.\n");
    MapPort(false);
    UnregisterValidationInterface(peerLogic.get());
    peerLogic.reset();
    g_connman.reset();
    MilliSleep(20); //Allow other threads (UI etc. a chance to cleanup as well)

    UnregisterNodeSignals(GetNodeSignals());
    if (fDumpMempoolLater && GetArg("-persistmempool", DEFAULT_PERSIST_MEMPOOL)) {
        DumpMempool();
    }

    if (fFeeEstimatesInitialized)
    {
        ::feeEstimator.FlushUnconfirmed(::mempool);
        fs::path est_path = GetDataDir() / FEE_ESTIMATES_FILENAME;
        CAutoFile est_fileout(fsbridge::fopen(est_path, "wb"), SER_DISK, CLIENT_VERSION);
        if (!est_fileout.IsNull())
            ::feeEstimator.Write(est_fileout);
        else
            LogPrintf("%s: Failed to write fee estimates to %s\n", __func__, est_path.string());
        fFeeEstimatesInitialized = false;
    }


    LogPrintf("Core shutdown: close coin databases.\n");
    {
        LOCK(cs_main);
        if (pcoinsTip != NULL) {
            FlushStateToDisk();
        }

        PruneBlockIndexForPartialSync();

        blockStore.CloseBlockFiles();
        delete pcoinsTip;
        pcoinsTip = NULL;
        delete pcoinscatcher;
        pcoinscatcher = NULL;
        delete pcoinsdbview;
        pcoinsdbview = NULL;

        //Already flushed to disk by FlushStateToDisk, setting to nullptr should trigger deletion.
        ppow2witTip = nullptr;
        delete ppow2witcatcher;
        ppow2witcatcher = NULL;
        delete ppow2witdbview;
        ppow2witdbview = NULL;

        delete pblocktree;
        pblocktree = NULL;
    }
    MilliSleep(20); //Allow other threads (UI etc. a chance to cleanup as well)


    #ifdef ENABLE_WALLET
    LogPrintf("Core shutdown: final flush wallets.\n");
    for (CWalletRef pwallet : vpwallets) {
        pwallet->Flush(true);
    }
    MilliSleep(20); //Allow other threads (UI etc. a chance to cleanup as well)
    #endif

    #if ENABLE_ZMQ
    LogPrintf("Core shutdown: close zmq interfaces.\n");
    if (pzmqNotificationInterface)
    {
        UnregisterValidationInterface(pzmqNotificationInterface);
        delete pzmqNotificationInterface;
        pzmqNotificationInterface = NULL;
    }
    #endif

    LogPrintf("Core shutdown: unregister validation interfaces.\n");
    #ifndef WIN32
    try
    {
        fs::remove(GetPidFile());
    }
    catch (const fs::filesystem_error& e)
    {
        LogPrintf("%s: Unable to remove pidfile: %s\n", __func__, e.what());
    }
    #endif
    UnregisterAllValidationInterfaces();
    MilliSleep(20); //Allow other threads (UI etc. a chance to cleanup as well)

    #ifdef ENABLE_WALLET
    LogPrintf("Core shutdown: delete wallets.\n");
    for (CWalletRef pwallet : vpwallets)
    {
        delete pwallet;
    }
    vpwallets.clear();
    MilliSleep(20); //Allow other threads (UI etc. a chance to cleanup as well)
    #endif
    globalVerifyHandle.reset();
    ECC_Stop();
    LogPrintf("Core shutdown: done.\n");
    MilliSleep(20); //Allow other threads (UI etc. a chance to cleanup as well)
}

//Signal handlers should be written in a way that does not result in any unwanted side-effects
//e.g. errno alteration, signal mask alteration, signal disposition change, and other global process attribute changes.
//Use of non-reentrant functions, e.g., malloc or printf, inside signal handlers is also unsafe.
//In particular, the POSIX specification and the Linux man page signal(7) requires that all system functions directly or indirectly called from a signal function are async-signal safe and gives
//list of such async-signal safe system functions (practically the system calls), otherwise it is an undefined behavior. It is suggested to simply set some volatile sig_atomic_t variable in a signal handler, and to test it elsewhere.
static void HandleSIGTERM(int)
{
    // We call a sigterm safe 'shutdown' function that does nothing but write to a socket.
    // The shutdown thread then safely handles the rest from within the already existing shutdown thread.
    if (GuldenAppManager::gApp)
        GuldenAppManager::gApp->shutdown();
}

static void HandleSIGHUP(int)
{
    fReopenDebugLog = true;
}

#ifndef WIN32
static void registerSignalHandler(int signal, void(*handler)(int))
{
    struct sigaction sa;
    sa.sa_handler = handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(signal, &sa, NULL);
}
#endif

bool static Bind(CConnman& connman, const CService &addr, unsigned int flags) {
    if (!(flags & BF_EXPLICIT) && IsLimited(addr))
        return false;
    std::string strError;
    if (!connman.BindListenPort(addr, strError, (flags & BF_WHITELIST) != 0)) {
        if (flags & BF_REPORT_ERROR)
            return InitError(strError);
        return false;
    }
    return true;
}

//If we want to translate help messages in future we can replace helptr with _ and everything will just work.
#define helptr(x) std::string(x)
//If we want to translate error messages in future we can replace helptr with _ and everything will just work.
#define errortr(x) std::string(x)
//If we want to translate warning messages in future we can replace helptr with _ and everything will just work.
#define warningtr(x) std::string(x)

extern std::string HelpMessage(HelpMessageMode mode);

std::string LicenseInfo()
{
    const std::string URL_WEBSITE = "<https://Gulden.com>";

    //fixme: (2.1) Mention additional libraries, boost etc.
    //fixme: (2.1) Translate
    //fixme: (2.1) Add code to ensure translations never strip copyrights
    return helptr("Copyright (C) 2014-2018 The Gulden developers")+ "\n"
           + helptr("Licensed under the Gulden license")+ "\n"
           + "\n"
           + helptr("This is experimental software.")+ "\n"
           + strprintf(helptr("Please contribute if you find %s useful. Visit %s for further information about the software."), PACKAGE_NAME, URL_WEBSITE)
           + "\n"
           + "\n"
           + strprintf(helptr("This product is originally based on a fork of the Bitcoin project. Copyright (C) 2014-2018 The Bitcoin Core Developers.")) + "\n"
           + strprintf(helptr("This product includes software developed by the OpenSSL Project for use in the OpenSSL Toolkit %s and cryptographic software written by Eric Young and UPnP software written by Thomas Bernard."), "<https://www.openssl.org>")+ "\n"
           + strprintf(helptr("This product uses a licensed copy of Font Awesome Pro"))+ "\n"
           + strprintf(helptr("This product includes and uses the Lato font which is licensed under the SIL Open Font License"))+ "\n"
           + strprintf(helptr("This product makes use of the Qt toolkit which is dynamically linked and licensed under the LGPL"))+ "\n";
}

static void BlockNotifyCallback(bool initialSync, const CBlockIndex *pBlockIndex)
{
    if (initialSync || !pBlockIndex)
        return;

    std::string strCmd = GetArg("-blocknotify", "");

    boost::replace_all(strCmd, "%s", pBlockIndex->GetBlockHashPoW2().GetHex());
    boost::thread t(runCommand, strCmd); // thread runs free
}

static bool fHaveGenesis = false;
static boost::mutex cs_GenesisWait;
static CConditionVariable condvar_GenesisWait;

static void BlockNotifyGenesisWait(bool, const CBlockIndex *pBlockIndex)
{
    if (pBlockIndex != NULL) {
        {
            boost::unique_lock<boost::mutex> lock_GenesisWait(cs_GenesisWait);
            fHaveGenesis = true;
        }
        condvar_GenesisWait.notify_all();
    }
}

struct CImportingNow
{
    CImportingNow() {
        assert(!fImporting);
        fImporting = true;
    }

    ~CImportingNow() {
        assert(fImporting);
        fImporting = false;
    }
};


// If we're using -prune with -reindex, then delete block files that will be ignored by the
// reindex.  Since reindexing works by starting at block file 0 and looping until a blockfile
// is missing, do the same here to delete any later block files after a gap.  Also delete all
// rev files since they'll be rewritten by the reindex anyway.  This ensures that vinfoBlockFile
// is in sync with what's actually on disk by the time we start downloading, so that pruning
// works correctly.
static void CleanupBlockRevFiles()
{
    std::map<std::string, fs::path> mapBlockFiles;

    // Glob all blk?????.dat and rev?????.dat files from the blocks directory.
    // Remove the rev files immediately and insert the blk file paths into an
    // ordered map keyed by block file index.
    LogPrintf("Removing unusable blk?????.dat and rev?????.dat files for -reindex with -prune\n");
    fs::path blocksdir = GetDataDir() / "blocks";
    for (fs::directory_iterator it(blocksdir); it != fs::directory_iterator(); it++) {
        if (is_regular_file(*it) &&
            it->path().filename().string().length() == 12 &&
            it->path().filename().string().substr(8,4) == ".dat")
        {
            if (it->path().filename().string().substr(0,3) == "blk")
                mapBlockFiles[it->path().filename().string().substr(3,5)] = it->path();
            else if (it->path().filename().string().substr(0,3) == "rev")
                remove(it->path());
        }
    }

    // Remove all block files that aren't part of a contiguous set starting at
    // zero by walking the ordered map (keys are block file indices) by
    // keeping a separate counter.  Once we hit a gap (or if 0 doesn't exist)
    // start removing block files.
    int nContigCounter = 0;
    for(const PAIRTYPE(std::string, fs::path)& item : mapBlockFiles) {
        if (atoi(item.first) == nContigCounter) {
            nContigCounter++;
            continue;
        }
        remove(item.second);
    }
}

static void ThreadImport(std::vector<fs::path> vImportFiles)
{
    const CChainParams& chainparams = Params();
    RenameThread("Gulden-loadblk");

    {
    CImportingNow imp;

    // -reindex
    if (fReindex) {
        int nFile = 0;
        while (true) {
            FILE* file;
            CDiskBlockPos pos(nFile, 0);
            {
                LOCK(cs_main);
                if (!blockStore.BlockFileExists(pos))
                    break; // No block files left to reindex
                FILE *tmpfile = blockStore.GetBlockFile(pos, true);
                if (!tmpfile)
                    break; // This error is logged in OpenBlockFile
                // dupping here because otherwise the cs_main might be locked for a long time
                file = fdopen(dup(fileno(tmpfile)), "rb+");
            }
            LogPrintf("Reindexing block file blk%05u.dat...\n", (unsigned int)nFile);
            LoadExternalBlockFile(chainparams, file, &pos);
            fclose(file);
            nFile++;
        }
        pblocktree->WriteReindexing(false);
        fReindex = false;
        LogPrintf("Reindexing finished\n");
        // To avoid ending up in a situation without genesis block, re-try initializing (no-op if reindexing worked):
        InitBlockIndex(chainparams);
    }

    // hardcoded $DATADIR/bootstrap.dat
    fs::path pathBootstrap = GetDataDir() / "bootstrap.dat";
    if (fs::exists(pathBootstrap)) {
        FILE *file = fsbridge::fopen(pathBootstrap, "rb");
        if (file) {
            fs::path pathBootstrapOld = GetDataDir() / "bootstrap.dat.old";
            LogPrintf("Importing bootstrap.dat...\n");
            LoadExternalBlockFile(chainparams, file);
            fclose(file);
            RenameOver(pathBootstrap, pathBootstrapOld);
        } else {
            LogPrintf("Warning: Could not open bootstrap file %s\n", pathBootstrap.string());
        }
    }

    // -loadblock=
    for(const fs::path& path : vImportFiles) {
        FILE *file = fsbridge::fopen(path, "rb");
        if (file) {
            LogPrintf("Importing blocks file %s...\n", path.string());
            LoadExternalBlockFile(chainparams, file);
            fclose(file);
        } else {
            LogPrintf("Warning: Could not open blocks file %s\n", path.string());
        }
    }

    // scan for better chains in the block chain database, that are not yet connected in the active best chain
    CValidationState state;
    if (!ActivateBestChain(state, chainparams)) {
        LogPrintf("Failed to connect best block\n");
        GuldenAppManager::gApp->shutdown();
    }

    if (GetBoolArg("-stopafterblockimport", DEFAULT_STOPAFTERBLOCKIMPORT)) {
        LogPrintf("Stopping after block import\n");
        GuldenAppManager::gApp->shutdown();
    }
    } // End scope of CImportingNow
    if (GetArg("-persistmempool", DEFAULT_PERSIST_MEMPOOL)) {
        LoadMempool();
        fDumpMempoolLater = !ShutdownRequested();
    }
}

/** Sanity checks
 *  Ensure that Gulden is running in a usable environment with all
 *  necessary library support.
 */
static bool InitSanityCheck(void)
{
    if(!ECC_InitSanityCheck()) {
        InitError("Elliptic curve cryptography sanity check failure. Aborting.");
        return false;
    }

    if (!glibc_sanity_test() || !glibcxx_sanity_test())
        return false;

    if (!Random_SanityCheck()) {
        InitError("OS cryptographic RNG sanity check failure. Aborting.");
        return false;
    }

    return true;
}

// Parameter interaction based on rules
void InitParameterInteraction()
{
    // when specifying an explicit binding address, you want to listen on it
    // even when -connect or -proxy is specified
    if (IsArgSet("-bind")) {
        if (SoftSetBoolArg("-listen", true))
            LogPrintf("%s: parameter interaction: -bind set -> setting -listen=1\n", __func__);
    }
    if (IsArgSet("-whitebind")) {
        if (SoftSetBoolArg("-listen", true))
            LogPrintf("%s: parameter interaction: -whitebind set -> setting -listen=1\n", __func__);
    }

    if (gArgs.IsArgSet("-connect")) {
        // when only connecting to trusted nodes, do not seed via DNS, or listen by default
        if (SoftSetBoolArg("-dnsseed", false))
            LogPrintf("%s: parameter interaction: -connect set -> setting -dnsseed=0\n", __func__);
        if (SoftSetBoolArg("-listen", false))
            LogPrintf("%s: parameter interaction: -connect set -> setting -listen=0\n", __func__);
    }

    if (IsArgSet("-proxy")) {
        // to protect privacy, do not listen by default if a default proxy server is specified
        if (SoftSetBoolArg("-listen", false))
            LogPrintf("%s: parameter interaction: -proxy set -> setting -listen=0\n", __func__);
        // to protect privacy, do not use UPNP when a proxy is set. The user may still specify -listen=1
        // to listen locally, so don't rely on this happening through -listen below.
        if (SoftSetBoolArg("-upnp", false))
            LogPrintf("%s: parameter interaction: -proxy set -> setting -upnp=0\n", __func__);
        // to protect privacy, do not discover addresses by default
        if (SoftSetBoolArg("-discover", false))
            LogPrintf("%s: parameter interaction: -proxy set -> setting -discover=0\n", __func__);
    }

    if (!GetBoolArg("-listen", DEFAULT_LISTEN)) {
        // do not map ports or try to retrieve public IP when not listening (pointless)
        if (SoftSetBoolArg("-upnp", false))
            LogPrintf("%s: parameter interaction: -listen=0 -> setting -upnp=0\n", __func__);
        if (SoftSetBoolArg("-discover", false))
            LogPrintf("%s: parameter interaction: -listen=0 -> setting -discover=0\n", __func__);
        if (SoftSetBoolArg("-listenonion", false))
            LogPrintf("%s: parameter interaction: -listen=0 -> setting -listenonion=0\n", __func__);
    }

    if (IsArgSet("-externalip")) {
        // if an explicit public IP is specified, do not try to find others
        if (SoftSetBoolArg("-discover", false))
            LogPrintf("%s: parameter interaction: -externalip set -> setting -discover=0\n", __func__);
    }

    // disable whitelistrelay in blocksonly mode
    if (GetBoolArg("-blocksonly", DEFAULT_BLOCKSONLY)) {
        if (SoftSetBoolArg("-whitelistrelay", false))
            LogPrintf("%s: parameter interaction: -blocksonly=1 -> setting -whitelistrelay=0\n", __func__);
    }

    // Forcing relay from whitelisted hosts implies we will accept relays from them in the first place.
    if (GetBoolArg("-whitelistforcerelay", DEFAULT_WHITELISTFORCERELAY)) {
        if (SoftSetBoolArg("-whitelistrelay", true))
            LogPrintf("%s: parameter interaction: -whitelistforcerelay=1 -> setting -whitelistrelay=1\n", __func__);
    }

    //For raspberry pis etc. we default to keeping logging at a minimum
    #if defined(__arm__) || defined(__aarch64__)
    SoftSetBoolArg("-minimallogging", true);
    #endif
    gbMinimalLogging = GetBoolArg("-minimallogging", false);
}

static std::string ResolveErrMsg(const char * const optname, const std::string& strBind)
{
    return strprintf(errortr("Cannot resolve -%s address: '%s'"), optname, strBind);
}

void InitLogging()
{
    fPrintToConsole = GetBoolArg("-printtoconsole", false);
    fLogTimestamps = GetBoolArg("-logtimestamps", DEFAULT_LOGTIMESTAMPS);
    fLogTimeMicros = GetBoolArg("-logtimemicros", DEFAULT_LOGTIMEMICROS);
    fLogIPs = GetBoolArg("-logips", DEFAULT_LOGIPS);

    LogPrintf("\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n");
    LogPrintf("Gulden version %s\n", FormatFullVersion());
}

namespace { // Variables internal to initialization process only

ServiceFlags nRelevantServices = NODE_NETWORK;
int nMaxConnections;
int nUserMaxConnections;
int nFD;
ServiceFlags nLocalServices = NODE_NETWORK;

}

[[noreturn]] static void new_handler_terminate()
{
    // Rather than throwing std::bad-alloc if allocation fails, terminate
    // immediately to (try to) avoid chain corruption.
    // Since LogPrintf may itself allocate memory, set the handler directly
    // to terminate first.
    std::set_new_handler(std::terminate);
    LogPrintf("Error: Out of memory. Terminating.\n");

    // The log was successful, terminate now.
    std::terminate();
};

extern void InitRegisterRPC();
bool AppInitBasicSetup()
{
    // ********************************************************* Step 1: setup
#ifdef _MSC_VER
    // Turn off Microsoft heap dump noise
    _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_WARN, CreateFileA("NUL", GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, 0));
#endif
#if _MSC_VER >= 1400
    // Disable confusing "helpful" text message on abort, Ctrl-C
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
#ifdef WIN32
    // Enable Data Execution Prevention (DEP)
    // Minimum supported OS versions: WinXP SP3, WinVista >= SP1, Win Server 2008
    // A failure is non-critical and needs no further attention!
#ifndef PROCESS_DEP_ENABLE
    // We define this here, because GCCs winbase.h limits this to _WIN32_WINNT >= 0x0601 (Windows 7),
    // which is not correct. Can be removed, when GCCs winbase.h is fixed!
#define PROCESS_DEP_ENABLE 0x00000001
#endif
    typedef BOOL (WINAPI *PSETPROCDEPPOL)(DWORD);
    PSETPROCDEPPOL setProcDEPPol = (PSETPROCDEPPOL)GetProcAddress(GetModuleHandleA("Kernel32.dll"), "SetProcessDEPPolicy");
    if (setProcDEPPol != NULL) setProcDEPPol(PROCESS_DEP_ENABLE);
#endif

    if (!SetupNetworking())
        return InitError("Initializing networking failed");

#ifndef WIN32
    if (!GetBoolArg("-sysperms", false)) {
        umask(077);
    }

    // Clean shutdown on SIGTERM
    registerSignalHandler(SIGTERM, HandleSIGTERM);
    registerSignalHandler(SIGINT, HandleSIGTERM);

    // Reopen debug.log on SIGHUP
    registerSignalHandler(SIGHUP, HandleSIGHUP);

    // Ignore SIGPIPE, otherwise it will bring the daemon down if the client closes unexpectedly
    signal(SIGPIPE, SIG_IGN);
#endif

    std::set_new_handler(new_handler_terminate);

    return true;
}

bool AppInitParameterInteraction()
{
    const CChainParams& chainparams = Params();
    // ********************************************************* Step 2: parameter interactions

    // also see: InitParameterInteraction()

    // if using block pruning, then disallow txindex
    if (GetArg("-prune", 0)) {
        if (GetBoolArg("-txindex", DEFAULT_TXINDEX))
            return InitError(errortr("Prune mode is incompatible with -txindex."));
    }

    // Make sure enough file descriptors are available
    int nBind = std::max(
                (gArgs.IsArgSet("-bind") ? gArgs.GetArgs("-bind").size() : 0) +
                (gArgs.IsArgSet("-whitebind") ? gArgs.GetArgs("-whitebind").size() : 0), size_t(1));
    nUserMaxConnections = GetArg("-maxconnections", DEFAULT_MAX_PEER_CONNECTIONS);
    nMaxConnections = std::max(nUserMaxConnections, 0);

    // Trim requested connection counts, to fit into system limitations
    int nSystemMaxConnections = FD_SETSIZE;
#ifndef WIN32
    rlimit rlim;
    int err = getrlimit(RLIMIT_NOFILE, &rlim);
    if (err) {
        LogPrintf("Could not determine max system file descriptors, assuming %d", nSystemMaxConnections);
    }
    else {
        nSystemMaxConnections = rlim.rlim_cur;
    }
#endif

    nMaxConnections = std::max(std::min(nMaxConnections, (int)(nSystemMaxConnections - nBind - MIN_CORE_FILEDESCRIPTORS - MAX_ADDNODE_CONNECTIONS)), 0);
    nFD = RaiseFileDescriptorLimit(nMaxConnections + MIN_CORE_FILEDESCRIPTORS + MAX_ADDNODE_CONNECTIONS);
    if (nFD < MIN_CORE_FILEDESCRIPTORS)
        return InitError(errortr("Not enough file descriptors available."));
    nMaxConnections = std::min(nFD - MIN_CORE_FILEDESCRIPTORS - MAX_ADDNODE_CONNECTIONS, nMaxConnections);

    if (nMaxConnections < nUserMaxConnections)
        InitWarning(strprintf(warningtr("Reducing -maxconnections from %d to %d, because of system limitations."), nUserMaxConnections, nMaxConnections));

    // ********************************************************* Step 3: parameter-to-internal-flags
    if (gArgs.IsArgSet("-debug")) {
        // Special-case: if -debug=0/-nodebug is set, turn off debugging messages
        const std::vector<std::string> categories = gArgs.GetArgs("-debug");

        if (find(categories.begin(), categories.end(), std::string("0")) == categories.end()) {
            for (const auto& cat : categories) {
                uint32_t flag = 0;
                if (!GetLogCategory(&flag, &cat)) {
                    InitWarning(strprintf(warningtr("Unsupported logging category %s=%s."), "-debug", cat));
                    continue;
                }
                logCategories |= flag;
            }
        }
    }

    // Now remove the logging categories which were explicitly excluded
    if (gArgs.IsArgSet("-debugexclude")) {
        for (const std::string& cat : gArgs.GetArgs("-debugexclude")) {
            uint32_t flag = 0;
            if (!GetLogCategory(&flag, &cat)) {
                InitWarning(strprintf(warningtr("Unsupported logging category %s=%s."), "-debugexclude", cat));
                continue;
            }
            logCategories &= ~flag;
        }
    }

    // Check for -debugnet
    if (GetBoolArg("-debugnet", false))
        InitWarning(warningtr("Unsupported argument -debugnet ignored, use -debug=net."));
    // Check for -socks - as this is a privacy risk to continue, exit here
    if (IsArgSet("-socks"))
        return InitError(errortr("Unsupported argument -socks found. Setting SOCKS version isn't possible anymore, only SOCKS5 proxies are supported."));
    // Check for -tor - as this is a privacy risk to continue, exit here
    if (GetBoolArg("-tor", false))
        return InitError(errortr("Unsupported argument -tor found, use -onion."));

    if (GetBoolArg("-benchmark", false))
        InitWarning(warningtr("Unsupported argument -benchmark ignored, use -debug=bench."));

    if (GetBoolArg("-whitelistalwaysrelay", false))
        InitWarning(warningtr("Unsupported argument -whitelistalwaysrelay ignored, use -whitelistrelay and/or -whitelistforcerelay."));

    if (IsArgSet("-blockminsize"))
        InitWarning("Unsupported argument -blockminsize ignored.");

    // Checkmempool and checkblockindex default to true in regtest mode
    int ratio = std::min<int>(std::max<int>(GetArg("-checkmempool", chainparams.DefaultConsistencyChecks() ? 1 : 0), 0), 1000000);
    if (ratio != 0) {
        mempool.setSanityCheck(1.0 / ratio);
    }
    fCheckBlockIndex = GetBoolArg("-checkblockindex", chainparams.DefaultConsistencyChecks());
    fCheckpointsEnabled = GetBoolArg("-checkpoints", DEFAULT_CHECKPOINTS_ENABLED);
    SetFullSyncMode(GetBoolArg("-fullsync", DEFAULT_FULL_SYNC_MODE));

    hashAssumeValid = uint256S(GetArg("-assumevalid", chainparams.GetConsensus().defaultAssumeValid.GetHex()));
    if (!hashAssumeValid.IsNull())
        LogPrintf("Assuming ancestors of block %s have valid signatures.\n", hashAssumeValid.GetHex());
    else
        LogPrintf("Validating signatures for all blocks.\n");

    // mempool limits
    int64_t nMempoolSizeMax = GetArg("-maxmempool", DEFAULT_MAX_MEMPOOL_SIZE) * 1000000;
    int64_t nMempoolSizeMin = GetArg("-limitdescendantsize", DEFAULT_DESCENDANT_SIZE_LIMIT) * 1000 * 40;
    if (nMempoolSizeMax < 0 || nMempoolSizeMax < nMempoolSizeMin)
        return InitError(strprintf(errortr("-maxmempool must be at least %d MB"), std::ceil(nMempoolSizeMin / 1000000.0)));
    // incremental relay fee sets the minimum feerate increase necessary for BIP 125 replacement in the mempool
    // and the amount the mempool min fee increases above the feerate of txs evicted due to mempool limiting.
    if (IsArgSet("-incrementalrelayfee"))
    {
        CAmount n = 0;
        if (!ParseMoney(GetArg("-incrementalrelayfee", ""), n))
            return InitError(AmountErrMsg("incrementalrelayfee", GetArg("-incrementalrelayfee", "")));
        incrementalRelayFee = CFeeRate(n);
    }

    // -par=0 means autodetect, but nScriptCheckThreads==0 means no concurrency
    nScriptCheckThreads = GetArg("-par", DEFAULT_SCRIPTCHECK_THREADS);
    if (nScriptCheckThreads <= 0)
        nScriptCheckThreads += GetNumCores();
    if (nScriptCheckThreads <= 1)
        nScriptCheckThreads = 0;
    else if (nScriptCheckThreads > MAX_SCRIPTCHECK_THREADS)
        nScriptCheckThreads = MAX_SCRIPTCHECK_THREADS;

    // block pruning; get the amount of disk space (in MiB) to allot for block & undo files
    int64_t nPruneArg = GetArg("-prune", 0);
    if (nPruneArg < 0) {
        return InitError(errortr("Prune cannot be configured with a negative value."));
    }
    nPruneTarget = (uint64_t) nPruneArg * 1024 * 1024;
    if (nPruneArg == 1) {  // manual pruning: -prune=1
        LogPrintf("Block pruning enabled.  Use RPC call pruneblockchain(height) to manually prune block and undo files.\n");
        nPruneTarget = std::numeric_limits<uint64_t>::max();
        fPruneMode = true;
    } else if (nPruneTarget) {
        if (nPruneTarget < MIN_DISK_SPACE_FOR_BLOCK_FILES) {
            return InitError(strprintf(errortr("Prune configured below the minimum of %d MiB.  Please use a higher number."), MIN_DISK_SPACE_FOR_BLOCK_FILES / 1024 / 1024));
        }
        LogPrintf("Prune configured to target %uMiB on disk for block and undo files.\n", nPruneTarget / 1024 / 1024);
        fPruneMode = true;
    }

    InitRegisterRPC();

    nConnectTimeout = GetArg("-timeout", DEFAULT_CONNECT_TIMEOUT);
    if (nConnectTimeout <= 0)
        nConnectTimeout = DEFAULT_CONNECT_TIMEOUT;

    // Fee-per-kilobyte amount required for mempool acceptance and relay
    // If you are mining, be careful setting this:
    // if you set it to zero then
    // a transaction spammer can cheaply fill blocks using
    // 0-fee transactions. It should be set above the real
    // cost to you of processing a transaction.
    if (IsArgSet("-minrelaytxfee"))
    {
        CAmount n = 0;
        if (!ParseMoney(GetArg("-minrelaytxfee", ""), n)) {
            return InitError(AmountErrMsg("minrelaytxfee", GetArg("-minrelaytxfee", "")));
        }
        // High fee check is done afterward in CWallet::ParameterInteraction()
        ::minRelayTxFee = CFeeRate(n);
    } else if (incrementalRelayFee > ::minRelayTxFee) {
        // Allow only setting incrementalRelayFee to control both
        ::minRelayTxFee = incrementalRelayFee;
        LogPrintf("Increasing minrelaytxfee to %s to match incrementalrelayfee\n",::minRelayTxFee.ToString());
    }

    // Sanity check argument for min fee for including tx in block
    // TODO: Harmonize which arguments need sanity checking and where that happens
    if (IsArgSet("-blockmintxfee"))
    {
        CAmount n = 0;
        if (!ParseMoney(GetArg("-blockmintxfee", ""), n))
            return InitError(AmountErrMsg("blockmintxfee", GetArg("-blockmintxfee", "")));
    }

    // Feerate used to define dust.  Shouldn't be changed lightly as old
    // implementations may inadvertently create non-standard transactions
    if (IsArgSet("-dustrelayfee"))
    {
        CAmount n = 0;
        if (!ParseMoney(GetArg("-dustrelayfee", ""), n) || 0 == n)
            return InitError(AmountErrMsg("dustrelayfee", GetArg("-dustrelayfee", "")));
        dustRelayFee = CFeeRate(n);
    }

    fRequireStandard = !GetBoolArg("-acceptnonstdtxn", !chainparams.RequireStandard());
    if (chainparams.RequireStandard() && !fRequireStandard)
        return InitError(strprintf("acceptnonstdtxn is not currently supported for %s chain", chainparams.NetworkIDString()));
    nBytesPerSigOp = GetArg("-bytespersigop", nBytesPerSigOp);

#ifdef ENABLE_WALLET
    if (!CWallet::ParameterInteraction())
        return false;
#endif
    //Gulden - generate private/public key pair for alert or checkpoint system
    if (IsArgSet("-genkeypair"))
    {
        ECC_Start();
        CKey key;
        key.MakeNewKey(false);

        CPrivKey vchPrivKey = key.GetPrivKey();
        printf("PrivateKey %s\n", HexStr<CPrivKey::iterator>(vchPrivKey.begin(), vchPrivKey.end()).c_str());
        CPubKey vchPubKey = key.GetPubKey();
        vchPubKey.Decompress();
        printf("PublicKey %s\n", HexStr(vchPubKey.begin(), vchPubKey.end()).c_str());
        exit(EXIT_SUCCESS);
    }


    fIsBareMultisigStd = GetBoolArg("-permitbaremultisig", DEFAULT_PERMIT_BAREMULTISIG);
    fAcceptDatacarrier = GetBoolArg("-datacarrier", DEFAULT_ACCEPT_DATACARRIER);
    nMaxDatacarrierBytes = GetArg("-datacarriersize", nMaxDatacarrierBytes);

    fAlerts = GetBoolArg("-alerts", DEFAULT_ALERTS);

    // Option to startup with mocktime set (used for regression testing):
    SetMockTime(GetArg("-mocktime", 0)); // SetMockTime(0) is a no-op

    if (GetBoolArg("-peerbloomfilters", DEFAULT_PEERBLOOMFILTERS))
        nLocalServices = ServiceFlags(nLocalServices | NODE_BLOOM);

    nMaxTipAge = GetArg("-maxtipage", DEFAULT_MAX_TIP_AGE);

    fEnableReplacement = GetBoolArg("-mempoolreplacement", DEFAULT_ENABLE_REPLACEMENT);
    if ((!fEnableReplacement) && IsArgSet("-mempoolreplacement")) {
        // Minimal effort at forwards compatibility
        std::string strReplacementModeList = GetArg("-mempoolreplacement", "");  // default is impossible
        std::vector<std::string> vstrReplacementModes;
        boost::split(vstrReplacementModes, strReplacementModeList, boost::is_any_of(","));
        fEnableReplacement = (std::find(vstrReplacementModes.begin(), vstrReplacementModes.end(), "fee") != vstrReplacementModes.end());
    }

    if (gArgs.IsArgSet("-vbparams")) {
        // Allow overriding version bits parameters for testing
        if (!chainparams.MineBlocksOnDemand()) {
            return InitError("Version bits parameters may only be overridden on regtest.");
        }
        for (const std::string& strDeployment : gArgs.GetArgs("-vbparams")) {
            std::vector<std::string> vDeploymentParams;
            boost::split(vDeploymentParams, strDeployment, boost::is_any_of(":"));
            if (vDeploymentParams.size() != 3) {
                return InitError("Version bits parameters malformed, expecting deployment:start:end");
            }
            int64_t nStartTime, nTimeout;
            if (!ParseInt64(vDeploymentParams[1], &nStartTime)) {
                return InitError(strprintf("Invalid nStartTime (%s)", vDeploymentParams[1]));
            }
            if (!ParseInt64(vDeploymentParams[2], &nTimeout)) {
                return InitError(strprintf("Invalid nTimeout (%s)", vDeploymentParams[2]));
            }
            bool found = false;
            for (int j=0; j<(int)Consensus::MAX_VERSION_BITS_DEPLOYMENTS; ++j)
            {
                if (vDeploymentParams[0].compare(VersionBitsDeploymentInfo[j].name) == 0) {
                    UpdateVersionBitsParameters(Consensus::DeploymentPos(j), nStartTime, nTimeout);
                    found = true;
                    LogPrintf("Setting version bits activation parameters for %s to start=%ld, timeout=%ld\n", vDeploymentParams[0], nStartTime, nTimeout);
                    break;
                }
            }
            if (!found) {
                return InitError(strprintf("Invalid deployment (%s)", vDeploymentParams[0]));
            }
        }
    }
    return true;
}

static bool LockDataDirectory(bool probeOnly)
{
    std::string strDataDir = GetDataDir().string();

    // Make sure only a single Gulden process is using the data directory.
    //fixme: (2.1)
    (unused) probeOnly;
    /* (GULDEN) - we do this elsewhere (MERGE) look into this again.
    FILE* file = fsbridge::fopen(pathLockFile, "a"); // empty lock file; created if it doesn't exist.
    if (file) fclose(file);

    try {
        static boost::interprocess::file_lock lock(pathLockFile.string().c_str());
        if (!lock.try_lock()) {
            return InitError(strprintf(errortr("Cannot obtain a lock on data directory %s. %s is probably already running."), strDataDir, errortr(PACKAGE_NAME)));
        }
        if (probeOnly) {
            lock.unlock();
        }
    } catch(const boost::interprocess::interprocess_exception& e) {
        return InitError(strprintf(errortr("Cannot obtain a lock on data directory %s. %s is probably already running.") + " %s.", strDataDir, errortr(PACKAGE_NAME), e.what()));
    }*/
    return true;
}

bool AppInitSanityChecks()
{
    // ********************************************************* Step 4: sanity checks

    // Initialize elliptic curve code
    ECC_Start();
    globalVerifyHandle.reset(new ECCVerifyHandle());

    // Sanity check
    if (!InitSanityCheck())
        return InitError(strprintf(errortr("Initialization sanity check failed. %s is shutting down."), _(PACKAGE_NAME)));

    // Probe the data directory lock to give an early error message, if possible
    return LockDataDirectory(true);
}

extern bool InitRPCWarmup(boost::thread_group& threadGroup);
extern bool InitTor(boost::thread_group& threadGroup, CScheduler& scheduler);

bool AppInitMain(boost::thread_group& threadGroup, CScheduler& scheduler)
{
    const CChainParams& chainparams = Params();
    // ********************************************************* Step 4a: application initialization
    // After daemonization get the data directory lock again and hold on to it until exit
    // This creates a slight window for a race condition to happen, however this condition is harmless: it
    // will at most make us exit without printing a message to console.
    if (!LockDataDirectory(false)) {
        // Detailed error printed inside LockDataDirectory
        return false;
    }

#ifndef WIN32
    CreatePidFile(GetPidFile(), getpid());
#endif
    if (GetBoolArg("-shrinkdebugfile", logCategories == BCLog::NONE)) {
        // Do this first since it both loads a bunch of debug.log into memory,
        // and because this needs to happen before any other debug.log printing
        ShrinkDebugFile();
    }

    if (fPrintToDebugLog)
        OpenDebugLog();

    if (!fLogTimestamps)
        LogPrintf("Startup time: %s\n", DateTimeStrFormat("%Y-%m-%d %H:%M:%S", GetTime()));
    LogPrintf("Default data directory %s\n", GetDefaultDataDir().string());
    LogPrintf("Using data directory %s\n", GetDataDir().string());
    LogPrintf("Using config file %s\n", GetConfigFile(GetArg("-conf", GULDEN_CONF_FILENAME)).string());
    LogPrintf("Using at most %i automatic connections (%i file descriptors available)\n", nMaxConnections, nFD);

    InitSignatureCache();

    LogPrintf("Using %u threads for script verification\n", nScriptCheckThreads);
    if (nScriptCheckThreads) {
        for (int i=0; i<nScriptCheckThreads-1; i++)
            threadGroup.create_thread(&ThreadScriptCheck);
    }

    //Gulden - private key for checkpoint system.
    if (IsArgSet("-checkpointkey"))
    {
        std::string sKey=GetArg("-checkpointkey", "");
        if (!Checkpoints::SetCheckpointPrivKey(sKey))
            return InitError(errortr("Unable to sign checkpoint, wrong checkpointkey?\n"));
        else
            LogPrintf("Checkpoint server enabled\n");
    }

#ifdef ENABLE_WALLET
    // InitRPCMining is needed here so getblocktemplate in the GUI debug console works properly.
    InitRPCMining();
#endif
    // Start the lightweight task scheduler thread
    CScheduler::Function serviceLoop = boost::bind(&CScheduler::serviceQueue, &scheduler);
    threadGroup.create_thread(boost::bind(&TraceThread<CScheduler::Function>, "scheduler", serviceLoop));

    /* Start the RPC server already.  It will be started in "warmup" mode
     * and not really process calls already (but it will signify connections
     * that the server is there and will be ready later).  Warmup mode will
     * be disabled when initialisation is finished.
     */
    if (!InitRPCWarmup(threadGroup))
        return false;

    int64_t nStart;

#if defined(USE_SSE2)
    scrypt_detect_sse2();
#endif

    // ********************************************************* Step 5: verify wallet database integrity
#ifdef ENABLE_WALLET
    if (!CWallet::Verify())
        return false;
#endif

    #ifdef ENABLE_WALLET
    StartShadowPoolManagerThread(threadGroup);
    #endif

    // ********************************************************* Step 6: network initialization
    // Note that we absolutely cannot open any actual connections
    // until the very end ("start node") as the UTXO/block state
    // is not yet setup and may end up being set up twice if we
    // need to reindex later.

    assert(!g_connman);
    g_connman = std::unique_ptr<CConnman>(new CConnman(GetRand(std::numeric_limits<uint64_t>::max()), GetRand(std::numeric_limits<uint64_t>::max())));
    CConnman& connman = *g_connman;

    if (gArgs.IsArgSet("-disablenet"))
        g_connman->SetNetworkActive(false);

    peerLogic.reset(new PeerLogicValidation(&connman));
    RegisterValidationInterface(peerLogic.get());
    RegisterNodeSignals(GetNodeSignals());

    // sanitize comments per BIP-0014, format user agent and check total size
    std::vector<std::string> uacomments;
    if (gArgs.IsArgSet("-uacomment")) {
        for(std::string cmt : gArgs.GetArgs("-uacomment"))
        {
            if (cmt != SanitizeString(cmt, SAFE_CHARS_UA_COMMENT))
                return InitError(strprintf(errortr("User Agent comment (%s) contains unsafe characters."), cmt));
            uacomments.push_back(cmt);
        }
    }
    strSubVersion = FormatSubVersion(CLIENT_NAME, CLIENT_VERSION, uacomments);
    if (strSubVersion.size() > MAX_SUBVERSION_LENGTH) {
        return InitError(strprintf(errortr("Total length of network version string (%i) exceeds maximum length (%i). Reduce the number or size of uacomments."),
            strSubVersion.size(), MAX_SUBVERSION_LENGTH));
    }

    if (gArgs.IsArgSet("-onlynet")) {
        std::set<enum Network> nets;
        for(const std::string& snet : gArgs.GetArgs("-onlynet")) {
            enum Network net = ParseNetwork(snet);
            if (net == NET_UNROUTABLE)
                return InitError(strprintf(errortr("Unknown network specified in -onlynet: '%s'"), snet));
            nets.insert(net);
        }
        for (int n = 0; n < NET_MAX; n++) {
            enum Network net = (enum Network)n;
            if (!nets.count(net))
                SetLimited(net);
        }
    }

    if (gArgs.IsArgSet("-whitelist")) {
        for(const std::string& net : gArgs.GetArgs("-whitelist")) {
            CSubNet subnet;
            LookupSubNet(net.c_str(), subnet);
            if (!subnet.IsValid())
                return InitError(strprintf(errortr("Invalid netmask specified in -whitelist: '%s'"), net));
            connman.AddWhitelistedRange(subnet);
        }
    }

    // Check for host lookup allowed before parsing any network related parameters
    fNameLookup = GetBoolArg("-dns", DEFAULT_NAME_LOOKUP);

    bool proxyRandomize = GetBoolArg("-proxyrandomize", DEFAULT_PROXYRANDOMIZE);
    // -proxy sets a proxy for all outgoing network traffic
    // -noproxy (or -proxy=0) as well as the empty string can be used to not set a proxy, this is the default
    std::string proxyArg = GetArg("-proxy", "");
    SetLimited(NET_TOR);
    if (proxyArg != "" && proxyArg != "0") {
        CService proxyAddr;
        if (!Lookup(proxyArg.c_str(), proxyAddr, 9050, fNameLookup)) {
            return InitError(strprintf(errortr("Invalid -proxy address or hostname: '%s'"), proxyArg));
        }

        proxyType addrProxy = proxyType(proxyAddr, proxyRandomize);
        if (!addrProxy.IsValid())
            return InitError(strprintf(errortr("Invalid -proxy address or hostname: '%s'"), proxyArg));

        SetProxy(NET_IPV4, addrProxy);
        SetProxy(NET_IPV6, addrProxy);
        SetProxy(NET_TOR, addrProxy);
        SetNameProxy(addrProxy);
        SetLimited(NET_TOR, false); // by default, -proxy sets onion as reachable, unless -noonion later
    }

    // -onion can be used to set only a proxy for .onion, or override normal proxy for .onion addresses
    // -noonion (or -onion=0) disables connecting to .onion entirely
    // An empty string is used to not override the onion proxy (in which case it defaults to -proxy set above, or none)
    std::string onionArg = GetArg("-onion", "");
    if (onionArg != "") {
        if (onionArg == "0") { // Handle -noonion/-onion=0
            SetLimited(NET_TOR); // set onions as unreachable
        } else {
            CService onionProxy;
            if (!Lookup(onionArg.c_str(), onionProxy, 9050, fNameLookup)) {
                return InitError(strprintf(errortr("Invalid -onion address or hostname: '%s'"), onionArg));
            }
            proxyType addrOnion = proxyType(onionProxy, proxyRandomize);
            if (!addrOnion.IsValid())
                return InitError(strprintf(errortr("Invalid -onion address or hostname: '%s'"), onionArg));
            SetProxy(NET_TOR, addrOnion);
            SetLimited(NET_TOR, false);
        }
    }

    // see Step 2: parameter interactions for more information about these
    fListen = GetBoolArg("-listen", DEFAULT_LISTEN);
    fDiscover = GetBoolArg("-discover", true);
    fRelayTxes = !GetBoolArg("-blocksonly", DEFAULT_BLOCKSONLY);

    //fixme: (2.1) Improve exception handling here
    try
    {
        if (fListen)
        {
            bool fBound = false;
            if (gArgs.IsArgSet("-bind")) {
                for(const std::string& strBind : gArgs.GetArgs("-bind"))
                {
                    CService addrBind;
                    if (!Lookup(strBind.c_str(), addrBind, GetListenPort(), false))
                        return InitError(ResolveErrMsg("bind", strBind));
                    fBound |= Bind(connman, addrBind, (BF_EXPLICIT | BF_REPORT_ERROR));
                }
            }
            if (gArgs.IsArgSet("-whitebind"))
            {
                for(const std::string& strBind : gArgs.GetArgs("-whitebind"))
                {
                    CService addrBind;
                    if (!Lookup(strBind.c_str(), addrBind, 0, false))
                        return InitError(ResolveErrMsg("whitebind", strBind));
                    if (addrBind.GetPort() == 0)
                        return InitError(strprintf(errortr("Need to specify a port with -whitebind: '%s'"), strBind));
                    fBound |= Bind(connman, addrBind, (BF_EXPLICIT | BF_REPORT_ERROR | BF_WHITELIST));
                }
            }
            if (!gArgs.IsArgSet("-bind") && !gArgs.IsArgSet("-whitebind"))
            {
                struct in_addr inaddr_any;
                inaddr_any.s_addr = INADDR_ANY;
                fBound |= Bind(connman, CService((in6_addr)IN6ADDR_ANY_INIT, GetListenPort()), BF_NONE);
                fBound |= Bind(connman, CService(inaddr_any, GetListenPort()), !fBound ? BF_REPORT_ERROR : BF_NONE);
            }
            if (!fBound)
                return InitError(errortr("Failed to listen on any port. Use -listen=0 if you want this."));
        }
    }
    catch(...)
    {
        return InitError(errortr("Failed to listen on any port. Use -listen=0 if you want this."));
    }

    if (gArgs.IsArgSet("-externalip")) {
        for(const std::string& strAddr : gArgs.GetArgs("-externalip")) {
            CService addrLocal;
            if (Lookup(strAddr.c_str(), addrLocal, GetListenPort(), fNameLookup) && addrLocal.IsValid())
                AddLocal(addrLocal, LOCAL_MANUAL);
            else
                return InitError(ResolveErrMsg("externalip", strAddr));
        }
    }

#if ENABLE_ZMQ
    pzmqNotificationInterface = CZMQNotificationInterface::Create();

    if (pzmqNotificationInterface) {
        RegisterValidationInterface(pzmqNotificationInterface);
    }
#endif
    uint64_t nMaxOutboundLimit = 0; //unlimited unless -maxuploadtarget is set
    uint64_t nMaxOutboundTimeframe = MAX_UPLOAD_TIMEFRAME;

    if (IsArgSet("-maxuploadtarget")) {
        nMaxOutboundLimit = GetArg("-maxuploadtarget", DEFAULT_MAX_UPLOAD_TARGET)*1024*1024;
    }

    // ********************************************************* Step 7: load block chain

    fReverseHeaders = GetBoolArg("-reverseheaders", true);
    fReindex = GetBoolArg("-reindex", false);
    bool fReindexChainState = GetBoolArg("-reindex-chainstate", false);

    fs::create_directories(GetDataDir() / "blocks");

    // cache size calculations
    int64_t nTotalCache = (GetArg("-dbcache", nDefaultDbCache) << 20);
    nTotalCache = std::max(nTotalCache, nMinDbCache << 20); // total cache cannot be less than nMinDbCache
    nTotalCache = std::min(nTotalCache, nMaxDbCache << 20); // total cache cannot be greater than nMaxDbcache
    int64_t nBlockTreeDBCache = nTotalCache / 8;
    nBlockTreeDBCache = std::min(nBlockTreeDBCache, (GetBoolArg("-txindex", DEFAULT_TXINDEX) ? nMaxBlockDBAndTxIndexCache : nMaxBlockDBCache) << 20);
    nTotalCache -= nBlockTreeDBCache;
    int64_t nCoinDBCache = std::min(nTotalCache / 2, (nTotalCache / 4) + (1 << 23)); // use 25%-50% of the remainder for disk cache
    nCoinDBCache = std::min(nCoinDBCache, nMaxCoinsDBCache << 20); // cap total coins db cache
    nTotalCache -= nCoinDBCache;
    nCoinCacheUsage = nTotalCache; // the rest goes to in-memory cache
    int64_t nMempoolSizeMax = GetArg("-maxmempool", DEFAULT_MAX_MEMPOOL_SIZE) * 1000000;
    LogPrintf("Cache configuration:\n");
    LogPrintf("* Using %.1fMiB for block index database\n", nBlockTreeDBCache * (1.0 / 1024 / 1024));
    LogPrintf("* Using %.1fMiB for chain state database\n", nCoinDBCache * (1.0 / 1024 / 1024));
    LogPrintf("* Using %.1fMiB for in-memory UTXO set (plus up to %.1fMiB of unused mempool space)\n", nCoinCacheUsage * (1.0 / 1024 / 1024), nMempoolSizeMax * (1.0 / 1024 / 1024));

    if (fReverseHeaders)
    {
        LogPrintf("Reverse header sync will temporarily use up to %.1fMiB until initial sync is complete", sizeof(CBlockHeader) * 1000000.0 / 1024.0 / 1024.0);
    }

    bool fLoaded = false;
    while (!fLoaded) {
        bool fReset = fReindex;
        std::string strLoadError;

        uiInterface.InitMessage(_("Loading block index..."));

        nStart = GetTimeMillis();
        do {
            try {
                static bool upgradeOnceOnly=true;
                UnloadBlockIndex();
                loadblockindex:
                delete pcoinsTip;
                delete pcoinsdbview;
                delete pcoinscatcher;
                delete pblocktree;

                pblocktree = new CBlockTreeDB(nBlockTreeDBCache, false, fReindex);
                pcoinsdbview = new CCoinsViewDB(nCoinDBCache, false, fReindex || fReindexChainState);
                pcoinscatcher = new CCoinsViewErrorCatcher(pcoinsdbview);
                pcoinsTip = new CCoinsViewCache(pcoinscatcher);

                delete ppow2witdbview;
                delete ppow2witcatcher;
                ppow2witTip = nullptr;

                ppow2witdbview = new CWitViewDB(nCoinDBCache, false, fReindex || fReindexChainState);
                ppow2witcatcher = new CCoinsViewErrorCatcher(ppow2witdbview);
                ppow2witTip = std::shared_ptr<CCoinsViewCache>(new CCoinsViewCache(ppow2witcatcher));

                pcoinsTip->SetSiblingView(ppow2witTip);


                if (fReindex) {
                    pblocktree->WriteReindexing(true);
                    //If we're reindexing in prune mode, wipe away unusable block files and all undo data files
                    if (fPruneMode)
                        CleanupBlockRevFiles();
                } else {
                    // If necessary, upgrade from older database format.
                    if (!pcoinsdbview->Upgrade()) {
                        strLoadError = errortr("Error upgrading chainstate database");
                        break;
                    }
                }

                //GULDEN - version 2.0 upgrade
                if (upgradeOnceOnly && pcoinsdbview->nPreviousVersion < 1)
                {
                    bool fullResyncForUpgrade = IsArgSet("-resyncforblockindexupgrade");
                    if (fullResyncForUpgrade)
                    {
                        upgradeOnceOnly = false;
                        uiInterface.InitMessage(_("Erasing block index..."));
                        UnloadBlockIndex();
                        fReindex = true;
                        blockStore.Delete();
                        goto loadblockindex;
                    }
                }

                if (!LoadBlockIndex(chainparams)) {
                    strLoadError = errortr("Error loading block database");
                    break;
                }

                //GULDEN - version 2.0 upgrade
                if (upgradeOnceOnly && pcoinsdbview->nPreviousVersion < 1)
                {
                    bool fullResyncForUpgrade = IsArgSet("-resyncforblockindexupgrade");
                    upgradeOnceOnly = false;
                    if (!fullResyncForUpgrade)
                    {
                        uiInterface.InitMessage(_("Upgrading block index..."));
                        if (!UpgradeBlockIndex(chainparams, pcoinsdbview->nPreviousVersion, pcoinsdbview->nCurrentVersion))
                        {
                            LogPrintf("Error upgrading block database to 2.0 (segsig) format, attempting to wipe index and resync instead.");
                            fullResyncForUpgrade = true;
                        }
                        else
                        {
                            uiInterface.InitMessage(_("Reloading block index..."));
                            // Flush and reload index
                            FlushStateToDisk();
                            UnloadBlockIndex();
                        }
                    }
                    if (fullResyncForUpgrade)
                    {
                        uiInterface.InitMessage(_("Erasing block index..."));
                        UnloadBlockIndex();
                        fReindex = true;
                        blockStore.Delete();
                        goto loadblockindex;
                    }
                    goto loadblockindex;
                }

                // If the loaded chain has a wrong genesis, bail out immediately
                // (we're likely using a testnet datadir, or the other way around).
                if (!mapBlockIndex.empty() && mapBlockIndex.count(chainparams.GetConsensus().hashGenesisBlock) == 0)
                    return InitError(_("Incorrect or no genesis block found. Wrong datadir for network?"));

                // Initialize the block index (no-op if non-empty database was already loaded)
                if (!InitBlockIndex(chainparams)) {
                    strLoadError = errortr("Error initializing block database");
                    break;
                }

                // Check for changed -txindex state
                if (fTxIndex != GetBoolArg("-txindex", DEFAULT_TXINDEX)) {
                    strLoadError = errortr("You need to rebuild the database using -reindex-chainstate to change -txindex");
                    break;
                }

                // Check for changed -prune state.  What we are concerned about is a user who has pruned blocks
                // in the past, but is now trying to run unpruned.
                if (fHavePruned && !fPruneMode) {
                    strLoadError = errortr("You need to rebuild the database using -reindex to go back to unpruned mode.  This will redownload the entire blockchain");
                    break;
                }

                if (!fReindex && chainActive.Tip() != NULL) {
                    uiInterface.InitMessage(_("Rewinding blocks..."));
                    if (!RewindBlockIndex(chainparams)) {
                        strLoadError = errortr("Unable to rewind the database to a pre-fork state. You will need to redownload the blockchain");
                        break;
                    }
                }

                uiInterface.InitMessage(_("Verifying blocks..."));
                if (fHavePruned && GetArg("-checkblocks", DEFAULT_CHECKBLOCKS) > MIN_BLOCKS_TO_KEEP) {
                    LogPrintf("Prune: pruned datadir may not have more than %d blocks; only checking available blocks",
                        MIN_BLOCKS_TO_KEEP);
                }

                {
                    LOCK(cs_main);
                    CBlockIndex* tip = chainActive.Tip();
                    RPCNotifyBlockChange(true, tip);
                    if (tip && tip->nTime > GetAdjustedTime() + 2 * 60 * 60) {
                        strLoadError = errortr("The block database contains a block which appears to be from the future. "
                                "This may be due to your computer's date and time being set incorrectly. "
                                "Only rebuild the block database if you are sure that your computer's date and time are correct");
                        break;
                    }
                }

                if (!CVerifyDB().VerifyDB(chainparams, pcoinsdbview, GetArg("-checklevel", DEFAULT_CHECKLEVEL),
                              GetArg("-checkblocks", DEFAULT_CHECKBLOCKS))) {
                    strLoadError = errortr("Corrupted block database detected");
                    break;
                }
            } catch (const std::exception& e) {
                LogPrintf("%s\n", e.what());
                strLoadError = errortr("Error opening block database");
                break;
            }

            fLoaded = true;
        } while(false);

        if (!fLoaded) {
            // first suggest a reindex
            if (!fReset) {
                auto fRet = uiInterface.ThreadSafeQuestion(
                    strLoadError + ".\n\n" + errortr("Do you want to rebuild the block database now?"),
                    strLoadError + ".\nPlease restart with -reindex or -reindex-chainstate to recover.",
                    "", CClientUIInterface::MSG_ERROR | CClientUIInterface::BTN_ABORT);
                if (fRet.value_or(false)) {
                    fReindex = true;
                } else {
                    LogPrintf("Aborted block database rebuild. Exiting.\n");
                    return false;
                }
            } else {
                return InitError(strLoadError);
            }
        }
    }

    // As LoadBlockIndex can take several minutes, it's possible the user
    // requested to kill the GUI during the last operation. If so, exit.
    // As the program has not fully started yet, Shutdown() is possibly overkill.
    if (ShutdownRequested())
    {
        LogPrintf("Shutdown requested. Exiting.\n");
        return false;
    }
    LogPrintf(" block index %15dms\n", GetTimeMillis() - nStart);

    fs::path est_path = GetDataDir() / FEE_ESTIMATES_FILENAME;
    CAutoFile est_filein(fsbridge::fopen(est_path, "rb"), SER_DISK, CLIENT_VERSION);
    // Allowed to fail as this file IS missing on first startup.
    if (!est_filein.IsNull())
        ::feeEstimator.Read(est_filein);
    fFeeEstimatesInitialized = true;

    // ********************************************************* Step 8: load wallet
#ifdef ENABLE_WALLET
    if (!CWallet::InitLoadWallet())
        return false;
#else
    LogPrintf("No wallet support compiled in!\n");
#endif

    StartPoW2WitnessThread(threadGroup);

    // ********************************************************* Step 9: data directory maintenance

    // if pruning, unset the service bit and perform the initial blockstore prune
    // after any wallet rescanning has taken place.
    if (fPruneMode) {
        LogPrintf("Unsetting NODE_NETWORK on prune mode\n");
        nLocalServices = ServiceFlags(nLocalServices & ~NODE_NETWORK);
        if (!fReindex) {
            uiInterface.InitMessage(_("Pruning blockstore..."));
            PruneAndFlush();
        }
    }

    if (chainparams.GetConsensus().vDeployments[Consensus::DEPLOYMENT_POW2_PHASE4].nTimeout != 0) {
        // Only advertise witness capabilities if they have a reasonable start time.
        // This allows us to have the code merged without a defined softfork, by setting its
        // end time to 0.
        // Note that setting NODE_SEGSIG is never required: the only downside from not
        // doing so is that after activation, no upgraded nodes will fetch from you.
        nLocalServices = ServiceFlags(nLocalServices | NODE_SEGSIG);
        // Only care about others providing witness capabilities if there is a softfork
        // defined.
        nRelevantServices = ServiceFlags(nRelevantServices | NODE_SEGSIG);
    }

    // ********************************************************* Step 10: import blocks

    if (!CheckDiskSpace())
        return false;

    // Either install a handler to notify us when genesis activates, or set fHaveGenesis directly.
    // No locking, as this happens before any background thread is started.
    if (chainActive.Tip() == NULL) {
        uiInterface.NotifyBlockTip.connect(BlockNotifyGenesisWait);
    } else {
        fHaveGenesis = true;
    }

    if (IsArgSet("-blocknotify"))
        uiInterface.NotifyBlockTip.connect(BlockNotifyCallback);

    std::vector<fs::path> vImportFiles;
    if (gArgs.IsArgSet("-loadblock"))
    {
        for(const std::string& strFile : gArgs.GetArgs("-loadblock"))
            vImportFiles.push_back(strFile);
    }

    threadGroup.create_thread(boost::bind(&ThreadImport, vImportFiles));

    // Wait for genesis block to be processed
    {
        boost::unique_lock<boost::mutex> lock(cs_GenesisWait);
        while (!fHaveGenesis) {
            condvar_GenesisWait.wait(lock);
        }
        uiInterface.NotifyBlockTip.disconnect(BlockNotifyGenesisWait);
    }

    // ********************************************************* Step 11: start node

    //// debug print
    LogPrintf("mapBlockIndex.size() = %u\n",   mapBlockIndex.size());
    LogPrintf("nBestHeight = %d\n",                   chainActive.Height());
    //LogPrintf("setKeyPoolExternal.size() = %u\n",      pwalletMain ? pwalletMain->setKeyPoolExternal.size() : 0);
    InitTor(threadGroup, scheduler);

    Discover(threadGroup);

    // Map ports with UPnP
    MapPort(GetBoolArg("-upnp", DEFAULT_UPNP));

    std::string strNodeError;
    CConnman::Options connOptions;
    connOptions.nLocalServices = nLocalServices;
    connOptions.nRelevantServices = nRelevantServices;
    connOptions.nMaxConnections = nMaxConnections;
    connOptions.nMaxOutbound = std::min(MAX_OUTBOUND_CONNECTIONS, connOptions.nMaxConnections);
    connOptions.nMaxAddnode = MAX_ADDNODE_CONNECTIONS;
    connOptions.nMaxFeeler = 1;
    connOptions.nBestHeight = chainActive.Height();
    connOptions.uiInterface = &uiInterface;
    connOptions.nSendBufferMaxSize = 1000*GetArg("-maxsendbuffer", DEFAULT_MAXSENDBUFFER);
    connOptions.nReceiveFloodSize = 1000*GetArg("-maxreceivebuffer", DEFAULT_MAXRECEIVEBUFFER);

    connOptions.nMaxOutboundTimeframe = nMaxOutboundTimeframe;
    connOptions.nMaxOutboundLimit = nMaxOutboundLimit;

    if (gArgs.IsArgSet("-seednode")) {
        connOptions.vSeedNodes = gArgs.GetArgs("-seednode");
    }

    if (!connman.Start(scheduler, strNodeError, connOptions))
        return InitError(strNodeError);


    // Generate coins in the background
    PoWMineGulden(GetBoolArg("-gen", DEFAULT_GENERATE), GetArg("-genproclimit", DEFAULT_GENERATE_THREADS), chainparams);
    // ********************************************************* Step 12: finished

    SetRPCWarmupFinished();
    uiInterface.InitMessage(_("Done loading"));

#ifdef ENABLE_WALLET
    for (CWalletRef pwallet : vpwallets) {
        pwallet->postInitProcess(scheduler);
    }
#endif

    return !ShutdownRequested();
}
