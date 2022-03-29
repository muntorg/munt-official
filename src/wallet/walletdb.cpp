// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// File contains modifications by: The Gulden developers
// All modifications:
// Copyright (c) 2016-2020 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#include "wallet/walletdb.h"

#include "base58.h"
#include "consensus/tx_verify.h"
#include "consensus/validation.h"
#include "fs.h"
#include "protocol.h"
#include "serialize.h"
#include "sync.h"
#include "util.h"
#include "util/time.h"
#include "wallet/wallet.h"

#include <atomic>


#include <boost/thread.hpp>

#include <map>

static std::map<CKeyID, int64_t> staticPoolCache;
//
// CWalletDB
//

bool CWalletDB::WriteRecipientName(const std::string& strAddress, const std::string& strName)
{
    return WriteIC(std::pair(std::string("name"), strAddress), strName);
}

bool CWalletDB::EraseRecipientName(const std::string& strAddress)
{
    // This should only be used for sending addresses, never for receiving addresses,
    // receiving addresses must always have an address book entry if they're not change return.
    return EraseIC(std::pair(std::string("name"), strAddress));
}

bool CWalletDB::WriteRecipientDescription(const std::string& strAddress, const std::string& strDescription)
{
    return WriteIC(std::pair(std::string("rcp_description"), strAddress), strDescription);
}

bool CWalletDB::EraseRecipientDescription(const std::string& strDescription)
{
    return EraseIC(std::pair(std::string("rcp_description"), strDescription));
}

bool CWalletDB::WriteRecipientPurpose(const std::string& strAddress, const std::string& strPurpose)
{
    return WriteIC(std::pair(std::string("purpose"), strAddress), strPurpose);
}

bool CWalletDB::EraseRecipientPurpose(const std::string& strPurpose)
{
    return EraseIC(std::pair(std::string("purpose"), strPurpose));
}

bool CWalletDB::WriteTx(const CWalletTx& wtx)
{
    auto hash = wtx.GetHash();

    // Remove old format if present as we are trying to remove it
    EraseIC(std::pair(std::string("tx"), hash));

    // Write only latest format
    return WriteIC(std::pair(std::string("wtx"), hash), wtx);
}

bool CWalletDB::EraseTx(uint256 hash)
{
    // Remove old format if present.
    bool erasedOld = EraseIC(std::pair(std::string("tx"), hash));
    
    // Remove new format if present.
    bool erasedNew = EraseIC(std::pair(std::string("wtx"), hash));
    
    // If either succeeded then we erased the transaction and can return true.
    return (erasedOld || erasedNew);
}

bool CWalletDB::EraseKey(const CPubKey& vchPubKey)
{
    bool eraseKey = EraseIC(std::pair(std::string("keyHD"), vchPubKey));
    eraseKey = EraseIC(std::pair(std::string("key"), vchPubKey)) || eraseKey;
    return EraseIC(std::pair(std::string("keymeta"), vchPubKey)) && eraseKey;
}

bool CWalletDB::EraseEncryptedKey(const CPubKey& vchPubKey)
{
    return EraseIC(std::pair(std::string("keymeta"), vchPubKey)) && EraseIC(std::pair(std::string("ckey"), vchPubKey));
}

bool CWalletDB::WriteKey(const CPubKey& vchPubKey, const CPrivKey& vchPrivKey, const CKeyMetadata& keyMeta, const std::string forAccount, int64_t nKeyChain)
{
    if (!WriteIC(std::pair(std::string("keymeta"), vchPubKey), keyMeta, false)) {
        return false;
    }

    // hash pubkey/privkey to accelerate wallet load
    std::vector<unsigned char> vchKey;
    vchKey.reserve(vchPubKey.size() + vchPrivKey.size());
    vchKey.insert(vchKey.end(), vchPubKey.begin(), vchPubKey.end());
    vchKey.insert(vchKey.end(), vchPrivKey.begin(), vchPrivKey.end());

    return WriteIC(std::pair(std::string("key"), vchPubKey), std::tuple(COMPACTSIZEVECTOR(vchPrivKey), Hash( vchKey.begin(), vchKey.end() ), forAccount, nKeyChain), false);
}

bool CWalletDB::WriteKeyOverride(const CPubKey& vchPubKey, const CPrivKey& vchPrivKey, const std::string forAccount, int64_t nKeyChain)
{
    // hash pubkey/privkey to accelerate wallet load
    std::vector<unsigned char> vchKey;
    vchKey.reserve(vchPubKey.size() + vchPrivKey.size());
    vchKey.insert(vchKey.end(), vchPubKey.begin(), vchPubKey.end());
    vchKey.insert(vchKey.end(), vchPrivKey.begin(), vchPrivKey.end());

    //Erase previous value if any.and new privkey is not null
    CPrivKey nullKey;
    if (vchPrivKey == nullKey)
    {
        WriteIC(std::pair(std::string("key"), vchPubKey), std::tuple(COMPACTSIZEVECTOR(vchPrivKey), Hash( vchKey.begin(), vchKey.end() ), forAccount, nKeyChain), false);
        return true;
    }
    EraseIC(std::pair(std::string("key"), vchPubKey));
    return WriteIC(std::pair(std::string("key"), vchPubKey), std::tuple(COMPACTSIZEVECTOR(vchPrivKey), Hash( vchKey.begin(), vchKey.end() ), forAccount, nKeyChain), false);
}

bool CWalletDB::WriteKeyHD(const CPubKey& vchPubKey, const int64_t HDKeyIndex, int64_t keyChain, const CKeyMetadata &keyMeta, const std::string forAccount)
{
    if (!WriteIC(std::pair(std::string("keymeta"), vchPubKey), keyMeta, false))
        return false;

    return WriteIC(std::pair(std::string("keyHD"), vchPubKey), std::tuple(HDKeyIndex, keyChain, forAccount), false);
}

bool CWalletDB::WriteCryptedKey(const CPubKey& vchPubKey, const std::vector<unsigned char>& vchCryptedSecret, const CKeyMetadata &keyMeta, const std::string forAccount, int64_t nKeyChain)
{
    const bool fEraseUnencryptedKey = true;

    if (!WriteIC(std::pair(std::string("keymeta"), vchPubKey), keyMeta)) {
        return false;
    }

    if (!WriteIC(std::pair(std::string("ckey"), vchPubKey), std::tuple(COMPACTSIZEVECTOR(vchCryptedSecret), forAccount, nKeyChain), false)) {
        return false;
    }
    if (fEraseUnencryptedKey)
    {
        EraseIC(std::pair(std::string("key"), vchPubKey));
        EraseIC(std::pair(std::string("wkey"), vchPubKey));
    }

    return true;
}

