// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// File contains modifications by: The Gulden developers
// All modifications:
// Copyright (c) 2016-2017 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#ifndef BITCOIN_WALLET_WALLET_H
#define BITCOIN_WALLET_WALLET_H

#include "amount.h"
#include "streams.h"
#include "tinyformat.h"
#include "ui_interface.h"
#include "utilstrencodings.h"
#include "validationinterface.h"
#include "script/ismine.h"
#include "script/sign.h"
#include "wallet/crypter.h"
#include "wallet/walletdb.h"
#include "wallet/rpcwallet.h"
#include "wallet/walletdberrors.h"
#include "account.h"

#include <algorithm>
#include <atomic>
#include <map>
#include <set>
#include <stdexcept>
#include <stdint.h>
#include <string>
#include <utility>
#include <vector>

#include <boost/foreach.hpp>
#include <boost/thread.hpp>
extern CWallet* pwalletMain;

void StartShadowPoolManagerThread(boost::thread_group& threadGroup);

//fixme: GULDEN Make configurable option
extern bool fShowChildAccountsSeperately;

/**
 * Settings
 */
extern CFeeRate payTxFee;
extern unsigned int nTxConfirmTarget;
extern bool bSpendZeroConfChange;
extern bool fWalletRbf;

static const unsigned int DEFAULT_KEYPOOL_SIZE = 100;
//! -paytxfee default
static const CAmount DEFAULT_TRANSACTION_FEE = 0;
//! -fallbackfee default
static const CAmount DEFAULT_FALLBACK_FEE = 20000;
//! -mintxfee default
static const CAmount DEFAULT_TRANSACTION_MINFEE = 1000000;
//! minimum recommended increment for BIP 125 replacement txs
static const CAmount WALLET_INCREMENTAL_RELAY_FEE = 5000;
//! target minimum change amount
static const CAmount MIN_CHANGE = CENT;
//! final minimum change amount after paying for fees
static const CAmount MIN_FINAL_CHANGE = MIN_CHANGE/2;
//! Default for -spendzeroconfchange
static const bool DEFAULT_SPEND_ZEROCONF_CHANGE = true;
//! Default for -walletrejectlongchains
static const bool DEFAULT_WALLET_REJECT_LONG_CHAINS = false;
//! -txconfirmtarget default
static const unsigned int DEFAULT_TX_CONFIRM_TARGET = 6;
//! -walletrbf default
static const bool DEFAULT_WALLET_RBF = false;
static const bool DEFAULT_WALLETBROADCAST = true;
static const bool DEFAULT_DISABLE_WALLET = false;
//! if set, all keys will be derived by using BIP32
static const bool DEFAULT_USE_HD_WALLET = true;

extern const char * DEFAULT_WALLET_DAT;

class CBlockIndex;
class CCoinControl;
class COutput;
class CReserveKey;
class CScript;
class CScheduler;
class CTxMemPool;
class CWalletTx;
class CWalletDB;

/** (client) version numbers for particular wallet features */
enum WalletFeature
{
    FEATURE_BASE = 10500, // the earliest version new wallets supports (only useful for getinfo's clientversion output)

    FEATURE_WALLETCRYPT = 40000, // wallet encryption
    FEATURE_COMPRPUBKEY = 60000, // compressed public keys

    FEATURE_HD = 130000, // Hierarchical key derivation after BIP32 (HD Wallet)
    FEATURE_LATEST = FEATURE_COMPRPUBKEY // HD is optional, use FEATURE_COMPRPUBKEY as latest version
};


/** A key pool entry */
class CKeyPool
{
public:
    int64_t nTime;
    CPubKey vchPubKey;
    std::string accountName;
    int64_t nChain;//internal or external keypool (HD wallets)

    CKeyPool();
    CKeyPool(const CPubKey& vchPubKeyIn, const std::string& accountNameIn, int64_t nChainIn);

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        int nVersion = s.GetVersion();
        if (!(s.GetType() & SER_GETHASH))
            READWRITE(nVersion);
        READWRITE(nTime);
        READWRITE(vchPubKey);
        //Allow to fail for legacy accounts.
        try
        {
            READWRITE(accountName);
            READWRITE(nChain);
        }
        catch(...)
        {
        }
    }
};

/** Address book data */
class CAddressBookData
{
public:
    std::string name;
    std::string purpose;

    CAddressBookData()
    {
        purpose = "unknown";
    }

    typedef std::map<std::string, std::string> StringMap;
    StringMap destdata;
};

struct CRecipient
{
    CScript scriptPubKey;
    CAmount nAmount;
    bool fSubtractFeeFromAmount;
};

typedef std::map<std::string, std::string> mapValue_t;


static inline void ReadOrderPos(int64_t& nOrderPos, mapValue_t& mapValue)
{
    if (!mapValue.count("n"))
    {
        nOrderPos = -1; // TODO: calculate elsewhere
        return;
    }
    nOrderPos = atoi64(mapValue["n"].c_str());
}


static inline void WriteOrderPos(const int64_t& nOrderPos, mapValue_t& mapValue)
{
    if (nOrderPos == -1)
        return;
    mapValue["n"] = i64tostr(nOrderPos);
}

struct COutputEntry
{
    CTxDestination destination;
    CAmount amount;
    int vout;
};

/** A transaction with a merkle branch linking it to the block chain. */
class CMerkleTx
{
private:
  /** Constant used in hashBlock to indicate tx has been abandoned */
    static const uint256 ABANDON_HASH;

public:
    CTransactionRef tx;
    uint256 hashBlock;

    /* An nIndex == -1 means that hashBlock (in nonzero) refers to the earliest
     * block in the chain we know this or any in-wallet dependency conflicts
     * with. Older clients interpret nIndex == -1 as unconfirmed for backward
     * compatibility.
     */
    int nIndex;

    CMerkleTx()
    {
        SetTx(MakeTransactionRef());
        Init();
    }

    CMerkleTx(CTransactionRef arg)
    {
        SetTx(std::move(arg));
        Init();
    }

    /** Helper conversion operator to allow passing CMerkleTx where CTransaction is expected.
     *  TODO: adapt callers and remove this operator. */
    operator const CTransaction&() const { return *tx; }

