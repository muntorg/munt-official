// Copyright (c) 2012 Pieter Wuille
// Copyright (c) 2012-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_ADDRMAN_H
#define BITCOIN_ADDRMAN_H

#include "netbase.h"
#include "protocol.h"
#include "random.h"
#include "sync.h"
#include "timedata.h"
#include "util.h"

#include <map>
#include <set>
#include <stdint.h>
#include <vector>

/**
 * Extended statistics about a CAddress
 */
class CAddrInfo : public CAddress {

public:
    int64_t nLastTry;

    int64_t nLastCountAttempt;

private:
    CNetAddr source;

    int64_t nLastSuccess;

    int nAttempts;

    int nRefCount;

    bool fInTried;

    int nRandomPos;

    friend class CAddrMan;

public:
    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
    {
        READWRITE(*(CAddress*)this);
        READWRITE(source);
        READWRITE(nLastSuccess);
        READWRITE(nAttempts);
    }

    void Init()
    {
        nLastSuccess = 0;
        nLastTry = 0;
        nLastCountAttempt = 0;
        nAttempts = 0;
        nRefCount = 0;
        fInTried = false;
        nRandomPos = -1;
    }

    CAddrInfo(const CAddress& addrIn, const CNetAddr& addrSource)
        : CAddress(addrIn)
        , source(addrSource)
    {
        Init();
    }

    CAddrInfo()
        : CAddress()
        , source()
    {
        Init();
    }

    int GetTriedBucket(const uint256& nKey) const;

    int GetNewBucket(const uint256& nKey, const CNetAddr& src) const;

    int GetNewBucket(const uint256& nKey) const
    {
        return GetNewBucket(nKey, source);
    }

    int GetBucketPosition(const uint256& nKey, bool fNew, int nBucket) const;

    bool IsTerrible(int64_t nNow = GetAdjustedTime()) const;

    double GetChance(int64_t nNow = GetAdjustedTime()) const;
};

/** Stochastic address manager
 *
 * Design goals:
 *  * Keep the address tables in-memory, and asynchronously dump the entire table to peers.dat.
 *  * Make sure no (localized) attacker can fill the entire table with his nodes/addresses.
 *
 * To that end:
 *  * Addresses are organized into buckets.
 *    * Addresses that have not yet been tried go into 1024 "new" buckets.
 *      * Based on the address range (/16 for IPv4) of the source of information, 64 buckets are selected at random.
 *      * The actual bucket is chosen from one of these, based on the range in which the address itself is located.
 *      * One single address can occur in up to 8 different buckets to increase selection chances for addresses that
 *        are seen frequently. The chance for increasing this multiplicity decreases exponentially.
 *      * When adding a new address to a full bucket, a randomly chosen entry (with a bias favoring less recently seen
 *        ones) is removed from it first.
 *    * Addresses of nodes that are known to be accessible go into 256 "tried" buckets.
 *      * Each address range selects at random 8 of these buckets.
 *      * The actual bucket is chosen from one of these, based on the full address.
 *      * When adding a new good address to a full bucket, a randomly chosen entry (with a bias favoring less recently
 *        tried ones) is evicted from it, back to the "new" buckets.
 *    * Bucket selection is based on cryptographic hashing, using a randomly-generated 256-bit key, which should not
 *      be observable by adversaries.
 *    * Several indexes are kept for high performance. Defining DEBUG_ADDRMAN will introduce frequent (and expensive)
 *      consistency checks for the entire data structure.
 */

#define ADDRMAN_TRIED_BUCKET_COUNT 256

#define ADDRMAN_NEW_BUCKET_COUNT 1024

#define ADDRMAN_BUCKET_SIZE 64

#define ADDRMAN_TRIED_BUCKETS_PER_GROUP 8

#define ADDRMAN_NEW_BUCKETS_PER_SOURCE_GROUP 64

#define ADDRMAN_NEW_BUCKETS_PER_ADDRESS 8

#define ADDRMAN_HORIZON_DAYS 30

#define ADDRMAN_RETRIES 3

#define ADDRMAN_MAX_FAILURES 10

#define ADDRMAN_MIN_FAIL_DAYS 7

