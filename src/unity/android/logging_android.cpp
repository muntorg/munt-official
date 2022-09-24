// Copyright (c) 2018-2022 The Centure developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "appname.h"
#include <string>
#include <android/log.h>
#include "../unity_impl.h"
#include "i_library_listener.hpp"

void OpenDebugLog()
{
}

int LogPrintStr(const std::string &str)
{
    return __android_log_print(ANDROID_LOG_INFO, GLOBAL_APPNAME"_core_jni_", "%s", str.c_str());
}

void UnityReportError(const std::string &str)
{
    if (signalHandler)
    {
        signalHandler->notifyError(str);
    }
}
