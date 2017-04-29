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

#ifndef BITCOIN_WALLET_WALLETDB_H
#define BITCOIN_WALLET_WALLETDB_H

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


/* simple HD chain data model */
class CHDChain
{
public:
    uint32_t nExternalChainCounter;
    CKeyID masterKeyID; //!< master key hash160

    static const int CURRENT_VERSION = 1;
    int nVersion;

    CHDChain() { SetNull(); }
    ADD_SERIALIZE_METHODS;
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(this->nVersion);
        READWRITE(nExternalChainCounter);
        READWRITE(masterKeyID);
    }

    void SetNull()
    {
        nVersion = CHDChain::CURRENT_VERSION;
        nExternalChainCounter = 0;
        masterKeyID.SetNull();
    }
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

/** Access to the wallet database */
class CWalletDB : public CDB
{
public:
    CWalletDB(const std::string& strFilename, const char* pszMode = "r+", bool _fFlushOnClose = true) : CDB(strFilename, pszMode, _fFlushOnClose)
    {
    }

    bool WriteName(const std::string& strAddress, const std::string& strName);
    bool EraseName(const std::string& strAddress);

    bool WritePurpose(const std::string& strAddress, const std::string& purpose);
    bool ErasePurpose(const std::string& strAddress);

    bool WriteTx(const CWalletTx& wtx);
    bool EraseTx(uint256 hash);

    bool EraseKey(const CPubKey& vchPubKey);
    bool EraseEncryptedKey(const CPubKey& vchPubKey);
    bool WriteKey(const CPubKey& vchPubKey, const CPrivKey& vchPrivKey, const CKeyMetadata &keyMeta, const std::string forAccount, int64_t nKeyChain);
    bool WriteKeyHD(const CPubKey& vchPubKey, const int64_t HDKeyIndex, const int64_t keyChain, const CKeyMetadata &keyMeta, const std::string forAccount);
    bool WriteCryptedKey(const CPubKey& vchPubKey, const std::vector<unsigned char>& vchCryptedSecret, const CKeyMetadata &keyMeta, const std::string forAccount, int64_t nKeyChain);
    bool WriteMasterKey(unsigned int nID, const CMasterKey& kMasterKey);

    bool WriteCScript(const uint160& hash, const CScript& redeemScript);

    bool WriteWatchOnly(const CScript &script);
    bool EraseWatchOnly(const CScript &script);

    bool WriteBestBlock(const CBlockLocator& locator);
    bool ReadBestBlock(CBlockLocator& locator);

    bool WriteOrderPosNext(int64_t nOrderPosNext);

    bool ReadPool(int64_t nPool, CKeyPool& keypool);
    bool WritePool(int64_t nPool, const CKeyPool& keypool);
    bool ErasePool(CWallet* pwallet, int64_t nPool);
    bool ErasePool(CWallet* pwallet, const CKeyID& id);
    bool HasPool(CWallet* pwallet, const CKeyID& id);

    bool WriteMinVersion(int nVersion);

    /// This writes directly to the database, and will not update the CWallet's cached accounting entries!
    /// Use wallet.AddAccountingEntry instead, to write *and* update its caches.
    bool WriteAccountingEntry(const uint64_t nAccEntryNum, const CAccountingEntry& acentry);
    bool WriteAccountingEntry_Backend(const CAccountingEntry& acentry);

    bool WriteAccountLabel(const std::string& strUUID, const std::string& strLabel);
    bool EraseAccountLabel(const std::string& strUUID);

    bool WriteAccount(const std::string& strAccount, const CAccount* account);

    /// Write destination data key,value tuple to database
    bool WriteDestData(const std::string &address, const std::string &key, const std::string &value);
    /// Erase destination data tuple from wallet database
    bool EraseDestData(const std::string &address, const std::string &key);

    CAmount GetAccountCreditDebit(const std::string& strAccount);
    void ListAccountCreditDebit(const std::string& strAccount, std::list<CAccountingEntry>& acentries);

    DBErrors LoadWallet(CWallet* pwallet, bool& firstRunRet);
    DBErrors FindWalletTx(std::vector<uint256>& vTxHash, std::vector<CWalletTx>& vWtx);
    DBErrors ZapWalletTx(std::vector<CWalletTx>& vWtx);
    DBErrors ZapSelectTx(std::vector<uint256>& vHashIn, std::vector<uint256>& vHashOut);
    /* Try to (very carefully!) recover wallet database (with a possible key type filter) */
    static bool Recover(const std::string& filename, void *callbackDataIn, bool (*recoverKVcallback)(void* callbackData, CDataStream ssKey, CDataStream ssValue));
    /* Recover convenience-function to bypass the key filter callback, called when verify failes, recoveres everything */
    static bool Recover(const std::string& filename);
    /* Recover filter (used as callback), will only let keys (cryptographical keys) as KV/key-type pass through */
    static bool RecoverKeysOnlyFilter(void *callbackData, CDataStream ssKey, CDataStream ssValue);
    /* Function to determin if a certain KV/key-type is a key (cryptographical key) type */
    static bool IsKeyType(const std::string& strType);
    /* verifies the database environment */
    static bool VerifyEnvironment(const std::string& walletFile, const boost::filesystem::path& dataDir, std::string& errorStr);
    /* verifies the database file */
    static bool VerifyDatabaseFile(const std::string& walletFile, const boost::filesystem::path& dataDir, std::string& warningStr, std::string& errorStr);

    //! write the hdchain model (external chain child index counter)
    bool WriteHDChain(const CHDChain& chain);
    
    //! write the seed (mnemonic / account index counter)
    bool WriteHDSeed(const CHDSeed& seed);
    bool DeleteHDSeed(const CHDSeed& seed);
    
    bool WritePrimarySeed(const CHDSeed& seed);
    bool WritePrimaryAccount(const CAccount* account);

    static void IncrementUpdateCounter();
    static unsigned int GetUpdateCounter();
private:
    CWalletDB(const CWalletDB&);
    void operator=(const CWalletDB&);
};

//! Compacts BDB state so that wallet.dat is self-contained (if there are changes)
void MaybeCompactWalletDB();

#endif // BITCOIN_WALLET_WALLETDB_H
