// Copyright (c) 2015-2022 The Centure developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the Libre Chain License, see the accompanying
// file COPYING
//
// This file replaces the old Difficulty Re-adjustment algorithms
// It returns a ZERO DIFF when out of range, which can be catched as
// error after the call (resulting diff > params.powLimit)

#ifndef POW_DIFF_OLD_H
#define POW_DIFF_OLD_H

extern unsigned int diff_old(int nHeight, unsigned int nPowLimit);

#endif
