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

#include "wallet/wallet.h"
#include "wallet/walletdb.h"

#include "base58.h"
#include "checkpoints.h"
#include "chain.h"
#include "coincontrol.h"
#include "consensus/consensus.h"
#include "consensus/validation.h"
#include "key.h"
#include "keystore.h"
#include "main.h"
#include "net.h"
#include "policy/policy.h"
#include "primitives/block.h"
#include "primitives/transaction.h"
#include "script/script.h"
#include "script/sign.h"
#include "timedata.h"
#include "txmempool.h"
#include "util.h"
#include "ui_interface.h"
#include "utilmoneystr.h"
#include "account.h"

#include <assert.h>
#include "script/ismine.h"

#include <boost/algorithm/string/replace.hpp>
#include <boost/filesystem.hpp>
#include <boost/thread.hpp>

#include <Gulden/Common/scrypt.h>
#include <Gulden/guldenapplication.h>
#include <Gulden/mnemonic.h>

using namespace std;

CWallet* pwalletMain = NULL;
/** Transaction fee set by the user */
CFeeRate payTxFee(DEFAULT_TRANSACTION_FEE);
unsigned int nTxConfirmTarget = DEFAULT_TX_CONFIRM_TARGET;
bool bSpendZeroConfChange = DEFAULT_SPEND_ZEROCONF_CHANGE;
bool fSendFreeTransactions = DEFAULT_SEND_FREE_TRANSACTIONS;

const char* DEFAULT_WALLET_DAT = "wallet.dat";

void ThreadShadowPoolManager()
{
    boost::mutex condition_mutex;
    boost::unique_lock<boost::mutex> lock(condition_mutex);

    int depth = 1;
    while (true) {
        long milliSleep = 100;

        if (pwalletMain) {
            LOCK(pwalletMain->cs_wallet);

            milliSleep = 500;

            int numNew = 0;
            bool dolock = true;
            for (const auto& seedIter : pwalletMain->mapSeeds) {

                if (seedIter.second->m_type != CHDSeed::CHDSeed::BIP44)
                    continue;

                for (const auto shadowSubType : { AccountSubType::Desktop, AccountSubType::Mobi }) {
                    int numShadow = 0;
                    {
                        for (const auto& accountPair : pwalletMain->mapAccounts) {
                            if (accountPair.second->IsHD() && ((CAccountHD*)accountPair.second)->getSeedUUID() == seedIter.second->getUUID()) {
                                if (accountPair.second->m_SubType == shadowSubType) {
                                    if (accountPair.second->m_Type == AccountType::Shadow) {
                                        ++numShadow;
                                    }
                                }
                            }
                        }
                    }
                    if (numShadow < 10) {
                        dolock = false;
                        if (!pwalletMain->IsLocked()) {
                            pwalletMain->delayLock = true;
                            CWalletDB db(pwalletMain->strWalletFile);
                            while (numShadow < 10) {
                                ++numShadow;
                                ++numNew;
                                depth = 1;

                                CAccountHD* newShadow = seedIter.second->GenerateAccount(shadowSubType, &db);

                                if (newShadow == NULL)
                                    break;

                                newShadow->m_Type = AccountType::Shadow;

                                pwalletMain->addAccount(newShadow, "Shadow");

                                if (numNew > 2) {
                                    milliSleep = 100;
                                    break;
                                }
                            }
                        } else {
                            pwalletMain->wantDelayLock = true;
                            if (numShadow < 2) {
                                uiInterface.RequestUnlock(pwalletMain, _("Wallet unlock required for account creation"));
                            }
                        }
                    }
                }
            }

            if (numNew == 0) {
                if (pwalletMain->TopUpKeyPool(depth, 5)) {
                    if (depth < GetArg("-keypool", 40)) {
                        ++depth;
                    }
                    if (depth < 10) {
                        milliSleep = 100;
                        dolock = false;
                    } else if (depth < 20) {
                        dolock = false;
                        milliSleep = 300;
                    } else {
                        milliSleep = 600;
                    }
                }
            }
            if (dolock) {
                if (pwalletMain->didDelayLock) {
                    pwalletMain->delayLock = false;
                    pwalletMain->wantDelayLock = false;
                    pwalletMain->didDelayLock = false;
                    pwalletMain->Lock();
                }
            }
        }

        boost::this_thread::interruption_point();
        MilliSleep(milliSleep);
    }
}

void StartShadowPoolManagerThread(boost::thread_group& threadGroup)
{
    threadGroup.create_thread(boost::bind(&TraceThread<void (*)()>, "shadowpoolmanager", &ThreadShadowPoolManager));
}

/**
 * Fees smaller than this (in satoshi) are considered zero fee (for transaction creation)
 * Override with -mintxfee
 */
CFeeRate CWallet::minTxFee = CFeeRate(DEFAULT_TRANSACTION_MINFEE);
/**
 * If fee estimation does not have enough data to provide estimates, use this fee instead.
 * Has no effect if not using fee estimation
 * Override with -fallbackfee
 */
CFeeRate CWallet::fallbackFee = CFeeRate(DEFAULT_FALLBACK_FEE);

const uint256 CMerkleTx::ABANDON_HASH(uint256S("0000000000000000000000000000000000000000000000000000000000000001"));

/** @defgroup mapWallet
 *
 * @{
 */

struct CompareValueOnly {
    bool operator()(const pair<CAmount, pair<const CWalletTx*, unsigned int> >& t1,
                    const pair<CAmount, pair<const CWalletTx*, unsigned int> >& t2) const
    {
        return t1.first < t2.first;
    }
};

std::string COutput::ToString() const
{
    return strprintf("COutput(%s, %d, %d) [%s]", tx->GetHash().ToString(), i, nDepth, FormatMoney(tx->vout[i].nValue));
}

isminetype IsMine(const CWallet& wallet, const CTxDestination& dest)
{
    LOCK(wallet.cs_wallet);

    isminetype ret = isminetype::ISMINE_NO;
    for (const auto& accountItem : wallet.mapAccounts) {
        for (auto keyChain : { KEYCHAIN_EXTERNAL, KEYCHAIN_CHANGE }) {
            isminetype temp = (keyChain == KEYCHAIN_EXTERNAL ? IsMine(accountItem.second->externalKeyStore, dest) : IsMine(accountItem.second->internalKeyStore, dest));
            if (temp > ret)
                ret = temp;
        }
    }
    return ret;
}

isminetype IsMine(const CWallet& wallet, const CScript& scriptPubKey)
{
    LOCK(wallet.cs_wallet);

    isminetype ret = isminetype::ISMINE_NO;
    for (const auto& accountItem : wallet.mapAccounts) {
        for (auto keyChain : { KEYCHAIN_EXTERNAL, KEYCHAIN_CHANGE }) {
            isminetype temp = (keyChain == KEYCHAIN_EXTERNAL ? IsMine(accountItem.second->externalKeyStore, scriptPubKey) : IsMine(accountItem.second->internalKeyStore, scriptPubKey));
            if (temp > ret)
                ret = temp;
        }
    }
    return ret;
}

CWallet::~CWallet()
{
    delete pwalletdbEncryption;
    pwalletdbEncryption = NULL;
}

const CWalletTx* CWallet::GetWalletTx(const uint256& hash) const
{
    LOCK(cs_wallet);
    std::map<uint256, CWalletTx>::const_iterator it = mapWallet.find(hash);
    if (it == mapWallet.end())
        return NULL;
    return &(it->second);
}

bool fShowChildAccountsSeperately = false;

CPubKey CWallet::GenerateNewKey(CAccount& forAccount, int keyChain)
{
    AssertLockHeld(cs_wallet); // mapKeyMetadata

    int64_t nCreationTime = GetTime();
    CKeyMetadata metadata(nCreationTime);

    CPubKey pubkey = forAccount.GenerateNewKey(*this, keyChain);
    mapKeyMetadata[pubkey.GetID()] = metadata;
    if (!nTimeFirstKey || nCreationTime < nTimeFirstKey)
        nTimeFirstKey = nCreationTime;

    return pubkey;
}

bool CWallet::AddKeyPubKey(const CKey& secret, const CPubKey& pubkey, CAccount& forAccount, int nKeyChain)
{
    AssertLockHeld(cs_wallet); // mapKeyMetadata
    if (!forAccount.AddKeyPubKey(secret, pubkey, nKeyChain))
        return false;

    CScript script;
    script = GetScriptForDestination(pubkey.GetID());
    if (HaveWatchOnly(script))
        RemoveWatchOnly(script);
    script = GetScriptForRawPubKey(pubkey);
    if (HaveWatchOnly(script))
        RemoveWatchOnly(script);

    if (!fFileBacked)
        return true;
    if (!IsCrypted()) {
        return CWalletDB(strWalletFile).WriteKey(pubkey, secret.GetPrivKey(), mapKeyMetadata[pubkey.GetID()], forAccount.getUUID(), nKeyChain);
    } else {
        std::vector<unsigned char> encryptedKeyOut;
        if (nKeyChain == KEYCHAIN_EXTERNAL) {
            if (!forAccount.externalKeyStore.GetKey(pubkey.GetID(), encryptedKeyOut)) {
                return false;
            }
            return CWalletDB(strWalletFile).WriteCryptedKey(pubkey, encryptedKeyOut, mapKeyMetadata[pubkey.GetID()], forAccount.getUUID(), nKeyChain);
        } else {
            if (!forAccount.internalKeyStore.GetKey(pubkey.GetID(), encryptedKeyOut)) {
                return false;
            }
            return CWalletDB(strWalletFile).WriteCryptedKey(pubkey, encryptedKeyOut, mapKeyMetadata[pubkey.GetID()], forAccount.getUUID(), nKeyChain);
        }
    }
    return true;
}

void CWallet::ForceRewriteKeys(CAccount& forAccount)
{
    if (!fFileBacked)
        return;

    {
        LOCK(cs_wallet);

        std::set<CKeyID> setAddress;
        forAccount.GetKeys(setAddress);

        CWalletDB walletDB(strWalletFile);
        for (const auto& keyID : setAddress) {
            if (!IsCrypted()) {
                CKey keyPair;
                if (!GetKey(keyID, keyPair)) {
                    throw runtime_error("Fatal error upgrading legacy wallet - could not upgrade all keys");
                }
                walletDB.EraseKey(keyPair.GetPubKey());
                if (!walletDB.WriteKey(keyPair.GetPubKey(), keyPair.GetPrivKey(), mapKeyMetadata[keyID], forAccount.getUUID(), KEYCHAIN_EXTERNAL)) {
                    throw runtime_error("Fatal error upgrading legacy wallet - could not write all upgraded keys");
                }
            } else {
                CPubKey pubKey;
                if (!GetPubKey(keyID, pubKey)) {
                    throw runtime_error("Fatal error upgrading legacy wallet - could not upgrade all encrypted keys");
                }
                walletDB.EraseEncryptedKey(pubKey);
                std::vector<unsigned char> secret;
                if (!forAccount.GetKey(keyID, secret)) {
                    throw runtime_error("Fatal error upgrading legacy wallet - could not upgrade all encrypted keys");
                }
                if (!walletDB.WriteCryptedKey(pubKey, secret, mapKeyMetadata[keyID], forAccount.getUUID(), KEYCHAIN_EXTERNAL)) {
                    throw runtime_error("Fatal error upgrading legacy wallet - could not write all upgraded keys");
                }
            }
        }
    }

    CWalletDB walletDB(strWalletFile);
    BOOST_FOREACH (int64_t nIndex, forAccount.setKeyPoolInternal) {
        CKeyPool keypoolentry;
        if (!walletDB.ReadPool(nIndex, keypoolentry)) {
            throw runtime_error("Fatal error upgrading legacy wallet - could not upgrade entire keypool");
        }
        if (!walletDB.ErasePool(this, nIndex)) {
            throw runtime_error("Fatal error upgrading legacy wallet - could not upgrade entire keypool");
        }
        keypoolentry.accountName = forAccount.getUUID();
        keypoolentry.nChain = KEYCHAIN_CHANGE;
        if (!walletDB.WritePool(nIndex, keypoolentry)) {
            throw runtime_error("Fatal error upgrading legacy wallet - could not upgrade entire keypool");
        }
    }
    BOOST_FOREACH (int64_t nIndex, forAccount.setKeyPoolExternal) {
        CKeyPool keypoolentry;
        if (!walletDB.ReadPool(nIndex, keypoolentry)) {
            throw runtime_error("Fatal error upgrading legacy wallet - could not upgrade entire keypool");
        }
        if (!walletDB.ErasePool(this, nIndex)) {
            throw runtime_error("Fatal error upgrading legacy wallet - could not upgrade entire keypool");
        }
        keypoolentry.accountName = forAccount.getUUID();
        keypoolentry.nChain = KEYCHAIN_EXTERNAL;
        if (!walletDB.WritePool(nIndex, keypoolentry)) {
            throw runtime_error("Fatal error upgrading legacy wallet - could not upgrade entire keypool");
        }
    }
}

bool CWallet::AddKeyPubKey(int64_t HDKeyIndex, const CPubKey& pubkey, CAccount& forAccount, int keyChain)
{
    AssertLockHeld(cs_wallet); // mapKeyMetadata
    if (!forAccount.AddKeyPubKey(HDKeyIndex, pubkey, keyChain))
        return false;

    CScript script;
    script = GetScriptForDestination(pubkey.GetID());
    if (HaveWatchOnly(script))
        RemoveWatchOnly(script);
    script = GetScriptForRawPubKey(pubkey);
    if (HaveWatchOnly(script))
        RemoveWatchOnly(script);

    if (!fFileBacked)
        return true;
    return CWalletDB(strWalletFile).WriteKeyHD(pubkey, HDKeyIndex, keyChain, mapKeyMetadata[pubkey.GetID()], forAccount.getUUID());
}

bool CWallet::LoadKeyMetadata(const CPubKey& pubkey, const CKeyMetadata& meta)
{
    AssertLockHeld(cs_wallet); // mapKeyMetadata
    if (meta.nCreateTime && (!nTimeFirstKey || meta.nCreateTime < nTimeFirstKey))
        nTimeFirstKey = meta.nCreateTime;

    mapKeyMetadata[pubkey.GetID()] = meta;
    return true;
}

bool CWallet::LoadCryptedKey(const CPubKey& vchPubKey, const std::vector<unsigned char>& vchCryptedSecret, const std::string& forAccount, int64_t nKeyChain)
{
    LOCK(cs_wallet);

    if (mapAccounts.find(forAccount) == mapAccounts.end())
        return false;

    return mapAccounts[forAccount]->AddCryptedKey(vchPubKey, vchCryptedSecret, nKeyChain);
}

bool CWallet::AddCScript(const CScript& redeemScript)
{
    LOCK(cs_wallet);

    bool ret = false;
    for (auto accountPair : mapAccounts) {
        if (accountPair.second->AddCScript(redeemScript)) {
            ret = true;
            break;
        }
    }

    if (!ret)
        return false;

    if (!fFileBacked)
        return true;
    return CWalletDB(strWalletFile).WriteCScript(Hash160(redeemScript), redeemScript);
}

bool CWallet::LoadCScript(const CScript& redeemScript)
{
    LOCK(cs_wallet);

    /* A sanity check was added in pull #3843 to avoid adding redeemScripts
     * that never can be redeemed. However, old wallets may still contain
     * these. Do not add them to the wallet and warn. */
    if (redeemScript.size() > MAX_SCRIPT_ELEMENT_SIZE) {
        std::string strAddr = CBitcoinAddress(CScriptID(redeemScript)).ToString();
        LogPrintf("%s: Warning: This wallet contains a redeemScript of size %i which exceeds maximum size %i thus can never be redeemed. Do not use address %s.\n",
                  __func__, redeemScript.size(), MAX_SCRIPT_ELEMENT_SIZE, strAddr);
        return true;
    }

    bool ret = false;
    for (auto accountPair : mapAccounts) {
        ret = accountPair.second->AddCScript(redeemScript);
        if (ret == true)
            break;
    }
    return ret;
}

bool CWallet::AddWatchOnly(const CScript& dest)
{
    AssertLockHeld(cs_wallet);

    bool ret = false;
    for (auto accountPair : mapAccounts) {
        if (accountPair.second->AddWatchOnly(dest))
            ret = true;
    }
    if (!ret)
        return false;

    nTimeFirstKey = 1; // No birthday information for watch-only keys.
    NotifyWatchonlyChanged(true);
    if (!fFileBacked)
        return true;
    return CWalletDB(strWalletFile).WriteWatchOnly(dest);
}

bool CWallet::RemoveWatchOnly(const CScript& dest)
{
    AssertLockHeld(cs_wallet);

    bool ret = true;
    for (auto accountPair : mapAccounts) {
        if (accountPair.second->RemoveWatchOnly(dest)) {
            ret = true;
            break;
        }
    }
    if (!ret)
        return false;

    if (!HaveWatchOnly())
        NotifyWatchonlyChanged(false);
    if (fFileBacked)
        if (!CWalletDB(strWalletFile).EraseWatchOnly(dest))
            return false;

    return true;
}

bool CWallet::LoadWatchOnly(const CScript& dest)
{
    AssertLockHeld(cs_wallet);

    bool ret = false;
    for (auto accountPair : mapAccounts) {
        ret = accountPair.second->AddWatchOnly(dest);
        if (ret)
            break;
    }
    return ret;
}

bool CWallet::Unlock(const SecureString& strWalletPassphrase)
{
    CCrypter crypter;
    CKeyingMaterial vMasterKey;

    {
        LOCK(cs_wallet);
        BOOST_FOREACH (const MasterKeyMap::value_type& pMasterKey, mapMasterKeys) {
            if (!crypter.SetKeyFromPassphrase(strWalletPassphrase, pMasterKey.second.vchSalt, pMasterKey.second.nDeriveIterations, pMasterKey.second.nDerivationMethod))
                return false;
            if (!crypter.Decrypt(pMasterKey.second.vchCryptedKey, vMasterKey))
                continue; // try another master key
            for (auto accountPair : mapAccounts) {
                if (!accountPair.second->Unlock(vMasterKey)) {
                    return false;
                }
            }
            for (auto seedPair : mapSeeds) {
                if (!seedPair.second->Unlock(vMasterKey)) {
                    return false;
                }
            }
            return true;
        }
    }
    return false;
}

