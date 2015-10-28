// Copyright (c) 2015 The Gulden developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef GULDEN_DIFF_COMMON_H
#define GULDEN_DIFF_COMMON_H

#ifdef TARGET_OS_IPHONE
    #define fDebug false
    #define LogPrintf(MSG)
    #define BLOCK_TYPE BRMerkleBlock*
    #define BLOCK_TIME(block) block.timestamp
    #define INDEX_TYPE BRMerkleBlock*
    #define INDEX_HEIGHT(block) block.height
    #define INDEX_TIME(block) block.timestamp
    #define INDEX_PREV(block) [[BRPeerManager sharedInstance] blockForHash:(block.previous)]
    #define INDEX_TARGET(block) block.target
#else
    #include "../consensus/params.h"
    #include "../arith_uint256.h"
    #include "../chain.h"
    #include "../sync.h"
    #include "util.h"
    #include <stdint.h>
    #define BLOCK_TYPE CBlockHeader*
    #define BLOCK_TIME(block) block->nTime
    #define INDEX_TYPE CBlockIndex*
    #define INDEX_HEIGHT(block) block->nHeight
    #define INDEX_TIME(block) block->GetBlockTime()
    #define INDEX_PREV(block) block->pprev
    #define INDEX_TARGET(block) block->nBits
#endif

#include "diff_delta.h"
#include "diff_old.h"

unsigned int static GetNextWorkRequired(const INDEX_TYPE indexLast, const BLOCK_TYPE block, int64_t nPowTargetSpacing, unsigned int nPowLimit)
{
    static int nDeltaSwitchoverBlock = GetBoolArg("-testnet", false) ? 350000 : (GetBoolArg("-testnetaccel", false) ? 1 : 213500);
    static int nOldDiffSwitchoverBlock = GetBoolArg("-testnet", false) ? 500 : (GetBoolArg("-testnetaccel", false) ? 500 : 260000);

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


#endif

