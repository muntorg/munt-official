#ifndef GULDENCOIN_HASH_H
#define GULDENCOIN_HASH_H

#include "../hash.h"
#include "../uint256.h"
#include "citycrc.h"
#include "../scrypt.h"

inline void hash_sha256(const void* pData, arith_uint256& thash)
{
    CSHA256 sha;
    sha.Write((unsigned char*)pData, 80);
    sha.Finalize((unsigned char*)&thash);
}

inline void hash_city(const void* pData, arith_uint256& thash)
{
    //For testing purposes
    uint128 temphash =  CityHash128((char*)pData, 80);
    thash |= temphash.first;
    thash <<= 64;
    thash |= temphash.second;
    thash <<= 64;
    //temphash = CityHash128((char*)&thash, 80);
    //thash |= temphash.first;
    //thash <<= 64;
    //thash |= temphash.second;
    thash <<= 64;
}


#endif