bool CWallet::ChangeWalletPassphrase(const SecureString& strOldWalletPassphrase, const SecureString& strNewWalletPassphrase)
{
    bool fWasLocked = IsLocked();

    {
        LOCK(cs_wallet);
        Lock();

        CCrypter crypter;
        CKeyingMaterial vMasterKey;
        BOOST_FOREACH (MasterKeyMap::value_type& pMasterKey, mapMasterKeys) {
            if (!crypter.SetKeyFromPassphrase(strOldWalletPassphrase, pMasterKey.second.vchSalt, pMasterKey.second.nDeriveIterations, pMasterKey.second.nDerivationMethod))
                return false;
            if (!crypter.Decrypt(pMasterKey.second.vchCryptedKey, vMasterKey))
                return false;
            if (Unlock(vMasterKey)) {
                int64_t nStartTime = GetTimeMillis();
                crypter.SetKeyFromPassphrase(strNewWalletPassphrase, pMasterKey.second.vchSalt, pMasterKey.second.nDeriveIterations, pMasterKey.second.nDerivationMethod);
                pMasterKey.second.nDeriveIterations = pMasterKey.second.nDeriveIterations * (100 / ((double)(GetTimeMillis() - nStartTime)));

                nStartTime = GetTimeMillis();
                crypter.SetKeyFromPassphrase(strNewWalletPassphrase, pMasterKey.second.vchSalt, pMasterKey.second.nDeriveIterations, pMasterKey.second.nDerivationMethod);
                pMasterKey.second.nDeriveIterations = (pMasterKey.second.nDeriveIterations + pMasterKey.second.nDeriveIterations * 100 / ((double)(GetTimeMillis() - nStartTime))) / 2;

                if (pMasterKey.second.nDeriveIterations < 25000)
                    pMasterKey.second.nDeriveIterations = 25000;

                LogPrintf("Wallet passphrase changed to an nDeriveIterations of %i\n", pMasterKey.second.nDeriveIterations);

                if (!crypter.SetKeyFromPassphrase(strNewWalletPassphrase, pMasterKey.second.vchSalt, pMasterKey.second.nDeriveIterations, pMasterKey.second.nDerivationMethod))
                    return false;
                if (!crypter.Encrypt(vMasterKey, pMasterKey.second.vchCryptedKey))
                    return false;
                CWalletDB(strWalletFile).WriteMasterKey(pMasterKey.first, pMasterKey.second);
                if (fWasLocked)
                    Lock();
                return true;
            }
        }
    }

    return false;
}

void CWallet::SetBestChain(const CBlockLocator& loc)
{
    CWalletDB walletdb(strWalletFile);
    walletdb.WriteBestBlock(loc);
}

bool CWallet::SetMinVersion(enum WalletFeature nVersion, CWalletDB* pwalletdbIn, bool fExplicit)
{
    LOCK(cs_wallet); // nWalletVersion
    if (nWalletVersion >= nVersion)
        return true;

    if (fExplicit && nVersion > nWalletMaxVersion)
        nVersion = FEATURE_LATEST;

    nWalletVersion = nVersion;

    if (nVersion > nWalletMaxVersion)
        nWalletMaxVersion = nVersion;

    if (fFileBacked) {
        CWalletDB* pwalletdb = pwalletdbIn ? pwalletdbIn : new CWalletDB(strWalletFile);
        if (nWalletVersion > 40000)
            pwalletdb->WriteMinVersion(nWalletVersion);
        if (!pwalletdbIn)
            delete pwalletdb;
    }

    return true;
}

bool CWallet::SetMaxVersion(int nVersion)
{
    LOCK(cs_wallet); // nWalletVersion, nWalletMaxVersion

    if (nWalletVersion > nVersion)
        return false;

    nWalletMaxVersion = nVersion;

    return true;
}

set<uint256> CWallet::GetConflicts(const uint256& txid) const
{
    set<uint256> result;
    AssertLockHeld(cs_wallet);

    std::map<uint256, CWalletTx>::const_iterator it = mapWallet.find(txid);
    if (it == mapWallet.end())
        return result;
    const CWalletTx& wtx = it->second;

    std::pair<TxSpends::const_iterator, TxSpends::const_iterator> range;

    BOOST_FOREACH (const CTxIn& txin, wtx.vin) {
        if (mapTxSpends.count(txin.prevout) <= 1)
            continue; // No conflict if zero or one spends
        range = mapTxSpends.equal_range(txin.prevout);
        for (TxSpends::const_iterator it = range.first; it != range.second; ++it)
            result.insert(it->second);
    }
    return result;
}

void CWallet::Flush(bool shutdown)
{
    bitdb.Flush(shutdown);
}

bool CWallet::Verify()
{
    LogPrintf("Using BerkeleyDB version %s\n", DbEnv::version(0, 0, 0));
    std::string walletFile = GetArg("-wallet", DEFAULT_WALLET_DAT);

    LogPrintf("Using wallet %s\n", walletFile);
    uiInterface.InitMessage(_("Verifying wallet..."));

    if (walletFile != boost::filesystem::basename(walletFile) + boost::filesystem::extension(walletFile))
        return InitError(strprintf(_("Wallet %s resides outside data directory %s"), walletFile, GetDataDir().string()));

    if (!bitdb.Open(GetDataDir())) {

        boost::filesystem::path pathDatabase = GetDataDir() / "database";
        boost::filesystem::path pathDatabaseBak = GetDataDir() / strprintf("database.%d.bak", GetTime());
        try {
            boost::filesystem::rename(pathDatabase, pathDatabaseBak);
            LogPrintf("Moved old %s to %s. Retrying.\n", pathDatabase.string(), pathDatabaseBak.string());
        }
        catch (const boost::filesystem::filesystem_error&) {
        }

        if (!bitdb.Open(GetDataDir())) {

            return InitError(strprintf(_("Error initializing wallet database environment %s!"), GetDataDir()));
        }
    }

    if (GetBoolArg("-salvagewallet", false)) {

        if (!CWalletDB::Recover(bitdb, walletFile, true))
            return false;
    }

    if (boost::filesystem::exists(GetDataDir() / walletFile)) {
        CDBEnv::VerifyResult r = bitdb.Verify(walletFile, CWalletDB::Recover);
        if (r == CDBEnv::RECOVER_OK) {
            InitWarning(strprintf(_("Warning: Wallet file corrupt, data salvaged!"
                                    " Original %s saved as %s in %s; if"
                                    " your balance or transactions are incorrect you should"
                                    " restore from a backup."),
                                  walletFile, "wallet.{timestamp}.bak", GetDataDir()));
        }
        if (r == CDBEnv::RECOVER_FAIL)
            return InitError(strprintf(_("%s corrupt, salvage failed"), walletFile));
    }

    return true;
}

void CWallet::SyncMetaData(pair<TxSpends::iterator, TxSpends::iterator> range)
{

    int nMinOrderPos = std::numeric_limits<int>::max();
    const CWalletTx* copyFrom = NULL;
    for (TxSpends::iterator it = range.first; it != range.second; ++it) {
        const uint256& hash = it->second;
        int n = mapWallet[hash].nOrderPos;
        if (n < nMinOrderPos) {
            nMinOrderPos = n;
            copyFrom = &mapWallet[hash];
        }
    }

    for (TxSpends::iterator it = range.first; it != range.second; ++it) {
        const uint256& hash = it->second;
        CWalletTx* copyTo = &mapWallet[hash];
        if (copyFrom == copyTo)
            continue;
        if (!copyFrom->IsEquivalentTo(*copyTo))
            continue;
        copyTo->mapValue = copyFrom->mapValue;
        copyTo->vOrderForm = copyFrom->vOrderForm;

        copyTo->nTimeSmart = copyFrom->nTimeSmart;
        copyTo->fFromMe = copyFrom->fFromMe;
        copyTo->strFromAccount = copyFrom->strFromAccount;
    }
}

/**
 * Outpoint is spent if any non-conflicted transaction
 * spends it:
 */
bool CWallet::IsSpent(const uint256& hash, unsigned int n) const
{
    const COutPoint outpoint(hash, n);
    pair<TxSpends::const_iterator, TxSpends::const_iterator> range;
    range = mapTxSpends.equal_range(outpoint);

    for (TxSpends::const_iterator it = range.first; it != range.second; ++it) {
        const uint256& wtxid = it->second;
        std::map<uint256, CWalletTx>::const_iterator mit = mapWallet.find(wtxid);
        if (mit != mapWallet.end()) {
            int depth = mit->second.GetDepthInMainChain();
            if (depth > 0 || (depth == 0 && !mit->second.isAbandoned()))
                return true; // Spent
        }
    }
    return false;
}

void CWallet::AddToSpends(const COutPoint& outpoint, const uint256& wtxid)
{
    mapTxSpends.insert(make_pair(outpoint, wtxid));

    pair<TxSpends::iterator, TxSpends::iterator> range;
    range = mapTxSpends.equal_range(outpoint);
    SyncMetaData(range);
}

void CWallet::AddToSpends(const uint256& wtxid)
{
    assert(mapWallet.count(wtxid));
    CWalletTx& thisTx = mapWallet[wtxid];
    if (thisTx.IsCoinBase()) // Coinbases don't spend anything!
        return;

    BOOST_FOREACH (const CTxIn& txin, thisTx.vin)
        AddToSpends(txin.prevout, wtxid);
}

bool CWallet::EncryptWallet(const SecureString& strWalletPassphrase)
{
    if (IsCrypted())
        return false;

    CKeyingMaterial vMasterKey;

    vMasterKey.resize(WALLET_CRYPTO_KEY_SIZE);
    GetStrongRandBytes(&vMasterKey[0], WALLET_CRYPTO_KEY_SIZE);

    CMasterKey kMasterKey;

    kMasterKey.vchSalt.resize(WALLET_CRYPTO_SALT_SIZE);
    GetStrongRandBytes(&kMasterKey.vchSalt[0], WALLET_CRYPTO_SALT_SIZE);

    CCrypter crypter;
    int64_t nStartTime = GetTimeMillis();
    crypter.SetKeyFromPassphrase(strWalletPassphrase, kMasterKey.vchSalt, 25000, kMasterKey.nDerivationMethod);
    kMasterKey.nDeriveIterations = 2500000 / ((double)(GetTimeMillis() - nStartTime));

    nStartTime = GetTimeMillis();
    crypter.SetKeyFromPassphrase(strWalletPassphrase, kMasterKey.vchSalt, kMasterKey.nDeriveIterations, kMasterKey.nDerivationMethod);
    kMasterKey.nDeriveIterations = (kMasterKey.nDeriveIterations + kMasterKey.nDeriveIterations * 100 / ((double)(GetTimeMillis() - nStartTime))) / 2;

    if (kMasterKey.nDeriveIterations < 25000)
        kMasterKey.nDeriveIterations = 25000;

    LogPrintf("Encrypting Wallet with an nDeriveIterations of %i\n", kMasterKey.nDeriveIterations);

    if (!crypter.SetKeyFromPassphrase(strWalletPassphrase, kMasterKey.vchSalt, kMasterKey.nDeriveIterations, kMasterKey.nDerivationMethod))
        return false;
    if (!crypter.Encrypt(vMasterKey, kMasterKey.vchCryptedKey))
        return false;

    {
        LOCK(cs_wallet);
        mapMasterKeys[++nMasterKeyMaxID] = kMasterKey;
        if (fFileBacked) {
            assert(!pwalletdbEncryption);
            pwalletdbEncryption = new CWalletDB(strWalletFile);
            if (!pwalletdbEncryption->TxnBegin()) {
                delete pwalletdbEncryption;
                pwalletdbEncryption = NULL;
                return false;
            }
            pwalletdbEncryption->WriteMasterKey(nMasterKeyMaxID, kMasterKey);
        }

        for (auto seedIter : mapSeeds) {
            if (!seedIter.second->Encrypt(vMasterKey)) {
                if (fFileBacked) {
                    pwalletdbEncryption->TxnAbort();
                    delete pwalletdbEncryption;
                }

                assert(false);
            }
            pwalletdbEncryption->WriteHDSeed(*seedIter.second);
        }

        for (auto accountPair : mapAccounts) {
            if (!accountPair.second->Encrypt(vMasterKey)) {
                if (fFileBacked) {
                    pwalletdbEncryption->TxnAbort();
                    delete pwalletdbEncryption;
                }

                assert(false);
            }
            pwalletdbEncryption->WriteAccount(accountPair.second->getUUID(), accountPair.second);
        }

        SetMinVersion(FEATURE_WALLETCRYPT, pwalletdbEncryption, true);

        if (fFileBacked) {
            if (!pwalletdbEncryption->TxnCommit()) {
                delete pwalletdbEncryption;

                assert(false);
            }

            delete pwalletdbEncryption;
            pwalletdbEncryption = NULL;
        }

        for (auto accountPair : mapAccounts) {
            accountPair.second->Lock();
        }
        for (auto seedIter : mapSeeds) {
            seedIter.second->Lock();
        }
        Unlock(strWalletPassphrase);

        NewKeyPool();

        /*for (auto accountPair : mapAccounts)
        {
            accountPair.second->Lock();
        }*/

        CDB::Rewrite(strWalletFile);
    }
    for (auto accountPair : mapAccounts) {
        accountPair.second->internalKeyStore.NotifyStatusChanged(&accountPair.second->internalKeyStore);
        accountPair.second->externalKeyStore.NotifyStatusChanged(&accountPair.second->externalKeyStore);
    }

    return true;
}

int64_t CWallet::IncOrderPosNext(CWalletDB* pwalletdb)
{
    AssertLockHeld(cs_wallet); // nOrderPosNext
    int64_t nRet = nOrderPosNext++;
    if (pwalletdb) {
        pwalletdb->WriteOrderPosNext(nOrderPosNext);
    } else {
        CWalletDB(strWalletFile).WriteOrderPosNext(nOrderPosNext);
    }
    return nRet;
}

bool CWallet::AccountMove(std::string strFrom, std::string strTo, CAmount nAmount, std::string strComment)
{
    assert(0);
    /*CWalletDB walletdb(strWalletFile);
    if (!walletdb.TxnBegin())
        return false;

    int64_t nNow = GetAdjustedTime();


    CAccountingEntry debit;
    debit.nOrderPos = IncOrderPosNext(&walletdb);
    debit.strAccount = strFrom;
    debit.nCreditDebit = -nAmount;
    debit.nTime = nNow;
    debit.strOtherAccount = strTo;
    debit.strComment = strComment;
    AddAccountingEntry(debit, walletdb);


    CAccountingEntry credit;
    credit.nOrderPos = IncOrderPosNext(&walletdb);
    credit.strAccount = strTo;
    credit.nCreditDebit = nAmount;
    credit.nTime = nNow;
    credit.strOtherAccount = strFrom;
    credit.strComment = strComment;
    AddAccountingEntry(credit, walletdb);

    if (!walletdb.TxnCommit())
        return false;*/

    return true;
}

bool CWallet::GetAccountPubkey(CPubKey& pubKey, std::string strAccount, bool bForceNew)
{
    assert(0);

    /*LOCK(cs_wallet);
    
    CWalletDB walletdb(strWalletFile);

    CAccount* account = mapAccounts[strAccount];

    if (account && !bForceNew) {
        if (!account->vchPubKey.IsValid())
            bForceNew = true;
        else {

            CScript scriptPubKey = GetScriptForDestination(account->vchPubKey.GetID());
            for (map<uint256, CWalletTx>::iterator it = mapWallet.begin();
                 it != mapWallet.end() && account->vchPubKey.IsValid();
                 ++it)
                BOOST_FOREACH(const CTxOut& txout, (*it).second.vout)
                    if (txout.scriptPubKey == scriptPubKey) {
                        bForceNew = true;
                        break;
                    }
        }
    }



    if (bForceNew) {
        if (!GetKeyFromPool(account->vchPubKey, account, KEYCHAIN_EXTERNAL))
            return false;

        account = activeSeed->GenerateAccount(Desktop, NULL);
        if (!walletdb.WriteAccount(strAccount, account))
        {
            throw runtime_error("Writing seed failed");
        }




    }

    pubKey = account->vchPubKey;*/

    return false;
}

CAccountHD* CWallet::GenerateNewAccount(std::string strAccount, AccountType type, AccountSubType subType)
{

    assert(type != AccountType::ShadowChild);
    assert(type != AccountType::Deleted);
    CAccountHD* newAccount = NULL;

    if (type == AccountType::Normal || type == AccountType::Shadow) {
        LOCK(cs_wallet);

        for (const auto& accountPair : mapAccounts) {
            if (accountPair.second->m_SubType == subType) {
                if (accountPair.second->m_Type == AccountType::Shadow) {
                    if (!newAccount || ((CAccountHD*)accountPair.second)->getIndex() < newAccount->getIndex())
                        newAccount = (CAccountHD*)accountPair.second;
                }
            }
        }
    }

    if (!newAccount) {
        while (true) {

            {
                LOCK(cs_wallet);
                if (!IsLocked())
                    break;
                wantDelayLock = true;
                uiInterface.RequestUnlock(this, _("Wallet unlock required for account creation"));
            }
            MilliSleep(5000);
        }
        if (!IsLocked()) {
            CWalletDB db(strWalletFile);
            newAccount = activeSeed->GenerateAccount(subType, &db);
        } else {
            return NULL;
        }
    }
    newAccount->m_Type = type;

    addAccount(newAccount, strAccount);

    if (activeAccount) ///
        TopUpKeyPool(2); //We only assign the bare minimum addresses here - and let the background thread do the rest

    return newAccount;
}

void CWallet::MarkDirty()
{
    {
        LOCK(cs_wallet);
        BOOST_FOREACH (PAIRTYPE(const uint256, CWalletTx) & item, mapWallet)
            item.second.MarkDirty();
    }
}

