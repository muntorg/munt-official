// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// File contains modifications by: The Gulden developers
// All modifications:
// Copyright (c) 2016-2019 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#include "txdb.h"

#include "chainparams.h"
#include "hash.h"
#include "pow.h"
#include "uint256.h"

#include <Gulden/util.h>
#include <stdint.h>

#include <boost/thread.hpp>

#include <validation/witnessvalidation.h> //For ppow2witTip (remove in future)

// Old v0 format, deprecated v1
static const char DB_COINS = 'c';

// v1 format
static const char DB_COIN = 'C';

static const char DB_BLOCK_FILES = 'f';
static const char DB_TXINDEX = 't';
static const char DB_BLOCK_INDEX = 'b';

static const char DB_BEST_BLOCK = 'B';
static const char DB_FLAG = 'F';
static const char DB_REINDEX_FLAG = 'R';
static const char DB_LAST_BLOCK = 'l';

static const char DB_VERSION     = '1';
static const char DB_POW2_PHASE2 = '2';
static const char DB_POW2_PHASE3 = '3';
static const char DB_POW2_PHASE4 = '4';
static const char DB_POW2_PHASE5 = '5';

// Additional v2 format
static const char DB_COIN_REF = 'r';

namespace {

struct CoinEntry {
    COutPoint* outpoint;
    char key;
    CoinEntry(const COutPoint* ptr) : outpoint(const_cast<COutPoint*>(ptr)), key(DB_COIN)  {}

    template<typename Stream>
    void Serialize(Stream &s) const {
        s << key;
        s << outpoint->getBucketHash();
        uint32_t nTemp = outpoint->n;
        s << VARINT(nTemp);
    }

    template<typename Stream>
    void Unserialize(Stream& s) {
        s >> key;
        uint256 hash;
        s >> hash;
        outpoint->setHash(hash);
        uint32_t n_ = 0;
        s >> VARINT(n_);
        outpoint->n = n_;
    }
};

struct CoinEntryRef {
    char key;
    uint64_t nHeight;
    uint64_t nTxIndex;
    uint32_t n;
    CoinEntryRef(uint64_t nHeightIn, uint64_t nTxIndexIn, uint64_t nIn)
    : key(DB_COIN_REF)
    , nHeight(nHeightIn)
    , nTxIndex(nTxIndexIn)
    , n(nIn)
    {}
    
    CoinEntryRef(const COutPoint& outpoint)
    : key(DB_COIN_REF)
    {
        assert(!outpoint.isHash);
        nHeight = outpoint.getTransactionBlockNumber();
        nTxIndex = outpoint.getTransactionIndex();
        n = outpoint.n;
    }

    template<typename Stream>
    void Serialize(Stream &s) const {
        s << key;
        s << nHeight; //NB! We delibritely don't use varint here, to remove the possibility of collisions.
        s << nTxIndex;
        s << VARINT(n);
    }

    template<typename Stream>
    void Unserialize(Stream& s) {
        s >> key;
        s >> nHeight;
        s >> nTxIndex;
        s >> VARINT(n);
    }
};

}

CWitViewDB::CWitViewDB(size_t nCacheSize, bool fMemory, bool fWipe) : CCoinsViewDB(nCacheSize, fMemory, fWipe, "witstate")
{
}

CCoinsViewDB::CCoinsViewDB(size_t nCacheSize, bool fMemory, bool fWipe, std::string name) : db(GetDataDir() / name, nCacheSize, fMemory, fWipe, true)
{
}

bool CCoinsViewDB::GetCoin(const COutPoint &outpoint, Coin &coin, COutPoint* pOutpointRet) const
{
    if (!outpoint.isHash)
    {
        uint256 hash;
        if (db.Read(CoinEntryRef(outpoint), hash))
        {
            COutPoint dereferencedOutpoint(hash, outpoint.n);
            if (pOutpointRet)
                *pOutpointRet = dereferencedOutpoint;
            return db.Read(CoinEntry(&dereferencedOutpoint), coin);
        }
        return false;
    }
    else 
    {
        if (pOutpointRet)
            *pOutpointRet = outpoint;
        return db.Read(CoinEntry(&outpoint), coin);
    }
}

