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

#include "wallet/walletdb.h"

#include "base58.h"
#include "consensus/validation.h"
#include "main.h" // For CheckTransaction
#include "protocol.h"
#include "serialize.h"
#include "sync.h"
#include "util.h"
#include "utiltime.h"
#include "wallet/wallet.h"

#include <boost/version.hpp>
#include <boost/filesystem.hpp>
#include <boost/foreach.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/thread.hpp>

#include <map>

using namespace std;

static uint64_t nAccountingEntryNumber = 0;

static std::map<CKeyID, int64_t> staticPoolCache;

bool CWalletDB::WriteName(const string& strAddress, const string& strName)
{
    nWalletDBUpdated++;
    return Write(make_pair(string("name"), strAddress), strName);
}

bool CWalletDB::EraseName(const string& strAddress)
{

    nWalletDBUpdated++;
    return Erase(make_pair(string("name"), strAddress));
}

bool CWalletDB::WritePurpose(const string& strAddress, const string& strPurpose)
{
    nWalletDBUpdated++;
    return Write(make_pair(string("purpose"), strAddress), strPurpose);
}

bool CWalletDB::ErasePurpose(const string& strPurpose)
{
    nWalletDBUpdated++;
    return Erase(make_pair(string("purpose"), strPurpose));
}

bool CWalletDB::WriteTx(const CWalletTx& wtx)
{
    nWalletDBUpdated++;
    return Write(std::make_pair(std::string("tx"), wtx.GetHash()), wtx);
}

bool CWalletDB::EraseTx(uint256 hash)
{
    nWalletDBUpdated++;
    return Erase(std::make_pair(std::string("tx"), hash));
}

bool CWalletDB::EraseKey(const CPubKey& vchPubKey)
{
    nWalletDBUpdated++;
    return Erase(std::make_pair(std::string("keymeta"), vchPubKey)) && Erase(std::make_pair(std::string("key"), vchPubKey));
}

bool CWalletDB::EraseEncryptedKey(const CPubKey& vchPubKey)
{
    nWalletDBUpdated++;
    return Erase(std::make_pair(std::string("keymeta"), vchPubKey)) && Erase(std::make_pair(std::string("ckey"), vchPubKey));
}

bool CWalletDB::WriteKey(const CPubKey& vchPubKey, const CPrivKey& vchPrivKey, const CKeyMetadata& keyMeta, const std::string forAccount, int64_t nKeyChain)
{
    nWalletDBUpdated++;

    if (!Write(std::make_pair(std::string("keymeta"), vchPubKey),
               keyMeta, false))
        return false;

    std::vector<unsigned char> vchKey;
    vchKey.reserve(vchPubKey.size() + vchPrivKey.size());
    vchKey.insert(vchKey.end(), vchPubKey.begin(), vchPubKey.end());
    vchKey.insert(vchKey.end(), vchPrivKey.begin(), vchPrivKey.end());

    return Write(std::make_pair(std::string("key"), vchPubKey), std::make_tuple(vchPrivKey, Hash(vchKey.begin(), vchKey.end()), forAccount, nKeyChain), false);
}

bool CWalletDB::WriteKeyHD(const CPubKey& vchPubKey, const int64_t HDKeyIndex, int64_t keyChain, const CKeyMetadata& keyMeta, const std::string forAccount)
{
    nWalletDBUpdated++;

    if (!Write(std::make_pair(std::string("keymeta"), vchPubKey),
               keyMeta, false))
        return false;

    return Write(std::make_pair(std::string("keyHD"), vchPubKey), std::make_tuple(HDKeyIndex, keyChain, forAccount), false);
}

bool CWalletDB::WriteCryptedKey(const CPubKey& vchPubKey, const std::vector<unsigned char>& vchCryptedSecret, const CKeyMetadata& keyMeta, const std::string forAccount, int64_t nKeyChain)
{
    const bool fEraseUnencryptedKey = true;
    nWalletDBUpdated++;

    if (!Write(std::make_pair(std::string("keymeta"), vchPubKey),
               keyMeta))
        return false;

    if (!Write(std::make_pair(std::string("ckey"), vchPubKey), std::make_tuple(vchCryptedSecret, forAccount, nKeyChain), false))
        return false;
    if (fEraseUnencryptedKey) {
        Erase(std::make_pair(std::string("key"), vchPubKey));
        Erase(std::make_pair(std::string("wkey"), vchPubKey));
    }
    return true;
}

bool CWalletDB::WriteMasterKey(unsigned int nID, const CMasterKey& kMasterKey)
{
    nWalletDBUpdated++;
    return Write(std::make_pair(std::string("mkey"), nID), kMasterKey, true);
}

bool CWalletDB::WriteCScript(const uint160& hash, const CScript& redeemScript)
{
    nWalletDBUpdated++;
    return Write(std::make_pair(std::string("cscript"), hash), *(const CScriptBase*)(&redeemScript), false);
}

bool CWalletDB::WriteWatchOnly(const CScript& dest)
{
    nWalletDBUpdated++;
    return Write(std::make_pair(std::string("watchs"), *(const CScriptBase*)(&dest)), '1');
}