#define ADDRMAN_GETADDR_MAX_PCT 23

#define ADDRMAN_GETADDR_MAX 2500

/** 
 * Stochastical (IP) address manager 
 */
class CAddrMan {
private:
    mutable CCriticalSection cs;

    int nIdCount;

    std::map<int, CAddrInfo> mapInfo;

    std::map<CNetAddr, int> mapAddr;

    std::vector<int> vRandom;

    int nTried;

    int vvTried[ADDRMAN_TRIED_BUCKET_COUNT][ADDRMAN_BUCKET_SIZE];

    int nNew;

    int vvNew[ADDRMAN_NEW_BUCKET_COUNT][ADDRMAN_BUCKET_SIZE];

    int64_t nLastGood;

protected:
    uint256 nKey;

    CAddrInfo* Find(const CNetAddr& addr, int* pnId = NULL);

    CAddrInfo* Create(const CAddress& addr, const CNetAddr& addrSource, int* pnId = NULL);

    void SwapRandom(unsigned int nRandomPos1, unsigned int nRandomPos2);

    void MakeTried(CAddrInfo& info, int nId);

    void Delete(int nId);

    void ClearNew(int nUBucket, int nUBucketPos);

    void Good_(const CService& addr, int64_t nTime);

    bool Add_(const CAddress& addr, const CNetAddr& source, int64_t nTimePenalty);

    void Attempt_(const CService& addr, bool fCountFailure, int64_t nTime);

    CAddrInfo Select_(bool newOnly);

    virtual int RandomInt(int nMax);

#ifdef DEBUG_ADDRMAN

    int Check_();
#endif

    void GetAddr_(std::vector<CAddress>& vAddr);

    void Connected_(const CService& addr, int64_t nTime);

    void SetServices_(const CService& addr, ServiceFlags nServices);

public:
    /**
     * serialized format:
     * * version byte (currently 1)
     * * 0x20 + nKey (serialized as if it were a vector, for backward compatibility)
     * * nNew
     * * nTried
     * * number of "new" buckets XOR 2**30
     * * all nNew addrinfos in vvNew
     * * all nTried addrinfos in vvTried
     * * for each bucket:
     *   * number of elements
     *   * for each element: index
     *
     * 2**30 is xorred with the number of buckets to make addrman deserializer v0 detect it
     * as incompatible. This is necessary because it did not check the version number on
     * deserialization.
     *
     * Notice that vvTried, mapAddr and vVector are never encoded explicitly;
     * they are instead reconstructed from the other information.
     *
     * vvNew is serialized, but only used if ADDRMAN_UNKNOWN_BUCKET_COUNT didn't change,
     * otherwise it is reconstructed as well.
     *
     * This format is more complex, but significantly smaller (at most 1.5 MiB), and supports
     * changes to the ADDRMAN_ parameters without breaking the on-disk structure.
     *
     * We don't use ADD_SERIALIZE_METHODS since the serialization and deserialization code has
     * very little in common.
     */
    template <typename Stream>
    void Serialize(Stream& s, int nType, int nVersionDummy) const
    {
        LOCK(cs);

        unsigned char nVersion = 1;
        s << nVersion;
        s << ((unsigned char)32);
        s << nKey;
        s << nNew;
        s << nTried;

        int nUBuckets = ADDRMAN_NEW_BUCKET_COUNT ^ (1 << 30);
        s << nUBuckets;
        std::map<int, int> mapUnkIds;
        int nIds = 0;
        for (std::map<int, CAddrInfo>::const_iterator it = mapInfo.begin(); it != mapInfo.end(); it++) {
            mapUnkIds[(*it).first] = nIds;
            const CAddrInfo& info = (*it).second;
            if (info.nRefCount) {
                assert(nIds != nNew); // this means nNew was wrong, oh ow
                s << info;
                nIds++;
            }
        }
        nIds = 0;
        for (std::map<int, CAddrInfo>::const_iterator it = mapInfo.begin(); it != mapInfo.end(); it++) {
            const CAddrInfo& info = (*it).second;
            if (info.fInTried) {
                assert(nIds != nTried); // this means nTried was wrong, oh ow
                s << info;
                nIds++;
            }
        }
        for (int bucket = 0; bucket < ADDRMAN_NEW_BUCKET_COUNT; bucket++) {
            int nSize = 0;
            for (int i = 0; i < ADDRMAN_BUCKET_SIZE; i++) {
                if (vvNew[bucket][i] != -1)
                    nSize++;
            }
            s << nSize;
            for (int i = 0; i < ADDRMAN_BUCKET_SIZE; i++) {
                if (vvNew[bucket][i] != -1) {
                    int nIndex = mapUnkIds[vvNew[bucket][i]];
                    s << nIndex;
                }
            }
        }
    }

