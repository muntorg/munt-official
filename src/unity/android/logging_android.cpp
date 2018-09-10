// Copyright (c) 2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <string>
#include <android/log.h>

void OpenDebugLog()
{
}

int LogPrintStr(const std::string &str)
{
    return __android_log_print(ANDROID_LOG_INFO, "gulden_unity_jni_", "%s", str.c_str());
}
