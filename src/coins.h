// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// File contains modifications by: The Gulden developers
// All modifications:
// Copyright (c) 2017-2020 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#ifndef COINS_H
#define COINS_H

#include "primitives/transaction.h"
#include "compressor.h"
#include "core_memusage.h"
#include "hash.h"
#include "memusage.h"
#include "serialize.h"
#include "uint256.h"

#include <assert.h>
#include <stdint.h>

#include <unordered_map>

#include <compat/sys.h>

/**
 * A UTXO entry.
 *
 * Serialized format:
 * - VARINT((height << 2) | (segsig << 1) | (coinbase))
 * - VARINT(index)
 * - the non-spent CTxOut (via CTxOutCompressor)
 */
class Coin
{
public:
    //! unspent transaction output
    CTxOut out;

    //! whether containing transaction was a coinbase
    unsigned int fCoinBase : 1;
    unsigned int fSegSig : 1;

    //! The height this containing transaction was included in the active block chain
    //NB! This limits us to ~5000 years before we need to update serialisation format - so may eventually have to change but not an issue for now.
    //MEMPOOL_HEIGHT indicates it is not yet in the chain - any adjustments to the size of this type should also adjust the value of MEMPOOL_HEIGHT
    uint32_t nHeight : 30;
    
    //! The index of the containing transaction within its parent block
    uint32_t nTxIndex;

    //! construct a Coin from a CTxOut and height/coinbase information.
    Coin(CTxOut&& outIn, uint32_t nHeightIn, uint32_t nTxIndexIn, bool fCoinBaseIn, bool fSegSigIn)
    : out(std::move(outIn))
    , fCoinBase(fCoinBaseIn)
    , fSegSig(fSegSigIn)
    , nHeight(nHeightIn)
    , nTxIndex(nTxIndexIn)
    {}
    Coin(const CTxOut& outIn, uint32_t nHeightIn, uint32_t nTxIndexIn, bool fCoinBaseIn, bool fSegSigIn)
    : out(outIn)
    , fCoinBase(fCoinBaseIn)
    , fSegSig(fSegSigIn)
    , nHeight(nHeightIn)
    , nTxIndex(nTxIndexIn)
    {}

    void Clear() {
        out.SetNull();
        fCoinBase = false;
        fSegSig = false;
        nHeight = 0;
        nTxIndex = 0;
    }

    //! empty constructor
    Coin()
    : fCoinBase(false)
    , fSegSig(false)
    , nHeight(0)
    , nTxIndex(0)
    { }

    bool IsCoinBase() const {
        return fCoinBase;
    }

    template<typename Stream>
    void Serialize(Stream &s) const {
        assert(!IsSpent());
        uint32_t code = (nHeight<<2) + (fSegSig << 1) + fCoinBase;
        ::Serialize(s, VARINT(code));
        ::Serialize(s, VARINT(nTxIndex));
        out.WriteToStream(s, (fSegSig ? CTransaction::SEGSIG_ACTIVATION_VERSION : CTransaction::SEGSIG_ACTIVATION_VERSION-1));
    }

    template<typename Stream>
    void Unserialize(Stream &s) {
        uint32_t code = 0;
        ::Unserialize(s, VARINT(code));
        nHeight = ( ((code  & 0b11111111111111111111111111111100) >> 2) );
        fSegSig = ( (code   & 0b00000000000000000000000000000010) > 0 );
        fCoinBase = ( (code & 0b00000000000000000000000000000001) > 0 );
        ::Unserialize(s, VARINT(nTxIndex));
        out.ReadFromStream(s, (fSegSig ? CTransaction::SEGSIG_ACTIVATION_VERSION : CTransaction::SEGSIG_ACTIVATION_VERSION-1));
    }

    bool IsSpent() const {
        return out.IsNull();
    }

    void Spend() {
        out.SetNull();
    }

    size_t DynamicMemoryUsage() const {
        return RecursiveDynamicUsage(out);
    }
};

class CoinUndo : public Coin
{
    public:
    uint256 prevhash;
    CoinUndo(Coin&& fromCoin, uint256 hashIn)
    : Coin(fromCoin)
    , prevhash(hashIn)
    {
    }
    
    //! empty constructor
    CoinUndo() : Coin() {}
    
