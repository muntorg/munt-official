// Copyright (c) 2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "winshutdownmonitor.h"

#if defined(Q_OS_WIN) && QT_VERSION >= 0x050000
#include "init.h"
#include "util.h"

#include <windows.h>

#include <QDebug>

#include <openssl/rand.h>

// If we don't want a message to be processed by Qt, return true and set result to
// the value that the window procedure should return. Otherwise return false.
bool WinShutdownMonitor::nativeEventFilter(const QByteArray& eventType, void* pMessage, long* pnResult)
{
    Q_UNUSED(eventType);

    MSG* pMsg = static_cast<MSG*>(pMessage);

    if (RAND_event(pMsg->message, pMsg->wParam, pMsg->lParam) == 0) {

        static bool warned = false;
        if (!warned) {
            LogPrintf("%s: OpenSSL RAND_event() failed to seed OpenSSL PRNG with enough data.\n", __func__);
            warned = true;
        }
    }

    switch (pMsg->message) {
    case WM_QUERYENDSESSION: {

        StartShutdown();
        *pnResult = FALSE;
        return true;
    }

    case WM_ENDSESSION: {
        *pnResult = FALSE;
        return true;
    }
    }

    return false;
}

void WinShutdownMonitor::registerShutdownBlockReason(const QString& strReason, const HWND& mainWinId)
{
    typedef BOOL(WINAPI * PSHUTDOWNBRCREATE)(HWND, LPCWSTR);
    PSHUTDOWNBRCREATE shutdownBRCreate = (PSHUTDOWNBRCREATE)GetProcAddress(GetModuleHandleA("User32.dll"), "ShutdownBlockReasonCreate");
    if (shutdownBRCreate == NULL) {
        qWarning() << "registerShutdownBlockReason: GetProcAddress for ShutdownBlockReasonCreate failed";
        return;
    }

    if (shutdownBRCreate(mainWinId, strReason.toStdWString().c_str()))
        qWarning() << "registerShutdownBlockReason: Successfully registered: " + strReason;
    else
        qWarning() << "registerShutdownBlockReason: Failed to register: " + strReason;
}
#endif
