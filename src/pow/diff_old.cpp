// Copyright (c) 2015-2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING
//
// This file replaces the old Difficulty Re-adjustment algorithms
// It returns a ZERO DIFF when out of range, which can be catched as
// error after the call (resulting diff > params.powLimit)

#include <stdint.h>
#include "diff_common.h"

const int32_t nMaxHeight = 1605366;
const int32_t nDiffArraySize = nMaxHeight + 1;
const int32_t udiff[nDiffArraySize] = {
 #include "../data/static_diff_data.cpp"
};


unsigned int diff_old(int nHeight, unsigned int nPowLimit)
{
    unsigned int nRet;

    if ((nHeight < 0) || (nHeight > nMaxHeight))
    {
        LogPrintf("DIFF_old: block height out of range (%d)\n", nHeight);
        return 0;
    }
    nRet = (unsigned int)udiff[nHeight];

    static bool fDebug = LogAcceptCategory(BCLog::DELTA);
    if (fDebug)
    {
        static RecursiveMutex logCS;
        LOCK(logCS);
        LogPrintf("<STATICDIFF> Height=%d Diff=%08x\n", nHeight, nRet);
    }

    return nRet;
}
