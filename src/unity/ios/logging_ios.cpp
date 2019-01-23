// Copyright (c) 2018 The Gulden developers
// Authored by: Willem de Jonge (willem@isnapp.nl)
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <string>
#include "../unity_impl.h"
#include "gulden_unified_frontend.hpp"

void OpenDebugLog()
{
}

int LogPrintStr(const std::string &str)
{
    signalHandler->logPrint(str);
    return str.size();
}