bool CWalletDB::WriteMasterKey(unsigned int nID, const CMasterKey& kMasterKey)
{
    return WriteIC(std::pair(std::string("mkey"), nID), kMasterKey, true);
}

bool CWalletDB::EraseMasterKey(unsigned int nID)
{
    return EraseIC(std::pair(std::string("mkey"), nID));
}

bool CWalletDB::WriteCScript(const uint160& hash, const CScript& redeemScript)
{
    return WriteIC(std::pair(std::string("cscript"), hash), *(const CScriptBase*)(&redeemScript), false);
}

bool CWalletDB::WriteWatchOnly(const CScript &dest, const CKeyMetadata& keyMeta)
{
    if (!WriteIC(std::pair(std::string("watchmeta"), *(const CScriptBase*)(&dest)), keyMeta)) {
        return false;
    }
    return WriteIC(std::pair(std::string("watchs"), *(const CScriptBase*)(&dest)), '1');
}

bool CWalletDB::EraseWatchOnly(const CScript &dest)
{
    if (!EraseIC(std::pair(std::string("watchmeta"), *(const CScriptBase*)(&dest)))) {
        return false;
    }
    return EraseIC(std::pair(std::string("watchs"), *(const CScriptBase*)(&dest)));
}

bool CWalletDB::WriteBestBlock(const CBlockLocator& locator)
{
    WriteIC(std::string("bestblock"), CBlockLocator()); // Write empty block locator so versions that require a merkle branch automatically rescan
    return WriteIC(std::string("bestblock_nomerkle"), locator);
}

bool CWalletDB::ReadBestBlock(CBlockLocator& locator)
{
    if (batch.Read(std::string("bestblock"), locator) && !locator.vHave.empty()) return true;
    return batch.Read(std::string("bestblock_nomerkle"), locator);
}

bool CWalletDB::EraseLastSPVBlockProcessed()
{
    return EraseIC(std::string("lastspvblock"));
}

bool CWalletDB::WriteLastSPVBlockProcessed(const CBlockLocator& locator, const int64_t time)
{
    return WriteIC(std::string("lastspvblock"), std::pair(locator, time));
}

bool CWalletDB::ReadLastSPVBlockProcessed(CBlockLocator& locator, int64_t& time)
{
    std::pair<CBlockLocator, int64_t> value;
    if (batch.Read(std::string("lastspvblock"), value))
    {
        locator = value.first;
        time = value.second;
        return true;
    }

    return false;
}

bool CWalletDB::WriteMiningAddressString(const std::string& miningAddress)
{
    return WriteIC(std::string("mining_address_string"), miningAddress);
}

bool CWalletDB::ReadMiningAddressString(std::string& miningAddress)
{
    return batch.Read(std::string("mining_address_string"), miningAddress);
}

bool CWalletDB::WriteOrderPosNext(int64_t nOrderPosNext)
{
    return WriteIC(std::string("orderposnext"), nOrderPosNext);
}
/*GULDEN - no default key (accounts)
bool CWalletDB::WriteDefaultKey(const CPubKey& vchPubKey)
{
    return WriteIC(std::string("defaultkey"), vchPubKey);
}
*/
bool CWalletDB::ReadPool(int64_t nPool, CKeyPool& keypool)
{
    return batch.Read(std::pair(std::string("pool"), nPool), keypool);
}

bool CWalletDB::WritePool(int64_t nPool, const CKeyPool& keypool)
{
    staticPoolCache[keypool.vchPubKey.GetID()] = nPool;
    return WriteIC(std::pair(std::string("pool"), nPool), keypool);
}

bool CWalletDB::ErasePool(CWallet* pwallet, int64_t nPool, bool forceErase)
{
    // If account uses a fixed keypool then never remove keys from it.
    if (!forceErase)
    {
        for (auto iter : pwallet->mapAccounts)
        {
            if (iter.second->IsFixedKeyPool() || iter.second->IsMinimalKeyPool())
            {
                if ((iter.second->setKeyPoolExternal.find(nPool) != iter.second->setKeyPoolExternal.end()) || (iter.second->setKeyPoolInternal.find(nPool) != iter.second->setKeyPoolInternal.end()))
                {
                    return true;
                }
            }
        }
    }

    return EraseIC(std::pair(std::string("pool"), nPool));
}

bool CWalletDB::ErasePool(CWallet* pwallet, const CKeyID& id, bool forceErase)
{
    int64_t keyIndex = staticPoolCache[id];
    // If account uses a fixed keypool then never remove keys from it.
    bool allowErase = true;
    if (!forceErase)
    {
        for (auto iter : pwallet->mapAccounts)
        {
            if ((iter.second->IsFixedKeyPool() || iter.second->IsMinimalKeyPool()))
            {
                if ((iter.second->setKeyPoolExternal.find(keyIndex) != iter.second->setKeyPoolExternal.end()) || (iter.second->setKeyPoolInternal.find(keyIndex) != iter.second->setKeyPoolInternal.end()))
                {
                    return true;
                }
            }
        }
    }

    //fixme: (Post-2.1) (CBSU)
    //Remove from internal keypool, key has been used so shouldn't circulate anymore - address will now reside only in address book.
    for (auto iter : pwallet->mapAccounts)
    {
        iter.second->setKeyPoolExternal.erase(keyIndex);
        iter.second->setKeyPoolInternal.erase(keyIndex);
    }

    //Remove from disk
    if (allowErase)
        return EraseIC(std::pair(std::string("pool"), staticPoolCache[id]));
    else
        return true;
}

bool CWalletDB::HasPool(CWallet* pwallet, const CKeyID& id)
{
    //Remove from disk
    return batch.Exists(std::pair(std::string("pool"), staticPoolCache[id]));
}

bool CWalletDB::WriteMinVersion(int nVersion)
{
    return WriteIC(std::string("minversion"), nVersion);
}

bool CWalletDB::WriteAccountLabel(const std::string& strUUID, const std::string& strLabel)
{
    return WriteIC(std::pair(std::string("acclabel"), strUUID), strLabel);
}

bool CWalletDB::EraseAccountLabel(const std::string& strUUID)
{
    return EraseIC(std::pair(std::string("acclabel"), strUUID));
}

