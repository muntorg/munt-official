// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_LOGGING_H
#define BITCOIN_LOGGING_H

#include <fs.h>
#include <tinyformat.h>
#include <threadsafety.h>

#include <atomic>
#include <cstdint>
#include <functional>
#include <list>
#include <mutex>
#include <string>
#include <vector>

extern std::atomic<uint32_t> logCategories;

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
        WALLET      = (1 << 25),
        LOCK        = (1 << 26),
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

template<typename... Args>
bool error(const char* fmt, const Args&... args)
{
    LogPrintStr("ERROR: " + tfm::format(fmt, args...) + "\n");
    return false;
}

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

// Use a macro instead of a function for conditional logging to prevent
// evaluating arguments when logging for the category is not enabled.
#define LogPrint(category, ...)              \
    do {                                     \
        if (LogAcceptCategory((category))) { \
            LogPrintf(__VA_ARGS__);          \
        }                                    \
    } while (0)

#endif // BITCOIN_LOGGING_H
