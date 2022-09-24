// Copyright (c) 2012-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// File contains modifications by: The Centure developers
// All modifications:
// Copyright (c) 2017-2022 The Centure developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the Libre Chain License, see the accompanying
// file COPYING

#include "coins.h"

#include "consensus/consensus.h"
#include "memusage.h"
#include "random.h"

#include <assert.h>

#include "txmempool.h"

#include "witnessutil.h"

#include <alert.h>
#include "ui_interface.h"

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

CCoinsViewCache::CCoinsViewCache(CCoinsView *baseIn)
: CCoinsViewBacked(baseIn)
, cachedCoinsUsage(0)
, pChainedWitView(nullptr)
{}

CCoinsViewCache::CCoinsViewCache(CCoinsViewCache *baseIn)
: CCoinsViewBacked(baseIn)
, cachedCoinsUsage(0)
, pChainedWitView(baseIn->pChainedWitView?std::shared_ptr<CCoinsViewCache>(new CCoinsViewCache(baseIn->pChainedWitView.get())):nullptr)
{}

size_t CCoinsViewCache::DynamicMemoryUsage() const
{
    return memusage::DynamicUsage(cacheCoins) + cachedCoinsUsage;
}


#if defined(DEBUG) && !defined(PLATFORM_MOBILE)
void CCoinsViewCache::validateInsert(const COutPoint &outpoint, uint64_t block, uint64_t txIndex, uint32_t voutIndex) const
{
    // check args
    assert(outpoint.isHash || (outpoint.getTransactionBlockNumber() == block && outpoint.getTransactionIndex() == txIndex));
    assert(outpoint.n == voutIndex);

    // cacheCoins and cacheCoinRefs keep a 1:1 correspondence, so any difference in size is a bug for sure
    // Sadly not quite true:
    // 1) cacheCoins may contain a dirty entry and a live entry both of which correspond to the same cacheCoinRefs (e.g. reorganisation)
    // 2) mempool entries don't have a block height (so aren't in cacheCoinRefs), so we track those as 'cacheMempoolRefs' counter and compensate
    /*if (cacheCoins.size() != cacheCoinRefs.size()+cacheMempoolRefs)
    {
        uint64_t nNotSpentCacheCoinsCount = 0;
        for (const auto& it : cacheCoins)
        {
            if (!(it.second.flags&CCoinsCacheEntry::DIRTY && it.second.coin.IsSpent()))
            {
                ++nNotSpentCacheCoinsCount;
            }
        }
        assert(nNotSpentCacheCoinsCount == cacheCoinRefs.size()+cacheMempoolRefs);
    }*/

    CCoinsRefMap::iterator refIt = cacheCoinRefs.find(outpoint.isHash ? COutPoint(block, txIndex, voutIndex) : outpoint);
    
    // Mempool entries should never be present in cacheCoinRefs (they have no block height)
    if (block == MEMPOOL_HEIGHT && txIndex == MEMPOOL_INDEX)
    {
        assert(outpoint.isHash);
        assert(refIt == cacheCoinRefs.end());        
    }
    else
    {    
        if (refIt != cacheCoinRefs.end())
        {
            // entry present in cacheCoinRefs
            const COutPoint& canonicalOutPoint = refIt->second;
            CCoinsMap::iterator it = cacheCoins.find(canonicalOutPoint);
            assert(it != cacheCoins.end());  // verify it is present in cacheCoins as well
            const Coin& coin = it->second.coin;

            // if canonicalOutPoint is not a hash then something is very wrong
            assert(canonicalOutPoint.isHash);
            
            // If the two don't share an identical transaction hash something is very wrong
            bool transactionHashesMatch = canonicalOutPoint.getTransactionHash() == outpoint.getTransactionHash();
            // Unless the other one is spent and marked dirty, in which case thats fine
            if (!transactionHashesMatch && !(it->second.flags&CCoinsCacheEntry::DIRTY&&it->second.coin.IsSpent()))
            {
                std::string warning = strprintf("Warning: outpoint mismatch.\nPlease notify the developers with this information to assist them.\n\n"
                                                "cohash:[%s]\nophash:[%s]\nblock:[%d] txidx:[%d] outidx:[%d] flags:[%d] spent:[%s] lookupishash: [%s]",
                                                canonicalOutPoint.getTransactionHash().ToString(), outpoint.getTransactionHash().ToString(), 
                                                block, txIndex, voutIndex, it->second.flags, (it->second.coin.IsSpent()?"yes":"no"), (outpoint.isHash?"yes":"no"));
                throw std::logic_error(warning);
            }
            
            // Block and index should match up
            assert(coin.nHeight == block);
            assert(coin.nTxIndex == txIndex);
            assert(canonicalOutPoint.n == voutIndex);
        }
        else
        {
            // no entry in cacheCoinRefs, so it should be absent in cacheCoins also
            // This assumption is not necessarily true if it was previously in mempool - so has been disabled for now
            #if 0
            if (outpoint.isHash)
            {
                if (cacheCoins.find(outpoint) != cacheCoins.end())
                {
                    CCoinsMap::iterator it = cacheCoins.find(outpoint);
                    std::string warning = strprintf("Warning: outpoint mismatch.\nPlease notify the developers with this information to assist them.\n\n"
                                                "ophash:[%s]\nblock:[%d] txidx:[%d] outidx:[%d] flags:[%d] spent:[%s] lookupishash: [%s]",
                                                outpoint.getTransactionHash().ToString(), 
                                                block, txIndex, voutIndex, it->second.flags, (it->second.coin.IsSpent()?"yes":"no"), (outpoint.isHash?"yes":"no"));
                    uiInterface.NotifyUIAlertChanged(warning);
                }
            }
            #endif
            // The non-hash output definitely shouldn't be in the cacheCoins ever
            assert(outpoint.isHash || cacheCoins.find(COutPoint(block, txIndex, voutIndex)) == cacheCoins.end());
        }
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
                if (coinIter->second.coin.nHeight == MEMPOOL_HEIGHT && coinIter->second.coin.nTxIndex == MEMPOOL_INDEX)
                {
                    *pRefIterReturn = cacheCoinRefs.end();
                }
                else
                {
                    *pRefIterReturn = cacheCoinRefs.find(COutPoint(coinIter->second.coin.nHeight, coinIter->second.coin.nTxIndex, outpoint.n));
                    assert(*pRefIterReturn != cacheCoinRefs.end());
                }
            }
            return coinIter;
        }
    }
    else
    {
        assert(!(outpoint.getTransactionBlockNumber() == MEMPOOL_HEIGHT && outpoint.getTransactionIndex() == MEMPOOL_INDEX));
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
    
    // Never directly insert non-hash outpoints
    assert(insertOutpoint.isHash);

    // Ensure consistency
    validateInsert(insertOutpoint, tmp.nHeight, tmp.nTxIndex, outpoint.n);

    // have it in base view, auto-create copy in the cache   
    if (tmp.nHeight == MEMPOOL_HEIGHT && tmp.nTxIndex == MEMPOOL_INDEX)
    {
        if (pRefIterReturn)
            *pRefIterReturn = cacheCoinRefs.end();
            
    }
    else
    {
        cacheCoinRefs[(COutPoint(tmp.nHeight, tmp.nTxIndex, insertOutpoint.n))] = insertOutpoint;
        if (pRefIterReturn)
        {
            *pRefIterReturn = cacheCoinRefs.find(COutPoint(tmp.nHeight, tmp.nTxIndex, insertOutpoint.n));
        }
    }
    auto retIter = cacheCoins.emplace(std::piecewise_construct, std::forward_as_tuple(insertOutpoint), std::forward_as_tuple(std::move(tmp))).first;
    
    
    // If the parent only has an empty entry for this outpoint; we can consider our version as fresh.
    if (retIter->second.coin.IsSpent())
    {
        retIter->second.flags = CCoinsCacheEntry::FRESH;
    }
    cachedCoinsUsage += retIter->second.coin.DynamicMemoryUsage();
    return retIter;
}