    void Init()
    {
        hashBlock = uint256();
        nIndex = -1;
    }

    void SetTx(CTransactionRef arg)
    {
        tx = std::move(arg);
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        std::vector<uint256> vMerkleBranch; // For compatibility with older versions.
        READWRITE(tx);
        READWRITE(hashBlock);
        READWRITE(vMerkleBranch);
        READWRITE(nIndex);
    }

    void SetMerkleBranch(const CBlockIndex* pIndex, int posInBlock);

    /**
     * Return depth of transaction in blockchain:
     * <0  : conflicts with a transaction this deep in the blockchain
     *  0  : in memory pool, waiting to be included in a block
     * >=1 : this many blocks deep in the main chain
     */
    int GetDepthInMainChain(const CBlockIndex* &pindexRet) const;
    int GetDepthInMainChain() const { const CBlockIndex *pindexRet; return GetDepthInMainChain(pindexRet); }
    bool IsInMainChain() const { const CBlockIndex *pindexRet; return GetDepthInMainChain(pindexRet) > 0; }
    int GetBlocksToMaturity() const;
    /** Pass this transaction to the mempool. Fails if absolute fee exceeds absurd fee. */
    bool AcceptToMemoryPool(const CAmount& nAbsurdFee, CValidationState& state);
    bool hashUnset() const { return (hashBlock.IsNull() || hashBlock == ABANDON_HASH); }
    bool isAbandoned() const { return (hashBlock == ABANDON_HASH); }
    void setAbandoned() { hashBlock = ABANDON_HASH; }

    const uint256& GetHash() const { return tx->GetHash(); }
    bool IsCoinBase() const { return tx->IsCoinBase(); }
};

/** 
 * A transaction with a bunch of additional info that only the owner cares about.
 * It includes any unrecorded transactions needed to link it back to the block chain.
 */
class CWalletTx : public CMerkleTx
{
private:
    const CWallet* pwallet;

public:
    /**
     * Key/value map with information about the transaction.
     *
     * The following keys can be read and written through the map and are
     * serialized in the wallet database:
     *
     *     "comment", "to"   - comment strings provided to sendtoaddress,
     *                         sendfrom, sendmany wallet RPCs
     *     "replaces_txid"   - txid (as HexStr) of transaction replaced by
     *                         bumpfee on transaction created by bumpfee
     *     "replaced_by_txid" - txid (as HexStr) of transaction created by
     *                         bumpfee on transaction replaced by bumpfee
     *     "from", "message" - obsolete fields that could be set in UI prior to
     *                         2011 (removed in commit 4d9b223)
     *
     * The following keys are serialized in the wallet database, but shouldn't
     * be read or written through the map (they will be temporarily added and
     * removed from the map during serialization):
     *
     *     "fromaccount"     - serialized strFromAccount value
     *     "n"               - serialized nOrderPos value
     *     "timesmart"       - serialized nTimeSmart value
     *     "spent"           - serialized vfSpent value that existed prior to
     *                         2014 (removed in commit 93a18a3)
     */
    mapValue_t mapValue;
    std::vector<std::pair<std::string, std::string> > vOrderForm;
    unsigned int fTimeReceivedIsTxTime;
    unsigned int nTimeReceived; //!< time received by this node
    /**
     * Stable timestamp that never changes, and reflects the order a transaction
     * was added to the wallet. Timestamp is based on the block time for a
     * transaction added as part of a block, or else the time when the
     * transaction was received if it wasn't part of a block, with the timestamp
     * adjusted in both cases so timestamp order matches the order transactions
     * were added to the wallet. More details can be found in
     * CWallet::ComputeTimeSmart().
     */
    unsigned int nTimeSmart;
    /**
     * From me flag is set to 1 for transactions that were created by the wallet
     * on this bitcoin node, and set to 0 for transactions that were created
     * externally and came in through the network or sendrawtransaction RPC.
     */
    char fFromMe;
    std::string strFromAccount;
    int64_t nOrderPos; //!< position in ordered transaction list

    // memory only
    mutable bool fDebitCached;
    mutable bool fCreditCached;
    mutable bool fImmatureCreditCached;
    mutable bool fAvailableCreditCached;
    //fixme: (GULDEN) (SBSU) - Caching would be faster but seems to become invalid if we do internal transfers between accounts.
    //mutable std::map<const CAccount*, CAmount> availableCreditForAccountCached;
    mutable bool fWatchDebitCached;
    mutable bool fWatchCreditCached;
    mutable bool fImmatureWatchCreditCached;
    mutable bool fAvailableWatchCreditCached;
    mutable bool fChangeCached;
    mutable CAmount nDebitCached;
    mutable CAmount nCreditCached;
    mutable CAmount nImmatureCreditCached;
    mutable CAmount nAvailableCreditCached;
    mutable CAmount nWatchDebitCached;
    mutable CAmount nWatchCreditCached;
    mutable CAmount nImmatureWatchCreditCached;
    mutable CAmount nAvailableWatchCreditCached;
    mutable CAmount nChangeCached;

    CWalletTx()
    {
        Init(NULL);
    }

    CWalletTx(const CWallet* pwalletIn, CTransactionRef arg) : CMerkleTx(std::move(arg))
    {
        Init(pwalletIn);
    }