bool CWallet::AddToWallet(const CWalletTx& wtxIn, bool fFromLoadWallet, CWalletDB* pwalletdb)
{
    uint256 hash = wtxIn.GetHash();

    if (fFromLoadWallet) {

        mapWallet[hash] = wtxIn;
        CWalletTx& wtx = mapWallet[hash];
        if (wtx.strFromAccount.empty()) {
            LOCK(cs_wallet);
            for (auto accountPair : mapAccounts) {
                if (accountPair.second->HaveWalletTx(wtx)) {
                    wtx.strFromAccount = accountPair.first;
                }
            }
        }
        wtx.BindWallet(this);
        wtxOrdered.insert(make_pair(wtx.nOrderPos, TxPair(&wtx, (CAccountingEntry*)0)));
        AddToSpends(hash);
        BOOST_FOREACH (const CTxIn& txin, wtx.vin) {
            if (mapWallet.count(txin.prevout.hash)) {
                CWalletTx& prevtx = mapWallet[txin.prevout.hash];
                if (prevtx.nIndex == -1 && !prevtx.hashUnset()) {
                    MarkConflicted(prevtx.hashBlock, wtx.GetHash());
                }
            }
        }
    } else {

        LOCK(cs_wallet);

        pair<map<uint256, CWalletTx>::iterator, bool> ret = mapWallet.insert(make_pair(hash, wtxIn));
        CWalletTx& wtx = (*ret.first).second;
        wtx.BindWallet(this);
        bool fInsertedNew = ret.second;
        if (fInsertedNew) {
            wtx.nTimeReceived = GetAdjustedTime();
            wtx.nOrderPos = IncOrderPosNext(pwalletdb);
            wtxOrdered.insert(make_pair(wtx.nOrderPos, TxPair(&wtx, (CAccountingEntry*)0)));

            wtx.nTimeSmart = wtx.nTimeReceived;
            if (!wtxIn.hashUnset()) {
                if (mapBlockIndex.count(wtxIn.hashBlock)) {
                    int64_t latestNow = wtx.nTimeReceived;
                    int64_t latestEntry = 0;
                    {

                        int64_t latestTolerated = latestNow + 300;
                        const TxItems& txOrdered = wtxOrdered;
                        for (TxItems::const_reverse_iterator it = txOrdered.rbegin(); it != txOrdered.rend(); ++it) {
                            CWalletTx* const pwtx = (*it).second.first;
                            if (pwtx == &wtx)
                                continue;
                            CAccountingEntry* const pacentry = (*it).second.second;
                            int64_t nSmartTime;
                            if (pwtx) {
                                nSmartTime = pwtx->nTimeSmart;
                                if (!nSmartTime)
                                    nSmartTime = pwtx->nTimeReceived;
                            } else
                                nSmartTime = pacentry->nTime;
                            if (nSmartTime <= latestTolerated) {
                                latestEntry = nSmartTime;
                                if (nSmartTime > latestNow)
                                    latestNow = nSmartTime;
                                break;
                            }
                        }
                    }

                    int64_t blocktime = mapBlockIndex[wtxIn.hashBlock]->GetBlockTime();
                    wtx.nTimeSmart = std::max(latestEntry, std::min(blocktime, latestNow));
                } else
                    LogPrintf("AddToWallet(): found %s in block %s not in index\n",
                              wtxIn.GetHash().ToString(),
                              wtxIn.hashBlock.ToString());
            }
            AddToSpends(hash);
        }

        bool fUpdated = false;
        if (!fInsertedNew) {

            if (!wtxIn.hashUnset() && wtxIn.hashBlock != wtx.hashBlock) {
                wtx.hashBlock = wtxIn.hashBlock;
                fUpdated = true;
            }

            if (wtxIn.hashBlock.IsNull() && wtx.isAbandoned()) {
                wtx.hashBlock = wtxIn.hashBlock;
                fUpdated = true;
            }
            if (wtxIn.nIndex != -1 && (wtxIn.nIndex != wtx.nIndex)) {
                wtx.nIndex = wtxIn.nIndex;
                fUpdated = true;
            }
            if (wtxIn.fFromMe && wtxIn.fFromMe != wtx.fFromMe) {
                wtx.fFromMe = wtxIn.fFromMe;
                fUpdated = true;
            }
        }

        LogPrintf("AddToWallet %s  %s%s\n", wtxIn.GetHash().ToString(), (fInsertedNew ? "new" : ""), (fUpdated ? "update" : ""));

        if (fInsertedNew || fUpdated)
            if (!pwalletdb->WriteTx(wtx))
                return false;

        wtx.MarkDirty();

        NotifyTransactionChanged(this, hash, fInsertedNew ? CT_NEW : CT_UPDATED);

        std::string strCmd = GetArg("-walletnotify", "");

        if (!strCmd.empty()) {
            boost::replace_all(strCmd, "%s", wtxIn.GetHash().GetHex());
            boost::thread t(runCommand, strCmd); // thread runs free
        }
    }
    return true;
}

/**
 * Add a transaction to the wallet, or update it.
 * pblock is optional, but should be provided if the transaction is known to be in a block.
 * If fUpdate is true, existing transactions will be updated.
 */
bool CWallet::AddToWalletIfInvolvingMe(const CTransaction& tx, const CBlock* pblock, bool fUpdate)
{
    {
        AssertLockHeld(cs_wallet);

        if (pblock) {
            BOOST_FOREACH (const CTxIn& txin, tx.vin) {
                std::pair<TxSpends::const_iterator, TxSpends::const_iterator> range = mapTxSpends.equal_range(txin.prevout);
                while (range.first != range.second) {
                    if (range.first->second != tx.GetHash()) {
                        LogPrintf("Transaction %s (in block %s) conflicts with wallet transaction %s (both spend %s:%i)\n", tx.GetHash().ToString(), pblock->GetHash().ToString(), range.first->second.ToString(), range.first->first.hash.ToString(), range.first->first.n);
                        MarkConflicted(pblock->GetHash(), range.first->second);
                    }
                    range.first++;
                }
            }
        }

        bool fExisted = mapWallet.count(tx.GetHash()) != 0;
        if (fExisted && !fUpdate)
            return false;
        if (fExisted || IsMine(tx) || IsFromMe(tx)) {
            CWalletTx wtx(this, tx);

            if (pblock)
                wtx.SetMerkleBranch(*pblock);

            CWalletDB walletdb(strWalletFile, "r+", false);

            RemoveAddressFromKeypoolIfIsMine(tx, pblock ? pblock->nTime : 0);

            for (const auto& txin : wtx.vin) {
                RemoveAddressFromKeypoolIfIsMine(txin, pblock ? pblock->nTime : 0);
            }

            for (const auto& txin : tx.vin) {
                CTransaction tx;
                uint256 hashBlock = uint256();
                if (GetTransaction(txin.prevout.hash, tx, Params().GetConsensus(), hashBlock, true)) {
                    AddToWallet(CWalletTx(this, tx), false, &walletdb);
                }
            }

            return AddToWallet(wtx, false, &walletdb);
        }
    }
    return false;
}

bool CWallet::AbandonTransaction(const uint256& hashTx)
{
    LOCK2(cs_main, cs_wallet);

    CWalletDB walletdb(strWalletFile, "r+", false);

    std::set<uint256> todo;
    std::set<uint256> done;

    assert(mapWallet.count(hashTx));
    CWalletTx& origtx = mapWallet[hashTx];
    if (origtx.GetDepthInMainChain() > 0 || origtx.InMempool()) {
        return false;
    }

    todo.insert(hashTx);

    while (!todo.empty()) {
        uint256 now = *todo.begin();
        todo.erase(now);
        done.insert(now);
        assert(mapWallet.count(now));
        CWalletTx& wtx = mapWallet[now];
        int currentconfirm = wtx.GetDepthInMainChain();

        assert(currentconfirm <= 0);

        if (currentconfirm == 0 && !wtx.isAbandoned()) {

            assert(!wtx.InMempool());
            wtx.nIndex = -1;
            wtx.setAbandoned();
            wtx.MarkDirty();
            walletdb.WriteTx(wtx);
            NotifyTransactionChanged(this, wtx.GetHash(), CT_UPDATED);

            TxSpends::const_iterator iter = mapTxSpends.lower_bound(COutPoint(hashTx, 0));
            while (iter != mapTxSpends.end() && iter->first.hash == now) {
                if (!done.count(iter->second)) {
                    todo.insert(iter->second);
                }
                iter++;
            }

            BOOST_FOREACH (const CTxIn& txin, wtx.vin) {
                if (mapWallet.count(txin.prevout.hash))
                    mapWallet[txin.prevout.hash].MarkDirty();
            }
        }
    }

    return true;
}

void CWallet::MarkConflicted(const uint256& hashBlock, const uint256& hashTx)
{
    LOCK2(cs_main, cs_wallet);

    int conflictconfirms = 0;
    if (mapBlockIndex.count(hashBlock)) {
        CBlockIndex* pindex = mapBlockIndex[hashBlock];
        if (chainActive.Contains(pindex)) {
            conflictconfirms = -(chainActive.Height() - pindex->nHeight + 1);
        }
    }

    if (conflictconfirms >= 0)
        return;

    CWalletDB walletdb(strWalletFile, "r+", false);

    std::set<uint256> todo;
    std::set<uint256> done;

    todo.insert(hashTx);

    while (!todo.empty()) {
        uint256 now = *todo.begin();
        todo.erase(now);
        done.insert(now);
        assert(mapWallet.count(now));
        CWalletTx& wtx = mapWallet[now];
        int currentconfirm = wtx.GetDepthInMainChain();
        if (conflictconfirms < currentconfirm) {

            wtx.nIndex = -1;
            wtx.hashBlock = hashBlock;
            wtx.MarkDirty();
            walletdb.WriteTx(wtx);

            TxSpends::const_iterator iter = mapTxSpends.lower_bound(COutPoint(now, 0));
            while (iter != mapTxSpends.end() && iter->first.hash == now) {
                if (!done.count(iter->second)) {
                    todo.insert(iter->second);
                }
                iter++;
            }

            BOOST_FOREACH (const CTxIn& txin, wtx.vin) {
                if (mapWallet.count(txin.prevout.hash))
                    mapWallet[txin.prevout.hash].MarkDirty();
            }
        }
    }
}

void CWallet::SyncTransaction(const CTransaction& tx, const CBlockIndex* pindex, const CBlock* pblock)
{
    LOCK2(cs_main, cs_wallet);

    if (!AddToWalletIfInvolvingMe(tx, pblock, true))
        return; // Not one of ours

    BOOST_FOREACH (const CTxIn& txin, tx.vin) {
        if (mapWallet.count(txin.prevout.hash))
            mapWallet[txin.prevout.hash].MarkDirty();
    }
}

void CWallet::RemoveAddressFromKeypoolIfIsMine(const CTxIn& txin, uint64_t time)
{
    {
        LOCK(cs_wallet);
        map<uint256, CWalletTx>::const_iterator mi = mapWallet.find(txin.prevout.hash);
        if (mi != mapWallet.end()) {
            const CWalletTx& prev = (*mi).second;
            if (txin.prevout.n < prev.vout.size())
                RemoveAddressFromKeypoolIfIsMine(prev.vout[txin.prevout.n], time);
        }
    }
}

isminetype CWallet::IsMine(const CTxIn& txin) const
{
    {
        LOCK(cs_wallet);
        map<uint256, CWalletTx>::const_iterator mi = mapWallet.find(txin.prevout.hash);
        if (mi != mapWallet.end()) {
            const CWalletTx& prev = (*mi).second;
            if (txin.prevout.n < prev.vout.size())
                return IsMine(prev.vout[txin.prevout.n]);
        }
    }
    return ISMINE_NO;
}

isminetype CWallet::IsMine(const CKeyStore& keystore, const CTxIn& txin) const
{
    {
        LOCK(cs_wallet);
        map<uint256, CWalletTx>::const_iterator mi = mapWallet.find(txin.prevout.hash);
        if (mi != mapWallet.end()) {
            const CWalletTx& prev = (*mi).second;
            if (txin.prevout.n < prev.vout.size())
                return ::IsMine(keystore, prev.vout[txin.prevout.n]);
        }
    }
    return ISMINE_NO;
}

bool IsMine(CAccount* forAccount, const CWalletTx& tx)
{
    isminetype ret = isminetype::ISMINE_NO;
    for (const auto& txout : tx.vout) {
        for (auto keyChain : { KEYCHAIN_EXTERNAL, KEYCHAIN_CHANGE }) {
            isminetype temp = (keyChain == KEYCHAIN_EXTERNAL ? IsMine(forAccount->externalKeyStore, txout.scriptPubKey) : IsMine(forAccount->internalKeyStore, txout.scriptPubKey));
            if (temp > ret)
                ret = temp;
        }
        if (ret > isminetype::ISMINE_NO)
            return true;
    }
    return false;
}

CAmount CWallet::GetDebit(const CTxIn& txin, const isminefilter& filter) const
{
    {
        LOCK(cs_wallet);
        map<uint256, CWalletTx>::const_iterator mi = mapWallet.find(txin.prevout.hash);
        if (mi != mapWallet.end()) {
            const CWalletTx& prev = (*mi).second;
            if (txin.prevout.n < prev.vout.size())
                if (IsMine(prev.vout[txin.prevout.n]) & filter)
                    return prev.vout[txin.prevout.n].nValue;
        }
    }
    return 0;
}

isminetype CWallet::IsMine(const CTxOut& txout) const
{
    return ::IsMine(*this, txout.scriptPubKey);
}

void CWallet::RemoveAddressFromKeypoolIfIsMine(const CTxOut& txout, uint64_t time)
{
    ::RemoveAddressFromKeypoolIfIsMine(*this, txout.scriptPubKey, time);
}

CAmount CWallet::GetCredit(const CTxOut& txout, const isminefilter& filter) const
{
    if (!MoneyRange(txout.nValue))
        throw std::runtime_error(std::string(__func__) + ": value out of range");
    return ((IsMine(txout) & filter) ? txout.nValue : 0);
}

bool CWallet::IsChange(const CTxOut& txout) const
{

    if (::IsMine(*this, txout.scriptPubKey)) {
        CTxDestination address;
        if (!ExtractDestination(txout.scriptPubKey, address))
            return true;

        LOCK(cs_wallet);
        if (!mapAddressBook.count(CBitcoinAddress(address).ToString()))
            return true;
    }
    return false;
}

CAmount CWallet::GetChange(const CTxOut& txout) const
{
    if (!MoneyRange(txout.nValue))
        throw std::runtime_error(std::string(__func__) + ": value out of range");
    return (IsChange(txout) ? txout.nValue : 0);
}

bool CWallet::IsMine(const CTransaction& tx) const
{
    BOOST_FOREACH (const CTxOut& txout, tx.vout)
        if (IsMine(txout) && txout.nValue >= nMinimumInputValue)
            return true;
    return false;
}

void CWallet::RemoveAddressFromKeypoolIfIsMine(const CTransaction& tx, uint64_t time)
{
    for (const CTxOut& txout : tx.vout) {
        RemoveAddressFromKeypoolIfIsMine(txout, time);
    }
}

bool CWallet::IsFromMe(const CTransaction& tx) const
{
    return (GetDebit(tx, ISMINE_ALL) > 0);
}

CAmount CWallet::GetDebit(const CTransaction& tx, const isminefilter& filter) const
{
    CAmount nDebit = 0;
    BOOST_FOREACH (const CTxIn& txin, tx.vin) {
        nDebit += GetDebit(txin, filter);
        if (!MoneyRange(nDebit))
            throw std::runtime_error(std::string(__func__) + ": value out of range");
    }
    return nDebit;
}

CAmount CWallet::GetCredit(const CTransaction& tx, const isminefilter& filter) const
{
    CAmount nCredit = 0;
    BOOST_FOREACH (const CTxOut& txout, tx.vout) {
        nCredit += GetCredit(txout, filter);
        if (!MoneyRange(nCredit))
            throw std::runtime_error(std::string(__func__) + ": value out of range");
    }
    return nCredit;
}

CAmount CWallet::GetChange(const CTransaction& tx) const
{
    CAmount nChange = 0;
    BOOST_FOREACH (const CTxOut& txout, tx.vout) {
        nChange += GetChange(txout);
        if (!MoneyRange(nChange))
            throw std::runtime_error(std::string(__func__) + ": value out of range");
    }
    return nChange;
}

int64_t CWalletTx::GetTxTime() const
{
    int64_t n = nTimeSmart;
    return n ? n : nTimeReceived;
}

int CWalletTx::GetRequestCount() const
{

    int nRequests = -1;
    {
        LOCK(pwallet->cs_wallet);
        if (IsCoinBase()) {

            if (!hashUnset()) {
                map<uint256, int>::const_iterator mi = pwallet->mapRequestCount.find(hashBlock);
                if (mi != pwallet->mapRequestCount.end())
                    nRequests = (*mi).second;
            }
        } else {

            map<uint256, int>::const_iterator mi = pwallet->mapRequestCount.find(GetHash());
            if (mi != pwallet->mapRequestCount.end()) {
                nRequests = (*mi).second;

                if (nRequests == 0 && !hashUnset()) {
                    map<uint256, int>::const_iterator mi = pwallet->mapRequestCount.find(hashBlock);
                    if (mi != pwallet->mapRequestCount.end())
                        nRequests = (*mi).second;
                    else
                        nRequests = 1; // If it's in someone else's block it must have got out
                }
            }
        }
    }
    return nRequests;
}

void CWalletTx::GetAmounts(list<COutputEntry>& listReceived, list<COutputEntry>& listSent, CAmount& nFee, const isminefilter& filter, CKeyStore* from) const
{
    if (!from)
        from = pwallet->activeAccount;

    nFee = 0;
    listReceived.clear();
    listSent.clear();

    CAmount nDebit = GetDebit(filter);
    if (nDebit > 0) // debit>0 means we signed/sent this transaction
    {
        CAmount nValueOut = GetValueOut();
        nFee = nDebit - nValueOut;
    }

    for (unsigned int i = 0; i < vin.size(); ++i) {
        const CTxIn& txin = vin[i];

        map<uint256, CWalletTx>::const_iterator mi = pwallet->mapWallet.find(txin.prevout.hash);
        if (mi != pwallet->mapWallet.end()) {
            const CWalletTx& prev = (*mi).second;
            if (txin.prevout.n < prev.vout.size()) {
                const auto& prevOut = prev.vout[txin.prevout.n];

                isminetype fIsMine = IsMine(*from, prevOut);

                if ((fIsMine & filter)) {
                    CTxDestination address;
                    if (!ExtractDestination(prevOut.scriptPubKey, address) && !prevOut.scriptPubKey.IsUnspendable()) {
                        LogPrintf("CWalletTx::GetAmounts: Unknown transaction type found, txid %s\n",
                                  this->GetHash().ToString());
                        address = CNoDestination();
                    }

                    COutputEntry output = { address, prevOut.nValue, (int)i };

                    listSent.push_back(output);
                }
            }
        }
    }

    for (unsigned int i = 0; i < vout.size(); ++i) {
        const CTxOut& txout = vout[i];
        isminetype fIsMine = IsMine(*from, txout);
        if (!(fIsMine & filter))
            continue;

        CTxDestination address;
        if (!ExtractDestination(txout.scriptPubKey, address) && !txout.scriptPubKey.IsUnspendable()) {
            LogPrintf("CWalletTx::GetAmounts: Unknown transaction type found, txid %s\n",
                      this->GetHash().ToString());
            address = CNoDestination();
        }

        COutputEntry output = { address, txout.nValue, (int)i };

        listReceived.push_back(output);
    }
}

