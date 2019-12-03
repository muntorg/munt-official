// Copyright (c) 2012-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// File contains modifications by: The Gulden developers
// All modifications:
// Copyright (c) 2017-2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#include "coins.h"

#include "consensus/consensus.h"
#include "memusage.h"
#include "random.h"

#include <assert.h>

// Gulden specific includes
#include "Gulden/util.h"

bool CCoinsView::GetCoin(const COutPoint &outpoint, Coin &coin, COutPoint* pOutpointRet) const { return false; }
bool CCoinsView::HaveCoin(const COutPoint &outpoint) const { return false; }
uint256 CCoinsView::GetBestBlock() const { return uint256(); }
bool CCoinsView::BatchWrite(CCoinsMap &mapCoins, const uint256 &hashBlock) { return false; }
CCoinsViewCursor *CCoinsView::Cursor() const { return 0; }


CCoinsViewBacked::CCoinsViewBacked(CCoinsView *viewIn) : base(viewIn) { }
bool CCoinsViewBacked::GetCoin(const COutPoint &outpoint, Coin &coin, COutPoint* pOutpointRet) const { return base->GetCoin(outpoint, coin, pOutpointRet); }
bool CCoinsViewBacked::HaveCoin(const COutPoint &outpoint) const { return base->HaveCoin(outpoint); }
uint256 CCoinsViewBacked::GetBestBlock() const { return base->GetBestBlock(); }
void CCoinsViewBacked::SetBackend(CCoinsView &viewIn) { base = &viewIn; }
bool CCoinsViewBacked::BatchWrite(CCoinsMap &mapCoins, const uint256 &hashBlock) { return base->BatchWrite(mapCoins, hashBlock); }
CCoinsViewCursor *CCoinsViewBacked::Cursor() const { return base->Cursor(); }
size_t CCoinsViewBacked::EstimateSize() const { return base->EstimateSize(); }

SaltedOutpointHasher::SaltedOutpointHasher() : k0(GetRand(std::numeric_limits<uint64_t>::max())), k1(GetRand(std::numeric_limits<uint64_t>::max())) {}

CCoinsViewCache::CCoinsViewCache(CCoinsView *baseIn) : CCoinsViewBacked(baseIn), cachedCoinsUsage(0), pChainedWitView(nullptr) {}
CCoinsViewCache::CCoinsViewCache(CCoinsViewCache *baseIn) : CCoinsViewBacked(baseIn), cachedCoinsUsage(0), pChainedWitView(baseIn->pChainedWitView?std::shared_ptr<CCoinsViewCache>(new CCoinsViewCache(baseIn->pChainedWitView.get())):nullptr) {}

size_t CCoinsViewCache::DynamicMemoryUsage() const
{
    return memusage::DynamicUsage(cacheCoins) + cachedCoinsUsage;
}

#ifdef DEBUG
void CCoinsViewCache::validateInsert(const COutPoint &outpoint, uint64_t block, uint64_t txIndex, uint32_t voutIndex) const
{
    // check args
    assert(outpoint.isHash || (outpoint.getTransactionBlockNumber() == block && outpoint.getTransactionIndex() == txIndex));
    assert(outpoint.n == voutIndex);

    // cacheCoins and cacheCoinRefs keep a 1:1 correspondence, so any difference in size is a bug for sure
    assert(cacheCoins.size() == cacheCoinRefs.size());

    CCoinsRefMap::iterator refIt = cacheCoinRefs.find(outpoint.isHash ? COutPoint(block, txIndex, voutIndex) : outpoint);
    if (refIt != cacheCoinRefs.end()) {
        // entry present in cacheCoinRefs
        const COutPoint& canonicalOutPoint = refIt->second;
        CCoinsMap::iterator it = cacheCoins.find(canonicalOutPoint);
        assert(it != cacheCoins.end());  // verify it is present in cacheCoins as well
        const Coin& coin = it->second.coin;

        // and that its properties are matching
        assert(!canonicalOutPoint.isHash || canonicalOutPoint.getHash() == outpoint.getHash());
        assert(canonicalOutPoint.isHash || (canonicalOutPoint.getTransactionBlockNumber() == block && canonicalOutPoint.getTransactionIndex() == txIndex));
        assert(canonicalOutPoint.n == voutIndex);
        assert(coin.nHeight == block);
        assert(coin.nTxIndex == txIndex);
    }
    else
    {
        // no entry in cacheCoinRefs, so it should be absent in cacheCoins also
        assert(!outpoint.isHash || cacheCoins.find(outpoint) == cacheCoins.end());
        assert(outpoint.isHash || cacheCoins.find(COutPoint(block, txIndex, voutIndex)) == cacheCoins.end());
    }
}
#endif