bool CWalletDB::EraseWatchOnly(const CScript& dest)
{
    nWalletDBUpdated++;
    return Erase(std::make_pair(std::string("watchs"), *(const CScriptBase*)(&dest)));
}

bool CWalletDB::WriteBestBlock(const CBlockLocator& locator)
{
    nWalletDBUpdated++;
    Write(std::string("bestblock"), CBlockLocator()); // Write empty block locator so versions that require a merkle branch automatically rescan
    return Write(std::string("bestblock_nomerkle"), locator);
}

bool CWalletDB::ReadBestBlock(CBlockLocator& locator)
{
    if (Read(std::string("bestblock"), locator) && !locator.vHave.empty())
        return true;
    return Read(std::string("bestblock_nomerkle"), locator);
}

bool CWalletDB::WriteOrderPosNext(int64_t nOrderPosNext)
{
    nWalletDBUpdated++;
    return Write(std::string("orderposnext"), nOrderPosNext);
}

bool CWalletDB::ReadPool(int64_t nPool, CKeyPool& keypool)
{
    return Read(std::make_pair(std::string("pool"), nPool), keypool);
}

bool CWalletDB::WritePool(int64_t nPool, const CKeyPool& keypool)
{
    staticPoolCache[keypool.vchPubKey.GetID()] = nPool;

    nWalletDBUpdated++;
    return Write(std::make_pair(std::string("pool"), nPool), keypool);
}

bool CWalletDB::ErasePool(CWallet* pwallet, int64_t nPool)
{
    nWalletDBUpdated++;
    return Erase(std::make_pair(std::string("pool"), nPool));
}

bool CWalletDB::ErasePool(CWallet* pwallet, const CKeyID& id)
{
    nWalletDBUpdated++;

    for (auto iter : pwallet->mapAccounts) {
        int64_t keyIndex = staticPoolCache[id];
        iter.second->setKeyPoolExternal.erase(keyIndex);
        iter.second->setKeyPoolInternal.erase(keyIndex);
    }

    return Erase(std::make_pair(std::string("pool"), staticPoolCache[id]));
}

bool CWalletDB::HasPool(CWallet* pwallet, const CKeyID& id)
{

    return Exists(std::make_pair(std::string("pool"), staticPoolCache[id]));
}

bool CWalletDB::WriteMinVersion(int nVersion)
{
    return Write(std::string("minversion"), nVersion);
}

bool CWalletDB::WriteAccountLabel(const string& strUUID, const string& strLabel)
{
    nWalletDBUpdated++;
    return Write(std::make_pair(string("acclabel"), strUUID), strLabel);
}

bool CWalletDB::EraseAccountLabel(const string& strUUID)
{
    nWalletDBUpdated++;
    return Erase(std::make_pair(string("acclabel"), strUUID));
}

bool CWalletDB::WriteAccount(const string& strAccount, const CAccount* account)
{
    nWalletDBUpdated++;

    if (account->IsHD())
        return Write(make_pair(string("acchd"), strAccount), *((CAccountHD*)account));
    else
        return Write(make_pair(string("accleg"), strAccount), *account);
}

bool CWalletDB::WriteAccountingEntry(const uint64_t nAccEntryNum, const CAccountingEntry& acentry)
{
    return Write(std::make_pair(std::string("acentry"), std::make_pair(acentry.strAccount, nAccEntryNum)), acentry);
}

bool CWalletDB::WriteAccountingEntry_Backend(const CAccountingEntry& acentry)
{
    return WriteAccountingEntry(++nAccountingEntryNumber, acentry);
}

CAmount CWalletDB::GetAccountCreditDebit(const string& strAccount)
{
    list<CAccountingEntry> entries;
    ListAccountCreditDebit(strAccount, entries);

    CAmount nCreditDebit = 0;
    BOOST_FOREACH (const CAccountingEntry& entry, entries)
        nCreditDebit += entry.nCreditDebit;

    return nCreditDebit;
}

void CWalletDB::ListAccountCreditDebit(const string& strAccount, list<CAccountingEntry>& entries)
{
    bool fAllAccounts = (strAccount == "*");

    Dbc* pcursor = GetCursor();
    if (!pcursor)
        throw runtime_error(std::string(__func__) + ": cannot create DB cursor");
    unsigned int fFlags = DB_SET_RANGE;
    while (true) {

        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        if (fFlags == DB_SET_RANGE)
            ssKey << std::make_pair(std::string("acentry"), std::make_pair((fAllAccounts ? string("") : strAccount), uint64_t(0)));
        CDataStream ssValue(SER_DISK, CLIENT_VERSION);
        int ret = ReadAtCursor(pcursor, ssKey, ssValue, fFlags);
        fFlags = DB_NEXT;
        if (ret == DB_NOTFOUND)
            break;
        else if (ret != 0) {
            pcursor->close();
            throw runtime_error(std::string(__func__) + ": error scanning DB");
        }

        string strType;
        ssKey >> strType;
        if (strType != "acentry")
            break;
        CAccountingEntry acentry;
        ssKey >> acentry.strAccount;
        if (!fAllAccounts && acentry.strAccount != strAccount)
            break;

        ssValue >> acentry;
        ssKey >> acentry.nEntryNo;
        entries.push_back(acentry);
    }

    pcursor->close();
}

