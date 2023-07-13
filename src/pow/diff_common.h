// Copyright (c) 2015-2022 The Centure developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the GNU Lesser General Public License v3, see the accompanying
// file COPYING


#ifndef POW_DIFF_COMMON_H
#define POW_DIFF_COMMON_H

#include <consensus/params.h>
#include <arith_uint256.h>
#include <chain.h>
#include <sync.h>
#include <util.h>
#include <stdint.h>

#define BLOCK_TIME(block) block->nTime
#define INDEX_TIME(block) block->GetBlockTime()
#define BIGINT_MULTIPLY(x, y) x * y
#define BIGINT_DIVIDE(x, y) x / y

#define DIFF_SWITCHOVER(TEST, MAIN) (Params().IsTestnet() ? TEST :  MAIN)
extern unsigned int GetNextWorkRequired(const CBlockIndex* indexLast, const CBlockHeader* block, unsigned int nPowTargetSpacing, unsigned int nPowLimit);

#endif