CCoinsMap::iterator CCoinsViewCache::FetchCoin(const COutPoint &outpoint, CCoinsRefMap::iterator* pRefIterReturn) const
{
    // lookup outpoint in either map and return iterators to both maps for it
    if (outpoint.isHash)
    {
        CCoinsMap::iterator coinIter = cacheCoins.find(outpoint);
        if (coinIter != cacheCoins.end())
        {
            if (pRefIterReturn)
            {
                *pRefIterReturn = cacheCoinRefs.find(COutPoint(coinIter->second.coin.nHeight, coinIter->second.coin.nTxIndex, outpoint.n));
                assert(*pRefIterReturn != cacheCoinRefs.end());
            }
            return coinIter;
        }
    }
    else
    {
        CCoinsRefMap::iterator refIter = cacheCoinRefs.find(outpoint);
        if (refIter != cacheCoinRefs.end())
        {
            auto coinIter = cacheCoins.find(refIter->second);
            assert(coinIter != cacheCoins.end());
            if (pRefIterReturn)
                *pRefIterReturn = refIter;
            return coinIter;
        }
    }

    // if outpoint not found, lookup in base view
    Coin tmp;
    COutPoint insertOutpoint(outpoint);
    if (!base->GetCoin(outpoint, tmp, &insertOutpoint))
        return cacheCoins.end();

    // verify coin and outpoint consistency
    if (!insertOutpoint.isHash)
    {
        assert(tmp.nHeight == insertOutpoint.getTransactionBlockNumber());
        assert(tmp.nTxIndex == insertOutpoint.getTransactionIndex());
    }

    // have it in base view, auto-create copy in the cache

    validateInsert(outpoint, tmp.nHeight, tmp.nTxIndex, outpoint.n);

    auto refRetIter = (cacheCoinRefs.emplace(COutPoint(tmp.nHeight, tmp.nTxIndex, insertOutpoint.n), insertOutpoint)).first;
    auto retIter = cacheCoins.emplace(std::piecewise_construct, std::forward_as_tuple(insertOutpoint), std::forward_as_tuple(std::move(tmp))).first;
    if (pRefIterReturn)
        *pRefIterReturn = refRetIter;
    
    if (retIter->second.coin.IsSpent()) {
        // The parent only has an empty entry for this outpoint; we can consider our
        // version as fresh.
        retIter->second.flags = CCoinsCacheEntry::FRESH;
    }
    cachedCoinsUsage += retIter->second.coin.DynamicMemoryUsage();
    return retIter;
}

bool CCoinsViewCache::GetCoin(const COutPoint &outpoint, Coin &coin, COutPoint* pOutpointRet) const
{
    CCoinsMap::const_iterator it = FetchCoin(outpoint);
    if (it != cacheCoins.end()) {
        if (pOutpointRet)
            *pOutpointRet = it->first;
        coin = it->second.coin;
        return true;
    }
    return false;
}

void CCoinsViewCache::AddCoin(const COutPoint &outpoint, Coin&& coin, bool possible_overwrite)
{
    if (pChainedWitView)
    {
        if ( IsPow2WitnessOutput(coin.out) )
        {
            pChainedWitView->AddCoin(outpoint, Coin(coin.out, coin.nHeight, coin.nTxIndex, coin.fCoinBase, coin.fSegSig), possible_overwrite);
        }
    }

    assert(!coin.IsSpent());
    if (coin.out.IsUnspendable()) return;

    validateInsert(outpoint, coin.nHeight, coin.nTxIndex, outpoint.n);

    CTxOut out = coin.out;

    cacheCoinRefs.emplace(COutPoint(coin.nHeight, coin.nTxIndex, outpoint.n), outpoint);
    auto [coinIter, inserted] = cacheCoins.emplace(std::piecewise_construct, std::forward_as_tuple(outpoint), std::tuple<>());
    
    bool fresh = false;
    if (!inserted) {
        cachedCoinsUsage -= coinIter->second.coin.DynamicMemoryUsage();
    }
    if (!possible_overwrite) {
        if (!coinIter->second.coin.IsSpent()) {
            throw std::logic_error("Adding new coin that replaces non-pruned entry");
        }
        fresh = !(coinIter->second.flags & CCoinsCacheEntry::DIRTY);
    }
    coinIter->second.coin = std::move(coin);
    coinIter->second.flags |= CCoinsCacheEntry::DIRTY | (fresh ? CCoinsCacheEntry::FRESH : 0);
    cachedCoinsUsage += coinIter->second.coin.DynamicMemoryUsage();
}