DBErrors CWalletDB::ReorderTransactions(CWallet* pwallet)
{
    LOCK(pwallet->cs_wallet);

    typedef pair<CWalletTx*, CAccountingEntry*> TxPair;
    typedef multimap<int64_t, TxPair> TxItems;
    TxItems txByTime;

    for (map<uint256, CWalletTx>::iterator it = pwallet->mapWallet.begin(); it != pwallet->mapWallet.end(); ++it) {
        CWalletTx* wtx = &((*it).second);
        txByTime.insert(make_pair(wtx->nTimeReceived, TxPair(wtx, (CAccountingEntry*)0)));
    }
    list<CAccountingEntry> acentries;
    ListAccountCreditDebit("", acentries);
    BOOST_FOREACH (CAccountingEntry& entry, acentries) {
        txByTime.insert(make_pair(entry.nTime, TxPair((CWalletTx*)0, &entry)));
    }

    int64_t& nOrderPosNext = pwallet->nOrderPosNext;
    nOrderPosNext = 0;
    std::vector<int64_t> nOrderPosOffsets;
    for (TxItems::iterator it = txByTime.begin(); it != txByTime.end(); ++it) {
        CWalletTx* const pwtx = (*it).second.first;
        CAccountingEntry* const pacentry = (*it).second.second;
        int64_t& nOrderPos = (pwtx != 0) ? pwtx->nOrderPos : pacentry->nOrderPos;

        if (nOrderPos == -1) {
            nOrderPos = nOrderPosNext++;
            nOrderPosOffsets.push_back(nOrderPos);

            if (pwtx) {
                if (!WriteTx(*pwtx))
                    return DB_LOAD_FAIL;
            } else if (!WriteAccountingEntry(pacentry->nEntryNo, *pacentry))
                return DB_LOAD_FAIL;
        } else {
            int64_t nOrderPosOff = 0;
            BOOST_FOREACH (const int64_t& nOffsetStart, nOrderPosOffsets) {
                if (nOrderPos >= nOffsetStart)
                    ++nOrderPosOff;
            }
            nOrderPos += nOrderPosOff;
            nOrderPosNext = std::max(nOrderPosNext, nOrderPos + 1);

            if (!nOrderPosOff)
                continue;

            if (pwtx) {
                if (!WriteTx(*pwtx))
                    return DB_LOAD_FAIL;
            } else if (!WriteAccountingEntry(pacentry->nEntryNo, *pacentry))
                return DB_LOAD_FAIL;
        }
    }
    WriteOrderPosNext(nOrderPosNext);

    return DB_LOAD_OK;
}

class CWalletScanState {
public:
    unsigned int nKeys;
    unsigned int nCKeys;
    unsigned int nKeyMeta;
    bool fIsEncrypted;
    bool fAnyUnordered;
    int nFileVersion;
    vector<uint256> vWalletUpgrade;

    CWalletScanState()
    {
        nKeys = nCKeys = nKeyMeta = 0;
        fIsEncrypted = false;
        fAnyUnordered = false;
        nFileVersion = 0;
    }
};

