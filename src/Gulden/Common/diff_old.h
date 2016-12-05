// Copyright (c) 2015-2016 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING
//
// This file replaces the old Difficulty Re-adjustment algorithms
// It returns a ZERO DIFF when out of range, which can be catched as
// error after the call (resulting diff > params.powLimit)

#ifndef __JAVA__
#ifndef GULDEN_DIFF_OLD_H
#define GULDEN_DIFF_OLD_H

extern unsigned int diff_old(int nHeight, unsigned int nPowLimit);

#endif
#endif