void AddCoins(CCoinsViewCache& cache, const CTransaction &tx, int nHeight, int nTxIndex)
{
    bool fCoinbase = tx.IsCoinBase();
    const uint256& txid = tx.GetHash();
    for (size_t i = 0; i < tx.vout.size(); ++i) {
        // Pass fCoinbase as the possible_overwrite flag to AddCoin, in order to correctly
        // deal with the pre-BIP30 occurrences of duplicate coinbase transactions.
        cache.AddCoin(COutPoint(txid, i), Coin(tx.vout[i], nHeight, nTxIndex, fCoinbase, !IsOldTransactionVersion(tx.nVersion)), fCoinbase);
    }
}

void CCoinsViewCache::SpendCoin(const COutPoint &outpoint, Coin* moveout, bool nodeletefresh)
{
    if (pChainedWitView)
    {
        // NB! The below is essential for the operation of GetWitness function, otherwise it returns unpredictable and incorrect results.
        // For chained view we force everything to 'dirty' because we need to know about fresh coins that have been removed and can't just erase them.
        pChainedWitView->SpendCoin(outpoint, NULL, true);
    }

    CCoinsRefMap::iterator coinRefIter;
    CCoinsMap::iterator coinIter = FetchCoin(outpoint, &coinRefIter);
    if (coinIter == cacheCoins.end()) return;
    assert(coinRefIter != cacheCoinRefs.end());
    cachedCoinsUsage -= coinIter->second.coin.DynamicMemoryUsage();
    if (moveout) {
        *moveout = std::move(coinIter->second.coin);
    }
    if (!nodeletefresh && coinIter->second.flags & CCoinsCacheEntry::FRESH) {
        cacheCoinRefs.erase(coinRefIter);
        cacheCoins.erase(coinIter);
    } else {
        coinIter->second.flags |= CCoinsCacheEntry::DIRTY;
        coinIter->second.coin.Spend();
    }
}

static const Coin coinEmpty;

const Coin& CCoinsViewCache::AccessCoin(const COutPoint &outpoint) const
{
    CCoinsMap::const_iterator it = FetchCoin(outpoint);
    if (it == cacheCoins.end()) {
        return coinEmpty;
    } else {
        return it->second.coin;
    }
}

bool CCoinsViewCache::HaveCoin(const COutPoint &outpoint) const
{
    CCoinsMap::const_iterator it = FetchCoin(outpoint);
    return (it != cacheCoins.end() && !it->second.coin.IsSpent());
}

bool CCoinsViewCache::HaveCoinInCache(const COutPoint &outpoint) const
{
    CCoinsMap::const_iterator it = cacheCoins.find(outpoint);
    return it != cacheCoins.end();
}

uint256 CCoinsViewCache::GetBestBlock() const
{
    if (hashBlock.IsNull())
        hashBlock = base->GetBestBlock();
    return hashBlock;
}

void CCoinsViewCache::SetBestBlock(const uint256 &hashBlockIn)
{
    hashBlock = hashBlockIn;
}