    void Init(const CWallet* pwalletIn)
    {
        pwallet = pwalletIn;
        mapValue.clear();
        vOrderForm.clear();
        fTimeReceivedIsTxTime = false;
        nTimeReceived = 0;
        nTimeSmart = 0;
        fFromMe = false;
        strFromAccount.clear();
        fDebitCached = false;
        fCreditCached = false;
        fImmatureCreditCached = false;
        fAvailableCreditCached = false;
        fWatchDebitCached = false;
        fWatchCreditCached = false;
        fImmatureWatchCreditCached = false;
        fAvailableWatchCreditCached = false;
        fChangeCached = false;
        nDebitCached = 0;
        nCreditCached = 0;
        nImmatureCreditCached = 0;
        nAvailableCreditCached = 0;
        nWatchDebitCached = 0;
        nWatchCreditCached = 0;
        nAvailableWatchCreditCached = 0;
        nImmatureWatchCreditCached = 0;
        nChangeCached = 0;
        nOrderPos = -1;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        if (ser_action.ForRead())
            Init(NULL);
        char fSpent = false;

        if (!ser_action.ForRead())
        {
            mapValue["fromaccount"] = strFromAccount;

            WriteOrderPos(nOrderPos, mapValue);

            if (nTimeSmart)
                mapValue["timesmart"] = strprintf("%u", nTimeSmart);
        }

        READWRITE(*(CMerkleTx*)this);
        std::vector<CMerkleTx> vUnused; //!< Used to be vtxPrev
        READWRITE(vUnused);
        READWRITE(mapValue);
        READWRITE(vOrderForm);
        READWRITE(fTimeReceivedIsTxTime);
        READWRITE(nTimeReceived);
        READWRITE(fFromMe);
        READWRITE(fSpent);

        if (ser_action.ForRead())
        {
            strFromAccount = mapValue["fromaccount"];

            ReadOrderPos(nOrderPos, mapValue);

            nTimeSmart = mapValue.count("timesmart") ? (unsigned int)atoi64(mapValue["timesmart"]) : 0;
        }

        mapValue.erase("fromaccount");
        mapValue.erase("spent");
        mapValue.erase("n");
        mapValue.erase("timesmart");
    }

    //! make sure balances are recalculated
    void MarkDirty()
    {
        fCreditCached = false;
        fAvailableCreditCached = false;
        fImmatureCreditCached = false;
        fWatchDebitCached = false;
        fWatchCreditCached = false;
        fAvailableWatchCreditCached = false;
        fImmatureWatchCreditCached = false;
        fDebitCached = false;
        fChangeCached = false;
    }

    void BindWallet(CWallet *pwalletIn)
    {
        pwallet = pwalletIn;
        MarkDirty();
    }

    //! filter decides which addresses will count towards the debit
    CAmount GetDebit(const isminefilter& filter) const;
    CAmount GetCredit(const isminefilter& filter) const;
    CAmount GetImmatureCredit(bool fUseCache=true, const CAccount* forAccount=NULL) const;
    CAmount GetAvailableCredit(bool fUseCache=true, const CAccount* forAccount=NULL) const;
    CAmount GetImmatureWatchOnlyCredit(const bool& fUseCache=true) const;
    CAmount GetAvailableWatchOnlyCredit(const bool& fUseCache=true) const;
    CAmount GetChange() const;

    void GetAmounts(std::list<COutputEntry>& listReceived,
                    std::list<COutputEntry>& listSent, CAmount& nFee, const isminefilter& filter, CKeyStore* from=NULL) const;

    void GetAccountAmounts(const std::string& strAccount, CAmount& nReceived,
                           CAmount& nSent, CAmount& nFee, const isminefilter& filter) const;

    bool IsFromMe(const isminefilter& filter) const
    {
        return (GetDebit(filter) > 0);
    }

    // True if only scriptSigs are different
    bool IsEquivalentTo(const CWalletTx& tx) const;

    bool InMempool() const;
    bool IsTrusted() const;

    int64_t GetTxTime() const;
    int GetRequestCount() const;

    bool RelayWalletTransaction(CConnman* connman);

    std::set<uint256> GetConflicts() const;
};


class CInputCoin {
public:
    CInputCoin(const CWalletTx* walletTx, unsigned int i)
    {
        if (!walletTx)
            throw std::invalid_argument("walletTx should not be null");
        if (i >= walletTx->tx->vout.size())
            throw std::out_of_range("The output index is out of range");

        outpoint = COutPoint(walletTx->GetHash(), i);
        txout = walletTx->tx->vout[i];
    }

    COutPoint outpoint;
    CTxOut txout;

    bool operator<(const CInputCoin& rhs) const {
        return outpoint < rhs.outpoint;
    }

    bool operator!=(const CInputCoin& rhs) const {
        return outpoint != rhs.outpoint;
    }

    bool operator==(const CInputCoin& rhs) const {
        return outpoint == rhs.outpoint;
    }
};

class COutput
{
public:
    const CWalletTx *tx;
    int i;
    int nDepth;

    /** Whether we have the private keys to spend this output */
    bool fSpendable;

    /** Whether we know how to spend this output, ignoring the lack of keys */
    bool fSolvable;

    /**
     * Whether this output is considered safe to spend. Unconfirmed transactions
     * from outside keys and unconfirmed replacement transactions are considered
     * unsafe and will not be used to fund new spending transactions.
     */
    bool fSafe;

    COutput(const CWalletTx *txIn, int iIn, int nDepthIn, bool fSpendableIn, bool fSolvableIn, bool fSafeIn)
    {
        tx = txIn; i = iIn; nDepth = nDepthIn; fSpendable = fSpendableIn; fSolvable = fSolvableIn; fSafe = fSafeIn;
    }

    std::string ToString() const;
};




/** Private key that includes an expiration date in case it never gets used. */
class CWalletKey
{
public:
    CPrivKey vchPrivKey;
    int64_t nTimeCreated;
    int64_t nTimeExpires;
    std::string strComment;
    //! todo: add something to note what created it (user, getnewaddress, change)
    //!   maybe should have a map<string, string> property map

    CWalletKey(int64_t nExpires=0);

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        int nVersion = s.GetVersion();
        if (!(s.GetType() & SER_GETHASH))
            READWRITE(nVersion);
        READWRITE(vchPrivKey);
        READWRITE(nTimeCreated);
        READWRITE(nTimeExpires);
        READWRITE(LIMITED_STRING(strComment, 65536));
    }
};

/**
 * Internal transfers.
 * Database key is acentry<account><counter>.
 */
class CAccountingEntry
{
public:
    std::string strAccount;
    CAmount nCreditDebit;
    int64_t nTime;
    std::string strOtherAccount;
    std::string strComment;
    mapValue_t mapValue;
    int64_t nOrderPos; //!< position in ordered transaction list
    uint64_t nEntryNo;

    CAccountingEntry()
    {
        SetNull();
    }

