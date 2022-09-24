// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// File contains modifications by: The Centure developers
// All modifications:
// Copyright (c) 2016-2022 The Centure developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the Libre Chain License, see the accompanying
// file COPYING

#ifndef POW_POW_H
#define POW_POW_H

#include "consensus/params.h"
#include "crypto/hash/sigma/sigma.h"

#include <stdint.h>

class CBlock;
class CBlockIndex;
class uint256;

/** Check whether a block hash satisfies the proof-of-work requirement specified by nBits */
bool CheckProofOfWork(const CBlock* block, const Consensus::Params& params);

extern uint64_t verifyFactor;

#endif