    template<typename Stream>
    void Serialize(Stream &s) const
    {
        assert(!IsSpent());
        uint32_t code = (nHeight<<2) + (fSegSig << 1) + fCoinBase;
        ::Serialize(s, VARINT(code));
        ::Serialize(s, VARINT(nTxIndex));
        out.WriteToStream(s, (fSegSig ? CTransaction::SEGSIG_ACTIVATION_VERSION : CTransaction::SEGSIG_ACTIVATION_VERSION-1));
        ::Serialize(s, prevhash);
    }

    template<typename Stream>
    void Unserialize(Stream &s)
    {
        uint32_t code = 0;
        ::Unserialize(s, VARINT(code));
        nHeight = ( ((code  & 0b11111111111111111111111111111100) >> 2) );
        fSegSig = ( (code   & 0b00000000000000000000000000000010) > 0 );
        fCoinBase = ( (code & 0b00000000000000000000000000000001) > 0 );
        ::Unserialize(s, VARINT(nTxIndex));
        out.ReadFromStream(s, (fSegSig ? CTransaction::SEGSIG_ACTIVATION_VERSION : CTransaction::SEGSIG_ACTIVATION_VERSION-1));
        ::Unserialize(s, prevhash);
    }
};

class SaltedOutpointHasher
{
private:
    /** Salt */
    const uint64_t k0, k1;

public:
    SaltedOutpointHasher();

    /**
     * This *must* return size_t. With Boost 1.46 on 32-bit systems the
     * unordered_map will behave unpredictably if the custom hasher returns a
     * uint64_t, resulting in failures when syncing the chain (#4634).
     */
    size_t operator()(const COutPoint& id) const {
        return SipHashUint256Extra(k0, k1, id.getBucketHash(), id.n);
    }
};

struct CCoinsCacheEntry
{
    Coin coin; // The actual cached data.
    unsigned char flags;

    enum Flags {
        DIRTY = (1 << 0), // This cache entry is potentially different from the version in the parent view.
        FRESH = (1 << 1), // The parent view does not have this entry (or it is pruned).
        /* Note that FRESH is a performance optimization with which we can
         * erase coins that are fully spent if we know we do not need to
         * flush the changes to the parent cache.  It is always safe to
         * not mark FRESH if that condition is not guaranteed.
         */
    };

    CCoinsCacheEntry() : flags(0) {}
    explicit CCoinsCacheEntry(Coin&& coin_) : coin(std::move(coin_)), flags(0) {}
};

typedef std::unordered_map<COutPoint, CCoinsCacheEntry, SaltedOutpointHasher> CCoinsMap;
typedef std::unordered_map<COutPoint, COutPoint, SaltedOutpointHasher> CCoinsRefMap;

/** Cursor for iterating over CoinsView state */
class CCoinsViewCursor
{
public:
    CCoinsViewCursor(const uint256 &hashBlockIn): hashBlock(hashBlockIn) {}
    virtual ~CCoinsViewCursor() {}

    virtual bool GetKey(COutPoint &key) const = 0;
    virtual bool GetValue(Coin &coin) const = 0;
    virtual unsigned int GetValueSize() const = 0;

    virtual bool Valid() const = 0;
    virtual void Next() = 0;

    //! Get best block at the time this cursor was created
    const uint256 &GetBestBlock() const { return hashBlock; }
private:
    uint256 hashBlock;
};

/** Abstract view on the open txout dataset. */
class CCoinsView
{
public:
    //! Retrieve the Coin (unspent transaction output) for a given outpoint.
    virtual bool GetCoin(const COutPoint &outpoint, Coin &coin, COutPoint* pOutpointRet=nullptr) const;

    //! Just check whether we have data for a given outpoint.
    //! This may (but cannot always) return true for spent outputs.
    virtual bool HaveCoin(const COutPoint &outpoint) const;

    //! Retrieve the block hash whose state this CCoinsView currently represents
    virtual uint256 GetBestBlock() const;

    //! Do a bulk modification (multiple Coin changes + BestBlock change).
    //! The passed mapCoins can be modified.
    virtual bool BatchWrite(CCoinsMap &mapCoins, const uint256 &hashBlock);