bool
ReadKeyValue(CWallet* pwallet, CDataStream& ssKey, CDataStream& ssValue,
             CWalletScanState& wss, string& strType, string& strErr)
{
    try {

        ssKey >> strType;
        if (strType == "name") {
            string strAddress;
            ssKey >> strAddress;
            ssValue >> pwallet->mapAddressBook[strAddress].name;
        } else if (strType == "purpose") {
            string strAddress;
            ssKey >> strAddress;
            ssValue >> pwallet->mapAddressBook[strAddress].purpose;
        } else if (strType == "tx") {
            uint256 hash;
            ssKey >> hash;
            CWalletTx wtx;
            ssValue >> wtx;
            CValidationState state;
            if (!(CheckTransaction(wtx, state) && (wtx.GetHash() == hash) && state.IsValid()))
                return false;

            if (31404 <= wtx.fTimeReceivedIsTxTime && wtx.fTimeReceivedIsTxTime <= 31703) {
                if (!ssValue.empty()) {
                    char fTmp;
                    char fUnused;
                    ssValue >> fTmp >> fUnused >> wtx.strFromAccount;
                    strErr = strprintf("LoadWallet() upgrading tx ver=%d %d '%s' %s",
                                       wtx.fTimeReceivedIsTxTime, fTmp, wtx.strFromAccount, hash.ToString());
                    wtx.fTimeReceivedIsTxTime = fTmp;
                } else {
                    strErr = strprintf("LoadWallet() repairing tx ver=%d %s", wtx.fTimeReceivedIsTxTime, hash.ToString());
                    wtx.fTimeReceivedIsTxTime = 0;
                }
                wss.vWalletUpgrade.push_back(hash);
            }

            if (wtx.nOrderPos == -1)
                wss.fAnyUnordered = true;

            pwallet->AddToWallet(wtx, true, NULL);
        } else if (strType == "acentry") {
            string strAccount;
            ssKey >> strAccount;
            uint64_t nNumber;
            ssKey >> nNumber;
            if (nNumber > nAccountingEntryNumber)
                nAccountingEntryNumber = nNumber;

            if (!wss.fAnyUnordered) {
                CAccountingEntry acentry;
                ssValue >> acentry;
                if (acentry.nOrderPos == -1)
                    wss.fAnyUnordered = true;
            }
        } else if (strType == "watchs") {
            CScript script;
            ssKey >> *(CScriptBase*)(&script);
            char fYes;
            ssValue >> fYes;
            if (fYes == '1')
                pwallet->LoadWatchOnly(script);

            pwallet->nTimeFirstKey = 1;
        } else if (strType == "keyHD") {
            std::string forAccount = "";
            CPubKey vchPubKey;
            ssKey >> vchPubKey;
            if (!vchPubKey.IsValid()) {
                strErr = "Error reading wallet database: CPubKey corrupt";
                return false;
            }

            int64_t HDKeyIndex;
            int64_t keyChain;
            ssValue >> HDKeyIndex;
            ssValue >> keyChain;
            ssValue >> forAccount;

            if (!pwallet->LoadKey(HDKeyIndex, keyChain, vchPubKey, forAccount)) {
                strErr = "Error reading wallet database: LoadKey (HD) failed";
                return false;
            }
        } else if (strType == "key" || strType == "wkey") {
            std::string forAccount = "";
            CPubKey vchPubKey;
            ssKey >> vchPubKey;
            if (!vchPubKey.IsValid()) {
                strErr = "Error reading wallet database: CPubKey corrupt";
                return false;
            }
            CKey key;
            CPrivKey pkey;
            uint256 hash;

            if (strType == "key") {
                wss.nKeys++;
                ssValue >> pkey;
            } else {
                CWalletKey wkey;
                ssValue >> wkey;
                pkey = wkey.vchPrivKey;
            }

            int64_t nKeyChain;
            try {
                ssValue >> hash;

                ssValue >> forAccount;
                ssValue >> nKeyChain;
            }
            catch (...) {
                forAccount = pwallet->activeAccount->getUUID();
                nKeyChain = KEYCHAIN_EXTERNAL;
            }

            if (strType == "key" && GetBoolArg("-skipplainkeys", false)) {
                LogPrintf("Skipping unencrypted key [skipplainkeys] [%s]\n", CBitcoinAddress(vchPubKey.GetID()).ToString());
            } else {
                bool fSkipCheck = false;

                if (!hash.IsNull()) {

                    std::vector<unsigned char> vchKey;
                    vchKey.reserve(vchPubKey.size() + pkey.size());
                    vchKey.insert(vchKey.end(), vchPubKey.begin(), vchPubKey.end());
                    vchKey.insert(vchKey.end(), pkey.begin(), pkey.end());

                    if (Hash(vchKey.begin(), vchKey.end()) != hash) {
                        strErr = "Error reading wallet database: CPubKey/CPrivKey corrupt";
                        return false;
                    }

                    fSkipCheck = true;
                }

                if (!key.Load(pkey, vchPubKey, fSkipCheck)) {
                    strErr = "Error reading wallet database: CPrivKey corrupt";
                    return false;
                }
                if (!pwallet->LoadKey(key, vchPubKey, forAccount, nKeyChain)) {
                    strErr = "Error reading wallet database: LoadKey failed";
                    return false;
                }
            }
        } else if (strType == "mkey") {
            unsigned int nID;
            ssKey >> nID;
            CMasterKey kMasterKey;
            ssValue >> kMasterKey;
            if (pwallet->mapMasterKeys.count(nID) != 0) {
                strErr = strprintf("Error reading wallet database: duplicate CMasterKey id %u", nID);
                return false;
            }
            pwallet->mapMasterKeys[nID] = kMasterKey;
            if (pwallet->nMasterKeyMaxID < nID)
                pwallet->nMasterKeyMaxID = nID;
        } else if (strType == "ckey") {
            std::string forAccount = "";
            int64_t nKeyChain;
            CPubKey vchPubKey;
            ssKey >> vchPubKey;
            if (!vchPubKey.IsValid()) {
                strErr = "Error reading wallet database: CPubKey corrupt";
                return false;
            }
            vector<unsigned char> vchPrivKey;
            ssValue >> vchPrivKey;
            wss.nCKeys++;
            try {

                ssValue >> forAccount;
                ssValue >> nKeyChain;
            }
            catch (...) {
                forAccount = pwallet->activeAccount->getUUID();
                nKeyChain = KEYCHAIN_EXTERNAL;
            }

            if (GetBoolArg("-skipplainkeys", false)) {
                LogPrintf("Load crypted key [skipplainkeys] [%s]\n", CBitcoinAddress(vchPubKey.GetID()).ToString());
            }

            if (!pwallet->LoadCryptedKey(vchPubKey, vchPrivKey, forAccount, nKeyChain)) {
                strErr = "Error reading wallet database: LoadCryptedKey failed";
                return false;
            }
            wss.fIsEncrypted = true;
        } else if (strType == "keymeta") {
            CPubKey vchPubKey;
            ssKey >> vchPubKey;
            CKeyMetadata keyMeta;
            ssValue >> keyMeta;
            wss.nKeyMeta++;

            pwallet->LoadKeyMetadata(vchPubKey, keyMeta);

            if (!pwallet->nTimeFirstKey || (keyMeta.nCreateTime < pwallet->nTimeFirstKey))
                pwallet->nTimeFirstKey = keyMeta.nCreateTime;
        } else if (strType == "pool") {
            int64_t nIndex;
            ssKey >> nIndex;
            CKeyPool keypool;
            ssValue >> keypool;

            CAccount* forAccount = NULL;
            std::string accountUUID = keypool.accountName;

            if (accountUUID.empty()) {
                forAccount = pwallet->activeAccount;
                keypool.nChain = KEYCHAIN_EXTERNAL;
            } else {
                if (pwallet->mapAccounts.count(accountUUID) == 0) {
                    strErr = "Wallet contains key for non existent account";
                    return false;
                }
                forAccount = pwallet->mapAccounts[accountUUID];
            }

            if (keypool.nChain == KEYCHAIN_EXTERNAL) {
                forAccount->setKeyPoolExternal.insert(nIndex);
            } else {
                forAccount->setKeyPoolInternal.insert(nIndex);
            }

            CKeyID keyid = keypool.vchPubKey.GetID();
            if (pwallet->mapKeyMetadata.count(keyid) == 0)
                pwallet->mapKeyMetadata[keyid] = CKeyMetadata(keypool.nTime);

            staticPoolCache[keyid] = nIndex;
        } else if (strType == "version") {
            ssValue >> wss.nFileVersion;
            if (wss.nFileVersion == 10300)
                wss.nFileVersion = 300;
        } else if (strType == "cscript") {
            uint160 hash;
            ssKey >> hash;
            CScript script;
            ssValue >> *(CScriptBase*)(&script);
            if (!pwallet->LoadCScript(script)) {
                strErr = "Error reading wallet database: LoadCScript failed";
                return false;
            }
        } else if (strType == "orderposnext") {
            ssValue >> pwallet->nOrderPosNext;
        } else if (strType == "destdata") {
            std::string strAddress, strKey, strValue;
            ssKey >> strAddress;
            ssKey >> strKey;
            ssValue >> strValue;
            if (!pwallet->LoadDestData(CBitcoinAddress(strAddress).Get(), strKey, strValue)) {
                strErr = "Error reading wallet database: LoadDestData failed";
                return false;
            }
        } else if (strType == "hdseed") {
            CHDSeed* newSeed = new CHDSeed();
            ssValue >> *newSeed;
            if (!pwallet->activeSeed)
                pwallet->activeSeed = newSeed;
            pwallet->mapSeeds[newSeed->getUUID()] = newSeed;
        } else if (strType == "primaryseed") {

        } else if (strType == "primaryaccount") {

        } else if (strType == "acc") {

        } else if (strType == "accleg") {
            std::string strAccountUUID;
            ssKey >> strAccountUUID;
            if (pwallet->mapAccounts.count(strAccountUUID) == 0) {
                CAccount* newAccount = new CAccount();
                newAccount->setUUID(strAccountUUID);
                ssValue >> *newAccount;
                pwallet->mapAccounts[strAccountUUID] = newAccount;

                if (!pwallet->activeAccount)
                    pwallet->activeAccount = newAccount;
            }
        } else if (strType == "acchd") {
            std::string strAccountUUID;
            ssKey >> strAccountUUID;
            if (pwallet->mapAccounts.count(strAccountUUID) == 0) {
                CAccountHD* newAccount = new CAccountHD();
                newAccount->setUUID(strAccountUUID);
                ssValue >> *newAccount;
                pwallet->mapAccounts[strAccountUUID] = newAccount;

                if (!pwallet->activeAccount)
                    pwallet->activeAccount = newAccount;
            }
        } else if (strType == "acclabel") {
            std::string strAccountUUID;
            std::string strAccountLabel;
            ssKey >> strAccountUUID;
            ssValue >> strAccountLabel;

            pwallet->mapAccountLabels[strAccountUUID] = strAccountLabel;
        }
    }
    catch (...) {
        return false;
    }
    return true;
}

