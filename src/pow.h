// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// File contains modifications by: The Gulden developers
// All modifications:
// Copyright (c) 2016-2019 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#ifndef GULDEN_POW_H
#define GULDEN_POW_H

#include "consensus/params.h"
#include "crypto/hash/sigma/sigma.h"

#include <stdint.h>

class CBlock;
class CBlockIndex;
class uint256;
/*GULDEN - our own version of this software from elsewhere in the source is used
unsigned int GetNextWorkRequired(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params&);
unsigned int CalculateNextWorkRequired(const CBlockIndex* pindexLast, int64_t nFirstBlockTime, const Consensus::Params&);
*/

/** Check whether a block hash satisfies the proof-of-work requirement specified by nBits */
bool CheckProofOfWork(const CBlock* block, const Consensus::Params& params);

extern uint64_t verifyFactor;

#endif // GULDEN_POW_H
