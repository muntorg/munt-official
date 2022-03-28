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

#include "wallet/wallet.h"
#include "wallet/wallettx.h"

#include "alert.h"
#include "base58.h"
#include "checkpoints.h"
#include "chain.h"
#include "wallet/coincontrol.h"
#include "consensus/consensus.h"
#include "consensus/validation.h"
#include "consensus/tx_verify.h"
#include "fs.h"
#include "key.h"
#include "keystore.h"
#include "validation/validation.h"
#include "validation/witnessvalidation.h"
#include "net.h"
#include "policy/fees.h"
#include "policy/policy.h"
#include "policy/rbf.h"
#include "primitives/block.h"
#include "primitives/transaction.h"
#include "script/script.h"
#include "script/sign.h"
#include "scheduler.h"
#include "spvscanner.h"
#include "timedata.h"
#include "txmempool.h"
#include "util.h"
#include "ui_interface.h"
#include "utilmoneystr.h"

#include <assert.h>

#include <boost/algorithm/string/replace.hpp>
#include <boost/thread.hpp>
#include <algorithm>
#include <random>
#include <fstream>

//Gulden specific includes
#include "init.h"
#include <unity/appmanager.h>
#include <wallet/mnemonic.h>
#include <script/ismine.h>
#include <txdb.h>

//fixme: (PHASE5) - we can remove these includes after phase4 activation.
#include "witnessutil.h"
#include "validation/validation.h"

std::vector<CWalletRef> vpwallets;
CWalletRef pactiveWallet = NULL;


/** Transaction fee set by the user */
CFeeRate payTxFee(DEFAULT_TRANSACTION_FEE);
unsigned int nTxConfirmTarget = DEFAULT_TX_CONFIRM_TARGET;
bool bSpendZeroConfChange = DEFAULT_SPEND_ZEROCONF_CHANGE;
bool fWalletRbf = DEFAULT_WALLET_RBF;

const char * DEFAULT_WALLET_DAT = "wallet.dat";
//const uint32_t BIP32_HARDENED_KEY_LIMIT = 0x80000000;

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

/** @defgroup mapWallet
 *
 * @{
 */

struct CompareValueOnly
{
    bool operator()(const CInputCoin& t1,
                    const CInputCoin& t2) const
    {
        return t1.txout.nValue < t2.txout.nValue;
    }
};

std::string COutput::ToString() const
{
    return strprintf("COutput(%s, %d, %d) [%s]", tx->GetHash().ToString(), i, nDepth, FormatMoney(tx->tx->vout[i].nValue));
}

const CWalletTx* CWallet::GetWalletTx(const uint256& hash) const
{
    LOCK(cs_wallet);
    std::map<uint256, CWalletTx>::const_iterator it = mapWallet.find(hash);
    if (it == mapWallet.end())
        return NULL;
    return &(it->second);
}

bool CWallet::GetTxHash(const COutPoint& outpoint, uint256& txHash) const
{
    auto hashIter = mapWalletHash.find(outpoint.getBucketHash());
    if (hashIter != mapWalletHash.end())
    {
        txHash = hashIter->second;
        return true;
    }
    else
    {
        return ::GetTxHash(outpoint, txHash);
    }
}

CWalletTx* CWallet::GetWalletTx(const COutPoint& outpoint) const
{
    uint256 txHash;
    if (outpoint.isHash)
    {
        txHash = outpoint.getTransactionHash();
    }
    else
    {
        if (!CWallet::GetTxHash(outpoint, txHash))
        {
            return nullptr;
        }
    }
    return const_cast<CWalletTx*>(GetWalletTx(txHash));
}

bool CWallet::AddKeyPubKey(const CKey& secret, const CPubKey &pubkey, CAccount& forAccount, int nKeyChain)
{
    AssertLockHeld(cs_wallet); // mapKeyMetadata
    if (!forAccount.AddKeyPubKey(secret, pubkey, nKeyChain))
        return false;

    // check if we need to remove from watch-only
    CScript script;
    script = GetScriptForDestination(pubkey.GetID());
    if (HaveWatchOnly(script))
        RemoveWatchOnly(script);
    script = GetScriptForRawPubKey(pubkey);
    if (HaveWatchOnly(script))
        RemoveWatchOnly(script);

    // Special case, witness keys are never encrypted when added.
    if ((forAccount.IsHD() && forAccount.IsPoW2Witness() && nKeyChain == KEYCHAIN_WITNESS))
    {
        return CWalletDB(*dbw).WriteKeyOverride(pubkey, secret.GetPrivKey(), getUUIDAsString(forAccount.getUUID()), nKeyChain);
    }
    else if (!IsCrypted() || (forAccount.IsFixedKeyPool() && forAccount.IsPoW2Witness()))
    {
        return CWalletDB(*dbw).WriteKey(pubkey, secret.GetPrivKey(), mapKeyMetadata[pubkey.GetID()], getUUIDAsString(forAccount.getUUID()), nKeyChain);
    }
    else
    {
        std::vector<unsigned char> encryptedKeyOut;
        if (nKeyChain == KEYCHAIN_EXTERNAL)
        {
            if (!forAccount.externalKeyStore.GetKey(pubkey.GetID(), encryptedKeyOut))
            {
                return false;
            }
            return CWalletDB(*dbw).WriteCryptedKey(pubkey, encryptedKeyOut, mapKeyMetadata[pubkey.GetID()], getUUIDAsString(forAccount.getUUID()), nKeyChain);
        }
        else
        {
            if (!forAccount.internalKeyStore.GetKey(pubkey.GetID(), encryptedKeyOut))
            {
                return false;
            }
            return CWalletDB(*dbw).WriteCryptedKey(pubkey, encryptedKeyOut, mapKeyMetadata[pubkey.GetID()], getUUIDAsString(forAccount.getUUID()), nKeyChain);
        }
    }
    return true;
}

bool CWallet::LoadKeyMetadata(const CPubKey &pubkey, const CKeyMetadata &meta)
{
    AssertLockHeld(cs_wallet); // mapKeyMetadata
    if (meta.nCreateTime && (!nTimeFirstKey || meta.nCreateTime < int64_t(nTimeFirstKey)))
        nTimeFirstKey = meta.nCreateTime;

    mapKeyMetadata[pubkey.GetID()] = meta;
    return true;
}

bool CWallet::LoadCryptedKey(const CPubKey &vchPubKey, const std::vector<unsigned char> &vchCryptedSecret, const std::string& forAccount, int64_t nKeyChain)
{
    LOCK(cs_wallet);

    if (mapAccounts.find(getUUIDFromString(forAccount)) == mapAccounts.end())
        return false;
 
    return mapAccounts[getUUIDFromString(forAccount)]->AddCryptedKeyWithChain(vchPubKey, vchCryptedSecret, nKeyChain);
}

void CWallet::UpdateTimeFirstKey(int64_t nCreateTime)
{
    AssertLockHeld(cs_wallet);
    if (nCreateTime <= 1) {
        // Cannot determine birthday information, so set the wallet birthday to
        // the beginning of time.
        nTimeFirstKey = 1;
    } else if (!nTimeFirstKey || nCreateTime < int64_t(nTimeFirstKey)) {
        nTimeFirstKey = nCreateTime;
    }
}

bool CWallet::AddCScript(const CScript& redeemScript)
{
    LOCK(cs_wallet);

    //fixme: (FUT) (WATCH_ONLY)
    bool ret = false;
    for (const auto& [accountUUID, forAccount] : mapAccounts)
    {
        (unused) accountUUID;
        if (forAccount->AddCScript(redeemScript))
        {
            ret = true;
            break;
        }
    }

    if (!ret)
        return false;
    return CWalletDB(*dbw).WriteCScript(Hash160(redeemScript), redeemScript);
}

bool CWallet::LoadCScript(const CScript& redeemScript)
{
    LOCK(cs_wallet);

    /* A sanity check was added in pull #3843 to avoid adding redeemScripts
     * that never can be redeemed. However, old wallets may still contain
     * these. Do not add them to the wallet and warn. */
    if (redeemScript.size() > MAX_SCRIPT_ELEMENT_SIZE)
    {
        std::string strAddr = CNativeAddress(CScriptID(redeemScript)).ToString();
        LogPrintf("%s: Warning: This wallet contains a redeemScript of size %i which exceeds maximum size %i thus can never be redeemed. Do not use address %s.\n",
            __func__, redeemScript.size(), MAX_SCRIPT_ELEMENT_SIZE, strAddr);
        return true;
    }

    //fixme: (FUT) (WATCH_ONLY)
    bool ret = false;
    for (const auto& [accountUUID, forAccount] : mapAccounts)
    {
        (unused) accountUUID;
        ret = forAccount->AddCScript(redeemScript);
        if (ret == true)
            break;
    }
    return ret;
}

bool CWallet::AddWatchOnly(const CScript &dest, int64_t nCreateTime)
{
    AssertLockHeld(cs_wallet);

    bool ret = false;
    for (const auto& [accountUUID, forAccount] : mapAccounts)
    {
        (unused) accountUUID;
        //fixme: (FUT) (WATCH_ONLY) - nCreateTime should go here as well?
        if (forAccount->AddWatchOnly(dest))
            ret = true;
    }
    if (!ret)
        return false;

    const CKeyMetadata& meta = mapKeyMetadata[CScriptID(dest)];
    UpdateTimeFirstKey(meta.nCreateTime);

     NotifyWatchonlyChanged(true);
    return CWalletDB(*dbw).WriteWatchOnly(dest, meta);
}

bool CWallet::RemoveWatchOnly(const CScript &dest)
{
    AssertLockHeld(cs_wallet);
    //fixme: (FUT) (WATCH_ONLY)
    bool ret = true;
    for (const auto& [accountUUID, forAccount] : mapAccounts)
    {
        (unused) accountUUID;
        if (forAccount->RemoveWatchOnly(dest))
        {
            ret = true;
            break;
        }
    }
    if (!ret)
        return false;
    if (!HaveWatchOnly())
        NotifyWatchonlyChanged(false);
    if (!CWalletDB(*dbw).EraseWatchOnly(dest))
        return false;

    return true;
}

bool CWallet::LoadWatchOnly(const CScript &dest)
{
    AssertLockHeld(cs_wallet);

    //fixme: (FUT) (WATCH_ONLY)
    bool ret = false;
    for (const auto& [accountUUID, forAccount] : mapAccounts)
    {
        (unused) accountUUID;
        ret = forAccount->AddWatchOnly(dest);
        if (ret)
            break;
    }
    return ret;
}

bool CWallet::Unlock(const SecureString& strWalletPassphrase)
{
    CCrypter crypter;
    CKeyingMaterial _vMasterKey;

    {
        LOCK(cs_wallet);
        for(const MasterKeyMap::value_type& pMasterKey : mapMasterKeys)
        {
            if(!crypter.SetKeyFromPassphrase(strWalletPassphrase, pMasterKey.second.vchSalt, pMasterKey.second.nDeriveIterations, pMasterKey.second.nDerivationMethod))
            {
                LogPrintf("CWallet::Unlock - Failed to set key from phrase");
                return false;
            }
            if (!crypter.Decrypt(pMasterKey.second.vchCryptedKey, _vMasterKey))
                continue; // try another master key
            if (IsLocked()) {
                bool unlocked = UnlockWithMasterKey(_vMasterKey);
                if (unlocked)
                    fAutoLock = false;
                return unlocked;
            }
            else
                return true;
        }
    }
    LogPrintf("CWallet::Unlock - Failed to unlock any keys");
    return false;
}

bool CWallet::UnlockWithTimeout(const SecureString& strWalletPassphrase, int64_t lockTimeoutSeconds)
{
    bool ret = Unlock(strWalletPassphrase);
    
    // Schedule the lock `lockTimeoutSeconds` from now
    if (ret && schedulerForLock)
    {
        nRelockTime = GetTime() + lockTimeoutSeconds;
        
        // Keep the time we are requesting here as a value copy in the lambda in addition to being stored as a member variable...
        // Why? So that we can check that it hasn't changed before calling the lock
        // Otherwise this sequence of events is an issue:
        // 1. Unlock with timeout of 300.
        // 2. Call lock after 250 seconds.
        // 3. Unlock again with a timeout of 300.
        // 4. We are now expecting to be locked for 300 seconds but instead of 50 seconds the first lambda executes and kills our lock.
        int64_t nOriginalRelockTime = nRelockTime;
        schedulerForLock->schedule( [this, nOriginalRelockTime](){
            if (this->nRelockTime == nOriginalRelockTime)
            {
                Lock();
            }
        },  boost::chrono::system_clock::time_point(boost::chrono::seconds(nRelockTime)));
    }
    return ret;
}
    