bool CCoinsViewCache::BatchWrite(CCoinsMap &mapCoins, const uint256 &hashBlockIn)
{
    for (CCoinsMap::iterator it = mapCoins.begin(); it != mapCoins.end();) {
        if (it->second.flags & CCoinsCacheEntry::DIRTY) { // Ignore non-dirty entries (optimization).
            CCoinsMap::iterator itUs = cacheCoins.find(it->first);
            if (itUs == cacheCoins.end()) {
                // The parent cache does not have an entry, while the child does
                // We can ignore it if it's both FRESH and pruned in the child
                if (!(it->second.flags & CCoinsCacheEntry::FRESH && it->second.coin.IsSpent())) {
                    // Otherwise we will need to create it in the parent
                    // and move the data up and mark it as dirty
                    CCoinsCacheEntry& entry = cacheCoins[it->first];
                    entry.coin = std::move(it->second.coin);
                    cachedCoinsUsage += entry.coin.DynamicMemoryUsage();
                    entry.flags = CCoinsCacheEntry::DIRTY;
                    // We can mark it FRESH in the parent if it was FRESH in the child
                    // Otherwise it might have just been flushed from the parent's cache
                    // and already exist in the grandparent
                    if (it->second.flags & CCoinsCacheEntry::FRESH)
                        entry.flags |= CCoinsCacheEntry::FRESH;
                    cacheCoinRefs[COutPoint(entry.coin.nHeight, entry.coin.nTxIndex, it->first.n)] = it->first;
                }
            } else {
                // Assert that the child cache entry was not marked FRESH if the
                // parent cache entry has unspent outputs. If this ever happens,
                // it means the FRESH flag was misapplied and there is a logic
                // error in the calling code.
                if ((it->second.flags & CCoinsCacheEntry::FRESH) && !itUs->second.coin.IsSpent())
                    throw std::logic_error("FRESH flag misapplied to cache entry for base transaction with spendable outputs");

                // Found the entry in the parent cache
                if ((itUs->second.flags & CCoinsCacheEntry::FRESH) && it->second.coin.IsSpent()) {
                    // The grandparent does not have an entry, and the child is
                    // modified and being pruned. This means we can just delete
                    // it from the parent.
                    cachedCoinsUsage -= itUs->second.coin.DynamicMemoryUsage();
                    cacheCoins.erase(itUs);
                    cacheCoinRefs.erase(COutPoint(itUs->second.coin.nHeight, itUs->second.coin.nTxIndex, itUs->first.n));
                } else {
                    // A normal modification.
                    cachedCoinsUsage -= itUs->second.coin.DynamicMemoryUsage();
                    itUs->second.coin = std::move(it->second.coin);
                    cachedCoinsUsage += itUs->second.coin.DynamicMemoryUsage();
                    itUs->second.flags |= CCoinsCacheEntry::DIRTY;
                    // NOTE: It is possible the child has a FRESH flag here in
                    // the event the entry we found in the parent is pruned. But
                    // we must not copy that FRESH flag to the parent as that
                    // pruned state likely still needs to be communicated to the
                    // grandparent.
                }
            }
        }
        CCoinsMap::iterator itOld = it++;
        mapCoins.erase(itOld);
    }
    hashBlock = hashBlockIn;
    return true;
}

bool CCoinsViewCache::Flush()
{
    if (pChainedWitView)
        pChainedWitView->Flush();

    bool fOk = base->BatchWrite(cacheCoins, hashBlock);
    cacheCoins.clear();
    cacheCoinRefs.clear();
    cachedCoinsUsage = 0;
    return fOk;
}

void CCoinsViewCache::Uncache(const COutPoint& hash)
{
    if (pChainedWitView)
        pChainedWitView->Uncache(hash);

    CCoinsMap::iterator it = cacheCoins.find(hash);
    if (it != cacheCoins.end() && it->second.flags == 0) {
        cachedCoinsUsage -= it->second.coin.DynamicMemoryUsage();
        cacheCoinRefs.erase(COutPoint(it->second.coin.nHeight, it->second.coin.nTxIndex, it->first.n));
        cacheCoins.erase(it);
    }
}

unsigned int CCoinsViewCache::GetCacheSize() const
{
    return cacheCoins.size();
}

CAmount CCoinsViewCache::GetValueIn(const CTransaction& tx) const
{
    if (tx.IsCoinBase() && !tx.IsPoW2WitnessCoinBase())
        return 0;

    CAmount nResult = 0;
    for (unsigned int i = 0; i < tx.vin.size(); i++)
    {
        CAmount coinAmount = AccessCoin(tx.vin[i].prevout).out.nValue;
        if (coinAmount != -1)
        {
            nResult += coinAmount;
        }
    }

    return nResult;
}

bool CCoinsViewCache::HaveInputs(const CTransaction& tx) const
{
    if (!tx.IsCoinBase() || tx.IsPoW2WitnessCoinBase()) {
        for (unsigned int i = 0; i < tx.vin.size(); i++) {
            if (!HaveCoin(tx.vin[i].prevout)) {
                if (!tx.IsPoW2WitnessCoinBase() || !tx.vin[i].prevout.IsNull())
                    return false;
            }
        }
    }
    return true;
}

//fixme: (POST-PHASE4) This can potentially be improved.
//22 is the lower bound for a new SegSig output
static const size_t MAX_OUTPUTS_PER_BLOCK = MAX_BLOCK_BASE_SIZE / 22; // TODO: merge with similar definition in undo.h.

const Coin& AccessByTxid(const CCoinsViewCache& view, const uint256& txid)
{
    COutPoint iter(txid, 0);
    while (iter.n < MAX_OUTPUTS_PER_BLOCK) {
        const Coin& alternate = view.AccessCoin(iter);
        if (!alternate.IsSpent()) return alternate;
        ++iter.n;
    }
    return coinEmpty;
}