bool CWalletDB::WriteAccountCompoundingSettings(const std::string& strUUID, const CAmount compoundAmount)
{
    return WriteIC(std::pair(std::string("acc_compound"), strUUID), compoundAmount);
}

bool CWalletDB::EraseAccountCompoundingSettings(const std::string& strUUID)
{
    return EraseIC(std::pair(std::string("acc_compound"), strUUID));
}

bool CWalletDB::WriteAccountCompoundingPercentSettings(const std::string& strUUID, const int32_t compoundPercent)
{
    return WriteIC(std::pair(std::string("acc_compound_percent"), strUUID), compoundPercent);
}

bool CWalletDB::EraseAccountCompoundingPercentSettings(const std::string& strUUID)
{
    return EraseIC(std::pair(std::string("acc_compound_percent"), strUUID));
}

bool CWalletDB::WriteAccountNonCompoundWitnessEarningsScript(const std::string& strUUID, const CScript& earningsScript)
{
    return WriteIC(std::pair(std::string("acc_non_compound_wit_earn_script"), strUUID), *(const CScriptBase*)(&earningsScript));
}

bool CWalletDB::EraseAccountNonCompoundWitnessEarningsScript(const std::string& strUUID)
{
    return EraseIC(std::pair(std::string("acc_non_compound_wit_earn_script"), strUUID));
}

bool CWalletDB::WriteAccountRewardTemplate(const std::string& strUUID, const CWitnessRewardTemplate& rewardTemplate)
{
    return WriteIC(std::pair(std::string("acc_reward_template"), strUUID), rewardTemplate);
}

bool CWalletDB::EraseAccountRewardTemplate(const std::string& strUUID)
{
    return EraseIC(std::pair(std::string("acc_reward_template"), strUUID));
}

bool CWalletDB::WriteAccount(const std::string& strAccount, const CAccount* account)
{
    if (account->IsHD())
      return WriteIC(std::pair(std::string("acchd"), strAccount), *((CAccountHD*)account));
    else
      return WriteIC(std::pair(std::string("accleg"), strAccount), *account);
}

bool CWalletDB::EraseAccount(const std::string& strAccount, const CAccount* account)
{
    if (account->IsHD())
      return EraseIC(std::pair(std::string("acchd"), strAccount));
    else
      return EraseIC(std::pair(std::string("accleg"), strAccount));
}

bool CWalletDB::WriteAccountingEntry(const uint64_t nAccEntryNum, const CAccountingEntry& acentry)
{
    return WriteIC(std::pair(std::string("acentry"), std::pair(acentry.strAccount, nAccEntryNum)), acentry);
}

CAmount CWalletDB::GetAccountCreditDebit(const std::string& strAccount)
{
    std::list<CAccountingEntry> entries;
    ListAccountCreditDebit(strAccount, entries);

    CAmount nCreditDebit = 0;
    for (const CAccountingEntry& entry : entries)
        nCreditDebit += entry.nCreditDebit;

    return nCreditDebit;
}