bool CCoinsViewCache::GetCoin(const COutPoint &outpoint, Coin &coin, COutPoint* pOutpointRet) const
{
    CCoinsMap::const_iterator it = FetchCoin(outpoint);
    if (it != cacheCoins.end())
    {
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
    if (coin.out.IsUnspendable())
    {
        return;
    }
    
    // Never directly insert non-hash outpoints
    assert(outpoint.isHash);

    // Ensure consistency
    validateInsert(outpoint, coin.nHeight, coin.nTxIndex, outpoint.n);

    // If there is already an existing entry the indexed outpoint map needs to be kept in sync
    if (cacheCoins.count(outpoint) > 0)
    {
        auto coin_entry = cacheCoins[outpoint];
        auto refIter = cacheCoinRefs.find(COutPoint(coin_entry.coin.nHeight, coin_entry.coin.nTxIndex, outpoint.n));

        // If the existing indexed outpoint is associated with this coin_entry the old entry is erased so there
        // will be no stale entry left if its index changes
        if (refIter != cacheCoinRefs.end() && refIter->second.getTransactionHash() == outpoint.getTransactionHash())
        {
            cacheCoinRefs.erase(refIter);
        }
    }

    if (!(coin.nHeight == MEMPOOL_HEIGHT && coin.nTxIndex == MEMPOOL_INDEX))
    {
        cacheCoinRefs[(COutPoint(coin.nHeight, coin.nTxIndex, outpoint.n))] = outpoint;
    }
    auto [coinIter, inserted] = cacheCoins.emplace(std::piecewise_construct, std::forward_as_tuple(outpoint), std::tuple<>());
    
    bool fresh = false;
    if (!inserted)
    {
        cachedCoinsUsage -= coinIter->second.coin.DynamicMemoryUsage();
    }
    if (!possible_overwrite)
    {
        if (!coinIter->second.coin.IsSpent())
        {
            throw std::logic_error("Adding new coin that replaces non-pruned entry");
        }
        fresh = !(coinIter->second.flags & CCoinsCacheEntry::DIRTY);
    }
    coinIter->second.coin = std::move(coin);
    coinIter->second.flags |= CCoinsCacheEntry::DIRTY | (fresh ? CCoinsCacheEntry::FRESH : 0);
    cachedCoinsUsage += coinIter->second.coin.DynamicMemoryUsage();
}

void AddCoins(CCoinsViewCache& cache, const CTransaction &tx, uint32_t nHeight, uint32_t nTxIndex)
{
    bool fCoinbase = tx.IsCoinBase();
    const uint256& txid = tx.GetHash();
    for (size_t i = 0; i < tx.vout.size(); ++i)
    {
        // Pass fCoinbase as the possible_overwrite flag to AddCoin, in order to correctly
        // deal with the pre-BIP30 occurrences of duplicate coinbase transactions.
        cache.AddCoin(COutPoint(txid, i), Coin(tx.vout[i], nHeight, nTxIndex, fCoinbase, !IsOldTransactionVersion(tx.nVersion)), fCoinbase);
    }
}

void CCoinsViewCache::SpendCoin(const COutPoint &outpoint, CoinUndo* moveout, bool nodeletefresh)
{
    if (pChainedWitView)
    {
        // NB! The below is essential for the operation of GetWitness function, otherwise it returns unpredictable and incorrect results.
        // For chained view we force everything to 'dirty' because we need to know about fresh coins that have been removed and can't just erase them.
        pChainedWitView->SpendCoin(outpoint, NULL, true);
    }

    CCoinsRefMap::iterator coinRefIter;
    CCoinsMap::iterator coinIter = FetchCoin(outpoint, &coinRefIter);
    if (coinIter == cacheCoins.end())
    {
        return;
    }
    if (!(coinIter->second.coin.nHeight == MEMPOOL_HEIGHT && coinIter->second.coin.nTxIndex == MEMPOOL_INDEX))
    {
        assert(coinRefIter != cacheCoinRefs.end());
    }
    cachedCoinsUsage -= coinIter->second.coin.DynamicMemoryUsage();
    if (moveout)
    {
        *moveout = CoinUndo(std::move(coinIter->second.coin), coinIter->first.getTransactionHash());
    }
    if (!nodeletefresh && coinIter->second.flags & CCoinsCacheEntry::FRESH)
    {
        if (coinRefIter != cacheCoinRefs.end())
        {
            if ((!outpoint.isHash) || coinRefIter->second == outpoint)
            {
                cacheCoinRefs.erase(coinRefIter);
            }
        }
        cacheCoins.erase(coinIter);
    }
    else
    {
        coinIter->second.flags |= CCoinsCacheEntry::DIRTY;
        coinIter->second.coin.Spend();
    }
}

static const Coin coinEmpty;

const Coin& CCoinsViewCache::AccessCoin(const COutPoint &outpoint) const
{
    CCoinsMap::const_iterator it = FetchCoin(outpoint);
    if (it == cacheCoins.end())
    {
        return coinEmpty;
    }
    else
    {
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
    // It is possible (in fact likely) for the same batch to be both erasing and writing the same entryref e.g. if swapping one block 1963 for a competing block 1963
    // mapCoins is 'randomly' ordered so doing this would create random behaviour and an inconsistent coin database
    // To overcome this we erase first always, and then pool up the inserts to do at the end, where we must still carefully order the inserts as well
    std::vector<COutPoint> modificationMap;
    for (CCoinsMap::iterator it = mapCoins.begin(); it != mapCoins.end();)
    {
        // Ignore non-dirty entries (optimization).
        if (it->second.flags & CCoinsCacheEntry::DIRTY)
        {
            CCoinsMap::iterator itUs = cacheCoins.find(it->first);
            if (itUs == cacheCoins.end())
            {
                // The parent cache does not have an entry, while the child does
                // We can ignore it if it's both FRESH and pruned in the child
                if (!(it->second.flags & CCoinsCacheEntry::FRESH && it->second.coin.IsSpent()))
                {
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
                    {
                        entry.flags |= CCoinsCacheEntry::FRESH;
                    }
                    if (!(entry.coin.nHeight == MEMPOOL_HEIGHT && entry.coin.nTxIndex == MEMPOOL_INDEX))
                    {
                        modificationMap.push_back(it->first);
                    }
                }
            }
            else
            {
                // Assert that the child cache entry was not marked FRESH if the
                // parent cache entry has unspent outputs. If this ever happens,
                // it means the FRESH flag was misapplied and there is a logic
                // error in the calling code.
                if ((it->second.flags & CCoinsCacheEntry::FRESH) && !itUs->second.coin.IsSpent())
                {
                    throw std::logic_error("FRESH flag misapplied to cache entry for base transaction with spendable outputs");
                }

                // Found the entry in the parent cache
                if ((itUs->second.flags & CCoinsCacheEntry::FRESH) && it->second.coin.IsSpent())
                {
                    // The grandparent does not have an entry, and the child is
                    // modified and being pruned. This means we can just delete
                    // it from the parent.
                    cachedCoinsUsage -= itUs->second.coin.DynamicMemoryUsage();
                    if (!(itUs->second.coin.nHeight == MEMPOOL_HEIGHT && itUs->second.coin.nTxIndex == MEMPOOL_INDEX))
                    {
                        // delete index, but only if it correspends
                        if (itUs->second.coin.nHeight == it->second.coin.nHeight
                            && itUs->second.coin.nTxIndex == it->second.coin.nTxIndex
                            && itUs->first.n == it->first.n)
                        {
                            cacheCoinRefs.erase(COutPoint(itUs->second.coin.nHeight, itUs->second.coin.nTxIndex, itUs->first.n));
                        }
                    }
                    cacheCoins.erase(itUs);
                }
                else
                {
                    // if there is already an indexed entry that is ours then delete it as we might be moving to another index
                    // if it is not ours then leave it
                    auto indexed = COutPoint(itUs->second.coin.nHeight, itUs->second.coin.nTxIndex, itUs->first.n);
                    if (cacheCoinRefs[indexed].getTransactionHash() == it->first.getTransactionHash())
                    {
                        cacheCoinRefs.erase(indexed);
                    }

                    // A normal modification.
                    cachedCoinsUsage -= itUs->second.coin.DynamicMemoryUsage();
                    itUs->second.coin = std::move(it->second.coin);
                    cachedCoinsUsage += itUs->second.coin.DynamicMemoryUsage();
                    itUs->second.flags |= CCoinsCacheEntry::DIRTY;

                    // A "normal" modification could also be changing the canonical outpoint that the cacheCoinRefs is pointing to.
                    // This happens when a re-org causes an earlier (coinbase) coin that is still in the cache (be it spend and dirty)
                    // gets re-added. If the re-org had a tx with the same index outpoint that will overwrite the (cacheCoinRefs).
                    // So when the original is re-added again because of a 2nd re-org the index outpoint has to be updated to match again.
                    // So it is added to the modificationMap to ensure the cacheCoinRefs is updated
                     modificationMap.push_back(it->first);

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
       
    for (const auto& outPoint : modificationMap)
    {
        const auto iter = cacheCoins.find(outPoint);
        if (iter != cacheCoins.end())
        {
            if (iter->second.coin.IsSpent())
            {
                // if there is an existing index entry which points to another coin that is NOT spent then leave it
                // otherwise update/create an index entry for the (now) spent coin
                auto indexedOut = COutPoint(iter->second.coin.nHeight, iter->second.coin.nTxIndex, outPoint.n);
                const auto iterRefs = cacheCoinRefs.find(indexedOut);
                if (iterRefs != cacheCoinRefs.end() && !cacheCoins[iterRefs->second].coin.IsSpent())
                {
                    continue;
                }
                cacheCoinRefs[COutPoint(iter->second.coin.nHeight, iter->second.coin.nTxIndex, outPoint.n)] = outPoint;
            }
        }
    }
    for (const auto& outPoint : modificationMap)
    {
        const auto iter = cacheCoins.find(outPoint);
        if (iter != cacheCoins.end())
        {
            if (!iter->second.coin.IsSpent())
            {
                cacheCoinRefs[COutPoint(iter->second.coin.nHeight, iter->second.coin.nTxIndex, outPoint.n)] = outPoint;
            }
        }
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

    //NB! Though it may not seem obvious from browsing the code there is one instance where this function can be called with an index based outpoint instead of a hash
    //This is when loading a wallet that has a transaction with a prevout that is index based (and not in the wallet)
    //We then get called from ReacceptWalletTransactions/AcceptToMemoryPool with an index based outpoint
    //So we handle that special case here then rest of the function MUST work on hash based only as the argument name implies
    COutPoint canonicalHash = hash;
    if (!hash.isHash)
    {
        auto refIter = cacheCoinRefs.find(hash);
        if (refIter == cacheCoinRefs.end())
        {
            return;
        }
        else
        {
            canonicalHash = refIter->second;
        }
    }
    
    
    CCoinsMap::iterator it = cacheCoins.find(canonicalHash);
    if (it != cacheCoins.end() && it->second.flags == 0)
    {
        cachedCoinsUsage -= it->second.coin.DynamicMemoryUsage();
        if (!(it->second.coin.nHeight == MEMPOOL_HEIGHT && it->second.coin.nTxIndex == MEMPOOL_INDEX))
        {
            COutPoint indexBased = COutPoint(it->second.coin.nHeight, it->second.coin.nTxIndex, it->first.n);
            auto refIter = cacheCoinRefs.find(indexBased);
            if (refIter != cacheCoinRefs.end())
            {
                if (refIter->second == canonicalHash)
                {
                    cacheCoinRefs.erase(COutPoint(it->second.coin.nHeight, it->second.coin.nTxIndex, it->first.n));
                }
            }
        }
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
        CAmount coinAmount = AccessCoin(tx.vin[i].GetPrevOut()).out.nValue;
        if (coinAmount != -1)
        {
            nResult += coinAmount;
        }
    }

    return nResult;
}

bool CCoinsViewCache::HaveInputs(const CTransaction& tx) const
{
    if (!tx.IsCoinBase() || tx.IsPoW2WitnessCoinBase())
    {
        for (unsigned int i = 0; i < tx.vin.size(); i++)
        {
            if (!HaveCoin(tx.vin[i].GetPrevOut()))
            {
                if (!tx.IsPoW2WitnessCoinBase() || !tx.vin[i].GetPrevOut().IsNull())
                {
                    return false;
                }
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
    while (iter.n < MAX_OUTPUTS_PER_BLOCK)
    {
        const Coin& alternate = view.AccessCoin(iter);
        if (!alternate.IsSpent()) return alternate;
        ++iter.n;
    }
    return coinEmpty;
}
