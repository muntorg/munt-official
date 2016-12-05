// Copyright (c) 2015-2016 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING
//
// This file contains Delta, the Gulden Difficulty Re-adjustment algorithm developed by Frank (dt_cdog@yahoo.com) with various enhancements by Malcolm MacLeod (mmacleod@webmail.co.za)
// The core algorithm works by taking time measurements of four periods (last block; short window; medium window; long window) and then apply a weighting to them.
// This allows the algorithm to react to short term fluctuations while still taking long term block targets into account, which helps prevent it from overreacting.
//
// In addition to the core algorithm several extra rules are then applied in certain situations (e.g. multiple quick blocks) to enhance the behaviour.

#ifndef GULDEN_DIFF_DELTA_H
#define GULDEN_DIFF_DELTA_H

#define PERCENT_FACTOR 100

#ifndef __JAVA__
extern unsigned int GetNextWorkRequired_DELTA(const INDEX_TYPE pindexLast, const BLOCK_TYPE block, int nPowTargetSpacing, unsigned int nPowLimit, unsigned int nFirstDeltaBlock);
#endif

#endif
