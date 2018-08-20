// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// File contains modifications by: The Gulden developers
// All modifications:
// Copyright (c) 2017-2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING


#include "sync.h"
#include "clientversion.h"
#include "util.h"
#include "warnings.h"
#include "alert.h"
#include "validation/validation.h"
#include <Gulden/auto_checkpoints.h>

CCriticalSection cs_warnings;
std::string strMiscWarning;
bool fLargeWorkForkFound = false;
bool fLargeWorkInvalidChainFound = false;

void SetMiscWarning(const std::string& strWarning)
{
    LOCK(cs_warnings);
    strMiscWarning = strWarning;
}

void SetfLargeWorkForkFound(bool flag)
{
    LOCK(cs_warnings);
    fLargeWorkForkFound = flag;
}

bool GetfLargeWorkForkFound()
{
    LOCK(cs_warnings);
    return fLargeWorkForkFound;
}

void SetfLargeWorkInvalidChainFound(bool flag)
{
    LOCK(cs_warnings);
    fLargeWorkInvalidChainFound = flag;
}

bool GetfLargeWorkInvalidChainFound()
{
    LOCK(cs_warnings);
    return fLargeWorkInvalidChainFound;
}

std::string GetWarnings(const std::string& strFor)
{
    std::string strStatusBar;
    std::string strRPC;
    std::string strGUI;
    const std::string uiAlertSeperator = "<hr />";

    LOCK(Checkpoints::cs_hashSyncCheckpoint); // prevents potential deadlock being reported from tests
    LOCK(cs_warnings);

    if (!CLIENT_VERSION_IS_RELEASE) {
        strStatusBar = "This is a pre-release test build - use at your own risk - do not use for mining or merchant applications";
        strGUI = _("This is a pre-release test build - use at your own risk - do not use for mining or merchant applications");
    }

    if (GetBoolArg("-testsafemode", DEFAULT_TESTSAFEMODE))
        strStatusBar = strRPC = strGUI = "testsafemode enabled";

    // Misc warnings like out of disk space and clock is wrong
    if (strMiscWarning != "")
    {
        strStatusBar = strMiscWarning;
        strGUI += (strGUI.empty() ? "" : uiAlertSeperator) + strMiscWarning;
    }

    if (fLargeWorkForkFound)
    {
        strStatusBar = strRPC = "Warning: The network appears to be forked, please seek assistance.";
        strGUI += (strGUI.empty() ? "" : uiAlertSeperator) + _("Warning: The network appears to be forked, please seek assistance.");
    }
    else if (fLargeWorkInvalidChainFound)
    {
        strStatusBar = strRPC = "Warning: We do not appear to fully agree with our peers! You may need to upgrade, or other nodes may need to upgrade.";
        strGUI += (strGUI.empty() ? "" : uiAlertSeperator) + _("Warning: We do not appear to fully agree with our peers! You may need to upgrade, or other nodes may need to upgrade.");
    }


    // Gulden: Warn if sync-checkpoint is too old (Don't enter safe mode)
    if (Checkpoints::IsSyncCheckpointTooOld(2 * 60 * 60))
    {
        if (!IsInitialBlockDownload())
        {
            strStatusBar = strGUI = strRPC = "WARNING: Checkpoint is too old, please wait for a new checkpoint to arrive before engaging in any transactions.";
        }
    }


    // Gulden: if detected invalid checkpoint enter safe mode
    if (Checkpoints::hashInvalidCheckpoint != uint256())
    {
        strStatusBar = strGUI = strRPC = "WARNING: Invalid checkpoint found! Displayed transactions may not be correct! You may need to upgrade, or notify developers of the issue.";
    }

    // Alerts
    {
        LOCK(cs_mapAlerts);
        for(PAIRTYPE(const uint256, CAlert)& item : mapAlerts)
        {
            const CAlert& alert = item.second;
            if (alert.AppliesToMe() )
            {
                strStatusBar = strGUI = strRPC = alert.strStatusBar;
            }
        }
    }

    if (strFor == "gui")
        return strGUI;
    else if (strFor == "statusbar")
        return strStatusBar;
    else if (strFor == "rpc")
        return strRPC;
    assert(!"GetWarnings(): invalid parameter");
    return "error";
}