bool CCoinsViewDB::HaveCoin(const COutPoint &outpoint) const
{
    if (outpoint.isHash)
        return db.Exists(CoinEntryRef(outpoint));
    else 
        return db.Exists(CoinEntry(&outpoint));
}

void CCoinsViewDB::SetPhase2ActivationHash(const uint256 &hashPhase2ActivationPoint)
{
    db.Write(DB_POW2_PHASE2, hashPhase2ActivationPoint);
}

uint256 CCoinsViewDB::GetPhase2ActivationHash()
{
    uint256 hashPhase2ActivationPoint;
    if (!db.Read(DB_POW2_PHASE2, hashPhase2ActivationPoint))
        return uint256();
    return hashPhase2ActivationPoint;
}

void CCoinsViewDB::SetPhase3ActivationHash(const uint256 &hashPhase3ActivationPoint)
{
    db.Write(DB_POW2_PHASE3, hashPhase3ActivationPoint);
}

uint256 CCoinsViewDB::GetPhase3ActivationHash()
{
    uint256 hashPhase3ActivationPoint;
    if (!db.Read(DB_POW2_PHASE3, hashPhase3ActivationPoint))
        return uint256();
    return hashPhase3ActivationPoint;
}

void CCoinsViewDB::SetPhase4ActivationHash(const uint256 &hashPhase4ActivationPoint)
{
    db.Write(DB_POW2_PHASE4, hashPhase4ActivationPoint);
}

uint256 CCoinsViewDB::GetPhase4ActivationHash()
{
    uint256 hashPhase4ActivationPoint;
    if (!db.Read(DB_POW2_PHASE4, hashPhase4ActivationPoint))
        return uint256();
    return hashPhase4ActivationPoint;
}

void CCoinsViewDB::SetPhase5ActivationHash(const uint256 &hashPhase5ActivationPoint)
{
    db.Write(DB_POW2_PHASE5, hashPhase5ActivationPoint);
}

uint256 CCoinsViewDB::GetPhase5ActivationHash()
{
    uint256 hashPhase5ActivationPoint;
    if (!db.Read(DB_POW2_PHASE5, hashPhase5ActivationPoint))
        return uint256();
    return hashPhase5ActivationPoint;
}

uint256 CCoinsViewDB::GetBestBlock() const {
    uint256 hashBestChain;
    if (!db.Read(DB_BEST_BLOCK, hashBestChain))
        return uint256();
    return hashBestChain;
}

bool CCoinsViewDB::BatchWrite(CCoinsMap &mapCoins, const uint256 &hashBlock) {
    CDBBatch batch(db);
    size_t count = 0;
    size_t changed = 0;
    for (CCoinsMap::iterator it = mapCoins.begin(); it != mapCoins.end();) {
        if (it->second.flags & CCoinsCacheEntry::DIRTY) {
            CoinEntry entry(&it->first);
            CoinEntryRef entryRef(it->second.coin.nHeight, it->second.coin.nTxIndex, it->first.n);
            if (it->second.coin.IsSpent())
            {
                batch.Erase(entry);
                batch.Erase(entryRef);
            }
            else
            {
                batch.Write(entry, it->second.coin);
                batch.Write(entryRef, it->first.getBucketHash());
            }
            changed++;
        }
        count++;
        CCoinsMap::iterator itOld = it++;
        mapCoins.erase(itOld);
    }
    if (!hashBlock.IsNull())
        batch.Write(DB_BEST_BLOCK, hashBlock);

    bool ret = db.WriteBatch(batch);
    LogPrint(BCLog::COINDB, "Committed %u changed transaction outputs (out of %u) to coin database...\n", (unsigned int)changed, (unsigned int)count);
    return ret;
}

size_t CCoinsViewDB::EstimateSize() const
{
    return db.EstimateSize(DB_COIN, (char)(DB_COIN+1));
}