    //! Get a cursor to iterate over the whole state
    virtual CCoinsViewCursor *Cursor() const;

    //! As we use CCoinsViews polymorphically, have a virtual destructor
    virtual ~CCoinsView() {}

    //! Estimate database size (0 if not implemented)
    virtual size_t EstimateSize() const { return 0; }

    virtual void GetAllCoins(std::map<COutPoint, Coin>&) const {};
    virtual void GetAllCoinsIndexBased(std::map<COutPoint, Coin>&) const {};
    virtual void GetAllCoinsIndexBasedDirect(std::map<COutPoint, Coin>& allCoins) const {};
    /*virtual int GetDepth() const
    {
        return 0;
    }*/
};


/** CCoinsView backed by another CCoinsView */
class CCoinsViewBacked : public CCoinsView
{
protected:
    CCoinsView *base;

public:
    CCoinsViewBacked(CCoinsView *viewIn);
    bool GetCoin(const COutPoint &outpoint, Coin &coin, COutPoint* pOutpointRet=nullptr) const override;
    bool HaveCoin(const COutPoint &outpoint) const override;
    uint256 GetBestBlock() const override;
    void SetBackend(CCoinsView &viewIn);
    bool BatchWrite(CCoinsMap &mapCoins, const uint256 &hashBlock) override;
    CCoinsViewCursor *Cursor() const override;
    size_t EstimateSize() const override;
    /*int GetDepth() const override
    {
        return base->GetDepth() + 1;
    }*/
    void GetAllCoins(std::map<COutPoint, Coin>& allCoins) const override
    {
        base->GetAllCoins(allCoins);
    }
    void GetAllCoinsIndexBased(std::map<COutPoint, Coin>& allCoins) const override
    {
        base->GetAllCoinsIndexBased(allCoins);
    }
    void GetAllCoinsIndexBasedDirect(std::map<COutPoint, Coin>& allCoins) const override
    {
        base->GetAllCoinsIndexBasedDirect(allCoins);
    }
};


/** CCoinsView that adds a memory cache for transactions to another CCoinsView */
class CCoinsViewCache : public CCoinsViewBacked
{
protected:
    /**
     * Make mutable so that we can "fill the cache" even from Get-methods
     * declared as "const".
     */
    mutable uint256 hashBlock;
    mutable CCoinsMap cacheCoins;
    mutable CCoinsRefMap cacheCoinRefs;
    mutable uint64_t cacheMempoolRefs;

    /* Cached dynamic memory usage for the inner Coin objects. */
    mutable size_t cachedCoinsUsage;

public:
    CCoinsViewCache(CCoinsViewCache *baseIn);
    CCoinsViewCache(CCoinsView *baseIn);

    // Standard CCoinsView methods
    bool GetCoin(const COutPoint &outpoint, Coin &coin, COutPoint* pOutpointRet=nullptr) const override;
    bool HaveCoin(const COutPoint &outpoint) const override;
    uint256 GetBestBlock() const override;
    void SetBestBlock(const uint256 &hashBlock);
    bool BatchWrite(CCoinsMap &mapCoins, const uint256 &hashBlock) override;
    CCoinsViewCursor* Cursor() const override {
        throw std::logic_error("CCoinsViewCache cursor iteration not supported.");
    }

    /**
     * Check if we have the given utxo already loaded in this cache.
     * The semantics are the same as HaveCoin(), but no calls to
     * the backing CCoinsView are made.
     */
    bool HaveCoinInCache(const COutPoint &outpoint) const;

    /**
     * Return a reference to Coin in the cache, or a pruned one if not found. This is
     * more efficient than GetCoin. Modifications to other cache entries are
     * allowed while accessing the returned pointer.
     */
    const Coin& AccessCoin(const COutPoint &output) const;

    /**
     * Add a coin. Set potential_overwrite to true if a non-pruned version may
     * already exist.
     */
    void AddCoin(const COutPoint& outpoint, Coin&& coin, bool potential_overwrite);

    /**
     * Spend a coin. Pass moveto in order to get the deleted data.
     * If no unspent output exists for the passed outpoint, this call
     * has no effect.
     */
    void SpendCoin(const COutPoint &outpoint, CoinUndo* moveto = nullptr, bool nodeletefresh = false);

