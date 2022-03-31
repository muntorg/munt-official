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

void PrintExceptionContinue(const std::exception *pex, const char* pszThread);

namespace util {
/**
 * A wrapper for do-something-once thread functions.
 */
void TraceThread(const char* thread_name, std::function<void()> thread_func);

} // namespace util

#endif // CORE_UTIL_THREAD_H