    void SetNull()
    {
        nCreditDebit = 0;
        nTime = 0;
        strAccount.clear();
        strOtherAccount.clear();
        strComment.clear();
        nOrderPos = -1;
        nEntryNo = 0;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        int nVersion = s.GetVersion();
        if (!(s.GetType() & SER_GETHASH))
            READWRITE(nVersion);
        //! Note: strAccount is serialized as part of the key, not here.
        READWRITE(nCreditDebit);
        READWRITE(nTime);
        READWRITE(LIMITED_STRING(strOtherAccount, 65536));

        if (!ser_action.ForRead())
        {
            WriteOrderPos(nOrderPos, mapValue);

            if (!(mapValue.empty() && _ssExtra.empty()))
            {
                CDataStream ss(s.GetType(), s.GetVersion());
                ss.insert(ss.begin(), '\0');
                ss << mapValue;
                ss.insert(ss.end(), _ssExtra.begin(), _ssExtra.end());
                strComment.append(ss.str());
            }
        }

        READWRITE(LIMITED_STRING(strComment, 65536));

        size_t nSepPos = strComment.find("\0", 0, 1);
        if (ser_action.ForRead())
        {
            mapValue.clear();
            if (std::string::npos != nSepPos)
            {
                CDataStream ss(std::vector<char>(strComment.begin() + nSepPos + 1, strComment.end()), s.GetType(), s.GetVersion());
                ss >> mapValue;
                _ssExtra = std::vector<char>(ss.begin(), ss.end());
            }
            ReadOrderPos(nOrderPos, mapValue);
        }
        if (std::string::npos != nSepPos)
            strComment.erase(nSepPos);

        mapValue.erase("n");
    }

private:
    std::vector<char> _ssExtra;
};


isminetype IsMine(const CWallet &wallet, const CTxDestination& dest);
isminetype IsMine(const CWallet &wallet, const CScript& scriptPubKey);
isminetype RemoveAddressFromKeypoolIfIsMine(CWallet &wallet, const CScript& scriptPubKey, uint64_t time);

/** 
 * A CWallet maintains a set of transactions and balances
 * and provides the ability to create new transactions.
 * it containes one or more accounts, which are responsible for creating/allocating/managing keys via their keystore interfaces.
 */
class CWallet : public CValidationInterface
{
private:
    static std::atomic<bool> fFlushScheduled;
    std::atomic<bool> fAbortRescan;
    std::atomic<bool> fScanningWallet;

    /**
     * Select a set of coins such that nValueRet >= nTargetValue and at least
     * all coins from coinControl are selected; Never select unconfirmed coins
     * if they are not ours
     */
    bool SelectCoins(const std::vector<COutput>& vAvailableCoins, const CAmount& nTargetValue, std::set<CInputCoin>& setCoinsRet, CAmount& nValueRet, const CCoinControl *coinControl = NULL) const;

    CWalletDB *pwalletdbEncryption;

    //! the current wallet version: clients below this version are not able to load the wallet
    int nWalletVersion;

    //! the maximum wallet format version: memory-only variable that specifies to what version this wallet may be upgraded
    int nWalletMaxVersion;

    int64_t nNextResend;
    int64_t nLastResend;
    bool fBroadcastTransactions;

    /**
     * Used to keep track of spent outpoints, and
     * detect and report conflicts (double-spends or
     * mutated transactions where the mutant gets mined).
     */
    typedef std::multimap<COutPoint, uint256> TxSpends;
    TxSpends mapTxSpends;
    void AddToSpends(const COutPoint& outpoint, const uint256& wtxid);
    void AddToSpends(const uint256& wtxid);

    /* Mark a transaction (and its in-wallet descendants) as conflicting with a particular block. */
    void MarkConflicted(const uint256& hashBlock, const uint256& hashTx);

    void SyncMetaData(std::pair<TxSpends::iterator, TxSpends::iterator>);

    /* Used by TransactionAddedToMemorypool/BlockConnected/Disconnected.
     * Should be called with pindexBlock and posInBlock if this is for a transaction that is included in a block. */
    void SyncTransaction(const CTransactionRef& tx, const CBlockIndex *pindex = NULL, int posInBlock = 0);

    bool fFileBacked;

    std::set<int64_t> setKeyPool;
    // Used to NotifyTransactionChanged of the previous block's coinbase when
    // the next block comes in
    uint256 hashPrevBestCoinbase;

public:
    /*
     * Main wallet lock.
     * This lock protects all the fields added by CWallet
     *   except for:
     *      fFileBacked (immutable after instantiation)
     *      strWalletFile (immutable after instantiation)
     */
    mutable CCriticalSection cs_wallet;

    const std::string strWalletFile;

    void LoadKeyPool(int nIndex, const CKeyPool &keypool)
    {
        setKeyPool.insert(nIndex);

        // If no metadata exists yet, create a default with the pool key's
        // creation time. Note that this may be overwritten by actually
        // stored metadata for that key later, which is fine.
        CKeyID keyid = keypool.vchPubKey.GetID();
        if (mapKeyMetadata.count(keyid) == 0)
            mapKeyMetadata[keyid] = CKeyMetadata(keypool.nTime);
    }

    std::map<CKeyID, CKeyMetadata> mapKeyMetadata;

    typedef std::map<unsigned int, CMasterKey> MasterKeyMap;
    MasterKeyMap mapMasterKeys;
    unsigned int nMasterKeyMaxID;

    CWallet()
    {
        SetNull();
    }

    CWallet(const std::string& strWalletFileIn) : strWalletFile(strWalletFileIn)
    {
        SetNull();
        fFileBacked = true;
    }

    virtual ~CWallet();

    void SetNull()
    {
        delayLock=false;
        wantDelayLock=false;
        didDelayLock=false;
        nWalletVersion = FEATURE_BASE;
        nWalletMaxVersion = FEATURE_BASE;
        fFileBacked = false;
        nMasterKeyMaxID = 0;
        pwalletdbEncryption = NULL;
        nOrderPosNext = 0;
        nNextResend = 0;
        nLastResend = 0;
        nTimeFirstKey = 0;
        fBroadcastTransactions = false;
        nRelockTime = 0;
        fAbortRescan = false;
        fScanningWallet = false;
        activeAccount = NULL;
        activeSeed = NULL;
    }
    
    
    // The 'shadow pool thread' sets delay lock true if it had a backlog of work it wants to do on the unlocked wallet
    bool delayLock;
    bool wantDelayLock;
    mutable bool didDelayLock;
    bool Lock() const
    {
        LOCK(cs_wallet);
        if (delayLock || wantDelayLock)
        {
            didDelayLock = true;
            return true;
        }
        
        bool ret = true;
        for (auto accountPair : mapAccounts)
        {
            if (!accountPair.second->Lock())
                ret = false;
        }
        for (auto seedPair : mapSeeds)
        {
            if (!seedPair.second->Lock())
                ret = false;
        }
        return ret;
    }
    
