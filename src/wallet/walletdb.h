// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// File contains modifications by: The Gulden developers
// All modifications:
// Copyright (c) 2016-2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#ifndef GULDEN_WALLET_WALLETDB_H
#define GULDEN_WALLET_WALLETDB_H

#include "amount.h"
#include "primitives/transaction.h"
#include "wallet/db.h"
#include "wallet/walletdberrors.h"
#include "key.h"

#include <list>
#include <stdint.h>
#include <string>
#include <utility>
#include <vector>

/**
 * Overview of wallet database classes:
 *
 * - CDBEnv is an environment in which the database exists (has no analog in dbwrapper.h)
 * - CWalletDBWrapper represents a wallet database (similar to CDBWrapper in dbwrapper.h)
 * - CDB is a low-level database transaction (similar to CDBBatch in dbwrapper.h)
 * - CWalletDB is a modifier object for the wallet, and encapsulates a database
 *   transaction as well as methods to act on the database (no analog in
 *   dbwrapper.h)
 *
 * The latter two are named confusingly, in contrast to what the names CDB
 * and CWalletDB suggest they are transient transaction objects and don't
 * represent the database itself.
 */

static const bool DEFAULT_FLUSHWALLET = true;

class CHDSeed;
class CAccount;
class CAccountHD;
class CAccountingEntry;
struct CBlockLocator;
class CKeyPool;
class CMasterKey;
class CScript;
class CWallet;
class CWalletTx;
class uint160;
class uint256;

enum WalletLoadState
{
    NEW_WALLET,
    EXISTING_WALLET_OLDACCOUNTSYSTEM,
    EXISTING_WALLET
};

class CKeyMetadata
{
public:
    static const int VERSION_BASIC=1;
    static const int VERSION_WITH_HDDATA=10;
    static const int CURRENT_VERSION=VERSION_WITH_HDDATA;
    int nVersion;
    int64_t nCreateTime; // 0 means unknown
    std::string hdKeypath; //optional HD/bip32 keypath
    std::string hdAccountUUID; //uuid of the account used to derive this key

    CKeyMetadata()
    {
        SetNull();
    }
    CKeyMetadata(int64_t nCreateTime_)
    {
        SetNull();
        nCreateTime = nCreateTime_;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(this->nVersion);
        nVersion = this->nVersion;
        READWRITE(nCreateTime);
        if (this->nVersion >= VERSION_WITH_HDDATA)
        {
            READWRITE(hdKeypath);
            READWRITE(hdAccountUUID);
        }

    }

    void SetNull()
    {
        nVersion = CKeyMetadata::CURRENT_VERSION;
        nCreateTime = 0;
        hdKeypath.clear();
        hdAccountUUID = "";
    }
};

/** Access to the wallet database.
 * This should really be named CWalletDBBatch, as it represents a single transaction at the
 * database. It will be committed when the object goes out of scope.
 * Optionally (on by default) it will flush to disk as well.
 */
class CWalletDB
{
private:
    template <typename K, typename T>
    bool WriteIC(const K& key, const T& value, bool fOverwrite = true)
    {
        if (!batch.Write(key, value, fOverwrite)) {
            return false;
        }
        m_dbw.IncrementUpdateCounter();
        return true;
    }

    template <typename K>
    bool EraseIC(const K& key)
    {
        if (!batch.Erase(key)) {
            return false;
        }
        m_dbw.IncrementUpdateCounter();
        return true;
    }

public:
    CWalletDB(CWalletDBWrapper& dbw, const char* pszMode = "r+", bool _fFlushOnClose = true) :
        batch(dbw, pszMode, _fFlushOnClose),
        m_dbw(dbw)
    {
    }

    bool WriteName(const std::string& strAddress, const std::string& strName);
    bool EraseName(const std::string& strAddress);

    bool WritePurpose(const std::string& strAddress, const std::string& purpose);
    bool ErasePurpose(const std::string& strAddress);

    bool WriteTx(const CWalletTx& wtx);
    bool EraseTx(uint256 hash);

    //! Write key and metadata to walletdb (unencrypted)
    bool WriteKey(const CPubKey& vchPubKey, const CPrivKey& vchPrivKey, const CKeyMetadata &keyMeta, const std::string forAccount, int64_t nKeyChain);
    //! Write key and metadata to walletdb (encrypted)
    bool WriteCryptedKey(const CPubKey& vchPubKey, const std::vector<unsigned char>& vchCryptedSecret, const CKeyMetadata &keyMeta, const std::string forAccount, int64_t nKeyChain);
    //! Special writer only for witness accounts (where we store the witness key in both HD and non-HD mode)
    bool WriteKeyOverride(const CPubKey& vchPubKey, const CPrivKey& vchPrivKey, const std::string forAccount, int64_t nKeyChain);
    bool WriteMasterKey(unsigned int nID, const CMasterKey& kMasterKey);

    bool EraseKey(const CPubKey& vchPubKey);
    bool EraseEncryptedKey(const CPubKey& vchPubKey);
    bool WriteKeyHD(const CPubKey& vchPubKey, const int64_t HDKeyIndex, const int64_t keyChain, const CKeyMetadata &keyMeta, const std::string forAccount);

    bool WriteCScript(const uint160& hash, const CScript& redeemScript);

    bool WriteWatchOnly(const CScript &script, const CKeyMetadata &keymeta);
    bool EraseWatchOnly(const CScript &script);

    bool WriteBestBlock(const CBlockLocator& locator);
    bool ReadBestBlock(CBlockLocator& locator);

