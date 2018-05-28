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

/**
 * Server/client environment: argument handling, config file parsing,
 * logging, thread wrappers
 */
#ifndef GULDEN_UTIL_H
#define GULDEN_UTIL_H

#if defined(HAVE_CONFIG_H)
#include "config/gulden-config.h"
#endif

#include "compat.h"
#include "fs.h"
#include "sync.h"
#include "tinyformat.h"
#include "utiltime.h"

#include <atomic>
#include <exception>
#include <map>
#include <stdint.h>
#include <string>
#include <vector>

#include <boost/signals2/signal.hpp>

static const bool DEFAULT_LOGTIMEMICROS = false;
static const bool DEFAULT_LOGIPS        = false;
static const bool DEFAULT_LOGTIMESTAMPS = true;

/** Signals for translation. */
class CTranslationInterface
{
public:
    /** Translate a message to the native language of the user. */
    boost::signals2::signal<std::string (const char* psz)> Translate;
};

extern bool fPrintToConsole;
extern bool fPrintToDebugLog;
extern bool fNoUI;
extern bool fLogTimestamps;
extern bool fLogTimeMicros;
extern bool fLogIPs;
extern std::atomic<bool> fReopenDebugLog;
extern CTranslationInterface translationInterface;

extern const char * const GULDEN_CONF_FILENAME;
extern const char * const GULDEN_PID_FILENAME;

extern std::atomic<uint32_t> logCategories;

/**
 * Translation function: Call Translate signal on UI interface, which returns a boost::optional result.
 * If no translation slot is registered, nothing is returned, and simply return the input.
 */
inline std::string _(const char* psz)
{
    boost::optional<std::string> rv = translationInterface.Translate(psz);
    return rv ? (*rv) : psz;
}

void SetupEnvironment();
bool SetupNetworking();

struct CLogCategoryActive
{
    std::string category;
    bool active;
};

namespace BCLog {
    enum LogFlags : uint32_t {
        NONE        = 0,
        ALERT       = (1 <<  0),
        NET         = (1 <<  1),
        TOR         = (1 <<  2),
        MEMPOOL     = (1 <<  3),
        HTTP        = (1 <<  4),
        BENCH       = (1 <<  5),
        ZMQ         = (1 <<  6),
        DB          = (1 <<  7),
        RPC         = (1 <<  8),
        ESTIMATEFEE = (1 <<  9),
        ADDRMAN     = (1 << 10),
        SELECTCOINS = (1 << 11),
        REINDEX     = (1 << 12),
        CMPCTBLOCK  = (1 << 13),
        RAND        = (1 << 14),
        PRUNE       = (1 << 15),
        PROXY       = (1 << 16),
        MEMPOOLREJ  = (1 << 17),
        LIBEVENT    = (1 << 18),
        COINDB      = (1 << 19),
        QT          = (1 << 20),
        LEVELDB     = (1 << 21),
        DELTA       = (1 << 22),
        WITNESS     = (1 << 23),
        IO          = (1 << 24),
        ALL         = ~(uint32_t)0,
    };
}
/** Return true if log accepts specified category */
static inline bool LogAcceptCategory(uint32_t category)
{
    return (logCategories.load(std::memory_order_relaxed) & category) != 0;
}

/** Returns a string with the log categories. */
std::string ListLogCategories();

/** Returns a vector of the active log categories. */
std::vector<CLogCategoryActive> ListActiveLogCategories();

/** Return true if str parses as a log category and set the flags in f */
bool GetLogCategory(uint32_t *f, const std::string *str);

/** Send a string to the log output */
int LogPrintStr(const std::string &str);

/** Get format string from VA_ARGS for error reporting */
template<typename... Args> std::string FormatStringFromLogArgs(const char *fmt, [[maybe_unused]] const Args&... args) { return fmt; }

#define LogPrintf(...) do { \
    std::string _log_msg_; /* Unlikely name to avoid shadowing variables */ \
    try { \
        _log_msg_ = tfm::format(__VA_ARGS__); \
    } catch (tinyformat::format_error &fmterr) { \
        /* Original format string will have newline so don't add one here */ \
        _log_msg_ = "Error \"" + std::string(fmterr.what()) + "\" while formatting log message: " + FormatStringFromLogArgs(__VA_ARGS__); \
    } \
    LogPrintStr(_log_msg_); \
} while(0)