void CWalletTx::GetAccountAmounts(const string& strAccount, CAmount& nReceived,
                                  CAmount& nSent, CAmount& nFee, const isminefilter& filter) const
{
    LOCK(pwalletMain->cs_wallet);

    nReceived = nSent = nFee = 0;

    CAmount allFee;
    list<COutputEntry> listReceived;
    list<COutputEntry> listSent;
    GetAmounts(listReceived, listSent, allFee, filter, pwalletMain->mapAccounts[strAccount]);

    BOOST_FOREACH (const COutputEntry& s, listSent)
        nSent += s.amount;
    nFee = allFee;
    {
        LOCK(pwallet->cs_wallet);
        BOOST_FOREACH (const COutputEntry& r, listReceived) {
            nReceived += r.amount;
        }
    }
}

/**
 * Scan the block chain (starting in pindexStart) for transactions
 * from or to us. If fUpdate is true, found transactions that already
 * exist in the wallet will be updated.
 */
int CWallet::ScanForWalletTransactions(CBlockIndex* pindexStart, bool fUpdate)
{
    int ret = 0;
    int64_t nNow = GetTime();
    const CChainParams& chainParams = Params();

    CBlockIndex* pindex = pindexStart;
    {
        LOCK2(cs_main, cs_wallet);

        while (pindex && nTimeFirstKey && (pindex->GetBlockTime() < (nTimeFirstKey - 7200)))
            pindex = chainActive.Next(pindex);

        ShowProgress(_("Rescanning..."), 0); // show rescan progress in GUI as dialog or on splashscreen, if -rescan on startup
        double dProgressStart = Checkpoints::GuessVerificationProgress(chainParams.Checkpoints(), pindex, false);
        double dProgressTip = Checkpoints::GuessVerificationProgress(chainParams.Checkpoints(), chainActive.Tip(), false);
        while (pindex) {
            if (pindex->nHeight % 100 == 0 && dProgressTip - dProgressStart > 0.0) {

                LEAVE_CRITICAL_SECTION(cs_wallet)
                ShowProgress(_("Rescanning..."), std::max(1, std::min(99, (int)((Checkpoints::GuessVerificationProgress(chainParams.Checkpoints(), pindex, false) - dProgressStart) / (dProgressTip - dProgressStart) * 100))));
                ENTER_CRITICAL_SECTION(cs_wallet)
            }

            CBlock block;
            ReadBlockFromDisk(block, pindex, Params().GetConsensus());
            BOOST_FOREACH (CTransaction& tx, block.vtx) {
                if (AddToWalletIfInvolvingMe(tx, &block, fUpdate))
                    ret++;
            }
            pindex = chainActive.Next(pindex);
            if (GetTime() >= nNow + 60) {
                nNow = GetTime();
                LogPrintf("Still rescanning. At block %d. Progress=%f\n", pindex->nHeight, Checkpoints::GuessVerificationProgress(chainParams.Checkpoints(), pindex));
            }
        }
        ShowProgress(_("Rescanning..."), 100); // hide progress dialog in GUI
    }
    return ret;
}

void CWallet::ReacceptWalletTransactions()
{

    if (!fBroadcastTransactions)
        return;
    LOCK2(cs_main, cs_wallet);
    std::map<int64_t, CWalletTx*> mapSorted;

    BOOST_FOREACH (PAIRTYPE(const uint256, CWalletTx) & item, mapWallet) {
        const uint256& wtxid = item.first;
        CWalletTx& wtx = item.second;
        assert(wtx.GetHash() == wtxid);

        int nDepth = wtx.GetDepthInMainChain();

        if (!wtx.IsCoinBase() && (nDepth == 0 && !wtx.isAbandoned())) {
            mapSorted.insert(std::make_pair(wtx.nOrderPos, &wtx));
        }
    }

    BOOST_FOREACH (PAIRTYPE(const int64_t, CWalletTx*)&item, mapSorted) {
        CWalletTx& wtx = *(item.second);

        LOCK(mempool.cs);
        wtx.AcceptToMemoryPool(false, maxTxFee);
    }
}

bool CWalletTx::RelayWalletTransaction()
{
    assert(pwallet->GetBroadcastTransactions());
    if (!IsCoinBase()) {
        if (GetDepthInMainChain() == 0 && !isAbandoned() && InMempool()) {
            LogPrintf("Relaying wtx %s\n", GetHash().ToString());
            RelayTransaction((CTransaction) * this);
            return true;
        }
    }
    return false;
}

set<uint256> CWalletTx::GetConflicts() const
{
    set<uint256> result;
    if (pwallet != NULL) {
        uint256 myHash = GetHash();
        result = pwallet->GetConflicts(myHash);
        result.erase(myHash);
    }
    return result;
}

CAmount CWalletTx::GetDebit(const isminefilter& filter) const
{
    if (vin.empty())
        return 0;

    CAmount debit = 0;
    if (filter & ISMINE_SPENDABLE) {
        if (fDebitCached)
            debit += nDebitCached;
        else {
            nDebitCached = pwallet->GetDebit(*this, ISMINE_SPENDABLE);
            fDebitCached = true;
            debit += nDebitCached;
        }
    }
    if (filter & ISMINE_WATCH_ONLY) {
        if (fWatchDebitCached)
            debit += nWatchDebitCached;
        else {
            nWatchDebitCached = pwallet->GetDebit(*this, ISMINE_WATCH_ONLY);
            fWatchDebitCached = true;
            debit += nWatchDebitCached;
        }
    }
    return debit;
}

CAmount CWalletTx::GetCredit(const isminefilter& filter) const
{

    if (IsCoinBase() && GetBlocksToMaturity() > 0)
        return 0;

    int64_t credit = 0;
    if (filter & ISMINE_SPENDABLE) {

        if (fCreditCached)
            credit += nCreditCached;
        else {
            nCreditCached = pwallet->GetCredit(*this, ISMINE_SPENDABLE);
            fCreditCached = true;
            credit += nCreditCached;
        }
    }
    if (filter & ISMINE_WATCH_ONLY) {
        if (fWatchCreditCached)
            credit += nWatchCreditCached;
        else {
            nWatchCreditCached = pwallet->GetCredit(*this, ISMINE_WATCH_ONLY);
            fWatchCreditCached = true;
            credit += nWatchCreditCached;
        }
    }
    return credit;
}

CAmount CWalletTx::GetImmatureCredit(bool fUseCache) const
{
    if (IsCoinBase() && GetBlocksToMaturity() > 0 && IsInMainChain()) {
        if (fUseCache && fImmatureCreditCached)
            return nImmatureCreditCached;
        nImmatureCreditCached = pwallet->GetCredit(*this, ISMINE_SPENDABLE);
        fImmatureCreditCached = true;
        return nImmatureCreditCached;
    }

    return 0;
}

CAmount CWalletTx::GetAvailableCredit(bool fUseCache) const
{
    if (pwallet == 0)
        return 0;

    if (IsCoinBase() && GetBlocksToMaturity() > 0)
        return 0;

    if (fUseCache && fAvailableCreditCached)
        return nAvailableCreditCached;

    CAmount nCredit = 0;
    uint256 hashTx = GetHash();
    for (unsigned int i = 0; i < vout.size(); i++) {
        if (!pwallet->IsSpent(hashTx, i)) {
            const CTxOut& txout = vout[i];
            nCredit += pwallet->GetCredit(txout, ISMINE_SPENDABLE);
            if (!MoneyRange(nCredit))
                throw std::runtime_error("CWalletTx::GetAvailableCredit() : value out of range");
        }
    }

    nAvailableCreditCached = nCredit;
    fAvailableCreditCached = true;
    return nCredit;
}

CAmount CWalletTx::GetImmatureWatchOnlyCredit(const bool& fUseCache) const
{
    if (IsCoinBase() && GetBlocksToMaturity() > 0 && IsInMainChain()) {
        if (fUseCache && fImmatureWatchCreditCached)
            return nImmatureWatchCreditCached;
        nImmatureWatchCreditCached = pwallet->GetCredit(*this, ISMINE_WATCH_ONLY);
        fImmatureWatchCreditCached = true;
        return nImmatureWatchCreditCached;
    }

    return 0;
}

CAmount CWalletTx::GetAvailableWatchOnlyCredit(const bool& fUseCache) const
{
    if (pwallet == 0)
        return 0;

    if (IsCoinBase() && GetBlocksToMaturity() > 0)
        return 0;

    if (fUseCache && fAvailableWatchCreditCached)
        return nAvailableWatchCreditCached;

    CAmount nCredit = 0;
    for (unsigned int i = 0; i < vout.size(); i++) {
        if (!pwallet->IsSpent(GetHash(), i)) {
            const CTxOut& txout = vout[i];
            nCredit += pwallet->GetCredit(txout, ISMINE_WATCH_ONLY);
            if (!MoneyRange(nCredit))
                throw std::runtime_error("CWalletTx::GetAvailableCredit() : value out of range");
        }
    }

    nAvailableWatchCreditCached = nCredit;
    fAvailableWatchCreditCached = true;
    return nCredit;
}

CAmount CWalletTx::GetChange() const
{
    if (fChangeCached)
        return nChangeCached;
    nChangeCached = pwallet->GetChange(*this);
    fChangeCached = true;
    return nChangeCached;
}

bool CWalletTx::InMempool() const
{
    LOCK(mempool.cs);
    if (mempool.exists(GetHash())) {
        return true;
    }
    return false;
}

bool CWalletTx::IsTrusted() const
{

    if (!CheckFinalTx(*this))
        return false;
    int nDepth = GetDepthInMainChain();
    if (nDepth >= 1)
        return true;
    if (nDepth < 0)
        return false;
    if (!bSpendZeroConfChange || !IsFromMe(ISMINE_ALL)) // using wtx's cached debit
        return false;

    if (!InMempool())
        return false;

    BOOST_FOREACH (const CTxIn& txin, vin) {

        const CWalletTx* parent = pwallet->GetWalletTx(txin.prevout.hash);
        if (parent == NULL)
            return false;
        const CTxOut& parentOut = parent->vout[txin.prevout.n];
        if (pwallet->IsMine(parentOut) != ISMINE_SPENDABLE)
            return false;
    }
    return true;
}

bool CWalletTx::IsEquivalentTo(const CWalletTx& tx) const
{
    CMutableTransaction tx1 = *this;
    CMutableTransaction tx2 = tx;
    for (unsigned int i = 0; i < tx1.vin.size(); i++)
        tx1.vin[i].scriptSig = CScript();
    for (unsigned int i = 0; i < tx2.vin.size(); i++)
        tx2.vin[i].scriptSig = CScript();
    return CTransaction(tx1) == CTransaction(tx2);
}

std::vector<uint256> CWallet::ResendWalletTransactionsBefore(int64_t nTime)
{
    std::vector<uint256> result;

    LOCK(cs_wallet);

    multimap<unsigned int, CWalletTx*> mapSorted;
    BOOST_FOREACH (PAIRTYPE(const uint256, CWalletTx) & item, mapWallet) {
        CWalletTx& wtx = item.second;

        if (wtx.nTimeReceived > nTime)
            continue;
        mapSorted.insert(make_pair(wtx.nTimeReceived, &wtx));
    }
    BOOST_FOREACH (PAIRTYPE(const unsigned int, CWalletTx*)&item, mapSorted) {
        CWalletTx& wtx = *item.second;
        if (wtx.RelayWalletTransaction())
            result.push_back(wtx.GetHash());
    }
    return result;
}

void CWallet::ResendWalletTransactions(int64_t nBestBlockTime)
{

    if (GetTime() < nNextResend || !fBroadcastTransactions)
        return;
    bool fFirst = (nNextResend == 0);
    nNextResend = GetTime() + GetRand(30 * 60);
    if (fFirst)
        return;

    if (nBestBlockTime < nLastResend)
        return;
    nLastResend = GetTime();

    std::vector<uint256> relayed = ResendWalletTransactionsBefore(nBestBlockTime - 5 * 60);
    if (!relayed.empty())
        LogPrintf("%s: rebroadcast %u unconfirmed transactions\n", __func__, relayed.size());
}

/** @} */ // end of mapWallet

/** @defgroup Actions
 *
 * @{
 */

CAmount CWallet::GetBalance(CAccount* forAccount) const
{
    CAmount nTotal = 0;
    {
        LOCK2(cs_main, cs_wallet);
        for (map<uint256, CWalletTx>::const_iterator it = mapWallet.begin(); it != mapWallet.end(); ++it) {
            const CWalletTx* pcoin = &(*it).second;

            if (!forAccount || ::IsMine(forAccount, *pcoin)) {
                if (pcoin->IsTrusted())
                    nTotal += pcoin->GetAvailableCredit();
            }
        }
    }

    return nTotal;
}

CAmount CWallet::GetUnconfirmedBalance(CAccount* forAccount) const
{
    CAmount nTotal = 0;
    {
        LOCK2(cs_main, cs_wallet);
        for (map<uint256, CWalletTx>::const_iterator it = mapWallet.begin(); it != mapWallet.end(); ++it) {
            const CWalletTx* pcoin = &(*it).second;

            if (!forAccount || ::IsMine(forAccount, *pcoin)) {
                if (!pcoin->IsTrusted() && pcoin->GetDepthInMainChain() == 0 && pcoin->InMempool())
                    nTotal += pcoin->GetAvailableCredit();
            }
        }
    }
    return nTotal;
}

CAmount CWallet::GetImmatureBalance() const
{
    CAmount nTotal = 0;
    {
        LOCK2(cs_main, cs_wallet);
        for (map<uint256, CWalletTx>::const_iterator it = mapWallet.begin(); it != mapWallet.end(); ++it) {
            const CWalletTx* pcoin = &(*it).second;
            nTotal += pcoin->GetImmatureCredit();
        }
    }
    return nTotal;
}

CAmount CWallet::GetWatchOnlyBalance() const
{
    CAmount nTotal = 0;
    {
        LOCK2(cs_main, cs_wallet);
        for (map<uint256, CWalletTx>::const_iterator it = mapWallet.begin(); it != mapWallet.end(); ++it) {
            const CWalletTx* pcoin = &(*it).second;
            if (pcoin->IsTrusted())
                nTotal += pcoin->GetAvailableWatchOnlyCredit();
        }
    }

    return nTotal;
}

CAmount CWallet::GetUnconfirmedWatchOnlyBalance() const
{
    CAmount nTotal = 0;
    {
        LOCK2(cs_main, cs_wallet);
        for (map<uint256, CWalletTx>::const_iterator it = mapWallet.begin(); it != mapWallet.end(); ++it) {
            const CWalletTx* pcoin = &(*it).second;
            if (!pcoin->IsTrusted() && pcoin->GetDepthInMainChain() == 0 && pcoin->InMempool())
                nTotal += pcoin->GetAvailableWatchOnlyCredit();
        }
    }
    return nTotal;
}

CAmount CWallet::GetImmatureWatchOnlyBalance() const
{
    CAmount nTotal = 0;
    {
        LOCK2(cs_main, cs_wallet);
        for (map<uint256, CWalletTx>::const_iterator it = mapWallet.begin(); it != mapWallet.end(); ++it) {
            const CWalletTx* pcoin = &(*it).second;
            nTotal += pcoin->GetImmatureWatchOnlyCredit();
        }
    }
    return nTotal;
}

void CWallet::AvailableCoins(CAccount* forAccount, vector<COutput>& vCoins, bool fOnlyConfirmed, const CCoinControl* coinControl, bool fIncludeZeroValue) const
{
    vCoins.clear();

    {
        LOCK2(cs_main, cs_wallet);
        for (map<uint256, CWalletTx>::const_iterator it = mapWallet.begin(); it != mapWallet.end(); ++it) {
            const uint256& wtxid = it->first;
            const CWalletTx* pcoin = &(*it).second;

            if (!::IsMine(forAccount, *pcoin))
                continue;

            if (!CheckFinalTx(*pcoin))
                continue;

            if (fOnlyConfirmed && !pcoin->IsTrusted())
                continue;

            if (pcoin->IsCoinBase() && pcoin->GetBlocksToMaturity() > 0)
                continue;

            int nDepth = pcoin->GetDepthInMainChain();
            if (nDepth < 0)
                continue;

            if (nDepth == 0 && !pcoin->InMempool())
                continue;

            for (unsigned int i = 0; i < pcoin->vout.size(); i++) {
                isminetype mine = ::IsMine(*forAccount, pcoin->vout[i].scriptPubKey);
                if (!(IsSpent(wtxid, i)) && mine != ISMINE_NO && !IsLockedCoin((*it).first, i) && (pcoin->vout[i].nValue > nMinimumInputValue || fIncludeZeroValue) && (!coinControl || !coinControl->HasSelected() || coinControl->fAllowOtherInputs || coinControl->IsSelected(COutPoint((*it).first, i))))
                    vCoins.push_back(COutput(pcoin, i, nDepth,
                                             ((mine & ISMINE_SPENDABLE) != ISMINE_NO) || (coinControl && coinControl->fAllowWatchOnly && (mine & ISMINE_WATCH_SOLVABLE) != ISMINE_NO),
                                             (mine & (ISMINE_SPENDABLE | ISMINE_WATCH_SOLVABLE)) != ISMINE_NO));
            }
        }
    }
}

static void ApproximateBestSubset(vector<pair<CAmount, pair<const CWalletTx*, unsigned int> > > vValue, const CAmount& nTotalLower, const CAmount& nTargetValue,
                                  vector<char>& vfBest, CAmount& nBest, int iterations = 1000)
{
    vector<char> vfIncluded;

    vfBest.assign(vValue.size(), true);
    nBest = nTotalLower;

    seed_insecure_rand();

    for (int nRep = 0; nRep < iterations && nBest != nTargetValue; nRep++) {
        vfIncluded.assign(vValue.size(), false);
        CAmount nTotal = 0;
        bool fReachedTarget = false;
        for (int nPass = 0; nPass < 2 && !fReachedTarget; nPass++) {
            for (unsigned int i = 0; i < vValue.size(); i++) {

                if (nPass == 0 ? insecure_rand() & 1 : !vfIncluded[i]) {
                    nTotal += vValue[i].first;
                    vfIncluded[i] = true;
                    if (nTotal >= nTargetValue) {
                        fReachedTarget = true;
                        if (nTotal < nBest) {
                            nBest = nTotal;
                            vfBest = vfIncluded;
                        }
                        nTotal -= vValue[i].first;
                        vfIncluded[i] = false;
                    }
                }
            }
        }
    }
}

