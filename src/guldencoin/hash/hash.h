// Copyright (c) 2015 The Guldencoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef GULDENCOIN_HASH_H
#define GULDENCOIN_HASH_H

#include "../hash.h"
#include "../uint256.h"
#include "../scrypt.h"
#include "city.h"

inline void hash_sha256(const void* pData, arith_uint256& thash)
{
    CSHA256 sha;
    sha.Write((unsigned char*)pData, 80);
    sha.Finalize((unsigned char*)&thash);
}

inline void hash_city(const void* pData, arith_uint256& thash)
{
    // For testing purposes only - i.e. designed to be fast not secure/good.
    // Difficulty will never be high enough on testnet that the last 128 its become significant so we cheat and don't generate them at all.
    uint64_t temphash =  CityHash64((char*)pData, 80);
    thash |= temphash;
    thash <<= 64;
    thash |= temphash;
    thash <<= 64;
    thash <<= 64;
}


#endif