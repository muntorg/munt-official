// Copyright (c) 2015 The Gulden developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef GULDEN_DIFF_H
#define GULDEN_DIFF_H

#include "../consensus/params.h"
#include "diff_common.h"

unsigned int static GetNextWorkRequired(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params& params)
{
    return GetNextWorkRequired(pindexLast, pblock, params.nPowTargetSpacing, UintToArith256(params.powLimit).GetCompact());
}


#endif