bool CWallet::SelectCoinsMinConf(const CAmount& nTargetValue, int nConfMine, int nConfTheirs, vector<COutput> vCoins,
                                 set<pair<const CWalletTx*, unsigned int> >& setCoinsRet, CAmount& nValueRet) const
{
    setCoinsRet.clear();
    nValueRet = 0;

    pair<CAmount, pair<const CWalletTx*, unsigned int> > coinLowestLarger;
    coinLowestLarger.first = std::numeric_limits<CAmount>::max();
    coinLowestLarger.second.first = NULL;
    vector<pair<CAmount, pair<const CWalletTx*, unsigned int> > > vValue;
    CAmount nTotalLower = 0;

    random_shuffle(vCoins.begin(), vCoins.end(), GetRandInt);

    BOOST_FOREACH (const COutput& output, vCoins) {
        if (!output.fSpendable)
            continue;

        const CWalletTx* pcoin = output.tx;

        if (output.nDepth < (pcoin->IsFromMe(ISMINE_ALL) ? nConfMine : nConfTheirs))
            continue;

        int i = output.i;
        CAmount n = pcoin->vout[i].nValue;

        pair<CAmount, pair<const CWalletTx*, unsigned int> > coin = make_pair(n, make_pair(pcoin, i));

        if (n == nTargetValue) {
            setCoinsRet.insert(coin.second);
            nValueRet += coin.first;
            return true;
        } else if (n < nTargetValue + MIN_CHANGE) {
            vValue.push_back(coin);
            nTotalLower += n;
        } else if (n < coinLowestLarger.first) {
            coinLowestLarger = coin;
        }
    }

    if (nTotalLower == nTargetValue) {
        for (unsigned int i = 0; i < vValue.size(); ++i) {
            setCoinsRet.insert(vValue[i].second);
            nValueRet += vValue[i].first;
        }
        return true;
    }

    if (nTotalLower < nTargetValue) {
        if (coinLowestLarger.second.first == NULL)
            return false;
        setCoinsRet.insert(coinLowestLarger.second);
        nValueRet += coinLowestLarger.first;
        return true;
    }

    std::sort(vValue.begin(), vValue.end(), CompareValueOnly());
    std::reverse(vValue.begin(), vValue.end());
    vector<char> vfBest;
    CAmount nBest;

    ApproximateBestSubset(vValue, nTotalLower, nTargetValue, vfBest, nBest);
    if (nBest != nTargetValue && nTotalLower >= nTargetValue + MIN_CHANGE)
        ApproximateBestSubset(vValue, nTotalLower, nTargetValue + MIN_CHANGE, vfBest, nBest);

    if (coinLowestLarger.second.first && ((nBest != nTargetValue && nBest < nTargetValue + MIN_CHANGE) || coinLowestLarger.first <= nBest)) {
        setCoinsRet.insert(coinLowestLarger.second);
        nValueRet += coinLowestLarger.first;
    } else {
        for (unsigned int i = 0; i < vValue.size(); i++)
            if (vfBest[i]) {
                setCoinsRet.insert(vValue[i].second);
                nValueRet += vValue[i].first;
            }

        LogPrint("selectcoins", "SelectCoins() best subset: ");
        for (unsigned int i = 0; i < vValue.size(); i++)
            if (vfBest[i])
                LogPrint("selectcoins", "%s ", FormatMoney(vValue[i].first));
        LogPrint("selectcoins", "total %s\n", FormatMoney(nBest));
    }

    return true;
}

bool CWallet::SelectCoins(const vector<COutput>& vAvailableCoins, const CAmount& nTargetValue, set<pair<const CWalletTx*, unsigned int> >& setCoinsRet, CAmount& nValueRet, const CCoinControl* coinControl) const
{
    vector<COutput> vCoins(vAvailableCoins);

    if (coinControl && coinControl->HasSelected() && !coinControl->fAllowOtherInputs) {
        BOOST_FOREACH (const COutput& out, vCoins) {
            if (!out.fSpendable)
                continue;
            nValueRet += out.tx->vout[out.i].nValue;
            setCoinsRet.insert(make_pair(out.tx, out.i));
        }
        return (nValueRet >= nTargetValue);
    }

    set<pair<const CWalletTx*, uint32_t> > setPresetCoins;
    CAmount nValueFromPresetInputs = 0;

    std::vector<COutPoint> vPresetInputs;
    if (coinControl)
        coinControl->ListSelected(vPresetInputs);
    BOOST_FOREACH (const COutPoint& outpoint, vPresetInputs) {
        map<uint256, CWalletTx>::const_iterator it = mapWallet.find(outpoint.hash);
        if (it != mapWallet.end()) {
            const CWalletTx* pcoin = &it->second;

            if (pcoin->vout.size() <= outpoint.n)
                return false;
            nValueFromPresetInputs += pcoin->vout[outpoint.n].nValue;
            setPresetCoins.insert(make_pair(pcoin, outpoint.n));
        } else
            return false; // TODO: Allow non-wallet inputs
    }

    for (vector<COutput>::iterator it = vCoins.begin(); it != vCoins.end() && coinControl && coinControl->HasSelected();) {
        if (setPresetCoins.count(make_pair(it->tx, it->i)))
            it = vCoins.erase(it);
        else
            ++it;
    }

    bool res = nTargetValue <= nValueFromPresetInputs || SelectCoinsMinConf(nTargetValue - nValueFromPresetInputs, 1, 6, vCoins, setCoinsRet, nValueRet) || SelectCoinsMinConf(nTargetValue - nValueFromPresetInputs, 1, 1, vCoins, setCoinsRet, nValueRet) || (bSpendZeroConfChange && SelectCoinsMinConf(nTargetValue - nValueFromPresetInputs, 0, 1, vCoins, setCoinsRet, nValueRet));

    setCoinsRet.insert(setPresetCoins.begin(), setPresetCoins.end());

    nValueRet += nValueFromPresetInputs;

    return res;
}

bool CWallet::FundTransaction(CAccount* fromAccount, CMutableTransaction& tx, CAmount& nFeeRet, bool overrideEstimatedFeeRate, const CFeeRate& specificFeeRate, int& nChangePosInOut, std::string& strFailReason, bool includeWatching, bool lockUnspents, const CTxDestination& destChange)
{
    vector<CRecipient> vecSend;

    BOOST_FOREACH (const CTxOut& txOut, tx.vout) {
        CRecipient recipient = { txOut.scriptPubKey, txOut.nValue, false };
        vecSend.push_back(recipient);
    }

    CCoinControl coinControl;
    coinControl.destChange = destChange;
    coinControl.fAllowOtherInputs = true;
    coinControl.fAllowWatchOnly = includeWatching;
    coinControl.fOverrideFeeRate = overrideEstimatedFeeRate;
    coinControl.nFeeRate = specificFeeRate;

    BOOST_FOREACH (const CTxIn& txin, tx.vin)
        coinControl.Select(txin.prevout);

    CReserveKey reservekey(this, fromAccount, KEYCHAIN_CHANGE);
    CWalletTx wtx;
    if (!CreateTransaction(fromAccount, vecSend, wtx, reservekey, nFeeRet, nChangePosInOut, strFailReason, &coinControl, false))
        return false;

    if (nChangePosInOut != -1)
        tx.vout.insert(tx.vout.begin() + nChangePosInOut, wtx.vout[nChangePosInOut]);

    BOOST_FOREACH (const CTxIn& txin, wtx.vin) {
        if (!coinControl.IsSelected(txin.prevout)) {
            tx.vin.push_back(txin);

            if (lockUnspents) {
                LOCK2(cs_main, cs_wallet);
                LockCoin(txin.prevout);
            }
        }
    }

    return true;
}

bool CWallet::CreateTransaction(CAccount* forAccount, const vector<CRecipient>& vecSend, CWalletTx& wtxNew, CReserveKey& reservekey, CAmount& nFeeRet,
                                int& nChangePosInOut, std::string& strFailReason, const CCoinControl* coinControl, bool sign)
{
    CAmount nValue = 0;
    int nChangePosRequest = nChangePosInOut;
    unsigned int nSubtractFeeFromAmount = 0;
    BOOST_FOREACH (const CRecipient& recipient, vecSend) {
        if (nValue < 0 || recipient.nAmount < 0) {
            strFailReason = _("Transaction amounts must be positive");
            return false;
        }
        nValue += recipient.nAmount;

        if (recipient.fSubtractFeeFromAmount)
            nSubtractFeeFromAmount++;
    }
    if (vecSend.empty() || nValue < 0) {
        strFailReason = _("Transaction amounts must be positive");
        return false;
    }

    wtxNew.fTimeReceivedIsTxTime = true;
    wtxNew.BindWallet(this);
    CMutableTransaction txNew;

    txNew.nLockTime = chainActive.Height();

    if (GetRandInt(10) == 0)
        txNew.nLockTime = std::max(0, (int)txNew.nLockTime - GetRandInt(100));

    assert(txNew.nLockTime <= (unsigned int)chainActive.Height());
    assert(txNew.nLockTime < LOCKTIME_THRESHOLD);

    {
        LOCK2(cs_main, cs_wallet);
        {
            std::vector<COutput> vAvailableCoins;
            AvailableCoins(forAccount, vAvailableCoins, true, coinControl);

            nFeeRet = 0;

            while (true) {
                nChangePosInOut = nChangePosRequest;
                txNew.vin.clear();
                txNew.vout.clear();
                txNew.wit.SetNull();
                wtxNew.fFromMe = true;
                bool fFirst = true;

                CAmount nValueToSelect = nValue;
                if (nSubtractFeeFromAmount == 0)
                    nValueToSelect += nFeeRet;
                double dPriority = 0;

                BOOST_FOREACH (const CRecipient& recipient, vecSend) {
                    CTxOut txout(recipient.nAmount, recipient.scriptPubKey);

                    if (recipient.fSubtractFeeFromAmount) {
                        txout.nValue -= nFeeRet / nSubtractFeeFromAmount; // Subtract fee equally from each selected recipient

                        if (fFirst) // first receiver pays the remainder not divisible by output count
                        {
                            fFirst = false;
                            txout.nValue -= nFeeRet % nSubtractFeeFromAmount;
                        }
                    }

                    if (txout.IsDust(::minRelayTxFee)) {
                        if (recipient.fSubtractFeeFromAmount && nFeeRet > 0) {
                            if (txout.nValue < 0)
                                strFailReason = _("The transaction amount is too small to pay the fee");
                            else
                                strFailReason = _("The transaction amount is too small to send after the fee has been deducted");
                        } else
                            strFailReason = _("Transaction amount too small");
                        return false;
                    }
                    txNew.vout.push_back(txout);
                }

                set<pair<const CWalletTx*, unsigned int> > setCoins;
                CAmount nValueIn = 0;

                if (!SelectCoins(vAvailableCoins, nValueToSelect, setCoins, nValueIn, coinControl)) {
                    strFailReason = _("Insufficient funds");
                    return false;
                }
                BOOST_FOREACH (PAIRTYPE(const CWalletTx*, unsigned int)pcoin, setCoins) {
                    CAmount nCredit = pcoin.first->vout[pcoin.second].nValue;

                    int age = pcoin.first->GetDepthInMainChain();
                    assert(age >= 0);
                    if (age != 0)
                        age += 1;
                    dPriority += (double)nCredit * age;
                }

                const CAmount nChange = nValueIn - nValueToSelect;
                if (nChange > 0) {

                    CScript scriptChange;

                    if (coinControl && !boost::get<CNoDestination>(&coinControl->destChange))
                        scriptChange = GetScriptForDestination(coinControl->destChange);

                    else {

                        CPubKey vchPubKey;
                        bool ret;
                        ret = reservekey.GetReservedKey(vchPubKey);
                        assert(ret); // should never fail, as we just unlocked

                        scriptChange = GetScriptForDestination(vchPubKey.GetID());
                    }

                    CTxOut newTxOut(nChange, scriptChange);

                    if (nSubtractFeeFromAmount > 0 && newTxOut.IsDust(::minRelayTxFee)) {
                        CAmount nDust = newTxOut.GetDustThreshold(::minRelayTxFee) - newTxOut.nValue;
                        newTxOut.nValue += nDust; // raise change until no more dust
                        for (unsigned int i = 0; i < vecSend.size(); i++) // subtract from first recipient
                        {
                            if (vecSend[i].fSubtractFeeFromAmount) {
                                txNew.vout[i].nValue -= nDust;
                                if (txNew.vout[i].IsDust(::minRelayTxFee)) {
                                    strFailReason = _("The transaction amount is too small to send after the fee has been deducted");
                                    return false;
                                }
                                break;
                            }
                        }
                    }

                    if (newTxOut.IsDust(::minRelayTxFee)) {
                        nChangePosInOut = -1;
                        nFeeRet += nChange;
                        reservekey.ReturnKey();
                    } else {
                        if (nChangePosInOut == -1) {

                            nChangePosInOut = GetRandInt(txNew.vout.size() + 1);
                        } else if ((unsigned int)nChangePosInOut > txNew.vout.size()) {
                            strFailReason = _("Change index out of range");
                            return false;
                        }

                        vector<CTxOut>::iterator position = txNew.vout.begin() + nChangePosInOut;
                        txNew.vout.insert(position, newTxOut);
                    }
                } else
                    reservekey.ReturnKey();

                BOOST_FOREACH (const PAIRTYPE(const CWalletTx*, unsigned int)&coin, setCoins)
                    txNew.vin.push_back(CTxIn(coin.first->GetHash(), coin.second, CScript(),
                                              std::numeric_limits<unsigned int>::max() - 1));

                int nIn = 0;
                CTransaction txNewConst(txNew);
                BOOST_FOREACH (const PAIRTYPE(const CWalletTx*, unsigned int)&coin, setCoins) {
                    bool signSuccess;
                    const CScript& scriptPubKey = coin.first->vout[coin.second].scriptPubKey;
                    SignatureData sigdata;

                    if (sign)
                        signSuccess = ProduceSignature(TransactionSignatureCreator(forAccount, &txNewConst, nIn, coin.first->vout[coin.second].nValue, SIGHASH_ALL), scriptPubKey, sigdata);
                    else
                        signSuccess = ProduceSignature(DummySignatureCreator(forAccount), scriptPubKey, sigdata);

                    if (!signSuccess) {
                        strFailReason = _("Signing transaction failed");
                        return false;
                    } else {
                        UpdateTransaction(txNew, nIn, sigdata);
                    }

                    nIn++;
                }

                unsigned int nBytes = GetVirtualTransactionSize(txNew);

                if (!sign) {
                    BOOST_FOREACH (CTxIn& vin, txNew.vin)
                        vin.scriptSig = CScript();
                    txNew.wit.SetNull();
                }

                *static_cast<CTransaction*>(&wtxNew) = CTransaction(txNew);

                if (GetTransactionWeight(txNew) >= MAX_STANDARD_TX_WEIGHT) {
                    strFailReason = _("Transaction too large");
                    return false;
                }

                dPriority = wtxNew.ComputePriority(dPriority, nBytes);

                if (fSendFreeTransactions && nBytes <= MAX_FREE_TRANSACTION_CREATE_SIZE) {

                    double dPriorityNeeded = mempool.estimateSmartPriority(nTxConfirmTarget);

                    if (dPriority >= dPriorityNeeded && AllowFree(dPriority))
                        break;
                }

                CAmount nFeeNeeded = GetMinimumFee(nBytes, nTxConfirmTarget, mempool);
                if (coinControl && nFeeNeeded > 0 && coinControl->nMinimumTotalFee > nFeeNeeded) {
                    nFeeNeeded = coinControl->nMinimumTotalFee;
                }
                if (coinControl && coinControl->fOverrideFeeRate)
                    nFeeNeeded = coinControl->nFeeRate.GetFee(nBytes);

                if (nFeeNeeded < ::minRelayTxFee.GetFee(nBytes)) {
                    strFailReason = _("Transaction too large for fee policy");
                    return false;
                }

                if (nFeeRet >= nFeeNeeded)
                    break; // Done, enough fee included.

                nFeeRet = nFeeNeeded;
                continue;
            }
        }
    }

    return true;
}

/**
 * Call after CreateTransaction unless you want to abort
 */
bool CWallet::CommitTransaction(CWalletTx& wtxNew, CReserveKey& reservekey)
{
    {
        LOCK2(cs_main, cs_wallet);
        LogPrintf("CommitTransaction:\n%s", wtxNew.ToString());
        {

            CWalletDB* pwalletdb = fFileBacked ? new CWalletDB(strWalletFile, "r+") : NULL;

            reservekey.KeepKey();

            AddToWallet(wtxNew, false, pwalletdb);

            set<CWalletTx*> setCoins;
            BOOST_FOREACH (const CTxIn& txin, wtxNew.vin) {
                CWalletTx& coin = mapWallet[txin.prevout.hash];
                coin.BindWallet(this);
                NotifyTransactionChanged(this, coin.GetHash(), CT_UPDATED);
            }

            if (fFileBacked)
                delete pwalletdb;
        }

        mapRequestCount[wtxNew.GetHash()] = 0;

        if (fBroadcastTransactions) {

            if (!wtxNew.AcceptToMemoryPool(false, maxTxFee)) {

                LogPrintf("CommitTransaction(): Error: Transaction not valid\n");
                return false;
            }
            wtxNew.RelayWalletTransaction();
        }
    }
    return true;
}

bool CWallet::AddAccountingEntry(const CAccountingEntry& acentry, CWalletDB& pwalletdb)
{
    if (!pwalletdb.WriteAccountingEntry_Backend(acentry))
        return false;

    laccentries.push_back(acentry);
    CAccountingEntry& entry = laccentries.back();
    wtxOrdered.insert(make_pair(entry.nOrderPos, TxPair((CWalletTx*)0, &entry)));

    return true;
}

CAmount CWallet::GetRequiredFee(unsigned int nTxBytes)
{
    return std::max(minTxFee.GetFee(nTxBytes), ::minRelayTxFee.GetFee(nTxBytes));
}

CAmount CWallet::GetMinimumFee(unsigned int nTxBytes, unsigned int nConfirmTarget, const CTxMemPool& pool)
{

    CAmount nFeeNeeded = payTxFee.GetFee(nTxBytes);

    if (nFeeNeeded == 0) {
        int estimateFoundTarget = nConfirmTarget;
        nFeeNeeded = pool.estimateSmartFee(nConfirmTarget, &estimateFoundTarget).GetFee(nTxBytes);

        if (nFeeNeeded == 0)
            nFeeNeeded = fallbackFee.GetFee(nTxBytes);
    }

    nFeeNeeded = std::max(nFeeNeeded, GetRequiredFee(nTxBytes));

    if (nFeeNeeded > maxTxFee)
        nFeeNeeded = maxTxFee;
    return nFeeNeeded;
}