bool CWallet::ChangeWalletPassphrase(const SecureString& strOldWalletPassphrase, const SecureString& strNewWalletPassphrase)
{
    bool fWasLocked = IsLocked();

    {
        LOCK(cs_wallet);
        Lock();

        CCrypter crypter;
        CKeyingMaterial _vMasterKey;
        for(MasterKeyMap::value_type& pMasterKey : mapMasterKeys)
        {
            if(!crypter.SetKeyFromPassphrase(strOldWalletPassphrase, pMasterKey.second.vchSalt, pMasterKey.second.nDeriveIterations, pMasterKey.second.nDerivationMethod))
                return false;
            std::vector<unsigned char> cryptedKeyCopy = pMasterKey.second.vchCryptedKey;
            if (!crypter.Decrypt(pMasterKey.second.vchCryptedKey, _vMasterKey))
                return false;
            
            //fixme: (PHASE5) Remove - Temporary fix for a very small limited subset of wallets that got exposed to a password change bug.
            //We can just leave this fix in for a while and then remove it.
            bool unlocked = false;
            unlocked = UnlockWithMasterKey(_vMasterKey);
            while (!unlocked)
            {
                if (unlocked)
                    break;
                if (cryptedKeyCopy.size() < 2)
                    break;
                cryptedKeyCopy = std::vector<unsigned char>(cryptedKeyCopy.begin()+1, cryptedKeyCopy.end());
                _vMasterKey.clear();
                if (crypter.Decrypt(cryptedKeyCopy, _vMasterKey))
                {
                    unlocked = UnlockWithMasterKey(_vMasterKey);
                }
            }
            if (unlocked)
            {
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
                if (!crypter.Encrypt(_vMasterKey, pMasterKey.second.vchCryptedKey))
                    return false;
                CWalletDB(*dbw).WriteMasterKey(pMasterKey.first, pMasterKey.second);
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
    CWalletDB walletdb(*dbw);
    walletdb.WriteBestBlock(loc);
}

bool CWallet::SetMinVersion(enum WalletFeature nVersion, CWalletDB* pwalletdbIn, bool fExplicit)
{
    LOCK(cs_wallet); // nWalletVersion
    if (nWalletVersion >= nVersion)
        return true;

    // when doing an explicit upgrade, if we pass the max version permitted, upgrade all the way
    if (fExplicit && nVersion > nWalletMaxVersion)
            nVersion = FEATURE_LATEST;

    nWalletVersion = nVersion;

    if (nVersion > nWalletMaxVersion)
        nWalletMaxVersion = nVersion;

    {
        CWalletDB* pwalletdb = pwalletdbIn ? pwalletdbIn : new CWalletDB(*dbw);
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
    // cannot downgrade below current version
    if (nWalletVersion > nVersion)
        return false;

    nWalletMaxVersion = nVersion;

    return true;
}

std::set<uint256> CWallet::GetConflicts(const uint256& txid) const
{
    std::set<uint256> result;
    AssertLockHeld(cs_wallet);

    std::map<uint256, CWalletTx>::const_iterator it = mapWallet.find(txid);
    if (it == mapWallet.end())
        return result;
    const CWalletTx& wtx = it->second;

    std::pair<TxSpends::const_iterator, TxSpends::const_iterator> range;

    for(const CTxIn& txin : wtx.tx->vin)
    {
        if (mapTxSpends.count(txin.GetPrevOut()) <= 1)
            continue;  // No conflict if zero or one spends
        range = mapTxSpends.equal_range(txin.GetPrevOut());
        for (TxSpends::const_iterator _it = range.first; _it != range.second; ++_it)
            result.insert(_it->second);
    }
    return result;
}

bool CWallet::HasWalletSpend(const uint256& txid) const
{
    AssertLockHeld(cs_wallet);
    auto iter = mapTxSpends.lower_bound(COutPoint(txid, 0));
    uint256 txHash;
    return (iter != mapTxSpends.end() && CWallet::GetTxHash(iter->first, txHash) && txHash == txid);
}

void CWallet::Flush(bool shutdown)
{
    dbw->Flush(shutdown);
}

bool CWallet::Verify()
{
    if (GetBoolArg("-disablewallet", DEFAULT_DISABLE_WALLET))
        return true;

    uiInterface.InitMessage(_("Verifying wallet(s)..."));

    for (const std::string& walletFile : gArgs.GetArgs("-wallet"))
    {
        if (boost::filesystem::path(walletFile).filename() != walletFile)
        {
            return InitError(_("-wallet parameter must only specify a filename (not a path)"));
        }
        else if (SanitizeString(walletFile, SAFE_CHARS_FILENAME) != walletFile)
        {
            return InitError(_("Invalid characters in -wallet filename"));
        }

        // Check file permissions by opening or creating file.
        {
            bool alreadyExists = fs::exists(GetDataDir() / walletFile);
            {
                std::fstream testPerms((GetDataDir() / walletFile).string(), std::ios::in | std::ios::out | std::ios::app);
                if (!testPerms.is_open())
                    return InitError(strprintf(_("%s may be read only or have permissions that deny access to the current user, please correct this and try again."), walletFile));
            }
            if (!alreadyExists)
            {
                fs::remove(GetDataDir() / walletFile);
            }
        }

        std::string strError;
        if (!CWalletDB::VerifyEnvironment(walletFile, GetDataDir().string(), strError))
        {
            return InitError(strError);
        }

        if (GetBoolArg("-salvagewallet", false))
        {
            // Recover readable keypairs:
            CWallet dummyWallet;
            std::string backup_filename;
            if (!CWalletDB::Recover(walletFile, (void *)&dummyWallet, CWalletDB::RecoverKeysOnlyFilter, backup_filename)) {
                return false;
            }
        }

        std::string strWarning;
        bool dbV = CWalletDB::VerifyDatabaseFile(walletFile, GetDataDir().string(), strWarning, strError);
        if (!strWarning.empty())
        {
            InitWarning(strWarning);
        }
        if (!dbV)
        {
            InitError(strError);
            return false;
        }
    }

    return true;
}

void CWallet::SyncMetaData(std::pair<TxSpends::iterator, TxSpends::iterator> range)
{
    // We want all the wallet transactions in range to have the same metadata as
    // the oldest (smallest nOrderPos).
    // So: find smallest nOrderPos:

    int nMinOrderPos = std::numeric_limits<int>::max();
    const CWalletTx* copyFrom = NULL;
    for (TxSpends::iterator it = range.first; it != range.second; ++it)
    {
        const uint256& hash = it->second;
        int n = mapWallet[hash].nOrderPos;
        if (n < nMinOrderPos)
        {
            nMinOrderPos = n;
            copyFrom = &mapWallet[hash];
        }
    }
    // Now copy data from copyFrom to rest:
    for (TxSpends::iterator it = range.first; it != range.second; ++it)
    {
        const uint256& hash = it->second;
        CWalletTx* copyTo = &mapWallet[hash];
        if (copyFrom == copyTo) continue;
        if (!copyFrom->IsEquivalentTo(*copyTo)) continue;
        copyTo->mapValue = copyFrom->mapValue;
        copyTo->vOrderForm = copyFrom->vOrderForm;
        // fTimeReceivedIsTxTime not copied on purpose
        // nTimeReceived not copied on purpose
        copyTo->nTimeSmart = copyFrom->nTimeSmart;
        copyTo->fFromMe = copyFrom->fFromMe;
        copyTo->strFromAccount = copyFrom->strFromAccount;
        // nOrderPos not copied on purpose
        // cached members not copied on purpose
    }
}

/**
 * Outpoint is spent if any non-conflicted transaction
 * spends it:
 */
bool CWallet::IsSpent(const COutPoint& outpoint) const
{
    std::pair<TxSpends::const_iterator, TxSpends::const_iterator> range;
    range = mapTxSpends.equal_range(outpoint);

    for (TxSpends::const_iterator it = range.first; it != range.second; ++it)
    {
        const uint256& wtxid = it->second;
        std::map<uint256, CWalletTx>::const_iterator mit = mapWallet.find(wtxid);
        if (mit != mapWallet.end()) {
            int depth = mit->second.GetDepthInMainChain();
            if ( depth > 0 )
            {
                return true; // Spent
            }
            else if ( (depth == 0 && !mit->second.isAbandoned()) )
            {
                if (mit->second.tx->vin.size() > 0)
                {
                    // Unconfirmed witness transactions never spend, otherwise we get strange display/balance issues with orphaned witness transactions.
                    const CWalletTx* prevtx = GetWalletTx(mit->second.tx->vin[0].GetPrevOut());
                    if (prevtx)
                    {
                        if (prevtx->tx->vout.size() == 0)
                            return true;

                        const auto& prevOut = prevtx->tx->vout[mit->second.tx->vin[0].GetPrevOut().n].output;
                        if ( prevOut.nType == ScriptLegacyOutput )
                        {
                            return true; // Spent
                        }
                        else if ( prevOut.nType != PoW2WitnessOutput )
                        {
                            return true; // Spent
                        }
                    }
                }
                else
                {
                    return true; // Spent
                }
            }
        }
    }
    return false;
}

void CWallet::AddToSpends(const COutPoint& outpoint, const uint256& wtxid)
{
    mapTxSpends.insert(std::pair(outpoint, wtxid));

    std::pair<TxSpends::iterator, TxSpends::iterator> range;
    range = mapTxSpends.equal_range(outpoint);
    SyncMetaData(range);
}


void CWallet::AddToSpends(const uint256& wtxid)
{
    assert(mapWallet.count(wtxid));
    CWalletTx& thisTx = mapWallet[wtxid];
    if (thisTx.IsCoinBase() && !thisTx.IsPoW2WitnessCoinBase()) // Coinbases don't spend anything!
        return;

    for(const CTxIn& txin : thisTx.tx->vin)
    {
        if (!txin.GetPrevOut().IsNull() || !thisTx.IsPoW2WitnessCoinBase())
        {
            AddToSpends(txin.GetPrevOut(), wtxid);
        }
    }
}

bool CWallet::EncryptWallet(const SecureString& strWalletPassphrase)
{
    if (IsCrypted())
        return false;

    CKeyingMaterial _vMasterKey;

    _vMasterKey.resize(WALLET_CRYPTO_KEY_SIZE);
    GetStrongRandBytes(&_vMasterKey[0], WALLET_CRYPTO_KEY_SIZE);

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
    if (!crypter.Encrypt(_vMasterKey, kMasterKey.vchCryptedKey))
        return false;

    {
        LOCK(cs_wallet);
        mapMasterKeys[++nMasterKeyMaxID] = kMasterKey;
        assert(!pwalletdbEncryption);
        pwalletdbEncryption = new CWalletDB(*dbw);
        if (!pwalletdbEncryption->TxnBegin()) {
            delete pwalletdbEncryption;
            pwalletdbEncryption = NULL;
            return false;
        }
        pwalletdbEncryption->WriteMasterKey(nMasterKeyMaxID, kMasterKey);

        for (const auto& [seedUUID, forSeed] : mapSeeds)
        {
            (unused) seedUUID;
            if (!forSeed->Encrypt(_vMasterKey))
            {
                pwalletdbEncryption->TxnAbort();
                delete pwalletdbEncryption;
                // We now probably have half of our keys encrypted in memory, and half not...
                // die and let the user reload the unencrypted wallet.
                assert(false);
            }
            pwalletdbEncryption->WriteHDSeed(*forSeed);
        }

        for (const auto& [accountUUID, forAccount] : mapAccounts)
        {
            if (!forAccount->Encrypt(_vMasterKey))
            {
                pwalletdbEncryption->TxnAbort();
                delete pwalletdbEncryption;

                // We now probably have half of our keys encrypted in memory, and half not...
                // die and let the user reload the unencrypted wallet.
                assert(false);
            }
            pwalletdbEncryption->WriteAccount(getUUIDAsString(accountUUID), forAccount);
        }

        // Encryption was introduced in version 0.4.0
        SetMinVersion(FEATURE_WALLETCRYPT, pwalletdbEncryption, true);

        if (!pwalletdbEncryption->TxnCommit()) {
            delete pwalletdbEncryption;
            // We now have keys encrypted in memory, but not on disk...
            // die to avoid confusion and let the user reload the unencrypted wallet.
            assert(false);
        }

        delete pwalletdbEncryption;
        pwalletdbEncryption = NULL;

        for (const auto& [accountUUID, forAccount] : mapAccounts)
        {
            (unused) accountUUID;
            forAccount->Lock();
        }
        for (const auto& [seedUUID, forSeed] : mapSeeds)
        {
            (unused) seedUUID;
            forSeed->Lock();
        }
        Unlock(strWalletPassphrase);

        //fixme: (FUT) (ACCOUNTS) (HD) Gulden HD - What to do here? We can't really throw the entire seed away...
        /*
        // if we are using HD, replace the HD master key (seed) with a new one
        if (IsHDEnabled()) {
            if (!SetHDMasterKey(GenerateNewHDMasterKey())) {
                return false;
            }
        }
        */

        NewKeyPool();
        Lock();

        // Need to completely rewrite the wallet file; if we don't, bdb might keep
        // bits of the unencrypted private key in slack space in the database file.
        dbw->Rewrite();

    }
    for (const auto& [accountUUID, forAccount] : mapAccounts)
    {
        (unused) accountUUID;
        forAccount->internalKeyStore.NotifyStatusChanged(&forAccount->internalKeyStore);
        forAccount->externalKeyStore.NotifyStatusChanged(&forAccount->externalKeyStore);
    }

    return true;
}

DBErrors CWallet::ReorderTransactions()
{
    LOCK(cs_wallet);
    CWalletDB walletdb(*dbw);

    // Old wallets didn't have any defined order for transactions
    // Probably a bad idea to change the output of this

    // First: get all CWalletTx and CAccountingEntry into a sorted-by-time multimap.
    typedef std::pair<CWalletTx*, CAccountingEntry*> TxPair;
    typedef std::multimap<int64_t, TxPair > TxItems;
    TxItems txByTime;

    for (std::map<uint256, CWalletTx>::iterator it = mapWallet.begin(); it != mapWallet.end(); ++it)
    {
        CWalletTx* wtx = &((*it).second);
        txByTime.insert(std::pair(wtx->nTimeReceived, TxPair(wtx, (CAccountingEntry*)0)));
    }
    std::list<CAccountingEntry> acentries;
    walletdb.ListAccountCreditDebit("", acentries);
    for(CAccountingEntry& entry : acentries)
    {
        txByTime.insert(std::pair(entry.nTime, TxPair((CWalletTx*)0, &entry)));
    }

    nOrderPosNext = 0;
    std::vector<int64_t> nOrderPosOffsets;
    for (TxItems::iterator it = txByTime.begin(); it != txByTime.end(); ++it)
    {
        CWalletTx *const pwtx = (*it).second.first;
        CAccountingEntry *const pacentry = (*it).second.second;
        int64_t& nOrderPos = (pwtx != 0) ? pwtx->nOrderPos : pacentry->nOrderPos;

        if (nOrderPos == -1)
        {
            nOrderPos = nOrderPosNext++;
            nOrderPosOffsets.push_back(nOrderPos);

            if (pwtx)
            {
                if (!walletdb.WriteTx(*pwtx))
                    return DB_LOAD_FAIL;
            }
            else
                if (!walletdb.WriteAccountingEntry(pacentry->nEntryNo, *pacentry))
                    return DB_LOAD_FAIL;
        }
        else
        {
            int64_t nOrderPosOff = 0;
            for(const int64_t& nOffsetStart : nOrderPosOffsets)
            {
                if (nOrderPos >= nOffsetStart)
                    ++nOrderPosOff;
            }
            nOrderPos += nOrderPosOff;
            nOrderPosNext = std::max(nOrderPosNext, nOrderPos + 1);

            if (!nOrderPosOff)
                continue;

            // Since we're changing the order, write it back
            if (pwtx)
            {
                if (!walletdb.WriteTx(*pwtx))
                    return DB_LOAD_FAIL;
            }
            else
                if (!walletdb.WriteAccountingEntry(pacentry->nEntryNo, *pacentry))
                    return DB_LOAD_FAIL;
        }
    }
    walletdb.WriteOrderPosNext(nOrderPosNext);

    return DB_LOAD_OK;
}

int64_t CWallet::IncOrderPosNext(CWalletDB *pwalletdb)
{
    AssertLockHeld(cs_wallet); // nOrderPosNext
    int64_t nRet = nOrderPosNext++;
    if (pwalletdb) {
        pwalletdb->WriteOrderPosNext(nOrderPosNext);
    } else {
        CWalletDB(*dbw).WriteOrderPosNext(nOrderPosNext);
    }
    return nRet;
}

bool CWallet::AccountMove(std::string strFrom, std::string strTo, CAmount nAmount, std::string strComment)
{
    //fixme: (FUT) (REFACTOR) (ACCOUNTS)
    //This should never be called.
    assert(0);
    return true;
}

bool CWallet::GetAccountPubkey(CPubKey &pubKey, std::string strAccount, bool bForceNew)
{
    //fixme: (FUT) (REFACTOR) (ACCOUNTS)
    //This should never be called.
    assert(0);
    return true;
}

void CWallet::MarkDirty()
{
    {
        LOCK(cs_wallet);
        for(PAIRTYPE(const uint256, CWalletTx)& item : mapWallet)
            item.second.MarkDirty();
    }
}

bool CWallet::MarkReplaced(const uint256& originalHash, const uint256& newHash)
{
    LOCK(cs_wallet);

    auto mi = mapWallet.find(originalHash);

    // There is a bug if MarkReplaced is not called on an existing wallet transaction.
    assert(mi != mapWallet.end());

    CWalletTx& wtx = (*mi).second;

    wtx.MarkDirty();

    // Ensure for now that we're not overwriting data
    if (wtx.mapValue.count("replaced_by_txid") != 0)
        return false;

    wtx.mapValue["replaced_by_txid"] = newHash.ToString();

    CWalletDB walletdb(*dbw, "r+");

    bool success = true;
    if (!walletdb.WriteTx(wtx)) {
        LogPrintf("%s: Updating walletdb tx %s failed", __func__, wtx.GetHash().ToString());
        success = false;
    }

    NotifyTransactionChanged(this, originalHash, CT_UPDATED, false);

    return success;
}

bool CWallet::AddToWallet(const CWalletTx& wtxIn, bool fFlushOnClose, bool fSelfComitted)
{
    LOCK(cs_wallet);

    CWalletDB walletdb(*dbw, "r+", fFlushOnClose);

    uint256 hash = wtxIn.GetHash();
    // Refuse to add a null hash to the wallet, as this flags the wallet as corrupted on restart
    if (hash.IsNull())
        return false;
    maintainHashMap(wtxIn, hash);

    // Inserts only if not already there, returns tx inserted or tx found
    std::pair<std::map<uint256, CWalletTx>::iterator, bool> ret = mapWallet.insert(std::pair(hash, wtxIn));
    CWalletTx& wtx = (*ret.first).second;
    if (wtx.strFromAccount.empty())
    {
        LOCK(cs_wallet);
        for (const auto& [accountUUID, forAccount] : mapAccounts)
        {
            if (forAccount->HaveWalletTx(wtx))
            {
                wtx.strFromAccount = getUUIDAsString(accountUUID);
            }
        }
    }
    wtx.BindWallet(this);
    bool fInsertedNew = ret.second;
    if (fInsertedNew)
    {
        wtx.nTimeReceived = GetAdjustedTime();
        wtx.nOrderPos = IncOrderPosNext(&walletdb);
        wtxOrdered.insert(std::pair(wtx.nOrderPos, TxPair(&wtx, (CAccountingEntry*)0)));
        wtx.nTimeSmart = ComputeTimeSmart(wtx);
        AddToSpends(hash);
    }

    bool fUpdated = false;
    if (!fInsertedNew)
    {
        // Merge
        if (!wtxIn.hashUnset() && wtxIn.hashBlock != wtx.hashBlock)
        {
            wtx.hashBlock = wtxIn.hashBlock;
            wtx.nHeight = wtxIn.nHeight;
            wtx.nBlockTime = wtxIn.nBlockTime;
            fUpdated = true;
        }
        // If no longer abandoned, update
        if (wtxIn.hashBlock.IsNull() && wtx.isAbandoned())
        {
            wtx.hashBlock = wtxIn.hashBlock;
            wtx.nHeight = wtxIn.nHeight;
            wtx.nBlockTime = wtxIn.nBlockTime;
            fUpdated = true;
        }
        // If height changed, update
        if (wtxIn.nHeight != -1 && (wtxIn.nHeight != wtx.nHeight))
        {
            wtx.nHeight = wtxIn.nHeight;
            fUpdated = true;
        }
        // If block time changed, update
        if (wtxIn.nBlockTime != 0 && (wtxIn.nBlockTime != wtx.nBlockTime))
        {
            wtx.nBlockTime = wtxIn.nBlockTime;
            fUpdated = true;
        }
        // If index changed, update
        if (wtxIn.nIndex != -1 && (wtxIn.nIndex != wtx.nIndex))
        {
            wtx.nIndex = wtxIn.nIndex;
            fUpdated = true;
        }
        if (wtxIn.fFromMe && wtxIn.fFromMe != wtx.fFromMe)
        {
            wtx.fFromMe = wtxIn.fFromMe;
            fUpdated = true;
        }
    }

    //// debug print
    LogPrintf("AddToWallet %s  %s%s\n", wtxIn.GetHash().ToString(), (fInsertedNew ? "new" : ""), (fUpdated ? "update" : ""));

    // Write to disk
    if (fInsertedNew || fUpdated)
        if (!walletdb.WriteTx(wtx))
            return false;

    // Break debit/credit balance caches:
    wtx.MarkDirty();

    // Notify UI of new or updated transaction
    NotifyTransactionChanged(this, hash, fInsertedNew ? CT_NEW : CT_UPDATED, fSelfComitted);

    // notify an external script when a wallet transaction comes in or is updated
    std::string strCmd = GetArg("-walletnotify", "");

    if ( !strCmd.empty())
    {
        boost::replace_all(strCmd, "%s", wtxIn.GetHash().GetHex());
        boost::thread t(runCommand, strCmd); // thread runs free
    }

    return true;
}

void CWallet::maintainHashMap(const CWalletTx& wtxIn, uint256& hash)
{
    //if (mapWallet.size() > 0 && mapWalletHash.empty())
    if (wtxIn.nHeight > 0)
    {
        for (unsigned int i = 0; i < wtxIn.tx->vout.size(); i++)
        {
            mapWalletHash[COutPoint(wtxIn.nHeight, wtxIn.nIndex, i).getBucketHash()] = hash;
        }
    }
}

bool CWallet::LoadToWallet(const CWalletTx& wtxIn)
{
    uint256 hash = wtxIn.GetHash();

    maintainHashMap(wtxIn, hash);
    mapWallet[hash] = wtxIn;
    
    CWalletTx& wtx = mapWallet[hash];
    wtx.BindWallet(this);
    wtxOrdered.insert(std::pair(wtx.nOrderPos, TxPair(&wtx, nullptr)));
    AddToSpends(hash);

    return true;
}

void CWallet::HandleTransactionsLoaded()
{
    for  (const auto& [hash, wtx] : mapWallet)
    {
        for (const CTxIn& txin : wtx.tx->vin)
        {
            const CWalletTx* prevtx = GetWalletTx(txin.GetPrevOut());
            if (prevtx)
            {
                if (prevtx->nIndex == -1 && !prevtx->hashUnset())
                {
                    MarkConflicted(prevtx->hashBlock, wtx.GetHash());
                }
            }
        }
    }
}

void CWallet::ClearCacheForTransaction(const uint256& hash)
{
    auto& wtx = mapWallet[hash];
    // Invalidate all caches for transaction as they will need to be recalculated.
    // Otherwise we get incorrect wallet balance displays.
    wtx.MarkDirty();
}

/**
 * Add a transaction to the wallet, or update it.  pIndex and posInBlock should
 * be set when the transaction was known to be included in a block.  When
 * pIndex == NULL, then wallet state is not updated in AddToWallet, but
 * notifications happen and cached balances are marked dirty.
 *
 * If fUpdate is true, existing transactions will be updated.
 * TODO: One exception to this is that the abandoned state is cleared under the
 * assumption that any further notification of a transaction that was considered
 * abandoned is an indication that it is not safe to be considered abandoned.
 * Abandoned state should probably be more carefully tracked via different
 * posInBlock signals or by checking mempool presence when necessary.
 */
bool CWallet::AddToWalletIfInvolvingMe(const CTransactionRef& ptx, const CBlockIndex* pIndex, int posInBlock, bool fUpdate)
{
    const CTransaction& tx = *ptx;
    {
        AssertLockHeld(cs_wallet);

        std::vector<std::pair<uint256, uint256>> markReplacements;
        if (pIndex != NULL)
        {
            for(const CTxIn& txin : tx.vin)
            {
                std::pair<TxSpends::const_iterator, TxSpends::const_iterator> range = mapTxSpends.equal_range(txin.GetPrevOut());
                while (range.first != range.second)
                {
                    if (range.first->second != tx.GetHash())
                    {
                        uint256 txHash;
                        CWallet::GetTxHash(range.first->first, txHash);
                        LogPrintf("Transaction %s (in block %s) conflicts with wallet transaction %s (both spend %s:%i)\n", tx.GetHash().ToString(), pIndex->GetBlockHashPoW2().ToString(), range.first->second.ToString(), txHash.ToString(), range.first->first.n);
                        MarkConflicted(pIndex->GetBlockHashPoW2(), range.first->second);
                    }
                    range.first++;
                }
            }
        }

        bool fExisted = mapWallet.count(tx.GetHash()) != 0;
        // Wipe cache from all transactions that possibly involve this one.
        if (fExisted)
            ClearCacheForTransaction(tx.GetHash());
        for(const auto& txin : tx.vin)
        {
            const CWalletTx* wtx = GetWalletTx(txin.GetPrevOut());
            if (wtx)
                ClearCacheForTransaction(wtx->GetHash());
        }
        if (fExisted && !fUpdate) return false;
        if (fExisted || IsMine(tx) || IsFromMe(tx))
        {
            CWalletTx wtx(this, ptx);

            // Get merkle branch if transaction was found in a block
            if (pIndex != NULL)
                wtx.SetMerkleBranch(pIndex, posInBlock);

            RemoveAddressFromKeypoolIfIsMine(tx, pIndex ? pIndex->nTime : 0);
            for(const auto& txin : wtx.tx->vin)
            {
                RemoveAddressFromKeypoolIfIsMine(txin, pIndex ? pIndex->nTime : 0);
            }

            bool ret = AddToWallet(wtx, false);
            //NB! We delibritely do this from here and not inside AddToWallet, because we sometimes add transactions that are not ours via AddToWallet (e.g. see lower down in this function where we add the incoming transactions)
            GetMainSignals().WalletTransactionAdded(this, wtx);

            // Update account state
            //fixme: (PHASE5) - More efficient way to do this?
            // If this is the first unconfirmed witness transaction a witness account is receiving then update the state to "pending" in the UI.
            // If it has a confirm then update to "locked"
            if (pactiveWallet)
            {
                for (const auto& txOut : wtx.tx->vout)
                {
                    if (txOut.GetType() == CTxOutType::PoW2WitnessOutput)
                    {
                        CAccount* potentialWitnessAccount = pactiveWallet->FindBestWitnessAccountForTransaction(txOut);
                        if (potentialWitnessAccount && potentialWitnessAccount->m_Type == AccountType::PoW2Witness)
                        {
                            if (potentialWitnessAccount->GetWarningState() == AccountStatus::WitnessEmpty || potentialWitnessAccount->GetWarningState() == AccountStatus::WitnessPending)
                            {
                                potentialWitnessAccount->SetWarningState(pIndex ? AccountStatus::Default : AccountStatus::WitnessPending);
                                static_cast<const CExtWallet*>(pactiveWallet)->NotifyAccountWarningChanged(pactiveWallet, potentialWitnessAccount);
                                // Set active so that witness account gains the focus immediately after funding.
                                if (!pIndex) {
                                    CWalletDB walletdb(*pactiveWallet->dbw);
                                    pactiveWallet->setActiveAccount(walletdb, potentialWitnessAccount);
                                }
                            }
                        }
                    }
                }
            }

            for (const auto& [wtx, newTx] : markReplacements)
            {
                MarkReplaced(wtx, newTx);
            }

            //fixme: (Future) This is slow and should be made faster.
            //Add all incoming transactions to the wallet as well (even though they aren't from us necessarily) - so that we can always get 'incoming' address details.
            //Is there maybe a better way to do this?
            //
            //Note the below has large speed implications - if we do need this it would be better to maybe just serialise the "from address" as part of CWalletTx and not keep additional transactions (that aren't ours) around.
            //GetTransaction() is expensive and locks on mutexes that intefere with other parts of the code.
            for(const auto& txin : tx.vin)
            {
                uint256 txHash;
                if (CWallet::GetTxHash(txin.GetPrevOut(), txHash))
                {
                    bool fExistedIncoming = mapWallet.count(txHash) != 0;
                    if (!fExistedIncoming)
                    {
                        uint256 hashBlock = uint256();
                        if (GetTransaction(txHash, wtx.tx, Params(), hashBlock, true))
                        {
                            AddToWallet(wtx, false);
                        }
                    }
                }
            }
            return ret;
        }
    }
    return false;
}

bool CWallet::TransactionCanBeAbandoned(const uint256& hashTx) const
{
    LOCK2(cs_main, cs_wallet);
    const CWalletTx* wtx = GetWalletTx(hashTx);
    return wtx && !wtx->isAbandoned() && wtx->GetDepthInMainChain() <= 0 && !wtx->InMempool();
}

bool CWallet::AbandonTransaction(const uint256& hashTx)
{
    LOCK2(cs_main, cs_wallet);

    CWalletDB walletdb(*dbw, "r+");

    std::set<uint256> todo;
    std::set<uint256> done;

    // Can't mark abandoned if confirmed or in mempool
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
        // If the orig tx was not in block, none of its spends can be
        assert(currentconfirm <= 0);
        // if (currentconfirm < 0) {Tx and spends are already conflicted, no need to abandon}
        if (currentconfirm == 0 && !wtx.isAbandoned()) {
            // If the orig tx was not in block/mempool, none of its spends can be in mempool
            assert(!wtx.InMempool());
            wtx.nIndex = -1;
            wtx.setAbandoned();
            wtx.MarkDirty();
            walletdb.WriteTx(wtx);
            NotifyTransactionChanged(this, wtx.GetHash(), CT_UPDATED, false);
            // Iterate over all its outputs, and mark transactions in the wallet that spend them abandoned too
            TxSpends::const_iterator iter = mapTxSpends.lower_bound(COutPoint(hashTx, 0));
            while (iter != mapTxSpends.end()) {
                uint256 txHash;
                if (!CWallet::GetTxHash(iter->first, txHash) || txHash != now)
                    break;
                if (!done.count(iter->second)) {
                    todo.insert(iter->second);
                }
                iter++;
            }
            // If a transaction changes 'conflicted' state, that changes the balance
            // available of the outputs it spends. So force those to be recomputed
            for(const CTxIn& txin : wtx.tx->vin)
            {
                CWalletTx* prev = GetWalletTx(txin.GetPrevOut());
                if (prev)
                    prev->MarkDirty();
            }
        }
    }

    return true;
}

static void forceRestart()
{
    // Immediately disable what we can so this is cleaner.
    if (g_connman)
    {
        g_connman->SetNetworkActive(false);
    }
    fullyEraseDatadirOnShutdown=true;
    
    // Event loop will exit after current HTTP requests have been handled, so
    // this reply will get back to the client.
    AppLifecycleManager::gApp->shutdown();
}

void CWallet::MarkConflicted(const uint256& hashBlock, const uint256& hashTx)
{
    LOCK2(cs_main, cs_wallet);

    int conflictconfirms = 0;
    if (mapBlockIndex.count(hashBlock))
    {
        CBlockIndex* pindex = mapBlockIndex[hashBlock];
        if (partialChain.Contains(pindex) || chainActive.Contains(pindex))
        {
            conflictconfirms = -(std::max(partialChain.Height(), chainActive.Height()) - pindex->nHeight + 1);
        }
    }
    // If number of conflict confirms cannot be determined, this means
    // that the block is still unknown or not yet part of the main chain,
    // for example when loading the wallet during a reindex. Do nothing in that
    // case.
    if (conflictconfirms >= 0)
        return;

    // Do not flush the wallet here for performance reasons
    CWalletDB walletdb(*dbw, "r+", false);

    std::set<uint256> todo;
    std::set<uint256> done;

    todo.insert(hashTx);

    while (!todo.empty())
    {
        uint256 now = *todo.begin();
        todo.erase(now);
        done.insert(now);
        auto it = mapWallet.find(now);
        if (fSPV)
        {
            if (it == mapWallet.end())
            {
                forceRestart();
                return;
            }
        }
        else
        {
            assert(it != mapWallet.end());
        }
        CWalletTx& wtx = it->second;
        int currentconfirm = wtx.GetDepthInMainChain();
        if (conflictconfirms < currentconfirm || (conflictconfirms == currentconfirm && wtx.nIndex >= 0))
        {
            // Block is 'more conflicted' than current confirm; update. Or tx not marked as conflicted yet.
            // Mark transaction as conflicted with this block.
            wtx.nIndex = -1;
            wtx.hashBlock = hashBlock;
            wtx.MarkDirty();
            walletdb.WriteTx(wtx);
            // Iterate over all its outputs, and mark transactions in the wallet that spend them conflicted too
            TxSpends::const_iterator iter = mapTxSpends.lower_bound(COutPoint(now, 0));
            while (iter != mapTxSpends.end())
            {
                uint256 txHash;
                if (!CWallet::GetTxHash(iter->first, txHash) || txHash != now)
                    break;
                 if (!done.count(iter->second))
                 {
                     todo.insert(iter->second);
                 }
                 iter++;
            }
            // If a transaction changes 'conflicted' state, that changes the balance
            // available of the outputs it spends. So force those to be recomputed
            for (const CTxIn& txin : wtx.tx->vin)
            {
                CWalletTx* prev = GetWalletTx(txin.GetPrevOut());
                if (prev)
                    prev->MarkDirty();
            }
        }
    }
}

void CWallet::SyncTransaction(const CTransactionRef& ptx, const CBlockIndex *pindex, int posInBlock) {
    const CTransaction& tx = *ptx;

    if (!AddToWalletIfInvolvingMe(ptx, pindex, posInBlock, true))
        return; // Not one of ours

    // If a transaction changes 'conflicted' state, that changes the balance
    // available of the outputs it spends. So force those to be
    // recomputed, also:
    for(const CTxIn& txin : tx.vin)
    {
        CWalletTx* prev = GetWalletTx(txin.GetPrevOut());
        if (prev)
            prev->MarkDirty();
    }
}

void CWallet::TransactionAddedToMempool(const CTransactionRef& ptx) {
    LOCK2(cs_main, cs_wallet);
    SyncTransaction(ptx);
}

void CWallet::TransactionDeletedFromMempool(const uint256& hash, MemPoolRemovalReason reason)
{
    LOCK2(cs_main, cs_wallet);

    if (!isFullSyncMode() && IsPartialSyncActive() && MemPoolRemovalReason::EXPIRY == reason) {
        const auto& it = mapWallet.find(hash);
        if (it != mapWallet.end()) {
            if (TransactionCanBeAbandoned(hash))
                AbandonTransaction(hash);
        }
    }
}

void CWallet::BlockConnected(const std::shared_ptr<const CBlock>& pblock, const CBlockIndex *pindex, const std::vector<CTransactionRef>& vtxConflicted) {
    LOCK2(cs_main, cs_wallet);
    // TODO: Temporarily ensure that mempool removals are notified before
    // connected transactions.  This shouldn't matter, but the abandoned
    // state of transactions in our wallet is currently cleared when we
    // receive another notification and there is a race condition where
    // notification of a connected conflict might cause an outside process
    // to abandon a transaction and then have it inadvertently cleared by
    // the notification that the conflicted transaction was evicted.

    for (const CTransactionRef& ptx : vtxConflicted) {
        SyncTransaction(ptx);
    }
    for (size_t i = 0; i < pblock->vtx.size(); i++) {
        SyncTransaction(pblock->vtx[i], pindex, i);
    }

    static int lastReportedDepthChangeHeight = 0;
    CChain& chain = IsPartialSyncActive() && partialChain.Height() > chainActive.Height() ? partialChain : chainActive;
    if (chain.Height() != lastReportedDepthChangeHeight) {
        lastReportedDepthChangeHeight = chain.Height();
        for(const auto& [hash, wtx] : mapWallet)
        {
            int nDepth = wtx.GetDepthInMainChain();
            if (nDepth > 0 && nDepth < 10)
            {
                NotifyTransactionDepthChanged(this, hash);
            }
        }
    }
}

void CWallet::BlockDisconnected(const std::shared_ptr<const CBlock>& pblock) {
    LOCK2(cs_main, cs_wallet);

    for (const CTransactionRef& ptx : pblock->vtx) {
        SyncTransaction(ptx);
    }
}

void CWallet::PruningConflictingBlock(const uint256& orphanBlockHash)
{
    LOCK2(cs_main, cs_wallet);

    if (!mapBlockIndex.count(orphanBlockHash))
        return;

    CBlockIndex* pIndex = mapBlockIndex[orphanBlockHash];

    if (pIndex->nHeight < partialChain.HeightOffset() || pIndex->nHeight >partialChain.Height())
        return;

    CBlock block;
    if (!ReadBlockFromDisk(block, pIndex, Params())) {
        LogPrintf("%s: Error reading block height=%d hash=%s\n", __func__, pIndex->nHeight, pIndex->GetBlockHashPoW2().ToString().c_str());
        return;
    }

    for (const CTransactionRef& ptx : block.vtx) {
        MarkConflicted(partialChain[pIndex->nHeight]->GetBlockHashPoW2(), ptx->GetHash());
    }
}

bool CWallet::IsChange(const CTxOut& txout) const
{
    LOCK(cs_wallet);

    if (::IsMine(*this, txout))
    {
        CTxDestination address;
        if (!ExtractDestination(txout, address))
            return true;

        for (const auto& [accountUUID, forAccount] : mapAccounts)
        {
            (unused) accountUUID;
            if (::IsMine(forAccount->internalKeyStore, address) > ISMINE_NO)
                return true;
        }
    }

    return false;
}

void CWallet::ReacceptWalletTransactions()
{
    // If transactions aren't being broadcasted, don't let them into local mempool either
    if (!fBroadcastTransactions)
        return;
    LOCK2(cs_main, cs_wallet);
    std::map<int64_t, CWalletTx*> mapSorted;

    // Sort pending wallet transactions based on their initial wallet insertion order
    for(PAIRTYPE(const uint256, CWalletTx)& item : mapWallet)
    {
        const uint256& wtxid = item.first;
        CWalletTx& wtx = item.second;
        assert(wtx.GetHash() == wtxid);

        int nDepth = wtx.GetDepthInMainChain();

        if (!wtx.IsCoinBase() && (nDepth == 0 && !wtx.isAbandoned())) {
            mapSorted.insert(std::pair(wtx.nOrderPos, &wtx));
        }
    }

    // Try to add wallet transactions to memory pool
    for(PAIRTYPE(const int64_t, CWalletTx*)& item : mapSorted)
    {
        CWalletTx& wtx = *(item.second);

        LOCK(mempool.cs);
        CValidationState state;
        wtx.AcceptToMemoryPool(maxTxFee, state);
    }
}

bool CWalletTx::RelayWalletTransaction(CConnman* connman)
{
    assert(pwallet->GetBroadcastTransactions());
    if (!IsCoinBase() && !isAbandoned() && GetDepthInMainChain() == 0)
    {
        CValidationState state;
        /* GetDepthInMainChain already catches known conflicts. */
        if (InMempool() || AcceptToMemoryPool(maxTxFee, state)) {
            LogPrintf("Relaying wtx %s\n", GetHash().ToString());
            if (connman) {
                CInv inv(MSG_TX, GetHash());
                connman->ForEachNode([&inv](CNode* pnode)
                {
                    pnode->PushInventory(inv);
                });
                return true;
            }
        }
    }
    return false;
}

std::set<uint256> CWalletTx::GetConflicts() const
{
    std::set<uint256> result;
    if (pwallet != NULL)
    {
        uint256 myHash = GetHash();
        result = pwallet->GetConflicts(myHash);
        result.erase(myHash);
    }
    return result;
}

bool CWalletTx::InMempool() const
{
    LOCK(mempool.cs);
    return mempool.exists(GetHash());
}

bool CWalletTx::IsTrusted() const
{
    // Quick answer in most cases
    if (!CheckFinalTx(*this, IsPartialSyncActive() ? partialChain : chainActive))
        return false;
    int nDepth = GetDepthInMainChain();
    if (nDepth >= 1)
        return true;
    if (nDepth < 0)
        return false;
    if (!bSpendZeroConfChange || !IsFromMe(ISMINE_ALL)) // using wtx's cached debit
        return false;

    // Don't trust unconfirmed transactions from us unless they are in the mempool.
    if (!InMempool())
        return false;

    // Trusted if all inputs are from us and are in the mempool:
    for(const CTxIn& txin : tx->vin)
    {
        // Transactions not sent by us: not trusted
        const CWalletTx* parent = pwallet->GetWalletTx(txin.GetPrevOut());
        if (parent == NULL || parent->tx->vout.size() == 0)
            return false;
        const CTxOut& parentOut = parent->tx->vout[txin.GetPrevOut().n];
        if (pwallet->IsMine(parentOut) != ISMINE_SPENDABLE)
            return false;
    }
    return true;
}

bool CWalletTx::IsEquivalentTo(const CWalletTx& _tx) const
{
        CMutableTransaction tx1 = *this->tx;
        CMutableTransaction tx2 = *_tx.tx;
        for (auto& txin : tx1.vin) txin.scriptSig = CScript();
        for (auto& txin : tx2.vin) txin.scriptSig = CScript();
        return CTransaction(tx1) == CTransaction(tx2);
}

std::vector<uint256> CWallet::ResendWalletTransactionsBefore(int64_t nTime, CConnman* connman)
{
    std::vector<uint256> result;

    LOCK(cs_wallet);
    // Sort them in chronological order
    std::multimap<unsigned int, CWalletTx*> mapSorted;
    for(PAIRTYPE(const uint256, CWalletTx)& item : mapWallet)
    {
        CWalletTx& wtx = item.second;
        // Don't rebroadcast if newer than nTime:
        if (wtx.nTimeReceived > nTime)
            continue;
        mapSorted.insert(std::pair(wtx.nTimeReceived, &wtx));
    }
    for(PAIRTYPE(const unsigned int, CWalletTx*)& item : mapSorted)
    {
        CWalletTx& wtx = *item.second;
        if (wtx.RelayWalletTransaction(connman))
            result.push_back(wtx.GetHash());
    }
    return result;
}

void CWallet::ResendWalletTransactions(int64_t nBestBlockTime, CConnman* connman)
{
    // Do this infrequently and randomly to avoid giving away
    // that these are our transactions.
    if (GetTime() < nNextResend || !fBroadcastTransactions)
        return;
    bool fFirst = (nNextResend == 0);
    nNextResend = GetTime() + GetRand(30 * 60);
    if (fFirst)
        return;

    // Only do it if there's been a new block since last time
    if (nBestBlockTime < nLastResend)
        return;
    nLastResend = GetTime();

    // Rebroadcast unconfirmed txes older than 5 minutes before the last
    // block was found:
    std::vector<uint256> relayed = ResendWalletTransactionsBefore(nBestBlockTime-5*60, connman);
    if (!relayed.empty())
        LogPrintf("%s: rebroadcast %u unconfirmed transactions\n", __func__, relayed.size());
}

/** @} */ // end of mapWallet




/** @defgroup Actions
 *
 * @{
 */

extern bool IsMine(const CKeyStore* forAccount, const CWalletTx& tx);


void CWallet::AvailableCoins(CAccount* forAccount, std::vector<COutput> &vCoins, bool fOnlySafe, const CCoinControl *coinControl, const CAmount &nMinimumAmount, const CAmount &nMaximumAmount, const CAmount &nMinimumSumAmount, const uint64_t &nMaximumCount, const int &nMinDepth, const int &nMaxDepth) const
{
    std::vector<CKeyStore*> accountsToTry;
    accountsToTry.push_back(forAccount);
    return AvailableCoins(accountsToTry, vCoins, fOnlySafe, coinControl, nMinimumAmount, nMaximumAmount, nMinimumSumAmount, nMaximumCount, nMinDepth, nMaxDepth);
}

void CWallet::AvailableCoins(std::vector<CKeyStore*>& accountsToTry, std::vector<COutput> &vCoins, bool fOnlySafe, const CCoinControl *coinControl, const CAmount &nMinimumAmount, const CAmount &nMaximumAmount, const CAmount &nMinimumSumAmount, const uint64_t &nMaximumCount, const int &nMinDepth, const int &nMaxDepth) const
{
    vCoins.clear();

    {
        LOCK2(cs_main, cs_wallet);

        CAmount nTotal = 0;

        for (std::map<uint256, CWalletTx>::const_iterator it = mapWallet.begin(); it != mapWallet.end(); ++it)
        {
            const uint256& wtxid = it->first;
            const CWalletTx* pcoin = &(*it).second;

            bool isMineAny=false;
            for (const auto& forAccount : accountsToTry)
            {
                if (::IsMine(forAccount, *pcoin))
                {
                    isMineAny = true;
                }
            }
            if (!isMineAny)
                continue;

            if (!CheckFinalTx(*pcoin, IsPartialSyncActive() ? partialChain : chainActive))
                continue;

            if (pcoin->IsCoinBase() && pcoin->GetBlocksToMaturity() > 0 && pcoin->nTimeSmart != Params().GenesisBlock().nTime)
                continue;

            int nDepth = pcoin->GetDepthInMainChain();
            if (nDepth < 0)
                continue;

            // We should not consider coins which aren't at least in our mempool
            // It's possible for these to be conflicted via ancestors which we may never be able to detect
            if (nDepth == 0 && !pcoin->InMempool())
                continue;

            bool safeTx = pcoin->IsTrusted();

            // We should not consider coins from transactions that are replacing
            // other transactions.
            //
            // Example: There is a transaction A which is replaced by bumpfee
            // transaction B. In this case, we want to prevent creation of
            // a transaction B' which spends an output of B.
            //
            // Reason: If transaction A were initially confirmed, transactions B
            // and B' would no longer be valid, so the user would have to create
            // a new transaction C to replace B'. However, in the case of a
            // one-block reorg, transactions B' and C might BOTH be accepted,
            // when the user only wanted one of them. Specifically, there could
            // be a 1-block reorg away from the chain where transactions A and C
            // were accepted to another chain where B, B', and C were all
            // accepted.
            if (nDepth == 0 && pcoin->mapValue.count("replaces_txid")) {
                safeTx = false;
            }

            // Similarly, we should not consider coins from transactions that
            // have been replaced. In the example above, we would want to prevent
            // creation of a transaction A' spending an output of A, because if
            // transaction B were initially confirmed, conflicting with A and
            // A', we wouldn't want to the user to create a transaction D
            // intending to replace A', but potentially resulting in a scenario
            // where A, A', and D could all be accepted (instead of just B and
            // D, or just A and A' like the user would want).
            if (nDepth == 0 && pcoin->mapValue.count("replaced_by_txid")) {
                safeTx = false;
            }

            if (fOnlySafe && !safeTx) {
                continue;
            }

            if (nDepth < nMinDepth || nDepth > nMaxDepth)
                continue;

            for (unsigned int i = 0; i < pcoin->tx->vout.size(); i++) {
                // If coin is a witness transaction output that is currently locked (current tip) then the coin is not available for spending.
                if (IsPoW2WitnessLocked(pcoin->tx->vout[i], (uint64_t)chainActive.Tip()->nHeight))
                    continue;

                if (pcoin->tx->vout[i].nValue < nMinimumAmount || pcoin->tx->vout[i].nValue > nMaximumAmount)
                    continue;

                if (coinControl && coinControl->HasSelected() && !coinControl->fAllowOtherInputs && !coinControl->IsSelected(COutPoint((*it).first, i)))
                    continue;

                if (IsLockedCoin((*it).first, i))
                    continue;

                if (IsSpent(COutPoint(wtxid, i)) || IsSpent(COutPoint(pcoin->nHeight, pcoin->nIndex, i)))
                    continue;

                isminetype mine = ISMINE_NO;
                for (const auto& forAccount : accountsToTry)
                {
                    isminetype temp = ::IsMine(*forAccount, pcoin->tx->vout[i]);
                    if (mine < temp)
                        mine = temp;
                }

                if (mine == ISMINE_NO) {
                    continue;
                }

                bool fSpendableIn = ((mine & ISMINE_SPENDABLE) != ISMINE_NO) || (coinControl && coinControl->fAllowWatchOnly && (mine & ISMINE_WATCH_SOLVABLE) != ISMINE_NO);
                bool fSolvableIn = (mine & (ISMINE_SPENDABLE | ISMINE_WATCH_SOLVABLE)) != ISMINE_NO;

                vCoins.push_back(COutput(pcoin, i, nDepth, fSpendableIn, fSolvableIn, safeTx));

                // Checks the sum amount of all UTXO's.
                if (nMinimumSumAmount != MAX_MONEY) {
                    nTotal += pcoin->tx->vout[i].nValue;

                    if (nTotal >= nMinimumSumAmount) {
                        return;
                    }
                }

                // Checks the maximum number of UTXO's.
                if (nMaximumCount > 0 && vCoins.size() >= nMaximumCount) {
                    return;
                }
            }
        }
    }
}

std::map<CTxDestination, std::vector<COutput>> CWallet::ListCoins(CAccount* forAccount) const
{
    // TODO: Add AssertLockHeld(cs_wallet) here.
    //
    // Because the return value from this function contains pointers to
    // CWalletTx objects, callers to this function really should acquire the
    // cs_wallet lock before calling it. However, the current caller doesn't
    // acquire this lock yet. There was an attempt to add the missing lock in
    // https://github.com/bitcoin/bitcoin/pull/10340, but that change has been
    // postponed until after https://github.com/bitcoin/bitcoin/pull/10244 to
    // avoid adding some extra complexity to the Qt code.

    std::map<CTxDestination, std::vector<COutput>> result;

    std::vector<COutput> availableCoins;
    AvailableCoins(forAccount, availableCoins);

    LOCK2(cs_main, cs_wallet);
    for (auto& coin : availableCoins)
    {
        CTxDestination address;
        if (coin.fSpendable && ExtractDestination(FindNonChangeParentOutput(*coin.tx->tx, coin.i), address))
        {
            result[address].emplace_back(std::move(coin));
        }
    }

    std::vector<COutPoint> lockedCoins;
    ListLockedCoins(lockedCoins);
    for (const auto& output : lockedCoins)
    {
        CWalletTx* wtx = GetWalletTx(output);
        if (wtx)
        {
            int depth = wtx->GetDepthInMainChain();
            if (depth >= 0 && output.n < wtx->tx->vout.size() && IsMine(wtx->tx->vout[output.n]) == ISMINE_SPENDABLE)
            {
                CTxDestination address;
                if (ExtractDestination(FindNonChangeParentOutput(*wtx->tx, output.n), address))
                {
                    bool isSpendable = true;
                    bool isSolvable = true;
                    bool isSafe = false;
                    result[address].emplace_back(wtx, output.n, depth, isSpendable, isSolvable, isSafe);
                }
            }
        }
    }

    return result;
}

const CTxOut& CWallet::FindNonChangeParentOutput(const CTransaction& tx, int output) const
{
    const CTransaction* ptx = &tx;
    int n = output;
    while (IsChange(ptx->vout[n]) && ptx->vin.size() > 0)
    {
        const COutPoint& prevout = ptx->vin[0].GetPrevOut();
        CWalletTx* prev = GetWalletTx(prevout);
        if (!prev || prev->tx->vout.size() <= prevout.n || !IsMine(prev->tx->vout[prevout.n]))
        {
            break;
        }
        ptx = prev->tx.get();
        n = prevout.n;
    }
    return ptx->vout[n];
}

static void ApproximateBestSubset(const std::vector<CInputCoin>& vValue, const CAmount& nTotalLower, const CAmount& nTargetValue,
                                  std::vector<char>& vfBest, CAmount& nBest, int iterations = 1000)
{
    std::vector<char> vfIncluded;

    vfBest.assign(vValue.size(), true);
    nBest = nTotalLower;

    FastRandomContext insecure_rand;

    for (int nRep = 0; nRep < iterations && nBest != nTargetValue; nRep++)
    {
        vfIncluded.assign(vValue.size(), false);
        CAmount nTotal = 0;
        bool fReachedTarget = false;
        for (int nPass = 0; nPass < 2 && !fReachedTarget; nPass++)
        {
            for (unsigned int i = 0; i < vValue.size(); i++)
            {
                //The solver here uses a randomized algorithm,
                //the randomness serves no real security purpose but is just
                //needed to prevent degenerate behavior and it is important
                //that the rng is fast. We do not use a constant random sequence,
                //because there may be some privacy improvement by making
                //the selection random.
                if (nPass == 0 ? insecure_rand.randbool() : !vfIncluded[i])
                {
                    nTotal += vValue[i].txout.nValue;
                    vfIncluded[i] = true;
                    if (nTotal >= nTargetValue)
                    {
                        fReachedTarget = true;
                        if (nTotal < nBest)
                        {
                            nBest = nTotal;
                            vfBest = vfIncluded;
                        }
                        nTotal -= vValue[i].txout.nValue;
                        vfIncluded[i] = false;
                    }
                }
            }
        }
    }
}

bool CWallet::SelectCoinsMinConf(const CAmount& nTargetValue, const int nConfMine, const int nConfTheirs, const uint64_t nMaxAncestors, std::vector<COutput> vCoins,
                                 std::set<CInputCoin>& setCoinsRet, CAmount& nValueRet, bool allowIndexBased) const
{
    setCoinsRet.clear();
    nValueRet = 0;

    // List of values less than target
    boost::optional<CInputCoin> coinLowestLarger;
    std::vector<CInputCoin> vValue;
    CAmount nTotalLower = 0;

    std::random_device rng;
    std::mt19937 urng(rng());
    std::shuffle(vCoins.begin(), vCoins.end(), urng);

    for(const COutput &output : vCoins)
    {
        if (!output.fSpendable)
            continue;

        const CWalletTx *pcoin = output.tx;

        if (output.nDepth < (pcoin->IsFromMe(ISMINE_ALL) ? nConfMine : nConfTheirs))
            continue;

        if (!mempool.TransactionWithinChainLimit(pcoin->GetHash(), nMaxAncestors))
            continue;

        int i = output.i;

        CInputCoin coin = CInputCoin(pcoin, i, allowIndexBased);

        if (coin.txout.nValue == nTargetValue)
        {
            setCoinsRet.insert(coin);
            nValueRet += coin.txout.nValue;
            return true;
        }
        else if (coin.txout.nValue < nTargetValue + MIN_CHANGE)
        {
            vValue.push_back(coin);
            nTotalLower += coin.txout.nValue;
        }
        else if (!coinLowestLarger || coin.txout.nValue < coinLowestLarger->txout.nValue)
        {
            coinLowestLarger = coin;
        }
    }

    if (nTotalLower == nTargetValue)
    {
        for (const auto& input : vValue)
        {
            setCoinsRet.insert(input);
            nValueRet += input.txout.nValue;
        }
        return true;
    }

    if (nTotalLower < nTargetValue)
    {
        if (!coinLowestLarger)
            return false;
        setCoinsRet.insert(coinLowestLarger.get());
        nValueRet += coinLowestLarger->txout.nValue;
        return true;
    }

    // Solve subset sum by stochastic approximation
    std::sort(vValue.begin(), vValue.end(), CompareValueOnly());
    std::reverse(vValue.begin(), vValue.end());
    std::vector<char> vfBest;
    CAmount nBest;

    ApproximateBestSubset(vValue, nTotalLower, nTargetValue, vfBest, nBest);
    if (nBest != nTargetValue && nTotalLower >= nTargetValue + MIN_CHANGE)
    {
        ApproximateBestSubset(vValue, nTotalLower, nTargetValue + MIN_CHANGE, vfBest, nBest);
    }

    // If we have a bigger coin and (either the stochastic approximation didn't find a good solution,
    //                                   or the next bigger coin is closer), return the bigger coin
    if (coinLowestLarger &&((nBest != nTargetValue && nBest < nTargetValue + MIN_CHANGE) || coinLowestLarger->txout.nValue <= nBest))
    {
        setCoinsRet.insert(coinLowestLarger.get());
        nValueRet += coinLowestLarger->txout.nValue;
    }
    else
    {
        for (unsigned int i = 0; i < vValue.size(); i++)
        {
            if (vfBest[i])
            {
                setCoinsRet.insert(vValue[i]);
                nValueRet += vValue[i].txout.nValue;
            }
        }

        if (LogAcceptCategory(BCLog::SELECTCOINS))
        {
            LogPrint(BCLog::SELECTCOINS, "SelectCoins() best subset: ");
            for (unsigned int i = 0; i < vValue.size(); i++)
            {
                if (vfBest[i])
                {
                    LogPrint(BCLog::SELECTCOINS, "%s ", FormatMoney(vValue[i].txout.nValue));
                }
            }
            LogPrint(BCLog::SELECTCOINS, "total %s\n", FormatMoney(nBest));
        }
    }

    return true;
}

bool CWallet::SelectCoins(bool allowIndexBased, const std::vector<COutput>& vAvailableCoins, const CAmount& nTargetValue, std::set<CInputCoin>& setCoinsRet, CAmount& nValueRet, const CCoinControl* coinControl) const
{
    std::vector<COutput> vCoins(vAvailableCoins);

    // coin control -> return all selected outputs (we want all selected to go into the transaction for sure)
    if (coinControl && coinControl->HasSelected() && !coinControl->fAllowOtherInputs)
    {
        for(const COutput& out : vCoins)
        {
            if (!out.fSpendable)
                 continue;
            nValueRet += out.tx->tx->vout[out.i].nValue;
            setCoinsRet.insert(CInputCoin(out.tx, out.i, allowIndexBased));
        }
        return (nValueRet >= nTargetValue);
    }

    // calculate value from preset inputs and store them
    std::set<CInputCoin> setPresetCoins;
    CAmount nValueFromPresetInputs = 0;

    std::vector<COutPoint> vPresetInputs;
    if (coinControl)
        coinControl->ListSelected(vPresetInputs);
    for(const COutPoint& outpoint : vPresetInputs)
    {
        CWalletTx* pcoin = GetWalletTx(outpoint);
        if (pcoin)
        {
            // Clearly invalid input, fail
            if (pcoin->tx->vout.size() <= outpoint.n)
                return false;
            nValueFromPresetInputs += pcoin->tx->vout[outpoint.n].nValue;
            setPresetCoins.insert(CInputCoin(pcoin, outpoint.n, allowIndexBased));
        } else
            return false; // TODO: Allow non-wallet inputs
    }

    // remove preset inputs from vCoins
    for (std::vector<COutput>::iterator it = vCoins.begin(); it != vCoins.end() && coinControl && coinControl->HasSelected();)
    {
        // Check in both forms (index based and not) to eliminate the same selection occuring twice
        if (setPresetCoins.count(CInputCoin(it->tx, it->i, allowIndexBased)) 
            || (allowIndexBased && setPresetCoins.count(CInputCoin(it->tx, it->i, false))))
        {
            it = vCoins.erase(it);
        }
        else
        {
            ++it;
        }
    }

    size_t nMaxChainLength = std::min(GetArg("-limitancestorcount", DEFAULT_ANCESTOR_LIMIT), GetArg("-limitdescendantcount", DEFAULT_DESCENDANT_LIMIT));
    bool fRejectLongChains = GetBoolArg("-walletrejectlongchains", DEFAULT_WALLET_REJECT_LONG_CHAINS);

    bool res = nTargetValue <= nValueFromPresetInputs ||
        SelectCoinsMinConf(nTargetValue - nValueFromPresetInputs, 1, 6, 0, vCoins, setCoinsRet, nValueRet, allowIndexBased) ||
        SelectCoinsMinConf(nTargetValue - nValueFromPresetInputs, 1, 1, 0, vCoins, setCoinsRet, nValueRet, allowIndexBased) ||
        (bSpendZeroConfChange && SelectCoinsMinConf(nTargetValue - nValueFromPresetInputs, 0, 1, 2, vCoins, setCoinsRet, nValueRet, allowIndexBased)) ||
        (bSpendZeroConfChange && SelectCoinsMinConf(nTargetValue - nValueFromPresetInputs, 0, 1, std::min((size_t)4, nMaxChainLength/3), vCoins, setCoinsRet, nValueRet, allowIndexBased)) ||
        (bSpendZeroConfChange && SelectCoinsMinConf(nTargetValue - nValueFromPresetInputs, 0, 1, nMaxChainLength/2, vCoins, setCoinsRet, nValueRet, allowIndexBased)) ||
        (bSpendZeroConfChange && SelectCoinsMinConf(nTargetValue - nValueFromPresetInputs, 0, 1, nMaxChainLength, vCoins, setCoinsRet, nValueRet, allowIndexBased)) ||
        (bSpendZeroConfChange && !fRejectLongChains && SelectCoinsMinConf(nTargetValue - nValueFromPresetInputs, 0, 1, std::numeric_limits<uint64_t>::max(), vCoins, setCoinsRet, nValueRet, allowIndexBased));

    // because SelectCoinsMinConf clears the setCoinsRet, we now add the possible inputs to the coinset
    setCoinsRet.insert(setPresetCoins.begin(), setPresetCoins.end());

    // add preset inputs to the total value selected
    nValueRet += nValueFromPresetInputs;

    return res;
}

void CWallet::ListAccountCreditDebit(const std::string& strAccount, std::list<CAccountingEntry>& entries) {
    CWalletDB walletdb(*dbw);
    return walletdb.ListAccountCreditDebit(strAccount, entries);
}

bool CWallet::AddAccountingEntry(const CAccountingEntry& acentry)
{
    CWalletDB walletdb(*dbw);

    return AddAccountingEntry(acentry, &walletdb);
}

bool CWallet::AddAccountingEntry(const CAccountingEntry& acentry, CWalletDB *pwalletdb)
{
    if (!pwalletdb->WriteAccountingEntry(++nAccountingEntryNumber, acentry)) {
        return false;
    }

    laccentries.push_back(acentry);
    CAccountingEntry & entry = laccentries.back();
    wtxOrdered.insert(std::pair(entry.nOrderPos, TxPair((CWalletTx*)0, &entry)));

    return true;
}

CAmount CWallet::GetRequiredFee(unsigned int nTxBytes)
{
    return std::max(minTxFee.GetFee(nTxBytes), ::minRelayTxFee.GetFee(nTxBytes));
}

CAmount CWallet::GetMinimumFee(unsigned int nTxBytes, unsigned int nConfirmTarget, const CTxMemPool& pool, const CBlockPolicyEstimator& estimator, bool ignoreGlobalPayTxFee)
{
    // payTxFee is the user-set global for desired feerate
    CAmount nFeeNeeded = payTxFee.GetFee(nTxBytes);
    // User didn't set: use -txconfirmtarget to estimate...
    if (nFeeNeeded == 0 || ignoreGlobalPayTxFee) {
        int estimateFoundTarget = nConfirmTarget;
        nFeeNeeded = estimator.estimateSmartFee(nConfirmTarget, &estimateFoundTarget, pool).GetFee(nTxBytes);
        // ... unless we don't have enough mempool data for estimatefee, then use fallbackFee
        if (nFeeNeeded == 0)
            nFeeNeeded = fallbackFee.GetFee(nTxBytes);
    }
    // prevent user from paying a fee below minRelayTxFee or minTxFee
    nFeeNeeded = std::max(nFeeNeeded, GetRequiredFee(nTxBytes));
    // But always obey the maximum
    if (nFeeNeeded > maxTxFee)
        nFeeNeeded = maxTxFee;
    return nFeeNeeded;
}

DBErrors CWallet::ZapSelectTx(CWalletDB& walletdb, std::vector<uint256>& vHashIn, std::vector<uint256>& vHashOut)
{
    AssertLockHeld(cs_wallet); // mapWallet
    //vchDefaultKey = CPubKey();//GULDEN - no default key.
    DBErrors nZapSelectTxRet = walletdb.ZapSelectTx(vHashIn, vHashOut);
    for (uint256 hash : vHashOut)
        mapWallet.erase(hash);

    if (nZapSelectTxRet == DB_NEED_REWRITE)
    {
        if (dbw->Rewrite("\x04pool"))
        {
            //fixme: (FUT) (ACCOUNTS) (MED)
            for (const auto& [accountUUID, forAccount] : mapAccounts)
            {
                (unused) accountUUID;
                forAccount->setKeyPoolInternal.clear();
                forAccount->setKeyPoolExternal.clear();
            }
            // Note: can't top-up keypool here, because wallet is locked.
            // User will be prompted to unlock wallet the next operation
            // that requires a new key.
        }
    }

    if (nZapSelectTxRet != DB_LOAD_OK)
        return nZapSelectTxRet;

    MarkDirty();

    return DB_LOAD_OK;

}

DBErrors CWallet::ZapWalletTx(std::vector<CWalletTx>& vWtx)
{
    //vchDefaultKey = CPubKey();//GULDEN - no default key.
    DBErrors nZapWalletTxRet = CWalletDB(*dbw,"cr+").ZapWalletTx(vWtx);
    if (nZapWalletTxRet == DB_NEED_REWRITE)
    {
        if (dbw->Rewrite("\x04pool"))
        {
            LOCK(cs_wallet);
            //fixme: (FUT) (ACCOUNTS) (MED)
            for (const auto& [accountUUID, forAccount] : mapAccounts)
            {
                (unused) accountUUID;
                forAccount->setKeyPoolInternal.clear();
                forAccount->setKeyPoolExternal.clear();
            }
            // Note: can't top-up keypool here, because wallet is locked.
            // User will be prompted to unlock wallet the next operation
            // that requires a new key.
        }
    }

    if (nZapWalletTxRet != DB_LOAD_OK)
        return nZapWalletTxRet;

    return DB_LOAD_OK;
}


bool CWallet::SetAddressBook(const std::string& address, const std::string& strName, const std::string& strRecipientDescription, const std::string& strPurpose)
{
    bool fUpdated = false;
    {
        LOCK(cs_wallet); // mapAddressBook
        std::map<std::string, CAddressBookData>::iterator mi = mapAddressBook.find(address);
        fUpdated = mi != mapAddressBook.end();
        auto& data = mapAddressBook[address];
        data.name = strName;
        // Only update if explicitely requested to do so
        if (!strPurpose.empty())
            data.purpose = strPurpose;
        if (!strRecipientDescription.empty())
            data.description = strName;
    }
    NotifyAddressBookChanged(this, address, strName, ::IsMine(*this, CNativeAddress(address).Get()) != ISMINE_NO, strPurpose, (fUpdated ? CT_UPDATED : CT_NEW) );
    
    if (!strPurpose.empty() && !CWalletDB(*dbw).WriteRecipientPurpose(address, strPurpose))
        return false;
    if (!strRecipientDescription.empty()  && !CWalletDB(*dbw).WriteRecipientDescription(address, strRecipientDescription)) 
        return false;
    return CWalletDB(*dbw).WriteRecipientName(address, strName);
}

bool CWallet::DelAddressBook(const std::string& address)
{
    {
        LOCK(cs_wallet); // mapAddressBook

        // Delete destdata tuples associated with address
        for(const PAIRTYPE(std::string, std::string) &item : mapAddressBook[address].destdata)
        {
            CWalletDB(*dbw).EraseDestData(address, item.first);
        }
        mapAddressBook.erase(address);
    }

    NotifyAddressBookChanged(this, address, "", ::IsMine(*this, CNativeAddress(address).Get()) != ISMINE_NO, "", CT_DELETED);

    // if address is a valid string encoding of a Gulden address use that for delete key,
    // else it is most likely an IBAN address and then use that directly as key
    CNativeAddress nativeAddress;
    std::string deleteKey = nativeAddress.SetString(address) ? nativeAddress.ToString() : address;
    CWalletDB(*dbw).EraseRecipientPurpose(deleteKey);
    return CWalletDB(*dbw).EraseRecipientName(deleteKey);
}

std::set< std::set<CTxDestination> > CWallet::GetAddressGroupings()
{
    AssertLockHeld(cs_wallet); // mapWallet
    std::set< std::set<CTxDestination> > groupings;
    std::set<CTxDestination> grouping;

    for (const auto& walletEntry : mapWallet)
    {
        const CWalletTx *pcoin = &walletEntry.second;

        if (pcoin->tx->vin.size() > 0)
        {
            bool any_mine = false;
            // group all input addresses with each other
            for(CTxIn txin : pcoin->tx->vin)
            {
                CTxDestination address;
                if(!IsMine(txin)) /* If this input isn't mine, ignore it */
                    continue;
                const CWalletTx* prev = GetWalletTx(txin.GetPrevOut());
                if(!prev || !ExtractDestination(prev->tx->vout[txin.GetPrevOut().n], address))
                    continue;
                grouping.insert(address);
                any_mine = true;
            }

            // group change with input addresses
            if (any_mine)
            {
               for(CTxOut txout : pcoin->tx->vout)
                   if (IsChange(txout))
                   {
                       CTxDestination txoutAddr;
                       if(!ExtractDestination(txout, txoutAddr))
                           continue;
                       grouping.insert(txoutAddr);
                   }
            }
            if (grouping.size() > 0)
            {
                groupings.insert(grouping);
                grouping.clear();
            }
        }

        // group lone addrs by themselves
        for (const auto& txout : pcoin->tx->vout)
            if (IsMine(txout))
            {
                CTxDestination address;
                if(!ExtractDestination(txout, address))
                    continue;
                grouping.insert(address);
                groupings.insert(grouping);
                grouping.clear();
            }
    }

    std::set< std::set<CTxDestination>* > uniqueGroupings; // a set of pointers to groups of addresses
    std::map< CTxDestination, std::set<CTxDestination>* > setmap;  // map addresses to the unique group containing it
    for(std::set<CTxDestination> _grouping : groupings)
    {
        // make a set of all the groups hit by this new group
        std::set< std::set<CTxDestination>* > hits;
        std::map< CTxDestination, std::set<CTxDestination>* >::iterator it;
        for(CTxDestination address : _grouping)
            if ((it = setmap.find(address)) != setmap.end())
                hits.insert((*it).second);

        // merge all hit groups into a new single group and delete old groups
        std::set<CTxDestination>* merged = new std::set<CTxDestination>(_grouping);
        for(std::set<CTxDestination>* hit : hits)
        {
            merged->insert(hit->begin(), hit->end());
            uniqueGroupings.erase(hit);
            delete hit;
        }
        uniqueGroupings.insert(merged);

        // update setmap
        for(CTxDestination element : *merged)
            setmap[element] = merged;
    }

    std::set< std::set<CTxDestination> > ret;
    for(std::set<CTxDestination>* uniqueGrouping : uniqueGroupings)
    {
        ret.insert(*uniqueGrouping);
        delete uniqueGrouping;
    }

    return ret;
}



std::set<CTxDestination> CWallet::GetAccountAddresses(const std::string& strAccount) const
{
    LOCK(cs_wallet);
    std::set<CTxDestination> result;
    for(const PAIRTYPE(std::string, CAddressBookData)& item : mapAddressBook)
    {
        const std::string& address = item.first;
        const std::string& strName = item.second.name;
        if (strName == strAccount)
            result.insert(CNativeAddress(address).Get());
    }
    return result;
}

bool CReserveKeyOrScript::GetReservedKey(CPubKey& pubkey)
{
    if (scriptOnly())
        return false;

    if (pwallet && nIndex == -1)
    {
        CKeyPool keypool;
        pwallet->ReserveKeyFromKeyPool(nIndex, keypool, account, nKeyChain);
        if (nIndex != -1)
        {
            vchPubKey = keypool.vchPubKey;
        }
        else
        {
            return false;
        }
    }
    assert(vchPubKey.IsValid());
    pubkey = vchPubKey;
    return true;
}

bool CReserveKeyOrScript::GetReservedKeyID(CKeyID &pubKeyID_)
{
    if (scriptOnly())
        return false;
    
    if (pubKeyID.IsNull())
        return false;
    
    pubKeyID_ = pubKeyID;
    return true;
}

void CReserveKeyOrScript::KeepKey()
{
    if (scriptOnly())
        return;
    if (nIndex != -1)
        pwallet->KeepKey(nIndex);
    nIndex = -1;
    vchPubKey = CPubKey();
}

void CReserveKeyOrScript::ReturnKey()
{
    if (scriptOnly())
        return;
    if (nIndex != -1)
        pwallet->ReturnKey(nIndex, account, nKeyChain);
    nIndex = -1;
    vchPubKey = CPubKey();
}

void CWallet::GetAllReserveKeys(std::set<CKeyID>& setAddress) const
{
    setAddress.clear();

    CWalletDB walletdb(*dbw);

    LOCK2(cs_main, cs_wallet);
    for (const auto& [accountUUID, forAccount] : mapAccounts)
    {
        (unused) accountUUID;
        for (auto keyChain : { KEYCHAIN_EXTERNAL, KEYCHAIN_CHANGE })
        {
            const auto& keyPool = ( keyChain == KEYCHAIN_EXTERNAL ? forAccount->setKeyPoolExternal : forAccount->setKeyPoolInternal );
            for (const int64_t& id : keyPool)
            {
                CKeyPool keypoolentry;
                if (!walletdb.ReadPool(id, keypoolentry))
                throw std::runtime_error(std::string(__func__) + ": read failed");
                assert(keypoolentry.vchPubKey.IsValid());
                CKeyID keyID = keypoolentry.vchPubKey.GetID();
                if (!HaveKey(keyID))
                    throw std::runtime_error(std::string(__func__) + ": unknown key in key pool");
                setAddress.insert(keyID);
            }
        }
    }
}

void CWallet::GetScriptForMining(std::shared_ptr<CReserveKeyOrScript> &script, CAccount* forAccount)
{
    //fixme: (PHASE5) - Clean this all up.
    std::shared_ptr<CReserveKeyOrScript> rKey;
    if (forAccount)
    {
        rKey = std::make_shared<CReserveKeyOrScript>(this, forAccount, KEYCHAIN_EXTERNAL);
    }
    else
    {
        rKey = std::make_shared<CReserveKeyOrScript>(this, activeAccount, KEYCHAIN_EXTERNAL);
    }
    CPubKey pubkey;
    if (!rKey->GetReservedKey(pubkey))
    {
        CAlert::Notify("Failed to obtain reward key for mining account.", true, true);
        return;
    }
    
    CKeyID pubKeyID;
    if (IsPow2Phase4Active(chainActive.Tip()) && rKey->GetReservedKeyID(pubKeyID))
    {
        script = std::make_shared<CReserveKeyOrScript>();
    }
    else
    {
        script = rKey;
        script->reserveScript = CScript() << ToByteVector(pubkey) << OP_CHECKSIG;
    }
}

void CWallet::GetScriptForWitnessing(std::shared_ptr<CReserveKeyOrScript> &script, CAccount* forAccount)
{
    std::shared_ptr<CReserveKeyOrScript> rKey;

    // forAccount should never be null
    if (!forAccount)
    {
        CAlert::Notify("Failed to obtain reward key for witness account, invalid account.", true, true);
        return;
    }

    // Always pay to KEYCHAIN_SPENDING instead of KEYCHAIN_WITNESS if possible - that way if witness key is stolen our funds are safe.
    // If an explicit script has been set via RPC then use that, otherwise we just make a script from a key
    if (forAccount->hasNonCompoundRewardScript())
    {
        rKey = std::make_shared<CReserveKeyOrScript>(nullptr, nullptr, KEYCHAIN_SPENDING);
        rKey->reserveScript = forAccount->getNonCompoundRewardScript();
    }
    else
    {
        rKey = std::make_shared<CReserveKeyOrScript>(this, forAccount, KEYCHAIN_SPENDING);

        CPubKey pubkey;
        if (!rKey->GetReservedKey(pubkey))
        {
            CAlert::Notify("Failed to obtain reward key for witness account.", true, true);
            return;
        }

        rKey->reserveScript = CScript() << ToByteVector(pubkey) << OP_CHECKSIG;
    }
    script = rKey;
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

void CWallet::ListLockedCoins(std::vector<COutPoint>& vOutpts) const
{
    AssertLockHeld(cs_wallet); // setLockedCoins
    for (std::set<COutPoint>::iterator it = setLockedCoins.begin();
         it != setLockedCoins.end(); it++) {
        COutPoint outpt = (*it);
        vOutpts.push_back(outpt);
    }
}


void CWallet::CompareWalletAgainstUTXO(int& nMismatchFound, int& nOrphansFound, int64_t& nBalanceInQuestion, bool attemptRepair)
{
    nMismatchFound = 0;
    nBalanceInQuestion = 0;
    nOrphansFound = 0;

    LOCK2(cs_main, cs_wallet);
    std::vector<CWalletTx*> allWalletCoins;
    allWalletCoins.reserve(mapWallet.size());
    for(auto& it : mapWallet)
    {
        allWalletCoins.push_back(&it.second);
    }

    CCoinsViewCache viewNew(pcoinsTip);
    std::map<COutPoint, Coin> allUTXOCoins;
    viewNew.GetAllCoins(allUTXOCoins);
    
    //Iterate all UTXO entries and check if they are ours
    //If they are check they are in the wallet
    //If they aren't this is an issue
    for(const auto& [utxoOutpoint, utxoCoin] : allUTXOCoins)
    {
        if (utxoOutpoint.isHash)
        {
            if(IsMine(utxoCoin.out) >= ISMINE_SPENDABLE)
            {
                if (mapWallet.find(utxoOutpoint.getTransactionHash()) == mapWallet.end())
                {
                    LogPrintf("CompareWalletAgainstUTXO: Found a utxo that is ours but that isn't in wallet %s %s[%d]\n", FormatMoney(utxoCoin.out.nValue).c_str(), utxoOutpoint.getTransactionHash().ToString().c_str(), utxoOutpoint.n);
                    nMismatchFound++;
                    nBalanceInQuestion += utxoCoin.out.nValue;
                }
            }
        }
    }
    //Iterate all wallet entries
    //Ensure that they are in the UTXO if they are unspent
    //Ensure that they aren't in the UTXO if they are spent
    //If either of the above is untrue this is an issue
    for(CWalletTx* walletCoin : allWalletCoins)
    {
        uint256 hash = walletCoin->GetHash();
        uint64_t n;

        if(walletCoin->IsCoinBase() && (walletCoin->GetDepthInMainChain() < 0))
        {
           nOrphansFound++;
           printf("CompareWalletAgainstUTXO: Found orphaned generation tx [%s]\n", hash.ToString().c_str());
        }
        else
        {
            for(n = 0; n < walletCoin->tx->vout.size(); n++)
            {
                if(IsMine(walletCoin->tx->vout[n]) >= ISMINE_SPENDABLE)
                {
                    COutPoint walletCoinOutpoint(walletCoin->tx->GetHash(), n);
                    COutPoint walletCoinOutpointIndex(walletCoin->nHeight, walletCoin->nIndex, n);
                    bool outputIsInUTXO = (allUTXOCoins.find(walletCoinOutpoint) != allUTXOCoins.end()) || (allUTXOCoins.find(walletCoinOutpointIndex) != allUTXOCoins.end());
                    bool outputSpentInWallet = pactiveWallet->IsSpent(walletCoinOutpoint) || IsSpent(walletCoinOutpointIndex);
                    if(outputSpentInWallet && outputIsInUTXO)
                    {
                        LogPrintf("CompareWalletAgainstUTXO: Found wallet-spent coins that are in the utxo and therefore shouldn't be spent %s %s[%d]\n", FormatMoney(walletCoin->tx->vout[n].nValue).c_str(), hash.ToString().c_str(), n);
                        nMismatchFound++;
                        nBalanceInQuestion += walletCoin->tx->vout[n].nValue;
                    }
                    else if(!outputSpentInWallet && !outputIsInUTXO)
                    {
                        if (attemptRepair)
                        {
                            std::vector<uint256> hashesToErase;
                            std::vector<uint256> hashesErased;
                            CWalletDB walletdb(*dbw);
                            hashesToErase.push_back(walletCoinOutpoint.getTransactionHash());
                            pactiveWallet->ZapSelectTx(walletdb, hashesToErase, hashesErased);
                        }
                        printf("CompareWalletAgainstUTXO: Found wallet-unspent coins that aren't in the chain utxo and therefore should be spent %s %s[%ld]\n", FormatMoney(walletCoin->tx->vout[n].nValue).c_str(), hash.ToString().c_str(), n);
                        nMismatchFound++;
                        nBalanceInQuestion += walletCoin->tx->vout[n].nValue;
                    }
                }
            }  
        }
    }
}

bool CWallet::RemoveAllOrphans(uint64_t& numErased, uint64_t& numDetected, std::string& strError)
{
    std::vector<uint256> transactionsToZap;
    std::vector<uint256> transactionsZapped;
    numErased = 0;
    for(auto& [hash, walletCoin] : mapWallet)
    {
        if(walletCoin.IsCoinBase() && (walletCoin.GetDepthInMainChain() < 0))
        {
           transactionsToZap.emplace_back(hash);
        }
    }
    
    numDetected = transactionsToZap.size();
    if (numDetected > 0)
    {
        LogPrintf("Removing [%d] orphan transactions from wallet\n", numDetected);
        
        CWalletDB walletdb(*pactiveWallet->dbw);
        bool ret = ZapSelectTx(walletdb, transactionsToZap, transactionsZapped) != DB_LOAD_OK;
        numErased = transactionsZapped.size();
        LogPrintf("Removed [%d] of [%d] orphan transactions from wallet\n", numErased, numDetected);
        if (!ret)
        {
            strError = "Failed to erase orphan transactions for account.";
            LogPrintf("%s\n", strError.c_str());
            CAlert::Notify(strError, true, true);
            return false;
        }
    }
    return true;
}

/** @} */ // end of Actions

class CAffectedKeysVisitor : public boost::static_visitor<void> {
private:
    const CKeyStore &keystore;
    std::vector<CKeyID> &vKeys;

public:
    CAffectedKeysVisitor(const CKeyStore &keystoreIn, std::vector<CKeyID> &vKeysIn) : keystore(keystoreIn), vKeys(vKeysIn) {}

    void Process(const CTxOut &out) {
        txnouttype type;
        std::vector<CTxDestination> vDest;
        int nRequired;
        if (ExtractDestinations(out, type, vDest, nRequired)) {
            for(const CTxDestination &dest : vDest)
                boost::apply_visitor(*this, dest);
        }
    }

    void operator()(const CKeyID &keyId) {
        if (keystore.HaveKey(keyId))
            vKeys.push_back(keyId);
    }

    void operator()(const CScriptID &scriptId) {
        CScript script;
        if (keystore.GetCScript(scriptId, script))
        {
            txnouttype type;
            std::vector<CTxDestination> vDest;
            int nRequired;
            if (ExtractDestinations(script, type, vDest, nRequired))
            {
                for (const CTxDestination &dest : vDest)
                {
                    boost::apply_visitor(*this, dest);
                }
            }
        }
    }

    void operator()(const CPoW2WitnessDestination &dest) {
        if (keystore.HaveKey(dest.witnessKey))
            vKeys.push_back(dest.witnessKey);
        if (keystore.HaveKey(dest.spendingKey))
            vKeys.push_back(dest.spendingKey);
    }

    void operator()(const CNoDestination &none) {}
};

void CWallet::GetKeyBirthTimes(std::map<CKeyID, int64_t> &mapKeyBirth) const
{
    LOCK(cs_wallet);

    AssertLockHeld(cs_wallet); // mapKeyMetadata
    mapKeyBirth.clear();

    // get birth times for keys with metadata
    for (std::map<CKeyID, CKeyMetadata>::const_iterator it = mapKeyMetadata.begin(); it != mapKeyMetadata.end(); it++)
    {
        if (it->second.nCreateTime)
        {
            mapKeyBirth[it->first] = it->second.nCreateTime;
        }
    }

    // map in which we'll infer heights of other keys
    auto& chain = IsPartialSyncActive() ? partialChain : chainActive;
    CBlockIndex *pindexMax = chain[std::max(0, chain.Height() - 144)]; // the tip can be reorganized; use a 144-block safety margin
    std::map<CKeyID, CBlockIndex*> mapKeyFirstBlock;
    std::set<CKeyID> setKeys;
    for (const auto& [accountUUID, forAccount] : mapAccounts)
    {
        (unused) accountUUID;
        forAccount->GetKeys(setKeys);
        for(const auto keyid : setKeys)
        {
            if (mapKeyBirth.count(keyid) == 0)
            {
                mapKeyFirstBlock[keyid] = pindexMax;
            }
        }
        setKeys.clear();
    }

    // if there are no such keys, we're done
    if (mapKeyFirstBlock.empty())
        return;

    // find first block that affects those keys, if there are any left
    std::vector<CKeyID> vAffected;
    for (std::map<uint256, CWalletTx>::const_iterator it = mapWallet.begin(); it != mapWallet.end(); it++)
    {
        // iterate over all wallet transactions...
        const CWalletTx &wtx = (*it).second;
        BlockMap::const_iterator blit = mapBlockIndex.find(wtx.hashBlock);
        if (blit != mapBlockIndex.end() && chain.Contains(blit->second))
        {
            // ... which are already in a block
            int nHeight = blit->second->nHeight;
            for(const CTxOut &txout : wtx.tx->vout)
            {
                // iterate over all their outputs
                for (const auto& [accountUUID, forAccount] : mapAccounts)
                {
                    (unused) accountUUID;
                    for (const auto keyChain : { KEYCHAIN_EXTERNAL, KEYCHAIN_CHANGE })
                    {
                        const auto& keyStore = (keyChain == KEYCHAIN_EXTERNAL) ? forAccount->externalKeyStore : forAccount->internalKeyStore;
                        CAffectedKeysVisitor(keyStore, vAffected).Process(txout);
                        // ... and all their affected keys
                        for(const CKeyID &keyid : vAffected)
                        {
                            std::map<CKeyID, CBlockIndex*>::iterator rit = mapKeyFirstBlock.find(keyid);
                            if (rit != mapKeyFirstBlock.end() && nHeight < rit->second->nHeight)
                            {
                                rit->second = blit->second;
                            }
                        }
                        vAffected.clear();
                    }
                }
            }
        }
    }

    // Extract block timestamps for those keys
    for (std::map<CKeyID, CBlockIndex*>::const_iterator it = mapKeyFirstBlock.begin(); it != mapKeyFirstBlock.end(); it++)
    {
        mapKeyBirth[it->first] = it->second->GetBlockTime() - TIMESTAMP_WINDOW; // block times can be 2h off
    }
}

/**
 * Compute smart timestamp for a transaction being added to the wallet.
 *
 * Logic:
 * - If sending a transaction, assign its timestamp to the current time.
 * - If receiving a transaction outside a block, assign its timestamp to the
 *   current time.
 * - If receiving a block with a future timestamp, assign all its (not already
 *   known) transactions' timestamps to the current time.
 * - If receiving a block with a past timestamp, but after the most recent known
 *   transaction, assign all its (not already known) transactions' timestamps to
 *   the block time.
 *
 * For more information see CWalletTx::nTimeSmart,
 * https://bitcointalk.org/?topic=54527, or
 * https://github.com/bitcoin/bitcoin/pull/1393.
 */
unsigned int CWallet::ComputeTimeSmart(const CWalletTx& wtx) const
{
    unsigned int nTimeSmart = wtx.nTimeReceived;
    if (!wtx.hashUnset()) {
        if (mapBlockIndex.count(wtx.hashBlock)) {
            int64_t latestNow = wtx.nTimeReceived;

            // Tolerate times up to the last timestamp in the wallet not more than 5 minutes into the future
            int64_t latestTolerated = latestNow + 300;
            const TxItems& txOrdered = wtxOrdered;
            for (auto it = txOrdered.rbegin(); it != txOrdered.rend(); ++it) {
                CWalletTx* const pwtx = it->second.first;
                if (pwtx == &wtx) {
                    continue;
                }
                CAccountingEntry* const pacentry = it->second.second;
                int64_t nSmartTime;
                if (pwtx) {
                    nSmartTime = pwtx->nTimeSmart;
                    if (!nSmartTime) {
                        nSmartTime = pwtx->nTimeReceived;
                    }
                } else {
                    nSmartTime = pacentry->nTime;
                }
                if (nSmartTime <= latestTolerated) {
                    if (nSmartTime > latestNow) {
                        latestNow = nSmartTime;
                    }
                    break;
                }
            }

            int64_t blocktime = mapBlockIndex[wtx.hashBlock]->GetBlockTime();
            nTimeSmart = std::min(blocktime, latestNow);
        } else {
            LogPrintf("%s: found %s in block %s not in index\n", __func__, wtx.GetHash().ToString(), wtx.hashBlock.ToString());
        }
    }
    return nTimeSmart;
}

bool CWallet::AddDestData(const CTxDestination &dest, const std::string &key, const std::string &value)
{
    if (boost::get<CNoDestination>(&dest))
        return false;

    mapAddressBook[CNativeAddress(dest).ToString()].destdata.insert(std::pair(key, value));
    return CWalletDB(*dbw).WriteDestData(CNativeAddress(dest).ToString(), key, value);
}

bool CWallet::EraseDestData(const CTxDestination &dest, const std::string &key)
{
    if (!mapAddressBook[CNativeAddress(dest).ToString()].destdata.erase(key))
        return false;
    return CWalletDB(*dbw).EraseDestData(CNativeAddress(dest).ToString(), key);
}

bool CWallet::LoadDestData(const CTxDestination &dest, const std::string &key, const std::string &value)
{
    mapAddressBook[CNativeAddress(dest).ToString()].destdata.insert(std::pair(key, value));
    return true;
}

bool CWallet::GetDestData(const CTxDestination &dest, const std::string &key, std::string *value) const
{
    std::map<std::string, CAddressBookData>::const_iterator i = mapAddressBook.find(CNativeAddress(dest).ToString());
    if(i != mapAddressBook.end())
    {
        CAddressBookData::StringMap::const_iterator j = i->second.destdata.find(key);
        if(j != i->second.destdata.end())
        {
            if(value)
                *value = j->second;
            return true;
        }
    }
    return false;
}

std::vector<std::string> CWallet::GetDestValues(const std::string& prefix) const
{
    LOCK(cs_wallet);
    std::vector<std::string> values;
    for (const auto& address : mapAddressBook) {
        for (const auto& data : address.second.destdata) {
            if (!data.first.compare(0, prefix.size(), prefix)) {
                values.emplace_back(data.second);
            }
        }
    }
    return values;
}

CKeyPool::CKeyPool()
{
    nTime = GetTime();
}

CKeyPool::CKeyPool(const CPubKey& vchPubKeyIn, const std::string& accountNameIn, int64_t nChainIn)
: nTime( GetTime() )
, vchPubKey( vchPubKeyIn )
, accountName( accountNameIn )
, nChain ( nChainIn )
{
}

CWalletKey::CWalletKey(int64_t nExpires)
{
    nTimeCreated = (nExpires ? GetTime() : 0);
    nTimeExpires = nExpires;
}

namespace
{
    class CRecipientVisitor : public boost::static_visitor<CRecipient>
    {
    private:
        CTxDestination dest;
        CAmount nValue;
        bool fSubtractFeeFromAmount;
    public:
        CRecipientVisitor(const CTxDestination& dest_, CAmount nValue_, bool fSubtractFeeFromAmount_) : dest(dest_), nValue(nValue_), fSubtractFeeFromAmount(fSubtractFeeFromAmount_) {  }

        CRecipient operator()(const CNoDestination &dest) const
        {
            return CRecipient(GetScriptForDestination(dest), nValue, fSubtractFeeFromAmount);
        }

        CRecipient operator()(const CKeyID &keyID) const
        {
            return CRecipient(CTxOutStandardKeyHash(keyID), nValue, fSubtractFeeFromAmount);
        }

        CRecipient operator()(const CScriptID &scriptID) const
        {
            return CRecipient(GetScriptForDestination(dest), nValue, fSubtractFeeFromAmount);
        }

        CRecipient operator()(const CPoW2WitnessDestination& destinationPoW2Witness) const
        {
            return CRecipient(GetPoW2WitnessOutputFromWitnessDestination(destinationPoW2Witness), nValue, fSubtractFeeFromAmount);
        }
    };
}
CRecipient GetRecipientForDestination(const CTxDestination& dest, CAmount nValue, bool fSubtractFeeFromAmount, int nPoW2Phase)
{
    if (nPoW2Phase < 4)
    {
        return CRecipient(GetScriptForDestination(dest), nValue, fSubtractFeeFromAmount);
    }
    else
    {
        return boost::apply_visitor(CRecipientVisitor(dest, nValue, fSubtractFeeFromAmount), dest);
    }
}

CRecipient GetRecipientForTxOut(const CTxOut& out, CAmount nValue, bool fSubtractFeeFromAmount)
{
    switch(out.GetType())
    {
        case CTxOutType::ScriptLegacyOutput:
            return CRecipient(out.output.scriptPubKey, nValue, fSubtractFeeFromAmount);
        case CTxOutType::PoW2WitnessOutput:
            return CRecipient(out.output.witnessDetails, nValue, fSubtractFeeFromAmount);
        case CTxOutType::StandardKeyHashOutput:
            return CRecipient(out.output.standardKeyHash, nValue, fSubtractFeeFromAmount);
    }
    return CRecipient();
}

const CBlockIndex* CWallet::LastSPVBlockProcessed() const
{
    return pSPVScanner ? pSPVScanner->LastBlockProcessed() : nullptr;
}

int64_t CWallet::birthTime() const
{
    int64_t birthTime = 0;

    // determine block time of earliest transaction (if any)
    // if this cannot be determined for every transaction a phrase without birth time acceleration will be used
    int64_t firstTransactionTime = std::numeric_limits<int64_t>::max();
    for (CWallet::TxItems::const_iterator it = pactiveWallet->wtxOrdered.begin(); it != pactiveWallet->wtxOrdered.end(); ++it)
    {
        CWalletTx* wtx = it->second.first;
        if (!wtx->hashUnset())
        {
            CBlockIndex* index = mapBlockIndex.count(wtx->hashBlock) ? mapBlockIndex[wtx->hashBlock] : nullptr;
            // try to get time from block timestamp
            if (index && index->IsValid(BLOCK_VALID_HEADER))
                firstTransactionTime = std::min(firstTransactionTime, std::max(int64_t(0), index->GetBlockTime()));
            else if (wtx->nBlockTime > 0)
            {
                firstTransactionTime = std::min(firstTransactionTime, int64_t(wtx->nBlockTime));
            }
            else
            {
                // can't determine transaction time, only safe option left
                firstTransactionTime = 0;
                break;
            }
        }
    }

    int64_t tipTime;
    const CBlockIndex* lastSPVBlock = pactiveWallet->LastSPVBlockProcessed();
    if (lastSPVBlock)
        tipTime = lastSPVBlock->GetBlockTime();
    else
        tipTime = chainActive.Tip()->GetBlockTime();

    // never use a time beyond our processed tip either spv or full sync
    birthTime = std::min(tipTime, firstTransactionTime);

    return birthTime;
}

void CWallet::BeginUnlocked(std::string reason, std::function<void (void)> callback)
{
    // scoped because of cs_wallet
    {
        LOCK(cs_wallet);

        bool beganLocked = IsLocked();

        // try to unlock
        if (beganLocked) {
            uiInterface.RequestUnlockWithCallback(this, reason, [=]() {
                // async callback, so the scoped lock above is already destructed, need to lock here again
                {
                    LOCK(cs_wallet);

                    // begin session
                    nUnlockSessions++;

                    // when locked before the session, enable auto lock when all sessions end
                    if (beganLocked)
                        fAutoLock = true;
                }

                // execute callback without cs_wallet
                callback();
            });
            return;
        }

        // begin session
        nUnlockSessions++;

        // when locked before the session enable, auto lock when all sessions end
        if (beganLocked)
            fAutoLock = true;
    }

    // execute callback without cs_wallet
    callback();
}

bool CWallet::BeginUnlocked(const SecureString& strWalletPassphrase)
{
    LOCK(cs_wallet);

    bool beganLocked = IsLocked();

    // try to unlock
    if (beganLocked) {
        if (!Unlock(strWalletPassphrase))
            return false;
    }

    // begin session
    nUnlockSessions++;

    // when locked before the session enable auto lock when all sessions end
    if (beganLocked)
        fAutoLock = true;

    return true;
}

void CWallet::EndUnlocked()
{
    LOCK(cs_wallet);

    assert(nUnlockSessions > 0);

    nUnlockSessions--;
    if (nUnlockSessions == 0 && fAutoLock) {
        LockHard();
    }
}
