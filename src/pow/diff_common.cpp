// Copyright (c) 2015-2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING
#include "diff_common.h"
#include "diff_delta.h"
#include "diff_old.h"
#include "../../chainparams.h"

unsigned int GetNextWorkRequired(const CBlockIndex* indexLast, const CBlockHeader* block, unsigned int nPowTargetSpacing, unsigned int nPowLimit)
{
    if (Params().GetConsensus().fPowNoRetargeting)
        return indexLast->nBits;

    static int nDeltaSwitchoverBlock = DIFF_SWITCHOVER(10, 250000);
    static int nOldDiffSwitchoverBlock = DIFF_SWITCHOVER(0, 1529258);

    if ((indexLast->nHeight+1) >= nOldDiffSwitchoverBlock)
    {
        // Delta can't handle a target spacing of 1 - so we just assume a static diff for testnets where target spacing is 1.
        if (nPowTargetSpacing>1 && (indexLast->nHeight+1) >= nDeltaSwitchoverBlock)
        {
            return GetNextWorkRequired_DELTA(indexLast, block, nPowTargetSpacing, nPowLimit, nDeltaSwitchoverBlock);
        }
        else
        {
            return 524287999;
        }
    }
    return diff_old(indexLast->nHeight+1, nPowLimit);
}