static bool IsKeyType(string strType)
{
    return (strType == "key" || strType == "wkey" || strType == "mkey" || strType == "ckey");
}

DBErrors CWalletDB::LoadWallet(CWallet* pwallet, bool& firstRunRet)
{
    CWalletScanState wss;
    bool fNoncriticalErrors = false;
    DBErrors result = DB_LOAD_OK;

    std::string primaryAccountString;
    std::string primarySeedString;
    try {
        LOCK(pwallet->cs_wallet);
        int nMinVersion = 0;
        if (Read((string) "minversion", nMinVersion)) {
            if (nMinVersion > CLIENT_VERSION)
                return DB_TOO_NEW;
            pwallet->LoadMinVersion(nMinVersion);
        }

        bool isPreHDWallet = false;

        {

            Dbc* pcursor = GetCursor();
            if (!pcursor) {
                LogPrintf("Error getting wallet database cursor\n");
                return DB_CORRUPT;
            }
            while (true) {

                CDataStream ssKey(SER_DISK, CLIENT_VERSION);
                CDataStream ssValue(SER_DISK, CLIENT_VERSION);
                int ret = ReadAtCursor(pcursor, ssKey, ssValue);
                if (ret == DB_NOTFOUND)
                    break;
                else if (ret != 0) {
                    LogPrintf("Error reading next record from wallet database\n");
                    return DB_CORRUPT;
                }

                std::string sKey;
                ssKey >> sKey;
                if (sKey == "accleg" || sKey == "acchd") {
                    CDataStream ssKey2(SER_DISK, CLIENT_VERSION);
                    std::string accountUUID;
                    ssKey >> accountUUID;
                    ssKey2 << sKey << accountUUID;

                    string strType, strErr;
                    if (!ReadKeyValue(pwallet, ssKey2, ssValue, wss, strType, strErr)) {

                        if (IsKeyType(strType))
                            result = DB_CORRUPT;
                        else {

                            fNoncriticalErrors = true; // ... but do warn the user there is something wrong.
                            if (strType == "tx") // Rescan if there is a bad transaction record:
                                SoftSetBoolArg("-rescan", true);
                        }
                    }
                    if (!strErr.empty())
                        LogPrintf("%s\n", strErr);
                } else if (sKey == "primaryseed") {
                    ssValue >> primarySeedString;
                } else if (sKey == "primaryaccount") {
                    ssValue >> primaryAccountString;
                } else if (sKey == "defaultkey") {
                    isPreHDWallet = true;
                }
            }
            pcursor->close();
        }

        firstRunRet = true;
        if (!primaryAccountString.empty()) {
            firstRunRet = false;
            if (pwallet->mapAccounts.count(primaryAccountString) == 0) {
                LogPrintf("Error - missing primary account for UUID [%s]\n", primaryAccountString);
                fNoncriticalErrors = true;
            } else {
                pwallet->activeAccount = pwallet->mapAccounts[primaryAccountString];
            }
        } else if (isPreHDWallet) {
            firstRunRet = false;

            if (pwallet->activeAccount == NULL && pwallet->activeSeed == NULL) {
                try {
                    boost::filesystem::path oldPath = bitdb.strPath;
                    oldPath = oldPath / strFile;
                    boost::filesystem::path backupPath = oldPath;
                    backupPath.replace_extension(".old.preHD");
                    boost::filesystem::copy_file(oldPath, backupPath);
                }
                catch (...) {
                }

                pwallet->activeAccount = new CAccount();
                pwallet->activeAccount->setLabel("Legacy", NULL);
                pwallet->mapAccounts[pwallet->activeAccount->getUUID()] = pwallet->activeAccount;
                pwallet->mapAccountLabels[pwallet->activeAccount->getUUID()] = "Legacy";
            }
        }

        Dbc* pcursor = GetCursor();
        if (!pcursor) {
            LogPrintf("Error getting wallet database cursor\n");
            return DB_CORRUPT;
        }

        while (true) {

            CDataStream ssKey(SER_DISK, CLIENT_VERSION);
            CDataStream ssValue(SER_DISK, CLIENT_VERSION);
            int ret = ReadAtCursor(pcursor, ssKey, ssValue);
            if (ret == DB_NOTFOUND)
                break;
            else if (ret != 0) {
                LogPrintf("Error reading next record from wallet database\n");
                return DB_CORRUPT;
            }

            string strType, strErr;
            if (!ReadKeyValue(pwallet, ssKey, ssValue, wss, strType, strErr)) {

                if (IsKeyType(strType))
                    result = DB_CORRUPT;
                else {

                    fNoncriticalErrors = true; // ... but do warn the user there is something wrong.
                    if (strType == "tx")

                        SoftSetBoolArg("-rescan", true);
                }
            }
            if (!strErr.empty())
                LogPrintf("%s\n", strErr);
        }
        pcursor->close();
    }
    catch (const boost::thread_interrupted&) {
        throw;
    }
    catch (...) {
        result = DB_CORRUPT;
    }

    for (const auto& labelPair : pwallet->mapAccountLabels) {
        if (pwallet->mapAccounts.count(labelPair.first) == 0) {

            LogPrintf("Error - missing account label for account UUID [%s]\n", labelPair.first);
            fNoncriticalErrors = true;
        } else {
            pwallet->mapAccounts[labelPair.first]->setLabel(labelPair.second, NULL);
        }
    }
    if (!primarySeedString.empty()) {
        if (pwallet->mapSeeds.count(primarySeedString) == 0) {

            LogPrintf("Error - missing primary seed for UUID [%s]\n", primarySeedString);
            fNoncriticalErrors = true;
        } else {
            pwallet->activeSeed = pwallet->mapSeeds[primarySeedString];
        }
    }

    if (fNoncriticalErrors && result == DB_LOAD_OK)
        result = DB_NONCRITICAL_ERROR;

    if (result != DB_LOAD_OK)
        return result;

    LogPrintf("nFileVersion = %d\n", wss.nFileVersion);

    LogPrintf("Keys: %u plaintext, %u encrypted, %u w/ metadata, %u total\n",
              wss.nKeys, wss.nCKeys, wss.nKeyMeta, wss.nKeys + wss.nCKeys);

    if ((wss.nKeys + wss.nCKeys) != wss.nKeyMeta)
        pwallet->nTimeFirstKey = 1; // 0 would be considered 'no value'

    BOOST_FOREACH (uint256 hash, wss.vWalletUpgrade)
        WriteTx(pwallet->mapWallet[hash]);

    if (wss.fIsEncrypted && (wss.nFileVersion == 40000 || wss.nFileVersion == 50000))
        return DB_NEED_REWRITE;

    if (wss.nFileVersion < CLIENT_VERSION) // Update
        WriteVersion(CLIENT_VERSION);

    if (wss.fAnyUnordered)
        result = ReorderTransactions(pwallet);

    pwallet->laccentries.clear();
    ListAccountCreditDebit("*", pwallet->laccentries);
    BOOST_FOREACH (CAccountingEntry& entry, pwallet->laccentries) {
        pwallet->wtxOrdered.insert(make_pair(entry.nOrderPos, CWallet::TxPair((CWalletTx*)0, &entry)));
    }

    return result;
}