    /**
     * Push the modifications applied to this cache to its base.
     * Failure to call this method before destruction will cause the changes to be forgotten.
     * If false is returned, the state of this cache (and its backing view) will be undefined.
     */
    bool Flush();

    /**
     * Removes the UTXO with the given outpoint from the cache, if it is
     * not modified.
     */
    void Uncache(const COutPoint &outpoint);

    //! Calculate the size of the cache (in number of transaction outputs)
    unsigned int GetCacheSize() const;

    //! Calculate the size of the cache (in bytes)
    size_t DynamicMemoryUsage() const;

    /** 
     * Amount of coins coming in to a transaction
     * Note that lightweight clients may not know anything besides the hash of previous transactions,
     * so may not be able to calculate this.
     *
     * @param[in] tx	transaction for which we are checking input total
     * @return	Sum of value of all inputs (scriptSigs)
     */
    CAmount GetValueIn(const CTransaction& tx) const;

    //! Check whether all prevouts of the transaction are present in the UTXO set represented by this view
    bool HaveInputs(const CTransaction& tx) const;

    // Side view
    void SetSiblingView(std::shared_ptr<CCoinsViewCache> pChainedWitView_) { pChainedWitView = pChainedWitView_; };
    std::shared_ptr<CCoinsViewCache> pChainedWitView;

    /*int GetDepth() const override
    {
        return base->GetDepth() + 1;
    }*/

    void GetAllCoins(std::map<COutPoint, Coin>& allCoins) const override
    {
        base->GetAllCoins(allCoins);

        for (auto iter : cacheCoins)
        {
            if (iter.second.coin.out.IsNull())
            {
                if (allCoins.find(iter.first) != allCoins.end())
                {
                    allCoins.erase(iter.first);
                }
            }
            else
            {
                allCoins[iter.first] = iter.second.coin;
            }
        }
    }
    
    void GetAllCoinsIndexBased(std::map<COutPoint, Coin>& allCoinsIndexBased) const override
    {
        std::map<COutPoint, Coin> allCoins;
        GetAllCoins(allCoins);
        
        for (auto& [outPoint, coin] : allCoins)
        {
            COutPoint indexBased(coin.nHeight, coin.nTxIndex, outPoint.n);
            allCoinsIndexBased[indexBased] = coin;
        }
    }
    
    //fixme: (FUT) We keep this form around but don't use it
    // If/When we can prove beyond a shadow of a doubt that it introduces no problems and that it really is faster consider switching to it, otherwise we should delete and stick to the simpler implementation
    void GetAllCoinsIndexBasedDirect(std::map<COutPoint, Coin>& allCoinsIndexBased) const override
    {
        for (auto iter : cacheCoins)
        {
            COutPoint indexBased(iter.second.coin.nHeight, iter.second.coin.nTxIndex, iter.first.n);
            if (!iter.second.coin.out.IsNull())
            {
                allCoinsIndexBased[indexBased] = iter.second.coin;
            }
        }
    }

private:
    CCoinsMap::iterator FetchCoin(const COutPoint &outpoint, CCoinsRefMap::iterator* pRefIterReturn=nullptr) const;

    #if defined(DEBUG) && !defined(PLATFORM_MOBILE)
    #define DEBUG_COINSCACHE_VALIDATE_INSERTS
    void validateInsert(const COutPoint &outpoint, uint64_t block, uint64_t txIndex, uint32_t voutIndex) const;
    #else
    void validateInsert(const COutPoint &outpoint, uint64_t block, uint64_t txIndex, uint32_t voutIndex) const {};
    #endif

    /**
     * By making the copy constructor private, we prevent accidentally using it when one intends to create a cache on top of a base cache.
     */
    CCoinsViewCache(const CCoinsViewCache &);
};

//! Utility function to add all of a transaction's outputs to a cache.
// It assumes that overwrites are only possible for coinbase transactions,
// TODO: pass in a boolean to limit these possible overwrites to known
// (pre-BIP34) cases.
void AddCoins(CCoinsViewCache& cache, const CTransaction& tx, uint32_t nHeight, uint32_t nTxIndex);

//! Utility function to find any unspent output with a given txid.
const Coin& AccessByTxid(const CCoinsViewCache& cache, const uint256& txid);

#endif