    template <typename Stream>
    void Unserialize(Stream& s, int nType, int nVersionDummy)
    {
        LOCK(cs);

        Clear();

        unsigned char nVersion;
        s >> nVersion;
        unsigned char nKeySize;
        s >> nKeySize;
        if (nKeySize != 32)
            throw std::ios_base::failure("Incorrect keysize in addrman deserialization");
        s >> nKey;
        s >> nNew;
        s >> nTried;
        int nUBuckets = 0;
        s >> nUBuckets;
        if (nVersion != 0) {
            nUBuckets ^= (1 << 30);
        }

        if (nNew > ADDRMAN_NEW_BUCKET_COUNT * ADDRMAN_BUCKET_SIZE) {
            throw std::ios_base::failure("Corrupt CAddrMan serialization, nNew exceeds limit.");
        }

        if (nTried > ADDRMAN_TRIED_BUCKET_COUNT * ADDRMAN_BUCKET_SIZE) {
            throw std::ios_base::failure("Corrupt CAddrMan serialization, nTried exceeds limit.");
        }

        for (int n = 0; n < nNew; n++) {
            CAddrInfo& info = mapInfo[n];
            s >> info;
            mapAddr[info] = n;
            info.nRandomPos = vRandom.size();
            vRandom.push_back(n);
            if (nVersion != 1 || nUBuckets != ADDRMAN_NEW_BUCKET_COUNT) {

                int nUBucket = info.GetNewBucket(nKey);
                int nUBucketPos = info.GetBucketPosition(nKey, true, nUBucket);
                if (vvNew[nUBucket][nUBucketPos] == -1) {
                    vvNew[nUBucket][nUBucketPos] = n;
                    info.nRefCount++;
                }
            }
        }
        nIdCount = nNew;

        int nLost = 0;
        for (int n = 0; n < nTried; n++) {
            CAddrInfo info;
            s >> info;
            int nKBucket = info.GetTriedBucket(nKey);
            int nKBucketPos = info.GetBucketPosition(nKey, false, nKBucket);
            if (vvTried[nKBucket][nKBucketPos] == -1) {
                info.nRandomPos = vRandom.size();
                info.fInTried = true;
                vRandom.push_back(nIdCount);
                mapInfo[nIdCount] = info;
                mapAddr[info] = nIdCount;
                vvTried[nKBucket][nKBucketPos] = nIdCount;
                nIdCount++;
            } else {
                nLost++;
            }
        }
        nTried -= nLost;

        for (int bucket = 0; bucket < nUBuckets; bucket++) {
            int nSize = 0;
            s >> nSize;
            for (int n = 0; n < nSize; n++) {
                int nIndex = 0;
                s >> nIndex;
                if (nIndex >= 0 && nIndex < nNew) {
                    CAddrInfo& info = mapInfo[nIndex];
                    int nUBucketPos = info.GetBucketPosition(nKey, true, bucket);
                    if (nVersion == 1 && nUBuckets == ADDRMAN_NEW_BUCKET_COUNT && vvNew[bucket][nUBucketPos] == -1 && info.nRefCount < ADDRMAN_NEW_BUCKETS_PER_ADDRESS) {
                        info.nRefCount++;
                        vvNew[bucket][nUBucketPos] = nIndex;
                    }
                }
            }
        }

        int nLostUnk = 0;
        for (std::map<int, CAddrInfo>::const_iterator it = mapInfo.begin(); it != mapInfo.end();) {
            if (it->second.fInTried == false && it->second.nRefCount == 0) {
                std::map<int, CAddrInfo>::const_iterator itCopy = it++;
                Delete(itCopy->first);
                nLostUnk++;
            } else {
                it++;
            }
        }
        if (nLost + nLostUnk > 0) {
            LogPrint("addrman", "addrman lost %i new and %i tried addresses due to collisions\n", nLostUnk, nLost);
        }

        Check();
    }

