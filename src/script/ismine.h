// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// File contains modifications by: The Gulden developers
// All modifications:
// Copyright (c) 2016 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#ifndef BITCOIN_SCRIPT_ISMINE_H
#define BITCOIN_SCRIPT_ISMINE_H

#include "script/standard.h"

#include <stdint.h>

class CKeyStore;
class CScript;
class CWallet;

/** IsMine() return codes */
enum isminetype {
    ISMINE_NO = 0,

    ISMINE_WATCH_UNSOLVABLE = 1,

    ISMINE_WATCH_SOLVABLE = 2,
    ISMINE_WATCH_ONLY = ISMINE_WATCH_SOLVABLE | ISMINE_WATCH_UNSOLVABLE,
    ISMINE_SPENDABLE = 4,
    ISMINE_ALL = ISMINE_WATCH_ONLY | ISMINE_SPENDABLE
};
/** used for bitflags of isminetype */
typedef uint8_t isminefilter;

isminetype IsMine(const CKeyStore& keystore, const CScript& scriptPubKey);
isminetype RemoveAddressFromKeypoolIfIsMine(CWallet& wallet, const CScript& scriptPubKey, uint64_t time);
isminetype RemoveAddressFromKeypoolIfIsMine(CWallet& wallet, const CKeyStore& keystore, const CScript& scriptPubKey, uint64_t time);

isminetype IsMine(const CKeyStore& keystore, const CTxDestination& dest);
isminetype RemoveAddressFromKeypoolIfIsMine(CWallet& wallet, const CTxDestination& dest, uint64_t time);
isminetype RemoveAddressFromKeypoolIfIsMine(CWallet& wallet, const CKeyStore& keystore, const CTxDestination& dest, uint64_t time);

isminetype IsMine(const CKeyStore& keystore, const CTxOut& txout);
isminetype RemoveAddressFromKeypoolIfIsMine(CWallet& wallet, const CTxOut& txout, uint64_t time);
isminetype RemoveAddressFromKeypoolIfIsMine(CWallet& wallet, const CKeyStore& keystore, const CTxOut& txout, uint64_t time);

#endif // BITCOIN_SCRIPT_ISMINE_H