    bool Unlock(const CKeyingMaterial& vMasterKeyIn) const
    {
        LOCK(cs_wallet);
        bool ret = true;
        for (auto accountPair : mapAccounts)
        {
            if (!accountPair.second->Unlock(vMasterKeyIn))
                ret = false;
        }
        for (auto seedPair : mapSeeds)
        {
            if (!seedPair.second->Unlock(vMasterKeyIn))
                ret = false;
        }
        return ret;
    }
    
    bool IsCrypted() const
    {
        for (auto accountPair : mapAccounts)
        {
            if(accountPair.second->IsCrypted())
                return true;
        }
        for (auto seedPair : mapSeeds)
        {
            if (seedPair.second->IsCrypted())
                return true;
        }
        return false;
    }
    
    
    bool IsLocked() const
    {
        for (auto accountPair : mapAccounts)
        {
            if(accountPair.second->IsLocked())
                return true;
        }
        for (auto seedPair : mapSeeds)
        {
            if (seedPair.second->IsLocked())
                return true;
        }
        return false;
    }
        
    bool GetKey(const CKeyID &address, CKey& keyOut) const
    {
        LOCK(cs_wallet);
        for (auto accountPair : mapAccounts)
        {
            if (accountPair.second->GetKey(address, keyOut))
                return true;
        }
        return false;
    }
    
    
    bool GetPubKey(const CKeyID &address, CPubKey& vchPubKeyOut)
    {
        LOCK(cs_wallet);
        for (auto accountPair : mapAccounts)
        {
            if (accountPair.second->GetPubKey(address, vchPubKeyOut))
                return true;
        }
        return false;
    }
    
    
    bool HaveWatchOnly(const CScript &dest) const
    {
        LOCK(cs_wallet);
        for (auto accountPair : mapAccounts)
        {
            if (accountPair.second->HaveWatchOnly(dest))
                return true;
        }
        return false;
    }
    
    bool HaveWatchOnly() const
    {
        LOCK(cs_wallet);
        for (auto accountPair : mapAccounts)
        {
            if (accountPair.second->HaveWatchOnly())
                return true;
        }
        return false;
    }
    
    bool HaveCScript(const CScriptID &hash)
    {
        LOCK(cs_wallet);
        for (auto accountPair : mapAccounts)
        {
            if (accountPair.second->HaveCScript(hash))
                return true;
        }
        return false;
    }
    
    bool GetCScript(const CScriptID &hash, CScript& redeemScriptOut)
    {
        LOCK(cs_wallet);
        for (auto accountPair : mapAccounts)
        {
            if (accountPair.second->GetCScript(hash, redeemScriptOut))
                return true;
        }
        return false;
    }
    
    
    bool HaveKey(const CKeyID &address) const
    {
        LOCK(cs_wallet);
        for (auto accountPair : mapAccounts)
        {
            if (accountPair.second->HaveKey(address))
            return true;
        }
        return false;
    }
    
    bool HaveWatchOnly(const CScript &dest)
    {
        LOCK(cs_wallet);
        for (auto accountPair : mapAccounts)
        {
            if (accountPair.second->HaveWatchOnly(dest))
            return true;
        }
        return false;
    }

    std::map<uint256, CWalletTx> mapWallet;
    std::list<CAccountingEntry> laccentries;
    
    
    std::map<std::string, CHDSeed*> mapSeeds;
    std::map<std::string, CAccount*> mapAccounts;
    std::map<std::string, std::string> mapAccountLabels;
    
    void changeAccountName(CAccount* account, const std::string& newName, bool notify=true);
    void addAccount(CAccount* account, const std::string& newName);
    void deleteAccount(CAccount* account);
    
    

    typedef std::pair<CWalletTx*, CAccountingEntry*> TxPair;
    typedef std::multimap<int64_t, TxPair > TxItems;
    TxItems wtxOrdered;

    int64_t nOrderPosNext;
    std::map<uint256, int> mapRequestCount;

    std::map<std::string, CAddressBookData> mapAddressBook;
/* GULDEN - no default key (accounts)
    CPubKey vchDefaultKey;
*/

    std::set<COutPoint> setLockedCoins;

    int64_t nTimeFirstKey;

    const CWalletTx* GetWalletTx(const uint256& hash) const;

    //! check whether we are allowed to upgrade (or already support) to the named feature
    bool CanSupportFeature(enum WalletFeature wf) { AssertLockHeld(cs_wallet); return nWalletMaxVersion >= wf; }

    /**
     * populate vCoins with vector of available COutputs.
     */
    void AvailableCoins(CAccount* forAccount, std::vector<COutput>& vCoins, bool fOnlySafe=true, const CCoinControl *coinControl = NULL, bool fIncludeZeroValue=false) const;

    /**
     * Shuffle and select coins until nTargetValue is reached while avoiding
     * small change; This method is stochastic for some inputs and upon
     * completion the coin set and corresponding actual target value is
     * assembled
     */
    bool SelectCoinsMinConf(const CAmount& nTargetValue, int nConfMine, int nConfTheirs, uint64_t nMaxAncestors, std::vector<COutput> vCoins, std::set<CInputCoin>& setCoinsRet, CAmount& nValueRet) const;

    bool IsSpent(const uint256& hash, unsigned int n) const;

    bool IsLockedCoin(uint256 hash, unsigned int n) const;
    void LockCoin(const COutPoint& output);
    void UnlockCoin(const COutPoint& output);
    void UnlockAllCoins();
    void ListLockedCoins(std::vector<COutPoint>& vOutpts);

    /*
     * Rescan abort properties
     */
    void AbortRescan() { fAbortRescan = true; }
    bool IsAbortingRescan() { return fAbortRescan; }
    bool IsScanning() { return fScanningWallet; }

