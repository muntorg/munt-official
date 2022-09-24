// Copyright (c) 2015-2022 The Centure developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the Libre Chain License, see the accompanying
// file COPYING

#ifndef POW_DIFF_H
#define POW_DIFF_H

#include <consensus/params.h>
#include "diff_common.h"

unsigned int static GetNextWorkRequired(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params& params)
{
    return GetNextWorkRequired(pindexLast, pblock, params.nPowTargetSpacing, UintToArith256(params.powLimit).GetCompact());
}


#endif