DBErrors CWalletDB::FindWalletTx(CWallet* pwallet, vector<uint256>& vTxHash, vector<CWalletTx>& vWtx)
{
    bool fNoncriticalErrors = false;
    DBErrors result = DB_LOAD_OK;

    try {
        LOCK(pwallet->cs_wallet);
        int nMinVersion = 0;
        if (Read((string) "minversion", nMinVersion)) {
            if (nMinVersion > CLIENT_VERSION)
                return DB_TOO_NEW;
            pwallet->LoadMinVersion(nMinVersion);
        }

        Dbc* pcursor = GetCursor();
        if (!pcursor) {
            LogPrintf("Error getting wallet database cursor\n");
            return DB_CORRUPT;
        }

        while (true) {

            CDataStream ssKey(SER_DISK, CLIENT_VERSION);
            CDataStream ssValue(SER_DISK, CLIENT_VERSION);
            int ret = ReadAtCursor(pcursor, ssKey, ssValue);
            if (ret == DB_NOTFOUND)
                break;
            else if (ret != 0) {
                LogPrintf("Error reading next record from wallet database\n");
                return DB_CORRUPT;
            }

            string strType;
            ssKey >> strType;
            if (strType == "tx") {
                uint256 hash;
                ssKey >> hash;

                CWalletTx wtx;
                ssValue >> wtx;

                vTxHash.push_back(hash);
                vWtx.push_back(wtx);
            }
        }
        pcursor->close();
    }
    catch (const boost::thread_interrupted&) {
        throw;
    }
    catch (...) {
        result = DB_CORRUPT;
    }

    if (fNoncriticalErrors && result == DB_LOAD_OK)
        result = DB_NONCRITICAL_ERROR;

    return result;
}