    /**
     * keystore implementation
     * Generate a new key
     */
    CPubKey GenerateNewKey(CAccount& forAccount, int keyChain);
    void DeriveNewChildKey(CKeyMetadata& metadata, CKey& secret);
    //! Adds a key to the store, and saves it to disk.
    bool AddKeyPubKey(const CKey& key, const CPubKey &pubkey, CAccount& forAccount, int keyChain);
    //! for wallet upgrade
    void ForceRewriteKeys(CAccount& forAccount);
    bool AddKeyPubKey(int64_t HDKeyIndex, const CPubKey &pubkey, CAccount& forAccount, int keyChain);
    //! Adds a key to the store, without saving it to disk (used by LoadWallet)
    bool LoadKey(const CKey& key, const CPubKey &pubkey, const std::string& forAccount, int64_t nKeyChain)
    {
        LOCK(cs_wallet);
        return mapAccounts[forAccount]->AddKeyPubKey(key, pubkey, nKeyChain);
    }
    bool LoadKey(int64_t HDKeyIndex, int64_t keyChain, const CPubKey &pubkey, const std::string& forAccount)
    {
        LOCK(cs_wallet);
        return mapAccounts[forAccount]->AddKeyPubKey(HDKeyIndex, pubkey, keyChain);
    }
    //! Load metadata (used by LoadWallet)
    bool LoadKeyMetadata(const CPubKey &pubkey, const CKeyMetadata &metadata);

    bool LoadMinVersion(int nVersion) { AssertLockHeld(cs_wallet); nWalletVersion = nVersion; nWalletMaxVersion = std::max(nWalletMaxVersion, nVersion); return true; }
    void UpdateTimeFirstKey(int64_t nCreateTime);

    //! Adds an encrypted key to the store, without saving it to disk (used by LoadWallet)
    bool LoadCryptedKey(const CPubKey &vchPubKey, const std::vector<unsigned char> &vchCryptedSecret, const std::string& forAccount, int64_t nKeyChain);
    bool AddCScript(const CScript& redeemScript);
    bool LoadCScript(const CScript& redeemScript);

    //! Adds a destination data tuple to the store, and saves it to disk
    bool AddDestData(const CTxDestination &dest, const std::string &key, const std::string &value);
    //! Erases a destination data tuple in the store and on disk
    bool EraseDestData(const CTxDestination &dest, const std::string &key);
    //! Adds a destination data tuple to the store, without saving it to disk
    bool LoadDestData(const CTxDestination &dest, const std::string &key, const std::string &value);
    //! Look up a destination data tuple in the store, return true if found false otherwise
    bool GetDestData(const CTxDestination &dest, const std::string &key, std::string *value) const;

    //! Adds a watch-only address to the store, and saves it to disk.
    bool AddWatchOnly(const CScript &dest, int64_t nCreateTime);
    bool RemoveWatchOnly(const CScript &dest);
    //! Adds a watch-only address to the store, without saving it to disk (used by LoadWallet)
    bool LoadWatchOnly(const CScript &dest);

    //! Holds a timestamp at which point the wallet is scheduled (externally) to be relocked. Caller must arrange for actual relocking to occur via Lock().
    int64_t nRelockTime;

    bool Unlock(const SecureString& strWalletPassphrase);
    bool ChangeWalletPassphrase(const SecureString& strOldWalletPassphrase, const SecureString& strNewWalletPassphrase);
    bool EncryptWallet(const SecureString& strWalletPassphrase);

    void GetKeyBirthTimes(std::map<CKeyID, int64_t> &mapKeyBirth) const;
    unsigned int ComputeTimeSmart(const CWalletTx& wtx) const;

    /** 
     * Increment the next transaction order id
     * @return next transaction order id
     */
    int64_t IncOrderPosNext(CWalletDB *pwalletdb = NULL);
    DBErrors ReorderTransactions();
    bool AccountMove(std::string strFrom, std::string strTo, CAmount nAmount, std::string strComment = "");
    bool GetAccountPubkey(CPubKey &pubKey, std::string strAccount, bool bForceNew = false);
    
    CAccountHD* GenerateNewAccount(std::string strAccount, AccountType type, AccountSubType subType);
    CAccount* GenerateNewLegacyAccount(std::string strAccount);
    CAccountHD* CreateReadOnlyAccount(std::string strAccount, SecureString encExtPubKey);

    void MarkDirty();
    bool AddToWallet(const CWalletTx& wtxIn, bool fFlushOnClose=true);
    bool LoadToWallet(const CWalletTx& wtxIn);
    void TransactionAddedToMempool(const CTransactionRef& tx) override;
    void BlockConnected(const std::shared_ptr<const CBlock>& pblock, const CBlockIndex *pindex, const std::vector<CTransactionRef>& vtxConflicted) override;
    void BlockDisconnected(const std::shared_ptr<const CBlock>& pblock) override;
    bool AddToWalletIfInvolvingMe(const CTransactionRef& tx, const CBlockIndex* pIndex, int posInBlock, bool fUpdate);
    CBlockIndex* ScanForWalletTransactions(CBlockIndex* pindexStart, bool fUpdate = false);
    void ReacceptWalletTransactions();
    void ResendWalletTransactions(int64_t nBestBlockTime, CConnman* connman);
    std::vector<uint256> ResendWalletTransactionsBefore(int64_t nTime, CConnman* connman);
    CAmount GetBalance(const CAccount* forAccount = NULL) const;
    CAmount GetUnconfirmedBalance(const CAccount* forAccount = NULL) const;
    CAmount GetImmatureBalance(const CAccount* forAccount = NULL) const;
    CAmount GetWatchOnlyBalance() const;
    CAmount GetUnconfirmedWatchOnlyBalance() const;
    CAmount GetImmatureWatchOnlyBalance() const;

    /**
     * Insert additional inputs into the transaction by
     * calling CreateTransaction();
     */
    bool FundTransaction(CAccount* fundingAccount, CMutableTransaction& tx, CAmount& nFeeRet, bool overrideEstimatedFeeRate, const CFeeRate& specificFeeRate, int& nChangePosInOut, std::string& strFailReason, bool includeWatching, bool lockUnspents, const std::set<int>& setSubtractFeeFromOutputs, bool keepReserveKey = true, const CTxDestination& destChange = CNoDestination());
    bool SignTransaction(CMutableTransaction& tx);

