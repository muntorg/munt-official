// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// File contains modifications by: The Gulden developers
// All modifications:
// Copyright (c) 2016 The Gulden developers
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
class CKeyMetadata;

/* simple HD chain data model */
class CHDChain {
public:
    uint32_t nExternalChainCounter;
    CKeyID masterKeyID; //!< master key hash160

    static const int CURRENT_VERSION = 1;
    int nVersion;

    CHDChain() { SetNull(); }
    ADD_SERIALIZE_METHODS;
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
    {
        READWRITE(this->nVersion);
        nVersion = this->nVersion;
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

/** Access to the wallet database */
class CWalletDB : public CDB {
public:
    CWalletDB(const std::string& strFilename, const char* pszMode = "r+", bool fFlushOnClose = true)
        : CDB(strFilename, pszMode, fFlushOnClose)
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
    bool WriteKey(const CPubKey& vchPubKey, const CPrivKey& vchPrivKey, const CKeyMetadata& keyMeta, const std::string forAccount, int64_t nKeyChain);
    bool WriteKeyHD(const CPubKey& vchPubKey, const int64_t HDKeyIndex, const int64_t keyChain, const CKeyMetadata& keyMeta, const std::string forAccount);
    bool WriteCryptedKey(const CPubKey& vchPubKey, const std::vector<unsigned char>& vchCryptedSecret, const CKeyMetadata& keyMeta, const std::string forAccount, int64_t nKeyChain);
    bool WriteMasterKey(unsigned int nID, const CMasterKey& kMasterKey);

    bool WriteCScript(const uint160& hash, const CScript& redeemScript);

    bool WriteWatchOnly(const CScript& script);
    bool EraseWatchOnly(const CScript& script);

    bool WriteBestBlock(const CBlockLocator& locator);
    bool ReadBestBlock(CBlockLocator& locator);

    bool WriteOrderPosNext(int64_t nOrderPosNext);

    bool ReadPool(int64_t nPool, CKeyPool& keypool);
    bool WritePool(int64_t nPool, const CKeyPool& keypool);
    bool ErasePool(CWallet* pwallet, int64_t nPool);
    bool ErasePool(CWallet* pwallet, const CKeyID& id);
    bool HasPool(CWallet* pwallet, const CKeyID& id);

    bool WriteMinVersion(int nVersion);

    bool WriteAccountingEntry_Backend(const CAccountingEntry& acentry);

    bool WriteAccountLabel(const std::string& strUUID, const std::string& strLabel);
    bool EraseAccountLabel(const std::string& strUUID);

    bool WriteAccount(const std::string& strAccount, const CAccount* account);

    bool WriteDestData(const std::string& address, const std::string& key, const std::string& value);

    bool EraseDestData(const std::string& address, const std::string& key);

    CAmount GetAccountCreditDebit(const std::string& strAccount);
    void ListAccountCreditDebit(const std::string& strAccount, std::list<CAccountingEntry>& acentries);

    DBErrors ReorderTransactions(CWallet* pwallet);
    DBErrors LoadWallet(CWallet* pwallet, bool& firstRunRet);
    DBErrors FindWalletTx(CWallet* pwallet, std::vector<uint256>& vTxHash, std::vector<CWalletTx>& vWtx);
    DBErrors ZapWalletTx(CWallet* pwallet, std::vector<CWalletTx>& vWtx);
    DBErrors ZapSelectTx(CWallet* pwallet, std::vector<uint256>& vHashIn, std::vector<uint256>& vHashOut);
    static bool Recover(CDBEnv& dbenv, const std::string& filename, bool fOnlyKeys);
    static bool Recover(CDBEnv& dbenv, const std::string& filename);

    bool WriteHDChain(const CHDChain& chain);

    bool WriteHDSeed(const CHDSeed& seed);

    bool WritePrimarySeed(const CHDSeed& seed);
    bool WritePrimaryAccount(const CAccount* account);

private:
    CWalletDB(const CWalletDB&);
    void operator=(const CWalletDB&);

    bool WriteAccountingEntry(const uint64_t nAccEntryNum, const CAccountingEntry& acentry);
};

void ThreadFlushWalletDB(const std::string& strFile);

#endif // BITCOIN_WALLET_WALLETDB_H
