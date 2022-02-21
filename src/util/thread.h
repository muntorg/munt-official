// Copyright (c) 2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef CORE_UTIL_THREAD_H
#define CORE_UTIL_THREAD_H

#include <functional>
#include <string>
#include <appname.h>
#include <tinyformat.h>
#include "logging.h"
#include <boost/thread/thread.hpp>

void RenameThread(const char* name);

void PrintExceptionContinue(const std::exception *pex, const char* pszThread);

/**
 * A wrapper for do-something-once thread functions.
 */
template <typename Callable> void TraceThread(const char* name,  Callable func)
{
    std::string s = strprintf(GLOBAL_APPNAME "-%s", name);
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

#endif // CORE_UTIL_THREAD_H
