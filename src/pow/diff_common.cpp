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

    static int nDeltaSwitchoverBlock = 0;
    // We disable old_diff for now as we don't use it in any way
    //static int nOldDiffSwitchoverBlock = DIFF_SWITCHOVER(0, 1020000);

    //if ((indexLast->nHeight+1) >= nOldDiffSwitchoverBlock)
    //{
        // Delta can't handle a target spacing of 1 - so we just assume a static diff for testnets where target spacing is 1.
        if (nPowTargetSpacing>1 && (indexLast->nHeight+1) >= nDeltaSwitchoverBlock)
        {
            return GetNextWorkRequired_DELTA(indexLast, block, nPowTargetSpacing, nPowLimit, nDeltaSwitchoverBlock);
        }
        else
        {
            return nPowLimit;
        }
    //}
    //return diff_old(indexLast->nHeight+1, nPowLimit);
}