DBErrors CWalletDB::ZapSelectTx(CWallet* pwallet, vector<uint256>& vTxHashIn, vector<uint256>& vTxHashOut)
{

    vector<uint256> vTxHash;
    vector<CWalletTx> vWtx;
    DBErrors err = FindWalletTx(pwallet, vTxHash, vWtx);
    if (err != DB_LOAD_OK) {
        return err;
    }

    std::sort(vTxHash.begin(), vTxHash.end());
    std::sort(vTxHashIn.begin(), vTxHashIn.end());

    bool delerror = false;
    vector<uint256>::iterator it = vTxHashIn.begin();
    BOOST_FOREACH (uint256 hash, vTxHash) {
        while (it < vTxHashIn.end() && (*it) < hash) {
            it++;
        }
        if (it == vTxHashIn.end()) {
            break;
        } else if ((*it) == hash) {
            pwallet->mapWallet.erase(hash);
            if (!EraseTx(hash)) {
                LogPrint("db", "Transaction was found for deletion but returned database error: %s\n", hash.GetHex());
                delerror = true;
            }
            vTxHashOut.push_back(hash);
        }
    }

    if (delerror) {
        return DB_CORRUPT;
    }
    return DB_LOAD_OK;
}

DBErrors CWalletDB::ZapWalletTx(CWallet* pwallet, vector<CWalletTx>& vWtx)
{

    vector<uint256> vTxHash;
    DBErrors err = FindWalletTx(pwallet, vTxHash, vWtx);
    if (err != DB_LOAD_OK)
        return err;

    BOOST_FOREACH (uint256& hash, vTxHash) {
        if (!EraseTx(hash))
            return DB_CORRUPT;
    }

    return DB_LOAD_OK;
}