void CCoinsViewDB::GetAllCoins(std::map<COutPoint, Coin>& allCoins) const
{
    CCoinsViewCursor* cursor = Cursor();
    if (cursor)
    {
        while (cursor->Valid())
        {
            COutPoint outPoint;
            if (!cursor->GetKey(outPoint))
                throw std::runtime_error("Error fetching record from witness cache.");

            Coin outCoin;
            if (!cursor->GetValue(outCoin))
                throw std::runtime_error("Error fetching record from witness cache.");

            allCoins.emplace(std::pair(outPoint, outCoin));

            cursor->Next();
        }
        delete cursor;
    }
}

CBlockTreeDB::CBlockTreeDB(size_t nCacheSize, bool fMemory, bool fWipe) : CDBWrapper(GetDataDir() / "blocks" / "index", nCacheSize, fMemory, fWipe) {
}

bool CBlockTreeDB::ReadBlockFileInfo(int nFile, CBlockFileInfo &info) {
    return Read(std::pair(DB_BLOCK_FILES, nFile), info);
}

bool CBlockTreeDB::WriteReindexing(bool fReindexing) {
    if (fReindexing)
        return Write(DB_REINDEX_FLAG, '1');
    else
        return Erase(DB_REINDEX_FLAG);
}

bool CBlockTreeDB::ReadReindexing(bool &fReindexing) {
    fReindexing = Exists(DB_REINDEX_FLAG);
    return true;
}

bool CBlockTreeDB::ReadLastBlockFile(int &nFile) {
    return Read(DB_LAST_BLOCK, nFile);
}

CCoinsViewCursor *CCoinsViewDB::Cursor() const
{
    CCoinsViewDBCursor *i = new CCoinsViewDBCursor(const_cast<CDBWrapper*>(&db)->NewIterator(), GetBestBlock());
    /* It seems that there are no "const iterators" for LevelDB.  Since we
       only need read operations on it, use a const-cast to get around
       that restriction.  */
    i->pcursor->Seek(DB_COIN);
    // Cache key of first record
    if (i->pcursor->Valid()) {
        CoinEntry entry(&i->keyTmp.second);
        i->pcursor->GetKey(entry);
        i->keyTmp.first = entry.key;
    } else {
        i->keyTmp.first = 0; // Make sure Valid() and GetKey() return false
    }
    return i;
}

bool CCoinsViewDBCursor::GetKey(COutPoint &key) const
{
    // Return cached key
    if (keyTmp.first == DB_COIN) {
        key = keyTmp.second;
        return true;
    }
    return false;
}

bool CCoinsViewDBCursor::GetValue(Coin &coin) const
{
    return pcursor->GetValue(coin);
}

unsigned int CCoinsViewDBCursor::GetValueSize() const
{
    return pcursor->GetValueSize();
}

bool CCoinsViewDBCursor::Valid() const
{
    return keyTmp.first == DB_COIN;
}

void CCoinsViewDBCursor::Next()
{
    pcursor->Next();
    CoinEntry entry(&keyTmp.second);
    if (!pcursor->Valid() || !pcursor->GetKey(entry)) {
        keyTmp.first = 0; // Invalidate cached key after last record so that Valid() and GetKey() return false
    } else {
        keyTmp.first = entry.key;
    }
}

bool CBlockTreeDB::UpdateBatchSync(const std::vector<std::pair<int, const CBlockFileInfo*> >& fileInfo, int nLastFile,
                                   const std::vector<const CBlockIndex*>& vWriteIndices,
                                   const std::vector<uint256>& vEraseHashes)
{
    CDBBatch batch(*this);
    for (std::vector<std::pair<int, const CBlockFileInfo*> >::const_iterator it=fileInfo.begin(); it != fileInfo.end(); it++) {
        batch.Write(std::pair(DB_BLOCK_FILES, it->first), *it->second);
    }
    batch.Write(DB_LAST_BLOCK, nLastFile);
    for (std::vector<const CBlockIndex*>::const_iterator it=vWriteIndices.begin(); it != vWriteIndices.end(); it++) {
        batch.Write(std::pair(DB_BLOCK_INDEX, (*it)->GetBlockHashPoW2()), CDiskBlockIndex(*it));
    }
    for (const uint256& hash: vEraseHashes) {
        batch.Erase(std::pair(DB_BLOCK_INDEX, hash));
    }
    return WriteBatch(batch, true);

}

