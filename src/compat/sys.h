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

#include <sys/sysinfo.h>
#include <stdint.h>

uint64_t systemPhysicalMemoryInBytes()
{
    struct sysinfo info;
    sysinfo(&info);
    return info.totalram/info.mem_unit;
}

#endif
