// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// File contains modifications by: The Centure developers
// All modifications:
// Copyright (c) 2016-2022 The Centure developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the GNU Lesser General Public License v3, see the accompanying
// file COPYING

#ifndef AMOUNT_H
#define AMOUNT_H

#include <stdint.h>

/** Amount in satoshis (Can be negative) */
typedef int64_t CAmount;

static const CAmount COIN          = 100000000;
static const CAmount DECI          =  10000000;
static const CAmount CENT          =   1000000;
static const CAmount DECICENT      =    100000;
static const CAmount CENTICENT     =     10000;
static const CAmount MILLICENT     =      1000;
static const CAmount MICROCOIN     =       100;


/** No amount larger than this (in satoshi) is valid.
 *
 * Note that this constant is *not* the total money supply, but
 * rather a sanity check. As this sanity check is used by consensus-critical
 * validation code, the exact value of the MAX_MONEY constant is consensus
 * critical; in unusual circumstances like a(nother) overflow bug that allowed
 * for the creation of coins out of thin air modification could lead to a fork.
 * */
//fixme: (PHASE5) Relook at this constant - I don't think the value here is actually right/ideal.
static const CAmount MAX_MONEY = 1680000000 * COIN;
inline bool MoneyRange(const CAmount& nValue) { return (nValue >= 0 && nValue <= MAX_MONEY); }

#endif