    bool EraseLastSPVBlockProcessed();
    bool WriteLastSPVBlockProcessed(const CBlockLocator& locator, const int64_t time);
    bool ReadLastSPVBlockProcessed(CBlockLocator& locator, int64_t& time);

    bool WriteOrderPosNext(int64_t nOrderPosNext);
    bool ReadPool(int64_t nPool, CKeyPool& keypool);
    bool WritePool(int64_t nPool, const CKeyPool& keypool);

    //! Erase a keypool AddAccountingEntry
    //! Pass forceErase=true to bypass keypool "protection" that is applied for IsFixedKeyPool() accounts
    bool ErasePool(CWallet* pwallet, int64_t nPool, bool forceErase=false);
    bool ErasePool(CWallet* pwallet, const CKeyID& id, bool forceErase=false);
    bool HasPool(CWallet* pwallet, const CKeyID& id);

    bool WriteMinVersion(int nVersion);

    /// This writes directly to the database, and will not update the CWallet's cached accounting entries!
    /// Use wallet.AddAccountingEntry instead, to write *and* update its caches.
    bool WriteAccountingEntry(const uint64_t nAccEntryNum, const CAccountingEntry& acentry);

    //! write the account to the database, this excludes information like the label and compounding status which are written seperately to allow them to be changeable without rewriting the entire account.
    bool WriteAccount(const std::string& strAccount, const CAccount* account);
    //! Wipe the acount from the database, note it's necessary to also erase the label and other info manually.
    bool EraseAccount(const std::string& strAccount, const CAccount* account);

    //! write the account label
    bool WriteAccountLabel(const std::string& strUUID, const std::string& strLabel);
    bool EraseAccountLabel(const std::string& strUUID);

    //! Write the account compounding amount settings; used only by witness accounts, controls whether the account should compound earnings or pay them to an external address.
    bool WriteAccountCompoundingSettings(const std::string& strUUID, const CAmount compoundAmount);
    bool EraseAccountCompoundingSettings(const std::string& strUUID);

    //! Write the account compounding script settings; used only by witness accounts, controls where the account should pay any non-compounding earnings it has to pay out.
    bool WriteAccountNonCompoundWitnessEarningsScript(const std::string& strUUID, const CScript& earningsScript);
    bool EraseAccountNonCompoundWitnessEarningsScript(const std::string& strUUID);

    //! write the seed (mnemonic / account index counter)
    bool WriteHDSeed(const CHDSeed& seed);
    bool DeleteHDSeed(const CHDSeed& seed);
    bool WritePrimarySeed(const CHDSeed& seed);
    bool ErasePrimarySeed();
    bool WritePrimaryAccount(const CAccount* account);
    bool ErasePrimaryAccount();

    /// Write destination data key,value tuple to database
    bool WriteDestData(const std::string &address, const std::string &key, const std::string &value);
    /// Erase destination data tuple from wallet database
    bool EraseDestData(const std::string &address, const std::string &key);

    CAmount GetAccountCreditDebit(const std::string& strAccount);
    void ListAccountCreditDebit(const std::string& strAccount, std::list<CAccountingEntry>& acentries);

    DBErrors LoadWallet(CWallet* pwallet, WalletLoadState& nExtraLoadState);
    DBErrors FindWalletTx(std::vector<uint256>& vTxHash, std::vector<CWalletTx>& vWtx);
    DBErrors ZapWalletTx(std::vector<CWalletTx>& vWtx);
    DBErrors ZapSelectTx(std::vector<uint256>& vHashIn, std::vector<uint256>& vHashOut);
    /* Try to (very carefully!) recover wallet database (with a possible key type filter) */
    static bool Recover(const std::string& filename, void *callbackDataIn, bool (*recoverKVcallback)(void* callbackData, CDataStream ssKey, CDataStream ssValue), std::string& out_backup_filename);
    /* Recover convenience-function to bypass the key filter callback, called when verify fails, recovers everything */
    static bool Recover(const std::string& filename, std::string& out_backup_filename);
    /* Recover filter (used as callback), will only let keys (cryptographical keys) as KV/key-type pass through */
    static bool RecoverKeysOnlyFilter(void *callbackData, CDataStream ssKey, CDataStream ssValue);
    /* Function to determine if a certain KV/key-type is a key (cryptographical key) type */
    static bool IsKeyType(const std::string& strType);
    /* verifies the database environment */
    static bool VerifyEnvironment(const std::string& walletFile, const fs::path& dataDir, std::string& errorStr);
    /* verifies the database file */
    static bool VerifyDatabaseFile(const std::string& walletFile, const fs::path& dataDir, std::string& warningStr, std::string& errorStr);

    //! write the hdchain model (external chain child index counter)
    /* GULDEN - We don't use HD chain.
    bool WriteHDChain(const CHDChain& chain);
    */

    //! Begin a new transaction
    bool TxnBegin();
    //! Commit current transaction
    bool TxnCommit();
    //! Abort current transaction
    bool TxnAbort();
    //! Read wallet version
    bool ReadVersion(int& nVersion);
    //! Write wallet version
    bool WriteVersion(int nVersion);
private:
    CDB batch;
    CWalletDBWrapper& m_dbw;

    CWalletDB(const CWalletDB&);
    void operator=(const CWalletDB&);
};

//! Compacts BDB state so that wallet.dat is self-contained (if there are changes)
void MaybeCompactWalletDB();

#endif // GULDEN_WALLET_WALLETDB_H