#define LogPrint(category, ...) do { \
    if (LogAcceptCategory((category))) { \
        LogPrintf(__VA_ARGS__); \
    } \
} while(0)

template<typename... Args>
bool error(const char* fmt, const Args&... args)
{
    LogPrintStr("ERROR: " + tfm::format(fmt, args...) + "\n");
    return false;
}

void PrintExceptionContinue(const std::exception *pex, const char* pszThread);
void FileCommit(FILE *file);
bool TruncateFile(FILE *file, unsigned int length);
int RaiseFileDescriptorLimit(int nMinFD);
void AllocateFileRange(FILE *file, unsigned int offset, unsigned int length);
bool RenameOver(fs::path src, fs::path dest);
bool TryCreateDirectory(const fs::path& p);
fs::path GetDefaultDataDir();
const fs::path &GetDataDir(bool fNetSpecific = true);
void ClearDatadirCache();
fs::path GetConfigFile(const std::string& confPath);
#ifndef WIN32
fs::path GetPidFile();
void CreatePidFile(const fs::path &path, pid_t pid);
#endif
#ifdef WIN32
fs::path GetSpecialFolderPath(int nFolder, bool fCreate = true);
#endif
void OpenDebugLog();
void ShrinkDebugFile();
void runCommand(const std::string& strCommand);

inline bool IsSwitchChar(char c)
{
#ifdef WIN32
    return c == '-' || c == '/';
#else
    return c == '-';
#endif
}

class ArgsManager
{
protected:
    CCriticalSection cs_args;
    std::map<std::string, std::string> mapArgs;
    std::map<std::string, std::vector<std::string> > mapMultiArgs;
public:
    void ParseParameters(int argc, const char*const argv[]);
    void ReadConfigFile(const std::string& confPath);
    std::vector<std::string> GetArgs(const std::string& strArg);
/**
 * Return true if the given argument has been manually set
 *
 * @param strArg Argument to get (e.g. "-foo")
 * @return true if the argument has been set
 */
bool IsArgSet(const std::string& strArg);

/**
 * Return string argument or default value
 *
 * @param strArg Argument to get (e.g. "-foo")
 * @param default (e.g. "1")
 * @return command-line argument or default value
 */
std::string GetArg(const std::string& strArg, const std::string& strDefault);

/**
 * Return integer argument or default value
 *
 * @param strArg Argument to get (e.g. "-foo")
 * @param default (e.g. 1)
 * @return command-line argument (0 if invalid number) or default value
 */
int64_t GetArg(const std::string& strArg, int64_t nDefault);

/**
 * Return boolean argument or default value
 *
 * @param strArg Argument to get (e.g. "-foo")
 * @param default (true or false)
 * @return command-line argument or default value
 */
bool GetBoolArg(const std::string& strArg, bool fDefault);

/**
 * Set an argument if it doesn't already have a value
 *
 * @param strArg Argument to set (e.g. "-foo")
 * @param strValue Value (e.g. "1")
 * @return true if argument gets set, false if it already had a value
 */
bool SoftSetArg(const std::string& strArg, const std::string& strValue);

/**
 * Set a boolean argument if it doesn't already have a value
 *
 * @param strArg Argument to set (e.g. "-foo")
 * @param fValue Value (e.g. false)
 * @return true if argument gets set, false if it already had a value
 */
bool SoftSetBoolArg(const std::string& strArg, bool fValue);

// Forces an arg setting. Called by SoftSetArg() if the arg hasn't already
// been set. Also called directly in testing.
void ForceSetArg(const std::string& strArg, const std::string& strValue);
};

extern ArgsManager gArgs;

// wrappers using the global ArgsManager:
static inline void ParseParameters(int argc, const char*const argv[])
{
    gArgs.ParseParameters(argc, argv);
}

static inline void ReadConfigFile(const std::string& confPath)
{
    gArgs.ReadConfigFile(confPath);
}

static inline bool SoftSetArg(const std::string& strArg, const std::string& strValue)
{
    return gArgs.SoftSetArg(strArg, strValue);
}

static inline void ForceSetArg(const std::string& strArg, const std::string& strValue)
{
    gArgs.ForceSetArg(strArg, strValue);
}

static inline bool IsArgSet(const std::string& strArg)
{
    return gArgs.IsArgSet(strArg);
}

static inline std::string GetArg(const std::string& strArg, const std::string& strDefault)
{
    return gArgs.GetArg(strArg, strDefault);
}

