// Copyright (c) 2015 The Gulden developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef GULDEN_DIFF_COMMON_H
#define GULDEN_DIFF_COMMON_H

#ifdef TARGET_OS_IPHONE
    #define BUILD_IOS
    #define fDebug false
    #define LogPrintf(...)
    #define BLOCK_TYPE BRMerkleBlock*
    #define BLOCK_TIME(block) block.timestamp
    #define INDEX_TYPE BRMerkleBlock*
    #define INDEX_HEIGHT(block) block.height
    #define INDEX_TIME(block) block.timestamp
    #define INDEX_PREV(block) [[BRPeerManager sharedInstance] blockForHash:(block.prevBlock)]
    #define INDEX_TARGET(block) block.target
    #define DIFF_SWITCHOVER(TEST, TESTA, MAIN) MAIN
    #define DIFF_ABS llabs
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
    #define DIFF_SWITCHOVER(TEST, TESTA, MAIN) GetBoolArg("-testnet", false) ? TEST : (GetBoolArg("-testnetaccel", false) ? TESTA : MAIN);
    #define DIFF_ABS std::abs
#endif

extern unsigned int GetNextWorkRequired(const INDEX_TYPE indexLast, const BLOCK_TYPE block, int64_t nPowTargetSpacing, unsigned int nPowLimit);

#endif

