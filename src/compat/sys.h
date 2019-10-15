// Copyright (c) 2019 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

// This file defines various operating system level helpers to get information about the current system

#ifndef GULDEN_COMPAT_SYS_H
#define GULDEN_COMPAT_SYS_H

#if defined(HAVE_CONFIG_H)
#include "config/gulden-config.h"
#endif

#include <stdint.h>

#if defined(WIN32)
#include <windows.h>
inline uint64_t systemPhysicalMemoryInBytes()
{
    MEMORYSTATUS status;
    status.dwLength = sizeof(status);
    GlobalMemoryStatus(&status);
    return status.dwTotalPhys;
}
#elif defined(MAC_OSX)
#include <sys/types.h>
#include <sys/sysctl.h>
inline uint64_t systemPhysicalMemoryInBytes()
{
    int dataRequest[2];
    dataRequest[0] = CTL_HW;
    dataRequest[1] = HW_MEMSIZE;
    int64_t totalRam = 0;
    size_t dataLen = sizeof(totalRam);
    if (sysctl(dataRequest, 2, &totalRam, &dataLen, nullptr, 0) == 0)
        return totalRam;
    return 0;
}
#else
#include <sys/sysinfo.h>
inline uint64_t systemPhysicalMemoryInBytes()
{
    struct sysinfo info;
    sysinfo(&info);
    return info.totalram/info.mem_unit;
}
#endif

#endif