DBErrors CWallet::LoadWallet(bool& fFirstRunRet)
{
    if (!fFileBacked)
        return DB_LOAD_OK;
    fFirstRunRet = false;
    DBErrors nLoadWalletRet = CWalletDB(strWalletFile, "cr+").LoadWallet(this, fFirstRunRet);
    if (nLoadWalletRet == DB_NEED_REWRITE) {
        if (CDB::Rewrite(strWalletFile, "\x04pool")) {
            LOCK(cs_wallet);

            for (auto accountPair : mapAccounts) {
                accountPair.second->setKeyPoolInternal.clear();
                accountPair.second->setKeyPoolExternal.clear();
            }
        }
    }

    if (nLoadWalletRet != DB_LOAD_OK)
        return nLoadWalletRet;

    uiInterface.LoadWallet(this);

    return DB_LOAD_OK;
}

DBErrors CWallet::ZapSelectTx(vector<uint256>& vHashIn, vector<uint256>& vHashOut)
{
    if (!fFileBacked)
        return DB_LOAD_OK;
    DBErrors nZapSelectTxRet = CWalletDB(strWalletFile, "cr+").ZapSelectTx(this, vHashIn, vHashOut);
    if (nZapSelectTxRet == DB_NEED_REWRITE) {
        if (CDB::Rewrite(strWalletFile, "\x04pool")) {
            LOCK(cs_wallet);

            for (auto accountPair : mapAccounts) {
                accountPair.second->setKeyPoolInternal.clear();
                accountPair.second->setKeyPoolExternal.clear();
            }
        }
    }

    if (nZapSelectTxRet != DB_LOAD_OK)
        return nZapSelectTxRet;

    MarkDirty();

    return DB_LOAD_OK;
}

DBErrors CWallet::ZapWalletTx(std::vector<CWalletTx>& vWtx)
{
    if (!fFileBacked)
        return DB_LOAD_OK;
    DBErrors nZapWalletTxRet = CWalletDB(strWalletFile, "cr+").ZapWalletTx(this, vWtx);
    if (nZapWalletTxRet == DB_NEED_REWRITE) {
        if (CDB::Rewrite(strWalletFile, "\x04pool")) {
            LOCK(cs_wallet);

            for (auto accountPair : mapAccounts) {
                accountPair.second->setKeyPoolInternal.clear();
                accountPair.second->setKeyPoolExternal.clear();
            }
        }
    }

    if (nZapWalletTxRet != DB_LOAD_OK)
        return nZapWalletTxRet;

    return DB_LOAD_OK;
}

bool CWallet::SetAddressBook(const std::string& address, const string& strName, const string& strPurpose)
{
    bool fUpdated = false;
    {
        LOCK(cs_wallet); // mapAddressBook
        std::map<std::string, CAddressBookData>::iterator mi = mapAddressBook.find(address);
        fUpdated = mi != mapAddressBook.end();
        mapAddressBook[address].name = strName;
        if (!strPurpose.empty()) /* update purpose only if requested */
            mapAddressBook[address].purpose = strPurpose;
    }
    NotifyAddressBookChanged(this, address, strName, ::IsMine(*this, CBitcoinAddress(address).Get()) != ISMINE_NO,
                             strPurpose, (fUpdated ? CT_UPDATED : CT_NEW));
    if (!fFileBacked)
        return false;
    if (!strPurpose.empty() && !CWalletDB(strWalletFile).WritePurpose(address, strPurpose))
        return false;
    return CWalletDB(strWalletFile).WriteName(address, strName);
}

bool CWallet::DelAddressBook(const std::string& address)
{
    {
        LOCK(cs_wallet); // mapAddressBook

        if (fFileBacked) {

            BOOST_FOREACH (const PAIRTYPE(string, string) & item, mapAddressBook[address].destdata) {
                CWalletDB(strWalletFile).EraseDestData(address, item.first);
            }
        }
        mapAddressBook.erase(address);
    }

    NotifyAddressBookChanged(this, address, "", ::IsMine(*this, CBitcoinAddress(address).Get()) != ISMINE_NO, "", CT_DELETED);

    if (!fFileBacked)
        return false;
    CWalletDB(strWalletFile).ErasePurpose(address);
    return CWalletDB(strWalletFile).EraseName(address);
}

/**
 * Mark old keypool keys as used,
 * and generate all new keys 
 */
bool CWallet::NewKeyPool()
{
    LogPrintf("keypool - newkeypool");
    {
        LOCK(cs_wallet);
        CWalletDB walletdb(strWalletFile);

        for (auto accountPair : mapAccounts) {
            if (!accountPair.second->IsHD()) {
                BOOST_FOREACH (int64_t nIndex, accountPair.second->setKeyPoolInternal)
                    walletdb.ErasePool(this, nIndex);
                BOOST_FOREACH (int64_t nIndex, accountPair.second->setKeyPoolExternal)
                    walletdb.ErasePool(this, nIndex);

                accountPair.second->setKeyPoolInternal.clear();
                accountPair.second->setKeyPoolExternal.clear();
            }
        }

        if (IsLocked())
            return false;
    }
    return true;
}

bool CWallet::TopUpKeyPool(unsigned int kpSize, unsigned int maxNew)
{
    unsigned int nNew = 0;
    {
        LOCK(cs_wallet);

        if (IsLocked())
            return false;

        CWalletDB walletdb(strWalletFile);

        unsigned int nTargetSize;
        if (kpSize > 0)
            nTargetSize = kpSize;
        else
            nTargetSize = GetArg("-keypool", 5);

        int64_t nIndex = 1;
        for (auto accountPair : mapAccounts) {
            for (auto keyChain : { KEYCHAIN_EXTERNAL, KEYCHAIN_CHANGE }) {
                auto& keyPool = (keyChain == KEYCHAIN_EXTERNAL ? accountPair.second->setKeyPoolExternal : accountPair.second->setKeyPoolInternal);
                if (!keyPool.empty())
                    nIndex = std::max(nIndex, *(--keyPool.end()) + 1);
            }
        }

        for (auto accountPair : mapAccounts) {
            unsigned int accountTargetSize = nTargetSize;
            for (auto& keyChain : { KEYCHAIN_EXTERNAL, KEYCHAIN_CHANGE }) {
                auto& keyPool = (keyChain == KEYCHAIN_EXTERNAL ? accountPair.second->setKeyPoolExternal : accountPair.second->setKeyPoolInternal);
                while (keyPool.size() < (accountTargetSize + 1)) {
                    if (!walletdb.WritePool(++nIndex, CKeyPool(GenerateNewKey(*accountPair.second, keyChain), accountPair.first, keyChain)))
                        throw runtime_error(std::string(__func__) + ": writing generated key failed");
                    keyPool.insert(nIndex);
                    LogPrintf("keypool [%s:%s] added key %d, size=%u\n", accountPair.second->getLabel(), (keyChain == KEYCHAIN_CHANGE ? "change" : "external"), nIndex, keyPool.size());

                    ++nNew;
                    if (maxNew != 0 && nNew >= maxNew)
                        return false;
                }
            }
        }
    }
    return true;
}

void CWallet::ReserveKeyFromKeyPool(int64_t& nIndex, CKeyPool& keypoolentry, CAccount* forAccount, int64_t keyChain)
{
    nIndex = -1;
    keypoolentry.vchPubKey = CPubKey();
    {
        LOCK(cs_wallet);

        if (!IsLocked())
            TopUpKeyPool(2); //Only assign the bare minimum here, let the background thread do the rest.

        auto& keyPool = (keyChain == KEYCHAIN_EXTERNAL ? forAccount->setKeyPoolExternal : forAccount->setKeyPoolInternal);

        if (keyPool.empty())
            return;

        CWalletDB walletdb(strWalletFile);

        nIndex = *(keyPool.begin());
        keyPool.erase(keyPool.begin());
        if (!walletdb.ReadPool(nIndex, keypoolentry))
            throw runtime_error(std::string(__func__) + ": read failed");
        if (!forAccount->HaveKey(keypoolentry.vchPubKey.GetID()))
            throw runtime_error(std::string(__func__) + ": unknown key in key pool");
        assert(keypoolentry.vchPubKey.IsValid());
        LogPrintf("keypool reserve %d\n", nIndex);
    }
}

void CWallet::MarkKeyUsed(CKeyID keyID, uint64_t usageTime)
{

    if (fFileBacked) {
        CWalletDB walletdb(strWalletFile);

        walletdb.ErasePool(this, keyID);
    }

    {
        LOCK(cs_wallet);
        for (const auto& accountItem : mapAccounts) {
            if (accountItem.second->HaveKey(keyID)) {
                if (usageTime > 0) {
                    if (fFileBacked) {
                        CWalletDB walletdb(strWalletFile);
                        accountItem.second->possiblyUpdateEarliestTime(usageTime, &walletdb);
                    } else {
                        accountItem.second->possiblyUpdateEarliestTime(usageTime, NULL);
                    }
                }

                static std::set<CKeyID> keyUsedSet;
                if (keyUsedSet.find(keyID) == keyUsedSet.end()) {
                    keyUsedSet.insert(keyID);
                    if (accountItem.second->m_Type != AccountType::Normal && accountItem.second->m_Type != AccountType::ShadowChild) {
                        accountItem.second->m_Type = AccountType::Normal;
                        std::string name = accountItem.second->getLabel();
                        if (name.find(_("[Deleted]")) != std::string::npos) {
                            name = name.replace(name.find(_("[Deleted]")), _("[Deleted]").length(), _("[Restored]"));
                        } else {
                            name = _("Restored");
                        }
                        addAccount(accountItem.second, name);
                    }
                }
            }
        }
    }

    TopUpKeyPool(10);
}

void CWallet::KeepKey(int64_t nIndex)
{

    if (fFileBacked) {
        CWalletDB walletdb(strWalletFile);
        walletdb.ErasePool(this, nIndex);
    }
    LogPrintf("keypool keep %d\n", nIndex);
}

void CWallet::ReturnKey(int64_t nIndex, CAccount* forAccount, int64_t keyChain)
{

    CKeyPool keypoolentry;
    if (fFileBacked) {
        CWalletDB walletdb(strWalletFile);
        if (!walletdb.ReadPool(nIndex, keypoolentry)) {
            LogPrintf("keypool return - aborted as key already used %d\n", nIndex);
            return;
        }
    }

    {
        LOCK(cs_wallet);
        auto& keyPool = (keyChain == KEYCHAIN_EXTERNAL ? forAccount->setKeyPoolExternal : forAccount->setKeyPoolInternal);
        keyPool.insert(nIndex);
    }
    LogPrintf("keypool return %d\n", nIndex);
}

bool CWallet::GetKeyFromPool(CPubKey& result, CAccount* forAccount, int64_t keyChain)
{
    int64_t nIndex = 0;
    CKeyPool keypool;
    {
        LOCK(cs_wallet);
        ReserveKeyFromKeyPool(nIndex, keypool, forAccount, keyChain);
        if (nIndex == -1) {
            if (IsLocked())
                return false;
            result = GenerateNewKey(*forAccount, keyChain);
            return true;
        }
        KeepKey(nIndex);
        result = keypool.vchPubKey;
    }
    return true;
}

int64_t CWallet::GetOldestKeyPoolTime()
{
    LOCK(cs_wallet);

    CKeyPool keypoolentry;
    CWalletDB walletdb(strWalletFile);

    int64_t nTime = GetTime();
    for (const auto& accountItem : mapAccounts) {
        for (auto keyChain : { KEYCHAIN_EXTERNAL, KEYCHAIN_CHANGE }) {
            const auto& keyPool = (keyChain == KEYCHAIN_EXTERNAL ? accountItem.second->setKeyPoolExternal : accountItem.second->setKeyPoolInternal);
            if (!keyPool.empty()) {
                int64_t nIndex = *(keyPool.begin());
                if (!walletdb.ReadPool(nIndex, keypoolentry))
                    throw runtime_error(std::string(__func__) + ": read oldest key in keypool failed");
                assert(keypoolentry.vchPubKey.IsValid());
                nTime = std::min(nTime, keypoolentry.nTime);
            }
        }
    }
    return nTime;
}

std::map<CTxDestination, CAmount> CWallet::GetAddressBalances()
{
    map<CTxDestination, CAmount> balances;

    {
        LOCK(cs_wallet);
        BOOST_FOREACH (PAIRTYPE(uint256, CWalletTx) walletEntry, mapWallet) {
            CWalletTx* pcoin = &walletEntry.second;

            if (!CheckFinalTx(*pcoin) || !pcoin->IsTrusted())
                continue;

            if (pcoin->IsCoinBase() && pcoin->GetBlocksToMaturity() > 0)
                continue;

            int nDepth = pcoin->GetDepthInMainChain();
            if (nDepth < (pcoin->IsFromMe(ISMINE_ALL) ? 0 : 1))
                continue;

            for (unsigned int i = 0; i < pcoin->vout.size(); i++) {
                CTxDestination addr;
                if (!IsMine(pcoin->vout[i]))
                    continue;
                if (!ExtractDestination(pcoin->vout[i].scriptPubKey, addr))
                    continue;

                CAmount n = IsSpent(walletEntry.first, i) ? 0 : pcoin->vout[i].nValue;

                if (!balances.count(addr))
                    balances[addr] = 0;
                balances[addr] += n;
            }
        }
    }

    return balances;
}

set<set<CTxDestination> > CWallet::GetAddressGroupings()
{
    AssertLockHeld(cs_wallet); // mapWallet
    set<set<CTxDestination> > groupings;
    set<CTxDestination> grouping;

    BOOST_FOREACH (PAIRTYPE(uint256, CWalletTx) walletEntry, mapWallet) {
        CWalletTx* pcoin = &walletEntry.second;

        if (pcoin->vin.size() > 0) {
            bool any_mine = false;

            BOOST_FOREACH (CTxIn txin, pcoin->vin) {
                CTxDestination address;
                if (!IsMine(txin)) /* If this input isn't mine, ignore it */
                    continue;
                if (!ExtractDestination(mapWallet[txin.prevout.hash].vout[txin.prevout.n].scriptPubKey, address))
                    continue;
                grouping.insert(address);
                any_mine = true;
            }

            if (any_mine) {
                BOOST_FOREACH (CTxOut txout, pcoin->vout)
                    if (IsChange(txout)) {
                        CTxDestination txoutAddr;
                        if (!ExtractDestination(txout.scriptPubKey, txoutAddr))
                            continue;
                        grouping.insert(txoutAddr);
                    }
            }
            if (grouping.size() > 0) {
                groupings.insert(grouping);
                grouping.clear();
            }
        }

        for (unsigned int i = 0; i < pcoin->vout.size(); i++)
            if (IsMine(pcoin->vout[i])) {
                CTxDestination address;
                if (!ExtractDestination(pcoin->vout[i].scriptPubKey, address))
                    continue;
                grouping.insert(address);
                groupings.insert(grouping);
                grouping.clear();
            }
    }

    set<set<CTxDestination>*> uniqueGroupings; // a set of pointers to groups of addresses
    map<CTxDestination, set<CTxDestination>*> setmap; // map addresses to the unique group containing it
    BOOST_FOREACH (set<CTxDestination> grouping, groupings) {

        set<set<CTxDestination>*> hits;
        map<CTxDestination, set<CTxDestination>*>::iterator it;
        BOOST_FOREACH (CTxDestination address, grouping)
            if ((it = setmap.find(address)) != setmap.end())
                hits.insert((*it).second);

        set<CTxDestination>* merged = new set<CTxDestination>(grouping);
        BOOST_FOREACH (set<CTxDestination>* hit, hits) {
            merged->insert(hit->begin(), hit->end());
            uniqueGroupings.erase(hit);
            delete hit;
        }
        uniqueGroupings.insert(merged);

        BOOST_FOREACH (CTxDestination element, *merged)
            setmap[element] = merged;
    }

    set<set<CTxDestination> > ret;
    BOOST_FOREACH (set<CTxDestination>* uniqueGrouping, uniqueGroupings) {
        ret.insert(*uniqueGrouping);
        delete uniqueGrouping;
    }

    return ret;
}

CAmount CWallet::GetAccountBalance(const std::string& strAccount, int nMinDepth, const isminefilter& filter, bool includeChildren)
{
    CWalletDB walletdb(strWalletFile);
    return GetAccountBalance(walletdb, strAccount, nMinDepth, filter, includeChildren);
}

CAmount CWallet::GetAccountBalance(CWalletDB& walletdb, const std::string& strAccount, int nMinDepth, const isminefilter& filter, bool includeChildren)
{
    CAmount nBalance = 0;

    {

        LOCK2(cs_main, cs_wallet);

        for (map<uint256, CWalletTx>::iterator it = mapWallet.begin(); it != mapWallet.end(); ++it) {
            const CWalletTx& wtx = (*it).second;
            if (!CheckFinalTx(wtx) || wtx.GetBlocksToMaturity() > 0 || wtx.GetDepthInMainChain() < 0)
                continue;

            CAmount nReceived, nSent, nFee;
            wtx.GetAccountAmounts(strAccount, nReceived, nSent, nFee, filter);

            if (nReceived != 0 && wtx.GetDepthInMainChain() >= nMinDepth) {
                nBalance += nReceived;
                nBalance -= nSent /*+ nFee*/;
            }
        }
    }

    nBalance += walletdb.GetAccountCreditDebit(strAccount);

    if (includeChildren) {
        for (const auto& accountItem : mapAccounts) {
            const auto& childAccount = accountItem.second;
            if (childAccount->getParentUUID() == strAccount) {
                nBalance += GetAccountBalance(walletdb, childAccount->getUUID(), nMinDepth, filter, includeChildren);
            }
        }
    }

    return nBalance;
}

std::set<CTxDestination> CWallet::GetAccountAddresses(const std::string& strAccount) const
{
    LOCK(cs_wallet);
    set<CTxDestination> result;
    BOOST_FOREACH (const PAIRTYPE(std::string, CAddressBookData) & item, mapAddressBook) {
        const std::string& address = item.first;
        const string& strName = item.second.name;
        if (strName == strAccount)
            result.insert(CBitcoinAddress(address).Get());
    }
    return result;
}

bool CReserveKey::GetReservedKey(CPubKey& pubkey)
{
    if (nIndex == -1) {
        CKeyPool keypool;
        pwallet->ReserveKeyFromKeyPool(nIndex, keypool, account, nKeyChain);
        if (nIndex != -1)
            vchPubKey = keypool.vchPubKey;
        else {
            return false;
        }
    }
    assert(vchPubKey.IsValid());
    pubkey = vchPubKey;
    return true;
}