bool CBlockTreeDB::EraseBatchSync(const std::vector<uint256>& vEraseHashes)
{
    CDBBatch batch(*this);
    for (const uint256& hash: vEraseHashes) {
        batch.Erase(std::pair(DB_BLOCK_INDEX, hash));
    }
    return WriteBatch(batch, true);
}

bool CBlockTreeDB::ReadTxIndex(const uint256 &txid, CDiskTxPos &pos) {
    return Read(std::pair(DB_TXINDEX, txid), pos);
}

bool CBlockTreeDB::WriteTxIndex(const std::vector<std::pair<uint256, CDiskTxPos> >&vect) {
    CDBBatch batch(*this);
    for (std::vector<std::pair<uint256,CDiskTxPos> >::const_iterator it=vect.begin(); it!=vect.end(); it++)
        batch.Write(std::pair(DB_TXINDEX, it->first), it->second);
    return WriteBatch(batch);
}

bool CBlockTreeDB::WriteFlag(const std::string &name, bool fValue) {
    return Write(std::pair(DB_FLAG, name), fValue ? '1' : '0');
}

bool CBlockTreeDB::ReadFlag(const std::string &name, bool &fValue) {
    char ch;
    if (!Read(std::pair(DB_FLAG, name), ch))
        return false;
    fValue = ch == '1';
    return true;
}

bool CBlockTreeDB::LoadBlockIndexGuts(std::function<CBlockIndex*(const uint256&)> insertBlockIndex)
{
    std::unique_ptr<CDBIterator> pcursor(NewIterator());

    pcursor->Seek(std::pair(DB_BLOCK_INDEX, uint256()));

    // Load mapBlockIndex
    while (pcursor->Valid()) {
        boost::this_thread::interruption_point();
        std::pair<char, uint256> key;
        if (pcursor->GetKey(key) && key.first == DB_BLOCK_INDEX) {
            CDiskBlockIndex diskindex;
            if (pcursor->GetValue(diskindex)) {
                // Construct block index object
                CBlockIndex* pindexNew = insertBlockIndex(diskindex.GetBlockHashPoW2());
                // this insertBlockIndex can create an index block that is never loaded with data
                pindexNew->pprev          = insertBlockIndex(diskindex.hashPrev);
                pindexNew->nHeight        = diskindex.nHeight;
                pindexNew->nFile          = diskindex.nFile;
                pindexNew->nDataPos       = diskindex.nDataPos;
                pindexNew->nUndoPos       = diskindex.nUndoPos;
                pindexNew->nVersion       = diskindex.nVersion;
                pindexNew->hashMerkleRoot = diskindex.hashMerkleRoot;
                pindexNew->nTime          = diskindex.nTime;
                pindexNew->nBits          = diskindex.nBits;
                pindexNew->nNonce         = diskindex.nNonce;
                pindexNew->nStatus        = diskindex.nStatus;
                // nStatus later used to check if a block index was created during loading but never filled
                assert(pindexNew->nStatus != 0);
                pindexNew->nTx            = diskindex.nTx;

                pindexNew->nVersionPoW2Witness = diskindex.nVersionPoW2Witness;
                pindexNew->nTimePoW2Witness = diskindex.nTimePoW2Witness;
                pindexNew->hashMerkleRootPoW2Witness = diskindex.hashMerkleRootPoW2Witness;
                pindexNew->witnessHeaderPoW2Sig = diskindex.witnessHeaderPoW2Sig;

                /** Scrypt is used for block proof-of-work, but for purposes of performance the index internally uses sha256.
                *  This check was considered unneccessary given the other safeguards like the genesis and checkpoints. */
                //if (!CheckProofOfWork(pindexNew, Params().GetConsensus()))
                    //return error("LoadBlockIndex(): CheckProofOfWork failed: %s", pindexNew->ToString());

                pcursor->Next();
            } else {
                return error("LoadBlockIndex() : failed to read value");
            }
        } else {
            break;
        }
    }

    return true;
}

