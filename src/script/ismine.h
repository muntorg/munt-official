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

#ifndef SCRIPT_ISMINE_H
#define SCRIPT_ISMINE_H

#include "script/standard.h"

#include <stdint.h>

class CKeyStore;
class CScript;
class CWallet;

/** IsMine() return codes */
enum isminetype : uint_fast8_t
{
    ISMINE_NO = 0,
    //! Indicates that we don't know how to create a scriptSig that would solve this if we were given the appropriate private keys
    ISMINE_WATCH_UNSOLVABLE = 1,
    //! Indicates that we know how to create a scriptSig that would solve this if we were given the appropriate private keys
    ISMINE_WATCH_SOLVABLE = 2,
    ISMINE_WATCH_ONLY = ISMINE_WATCH_SOLVABLE | ISMINE_WATCH_UNSOLVABLE,
    //fixme: (PHASE5) - We should assign a different value to ISMINE_WITNESS than ISMINE_SPENDABLE
    //However this will require carefully going through every case in the code that deals with "ISMINE_" values to ensure its handled right.
    ISMINE_WITNESS = 4,
    ISMINE_SPENDABLE = 4,
    ISMINE_ALL = ISMINE_WATCH_ONLY | ISMINE_SPENDABLE
};
/** used for bitflags of isminetype */
typedef uint8_t isminefilter;

/* isInvalid becomes true when the script is found invalid by consensus or policy. This will terminate the recursion
 * and return a ISMINE_NO immediately, as an invalid script should never be considered as "mine". This is needed as
 * different SIGVERSION may have different network rules. Currently the only use of isInvalid is indicate uncompressed
 * keys in SIGVERSION_SEGSIG script, but could also be used in similar cases in the future
 */
isminetype IsMine(const CKeyStore& keystore, const CScript& scriptPubKey, bool& isInvalid, SigVersion = SIGVERSION_BASE);
isminetype IsMine(const CKeyStore& keystore, const CScript& scriptPubKey, SigVersion = SIGVERSION_BASE);
isminetype IsMine(const CKeyStore& keystore, const CTxDestination& dest, bool& isInvalid, SigVersion = SIGVERSION_BASE);
isminetype IsMine(const CKeyStore& keystore, const CTxDestination& dest, SigVersion = SIGVERSION_BASE);
isminetype IsMine(const CKeyStore& keystore, const CTxOut& txout);
isminetype IsMine(const CKeyStore& keystore, const CPoW2WitnessDestination& witnessDetails);
isminetype IsMine(const CKeyStore& keystore, const CTxOutPoW2Witness& witnessDetails);
isminetype IsMine(const CKeyStore& keystore, const CTxOutStandardKeyHash& standardKeyHash);

isminetype RemoveAddressFromKeypoolIfIsMine(CWallet& keystore, const CScript& scriptPubKey, uint64_t time, bool& isInvalid, SigVersion = SIGVERSION_BASE);
isminetype RemoveAddressFromKeypoolIfIsMine(CWallet& keystore, const CScript& scriptPubKey, uint64_t time, SigVersion = SIGVERSION_BASE);
isminetype RemoveAddressFromKeypoolIfIsMine(CWallet& keystore, const CTxOut& txout, uint64_t time);

#endif