void CReserveKey::KeepKey()
{
    if (nIndex != -1)
        pwallet->KeepKey(nIndex);
    nIndex = -1;
    vchPubKey = CPubKey();
}

void CReserveKey::ReturnKey()
{
    if (nIndex != -1)
        pwallet->ReturnKey(nIndex, account, nKeyChain);
    nIndex = -1;
    vchPubKey = CPubKey();
}

void CWallet::GetAllReserveKeys(set<CKeyID>& setAddress) const
{
    setAddress.clear();

    CWalletDB walletdb(strWalletFile);

    LOCK2(cs_main, cs_wallet);
    for (const auto& accountItem : mapAccounts) {
        for (auto keyChain : { KEYCHAIN_EXTERNAL, KEYCHAIN_CHANGE }) {
            const auto& keyPool = (keyChain == KEYCHAIN_EXTERNAL ? accountItem.second->setKeyPoolExternal : accountItem.second->setKeyPoolInternal);
            for (const int64_t& id : keyPool) {
                CKeyPool keypoolentry;
                if (!walletdb.ReadPool(id, keypoolentry))
                    throw runtime_error(std::string(__func__) + ": read failed");
                assert(keypoolentry.vchPubKey.IsValid());
                CKeyID keyID = keypoolentry.vchPubKey.GetID();
                if (!HaveKey(keyID))
                    throw runtime_error(std::string(__func__) + ": unknown key in key pool");
                setAddress.insert(keyID);
            }
        }
    }
}

void CWallet::UpdatedTransaction(const uint256& hashTx)
{
    {
        LOCK(cs_wallet);

        map<uint256, CWalletTx>::const_iterator mi = mapWallet.find(hashTx);
        if (mi != mapWallet.end())
            NotifyTransactionChanged(this, hashTx, CT_UPDATED);
    }
}

void CWallet::GetScriptForMining(boost::shared_ptr<CReserveScript>& script)
{

    boost::shared_ptr<CReserveKey> rKey(new CReserveKey(this, activeAccount, KEYCHAIN_EXTERNAL));
    CPubKey pubkey;
    if (!rKey->GetReservedKey(pubkey))
        return;

    script = rKey;
    script->reserveScript = CScript() << ToByteVector(pubkey) << OP_CHECKSIG;
}

void CWallet::LockCoin(const COutPoint& output)
{
    AssertLockHeld(cs_wallet); // setLockedCoins
    setLockedCoins.insert(output);
}

void CWallet::UnlockCoin(const COutPoint& output)
{
    AssertLockHeld(cs_wallet); // setLockedCoins
    setLockedCoins.erase(output);
}

void CWallet::UnlockAllCoins()
{
    AssertLockHeld(cs_wallet); // setLockedCoins
    setLockedCoins.clear();
}

bool CWallet::IsLockedCoin(uint256 hash, unsigned int n) const
{
    AssertLockHeld(cs_wallet); // setLockedCoins
    COutPoint outpt(hash, n);

    return (setLockedCoins.count(outpt) > 0);
}

void CWallet::ListLockedCoins(std::vector<COutPoint>& vOutpts)
{
    AssertLockHeld(cs_wallet); // setLockedCoins
    for (std::set<COutPoint>::iterator it = setLockedCoins.begin();
         it != setLockedCoins.end(); it++) {
        COutPoint outpt = (*it);
        vOutpts.push_back(outpt);
    }
}

/** @} */ // end of Actions

class CAffectedKeysVisitor : public boost::static_visitor<void> {
private:
    const CKeyStore& keystore;
    std::vector<CKeyID>& vKeys;

public:
    CAffectedKeysVisitor(const CKeyStore& keystoreIn, std::vector<CKeyID>& vKeysIn)
        : keystore(keystoreIn)
        , vKeys(vKeysIn)
    {
    }

    void Process(const CScript& script)
    {
        txnouttype type;
        std::vector<CTxDestination> vDest;
        int nRequired;
        if (ExtractDestinations(script, type, vDest, nRequired)) {
            BOOST_FOREACH (const CTxDestination& dest, vDest)
                boost::apply_visitor(*this, dest);
        }
    }

    void operator()(const CKeyID& keyId)
    {
        if (keystore.HaveKey(keyId))
            vKeys.push_back(keyId);
    }

    void operator()(const CScriptID& scriptId)
    {
        CScript script;
        if (keystore.GetCScript(scriptId, script))
            Process(script);
    }

    void operator()(const CNoDestination& none) {}
};

void CWallet::GetKeyBirthTimes(std::map<CKeyID, int64_t>& mapKeyBirth) const
{
    LOCK(cs_wallet);

    AssertLockHeld(cs_wallet); // mapKeyMetadata
    mapKeyBirth.clear();

    for (std::map<CKeyID, CKeyMetadata>::const_iterator it = mapKeyMetadata.begin(); it != mapKeyMetadata.end(); it++)
        if (it->second.nCreateTime)
            mapKeyBirth[it->first] = it->second.nCreateTime;

    CBlockIndex* pindexMax = chainActive[std::max(0, chainActive.Height() - 144)]; // the tip can be reorganized; use a 144-block safety margin
    std::map<CKeyID, CBlockIndex*> mapKeyFirstBlock;
    std::set<CKeyID> setKeys;
    for (const auto accountPair : mapAccounts) {
        accountPair.second->GetKeys(setKeys);
        for (const auto keyid : setKeys) {
            if (mapKeyBirth.count(keyid) == 0)
                mapKeyFirstBlock[keyid] = pindexMax;
        }
        setKeys.clear();
    }

    if (mapKeyFirstBlock.empty())
        return;

    std::vector<CKeyID> vAffected;
    for (std::map<uint256, CWalletTx>::const_iterator it = mapWallet.begin(); it != mapWallet.end(); it++) {

        const CWalletTx& wtx = (*it).second;
        BlockMap::const_iterator blit = mapBlockIndex.find(wtx.hashBlock);
        if (blit != mapBlockIndex.end() && chainActive.Contains(blit->second)) {

            int nHeight = blit->second->nHeight;
            BOOST_FOREACH (const CTxOut& txout, wtx.vout) {

                CAffectedKeysVisitor(activeAccount->externalKeyStore, vAffected).Process(txout.scriptPubKey);
                BOOST_FOREACH (const CKeyID& keyid, vAffected) {

                    std::map<CKeyID, CBlockIndex*>::iterator rit = mapKeyFirstBlock.find(keyid);
                    if (rit != mapKeyFirstBlock.end() && nHeight < rit->second->nHeight)
                        rit->second = blit->second;
                }
                vAffected.clear();
            }
        }
    }

    for (std::map<CKeyID, CBlockIndex*>::const_iterator it = mapKeyFirstBlock.begin(); it != mapKeyFirstBlock.end(); it++)
        mapKeyBirth[it->first] = it->second->GetBlockTime() - 7200; // block times can be 2h off
}

bool CWallet::AddDestData(const CTxDestination& dest, const std::string& key, const std::string& value)
{
    if (boost::get<CNoDestination>(&dest))
        return false;

    mapAddressBook[CBitcoinAddress(dest).ToString()].destdata.insert(std::make_pair(key, value));
    if (!fFileBacked)
        return true;
    return CWalletDB(strWalletFile).WriteDestData(CBitcoinAddress(dest).ToString(), key, value);
}

bool CWallet::EraseDestData(const CTxDestination& dest, const std::string& key)
{
    if (!mapAddressBook[CBitcoinAddress(dest).ToString()].destdata.erase(key))
        return false;
    if (!fFileBacked)
        return true;
    return CWalletDB(strWalletFile).EraseDestData(CBitcoinAddress(dest).ToString(), key);
}

bool CWallet::LoadDestData(const CTxDestination& dest, const std::string& key, const std::string& value)
{
    mapAddressBook[CBitcoinAddress(dest).ToString()].destdata.insert(std::make_pair(key, value));
    return true;
}

bool CWallet::GetDestData(const CTxDestination& dest, const std::string& key, std::string* value) const
{
    std::map<std::string, CAddressBookData>::const_iterator i = mapAddressBook.find(CBitcoinAddress(dest).ToString());
    if (i != mapAddressBook.end()) {
        CAddressBookData::StringMap::const_iterator j = i->second.destdata.find(key);
        if (j != i->second.destdata.end()) {
            if (value)
                *value = j->second;
            return true;
        }
    }
    return false;
}

std::string CWallet::GetWalletHelpString(bool showDebug)
{
    std::string strUsage = HelpMessageGroup(_("Wallet options:"));
    strUsage += HelpMessageOpt("-disablewallet", _("Do not load the wallet and disable wallet RPC calls"));
    strUsage += HelpMessageOpt("-keypool=<n>", strprintf(_("Set key pool size to <n> (default: %u)"), DEFAULT_KEYPOOL_SIZE));
    strUsage += HelpMessageOpt("-fallbackfee=<amt>", strprintf(_("A fee rate (in %s/kB) that will be used when fee estimation has insufficient data (default: %s)"),
                                                               CURRENCY_UNIT, FormatMoney(DEFAULT_FALLBACK_FEE)));
    strUsage += HelpMessageOpt("-mintxfee=<amt>", strprintf(_("Fees (in %s/kB) smaller than this are considered zero fee for transaction creation (default: %s)"),
                                                            CURRENCY_UNIT, FormatMoney(DEFAULT_TRANSACTION_MINFEE)));
    strUsage += HelpMessageOpt("-paytxfee=<amt>", strprintf(_("Fee (in %s/kB) to add to transactions you send (default: %s)"),
                                                            CURRENCY_UNIT, FormatMoney(payTxFee.GetFeePerK())));
    strUsage += HelpMessageOpt("-rescan", _("Rescan the block chain for missing wallet transactions on startup"));
    strUsage += HelpMessageOpt("-salvagewallet", _("Attempt to recover private keys from a corrupt wallet on startup"));
    if (showDebug)
        strUsage += HelpMessageOpt("-sendfreetransactions", strprintf(_("Send transactions as zero-fee transactions if possible (default: %u)"), DEFAULT_SEND_FREE_TRANSACTIONS));
    strUsage += HelpMessageOpt("-spendzeroconfchange", strprintf(_("Spend unconfirmed change when sending transactions (default: %u)"), DEFAULT_SPEND_ZEROCONF_CHANGE));
    strUsage += HelpMessageOpt("-txconfirmtarget=<n>", strprintf(_("If paytxfee is not set, include enough fee so transactions begin confirmation on average within n blocks (default: %u)"), DEFAULT_TX_CONFIRM_TARGET));
    strUsage += HelpMessageOpt("-usehd", _("Use hierarchical deterministic key generation (HD) after BIP32. Only has effect during wallet creation/first start") + " " + strprintf(_("(default: %u)"), DEFAULT_USE_HD_WALLET));
    strUsage += HelpMessageOpt("-upgradewallet", _("Upgrade wallet to latest format on startup"));
    strUsage += HelpMessageOpt("-wallet=<file>", _("Specify wallet file (within data directory)") + " " + strprintf(_("(default: %s)"), DEFAULT_WALLET_DAT));
    strUsage += HelpMessageOpt("-walletbroadcast", _("Make the wallet broadcast transactions") + " " + strprintf(_("(default: %u)"), DEFAULT_WALLETBROADCAST));
    strUsage += HelpMessageOpt("-walletnotify=<cmd>", _("Execute command when a wallet transaction changes (%s in cmd is replaced by TxID)"));
    strUsage += HelpMessageOpt("-zapwallettxes=<mode>", _("Delete all wallet transactions and only recover those parts of the blockchain through -rescan on startup") + " " + _("(1 = keep tx meta data e.g. account owner and payment request information, 2 = drop tx meta data)"));

    if (showDebug) {
        strUsage += HelpMessageGroup(_("Wallet debugging/testing options:"));

        strUsage += HelpMessageOpt("-dblogsize=<n>", strprintf("Flush wallet database activity from memory to disk log every <n> megabytes (default: %u)", DEFAULT_WALLET_DBLOGSIZE));
        strUsage += HelpMessageOpt("-flushwallet", strprintf("Run a thread to flush wallet periodically (default: %u)", DEFAULT_FLUSHWALLET));
        strUsage += HelpMessageOpt("-privdb", strprintf("Sets the DB_PRIVATE flag in the wallet db environment (default: %u)", DEFAULT_WALLET_PRIVDB));
    }

    return strUsage;
}

void CWallet::changeAccountName(CAccount* account, const std::string& newName, bool notify)
{

    std::string finalNewName = newName;
    std::string oldName = account->getLabel();
    {
        LOCK(cs_wallet);

        CWalletDB db(strWalletFile);

        int nPrefix = 1;
        bool possibleDuplicate = true;
        while (possibleDuplicate) {
            possibleDuplicate = false;
            for (const auto& labelIter : mapAccountLabels) {
                if (labelIter.second == finalNewName) {
                    finalNewName = newName + strprintf("_%d", nPrefix++);
                    possibleDuplicate = true;
                    continue;
                }
            }
        }

        account->setLabel(finalNewName, &db);
        mapAccountLabels[account->getUUID()] = finalNewName;
    }

    if (notify && oldName != finalNewName)
        NotifyAccountNameChanged(this, account);
}

void CWallet::deleteAccount(CAccount* account)
{
    std::string newLabel = account->getLabel();
    if (newLabel.find(_("[Restored]")) != std::string::npos) {
        newLabel = newLabel.replace(newLabel.find(_("[Restored]")), _("[Restored]").length(), _("[Deleted]"));
    } else {
        newLabel = newLabel + " " + _("[Deleted]");
    }

    {
        LOCK(cs_wallet);

        CWalletDB db(strWalletFile);
        account->setLabel(newLabel, &db);
        account->m_Type = AccountType::Deleted;
        mapAccountLabels[account->getUUID()] = newLabel;
        if (!db.WriteAccount(account->getUUID(), account)) {
            throw runtime_error("Writing account failed");
        }
    }
    NotifyAccountDeleted(this, account);
}

void CWallet::addAccount(CAccount* account, const std::string& newName)
{
    {
        LOCK(cs_wallet);

        CWalletDB walletdb(strWalletFile);
        if (!walletdb.WriteAccount(account->getUUID(), account)) {
            throw runtime_error("Writing account failed");
        }
        mapAccounts[account->getUUID()] = account;
        changeAccountName(account, newName, false);
    }

    if (account->m_Type == AccountType::Normal) {
        NotifyAccountAdded(this, account);
        NotifyAccountNameChanged(this, account);
        NotifyUpdateAccountList(this, account);
    }
}