void CWalletDB::ListAccountCreditDebit(const std::string& strAccount, std::list<CAccountingEntry>& entries)
{
    bool fAllAccounts = (strAccount == "*");

    Dbc* pcursor = batch.GetCursor();
    if (!pcursor)
        throw std::runtime_error(std::string(__func__) + ": cannot create DB cursor");
    bool setRange = true;
    while (true)
    {
        // Read next record
        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        if (setRange)
            ssKey << std::pair(std::string("acentry"), std::pair((fAllAccounts ? std::string("") : strAccount), uint64_t(0)));
        CDataStream ssValue(SER_DISK, CLIENT_VERSION);
        int ret = batch.ReadAtCursor(pcursor, ssKey, ssValue, setRange);
        setRange = false;
        if (ret == DB_NOTFOUND)
            break;
        else if (ret != 0)
        {
            pcursor->close();
            throw std::runtime_error(std::string(__func__) + ": error scanning DB");
        }

        // Unserialize
        std::string strType;
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

class CWalletScanState {
public:
    unsigned int nKeys;
    unsigned int nCKeys;
    unsigned int nKeyMeta;
    bool fIsEncrypted;
    bool fAnyUnordered;
    int nFileVersion;
    std::vector<uint256> vWalletUpgrade;

    CWalletScanState() {
        nKeys = nCKeys = nKeyMeta = 0;
        fIsEncrypted = false;
        fAnyUnordered = false;
        nFileVersion = 0;
    }
};

bool ReadKeyValue(CWallet* pwallet, CDataStream& ssKey, CDataStream& ssValue, CWalletScanState &wss, std::string& strType, std::string& strErr)
{
    try
    {
        // Unserialize
        // Taking advantage of the fact that pair serialization
        // is just the two items serialized one after the other
        ssKey >> strType;
        if (strType == "name")
        {
            std::string strAddress;
            ssKey >> strAddress;
            ssValue >> pwallet->mapAddressBook[strAddress].name;
        }
        else if (strType == "rcp_description")
        {
            std::string strAddress;
            ssKey >> strAddress;
            ssValue >> pwallet->mapAddressBook[strAddress].description;
        }
        else if (strType == "purpose")
        {
            std::string strAddress;
            ssKey >> strAddress;
            ssValue >> pwallet->mapAddressBook[strAddress].purpose;
        }
        else if (strType == "wtx" || strType == "tx")
        {
            // Work around for a mistake in initial unity (for mobile) rollout
            // Where format changed in a way that wasn't fully 'backwards' compatible with future desktop upgrades.
            // For desktop we introduce 'wtx' to do the upgrade 'right', but on mobile 'tx' is already also upgraded so we need to treat it as such (only on mobile)
            // wtx is treated the same on both and should be preffered in future - we should phase out and remove 'tx' in future.
            //fixme: (FUTURE) - Remove 'tx' completely once wallets are upgraded; may have to write some code to forcefully upgrade all 'tx' to 'wtx'
            #ifndef PLATFORM_MOBILE
                // Force old serialization version
                if (strType == "tx")
                {
                    ssValue.SetVersion(2010000);
                }
            #endif

            uint256 hash;
            ssKey >> hash;
            CWalletTx wtx;
            ssValue >> wtx;
            CValidationState state;

            if (!(CheckTransaction(wtx, state) && (wtx.GetHash() == hash) && state.IsValid()))
            {
                return false;
            }

            // Undo serialize changes in 31600
            if (31404 <= wtx.fTimeReceivedIsTxTime && wtx.fTimeReceivedIsTxTime <= 31703)
            {
                if (!ssValue.empty())
                {
                    char fTmp;
                    char fUnused;
                    ssValue >> fTmp >> fUnused >> wtx.strFromAccount;
                    strErr = strprintf("LoadWallet() upgrading tx ver=%d %d '%s' %s", wtx.fTimeReceivedIsTxTime, fTmp, wtx.strFromAccount, hash.ToString());
                    wtx.fTimeReceivedIsTxTime = fTmp;
                }
                else
                {
                    strErr = strprintf("LoadWallet() repairing tx ver=%d %s", wtx.fTimeReceivedIsTxTime, hash.ToString());
                    wtx.fTimeReceivedIsTxTime = 0;
                }
                wss.vWalletUpgrade.push_back(hash);
            }

            if (wtx.nOrderPos == -1)
                wss.fAnyUnordered = true;

            pwallet->LoadToWallet(wtx);
        }
        else if (strType == "acentry")
        {
            std::string strAccount;
            ssKey >> strAccount;
            uint64_t nNumber;
            ssKey >> nNumber;
            if (nNumber > pwallet->nAccountingEntryNumber) {
                pwallet->nAccountingEntryNumber = nNumber;
            }

            if (!wss.fAnyUnordered)
            {
                CAccountingEntry acentry;
                ssValue >> acentry;
                if (acentry.nOrderPos == -1)
                    wss.fAnyUnordered = true;
            }
        }
        else if (strType == "watchs")
        {
            CScript script;
            ssKey >> *(CScriptBase*)(&script);
            char fYes;
            ssValue >> fYes;
            if (fYes == '1')
                pwallet->LoadWatchOnly(script);

            // Watch-only addresses have no birthday information for now,
            // so set the wallet birthday to the beginning of time.
            pwallet->nTimeFirstKey = 1;
        }
        else if (strType == "keyHD")
        {
            std::string forAccount = "";
            CPubKey vchPubKey;
            ssKey >> vchPubKey;
            if (!vchPubKey.IsValid())
            {
                strErr = "Error reading wallet database: CPubKey corrupt";
                return false;
            }

            int64_t HDKeyIndex;
            int64_t keyChain;
            ssValue >> HDKeyIndex;
            ssValue >> keyChain;
            ssValue >> forAccount;

            if (pwallet->mapAccounts.count(getUUIDFromString(forAccount)) == 0)
            {
                strErr = "Wallet contains key for non existent HD account";
                return false;
            }

            if (!pwallet->LoadHDKey(HDKeyIndex, keyChain, vchPubKey, forAccount))
            {
                strErr = "Error reading wallet database: LoadKey (HD) failed";
                return false;
            }
        }
        else if (strType == "key" || strType == "wkey")
        {
            std::string forAccount = "";
            CPubKey vchPubKey;
            ssKey >> vchPubKey;
            if (!vchPubKey.IsValid())
            {
                strErr = "Error reading wallet database: CPubKey corrupt";
                return false;
            }
            CKey key;
            CPrivKey pkey;
            uint256 hash;

            if (strType == "key")
            {
                wss.nKeys++;
                ssValue >> COMPACTSIZEVECTOR(pkey);
            } else {
                CWalletKey wkey;
                ssValue >> wkey;
                pkey = wkey.vchPrivKey;
            }

            // Old wallets store keys as "key" [pubkey] => [privkey]
            // ... which was slow for wallets with lots of keys, because the public key is re-derived from the private key
            // using EC operations as a checksum.
            // Newer wallets store keys as "key"[pubkey] => [privkey][hash(pubkey,privkey)], which is much faster while
            // remaining backwards-compatible.
            int64_t nKeyChain;
            try
            {
                ssValue >> hash;
                //1.6.0 wallets and upward store keys by account - older wallets will just lump all keys into a default account.
                ssValue >> forAccount;
                ssValue >> nKeyChain;
            }
            catch (...)
            {
                forAccount = getUUIDAsString(pwallet->activeAccount->getUUID());
                nKeyChain = KEYCHAIN_EXTERNAL;
            }

            if (strType == "key" && GetBoolArg("-skipplainkeys", false))
            {
                LogPrintf("Skipping unencrypted key [skipplainkeys] [%s]\n", CNativeAddress(vchPubKey.GetID()).ToString());
            }
            else
            {
                bool fSkipCheck = false;

                if (!hash.IsNull())
                {
                    // hash pubkey/privkey to accelerate wallet load
                    std::vector<unsigned char> vchKey;
                    vchKey.reserve(vchPubKey.size() + pkey.size());
                    vchKey.insert(vchKey.end(), vchPubKey.begin(), vchPubKey.end());
                    vchKey.insert(vchKey.end(), pkey.begin(), pkey.end());

                    if (Hash(vchKey.begin(), vchKey.end()) != hash)
                    {
                        strErr = "Error reading wallet database: CPubKey/CPrivKey corrupt";
                        return false;
                    }

                    fSkipCheck = true;
                }

                if (!key.Load(pkey, vchPubKey, fSkipCheck))
                {
                    if ( pwallet->mapAccounts.count(getUUIDFromString(forAccount)) == 0 )
                    {
                        strErr = "Wallet contains key for non existent account";
                        return false;
                    }
                    CAccount* targetAccount = pwallet->mapAccounts[getUUIDFromString(forAccount)];
                    if (targetAccount->IsHD() && targetAccount->IsPoW2Witness())
                    {
                        //NULL key was expected in this case.
                        key = CKey();
                    }
                    else
                    {
                        strErr = "Error reading wallet database: CPrivKey corrupt";
                        return false;
                    }
                }
                if (!pwallet->LoadKey(key, vchPubKey, forAccount, nKeyChain))
                {
                    strErr = "Error reading wallet database: LoadKey failed";
                    return false;
                }
            }
        }
        else if (strType == "mkey")
        {
            unsigned int nID;
            ssKey >> nID;
            CMasterKey kMasterKey;
            ssValue >> kMasterKey;
            if(pwallet->mapMasterKeys.count(nID) != 0)
            {
                strErr = strprintf("Error reading wallet database: duplicate CMasterKey id %u", nID);
                return false;
            }
            pwallet->mapMasterKeys[nID] = kMasterKey;
            if (pwallet->nMasterKeyMaxID < nID)
                pwallet->nMasterKeyMaxID = nID;
        }
        else if (strType == "ckey")
        {
            std::string forAccount="";
            int64_t nKeyChain;
            CPubKey vchPubKey;
            ssKey >> vchPubKey;
            if (!vchPubKey.IsValid())
            {
                strErr = "Error reading wallet database: CPubKey corrupt";
                return false;
            }
            std::vector<unsigned char> vchPrivKey;
            ssValue >> COMPACTSIZEVECTOR(vchPrivKey);
            wss.nCKeys++;
            try
            {
                //1.6.0 wallets and upward store keys by account - older wallets will just lump all keys into a default account.
                ssValue >> forAccount;
                ssValue >> nKeyChain;
            }
            catch (...)
            {
                forAccount = getUUIDAsString(pwallet->activeAccount->getUUID());
                nKeyChain = KEYCHAIN_EXTERNAL;
            }

            if (GetBoolArg("-skipplainkeys", false))
            {
                LogPrintf("Load crypted key [skipplainkeys] [%s]\n", CNativeAddress(vchPubKey.GetID()).ToString());
            }

            if (!pwallet->LoadCryptedKey(vchPubKey, vchPrivKey, forAccount, nKeyChain))
            {
                strErr = "Error reading wallet database: LoadCryptedKey failed";
                return false;
            }
            wss.fIsEncrypted = true;
        }
        else if (strType == "keymeta")
        {
            CPubKey vchPubKey;
            ssKey >> vchPubKey;
            CKeyMetadata keyMeta;
            ssValue >> keyMeta;
            wss.nKeyMeta++;

            pwallet->LoadKeyMetadata(vchPubKey, keyMeta);

            // find earliest key creation time, as wallet birthday
            if (!pwallet->nTimeFirstKey ||
                (keyMeta.nCreateTime < int64_t(pwallet->nTimeFirstKey)))
                pwallet->nTimeFirstKey = keyMeta.nCreateTime;
        }
        else if (strType == "pool")
        {
            int64_t nIndex;
            ssKey >> nIndex;
            CKeyPool keypool;
            ssValue >> keypool;

            CAccount* forAccount = NULL;
            std::string accountUUID = keypool.accountName;
            // If we are importing an old legacy (pre HD) wallet - then this keypool becomes the keypool of our 'legacy' account
            if (accountUUID.empty())
            {
                forAccount = pwallet->activeAccount;
                keypool.nChain = KEYCHAIN_EXTERNAL;
            }
            else
            {
                if ( pwallet->mapAccounts.count(getUUIDFromString(accountUUID)) == 0 )
                {
                    strErr = "Wallet contains key for non existent account";
                    return false;
                }
                forAccount = pwallet->mapAccounts[getUUIDFromString(accountUUID)];
            }

            if (keypool.nChain == KEYCHAIN_EXTERNAL)
            {
                forAccount->setKeyPoolExternal.insert(nIndex);
            }
            else
            {
                forAccount->setKeyPoolInternal.insert(nIndex);
            }

            pwallet->LoadKeyPool(nIndex, keypool);

            staticPoolCache[keypool.vchPubKey.GetID()] = nIndex;
        }
        else if (strType == "version")
        {
            ssValue >> wss.nFileVersion;
            if (wss.nFileVersion == 10300)
                wss.nFileVersion = 300;
        }
        else if (strType == "cscript")
        {
            uint160 hash;
            ssKey >> hash;
            CScript script;
            ssValue >> *(CScriptBase*)(&script);
            if (!pwallet->LoadCScript(script))
            {
                strErr = "Error reading wallet database: LoadCScript failed";
                return false;
            }
        }
        else if (strType == "orderposnext")
        {
            ssValue >> pwallet->nOrderPosNext;
        }
        else if (strType == "destdata")
        {
            std::string strAddress, strKey, strValue;
            ssKey >> strAddress;
            ssKey >> strKey;
            ssValue >> strValue;
            if (!pwallet->LoadDestData(CNativeAddress(strAddress).Get(), strKey, strValue))
            {
                strErr = "Error reading wallet database: LoadDestData failed";
                return false;
            }
        }
        else if (strType == "hdseed")
        {
            CHDSeed* newSeed = new CHDSeed();
            ssValue >> *newSeed;
            if (!pwallet->activeSeed)
                pwallet->activeSeed = newSeed;
            pwallet->mapSeeds[newSeed->getUUID()] = newSeed;
        }
        else if (strType == "primaryseed")
        {
            //Do nothing - aleady handled in first pass through
        }
        else if (strType == "primaryaccount")
        {
            //Do nothing - aleady handled in first pass through
        }
        else if (strType == "acc")
        {
            //Throw old 'accounts' away.
        }
        else if (strType == "accleg")
        {
            std::string strAccountUUID;
            ssKey >> strAccountUUID;
            if (pwallet->mapAccounts.count(getUUIDFromString(strAccountUUID)) == 0)
            {
                CAccount* newAccount = new CAccount();
                newAccount->setUUID(strAccountUUID);
                ssValue >> *newAccount;
                pwallet->mapAccounts[getUUIDFromString(strAccountUUID)] = newAccount;
                //If no active account saved (for whatever reason) - make the first one we run into the active one.
                if (!pwallet->activeAccount)
                    pwallet->activeAccount = newAccount;
            }
        }
        else if (strType == "acchd")
        {
            std::string strAccountUUID;
            ssKey >> strAccountUUID;
            if (pwallet->mapAccounts.count(getUUIDFromString(strAccountUUID)) == 0)
            {
                CAccountHD* newAccount = new CAccountHD();
                newAccount->setUUID(strAccountUUID);
                ssValue >> *newAccount;
                pwallet->mapAccounts[getUUIDFromString(strAccountUUID)] = newAccount;
                if (!pwallet->activeAccount)
                    pwallet->activeAccount = newAccount;
            }
        }
        else if (strType == "acclabel")
        {
            std::string strAccountUUID;
            std::string strAccountLabel;
            ssKey >> strAccountUUID;
            ssValue >> strAccountLabel;

            pwallet->mapAccountLabels[getUUIDFromString(strAccountUUID)] = strAccountLabel;
        }
        else if (strType == "acc_compound")
        {
            std::string accountUUID;
            CAmount compoundAmount;

            ssKey >> accountUUID;
            ssValue >> compoundAmount;

            auto findIter = pwallet->mapAccounts.find(getUUIDFromString(accountUUID));
            if (findIter != pwallet->mapAccounts.end())
            {
                findIter->second->setCompounding(compoundAmount, nullptr);
            }
            else
            {
                strErr = "Error reading compound status for account";
                return false;
            }
        }
        else if (strType == "acc_compound_percent")
        {
            std::string accountUUID;
            int32_t compoundPercent;

            ssKey >> accountUUID;
            ssValue >> compoundPercent;

            auto findIter = pwallet->mapAccounts.find(getUUIDFromString(accountUUID));
            if (findIter != pwallet->mapAccounts.end())
            {
                findIter->second->setCompoundingPercent(compoundPercent, nullptr);
            }
            else
            {
                strErr = "Error reading compound status for account";
                return false;
            }
        }
        else if (strType == "acc_non_compound_wit_earn_script")
        {
            std::string accountUUID;
            CScript earningsScript;

            ssKey >> accountUUID;
            ssValue >> *(CScriptBase*)(&earningsScript);

            auto findIter = pwallet->mapAccounts.find(getUUIDFromString(accountUUID));
            if (findIter != pwallet->mapAccounts.end())
            {
                findIter->second->setNonCompoundRewardScript(earningsScript, nullptr);
            }
            else
            {
                strErr = strprintf("Error reading compound script for account [%s]", accountUUID);
                return false;
            }
        }
        else if (strType == "acc_reward_template")
        {
            std::string accountUUID;
            CWitnessRewardTemplate rewardTemplate;

            ssKey >> accountUUID;
            ssValue >> rewardTemplate;

            auto findIter = pwallet->mapAccounts.find(getUUIDFromString(accountUUID));
            if (findIter != pwallet->mapAccounts.end())
            {
                findIter->second->setRewardTemplate(rewardTemplate, nullptr);
            }
            else
            {
                strErr = strprintf("Error reading reward template for account [%s]", accountUUID);
                return false;
            }
        }
    }
    catch (...)
    {
        return false;
    }
    return true;
}

bool CWalletDB::IsKeyType(const std::string& strType)
{
    return (strType== "key" || strType == "wkey" ||
            strType == "mkey" || strType == "ckey");
}

DBErrors CWalletDB::LoadWallet(CWallet* pwallet, WalletLoadState& nExtraLoadState)
{
    CWalletScanState wss;
    bool fNoncriticalErrors = false;
    DBErrors result = DB_LOAD_OK;

    std::string primaryAccountString;
    std::string primarySeedString;
    try {
        LOCK(pwallet->cs_wallet);
        int nMinVersion = 0;
        if (batch.Read((std::string)"minversion", nMinVersion))
        {
            if (nMinVersion > CLIENT_VERSION)
                return DB_TOO_NEW;
            pwallet->LoadMinVersion(nMinVersion);
        }


        bool isPreHDWallet=false;
        bool haveAnyAccounts=false;
        // Accounts first
        {
            // Get cursor
            Dbc* pcursor = batch.GetCursor();
            if (!pcursor)
            {
                LogPrintf("Error getting wallet database cursor\n");
                return DB_CORRUPT;
            }
            while (true)
            {
                // Read next record
                CDataStream ssKey(SER_DISK, CLIENT_VERSION);
                CDataStream ssValue(SER_DISK, CLIENT_VERSION);
                int ret = batch.ReadAtCursor(pcursor, ssKey, ssValue);
                if (ret == DB_NOTFOUND)
                    break;
                else if (ret != 0)
                {
                    LogPrintf("Error reading next record from wallet database\n");
                    return DB_CORRUPT;
                }

                std::string sKey;
                ssKey >> sKey;
                if (sKey == "accleg" || sKey == "acchd")
                {
                    haveAnyAccounts = true;
                    CDataStream ssKey2(SER_DISK, CLIENT_VERSION);
                    std::string accountUUID;
                    ssKey >> accountUUID;
                    ssKey2 << sKey << accountUUID;
                    // Try to be tolerant of single corrupt records:
                    std::string strType, strErr;
                    if (!ReadKeyValue(pwallet, ssKey2, ssValue, wss, strType, strErr))
                    {
                        // losing keys is considered a catastrophic error, anything else
                        // we assume the user can live with:
                        if (IsKeyType(strType))
                            result = DB_CORRUPT;
                        else
                        {
                            // Leave other errors alone, if we try to fix them we might make things worse.
                            fNoncriticalErrors = true; // ... but do warn the user there is something wrong.
                            // Rescan if there is a bad transaction record:
                            if (strType == "tx" || strType == "wtx")
                            {
                                SoftSetBoolArg("-rescan", true);
                            }
                        }
                    }
                    if (!strErr.empty())
                        LogPrintf("%s\n", strErr);
                }
                else if (sKey == "primaryseed")
                {
                    ssValue >> primarySeedString;
                }
                else if (sKey == "primaryaccount")
                {
                    ssValue >> primaryAccountString;
                }
                else if (sKey == "defaultkey")
                {
                    isPreHDWallet = true;
                }
            }
            pcursor->close();
        }


        nExtraLoadState = NEW_WALLET;
        if (!primaryAccountString.empty())
        {
            nExtraLoadState = EXISTING_WALLET;
            if (pwallet->mapAccounts.count(getUUIDFromString(primaryAccountString)) == 0)
            {
                LogPrintf("Error - missing primary account for UUID [%s]\n", primaryAccountString);
                fNoncriticalErrors = true;
            }
            else
            {
                pwallet->activeAccount = pwallet->mapAccounts[getUUIDFromString(primaryAccountString)];
            }
        }
        else if (isPreHDWallet && !haveAnyAccounts)
        {
            nExtraLoadState = EXISTING_WALLET_OLDACCOUNTSYSTEM;

            //Upgrade old legacy wallet - set active account - all the old keys will just land up in this.
            if (pwallet->activeAccount == NULL && pwallet->activeSeed == NULL)
            {
                try
                {
                    fs::path oldPath = bitdb.strPath;
                    oldPath = oldPath / pwallet->dbw->GetName();
                    fs::path backupPath = oldPath;
                    backupPath.replace_extension(".old.preHD");
                    fs::copy_file(oldPath, backupPath);
                }
                catch(...)
                {
                    //We don't care enough about this to worry - if it fails we just carry on.
                }

                pwallet->activeAccount = new CAccount();
                pwallet->activeAccount->setLabel("Legacy", nullptr);
                pwallet->mapAccounts[pwallet->activeAccount->getUUID()] = pwallet->activeAccount;
                pwallet->mapAccountLabels[pwallet->activeAccount->getUUID()] = "Legacy";
            }
        }
        else if (haveAnyAccounts)
        {
            nExtraLoadState = EXISTING_WALLET;
        }

        // Get cursor
        Dbc* pcursor = batch.GetCursor();
        if (!pcursor)
        {
            LogPrintf("Error getting wallet database cursor\n");
            return DB_CORRUPT;
        }

        while (true)
        {
            // Read next record
            CDataStream ssKey(SER_DISK, CLIENT_VERSION);
            CDataStream ssValue(SER_DISK, CLIENT_VERSION);
            int ret = batch.ReadAtCursor(pcursor, ssKey, ssValue);
            if (ret == DB_NOTFOUND)
                break;
            else if (ret != 0)
            {
                LogPrintf("Error reading next record from wallet database\n");
                return DB_CORRUPT;
            }

            // Try to be tolerant of single corrupt records:
            std::string strType, strErr;
            if (!ReadKeyValue(pwallet, ssKey, ssValue, wss, strType, strErr))
            {
                // losing keys is considered a catastrophic error, anything else
                // we assume the user can live with:
                if (IsKeyType(strType))
                    result = DB_CORRUPT;
                else
                {
                    // Leave other errors alone, if we try to fix them we might make things worse.
                    fNoncriticalErrors = true; // ... but do warn the user there is something wrong.
                    // Rescan if there is a bad transaction record:
                    if (strType == "tx" || strType == "wtx")
                    {
                        SoftSetBoolArg("-rescan", true);
                    }
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
    
    // Batch process code that would otherwise be called for every LoadToWallet call
    pwallet->HandleTransactionsLoaded();

    for (const auto& labelPair : pwallet->mapAccountLabels)
        {
            if (pwallet->mapAccounts.count(labelPair.first) == 0)
            {
                //Definitely a non-crticial error, user will just see a very unfriendly account name that they can manually correct.
                LogPrintf("Error - missing account label for account UUID [%s]\n", labelPair.first);
                fNoncriticalErrors = true;
            }
            else
            {
                pwallet->mapAccounts[labelPair.first]->setLabel(labelPair.second, nullptr);
            }
        }
    if (!primarySeedString.empty())
    {
        if (pwallet->mapSeeds.count(getUUIDFromString(primarySeedString)) == 0)
        {
            //fixme: (2.1) Treat this more severely?
            LogPrintf("Error - missing primary seed for UUID [%s]\n", primarySeedString);
            fNoncriticalErrors = true;
        }
        else
        {
            pwallet->activeSeed = pwallet->mapSeeds[getUUIDFromString(primarySeedString)];
        }
    }


    if (fNoncriticalErrors && result == DB_LOAD_OK)
        result = DB_NONCRITICAL_ERROR;

    // Any wallet corruption at all: skip any rewriting or
    // upgrading, we don't want to make it worse.
    if (result != DB_LOAD_OK)
        return result;

    LogPrintf("nFileVersion = %d\n", wss.nFileVersion);

    LogPrintf("Keys: %u plaintext, %u encrypted, %u w/ metadata, %u total\n",
           wss.nKeys, wss.nCKeys, wss.nKeyMeta, wss.nKeys + wss.nCKeys);

    // nTimeFirstKey is only reliable if all keys have metadata
    if ((wss.nKeys + wss.nCKeys) != wss.nKeyMeta)
        pwallet->nTimeFirstKey = 1; // 0 would be considered 'no value'

    for(uint256 hash : wss.vWalletUpgrade)
        WriteTx(pwallet->mapWallet[hash]);

    // Rewrite encrypted wallets of versions 0.4.0 and 0.5.0rc:
    if (wss.fIsEncrypted && (wss.nFileVersion == 40000 || wss.nFileVersion == 50000))
        return DB_NEED_REWRITE;

    if (wss.nFileVersion < CLIENT_VERSION) // Update
        WriteVersion(CLIENT_VERSION);

    if (wss.fAnyUnordered)
        result = pwallet->ReorderTransactions();

    pwallet->laccentries.clear();
    ListAccountCreditDebit("*", pwallet->laccentries);
    for(CAccountingEntry& entry : pwallet->laccentries) {
        pwallet->wtxOrdered.insert(std::pair(entry.nOrderPos, CWallet::TxPair((CWalletTx*)0, &entry)));
    }

    return result;
}

DBErrors CWalletDB::FindWalletTx(std::vector<uint256>& vTxHash, std::vector<CWalletTx>& vWtx)
{
    bool fNoncriticalErrors = false;
    DBErrors result = DB_LOAD_OK;

    try {
        int nMinVersion = 0;
        if (batch.Read((std::string)"minversion", nMinVersion))
        {
            if (nMinVersion > CLIENT_VERSION)
                return DB_TOO_NEW;
        }

        // Get cursor
        Dbc* pcursor = batch.GetCursor();
        if (!pcursor)
        {
            LogPrintf("Error getting wallet database cursor\n");
            return DB_CORRUPT;
        }

        while (true)
        {
            // Read next record
            CDataStream ssKey(SER_DISK, CLIENT_VERSION);
            CDataStream ssValue(SER_DISK, CLIENT_VERSION);
            int ret = batch.ReadAtCursor(pcursor, ssKey, ssValue);
            if (ret == DB_NOTFOUND)
                break;
            else if (ret != 0)
            {
                LogPrintf("Error reading next record from wallet database\n");
                return DB_CORRUPT;
            }

            std::string strType;
            ssKey >> strType;
            if (strType == "tx" || strType == "wtx")
            {
                // Work around for a mistake in initial unity (for mobile) rollout
                // Where format changed in a way that wasn't fully 'backwards' compatible with future desktop upgrades.
                // For desktop we introduce 'wtx' to do the upgrade 'right', but on mobile 'tx' is already also upgraded so we need to treat it as such (only on mobile)
                // wtx is treated the same on both and should be preffered in future - we should phase out and remove 'tx' in future.
                //fixme: (FUTURE) - Remove 'tx' completely once wallets are upgraded; may have to write some code to forcefully upgrade all 'tx' to 'wtx'
                #ifndef PLATFORM_MOBILE
                    // Force old serialization version
                    if (strType == "tx")
                    {
                        ssValue.SetVersion(2010000);
                    }
                #endif
            
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
    catch (const boost::thread_interrupted&)
    {
        throw;
    }
    catch (...)
    {
        result = DB_CORRUPT;
    }

    if (fNoncriticalErrors && result == DB_LOAD_OK)
    {
        result = DB_NONCRITICAL_ERROR;
    }

    return result;
}

DBErrors CWalletDB::ZapSelectTx(std::vector<uint256>& vTxHashIn, std::vector<uint256>& vTxHashOut)
{
    // build list of wallet TXs and hashes
    std::vector<uint256> vTxHash;
    std::vector<CWalletTx> vWtx;
    DBErrors err = FindWalletTx(vTxHash, vWtx);
    if (err != DB_LOAD_OK) {
        return err;
    }

    std::sort(vTxHash.begin(), vTxHash.end());
    std::sort(vTxHashIn.begin(), vTxHashIn.end());

    // erase each matching wallet TX
    bool delerror = false;
    std::vector<uint256>::iterator it = vTxHashIn.begin();
    for (uint256 hash : vTxHash) {
        while (it < vTxHashIn.end() && (*it) < hash) {
            it++;
        }
        if (it == vTxHashIn.end()) {
            break;
        }
        else if ((*it) == hash) {
            if(!EraseTx(hash)) {
                LogPrint(BCLog::DB, "Transaction was found for deletion but returned database error: %s\n", hash.GetHex());
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

DBErrors CWalletDB::ZapWalletTx(std::vector<CWalletTx>& vWtx)
{
    // build list of wallet TXs
    std::vector<uint256> vTxHash;
    DBErrors err = FindWalletTx(vTxHash, vWtx);
    if (err != DB_LOAD_OK)
        return err;

    // erase each wallet TX
    for (uint256& hash : vTxHash) {
        if (!EraseTx(hash))
            return DB_CORRUPT;
    }

    return DB_LOAD_OK;
}

void MaybeCompactWalletDB()
{
    static std::atomic<bool> fOneThread;
    if (fOneThread.exchange(true)) {
        return;
    }
    if (!GetBoolArg("-flushwallet", DEFAULT_FLUSHWALLET)) {
        return;
    }

    for (CWalletRef pwallet : vpwallets) {
        CWalletDBWrapper& dbh = pwallet->GetDBHandle();

        unsigned int nUpdateCounter = dbh.nUpdateCounter;

        if (dbh.nLastSeen != nUpdateCounter) {
            dbh.nLastSeen = nUpdateCounter;
            dbh.nLastWalletUpdate = GetTime();
        }

        if (dbh.nLastFlushed != nUpdateCounter && GetTime() - dbh.nLastWalletUpdate >= 2) {
            if (CDB::PeriodicFlush(dbh)) {
                dbh.nLastFlushed = nUpdateCounter;
            }
        }
    }

    fOneThread = false;
}

//
// Try to (very carefully!) recover wallet file if there is a problem.
//
bool CWalletDB::Recover(const std::string& filename, void *callbackDataIn, bool (*recoverKVcallback)(void* callbackData, CDataStream ssKey, CDataStream ssValue), std::string& out_backup_filename)
{
    return CDB::Recover(filename, callbackDataIn, recoverKVcallback, out_backup_filename);
}

bool CWalletDB::Recover(const std::string& filename, std::string& out_backup_filename)
{
    // recover without a key filter callback
    // results in recovering all record types
    return CWalletDB::Recover(filename, NULL, NULL, out_backup_filename);
}

bool CWalletDB::RecoverKeysOnlyFilter(void *callbackData, CDataStream ssKey, CDataStream ssValue)
{
    CWallet *dummyWallet = reinterpret_cast<CWallet*>(callbackData);
    CWalletScanState dummyWss;
    std::string strType, strErr;
    bool fReadOK;
    {
        // Required in LoadKeyMetadata():
        LOCK(dummyWallet->cs_wallet);
        fReadOK = ReadKeyValue(dummyWallet, ssKey, ssValue,
                               dummyWss, strType, strErr);
    }
    if (!IsKeyType(strType) && strType != "hdchain")
        return false;
    if (!fReadOK)
    {
        LogPrintf("WARNING: CWalletDB::Recover skipping %s: %s\n", strType, strErr);
        return false;
    }

    return true;
}

bool CWalletDB::VerifyEnvironment(const std::string& walletFile, const fs::path& dataDir, std::string& errorStr)
{
    return CDB::VerifyEnvironment(walletFile, dataDir, errorStr);
}

bool CWalletDB::VerifyDatabaseFile(const std::string& walletFile, const fs::path& dataDir, std::string& warningStr, std::string& errorStr)
{
    return CDB::VerifyDatabaseFile(walletFile, dataDir, warningStr, errorStr, CWalletDB::Recover);
}

bool CWalletDB::WriteDestData(const std::string &address, const std::string &key, const std::string &value)
{
    return WriteIC(std::pair(std::string("destdata"), std::pair(address, key)), value);
}

bool CWalletDB::EraseDestData(const std::string &address, const std::string &key)
{
    return EraseIC(std::pair(std::string("destdata"), std::pair(address, key)));
}

/*
bool CWalletDB::WriteHDChain(const CHDChain& chain)
{
    return WriteIC(std::string("hdchain"), chain);
}
*/

bool CWalletDB::TxnBegin()
{
    return batch.TxnBegin();
}

bool CWalletDB::TxnCommit()
{
    return batch.TxnCommit();
}

bool CWalletDB::TxnAbort()
{
    return batch.TxnAbort();
}

bool CWalletDB::ReadVersion(int& nVersion)
{
    return batch.ReadVersion(nVersion);
}

bool CWalletDB::WriteVersion(int nVersion)
{
    return batch.WriteVersion(nVersion);
}

bool CWalletDB::WriteHDSeed(const CHDSeed& seed)
{
    return WriteIC(std::pair(std::string("hdseed"), getUUIDAsString(seed.getUUID())), seed);
}

bool CWalletDB::DeleteHDSeed(const CHDSeed& seed)
{
    return EraseIC(std::pair(std::string("hdseed"), getUUIDAsString(seed.getUUID())));
}

bool CWalletDB::WritePrimarySeed(const CHDSeed& seed)
{
    return WriteIC(std::string("primaryseed"), getUUIDAsString(seed.getUUID()));
}

bool CWalletDB::ErasePrimarySeed()
{
    return EraseIC(std::string("primaryseed"));
}

bool CWalletDB::WritePrimaryAccount(const CAccount* account)
{
    return WriteIC(std::string("primaryaccount"), getUUIDAsString(account->getUUID()));
}

bool CWalletDB::ErasePrimaryAccount()
{
    return EraseIC(std::string("primaryaccount"));
}
