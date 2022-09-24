// Copyright (c) 2015-2022 The Centure developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the Libre Chain License, see the accompanying
// file COPYING
//
// This file contains Delta, the Gulden Difficulty Re-adjustment algorithm developed by Frank (dt_cdog@yahoo.com) with various enhancements by Malcolm MacLeod (mmacleod@gmx.com)
// The core algorithm works by taking time measurements of four periods (last block; short window; medium window; long window) and then apply a weighting to them.
// This allows the algorithm to react to short term fluctuations while still taking long term block targets into account, which helps prevent it from overreacting.
//
// In addition to the core algorithm several extra rules are then applied in certain situations (e.g. multiple quick blocks) to enhance the behaviour.


#ifndef POW_DIFF_DELTA_H
#define POW_DIFF_DELTA_H

#define PERCENT_FACTOR 100
extern unsigned int GetNextWorkRequired_DELTA (const CBlockIndex* pindexLast, const CBlockHeader* block, int nPowTargetSpacing, unsigned int nPowLimit, unsigned int nFirstDeltaBlock);

#endif