bool CWallet::InitLoadWallet()
{
    std::string walletFile = GetArg("-wallet", DEFAULT_WALLET_DAT);

    std::vector<CWalletTx> vWtx;

    if (GetBoolArg("-zapwallettxes", false)) {
        uiInterface.InitMessage(_("Zapping all transactions from wallet..."));

        CWallet* tempWallet = new CWallet(walletFile);
        DBErrors nZapWalletRet = tempWallet->ZapWalletTx(vWtx);
        if (nZapWalletRet != DB_LOAD_OK) {
            return InitError(strprintf(_("Error loading %s: Wallet corrupted"), walletFile));
        }

        delete tempWallet;
        tempWallet = NULL;
    }

    uiInterface.InitMessage(_("Loading wallet..."));

    int64_t nStart = GetTimeMillis();
    bool fFirstRun = true;
    CWallet* walletInstance = new CWallet(walletFile);
    DBErrors nLoadWalletRet = walletInstance->LoadWallet(fFirstRun);
    if (nLoadWalletRet != DB_LOAD_OK) {
        if (nLoadWalletRet == DB_CORRUPT)
            return InitError(strprintf(_("Error loading %s: Wallet corrupted"), walletFile));
        else if (nLoadWalletRet == DB_NONCRITICAL_ERROR) {
            InitWarning(strprintf(_("Error reading %s! All keys read correctly, but transaction data"
                                    " or address book entries might be missing or incorrect."),
                                  walletFile));
        } else if (nLoadWalletRet == DB_TOO_NEW)
            return InitError(strprintf(_("Error loading %s: Wallet requires newer version of %s"),
                                       walletFile, _(PACKAGE_NAME)));
        else if (nLoadWalletRet == DB_NEED_REWRITE) {
            return InitError(strprintf(_("Wallet needed to be rewritten: restart %s to complete"), _(PACKAGE_NAME)));
        } else
            return InitError(strprintf(_("Error loading %s"), walletFile));
    }

    if (GetBoolArg("-upgradewallet", fFirstRun)) {
        int nMaxVersion = GetArg("-upgradewallet", 0);
        if (nMaxVersion == 0) // the -upgradewallet without argument case
        {
            LogPrintf("Performing wallet upgrade to %i\n", FEATURE_LATEST);
            nMaxVersion = CLIENT_VERSION;
            walletInstance->SetMinVersion(FEATURE_LATEST); // permanently upgrade the wallet immediately
        } else
            LogPrintf("Allowing wallet upgrade up to %i\n", nMaxVersion);
        if (nMaxVersion < walletInstance->GetVersion()) {
            return InitError(_("Cannot downgrade wallet"));
        }
        walletInstance->SetMaxVersion(nMaxVersion);
    }

    if (fFirstRun) {

        if (GetBoolArg("-usehd", DEFAULT_USE_HD_WALLET)) {
            if (fNoUI) {
                std::vector<unsigned char> entropy(16);
                GetStrongRandBytes(&entropy[0], 16);
                GuldenApplication::gApp->setRecoveryPhrase(mnemonicFromEntropy(entropy, entropy.size() * 8));
            }

            walletInstance->activeSeed = new CHDSeed(GuldenApplication::gApp->getRecoveryPhrase().c_str(), CHDSeed::CHDSeed::BIP44);
            if (!CWalletDB(walletFile).WriteHDSeed(*walletInstance->activeSeed)) {
                throw runtime_error("Writing seed failed");
            }
            walletInstance->mapSeeds[walletInstance->activeSeed->getUUID()] = walletInstance->activeSeed;
            walletInstance->activeAccount = walletInstance->GenerateNewAccount("My account", AccountType::Normal, AccountSubType::Desktop);

            if (GuldenApplication::gApp->isRecovery) {
                CHDSeed* seedBip32 = new CHDSeed(GuldenApplication::gApp->getRecoveryPhrase().c_str(), CHDSeed::CHDSeed::BIP32);
                if (!CWalletDB(walletFile).WriteHDSeed(*seedBip32)) {
                    throw runtime_error("Writing bip32 seed failed");
                }
                CHDSeed* seedBip32Legacy = new CHDSeed(GuldenApplication::gApp->getRecoveryPhrase().c_str(), CHDSeed::CHDSeed::BIP32Legacy);
                if (!CWalletDB(walletFile).WriteHDSeed(*seedBip32Legacy)) {
                    throw runtime_error("Writing bip32 legacy seed failed");
                }
                walletInstance->mapSeeds[seedBip32->getUUID()] = seedBip32;
                walletInstance->mapSeeds[seedBip32Legacy->getUUID()] = seedBip32Legacy;

                CAccountHD* newAccountBip32 = seedBip32->GenerateAccount(AccountSubType::Desktop, NULL);
                newAccountBip32->m_Type = AccountType::ShadowChild;
                walletInstance->activeAccount->AddChild(newAccountBip32);
                walletInstance->addAccount(newAccountBip32, "BIP32 child account");

                CAccountHD* newAccountBip32Legacy = seedBip32Legacy->GenerateAccount(AccountSubType::Desktop, NULL);
                newAccountBip32Legacy->m_Type = AccountType::ShadowChild;
                walletInstance->activeAccount->AddChild(newAccountBip32Legacy);
                walletInstance->addAccount(newAccountBip32Legacy, "BIP32 legacy child account");
            }

            {
                CWalletDB walletdb(walletFile);
                walletdb.WritePrimarySeed(*walletInstance->activeSeed);
                walletdb.WritePrimaryAccount(walletInstance->activeAccount);
            }

            GuldenApplication::gApp->BurnRecoveryPhrase();

            walletInstance->TopUpKeyPool(2);
        } else {
            walletInstance->activeAccount = new CAccount();
            walletInstance->activeAccount->m_Type = AccountType::Normal;
            walletInstance->activeAccount->m_SubType = AccountSubType::Desktop;

            walletInstance->TopUpKeyPool(2);
        }

        pwalletMain = walletInstance;
        walletInstance->SetBestChain(chainActive.GetLocator());

        CWalletDB walletdb(walletFile);
    } else {

        if (fNoUI) {
            if (!walletInstance->activeAccount->IsHD() && !walletInstance->activeSeed) {
                if (mapArgs.count("-usehd") == 0) {
                    throw runtime_error("Must specify -usehd=1 or -usehd=0, in order to allow or refuse HD upgrade.");
                }
            }
        }

        if (GetBoolArg("-usehd", DEFAULT_USE_HD_WALLET)) {
            if (!walletInstance->activeAccount->IsHD() && !walletInstance->activeSeed) {
                while (true) {
                    {
                        LOCK(walletInstance->cs_wallet);
                        if (!walletInstance->IsLocked())
                            break;
                        walletInstance->wantDelayLock = true;
                        uiInterface.RequestUnlock(walletInstance, _("Wallet unlock required for wallet upgrade"));
                    }
                    MilliSleep(5000);
                }

                bool walletWasCrypted = walletInstance->activeAccount->externalKeyStore.IsCrypted();
                {
                    LOCK(walletInstance->cs_wallet);

                    {
                        CWalletDB walletdb(walletFile);
                        walletInstance->changeAccountName(walletInstance->activeAccount, _("Legacy account"), true);
                        if (!walletdb.WriteAccount(walletInstance->activeAccount->getUUID(), walletInstance->activeAccount)) {
                            throw runtime_error("Writing legacy account failed");
                        }
                        if (walletWasCrypted && !walletInstance->activeAccount->internalKeyStore.IsCrypted()) {
                            walletInstance->activeAccount->internalKeyStore.SetCrypted();
                            walletInstance->activeAccount->internalKeyStore.Unlock(walletInstance->activeAccount->vMasterKey);
                        }
                        walletInstance->ForceRewriteKeys(*walletInstance->activeAccount);
                    }

                    std::vector<unsigned char> entropy(16);
                    GetStrongRandBytes(&entropy[0], 16);
                    walletInstance->activeSeed = new CHDSeed(mnemonicFromEntropy(entropy, entropy.size() * 8).c_str(), CHDSeed::CHDSeed::BIP44);
                    if (!CWalletDB(walletFile).WriteHDSeed(*walletInstance->activeSeed)) {
                        throw runtime_error("Writing seed failed");
                    }
                    if (walletWasCrypted) {
                        if (!walletInstance->activeSeed->Encrypt(walletInstance->activeAccount->vMasterKey)) {
                            throw runtime_error("Encrypting seed failed");
                        }
                    }
                    walletInstance->mapSeeds[walletInstance->activeSeed->getUUID()] = walletInstance->activeSeed;
                    {
                        CWalletDB walletdb(walletFile);
                        walletdb.WritePrimarySeed(*walletInstance->activeSeed);
                    }
                    walletInstance->activeAccount = walletInstance->GenerateNewAccount(_("My account"), AccountType::Normal, AccountSubType::Desktop);
                    {
                        CWalletDB walletdb(walletFile);
                        walletdb.WritePrimaryAccount(walletInstance->activeAccount);
                    }
                }
            }
        } else {
            while (true) {
                {
                    LOCK(walletInstance->cs_wallet);
                    if (!walletInstance->IsLocked())
                        break;
                    walletInstance->wantDelayLock = true;
                    uiInterface.RequestUnlock(walletInstance, _("Wallet unlock required for wallet upgrade"));
                }
                MilliSleep(5000);
            }

            bool walletWasCrypted = walletInstance->activeAccount->externalKeyStore.IsCrypted();
            {
                LOCK(walletInstance->cs_wallet);

                {
                    CWalletDB walletdb(walletFile);
                    walletInstance->changeAccountName(walletInstance->activeAccount, _("Legacy account"), true);
                    if (!walletdb.WriteAccount(walletInstance->activeAccount->getUUID(), walletInstance->activeAccount)) {
                        throw runtime_error("Writing legacy account failed");
                    }
                    if (walletWasCrypted && !walletInstance->activeAccount->internalKeyStore.IsCrypted()) {
                        walletInstance->activeAccount->internalKeyStore.SetCrypted();
                        walletInstance->activeAccount->internalKeyStore.Unlock(walletInstance->activeAccount->vMasterKey);
                    }
                    walletInstance->ForceRewriteKeys(*walletInstance->activeAccount);
                }
            }
        }
    }

    if (GuldenApplication::gApp->isRecovery) {
        walletInstance->nTimeFirstKey = chainActive.Genesis()->nTime;
    }

    LogPrintf(" wallet      %15dms\n", GetTimeMillis() - nStart);

    RegisterValidationInterface(walletInstance);

    CBlockIndex* pindexRescan = chainActive.Tip();
    if (GetBoolArg("-rescan", false) || GuldenApplication::gApp->isRecovery)
        pindexRescan = chainActive.Genesis();
    else {
        CWalletDB walletdb(walletFile);
        CBlockLocator locator;
        if (walletdb.ReadBestBlock(locator))
            pindexRescan = FindForkInGlobalIndex(chainActive, locator);
        else
            pindexRescan = chainActive.Genesis();
    }
    if (chainActive.Tip() && chainActive.Tip() != pindexRescan) {

        if (fPruneMode) {
            CBlockIndex* block = chainActive.Tip();
            while (block && block->pprev && (block->pprev->nStatus & BLOCK_HAVE_DATA) && block->pprev->nTx > 0 && pindexRescan != block)
                block = block->pprev;

            if (pindexRescan != block)
                return InitError(_("Prune: last wallet synchronisation goes beyond pruned data. You need to -reindex (download the whole blockchain again in case of pruned node)"));
        }

        uiInterface.InitMessage(_("Rescanning..."));
        LogPrintf("Rescanning last %i blocks (from block %i)...\n", chainActive.Height() - pindexRescan->nHeight, pindexRescan->nHeight);
        nStart = GetTimeMillis();
        walletInstance->ScanForWalletTransactions(pindexRescan, true);
        LogPrintf(" rescan      %15dms\n", GetTimeMillis() - nStart);
        walletInstance->SetBestChain(chainActive.GetLocator());
        nWalletDBUpdated++;

        if (GetBoolArg("-zapwallettxes", false) && GetArg("-zapwallettxes", "1") != "2") {
            CWalletDB walletdb(walletFile);

            BOOST_FOREACH (const CWalletTx& wtxOld, vWtx) {
                uint256 hash = wtxOld.GetHash();
                std::map<uint256, CWalletTx>::iterator mi = walletInstance->mapWallet.find(hash);
                if (mi != walletInstance->mapWallet.end()) {
                    const CWalletTx* copyFrom = &wtxOld;
                    CWalletTx* copyTo = &mi->second;
                    copyTo->mapValue = copyFrom->mapValue;
                    copyTo->vOrderForm = copyFrom->vOrderForm;
                    copyTo->nTimeReceived = copyFrom->nTimeReceived;
                    copyTo->nTimeSmart = copyFrom->nTimeSmart;
                    copyTo->fFromMe = copyFrom->fFromMe;
                    copyTo->strFromAccount = copyFrom->strFromAccount;
                    copyTo->nOrderPos = copyFrom->nOrderPos;
                    walletdb.WriteTx(*copyTo);
                }
            }
        }
    }
    walletInstance->SetBroadcastTransactions(GetBoolArg("-walletbroadcast", DEFAULT_WALLETBROADCAST));

    pwalletMain = walletInstance;
    return true;
}

bool CWallet::ParameterInteraction()
{
    if (mapArgs.count("-mintxfee")) {
        CAmount n = 0;
        if (ParseMoney(mapArgs["-mintxfee"], n) && n > 0)
            CWallet::minTxFee = CFeeRate(n);
        else
            return InitError(AmountErrMsg("mintxfee", mapArgs["-mintxfee"]));
    }
    if (mapArgs.count("-fallbackfee")) {
        CAmount nFeePerK = 0;
        if (!ParseMoney(mapArgs["-fallbackfee"], nFeePerK))
            return InitError(strprintf(_("Invalid amount for -fallbackfee=<amount>: '%s'"), mapArgs["-fallbackfee"]));
        if (nFeePerK > HIGH_TX_FEE_PER_KB)
            InitWarning(_("-fallbackfee is set very high! This is the transaction fee you may pay when fee estimates are not available."));
        CWallet::fallbackFee = CFeeRate(nFeePerK);
    }
    if (mapArgs.count("-paytxfee")) {
        CAmount nFeePerK = 0;
        if (!ParseMoney(mapArgs["-paytxfee"], nFeePerK))
            return InitError(AmountErrMsg("paytxfee", mapArgs["-paytxfee"]));
        if (nFeePerK > HIGH_TX_FEE_PER_KB)
            InitWarning(_("-paytxfee is set very high! This is the transaction fee you will pay if you send a transaction."));
        payTxFee = CFeeRate(nFeePerK, 1000);
        if (payTxFee < ::minRelayTxFee) {
            return InitError(strprintf(_("Invalid amount for -paytxfee=<amount>: '%s' (must be at least %s)"),
                                       mapArgs["-paytxfee"], ::minRelayTxFee.ToString()));
        }
    }
    if (mapArgs.count("-maxtxfee")) {
        CAmount nMaxFee = 0;
        if (!ParseMoney(mapArgs["-maxtxfee"], nMaxFee))
            return InitError(AmountErrMsg("maxtxfee", mapArgs["-maxtxfee"]));
        if (nMaxFee > HIGH_MAX_TX_FEE)
            InitWarning(_("-maxtxfee is set very high! Fees this large could be paid on a single transaction."));
        maxTxFee = nMaxFee;
        if (CFeeRate(maxTxFee, 1000) < ::minRelayTxFee) {
            return InitError(strprintf(_("Invalid amount for -maxtxfee=<amount>: '%s' (must be at least the minrelay fee of %s to prevent stuck transactions)"),
                                       mapArgs["-maxtxfee"], ::minRelayTxFee.ToString()));
        }
    }
    nTxConfirmTarget = GetArg("-txconfirmtarget", DEFAULT_TX_CONFIRM_TARGET);
    bSpendZeroConfChange = GetBoolArg("-spendzeroconfchange", DEFAULT_SPEND_ZEROCONF_CHANGE);
    fSendFreeTransactions = GetBoolArg("-sendfreetransactions", DEFAULT_SEND_FREE_TRANSACTIONS);

    return true;
}

bool CWallet::BackupWallet(const std::string& strDest)
{
    if (!fFileBacked)
        return false;
    while (true) {
        {
            LOCK(bitdb.cs_db);
            if (!bitdb.mapFileUseCount.count(strWalletFile) || bitdb.mapFileUseCount[strWalletFile] == 0) {

                bitdb.CloseDb(strWalletFile);
                bitdb.CheckpointLSN(strWalletFile);
                bitdb.mapFileUseCount.erase(strWalletFile);

                boost::filesystem::path pathSrc = GetDataDir() / strWalletFile;
                boost::filesystem::path pathDest(strDest);
                if (boost::filesystem::is_directory(pathDest))
                    pathDest /= strWalletFile;

                try {
#if BOOST_VERSION >= 104000
                    boost::filesystem::copy_file(pathSrc, pathDest, boost::filesystem::copy_option::overwrite_if_exists);
#else
                    boost::filesystem::copy_file(pathSrc, pathDest);
#endif
                    LogPrintf("copied %s to %s\n", strWalletFile, pathDest.string());
                    return true;
                }
                catch (const boost::filesystem::filesystem_error& e) {
                    LogPrintf("error copying %s to %s - %s\n", strWalletFile, pathDest.string(), e.what());
                    return false;
                }
            }
        }
        MilliSleep(100);
    }
    return false;
}

void CWallet::setActiveAccount(CAccount* newActiveAccount)
{
    if (activeAccount != newActiveAccount) {
        activeAccount = newActiveAccount;
        CWalletDB walletdb(strWalletFile);
        walletdb.WritePrimaryAccount(activeAccount);

        NotifyActiveAccountChanged(this, newActiveAccount);
    }
}

CAccount* CWallet::getActiveAccount()
{
    return activeAccount;
}

void CWallet::setActiveSeed(CHDSeed* newActiveSeed)
{
    if (activeSeed != newActiveSeed) {
        activeSeed = newActiveSeed;
        CWalletDB walletdb(strWalletFile);
        walletdb.WritePrimarySeed(*activeSeed);
    }
}

CHDSeed* CWallet::GenerateHDSeed()
{
    if (IsCrypted() && (!activeAccount || IsLocked())) {
        throw runtime_error("Generating seed requires unlocked wallet");
    }

    std::vector<unsigned char> entropy(16);
    GetStrongRandBytes(&entropy[0], 16);
    CHDSeed* newSeed = new CHDSeed(mnemonicFromEntropy(entropy, entropy.size() * 8).c_str(), CHDSeed::CHDSeed::BIP44);
    if (!CWalletDB(strWalletFile).WriteHDSeed(*newSeed)) {
        throw runtime_error("Writing seed failed");
    }
    if (IsCrypted()) {
        if (!newSeed->Encrypt(activeAccount->vMasterKey)) {
            throw runtime_error("Encrypting seed failed");
        }
    }
    mapSeeds[newSeed->getUUID()] = newSeed;

    return newSeed;
}

CHDSeed* CWallet::ImportHDSeed(SecureString mnemonic)
{
    if (IsCrypted() && (!activeAccount || IsLocked())) {
        throw runtime_error("Generating seed requires unlocked wallet");
    }

    if (!checkMnemonic(mnemonic)) {
        throw runtime_error("Not a valid Gulden mnemonic");
    }

    std::vector<unsigned char> entropy(16);
    GetStrongRandBytes(&entropy[0], 16);
    CHDSeed* newSeed = new CHDSeed(mnemonic, CHDSeed::CHDSeed::BIP44);
    if (!CWalletDB(strWalletFile).WriteHDSeed(*newSeed)) {
        throw runtime_error("Writing seed failed");
    }
    if (IsCrypted()) {
        if (!newSeed->Encrypt(activeAccount->vMasterKey)) {
            throw runtime_error("Encrypting seed failed");
        }
    }
    mapSeeds[newSeed->getUUID()] = newSeed;

    return newSeed;
}

CHDSeed* CWallet::getActiveSeed()
{
    return activeSeed;
}

CKeyPool::CKeyPool()
{
    nTime = GetTime();
}

CKeyPool::CKeyPool(const CPubKey& vchPubKeyIn, const std::string& accountNameIn, int64_t nChainIn)
    : nTime(GetTime())
    , vchPubKey(vchPubKeyIn)
    , accountName(accountNameIn)
    , nChain(nChainIn)
{
}

CWalletKey::CWalletKey(int64_t nExpires)
{
    nTimeCreated = (nExpires ? GetTime() : 0);
    nTimeExpires = nExpires;
}

int CMerkleTx::SetMerkleBranch(const CBlock& block)
{
    AssertLockHeld(cs_main);
    CBlock blockTmp;

    hashBlock = block.GetHash();

    for (nIndex = 0; nIndex < (int)block.vtx.size(); nIndex++)
        if (block.vtx[nIndex] == *(CTransaction*)this)
            break;
    if (nIndex == (int)block.vtx.size()) {
        nIndex = -1;
        LogPrintf("ERROR: SetMerkleBranch(): couldn't find tx in block\n");
        return 0;
    }

    BlockMap::iterator mi = mapBlockIndex.find(hashBlock);
    if (mi == mapBlockIndex.end())
        return 0;
    const CBlockIndex* pindex = (*mi).second;
    if (!pindex || !chainActive.Contains(pindex))
        return 0;

    return chainActive.Height() - pindex->nHeight + 1;
}

int CMerkleTx::GetDepthInMainChain(const CBlockIndex*& pindexRet) const
{
    if (hashUnset())
        return 0;

    AssertLockHeld(cs_main);

    BlockMap::iterator mi = mapBlockIndex.find(hashBlock);
    if (mi == mapBlockIndex.end())
        return 0;
    CBlockIndex* pindex = (*mi).second;
    if (!pindex || !chainActive.Contains(pindex))
        return 0;

    pindexRet = pindex;
    return ((nIndex == -1) ? (-1) : 1) * (chainActive.Height() - pindex->nHeight + 1);
}

int CMerkleTx::GetBlocksToMaturity() const
{
    if (!IsCoinBase())
        return 0;
    return max(0, (COINBASE_MATURITY + 1) - GetDepthInMainChain());
}

bool CMerkleTx::AcceptToMemoryPool(bool fLimitFree, CAmount nAbsurdFee)
{
    CValidationState state;
    return ::AcceptToMemoryPool(mempool, state, *this, fLimitFree, NULL, false, nAbsurdFee);
}
