// Copyright (c) 2015 The Gulden developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// This file replaces the old Difficulty Re-adjustment algorithms
// It returns a ZERO DIFF when out of range, which can be catched as
// error after the call (resulting diff > params.powLimit)

#ifndef GULDEN_DIFF_OLD_H
#define GULDEN_DIFF_OLD_H

extern unsigned int diff_old(int nHeight);

#endif
