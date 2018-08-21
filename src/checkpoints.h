// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef GULDEN_CHECKPOINTS_H
#define GULDEN_CHECKPOINTS_H

#include "chainparams.h"

class CBlockIndex;

/**
 * Block-chain checkpoints are compiled-in sanity checks.
 * They are updated every release or three.
 */
namespace Checkpoints
{

//! Returns last CBlockIndex* in mapBlockIndex that is a checkpoint in Params()
CBlockIndex* GetLastCheckpointIndex();

//! Height of last checkpoint in Params()
int LastCheckPointHeight();

/** Last checkpoint with timestamp before or at atTime.
    Returns height of the checkpoint, or -1 iof there is none. */
int LastCheckpointAt(int64_t atTime, CheckPointEntry& entry);

} //namespace Checkpoints

#endif // GULDEN_CHECKPOINTS_H
