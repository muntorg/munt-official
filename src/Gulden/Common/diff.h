// Copyright (c) 2015-2016 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#ifndef GULDEN_DIFF_H
#define GULDEN_DIFF_H

#include <consensus/params.h>
#include "diff_common.h"

unsigned int static GetNextWorkRequired(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params& params)
{
    return GetNextWorkRequired(pindexLast, pblock, params.nPowTargetSpacing, UintToArith256(params.powLimit).GetCompact());
}


#endif