    /**
     * Create a new transaction paying the recipients with a set of coins
     * selected by SelectCoins(); Also create the change output, when needed
     * @note passing nChangePosInOut as -1 will result in setting a random position
     */
    bool CreateTransaction(CAccount* forAccount, const std::vector<CRecipient>& vecSend, CWalletTx& wtxNew, CReserveKey& reservekey, CAmount& nFeeRet, int& nChangePosInOut,
                           std::string& strFailReason, const CCoinControl *coinControl = NULL, bool sign = true);
    bool CommitTransaction(CWalletTx& wtxNew, CReserveKey& reservekey, CConnman* connman, CValidationState& state);

    void ListAccountCreditDebit(const std::string& strAccount, std::list<CAccountingEntry>& entries);
    bool AddAccountingEntry(const CAccountingEntry&);
    bool AddAccountingEntry(const CAccountingEntry&, CWalletDB *pwalletdb);
    template <typename ContainerType>
    bool DummySignTx(CMutableTransaction &txNew, const ContainerType &coins) const;

    static CFeeRate minTxFee;
    static CFeeRate fallbackFee;
    /**
     * Estimate the minimum fee considering user set parameters
     * and the required fee
     */
    static CAmount GetMinimumFee(unsigned int nTxBytes, unsigned int nConfirmTarget, const CTxMemPool& pool);
    /**
     * Estimate the minimum fee considering required fee and targetFee or if 0
     * then fee estimation for nConfirmTarget
     */
    static CAmount GetMinimumFee(unsigned int nTxBytes, unsigned int nConfirmTarget, const CTxMemPool& pool, CAmount targetFee);
    /**
     * Return the minimum required fee taking into account the
     * floating relay fee and user set minimum transaction fee
     */
    static CAmount GetRequiredFee(unsigned int nTxBytes);

    bool NewKeyPool();
    int TopUpKeyPool(unsigned int kpSize = 0, unsigned int maxNew = 0);
    void ReserveKeyFromKeyPool(int64_t& nIndex, CKeyPool& keypool, CAccount* forAccount, int64_t keyChain);
    void KeepKey(int64_t nIndex);
    void MarkKeyUsed(CKeyID keyID, uint64_t usageTime);
    void ReturnKey(int64_t nIndex, CAccount* forAccount, int64_t keyChain);
    bool GetKeyFromPool(CPubKey &key, CAccount* forAccount, int64_t keyChain);
    int64_t GetOldestKeyPoolTime();
    void GetAllReserveKeys(std::set<CKeyID>& setAddress) const;

    std::set< std::set<CTxDestination> > GetAddressGroupings();
    std::map<CTxDestination, CAmount> GetAddressBalances();

    CAmount GetAccountBalance(const std::string& strAccount, int nMinDepth, const isminefilter& filter, bool includeChildren=false);
    CAmount GetAccountBalance(CWalletDB& walletdb, const std::string& strAccount, int nMinDepth, const isminefilter& filter, bool includeChildren=false);
    std::set<CTxDestination> GetAccountAddresses(const std::string& strAccount) const;

    isminetype IsMine(const CTxIn& txin) const;
    isminetype IsMine(const CKeyStore &keystore, const CTxIn& txin) const;
    void RemoveAddressFromKeypoolIfIsMine(const CTxIn& txin, uint64_t time);
    
    /**
     * Returns amount of debit if the input matches the
     * filter, otherwise returns 0
     */
    CAmount GetDebit(const CTxIn& txin, const isminefilter& filter) const;
    isminetype IsMine(const CTxOut& txout) const;
    void RemoveAddressFromKeypoolIfIsMine(const CTxOut& txout, uint64_t time);
    CAmount GetCredit(const CTxOut& txout, const isminefilter& filter) const;
    bool IsChange(const CTxOut& txout) const;
    CAmount GetChange(const CTxOut& txout) const;
    bool IsMine(const CTransaction& tx) const;
    void RemoveAddressFromKeypoolIfIsMine(const CTransaction& tx, uint64_t time);
    /** should probably be renamed to IsRelevantToMe */
    bool IsFromMe(const CTransaction& tx) const;
    CAmount GetDebit(const CTransaction& tx, const isminefilter& filter) const;
    /** Returns whether all of the inputs match the filter */
    bool IsAllFromMe(const CTransaction& tx, const isminefilter& filter) const;
    CAmount GetCredit(const CTransaction& tx, const isminefilter& filter) const;
    CAmount GetChange(const CTransaction& tx) const;
    void SetBestChain(const CBlockLocator& loc);

    DBErrors LoadWallet(bool& fFirstRunRet);
    DBErrors ZapWalletTx(std::vector<CWalletTx>& vWtx);
    DBErrors ZapSelectTx(std::vector<uint256>& vHashIn, std::vector<uint256>& vHashOut);

    bool SetAddressBook(const std::string& address, const std::string& strName, const std::string& purpose);

    bool DelAddressBook(const std::string& address);

    void Inventory(const uint256 &hash)
    {
        {
            LOCK(cs_wallet);
            std::map<uint256, int>::iterator mi = mapRequestCount.find(hash);
            if (mi != mapRequestCount.end())
                (*mi).second++;
        }
    }

    void GetScriptForMining(std::shared_ptr<CReserveScript> &script) override;
/*GULDEN - we don't use these (accounts)
    unsigned int GetKeyPoolSize()
    {
        AssertLockHeld(cs_wallet); // setKeyPool
        return setKeyPool.size();
    }

    bool SetDefaultKey(const CPubKey &vchPubKey);
*/
    //! signify that a particular wallet feature is now used. this may change nWalletVersion and nWalletMaxVersion if those are lower
    bool SetMinVersion(enum WalletFeature, CWalletDB* pwalletdbIn = NULL, bool fExplicit = false);

    //! change which version we're allowed to upgrade to (note that this does not immediately imply upgrading to that format)
    bool SetMaxVersion(int nVersion);

    //! get the current wallet format (the oldest client version guaranteed to understand this wallet)
    int GetVersion() { LOCK(cs_wallet); return nWalletVersion; }