static inline int64_t GetArg(const std::string& strArg, int64_t nDefault)
{
    return gArgs.GetArg(strArg, nDefault);
}

static inline bool GetBoolArg(const std::string& strArg, bool fDefault)
{
    return gArgs.GetBoolArg(strArg, fDefault);
}

static inline bool SoftSetBoolArg(const std::string& strArg, bool fValue)
{
    return gArgs.SoftSetBoolArg(strArg, fValue);
}

/**
 * Format a string to be used as group of options in help messages
 *
 * @param message Group name (e.g. "RPC server options:")
 * @return the formatted string
 */
std::string HelpMessageGroup(const std::string& message);

/**
 * Format a string to be used as option description in help messages
 *
 * @param option Option message (e.g. "-rpcuser=<user>")
 * @param message Option description (e.g. "Username for JSON-RPC connections")
 * @return the formatted string
 */
std::string HelpMessageOpt(const std::string& option, const std::string& message);

inline uint32_t ByteReverse(uint32_t value)
{
    value = ((value & 0xFF00FF00) >> 8) | ((value & 0x00FF00FF) << 8);
    return (value<<16) | (value>>16);
}

/**
 * Return the number of physical cores available on the current system.
 * @note This does not count virtual cores, such as those provided by HyperThreading
 * when boost is newer than 1.56.
 */
int GetNumCores();

void RenameThread(const char* name);

/**
 * .. and a wrapper that just calls func once
 */
template <typename Callable> void TraceThread(const char* name,  Callable func)
{
    std::string s = strprintf("Gulden-%s", name);
    RenameThread(s.c_str());
    try
    {
        LogPrintf("%s thread start\n", name);
        func();
        LogPrintf("%s thread exit\n", name);
    }
    catch (const boost::thread_interrupted&)
    {
        LogPrintf("%s thread interrupt\n", name);
        throw;
    }
    catch (const std::exception& e) {
        PrintExceptionContinue(&e, name);
        throw;
    }
    catch (...) {
        PrintExceptionContinue(NULL, name);
        throw;
    }
}

std::string CopyrightHolders(const std::string& strPrefix);

//fixme: (C++-14) We should be able to take the string desc as a constant compile time paramater as well.
// Little helper class to RAII encapusulate benchmarks at minimal runtime overhead.
template <uint32_t logCategory=BCLog::BENCH> class BenchMarkHelper
{
public:
    BenchMarkHelper(const char* strDesc_, uint64_t& nTotal_, uint64_t& nCounter_, const uint32_t nLogThreshold_=1)
    : strDesc(strDesc_)
    , nTotal(nTotal_)
    , nCounter(nCounter_)
    , nLogThreshold(nLogThreshold_)
    {
        nStart = GetTimeMicros();
        ++nCounter_;
    }
    ~BenchMarkHelper()
    {
        Split();
    }
    void Split()
    {
        uint64_t nTime1 = GetTimeMicros(); 
        nTotal += nTime1 - nStart;
        if (++nCounter % 100 == 0)
        {
            if (nTotal * 0.000001 > nLogThreshold)
            {
                //fixme: (C++-14) We should be able to concat the strDesc at compile time as well.
                LogPrint(logCategory, "%s: %.2fms [%.2fs]\n", strDesc, 0.001 * (nTime1 - nStart), nTotal * 0.000001);
            }
        }
    }
private:
    const char* strDesc;
    uint64_t& nTotal;
    uint64_t& nCounter;
    uint32_t nLogThreshold;
    uint64_t nStart;
};

/** Run a benchmark - only logs once every 100 calls, and only once overall time passes THRESHOLD (default 1) */
#define DO_BENCHMARK(DESC, LOGCATEGORY) static uint64_t nTotalBenchMarkTime = 0; static uint64_t nBenchmarkCounter = 0; BenchMarkHelper<LOGCATEGORY>(DESC, nTotalBenchMarkTime, nBenchmarkCounter);
#define DO_BENCHMARKT(DESC, LOGCATEGORY, THRESHOLD) static uint64_t nTotalBenchMarkTime = 0; static uint64_t nBenchmarkCounter = 0; BenchMarkHelper<LOGCATEGORY>(DESC, nTotalBenchMarkTime, nBenchmarkCounter, THRESHOLD);

#endif // GULDEN_UTIL_H