void ThreadFlushWalletDB(const string& strFile)
{

    RenameThread("Gulden-wallet");

    static bool fOneThread;
    if (fOneThread)
        return;
    fOneThread = true;
    if (!GetBoolArg("-flushwallet", DEFAULT_FLUSHWALLET))
        return;

    unsigned int nLastSeen = nWalletDBUpdated;
    unsigned int nLastFlushed = nWalletDBUpdated;
    int64_t nLastWalletUpdate = GetTime();
    while (true) {
        MilliSleep(500);

        if (nLastSeen != nWalletDBUpdated) {
            nLastSeen = nWalletDBUpdated;
            nLastWalletUpdate = GetTime();
        }

        if (nLastFlushed != nWalletDBUpdated && GetTime() - nLastWalletUpdate >= 2) {
            TRY_LOCK(bitdb.cs_db, lockDb);
            if (lockDb) {

                int nRefCount = 0;
                map<string, int>::iterator mi = bitdb.mapFileUseCount.begin();
                while (mi != bitdb.mapFileUseCount.end()) {
                    nRefCount += (*mi).second;
                    mi++;
                }

                if (nRefCount == 0) {
                    boost::this_thread::interruption_point();
                    map<string, int>::iterator mi = bitdb.mapFileUseCount.find(strFile);
                    if (mi != bitdb.mapFileUseCount.end()) {
                        LogPrint("db", "Flushing %s\n", strFile);
                        nLastFlushed = nWalletDBUpdated;
                        int64_t nStart = GetTimeMillis();

                        bitdb.CloseDb(strFile);
                        bitdb.CheckpointLSN(strFile);

                        bitdb.mapFileUseCount.erase(mi++);
                        LogPrint("db", "Flushed %s %dms\n", strFile, GetTimeMillis() - nStart);
                    }
                }
            }
        }
    }
}

bool CWalletDB::Recover(CDBEnv& dbenv, const std::string& filename, bool fOnlyKeys)
{

    int64_t now = GetTime();
    std::string newFilename = strprintf("wallet.%d.bak", now);

    int result = dbenv.dbenv->dbrename(NULL, filename.c_str(), NULL,
                                       newFilename.c_str(), DB_AUTO_COMMIT);
    if (result == 0)
        LogPrintf("Renamed %s to %s\n", filename, newFilename);
    else {
        LogPrintf("Failed to rename %s to %s\n", filename, newFilename);
        return false;
    }

    std::vector<CDBEnv::KeyValPair> salvagedData;
    bool fSuccess = dbenv.Salvage(newFilename, true, salvagedData);
    if (salvagedData.empty()) {
        LogPrintf("Salvage(aggressive) found no records in %s.\n", newFilename);
        return false;
    }
    LogPrintf("Salvage(aggressive) found %u records\n", salvagedData.size());

    boost::scoped_ptr<Db> pdbCopy(new Db(dbenv.dbenv, 0));
    int ret = pdbCopy->open(NULL, // Txn pointer
                            filename.c_str(), // Filename
                            "main", // Logical db name
                            DB_BTREE, // Database type
                            DB_CREATE, // Flags
                            0);
    if (ret > 0) {
        LogPrintf("Cannot create database file %s\n", filename);
        return false;
    }
    CWallet dummyWallet;
    CWalletScanState wss;

    DbTxn* ptxn = dbenv.TxnBegin();
    BOOST_FOREACH (CDBEnv::KeyValPair& row, salvagedData) {
        if (fOnlyKeys) {
            CDataStream ssKey(row.first, SER_DISK, CLIENT_VERSION);
            CDataStream ssValue(row.second, SER_DISK, CLIENT_VERSION);
            string strType, strErr;
            bool fReadOK;
            {

                LOCK(dummyWallet.cs_wallet);
                fReadOK = ReadKeyValue(&dummyWallet, ssKey, ssValue,
                                       wss, strType, strErr);
            }
            if (!IsKeyType(strType) && strType != "hdchain")
                continue;
            if (!fReadOK) {
                LogPrintf("WARNING: CWalletDB::Recover skipping %s: %s\n", strType, strErr);
                continue;
            }
        }
        Dbt datKey(&row.first[0], row.first.size());
        Dbt datValue(&row.second[0], row.second.size());
        int ret2 = pdbCopy->put(ptxn, &datKey, &datValue, DB_NOOVERWRITE);
        if (ret2 > 0)
            fSuccess = false;
    }
    ptxn->commit(0);
    pdbCopy->close(0);

    return fSuccess;
}

bool CWalletDB::Recover(CDBEnv& dbenv, const std::string& filename)
{
    return CWalletDB::Recover(dbenv, filename, false);
}

bool CWalletDB::WriteDestData(const std::string& address, const std::string& key, const std::string& value)
{
    nWalletDBUpdated++;
    return Write(std::make_pair(std::string("destdata"), std::make_pair(address, key)), value);
}

bool CWalletDB::EraseDestData(const std::string& address, const std::string& key)
{
    nWalletDBUpdated++;
    return Erase(std::make_pair(std::string("destdata"), std::make_pair(address, key)));
}

bool CWalletDB::WriteHDChain(const CHDChain& chain)
{
    nWalletDBUpdated++;
    return Write(std::string("hdchain"), chain);
}

bool CWalletDB::WriteHDSeed(const CHDSeed& seed)
{
    nWalletDBUpdated++;
    return Write(std::make_pair(std::string("hdseed"), seed.getUUID()), seed);
}

bool CWalletDB::WritePrimarySeed(const CHDSeed& seed)
{
    nWalletDBUpdated++;
    return Write(std::string("primaryseed"), seed.getUUID());
}

bool CWalletDB::WritePrimaryAccount(const CAccount* account)
{
    nWalletDBUpdated++;
    return Write(std::string("primaryaccount"), account->getUUID());
}