    //! Get wallet transactions that conflict with given transaction (spend same outputs)
    std::set<uint256> GetConflicts(const uint256& txid) const;

    //! Check if a given transaction has any of its outputs spent by another transaction in the wallet
    bool HasWalletSpend(const uint256& txid) const;

    //! Flush wallet (bitdb flush)
    void Flush(bool shutdown=false);

    //! Verify the wallet database and perform salvage if required
    static bool Verify();
    
    /** 
     * Address book entry changed.
     * @note called with lock cs_wallet held.
     */
    boost::signals2::signal<void (CWallet *wallet, const std::string
            &address, const std::string &label, bool isMine,
            const std::string &purpose,
            ChangeType status)> NotifyAddressBookChanged;

    /** 
     * Wallet transaction added, removed or updated.
     * @note called with lock cs_wallet held.
     */
    boost::signals2::signal<void (CWallet *wallet, const uint256 &hashTx,
            ChangeType status)> NotifyTransactionChanged;

    /** Show progress e.g. for rescan */
    boost::signals2::signal<void (const std::string &title, int nProgress)> ShowProgress;

    /** Watch-only address added */
    boost::signals2::signal<void (bool fHaveWatchOnly)> NotifyWatchonlyChanged;
    
    // Account changed (name change)
    boost::signals2::signal<void (CWallet* wallet, CAccount* account)> NotifyAccountNameChanged;
    
    // New account added to the wallet
    boost::signals2::signal<void (CWallet* wallet, CAccount* account)> NotifyAccountAdded;
    
    // Account marked as deleted.
    boost::signals2::signal<void (CWallet* wallet, CAccount* account)> NotifyAccountDeleted;
    
    boost::signals2::signal<void (CWallet* wallet, CAccount* account)> NotifyUpdateAccountList;
    
    // Currently active account changed
    boost::signals2::signal<void (CWallet* wallet, CAccount* account)> NotifyActiveAccountChanged;
    
    /** Inquire whether this wallet broadcasts transactions. */
    bool GetBroadcastTransactions() const { return fBroadcastTransactions; }
    /** Set whether this wallet broadcasts transactions. */
    void SetBroadcastTransactions(bool broadcast) { fBroadcastTransactions = broadcast; }

    /* Mark a transaction (and it in-wallet descendants) as abandoned so its inputs may be respent. */
    bool AbandonTransaction(const uint256& hashTx);

    /** Mark a transaction as replaced by another transaction (e.g., BIP 125). */
    bool MarkReplaced(const uint256& originalHash, const uint256& newHash);

    /* Returns the wallets help message */
    static std::string GetWalletHelpString(bool showDebug);

    /* Initializes the wallet, returns a new CWallet instance or a null pointer in case of an error */
    static CWallet* CreateWalletFromFile(const std::string walletFile);
    static bool InitLoadWallet();

    /**
     * Wallet post-init setup
     * Gives the wallet a chance to register repetitive tasks and complete post-init tasks
     */
    void postInitProcess(CScheduler& scheduler);

    /* Wallets parameter interaction */
    static bool ParameterInteraction();

    bool BackupWallet(const std::string& strDest);

    void setActiveAccount(CAccount* newActiveAccount);
    CAccount* getActiveAccount();
    void setActiveSeed(CHDSeed* newActiveSeed);
    CHDSeed* GenerateHDSeed(CHDSeed::SeedType);
    void DeleteSeed(CHDSeed* deleteSeed, bool purge);
    CHDSeed* ImportHDSeed(SecureString mnemonic);
    CHDSeed* ImportHDSeedFromPubkey(SecureString pubKeyString);
    CHDSeed* getActiveSeed();
    
    //const CHDChain& GetHDChain() { return hdChain; }

    /* Returns true if HD is enabled */
    /*bool IsHDEnabled() const;*/

    /* Generates a new HD master key (will not be activated) */
    //CPubKey GenerateNewHDMasterKey();
    CAccount* activeAccount;
    CHDSeed* activeSeed;
        
    friend class CAccount;
};

/** A key allocated from the key pool. */
class CReserveKey : public CReserveScript
{
protected:
    CWallet* pwallet;
    CAccount* account;
    int64_t nIndex;
    int64_t nKeyChain;
    CPubKey vchPubKey;
public:
    CReserveKey(CWallet* pwalletIn, CAccount* forAccount, int64_t forKeyChain)
    {
        pwallet = pwalletIn;
        account = forAccount;
        nIndex = -1;
        nKeyChain = forKeyChain;
    }

    CReserveKey() = default;
    CReserveKey(const CReserveKey&) = delete;
    CReserveKey& operator=(const CReserveKey&) = delete;

    ~CReserveKey()
    {
        ReturnKey();
    }

    void ReturnKey();
    bool GetReservedKey(CPubKey &pubkey);
    void KeepKey();
    void KeepScript() { KeepKey(); }
};


/** 
 * Account information.
 * Stored in wallet with key "acc"+string account name.
 */
/* Gulden - we don't use this as we have our own real account system.
class CAccount
{
public:
    CPubKey vchPubKey;

    CAccount()
    {
        SetNull();
    }

    void SetNull()
    {
        vchPubKey = CPubKey();
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        int nVersion = s.GetVersion();
        if (!(s.GetType() & SER_GETHASH))
            READWRITE(nVersion);
        READWRITE(vchPubKey);
    }
};
*/
// Helper for producing a bunch of max-sized low-S signatures (eg 72 bytes)
// ContainerType is meant to hold pair<CWalletTx *, int>, and be iterable
// so that each entry corresponds to each vIn, in order.
template <typename ContainerType>
bool CWallet::DummySignTx(CMutableTransaction &txNew, const ContainerType &coins) const
{
    // Fill in dummy signatures for fee calculation.
    int nIn = 0;
    for (const auto& coin : coins)
    {
        const CScript& scriptPubKey = coin.txout.scriptPubKey;
        SignatureData sigdata;

	//gixme: (GULDEN) (MERGE)
        /*if (!ProduceSignature(DummySignatureCreator(this), scriptPubKey, sigdata))
        {
            return false;
        } else {
            UpdateTransaction(txNew, nIn, sigdata);
        }*/

        nIn++;
    }
    return true;
}
#endif // BITCOIN_WALLET_WALLET_H