namespace {

//! Legacy class to deserialize pre-pertxout database entries without reindex.
class CCoins
{
public:
    //! whether transaction is a coinbase
    bool fCoinBase;

    //! unspent transaction outputs; spent outputs are .IsNull(); spent outputs at the end of the array are dropped
    std::vector<CTxOut> vout;

    //! at which height this transaction was included in the active block chain
    int nHeight;

    //! empty constructor
    CCoins() : fCoinBase(false), vout(0), nHeight(0) { }

    template<typename Stream>
    void Unserialize(Stream &s) {
        unsigned int nCode = 0;
        // version
        int nVersionDummy;
        ::Unserialize(s, VARINT(nVersionDummy));
        // header code
        ::Unserialize(s, VARINT(nCode));
        fCoinBase = nCode & 1;
        std::vector<bool> vAvail(2, false);
        vAvail[0] = (nCode & 2) != 0;
        vAvail[1] = (nCode & 4) != 0;
        unsigned int nMaskCode = (nCode / 8) + ((nCode & 6) != 0 ? 0 : 1);
        // spentness bitmask
        while (nMaskCode > 0) {
            unsigned char chAvail = 0;
            ::Unserialize(s, chAvail);
            for (unsigned int p = 0; p < 8; p++) {
                bool f = (chAvail & (1 << p)) != 0;
                vAvail.push_back(f);
            }
            if (chAvail != 0)
                nMaskCode--;
        }
        // txouts themself
        vout.assign(vAvail.size(), CTxOut());
        for (unsigned int i = 0; i < vAvail.size(); i++) {
            if (vAvail[i])
                ::Unserialize(s, REF(CTxOutCompressor(vout[i])));
        }
        // coinbase height
        ::Unserialize(s, VARINT(nHeight));
    }

    template<typename Stream>
    void UnserializeLegacy(Stream &s) {
        unsigned int nCode = 0;
        // version
        int nVersionDummy;
        ::Unserialize(s, VARINT(nVersionDummy));
        // header code
        ::Unserialize(s, VARINT(nCode));
        fCoinBase = nCode & 1;
        std::vector<bool> vAvail(2, false);
        vAvail[0] = (nCode & 2) != 0;
        vAvail[1] = (nCode & 4) != 0;
        unsigned int nMaskCode = (nCode / 8) + ((nCode & 6) != 0 ? 0 : 1);
        // spentness bitmask
        while (nMaskCode > 0) {
            unsigned char chAvail = 0;
            ::Unserialize(s, chAvail);
            for (unsigned int p = 0; p < 8; p++) {
                bool f = (chAvail & (1 << p)) != 0;
                vAvail.push_back(f);
            }
            if (chAvail != 0)
                nMaskCode--;
        }
        // txouts themself
        vout.assign(vAvail.size(), CTxOut());
        for (unsigned int i = 0; i < vAvail.size(); i++) {
            if (vAvail[i])
                ::Unserialize(s, REF(CTxOutCompressorLegacy(vout[i])));
        }
        // coinbase height
        ::Unserialize(s, VARINT(nHeight));
    }
};

}


// For non backwards compatible/major upgrades we return true here and force the entire database to be regenerated.
bool CCoinsViewDB::RequiresReindex()
{
    if (db.Exists(DB_VERSION))
    {
        db.Read(DB_VERSION, nPreviousVersion);
    }
    
    // PHASE4 related update, store transaction index as part of output database.
    if (nPreviousVersion < 2)
    {
        return true;
    }
        
    return false;
}
    
// Minor upgrades take place inside here, for major upgrades see 'RequiresReindex'
bool CCoinsViewDB::Upgrade()
{
    // Any future upgrade code goes here.
    // NB! version number should be upgraded at end of upgrade
    return true;
}

bool CCoinsViewDB::WriteVersion()
{
    std::unique_ptr<CDBIterator> pcursor(db.NewIterator());
    pcursor->Seek(std::pair(DB_COINS, uint256()));
    
    if (!pcursor->Valid())
    {
        db.Write(DB_VERSION, (uint32_t)nCurrentVersion);
        return true;
    }
    return true;
}

