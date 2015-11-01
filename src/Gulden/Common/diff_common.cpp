// Copyright (c) 2015 The Gulden developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifdef __APPLE__
    #include <TargetConditionals.h>
#endif
#include "diff_common.h"
#include "diff_delta.h"
#include "diff_old.h"
#ifdef BUILD_IOS
    #import "BRMerkleBlock.h"
#endif

unsigned int GetNextWorkRequired(const INDEX_TYPE indexLast, const BLOCK_TYPE block, unsigned int nPowTargetSpacing, unsigned int nPowLimit)
{
    static int nDeltaSwitchoverBlock = DIFF_SWITCHOVER(350000, 1, 250000);
    static int nOldDiffSwitchoverBlock = DIFF_SWITCHOVER(200, 100, 260000);

    if (INDEX_HEIGHT(indexLast)+1 >= nOldDiffSwitchoverBlock)
    {
        if (INDEX_HEIGHT(indexLast)+1 >= nDeltaSwitchoverBlock)
        {
            return GetNextWorkRequired_DELTA(indexLast, block, nPowTargetSpacing, nPowLimit, nDeltaSwitchoverBlock);
        }
        else
        {
            return 524287999;
        }
    }
    return diff_old(INDEX_HEIGHT(indexLast)+1);
}