    unsigned int GetSerializeSize(int nType, int nVersion) const
    {
        return (CSizeComputer(nType, nVersion) << *this).size();
    }

    void Clear()
    {
        std::vector<int>().swap(vRandom);
        nKey = GetRandHash();
        for (size_t bucket = 0; bucket < ADDRMAN_NEW_BUCKET_COUNT; bucket++) {
            for (size_t entry = 0; entry < ADDRMAN_BUCKET_SIZE; entry++) {
                vvNew[bucket][entry] = -1;
            }
        }
        for (size_t bucket = 0; bucket < ADDRMAN_TRIED_BUCKET_COUNT; bucket++) {
            for (size_t entry = 0; entry < ADDRMAN_BUCKET_SIZE; entry++) {
                vvTried[bucket][entry] = -1;
            }
        }

        nIdCount = 0;
        nTried = 0;
        nNew = 0;
        nLastGood = 1; //Initially at 1 so that "never" is strictly worse.
    }

    CAddrMan()
    {
        Clear();
    }

    ~CAddrMan()
    {
        nKey.SetNull();
    }

    size_t size() const
    {
        return vRandom.size();
    }

    void Check()
    {
#ifdef DEBUG_ADDRMAN
        {
            LOCK(cs);
            int err;
            if ((err = Check_()))
                LogPrintf("ADDRMAN CONSISTENCY CHECK FAILED!!! err=%i\n", err);
        }
#endif
    }

    bool Add(const CAddress& addr, const CNetAddr& source, int64_t nTimePenalty = 0)
    {
        bool fRet = false;
        {
            LOCK(cs);
            Check();
            fRet |= Add_(addr, source, nTimePenalty);
            Check();
        }
        if (fRet)
            LogPrint("addrman", "Added %s from %s: %i tried, %i new\n", addr.ToStringIPPort(), source.ToString(), nTried, nNew);
        return fRet;
    }

    bool Add(const std::vector<CAddress>& vAddr, const CNetAddr& source, int64_t nTimePenalty = 0)
    {
        int nAdd = 0;
        {
            LOCK(cs);
            Check();
            for (std::vector<CAddress>::const_iterator it = vAddr.begin(); it != vAddr.end(); it++)
                nAdd += Add_(*it, source, nTimePenalty) ? 1 : 0;
            Check();
        }
        if (nAdd)
            LogPrint("addrman", "Added %i addresses from %s: %i tried, %i new\n", nAdd, source.ToString(), nTried, nNew);
        return nAdd > 0;
    }

    void Good(const CService& addr, int64_t nTime = GetAdjustedTime())
    {
        {
            LOCK(cs);
            Check();
            Good_(addr, nTime);
            Check();
        }
    }

    void Attempt(const CService& addr, bool fCountFailure, int64_t nTime = GetAdjustedTime())
    {
        {
            LOCK(cs);
            Check();
            Attempt_(addr, fCountFailure, nTime);
            Check();
        }
    }

    /**
     * Choose an address to connect to.
     */
    CAddrInfo Select(bool newOnly = false)
    {
        CAddrInfo addrRet;
        {
            LOCK(cs);
            Check();
            addrRet = Select_(newOnly);
            Check();
        }
        return addrRet;
    }

    std::vector<CAddress> GetAddr()
    {
        Check();
        std::vector<CAddress> vAddr;
        {
            LOCK(cs);
            GetAddr_(vAddr);
        }
        Check();
        return vAddr;
    }

    void Connected(const CService& addr, int64_t nTime = GetAdjustedTime())
    {
        {
            LOCK(cs);
            Check();
            Connected_(addr, nTime);
            Check();
        }
    }

    void SetServices(const CService& addr, ServiceFlags nServices)
    {
        LOCK(cs);
        Check();
        SetServices_(addr, nServices);
        Check();
    }
};

#endif // BITCOIN_ADDRMAN_H
