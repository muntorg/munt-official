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

#include "wallet/wallet.h"

#include "base58.h"
#include "checkpoints.h"
#include "chain.h"
#include "wallet/coincontrol.h"
#include "consensus/consensus.h"
#include "consensus/validation.h"
#include "fs.h"
#include "key.h"
#include "keystore.h"
#include "validation.h"
#include "net.h"
#include "policy/fees.h"
#include "policy/policy.h"
#include "policy/rbf.h"
#include "primitives/block.h"
#include "primitives/transaction.h"
#include "script/script.h"
#include "script/sign.h"
#include "scheduler.h"
#include "timedata.h"
#include "txmempool.h"
#include "util.h"
#include "ui_interface.h"
#include "utilmoneystr.h"

#include <assert.h>

#include <boost/algorithm/string/replace.hpp>
#include <boost/thread.hpp>
#include <fstream>

//Gulden specific includes
#include "init.h"
#include <Gulden/guldenapplication.h>
#include <Gulden/mnemonic.h>
#include <script/ismine.h>

//fixme: (GULDEN) (2.1)
#include "Gulden/util.h"
#include "validation.h"

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

const uint256 CMerkleTx::ABANDON_HASH(uint256S("0000000000000000000000000000000000000000000000000000000000000001"));

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

/* GULDEN - we implement this completely differently
CPubKey CWallet::GenerateNewKey(bool internal)
{
    AssertLockHeld(cs_wallet); // mapKeyMetadata
    bool fCompressed = CanSupportFeature(FEATURE_COMPRPUBKEY); // default to compressed public keys if we want 0.6.0 wallets

    CKey secret;

    // Create new metadata
    int64_t nCreationTime = GetTime();
    CKeyMetadata metadata(nCreationTime);

    // use HD key derivation if HD was enabled during wallet creation
    if (IsHDEnabled()) {
        DeriveNewChildKey(metadata, secret, (CanSupportFeature(FEATURE_HD_SPLIT) ? internal : false));
    } else {
        secret.MakeNewKey(fCompressed);
    }

    // Compressed public keys were introduced in version 0.6.0
    if (fCompressed)
        SetMinVersion(FEATURE_COMPRPUBKEY);

    CPubKey pubkey = secret.GetPubKey();
    assert(secret.VerifyPubKey(pubkey));

    mapKeyMetadata[pubkey.GetID()] = metadata;
    UpdateTimeFirstKey(nCreationTime);

    if (!AddKeyPubKey(secret, pubkey))
        throw std::runtime_error(std::string(__func__) + ": AddKey failed");
    return pubkey;
}

void CWallet::DeriveNewChildKey(CKeyMetadata& metadata, CKey& secret, bool internal)
{
    // for now we use a fixed keypath scheme of m/0'/0'/k
    CKey key;                      //master key seed (256bit)
    CExtKey masterKey;             //hd master key
    CExtKey accountKey;            //key at m/0'
    CExtKey chainChildKey;         //key at m/0'/0' (external) or m/0'/1' (internal)
    CExtKey childKey;              //key at m/0'/0'/<n>'

    // try to get the master key
    if (!GetKey(hdChain.masterKeyID, key))
        throw std::runtime_error(std::string(__func__) + ": Master key not found");

    masterKey.SetMaster(key.begin(), key.size());

    // derive m/0'
    // use hardened derivation (child keys >= 0x80000000 are hardened after bip32)
    masterKey.Derive(accountKey, BIP32_HARDENED_KEY_LIMIT);

    // derive m/0'/0' (external chain) OR m/0'/1' (internal chain)
    assert(internal ? CanSupportFeature(FEATURE_HD_SPLIT) : true);
    accountKey.Derive(chainChildKey, BIP32_HARDENED_KEY_LIMIT+(internal ? 1 : 0));

    // derive child key at next index, skip keys already known to the wallet
    do {
        // always derive hardened keys
        // childIndex | BIP32_HARDENED_KEY_LIMIT = derive childIndex in hardened child-index-range
        // example: 1 | BIP32_HARDENED_KEY_LIMIT == 0x80000001 == 2147483649
        if (internal) {
            chainChildKey.Derive(childKey, hdChain.nInternalChainCounter | BIP32_HARDENED_KEY_LIMIT);
            metadata.hdKeypath = "m/0'/1'/" + std::to_string(hdChain.nInternalChainCounter) + "'";
            hdChain.nInternalChainCounter++;
        }
        else {
            chainChildKey.Derive(childKey, hdChain.nExternalChainCounter | BIP32_HARDENED_KEY_LIMIT);
            metadata.hdKeypath = "m/0'/0'/" + std::to_string(hdChain.nExternalChainCounter) + "'";
            hdChain.nExternalChainCounter++;
        }
    } while (HaveKey(childKey.key.GetPubKey().GetID()));
    secret = childKey.key;
    metadata.hdMasterKeyID = hdChain.masterKeyID;
    // update the chain model in the database
    if (!CWalletDB(*dbw).WriteHDChain(hdChain))
        throw std::runtime_error(std::string(__func__) + ": Writing HD chain model failed");
}
*/

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

    if (!IsCrypted())
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

/* GULDEN - we implement this completely differently
bool CWallet::AddCryptedKey(const CPubKey &vchPubKey,
                            const std::vector<unsigned char> &vchCryptedSecret)
{
    if (!CCryptoKeyStore::AddCryptedKey(vchPubKey, vchCryptedSecret))
        return false;
    {
        LOCK(cs_wallet);
        if (pwalletdbEncryption)
            return pwalletdbEncryption->WriteCryptedKey(vchPubKey,
                                                        vchCryptedSecret,
                                                        mapKeyMetadata[vchPubKey.GetID()]);
        else
            return CWalletDB(*dbw).WriteCryptedKey(vchPubKey,
                                                            vchCryptedSecret,
                                                            mapKeyMetadata[vchPubKey.GetID()]);
    }
    return false;
}
*/

bool CWallet::LoadKeyMetadata(const CPubKey &pubkey, const CKeyMetadata &meta)
{
    AssertLockHeld(cs_wallet); // mapKeyMetadata
    if (meta.nCreateTime && (!nTimeFirstKey || meta.nCreateTime < nTimeFirstKey))
        nTimeFirstKey = meta.nCreateTime;

    mapKeyMetadata[pubkey.GetID()] = meta;
    return true;
}

bool CWallet::LoadCryptedKey(const CPubKey &vchPubKey, const std::vector<unsigned char> &vchCryptedSecret, const std::string& forAccount, int64_t nKeyChain)
{
    LOCK(cs_wallet);

    if (mapAccounts.find(getUUIDFromString(forAccount)) == mapAccounts.end())
        return false;
 
    return mapAccounts[getUUIDFromString(forAccount)]->AddCryptedKey(vchPubKey, vchCryptedSecret, nKeyChain);
}

void CWallet::UpdateTimeFirstKey(int64_t nCreateTime)
{
    AssertLockHeld(cs_wallet);
    if (nCreateTime <= 1) {
        // Cannot determine birthday information, so set the wallet birthday to
        // the beginning of time.
        nTimeFirstKey = 1;
    } else if (!nTimeFirstKey || nCreateTime < nTimeFirstKey) {
        nTimeFirstKey = nCreateTime;
    }
}

bool CWallet::AddCScript(const CScript& redeemScript)
{
    LOCK(cs_wallet);

    //fixme: (GULDEN) (FUT) (WATCHONLY)
    bool ret = false;
    for (auto accountPair : mapAccounts)
    {
        if (accountPair.second->AddCScript(redeemScript))
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
        std::string strAddr = CBitcoinAddress(CScriptID(redeemScript)).ToString();
        LogPrintf("%s: Warning: This wallet contains a redeemScript of size %i which exceeds maximum size %i thus can never be redeemed. Do not use address %s.\n",
            __func__, redeemScript.size(), MAX_SCRIPT_ELEMENT_SIZE, strAddr);
        return true;
    }

    //fixme: (GULDEN) (FUT) (WATCHONLY)
    bool ret = false;
    for (auto accountPair : mapAccounts)
    {
        ret = accountPair.second->AddCScript(redeemScript);
        if (ret == true)
            break;
    }
    return ret;
}

bool CWallet::AddWatchOnly(const CScript &dest, int64_t nCreateTime)
{
    AssertLockHeld(cs_wallet);

    bool ret = false;
    for (auto accountPair : mapAccounts)
    {
        //fixme: (GULDEN) (FUT) (WATCHONLY) - nCreateTime should go here as well?
        if (accountPair.second->AddWatchOnly(dest))
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
    //fixme: (GULDEN) (FUT) (WATCHONLY)
    bool ret = true;
    for (auto accountPair : mapAccounts)
    {
        if (accountPair.second->RemoveWatchOnly(dest))
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

    //fixme: (GULDEN) (FUT) (WATCHONLY)
    bool ret = false;
    for (auto accountPair : mapAccounts)
    {
        ret = accountPair.second->AddWatchOnly(dest);
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
        BOOST_FOREACH(const MasterKeyMap::value_type& pMasterKey, mapMasterKeys)
        {
            if(!crypter.SetKeyFromPassphrase(strWalletPassphrase, pMasterKey.second.vchSalt, pMasterKey.second.nDeriveIterations, pMasterKey.second.nDerivationMethod))
                return false;
            if (!crypter.Decrypt(pMasterKey.second.vchCryptedKey, _vMasterKey))
                continue; // try another master key
            for (auto accountPair : mapAccounts)
            {
                if (!accountPair.second->Unlock(_vMasterKey))
                {
                    return false;
                }
            }
            for (auto seedPair : mapSeeds)
            {
                if (!seedPair.second->Unlock(_vMasterKey))
                {
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
        CKeyingMaterial _vMasterKey;
        BOOST_FOREACH(MasterKeyMap::value_type& pMasterKey, mapMasterKeys)
        {
            if(!crypter.SetKeyFromPassphrase(strOldWalletPassphrase, pMasterKey.second.vchSalt, pMasterKey.second.nDeriveIterations, pMasterKey.second.nDerivationMethod))
                return false;
            if (!crypter.Decrypt(pMasterKey.second.vchCryptedKey, _vMasterKey))
                return false;
            if (static_cast<CGuldenWallet*>(this)->Unlock(_vMasterKey))
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

    BOOST_FOREACH(const CTxIn& txin, wtx.tx->vin)
    {
        if (mapTxSpends.count(txin.prevout) <= 1)
            continue;  // No conflict if zero or one spends
        range = mapTxSpends.equal_range(txin.prevout);
        for (TxSpends::const_iterator _it = range.first; _it != range.second; ++_it)
            result.insert(_it->second);
    }
    return result;
}

bool CWallet::HasWalletSpend(const uint256& txid) const
{
    AssertLockHeld(cs_wallet);
    auto iter = mapTxSpends.lower_bound(COutPoint(txid, 0));
    return (iter != mapTxSpends.end() && iter->first.hash == txid);
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

    for (const std::string& walletFile : gArgs.GetArgs("-wallet")) {
        if (boost::filesystem::path(walletFile).filename() != walletFile) {
            return InitError(_("-wallet parameter must only specify a filename (not a path)"));
        } else if (SanitizeString(walletFile, SAFE_CHARS_FILENAME) != walletFile) {
            return InitError(_("Invalid characters in -wallet filename"));
        }

        std::string strError;
        if (!CWalletDB::VerifyEnvironment(walletFile, GetDataDir().string(), strError)) {
            return InitError(strError);
        }

        if (GetBoolArg("-salvagewallet", false)) {
            // Recover readable keypairs:
            CWallet dummyWallet;
            std::string backup_filename;
            if (!CWalletDB::Recover(walletFile, (void *)&dummyWallet, CWalletDB::RecoverKeysOnlyFilter, backup_filename)) {
                return false;
            }
        }

        std::string strWarning;
        bool dbV = CWalletDB::VerifyDatabaseFile(walletFile, GetDataDir().string(), strWarning, strError);
        if (!strWarning.empty()) {
            InitWarning(strWarning);
        }
        if (!dbV) {
            InitError(strError);
            return false;
        }
    }

    //fixme: (GULDEN) (MERGE)
    // Check file permissions.
    /*{
        std::fstream testPerms((GetDataDir() / walletFile).string(), std::ios::in | std::ios::out | std::ios::app);
        if (!testPerms.is_open())
            return InitError(strprintf(_("%s may be read only or have permissions that deny access to the current user, please correct this and try again."), walletFile));
    }*/

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
bool CWallet::IsSpent(const uint256& hash, unsigned int n) const
{
    const COutPoint outpoint(hash, n);
    std::pair<TxSpends::const_iterator, TxSpends::const_iterator> range;
    range = mapTxSpends.equal_range(outpoint);

    for (TxSpends::const_iterator it = range.first; it != range.second; ++it)
    {
        const uint256& wtxid = it->second;
        std::map<uint256, CWalletTx>::const_iterator mit = mapWallet.find(wtxid);
        if (mit != mapWallet.end()) {
            int depth = mit->second.GetDepthInMainChain();
            if (depth > 0  || (depth == 0 && !mit->second.isAbandoned()))
                return true; // Spent
        }
    }
    return false;
}

void CWallet::AddToSpends(const COutPoint& outpoint, const uint256& wtxid)
{
    mapTxSpends.insert(std::make_pair(outpoint, wtxid));

    std::pair<TxSpends::iterator, TxSpends::iterator> range;
    range = mapTxSpends.equal_range(outpoint);
    SyncMetaData(range);
}


void CWallet::AddToSpends(const uint256& wtxid)
{
    assert(mapWallet.count(wtxid));
    CWalletTx& thisTx = mapWallet[wtxid];
    if (thisTx.IsCoinBase()) // Coinbases don't spend anything!
        return;

    BOOST_FOREACH(const CTxIn& txin, thisTx.tx->vin)
        AddToSpends(txin.prevout, wtxid);
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

        for (auto seedIter : mapSeeds)
        {
            if (!seedIter.second->Encrypt(_vMasterKey))
            {
            pwalletdbEncryption->TxnAbort();
            delete pwalletdbEncryption;
                // We now probably have half of our keys encrypted in memory, and half not...
                // die and let the user reload the unencrypted wallet.
                assert(false);
            }
            pwalletdbEncryption->WriteHDSeed(*seedIter.second);
        }

        for (auto accountPair : mapAccounts)
        {
            if (!accountPair.second->Encrypt(_vMasterKey))
            {
                pwalletdbEncryption->TxnAbort();
                delete pwalletdbEncryption;

                // We now probably have half of our keys encrypted in memory, and half not...
                // die and let the user reload the unencrypted wallet.
                assert(false);
            }
            pwalletdbEncryption->WriteAccount(getUUIDAsString(accountPair.second->getUUID()), accountPair.second);
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

        for (auto accountPair : mapAccounts)
        {
            accountPair.second->Lock();
        }
        for (auto seedIter : mapSeeds)
        {
            seedIter.second->Lock();
        }
        Unlock(strWalletPassphrase);

        //fixme: (GULDEN) (FUT) (HD) (ACCOUNTS)
        //fixme: Gulden HD - What to do here? We can't really throw the entire seed away...
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
    for (auto accountPair : mapAccounts)
    {
        accountPair.second->internalKeyStore.NotifyStatusChanged(&accountPair.second->internalKeyStore);
        accountPair.second->externalKeyStore.NotifyStatusChanged(&accountPair.second->externalKeyStore);
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
        txByTime.insert(std::make_pair(wtx->nTimeReceived, TxPair(wtx, (CAccountingEntry*)0)));
    }
    std::list<CAccountingEntry> acentries;
    walletdb.ListAccountCreditDebit("", acentries);
    BOOST_FOREACH(CAccountingEntry& entry, acentries)
    {
        txByTime.insert(std::make_pair(entry.nTime, TxPair((CWalletTx*)0, &entry)));
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
            BOOST_FOREACH(const int64_t& nOffsetStart, nOrderPosOffsets)
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
    assert(0);
    /*CWalletDB walletdb(*dbw);
    if (!walletdb.TxnBegin())
        return false;

    int64_t nNow = GetAdjustedTime();

    // Debit
    CAccountingEntry debit;
    debit.nOrderPos = IncOrderPosNext(&walletdb);
    debit.strAccount = strFrom;
    debit.nCreditDebit = -nAmount;
    debit.nTime = nNow;
    debit.strOtherAccount = strTo;
    debit.strComment = strComment;
    AddAccountingEntry(debit, &walletdb);

    // Credit
    CAccountingEntry credit;
    credit.nOrderPos = IncOrderPosNext(&walletdb);
    credit.strAccount = strTo;
    credit.nCreditDebit = nAmount;
    credit.nTime = nNow;
    credit.strOtherAccount = strFrom;
    credit.strComment = strComment;
    AddAccountingEntry(credit, &walletdb);

    if (!walletdb.TxnCommit())
        return false;*/

    return true;
}

bool CWallet::GetAccountPubkey(CPubKey &pubKey, std::string strAccount, bool bForceNew)
{
    assert(0);
    /* GULDEN - function unused.
    CWalletDB walletdb(*dbw);

    CAccount account;
    walletdb.ReadAccount(strAccount, account);

    if (!bForceNew) {
        if (!account.vchPubKey.IsValid())
            bForceNew = true;
        else {
            // Check if the current key has been used
            CScript scriptPubKey = GetScriptForDestination(account.vchPubKey.GetID());
            for (std::map<uint256, CWalletTx>::iterator it = mapWallet.begin();
                 it != mapWallet.end() && account.vchPubKey.IsValid();
                 ++it)
                BOOST_FOREACH(const CTxOut& txout, (*it).second.tx->vout)
                    if (txout.scriptPubKey == scriptPubKey) {
                        bForceNew = true;
                        break;
                    }
        }
    }

    //fixme: GULDEN - the below is completely broken - redo this if we ever actually need it.
    // Generate a new key
    if (bForceNew) {
        if (!GetKeyFromPool(account.vchPubKey, false))
            return false;

        SetAddressBook(account.vchPubKey.GetID(), strAccount, "receive");
        walletdb.WriteAccount(strAccount, account);
    }

    pubKey = account.vchPubKey;
    */
    return true;
}

void CWallet::MarkDirty()
{
    {
        LOCK(cs_wallet);
        BOOST_FOREACH(PAIRTYPE(const uint256, CWalletTx)& item, mapWallet)
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

    // Ensure for now that we're not overwriting data
    assert(wtx.mapValue.count("replaced_by_txid") == 0);

    wtx.mapValue["replaced_by_txid"] = newHash.ToString();

    CWalletDB walletdb(*dbw, "r+");

    bool success = true;
    if (!walletdb.WriteTx(wtx)) {
        LogPrintf("%s: Updating walletdb tx %s failed", __func__, wtx.GetHash().ToString());
        success = false;
    }

    NotifyTransactionChanged(this, originalHash, CT_UPDATED);

    return success;
}

bool CWallet::AddToWallet(const CWalletTx& wtxIn, bool fFlushOnClose)
{
    LOCK(cs_wallet);

    CWalletDB walletdb(*dbw, "r+", fFlushOnClose);

    uint256 hash = wtxIn.GetHash();

    // Inserts only if not already there, returns tx inserted or tx found
    std::pair<std::map<uint256, CWalletTx>::iterator, bool> ret = mapWallet.insert(std::make_pair(hash, wtxIn));
    CWalletTx& wtx = (*ret.first).second;
    if (wtx.strFromAccount.empty())
    {
        LOCK(cs_wallet);
        for (auto accountPair : mapAccounts)
        {
            if (accountPair.second->HaveWalletTx(wtx))
            {
                wtx.strFromAccount = getUUIDAsString(accountPair.first);
            }
        }
    }
    wtx.BindWallet(this);
    bool fInsertedNew = ret.second;
    if (fInsertedNew)
    {
        wtx.nTimeReceived = GetAdjustedTime();
        wtx.nOrderPos = IncOrderPosNext(&walletdb);
        wtxOrdered.insert(std::make_pair(wtx.nOrderPos, TxPair(&wtx, (CAccountingEntry*)0)));
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
            fUpdated = true;
        }
        // If no longer abandoned, update
        if (wtxIn.hashBlock.IsNull() && wtx.isAbandoned())
        {
            wtx.hashBlock = wtxIn.hashBlock;
            fUpdated = true;
        }
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
    NotifyTransactionChanged(this, hash, fInsertedNew ? CT_NEW : CT_UPDATED);

    // notify an external script when a wallet transaction comes in or is updated
    std::string strCmd = GetArg("-walletnotify", "");

    if ( !strCmd.empty())
    {
        boost::replace_all(strCmd, "%s", wtxIn.GetHash().GetHex());
        boost::thread t(runCommand, strCmd); // thread runs free
    }

    return true;
}

bool CWallet::LoadToWallet(const CWalletTx& wtxIn)
{
    uint256 hash = wtxIn.GetHash();

    mapWallet[hash] = wtxIn;
    CWalletTx& wtx = mapWallet[hash];
    wtx.BindWallet(this);
    wtxOrdered.insert(std::make_pair(wtx.nOrderPos, TxPair(&wtx, nullptr)));
    AddToSpends(hash);
    for (const CTxIn& txin : wtx.tx->vin) {
        auto it = mapWallet.find(txin.prevout.hash);
        if (it != mapWallet.end()) {
            CWalletTx& prevtx = it->second;
            if (prevtx.nIndex == -1 && !prevtx.hashUnset()) {
                MarkConflicted(prevtx.hashBlock, wtx.GetHash());
            }
        }
    }

    return true;
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

        if (pIndex != NULL) {
            BOOST_FOREACH(const CTxIn& txin, tx.vin) {
                std::pair<TxSpends::const_iterator, TxSpends::const_iterator> range = mapTxSpends.equal_range(txin.prevout);
                while (range.first != range.second) {
                    if (range.first->second != tx.GetHash()) {
                        LogPrintf("Transaction %s (in block %s) conflicts with wallet transaction %s (both spend %s:%i)\n", tx.GetHash().ToString(), pIndex->GetBlockHashPoW2().ToString(), range.first->second.ToString(), range.first->first.hash.ToString(), range.first->first.n);
                        //fixme: (2.0) (POW2) (check that PoW and PoW2 block don't conflict here?)
                        MarkConflicted(pIndex->GetBlockHashPoW2(), range.first->second);
                    }
                    range.first++;
                }
            }
        }

        bool fExisted = mapWallet.count(tx.GetHash()) != 0;
        if (fExisted && !fUpdate) return false;
        if (fExisted || IsMine(tx) || IsFromMe(tx))
        {
            CWalletTx wtx(this, ptx);

            // Get merkle branch if transaction was found in a block
            if (pIndex != NULL)
                wtx.SetMerkleBranch(pIndex, posInBlock);

            RemoveAddressFromKeypoolIfIsMine(tx, pIndex ? pIndex->nTime : 0);
            //fixme: Is this even needed? Surely only checking the outputs is fine
            for(const auto& txin : wtx.tx->vin)
            {
                RemoveAddressFromKeypoolIfIsMine(txin, pIndex ? pIndex->nTime : 0);
            }

            /* GULDEN (MERGE) - IS THIS STILL NEEDED?
            // Add all incoming transactions to the wallet as well - so that we can always get 'incoming' address details.
            for(const auto& txin : tx.vin)
            {
                uint256 hashBlock = uint256();
                if (GetTransaction(txin.prevout.hash, wtx.tx, Params().GetConsensus(), hashBlock, true))
                {
                    AddToWallet(wtx, false);
                }
            }*/

            return AddToWallet(wtx, false);
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
            NotifyTransactionChanged(this, wtx.GetHash(), CT_UPDATED);
            // Iterate over all its outputs, and mark transactions in the wallet that spend them abandoned too
            TxSpends::const_iterator iter = mapTxSpends.lower_bound(COutPoint(hashTx, 0));
            while (iter != mapTxSpends.end() && iter->first.hash == now) {
                if (!done.count(iter->second)) {
                    todo.insert(iter->second);
                }
                iter++;
            }
            // If a transaction changes 'conflicted' state, that changes the balance
            // available of the outputs it spends. So force those to be recomputed
            BOOST_FOREACH(const CTxIn& txin, wtx.tx->vin)
            {
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

    while (!todo.empty()) {
        uint256 now = *todo.begin();
        todo.erase(now);
        done.insert(now);
        auto it = mapWallet.find(now);
        assert(it != mapWallet.end());
        CWalletTx& wtx = it->second;
        int currentconfirm = wtx.GetDepthInMainChain();
        if (conflictconfirms < currentconfirm) {
            // Block is 'more conflicted' than current confirm; update.
            // Mark transaction as conflicted with this block.
            wtx.nIndex = -1;
            wtx.hashBlock = hashBlock;
            wtx.MarkDirty();
            walletdb.WriteTx(wtx);
            // Iterate over all its outputs, and mark transactions in the wallet that spend them conflicted too
            TxSpends::const_iterator iter = mapTxSpends.lower_bound(COutPoint(now, 0));
            while (iter != mapTxSpends.end() && iter->first.hash == now) {
                 if (!done.count(iter->second)) {
                     todo.insert(iter->second);
                 }
                 iter++;
            }
            // If a transaction changes 'conflicted' state, that changes the balance
            // available of the outputs it spends. So force those to be recomputed
            for (const CTxIn& txin : wtx.tx->vin) {
                auto it = mapWallet.find(txin.prevout.hash);
                if (it != mapWallet.end()) {
                    it->second.MarkDirty();
                }
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
    BOOST_FOREACH(const CTxIn& txin, tx.vin)
    {
        if (mapWallet.count(txin.prevout.hash))
            mapWallet[txin.prevout.hash].MarkDirty();
    }
}

void CWallet::TransactionAddedToMempool(const CTransactionRef& ptx) {
    LOCK2(cs_main, cs_wallet);
    SyncTransaction(ptx);
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
}

void CWallet::BlockDisconnected(const std::shared_ptr<const CBlock>& pblock) {
    LOCK2(cs_main, cs_wallet);

    for (const CTransactionRef& ptx : pblock->vtx) {
        SyncTransaction(ptx);
    }
}



isminetype CWallet::IsMine(const CTxIn &txin) const
{
    {
        LOCK(cs_wallet);
        std::map<uint256, CWalletTx>::const_iterator mi = mapWallet.find(txin.prevout.hash);
        if (mi != mapWallet.end())
        {
            const CWalletTx& prev = (*mi).second;
            if (txin.prevout.n < prev.tx->vout.size())
                return IsMine(prev.tx->vout[txin.prevout.n]);
        }
    }
    return ISMINE_NO;
}

// Note that this function doesn't distinguish between a 0-valued input,
// and a not-"is mine" (according to the filter) input.
CAmount CWallet::GetDebit(const CTxIn &txin, const isminefilter& filter, CAccount* forAccount) const
{
    {
        LOCK(cs_wallet);
        std::map<uint256, CWalletTx>::const_iterator mi = mapWallet.find(txin.prevout.hash);
        if (mi != mapWallet.end())
        {
            const CWalletTx& prev = (*mi).second;
            if (txin.prevout.n < prev.tx->vout.size())
            {
                if ( (forAccount && (::IsMine(*forAccount, prev.tx->vout[txin.prevout.n]) & filter)) || (!forAccount && (IsMine(prev.tx->vout[txin.prevout.n]) & filter)) )
                {
                    return prev.tx->vout[txin.prevout.n].nValue;
                }
            }
        }
    }
    return 0;
}


isminetype CWallet::IsMine(const CTxOut& txout) const
{
    return ::IsMine(*this, txout);
}

CAmount CWallet::GetCredit(const CTxOut& txout, const isminefilter& filter, CAccount* forAccount) const
{
    if (!MoneyRange(txout.nValue))
        throw std::runtime_error(std::string(__func__) + ": value out of range");
    return ( ( (forAccount && ::IsMine(*forAccount, txout) & filter) || (!forAccount && IsMine(txout) & filter) ) ? txout.nValue : 0);
}

bool CWallet::IsChange(const CTxOut& txout) const
{
    // TODO: fix handling of 'change' outputs. The assumption is that any
    // payment to a script that is ours, but is not in the address book
    // is change. That assumption is likely to break when we implement multisignature
    // wallets that return change back into a multi-signature-protected address;
    // a better way of identifying which outputs are 'the send' and which are
    // 'the change' will need to be implemented (maybe extend CWalletTx to remember
    // which output, if any, was change).
    if (::IsMine(*this, txout))
    {
        CTxDestination address;
        if (!ExtractDestination(txout, address))
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
    BOOST_FOREACH(const CTxOut& txout, tx.vout)
        if (IsMine(txout)  && txout.nValue >= nMinimumInputValue)
            return true;
    return false;
}

bool CWallet::IsFromMe(const CTransaction& tx) const
{
    return (GetDebit(tx, ISMINE_ALL) > 0);
}

CAmount CWallet::GetDebit(const CTransaction& tx, const isminefilter& filter, CAccount* forAccount) const
{
    CAmount nDebit = 0;
    BOOST_FOREACH(const CTxIn& txin, tx.vin)
    {
        nDebit += GetDebit(txin, filter, forAccount);
        if (!MoneyRange(nDebit))
            throw std::runtime_error(std::string(__func__) + ": value out of range");
    }
    return nDebit;
}

bool CWallet::IsAllFromMe(const CTransaction& tx, const isminefilter& filter) const
{
    LOCK(cs_wallet);

    BOOST_FOREACH(const CTxIn& txin, tx.vin)
    {
        auto mi = mapWallet.find(txin.prevout.hash);
        if (mi == mapWallet.end())
            return false; // any unknown inputs can't be from us

        const CWalletTx& prev = (*mi).second;

        if (txin.prevout.n >= prev.tx->vout.size())
            return false; // invalid input!

        if (!(IsMine(prev.tx->vout[txin.prevout.n]) & filter))
            return false;
    }
    return true;
}

CAmount CWallet::GetCredit(const CTransaction& tx, const isminefilter& filter, CAccount* forAccount) const
{
    CAmount nCredit = 0;
    BOOST_FOREACH(const CTxOut& txout, tx.vout)
    {
        nCredit += GetCredit(txout, filter, forAccount);
        if (!MoneyRange(nCredit))
            throw std::runtime_error(std::string(__func__) + ": value out of range");
    }
    return nCredit;
}

CAmount CWallet::GetChange(const CTransaction& tx) const
{
    CAmount nChange = 0;
    BOOST_FOREACH(const CTxOut& txout, tx.vout)
    {
        nChange += GetChange(txout);
        if (!MoneyRange(nChange))
            throw std::runtime_error(std::string(__func__) + ": value out of range");
    }
    return nChange;
}

/* GULDEN - We don't use these
CPubKey CWallet::GenerateNewHDMasterKey()
{
    CKey key;
    key.MakeNewKey(true);

    int64_t nCreationTime = GetTime();
    CKeyMetadata metadata(nCreationTime);

    // calculate the pubkey
    CPubKey pubkey = key.GetPubKey();
    assert(key.VerifyPubKey(pubkey));

    // set the hd keypath to "m" -> Master, refers the masterkeyid to itself
    metadata.hdKeypath     = "m";
    metadata.hdMasterKeyID = pubkey.GetID();

    {
        LOCK(cs_wallet);

        // mem store the metadata
        mapKeyMetadata[pubkey.GetID()] = metadata;

        // write the key&metadata to the database
        if (!AddKeyPubKey(key, pubkey))
            throw std::runtime_error(std::string(__func__) + ": AddKeyPubKey failed");
    }

    return pubkey;
}

bool CWallet::SetHDMasterKey(const CPubKey& pubkey)
{
    LOCK(cs_wallet);
    // store the keyid (hash160) together with
    // the child index counter in the database
    // as a hdchain object
    CHDChain newHdChain;
    newHdChain.nVersion = CanSupportFeature(FEATURE_HD_SPLIT) ? CHDChain::VERSION_HD_CHAIN_SPLIT : CHDChain::VERSION_HD_BASE;
    newHdChain.masterKeyID = pubkey.GetID();
    SetHDChain(newHdChain, false);

    return true;
}

bool CWallet::SetHDChain(const CHDChain& chain, bool memonly)
{
    LOCK(cs_wallet);
    if (!memonly && !CWalletDB(*dbw).WriteHDChain(chain))
        throw std::runtime_error(std::string(__func__) + ": writing chain failed");

    hdChain = chain;
    return true;
}

bool CWallet::IsHDEnabled() const
{
    return !hdChain.masterKeyID.IsNull();
}
*/

int64_t CWalletTx::GetTxTime() const
{
    int64_t n = nTimeSmart;
    return n ? n : nTimeReceived;
}

int CWalletTx::GetRequestCount() const
{
    // Returns -1 if it wasn't being tracked
    int nRequests = -1;
    {
        LOCK(pwallet->cs_wallet);
        if (IsCoinBase())
        {
            // Generated block
            if (!hashUnset())
            {
                std::map<uint256, int>::const_iterator mi = pwallet->mapRequestCount.find(hashBlock);
                if (mi != pwallet->mapRequestCount.end())
                    nRequests = (*mi).second;
            }
        }
        else
        {
            // Did anyone request this transaction?
            std::map<uint256, int>::const_iterator mi = pwallet->mapRequestCount.find(GetHash());
            if (mi != pwallet->mapRequestCount.end())
            {
                nRequests = (*mi).second;

                // How about the block it's in?
                if (nRequests == 0 && !hashUnset())
                {
                    std::map<uint256, int>::const_iterator _mi = pwallet->mapRequestCount.find(hashBlock);
                    if (_mi != pwallet->mapRequestCount.end())
                        nRequests = (*_mi).second;
                    else
                        nRequests = 1; // If it's in someone else's block it must have got out
                }
            }
        }
    }
    return nRequests;
}

void CWalletTx::GetAmounts(std::list<COutputEntry>& listReceived,
                           std::list<COutputEntry>& listSent, CAmount& nFee, const isminefilter& filter, CKeyStore* from) const
{
    if (!from)
        from = pwallet->activeAccount;

    nFee = 0;
    listReceived.clear();
    listSent.clear();

    // Compute fee:
    CAmount nDebit = GetDebit(filter);
    if (nDebit > 0) // debit>0 means we signed/sent this transaction
    {
        CAmount nValueOut = tx->GetValueOut();
        nFee = nDebit - nValueOut;
    }

    // Sent.
    for (unsigned int i = 0; i < tx->vin.size(); ++i)
    {
        const CTxIn& txin = tx->vin[i];

        std::map<uint256, CWalletTx>::const_iterator mi = pwallet->mapWallet.find(txin.prevout.hash);
        if (mi != pwallet->mapWallet.end())
        {
            const CWalletTx& prev = (*mi).second;
            if (txin.prevout.n < prev.tx->vout.size())
            {
                const auto& prevOut =  prev.tx->vout[txin.prevout.n];

                isminetype fIsMine = IsMine(*from, prevOut);

                if ((fIsMine & filter))
                {
                    CTxDestination address;
                    if (!ExtractDestination(prevOut, address) && !prevOut.IsUnspendable())
                    {
                        LogPrintf("CWalletTx::GetAmounts: Unknown transaction type found, txid %s\n",
                                this->GetHash().ToString());
                        address = CNoDestination();
                    }

                    //fixme: (GULDEN) (FUT) - There should be a seperate CInputEntry class/array here or something.
                    COutputEntry output = {address, prevOut.nValue, (int)i};

                    // We are debited by the transaction, add the output as a "sent" entry
                    listSent.push_back(output);
                }
            }
        }
    }

    // received.
    for (unsigned int i = 0; i < tx->vout.size(); ++i)
    {
        const CTxOut& txout = tx->vout[i];
        isminetype fIsMine = IsMine(*from, txout);
        if (!(fIsMine & filter))
            continue;

        // Get the destination address
        CTxDestination address;
        if (!ExtractDestination(txout, address) && !txout.IsUnspendable())
        {
            LogPrintf("CWalletTx::GetAmounts: Unknown transaction type found, txid %s\n",
                     this->GetHash().ToString());
            address = CNoDestination();
        }

        COutputEntry output = {address, txout.nValue, (int)i};

        // We are receiving the output, add it as a "received" entry
        listReceived.push_back(output);
    }

}


/**
 * Scan the block chain (starting in pindexStart) for transactions
 * from or to us. If fUpdate is true, found transactions that already
 * exist in the wallet will be updated.
 *
 * Returns null if scan was successful. Otherwise, if a complete rescan was not
 * possible (due to pruning or corruption), returns pointer to the most recent
 * block that could not be scanned.
 */
CBlockIndex* CWallet::ScanForWalletTransactions(CBlockIndex* pindexStart, bool fUpdate)
{
    int64_t nNow = GetTime();
    const CChainParams& chainParams = Params();

    CBlockIndex* pindex = pindexStart;
    CBlockIndex* ret = nullptr;
    {
        LOCK2(cs_main, cs_wallet);
        fAbortRescan = false;
        fScanningWallet = true;

        // no need to read and scan block, if block was created before
        // our wallet birthday (as adjusted for block time variability)
        while (pindex && nTimeFirstKey && (pindex->GetBlockTime() < (nTimeFirstKey - TIMESTAMP_WINDOW)))
            pindex = chainActive.Next(pindex);

        ShowProgress(_("Rescanning..."), 0); // show rescan progress in GUI as dialog or on splashscreen, if -rescan on startup
        double dProgressStart = GuessVerificationProgress(chainParams.TxData(), pindex);
        double dProgressTip = GuessVerificationProgress(chainParams.TxData(), chainActive.Tip());
        while (pindex && !fAbortRescan)
        {
            // Temporarily release lock to allow shadow key allocation a chance to do it's thing
            LEAVE_CRITICAL_SECTION(cs_main)
            LEAVE_CRITICAL_SECTION(cs_wallet)
            if (pindex->nHeight % 100 == 0 && dProgressTip - dProgressStart > 0.0)
            {
                ShowProgress(_("Rescanning..."), std::max(1, std::min(99, (int)((GuessVerificationProgress(chainParams.TxData(), pindex) - dProgressStart) / (dProgressTip - dProgressStart) * 100))));
            }
            if (GetTime() >= nNow + 60) {
                nNow = GetTime();
                LogPrintf("Still rescanning. At block %d. Progress=%f\n", pindex->nHeight, GuessVerificationProgress(chainParams.TxData(), pindex));
            }
            ENTER_CRITICAL_SECTION(cs_main)
            ENTER_CRITICAL_SECTION(cs_wallet)

            if (ShutdownRequested())
                return ret;

            CBlock block;
            if (ReadBlockFromDisk(block, pindex, Params().GetConsensus())) {
                for (size_t posInBlock = 0; posInBlock < block.vtx.size(); ++posInBlock) {
                    AddToWalletIfInvolvingMe(block.vtx[posInBlock], pindex, posInBlock, fUpdate);
                }
            } else {
                ret = pindex;
            }
            pindex = chainActive.Next(pindex);
        }
        if (pindex && fAbortRescan) {
            LogPrintf("Rescan aborted at block %d. Progress=%f\n", pindex->nHeight, GuessVerificationProgress(chainParams.TxData(), pindex));
        }
        ShowProgress(_("Rescanning..."), 100); // hide progress dialog in GUI

        fScanningWallet = false;
    }
    return ret;
}

void CWallet::ReacceptWalletTransactions()
{
    // If transactions aren't being broadcasted, don't let them into local mempool either
    if (!fBroadcastTransactions)
        return;
    LOCK2(cs_main, cs_wallet);
    std::map<int64_t, CWalletTx*> mapSorted;

    // Sort pending wallet transactions based on their initial wallet insertion order
    BOOST_FOREACH(PAIRTYPE(const uint256, CWalletTx)& item, mapWallet)
    {
        const uint256& wtxid = item.first;
        CWalletTx& wtx = item.second;
        assert(wtx.GetHash() == wtxid);

        int nDepth = wtx.GetDepthInMainChain();

        if (!wtx.IsCoinBase() && (nDepth == 0 && !wtx.isAbandoned())) {
            mapSorted.insert(std::make_pair(wtx.nOrderPos, &wtx));
        }
    }

    // Try to add wallet transactions to memory pool
    BOOST_FOREACH(PAIRTYPE(const int64_t, CWalletTx*)& item, mapSorted)
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

CAmount CWalletTx::GetDebit(const isminefilter& filter, CAccount* forAccount) const
{
    if (tx->vin.empty())
        return 0;

    CAmount debit = 0;
    if(filter & ISMINE_SPENDABLE)
    {
        if (debitCached.find(forAccount) != debitCached.end())
            debit += debitCached[forAccount];
        else
        {
            debitCached[forAccount] = pwallet->GetDebit(*this, ISMINE_SPENDABLE, forAccount);
            debit += debitCached[forAccount];
        }
    }
    if(filter & ISMINE_WATCH_ONLY)
    {
        if (watchDebitCached.find(forAccount) != watchDebitCached.end())
            debit += watchDebitCached[forAccount];
        else
        {
            watchDebitCached[forAccount] = pwallet->GetDebit(*this, ISMINE_WATCH_ONLY, forAccount);
            debit += watchDebitCached[forAccount];
        }
    }
    return debit;
}

CAmount CWalletTx::GetCredit(const isminefilter& filter, CAccount* forAccount) const
{
    // Must wait until coinbase is safely deep enough in the chain before valuing it
    if (IsCoinBase() && GetBlocksToMaturity() > 0)
        return 0;

    CAmount credit = 0;
    if (filter & ISMINE_SPENDABLE)
    {
        // GetBalance can assume transactions in mapWallet won't change
        if (creditCached.find(forAccount) != creditCached.end())
            credit += creditCached[forAccount];
        else
        {
            creditCached[forAccount] = pwallet->GetCredit(*this, ISMINE_SPENDABLE, forAccount);
            credit += creditCached[forAccount];
        }
    }
    if (filter & ISMINE_WATCH_ONLY)
    {
        if (watchCreditCached.find(forAccount) != watchCreditCached.end())
            credit += watchCreditCached[forAccount];
        else
        {
            watchCreditCached[forAccount] = pwallet->GetCredit(*this, ISMINE_WATCH_ONLY, forAccount);
            credit += watchCreditCached[forAccount];
        }
    }
    return credit;
}

CAmount CWalletTx::GetImmatureCredit(bool fUseCache, const CAccount* forAccount) const
{
    if (IsCoinBase() && GetBlocksToMaturity() > 0 && IsInMainChain())
    {
        if (fUseCache && immatureCreditCached.find(forAccount) != immatureCreditCached.end())
            return immatureCreditCached[forAccount];
        immatureCreditCached[forAccount] = pwallet->GetCredit(*this, ISMINE_SPENDABLE);
        return immatureCreditCached[forAccount];
    }

    return 0;
}

CAmount CWalletTx::GetAvailableCredit(bool fUseCache, const CAccount* forAccount) const
{
    if (pwallet == 0)
        return 0;

    // Must wait until coinbase is safely deep enough in the chain before valuing it
    if (IsCoinBase() && GetBlocksToMaturity() > 0)
        return 0;

    if (fUseCache && availableCreditCached.find(forAccount) != availableCreditCached.end())
        return availableCreditCached[forAccount];

    CAmount nCredit = 0;
    uint256 hashTx = GetHash();
    for (unsigned int i = 0; i < tx->vout.size(); i++)
    {
        if (!pwallet->IsSpent(hashTx, i))
        {
            const CTxOut &txout = tx->vout[i];
            if (!forAccount || IsMine(*forAccount, txout))
            {
                if (!IsPoW2WitnessLocked(txout, chainActive.Tip()->nHeight))
                {
                    nCredit += pwallet->GetCredit(txout, ISMINE_SPENDABLE);
                    if (!MoneyRange(nCredit))
                        throw std::runtime_error("CWalletTx::GetAvailableCredit() : value out of range");
                }
            }
        }
    }

    availableCreditCached[forAccount] = nCredit;

    return nCredit;
}

CAmount CWalletTx::GetAvailableCreditIncludingLockedWitnesses(bool fUseCache, const CAccount* forAccount) const
{
    if (pwallet == 0)
        return 0;

    // Must wait until coinbase is safely deep enough in the chain before valuing it
    if (IsCoinBase() && GetBlocksToMaturity() > 0)
        return 0;

    if (fUseCache && availableCreditCachedIncludingLockedWitnesses.find(forAccount) != availableCreditCachedIncludingLockedWitnesses.end())
        return availableCreditCachedIncludingLockedWitnesses[forAccount];

    CAmount nCredit = 0;
    uint256 hashTx = GetHash();
    for (unsigned int i = 0; i < tx->vout.size(); i++)
    {
        if (!pwallet->IsSpent(hashTx, i))
        {
            const CTxOut &txout = tx->vout[i];
            if (!forAccount || IsMine(*forAccount, txout))
            {
                nCredit += pwallet->GetCredit(txout, ISMINE_SPENDABLE);
                if (!MoneyRange(nCredit))
                    throw std::runtime_error("CWalletTx::GetAvailableCredit() : value out of range");
            }
        }
    }

    availableCreditCachedIncludingLockedWitnesses[forAccount] = nCredit;

    return nCredit;
}

CAmount CWalletTx::GetImmatureWatchOnlyCredit(const bool& fUseCache, const CAccount* forAccount) const
{
    if (IsCoinBase() && GetBlocksToMaturity() > 0 && IsInMainChain())
    {
        if (fUseCache && immatureWatchCreditCached.find(forAccount) != immatureWatchCreditCached.end())
            return immatureWatchCreditCached[forAccount];
        immatureWatchCreditCached[forAccount] = pwallet->GetCredit(*this, ISMINE_WATCH_ONLY);
        return immatureWatchCreditCached[forAccount];
    }

    return 0;
}

CAmount CWalletTx::GetAvailableWatchOnlyCredit(const bool& fUseCache, const CAccount* forAccount) const
{
    if (pwallet == 0)
        return 0;

    // Must wait until coinbase is safely deep enough in the chain before valuing it
    if (IsCoinBase() && GetBlocksToMaturity() > 0)
        return 0;

    if (fUseCache && availableWatchCreditCached.find(forAccount) != availableWatchCreditCached.end())
        return availableWatchCreditCached[forAccount];

    CAmount nCredit = 0;
    for (unsigned int i = 0; i < tx->vout.size(); i++)
    {
        if (!pwallet->IsSpent(GetHash(), i))
        {
            const CTxOut &txout = tx->vout[i];
            nCredit += pwallet->GetCredit(txout, ISMINE_WATCH_ONLY);
            if (!MoneyRange(nCredit))
                throw std::runtime_error("CWalletTx::GetAvailableCredit() : value out of range");
        }
    }

    availableWatchCreditCached[forAccount] = nCredit;
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
    return mempool.exists(GetHash());
}

bool CWalletTx::IsTrusted() const
{
    // Quick answer in most cases
    if (!CheckFinalTx(*this))
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
    BOOST_FOREACH(const CTxIn& txin, tx->vin)
    {
        // Transactions not sent by us: not trusted
        const CWalletTx* parent = pwallet->GetWalletTx(txin.prevout.hash);
        if (parent == NULL)
            return false;
        const CTxOut& parentOut = parent->tx->vout[txin.prevout.n];
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
    BOOST_FOREACH(PAIRTYPE(const uint256, CWalletTx)& item, mapWallet)
    {
        CWalletTx& wtx = item.second;
        // Don't rebroadcast if newer than nTime:
        if (wtx.nTimeReceived > nTime)
            continue;
        mapSorted.insert(std::make_pair(wtx.nTimeReceived, &wtx));
    }
    BOOST_FOREACH(PAIRTYPE(const unsigned int, CWalletTx*)& item, mapSorted)
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

extern bool IsMine(const CAccount* forAccount, const CWalletTx& tx);

CAmount CWallet::GetBalance(const CAccount* forAccount, bool includePoW2LockedWitnesses, bool includeChildren) const
{
    CAmount nTotal = 0;
    {
        LOCK2(cs_main, cs_wallet);
        for (std::map<uint256, CWalletTx>::const_iterator it = mapWallet.begin(); it != mapWallet.end(); ++it)
        {
            const CWalletTx* pcoin = &(*it).second;
            //fixme: GULDEN (FUT) - is this okay? Should it be cached or something? (CBSU?)
            if (!forAccount || ::IsMine(forAccount, *pcoin))
            {
                if (pcoin->IsTrusted() && !pcoin->isAbandoned())
                {
                    nTotal += includePoW2LockedWitnesses ? pcoin->GetAvailableCreditIncludingLockedWitnesses(true, forAccount) : pcoin->GetAvailableCredit(true, forAccount);
                }
            }

        }
    }
    if (forAccount && includeChildren)
    {
        for (const auto& accountItem : mapAccounts)
        {
            const auto& childAccount = accountItem.second;
            if (childAccount->getParentUUID() == forAccount->getUUID())
            {
                nTotal += GetBalance(childAccount, includePoW2LockedWitnesses, false);
            }
        }
    }

    return nTotal;
}

CAmount CWallet::GetUnconfirmedBalance(const CAccount* forAccount, bool includeChildren) const
{
    CAmount nTotal = 0;
    {
        LOCK2(cs_main, cs_wallet);
        for (std::map<uint256, CWalletTx>::const_iterator it = mapWallet.begin(); it != mapWallet.end(); ++it)
        {
            const CWalletTx* pcoin = &(*it).second;
            //fixme: GULDEN (FUT) (1.6.1) - is this okay? Should it be cached or something? (CBSU?)
            if (!forAccount || ::IsMine(forAccount, *pcoin))
            {
                if (!pcoin->IsTrusted() && pcoin->GetDepthInMainChain() == 0 && pcoin->InMempool())
                    nTotal += pcoin->GetAvailableCredit(true, forAccount);
            }
        }
    }
    if (forAccount && includeChildren)
    {
        for (const auto& accountItem : mapAccounts)
        {
            const auto& childAccount = accountItem.second;
            if (childAccount->getParentUUID() == forAccount->getUUID())
            {
                nTotal += GetUnconfirmedBalance(childAccount, false);
            }
        }
    }
    return nTotal;
}

CAmount CWallet::GetImmatureBalance(const CAccount* forAccount) const
{
    CAmount nTotal = 0;
    {
        LOCK2(cs_main, cs_wallet);
        for (std::map<uint256, CWalletTx>::const_iterator it = mapWallet.begin(); it != mapWallet.end(); ++it)
        {
            const CWalletTx* pcoin = &(*it).second;
            if (!forAccount || ::IsMine(forAccount, *pcoin))
            {
                nTotal += pcoin->GetImmatureCredit(true, forAccount);
            }
        }
    }
    return nTotal;
}

CAmount CWallet::GetWatchOnlyBalance() const
{
    CAmount nTotal = 0;
    {
        LOCK2(cs_main, cs_wallet);
        for (std::map<uint256, CWalletTx>::const_iterator it = mapWallet.begin(); it != mapWallet.end(); ++it)
        {
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
        for (std::map<uint256, CWalletTx>::const_iterator it = mapWallet.begin(); it != mapWallet.end(); ++it)
        {
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
        for (std::map<uint256, CWalletTx>::const_iterator it = mapWallet.begin(); it != mapWallet.end(); ++it)
        {
            const CWalletTx* pcoin = &(*it).second;
            nTotal += pcoin->GetImmatureWatchOnlyCredit();
        }
    }
    return nTotal;
}

// Calculate total balance in a different way from GetBalance. The biggest
// difference is that GetBalance sums up all unspent TxOuts paying to the
// wallet, while this sums up both spent and unspent TxOuts paying to the
// wallet, and then subtracts the values of TxIns spending from the wallet. This
// also has fewer restrictions on which unconfirmed transactions are considered
// trusted.
CAmount CWallet::GetLegacyBalance(const isminefilter& filter, int minDepth, const boost::uuids::uuid* accountUUID) const
{
    LOCK2(cs_main, cs_wallet);

    CAccount* forAccount = NULL;
    if (accountUUID && mapAccounts.find(*accountUUID) != mapAccounts.end())
        forAccount = mapAccounts.find(*accountUUID)->second;

    CAmount balance = 0;


    //checkme: (GULDEN) - Is fee handled right?
    for (const auto& entry : mapWallet) {
        const CWalletTx& wtx = entry.second;
        const int depth = wtx.GetDepthInMainChain();
        if (depth < 0 || !CheckFinalTx(*wtx.tx) || wtx.GetBlocksToMaturity() > 0 || wtx.isAbandoned()) {
            continue;
        }

        if (depth >= minDepth)
        {
            balance += wtx.GetCredit(filter, forAccount);
            balance -= wtx.GetDebit(filter, forAccount);
        }
    }

    return balance;
}

//fixme: (MERGE)
CAmount CWallet::GetAvailableBalance(CAccount* forAccount, const CCoinControl* coinControl) const
{
    LOCK2(cs_main, cs_wallet);

    CAmount balance = 0;
    std::vector<COutput> vCoins;
    AvailableCoins(forAccount, vCoins, true, coinControl);
    for (const COutput& out : vCoins) {
        if (out.fSpendable) {
            balance += out.tx->tx->vout[out.i].nValue;
        }
    }
    return balance;
}

void CWallet::AvailableCoins(CAccount* forAccount, std::vector<COutput> &vCoins, bool fOnlySafe, const CCoinControl *coinControl, const CAmount &nMinimumAmount, const CAmount &nMaximumAmount, const CAmount &nMinimumSumAmount, const uint64_t &nMaximumCount, const int &nMinDepth, const int &nMaxDepth) const
{
    vCoins.clear();

    {
        LOCK2(cs_main, cs_wallet);

        CAmount nTotal = 0;

        for (std::map<uint256, CWalletTx>::const_iterator it = mapWallet.begin(); it != mapWallet.end(); ++it)
        {
            const uint256& wtxid = it->first;
            const CWalletTx* pcoin = &(*it).second;

            if (!::IsMine(forAccount, *pcoin))
                continue;

            if (!CheckFinalTx(*pcoin))
                continue;

            if (pcoin->IsCoinBase() && pcoin->GetBlocksToMaturity() > 0)
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

                if (IsSpent(wtxid, i))
                    continue;

                isminetype mine = ::IsMine(*forAccount, pcoin->tx->vout[i]);

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
    for (auto& coin : availableCoins) {
        CTxDestination address;
        if (coin.fSpendable &&
            ExtractDestination(FindNonChangeParentOutput(*coin.tx->tx, coin.i), address)) {
            result[address].emplace_back(std::move(coin));
        }
    }

    std::vector<COutPoint> lockedCoins;
    ListLockedCoins(lockedCoins);
    for (const auto& output : lockedCoins) {
        auto it = mapWallet.find(output.hash);
        if (it != mapWallet.end()) {
            int depth = it->second.GetDepthInMainChain();
            if (depth >= 0 && output.n < it->second.tx->vout.size() &&
                IsMine(it->second.tx->vout[output.n]) == ISMINE_SPENDABLE) {
                CTxDestination address;
                if (ExtractDestination(FindNonChangeParentOutput(*it->second.tx, output.n), address)) {
                    result[address].emplace_back(
                        &it->second, output.n, depth, true /* spendable */, true /* solvable */, false /* safe */);
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
    while (IsChange(ptx->vout[n]) && ptx->vin.size() > 0) {
        const COutPoint& prevout = ptx->vin[0].prevout;
        auto it = mapWallet.find(prevout.hash);
        if (it == mapWallet.end() || it->second.tx->vout.size() <= prevout.n ||
            !IsMine(it->second.tx->vout[prevout.n])) {
            break;
        }
        ptx = it->second.tx.get();
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
                                 std::set<CInputCoin>& setCoinsRet, CAmount& nValueRet) const
{
    setCoinsRet.clear();
    nValueRet = 0;

    // List of values less than target
    boost::optional<CInputCoin> coinLowestLarger;
    std::vector<CInputCoin> vValue;
    CAmount nTotalLower = 0;

    random_shuffle(vCoins.begin(), vCoins.end(), GetRandInt);

    BOOST_FOREACH(const COutput &output, vCoins)
    {
        if (!output.fSpendable)
            continue;

        const CWalletTx *pcoin = output.tx;

        if (output.nDepth < (pcoin->IsFromMe(ISMINE_ALL) ? nConfMine : nConfTheirs))
            continue;

        if (!mempool.TransactionWithinChainLimit(pcoin->GetHash(), nMaxAncestors))
            continue;

        int i = output.i;

        CInputCoin coin = CInputCoin(pcoin, i);

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
        ApproximateBestSubset(vValue, nTotalLower, nTargetValue + MIN_CHANGE, vfBest, nBest);

    // If we have a bigger coin and (either the stochastic approximation didn't find a good solution,
    //                                   or the next bigger coin is closer), return the bigger coin
    if (coinLowestLarger &&
        ((nBest != nTargetValue && nBest < nTargetValue + MIN_CHANGE) || coinLowestLarger->txout.nValue <= nBest))
    {
        setCoinsRet.insert(coinLowestLarger.get());
        nValueRet += coinLowestLarger->txout.nValue;
    }
    else {
        for (unsigned int i = 0; i < vValue.size(); i++)
            if (vfBest[i])
            {
                setCoinsRet.insert(vValue[i]);
                nValueRet += vValue[i].txout.nValue;
            }

        if (LogAcceptCategory(BCLog::SELECTCOINS)) {
            LogPrint(BCLog::SELECTCOINS, "SelectCoins() best subset: ");
            for (unsigned int i = 0; i < vValue.size(); i++) {
                if (vfBest[i]) {
                    LogPrint(BCLog::SELECTCOINS, "%s ", FormatMoney(vValue[i].txout.nValue));
                }
            }
            LogPrint(BCLog::SELECTCOINS, "total %s\n", FormatMoney(nBest));
        }
    }

    return true;
}

bool CWallet::SelectCoins(const std::vector<COutput>& vAvailableCoins, const CAmount& nTargetValue, std::set<CInputCoin>& setCoinsRet, CAmount& nValueRet, const CCoinControl* coinControl) const
{
    std::vector<COutput> vCoins(vAvailableCoins);

    // coin control -> return all selected outputs (we want all selected to go into the transaction for sure)
    if (coinControl && coinControl->HasSelected() && !coinControl->fAllowOtherInputs)
    {
        BOOST_FOREACH(const COutput& out, vCoins)
        {
            if (!out.fSpendable)
                 continue;
            nValueRet += out.tx->tx->vout[out.i].nValue;
            setCoinsRet.insert(CInputCoin(out.tx, out.i));
        }
        return (nValueRet >= nTargetValue);
    }

    // calculate value from preset inputs and store them
    std::set<CInputCoin> setPresetCoins;
    CAmount nValueFromPresetInputs = 0;

    std::vector<COutPoint> vPresetInputs;
    if (coinControl)
        coinControl->ListSelected(vPresetInputs);
    BOOST_FOREACH(const COutPoint& outpoint, vPresetInputs)
    {
        std::map<uint256, CWalletTx>::const_iterator it = mapWallet.find(outpoint.hash);
        if (it != mapWallet.end())
        {
            const CWalletTx* pcoin = &it->second;
            // Clearly invalid input, fail
            if (pcoin->tx->vout.size() <= outpoint.n)
                return false;
            nValueFromPresetInputs += pcoin->tx->vout[outpoint.n].nValue;
            setPresetCoins.insert(CInputCoin(pcoin, outpoint.n));
        } else
            return false; // TODO: Allow non-wallet inputs
    }

    // remove preset inputs from vCoins
    for (std::vector<COutput>::iterator it = vCoins.begin(); it != vCoins.end() && coinControl && coinControl->HasSelected();)
    {
        if (setPresetCoins.count(CInputCoin(it->tx, it->i)))
            it = vCoins.erase(it);
        else
            ++it;
    }

    size_t nMaxChainLength = std::min(GetArg("-limitancestorcount", DEFAULT_ANCESTOR_LIMIT), GetArg("-limitdescendantcount", DEFAULT_DESCENDANT_LIMIT));
    bool fRejectLongChains = GetBoolArg("-walletrejectlongchains", DEFAULT_WALLET_REJECT_LONG_CHAINS);

    bool res = nTargetValue <= nValueFromPresetInputs ||
        SelectCoinsMinConf(nTargetValue - nValueFromPresetInputs, 1, 6, 0, vCoins, setCoinsRet, nValueRet) ||
        SelectCoinsMinConf(nTargetValue - nValueFromPresetInputs, 1, 1, 0, vCoins, setCoinsRet, nValueRet) ||
        (bSpendZeroConfChange && SelectCoinsMinConf(nTargetValue - nValueFromPresetInputs, 0, 1, 2, vCoins, setCoinsRet, nValueRet)) ||
        (bSpendZeroConfChange && SelectCoinsMinConf(nTargetValue - nValueFromPresetInputs, 0, 1, std::min((size_t)4, nMaxChainLength/3), vCoins, setCoinsRet, nValueRet)) ||
        (bSpendZeroConfChange && SelectCoinsMinConf(nTargetValue - nValueFromPresetInputs, 0, 1, nMaxChainLength/2, vCoins, setCoinsRet, nValueRet)) ||
        (bSpendZeroConfChange && SelectCoinsMinConf(nTargetValue - nValueFromPresetInputs, 0, 1, nMaxChainLength, vCoins, setCoinsRet, nValueRet)) ||
        (bSpendZeroConfChange && !fRejectLongChains && SelectCoinsMinConf(nTargetValue - nValueFromPresetInputs, 0, 1, std::numeric_limits<uint64_t>::max(), vCoins, setCoinsRet, nValueRet));

    // because SelectCoinsMinConf clears the setCoinsRet, we now add the possible inputs to the coinset
    setCoinsRet.insert(setPresetCoins.begin(), setPresetCoins.end());

    // add preset inputs to the total value selected
    nValueRet += nValueFromPresetInputs;

    return res;
}

bool CWallet::SignTransaction(CAccount* fromAccount, CMutableTransaction &tx, SignType type)
{
    AssertLockHeld(cs_wallet); // mapWallet

    // sign the new tx
    CTransaction txNewConst(tx);
    int nIn = 0;
    for (const auto& input : tx.vin) {
        std::map<uint256, CWalletTx>::const_iterator mi = mapWallet.find(input.prevout.hash);
        if(mi == mapWallet.end() || input.prevout.n >= mi->second.tx->vout.size()) {
            return false;
        }
        const CAmount& amount = mi->second.tx->vout[input.prevout.n].nValue;
        //fixme: (GULDEN) (HIGH) (sign type)
        CKeyID signingKeyID = ExtractSigningPubkeyFromTxOutput(mi->second.tx->vout[input.prevout.n], SignType::Spend);
        SignatureData sigdata;
        if (!ProduceSignature(TransactionSignatureCreator(signingKeyID, fromAccount, &txNewConst, nIn, amount, SIGHASH_ALL), mi->second.tx->vout[input.prevout.n], sigdata, type, txNewConst.nVersion)) {
            return false;
        }
        UpdateTransaction(tx, nIn, sigdata);
        nIn++;
    }
    return true;
}

CAccount* CWallet::FindAccountForTransaction(const CTxOut& out)
{
    for (const auto& accountItem : mapAccounts)
    {
        CAccount* childAccount = accountItem.second;
        if (::IsMine(*childAccount, out) == ISMINE_SPENDABLE)
        {
            return childAccount;
        }
    }
    return NULL;
}

bool CWallet::FundTransaction(CAccount* fromAccount, CMutableTransaction& tx, CAmount& nFeeRet, int& nChangePosInOut, std::string& strFailReason, bool lockUnspents, const std::set<int>& setSubtractFeeFromOutputs, CCoinControl coinControl, bool keepReserveKey)
{
    std::vector<CRecipient> vecSend;

    // Turn the txout set into a CRecipient vector
    for (size_t idx = 0; idx < tx.vout.size(); idx++)
    {
        const CTxOut& txOut = tx.vout[idx];
        CRecipient recipient = GetRecipientForTxOut(txOut, txOut.nValue, setSubtractFeeFromOutputs.count(idx) == 1);
        vecSend.push_back(recipient);
    }

    coinControl.fAllowOtherInputs = true;

    BOOST_FOREACH(const CTxIn& txin, tx.vin)
        coinControl.Select(txin.prevout);

    CReserveKey reservekey(this, fromAccount, KEYCHAIN_CHANGE);
    CWalletTx wtx;
    if (!CreateTransaction(fromAccount, vecSend, wtx, reservekey, nFeeRet, nChangePosInOut, strFailReason, &coinControl, false))
        return false;

    if (nChangePosInOut != -1)
        tx.vout.insert(tx.vout.begin() + nChangePosInOut, wtx.tx->vout[nChangePosInOut]);

    // Copy output sizes from new transaction; they may have had the fee subtracted from them
    for (unsigned int idx = 0; idx < tx.vout.size(); idx++)
        tx.vout[idx].nValue = wtx.tx->vout[idx].nValue;

    // Add new txins (keeping original txin scriptSig/order)
    BOOST_FOREACH(const CTxIn& txin, wtx.tx->vin)
    {
        if (!coinControl.IsSelected(txin.prevout))
        {
            tx.vin.push_back(txin);

            if (lockUnspents)
            {
              LOCK2(cs_main, cs_wallet);
              LockCoin(txin.prevout);
            }
        }
    }

    // optionally keep the change output key
    if (keepReserveKey)
        reservekey.KeepKey();

    return true;
}


void CWallet::AddTxInput(CMutableTransaction& tx, const CInputCoin& inputCoin, bool rbf)
{
    std::set<CInputCoin> setCoins;
    setCoins.insert(inputCoin);
    AddTxInputs(tx, setCoins, rbf);
}

void CWallet::AddTxInputs(CMutableTransaction& tx, std::set<CInputCoin>& setCoins, bool rbf)
{
    uint8_t nFlags = 0;
    uint32_t nSequence = 0;
    if (tx.nVersion < CTransaction::SEGSIG_ACTIVATION_VERSION)
    {
        if (rbf)
        {
            // Use sequence number to signal RBF
            nSequence = MAX_BIP125_RBF_SEQUENCE;
        }
        else if(tx.nLockTime == 0)
        {
            // Use sequence number to signal locktime
            nSequence = std::numeric_limits<unsigned int>::max() - 1;
        }
        else
        {
            // Final sequence number
            nSequence = std::numeric_limits<unsigned int>::max();
        }
    }
    else
    {
        if (rbf)
        {
            nFlags |= CTxInFlags::OptInRBF;
        }
        else if(tx.nLockTime == 0)
        {
            //fixme: (GULDEN) (2.0) - Do we have to set relative lock time on the inputs?
            //Whats the relationship between relative and absolute locktime?
            //nFlags |= CTxInFlags::OptInRBF;
        }
        else
        {
            // Final sequence number
            nSequence = std::numeric_limits<unsigned int>::max();
        }
    }
    for (const auto& coin : setCoins)
        tx.vin.push_back(CTxIn(coin.outpoint,CScript(), nSequence, nFlags));
}

bool CWallet::CreateTransaction(CAccount* forAccount, const std::vector<CRecipient>& vecSend, CWalletTx& wtxNew, CReserveKey& reservekey, CAmount& nFeeRet,
                                int& nChangePosInOut, std::string& strFailReason, const CCoinControl* coinControl, bool sign)
{
    if (forAccount->IsReadOnly())
    {
        strFailReason = _("Can't send from read only (watch) account.");
        return false;
    }

    CAmount nValue = 0;
    int nChangePosRequest = nChangePosInOut;
    unsigned int nSubtractFeeFromAmount = 0;
    for (const auto& recipient : vecSend)
    {
        if (nValue < 0 || recipient.nAmount < 0)
        {
            strFailReason = _("Transaction amounts must not be negative");
            return false;
        }
        nValue += recipient.nAmount;

        if (recipient.fSubtractFeeFromAmount)
            nSubtractFeeFromAmount++;
    }
    if (vecSend.empty())
    {
        strFailReason = _("Transaction must have at least one recipient");
        return false;
    }

    wtxNew.fTimeReceivedIsTxTime = true;
    wtxNew.BindWallet(this);
    CMutableTransaction txNew(CURRENT_TX_VERSION_POW2);

    // Discourage fee sniping.
    //
    // For a large miner the value of the transactions in the best block and
    // the mempool can exceed the cost of deliberately attempting to mine two
    // blocks to orphan the current best block. By setting nLockTime such that
    // only the next block can include the transaction, we discourage this
    // practice as the height restricted and limited blocksize gives miners
    // considering fee sniping fewer options for pulling off this attack.
    //
    // A simple way to think about this is from the wallet's point of view we
    // always want the blockchain to move forward. By setting nLockTime this
    // way we're basically making the statement that we only want this
    // transaction to appear in the next block; we don't want to potentially
    // encourage reorgs by allowing transactions to appear at lower heights
    // than the next block in forks of the best chain.
    //
    // Of course, the subsidy is high enough, and transaction volume low
    // enough, that fee sniping isn't a problem yet, but by implementing a fix
    // now we ensure code won't be written that makes assumptions about
    // nLockTime that preclude a fix later.
    if (GetRandInt(10) == 0)//Gulden - we only set this on 10% of blocks to avoid unnecessary space wastage. //fixme: (GULDEN) (FUT) (only set this for high fee [per byte] transactions?)
    txNew.nLockTime = chainActive.Height();

    // Secondly occasionally randomly pick a nLockTime even further back, so
    // that transactions that are delayed after signing for whatever reason,
    // e.g. high-latency mix networks and some CoinJoin implementations, have
    // better privacy.
    if (GetRandInt(100) == 0)
        txNew.nLockTime = std::max(0, (int)txNew.nLockTime - GetRandInt(100));

    assert(txNew.nLockTime <= (unsigned int)chainActive.Height());
    assert(txNew.nLockTime < LOCKTIME_THRESHOLD);

    {
        std::set<CInputCoin> setCoins;
        LOCK2(cs_main, cs_wallet);
        {
            std::vector<COutput> vAvailableCoins;
            AvailableCoins(forAccount, vAvailableCoins, true, coinControl);

            nFeeRet = 0;
            // Start with no fee and loop until there is enough fee
            while (true)
            {
                nChangePosInOut = nChangePosRequest;
                txNew.vin.clear();
                txNew.vout.clear();
                wtxNew.fFromMe = true;
                bool fFirst = true;

                CAmount nValueToSelect = nValue;
                if (nSubtractFeeFromAmount == 0)
                    nValueToSelect += nFeeRet;
                // vouts to the payees
                for (const auto& recipient : vecSend)
                {
                    CTxOut txout = recipient.GetTxOut();

                    if (recipient.fSubtractFeeFromAmount)
                    {
                        txout.nValue -= nFeeRet / nSubtractFeeFromAmount; // Subtract fee equally from each selected recipient

                        if (fFirst) // first receiver pays the remainder not divisible by output count
                        {
                            fFirst = false;
                            txout.nValue -= nFeeRet % nSubtractFeeFromAmount;
                        }
                    }

                    if (IsDust(txout, ::dustRelayFee))
                    {
                        if (recipient.fSubtractFeeFromAmount && nFeeRet > 0)
                        {
                            if (txout.nValue < 0)
                                strFailReason = _("The transaction amount is too small to pay the fee");
                            else
                                strFailReason = _("The transaction amount is too small to send after the fee has been deducted");
                        }
                        else
                            strFailReason = _("Transaction amount too small");
                        return false;
                    }
                    txNew.vout.push_back(txout);
                }

                // Choose coins to use
                CAmount nValueIn = 0;
                setCoins.clear();
                //fixme: GULDEN HIGH ACCOUNTS - ensure this only selects from forAccount - in theory it should because AvailableCoins is doing a forAccount check?
                if (!SelectCoins(vAvailableCoins, nValueToSelect, setCoins, nValueIn, coinControl))
                {
                    strFailReason = _("Insufficient funds");
                    return false;
                }

                const CAmount nChange = nValueIn - nValueToSelect;
                if (nChange > 0)
                {
                    std::shared_ptr<CTxOut> newTxOut = nullptr;
                    if (txNew.nVersion >= CTransaction::SEGSIG_ACTIVATION_VERSION)
                    {
                        //fixme: (GULDEN) (COINCONTROL) - coin control could still produce script in this instance.
                        // Reserve a new key pair from key pool
                        CPubKey vchPubKey;
                        bool ret;
                        ret = reservekey.GetReservedKey(vchPubKey);
                        if (!ret)
                        {
                            strFailReason = _("Keypool ran out, please call keypoolrefill first");
                            return false;
                        }

                        newTxOut = std::make_shared<CTxOut>(CTxOut(nChange, vchPubKey.GetID()));
                    }
                    else
                    {
                        // Fill a vout to ourself
                        // TODO: pass in scriptChange instead of reservekey so
                        // change transaction isn't always pay-to-Gulden-address
                        CScript scriptChange;

                        // coin control: send change to custom address
                        if (coinControl && !boost::get<CNoDestination>(&coinControl->destChange))
                            scriptChange = GetScriptForDestination(coinControl->destChange);

                        // no coin control: send change to newly generated address
                        else
                        {
                            // Note: We use a new key here to keep it from being obvious which side is the change.
                            //  The drawback is that by not reusing a previous key, the change may be lost if a
                            //  backup is restored, if the backup doesn't have the new private key for the change.
                            //  If we reused the old key, it would be possible to add code to look for and
                            //  rediscover unknown transactions that were written with keys of ours to recover
                            //  post-backup change.

                            // Reserve a new key pair from key pool
                            CPubKey vchPubKey;
                            bool ret;
                            ret = reservekey.GetReservedKey(vchPubKey);
                            if (!ret)
                            {
                                strFailReason = _("Keypool ran out, please call keypoolrefill first");
                                return false;
                            }

                            scriptChange = GetScriptForDestination(vchPubKey.GetID());
                        }
                        newTxOut = std::make_shared<CTxOut>(CTxOut(nChange, scriptChange));
                    }

                    // We do not move dust-change to fees, because the sender would end up paying more than requested.
                    // This would be against the purpose of the all-inclusive feature.
                    // So instead we raise the change and deduct from the recipient.
                    if (nSubtractFeeFromAmount > 0 && IsDust(*newTxOut, ::dustRelayFee))
                    {
                        CAmount nDust = GetDustThreshold(*newTxOut, ::dustRelayFee) - newTxOut->nValue;
                        newTxOut->nValue += nDust; // raise change until no more dust
                        for (unsigned int i = 0; i < vecSend.size(); i++) // subtract from first recipient
                        {
                            if (vecSend[i].fSubtractFeeFromAmount)
                            {
                                txNew.vout[i].nValue -= nDust;
                                if (IsDust(txNew.vout[i], ::dustRelayFee))
                                {
                                    strFailReason = _("The transaction amount is too small to send after the fee has been deducted");
                                    return false;
                                }
                                break;
                            }
                        }
                    }

                    // Never create dust outputs; if we would, just
                    // add the dust to the fee.
                    if (IsDust(*newTxOut, ::dustRelayFee))
                    {
                        nChangePosInOut = -1;
                        nFeeRet += nChange;
                        reservekey.ReturnKey();
                    }
                    else
                    {
                        if (nChangePosInOut == -1)
                        {
                            // Insert change txn at random position:
                            nChangePosInOut = GetRandInt(txNew.vout.size()+1);
                        }
                        else if ((unsigned int)nChangePosInOut > txNew.vout.size())
                        {
                            strFailReason = _("Change index out of range");
                            return false;
                        }

                        std::vector<CTxOut>::iterator position = txNew.vout.begin()+nChangePosInOut;
                        txNew.vout.insert(position, *newTxOut);
                    }
                } else {
                    reservekey.ReturnKey();
                    nChangePosInOut = -1;
                }

                // Fill vin
                //
                // Note how the sequence number is set to non-maxint so that
                // the nLockTime set above actually works.
                //
                // BIP125 defines opt-in RBF as any nSequence < maxint-1, so
                // we use the highest possible value in that range (maxint-2)
                // to avoid conflicting with other possible uses of nSequence,
                // and in the spirit of "smallest possible change from prior
                // behavior."
                bool rbf = coinControl ? coinControl->signalRbf : fWalletRbf;
                AddTxInputs(txNew, setCoins , rbf);

                // Fill in dummy signatures for fee calculation.
                if (!DummySignTx(forAccount, txNew, setCoins, Spend)) {
                    SignatureData sigdata;
                    //fixme: (GULDEN) (2.0) HIGHNEXT ensure this still works.
                    if (!ProduceSignature(DummySignatureCreator(forAccount), CTxOut(), sigdata, Spend, txNew.nVersion))
                    {
                        strFailReason = _("Signing transaction failed");
                        return false;
                    }
                }

                unsigned int nBytes = GetVirtualTransactionSize(txNew);

                CTransaction txNewConst(txNew);

                // Remove scriptSigs to eliminate the fee calculation dummy signatures
                for (auto& vin : txNew.vin) {
                    vin.scriptSig = CScript();
                    vin.scriptWitness.SetNull();
                }

                // Allow to override the default confirmation target over the CoinControl instance
                int currentConfirmationTarget = nTxConfirmTarget;
                if (coinControl && coinControl->nConfirmTarget > 0)
                    currentConfirmationTarget = coinControl->nConfirmTarget;

                CAmount nFeeNeeded = GetMinimumFee(nBytes, currentConfirmationTarget, ::mempool, ::feeEstimator);
                if (coinControl && coinControl->fOverrideFeeRate)
                    nFeeNeeded = coinControl->nFeeRate.GetFee(nBytes);

                // If we made it here and we aren't even able to meet the relay fee on the next pass, give up
                // because we must be at the maximum allowed fee.
                if (nFeeNeeded < ::minRelayTxFee.GetFee(nBytes))
                {
                    strFailReason = _("Transaction too large for fee policy");
                    return false;
                }

                if (nFeeRet >= nFeeNeeded) {
                    // Reduce fee to only the needed amount if we have change
                    // output to increase.  This prevents potential overpayment
                    // in fees if the coins selected to meet nFeeNeeded result
                    // in a transaction that requires less fee than the prior
                    // iteration.
                    // TODO: The case where nSubtractFeeFromAmount > 0 remains
                    // to be addressed because it requires returning the fee to
                    // the payees and not the change output.
                    // TODO: The case where there is no change output remains
                    // to be addressed so we avoid creating too small an output.
                    if (nFeeRet > nFeeNeeded && nChangePosInOut != -1 && nSubtractFeeFromAmount == 0) {
                        CAmount extraFeePaid = nFeeRet - nFeeNeeded;
                        std::vector<CTxOut>::iterator change_position = txNew.vout.begin()+nChangePosInOut;
                        change_position->nValue += extraFeePaid;
                        nFeeRet -= extraFeePaid;
                    }
                    break; // Done, enough fee included.
                }

                // Try to reduce change to include necessary fee
                if (nChangePosInOut != -1 && nSubtractFeeFromAmount == 0) {
                    CAmount additionalFeeNeeded = nFeeNeeded - nFeeRet;
                    std::vector<CTxOut>::iterator change_position = txNew.vout.begin()+nChangePosInOut;
                    // Only reduce change if remaining amount is still a large enough output.
                    if (change_position->nValue >= MIN_FINAL_CHANGE + additionalFeeNeeded) {
                        change_position->nValue -= additionalFeeNeeded;
                        nFeeRet += additionalFeeNeeded;
                        break; // Done, able to increase fee from change
                    }
                }

                // Include more fee and try again.
                nFeeRet = nFeeNeeded;
                continue;
            }
        }

        if (sign)
        {
            CTransaction txNewConst(txNew);
            int nIn = 0;
            for (const auto& coin : setCoins)
            {
                //const CScript& scriptPubKey = coin.txout.scriptPubKey;
                SignatureData sigdata;

                //fixme: (GULDEN) (HIGH) (sign type)
                CKeyID signingKeyID = ExtractSigningPubkeyFromTxOutput(coin.txout, SignType::Spend);

                if (!ProduceSignature(TransactionSignatureCreator(signingKeyID, forAccount, &txNewConst, nIn, coin.txout.nValue, SIGHASH_ALL),  coin.txout, sigdata, Spend, txNewConst.nVersion))
                {
                    strFailReason = _("Signing transaction failed");
                    return false;
                } else {
                    UpdateTransaction(txNew, nIn, sigdata);
                }

                nIn++;
            }
        }

        // Embed the constructed transaction data in wtxNew.
        wtxNew.SetTx(MakeTransactionRef(std::move(txNew)));

        // Limit size
        if (GetTransactionWeight(wtxNew) >= MAX_STANDARD_TX_WEIGHT)
        {
            strFailReason = _("Transaction too large");
            return false;
        }
    }

    if (GetBoolArg("-walletrejectlongchains", DEFAULT_WALLET_REJECT_LONG_CHAINS)) {
        // Lastly, ensure this tx will pass the mempool's chain limits
        LockPoints lp;
        CTxMemPoolEntry entry(wtxNew.tx, 0, 0, 0, false, 0, lp);
        CTxMemPool::setEntries setAncestors;
        size_t nLimitAncestors = GetArg("-limitancestorcount", DEFAULT_ANCESTOR_LIMIT);
        size_t nLimitAncestorSize = GetArg("-limitancestorsize", DEFAULT_ANCESTOR_SIZE_LIMIT)*1000;
        size_t nLimitDescendants = GetArg("-limitdescendantcount", DEFAULT_DESCENDANT_LIMIT);
        size_t nLimitDescendantSize = GetArg("-limitdescendantsize", DEFAULT_DESCENDANT_SIZE_LIMIT)*1000;
        std::string errString;
        if (!mempool.CalculateMemPoolAncestors(entry, setAncestors, nLimitAncestors, nLimitAncestorSize, nLimitDescendants, nLimitDescendantSize, errString)) {
            strFailReason = _("Transaction has too long of a mempool chain");
            return false;
        }
    }
    return true;
}

/**
 * Call after CreateTransaction unless you want to abort
 */
bool CWallet::CommitTransaction(CWalletTx& wtxNew, CReserveKey& reservekey, CConnman* connman, CValidationState& state)
{
    {
        LOCK2(cs_main, cs_wallet);
        LogPrintf("CommitTransaction:\n%s", wtxNew.tx->ToString());
        {
            // Take key pair from key pool so it won't be used again
            reservekey.KeepKey();

            // Add tx to wallet, because if it has change it's also ours,
            // otherwise just for transaction history.
            AddToWallet(wtxNew);

            // Notify that old coins are spent
            BOOST_FOREACH(const CTxIn& txin, wtxNew.tx->vin)
            {
                CWalletTx &coin = mapWallet[txin.prevout.hash];
                coin.BindWallet(this);
                NotifyTransactionChanged(this, coin.GetHash(), CT_UPDATED);
            }
        }

        // Track how many getdata requests our transaction gets
        mapRequestCount[wtxNew.GetHash()] = 0;

        if (fBroadcastTransactions)
        {
            // Broadcast
            if (!wtxNew.AcceptToMemoryPool(maxTxFee, state)) {
                LogPrintf("CommitTransaction(): Transaction cannot be broadcast immediately, %s\n", state.GetRejectReason());
                // TODO: if we expect the failure to be long term or permanent, instead delete wtx from the wallet and return failure.
            } else {
                wtxNew.RelayWalletTransaction(connman);
            }
        }
    }
    return true;
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
    wtxOrdered.insert(std::make_pair(entry.nOrderPos, TxPair((CWalletTx*)0, &entry)));

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




DBErrors CWallet::LoadWallet(WalletLoadState& nExtraLoadState)
{
    nExtraLoadState = NEW_WALLET;
    DBErrors nLoadWalletRet = CWalletDB(*dbw,"cr+").LoadWallet(this, nExtraLoadState);
    if (nLoadWalletRet == DB_NEED_REWRITE)
    {
        if (dbw->Rewrite("\x04pool"))
        {
            LOCK(cs_wallet);
            //fixme: (GULDEN) (FUT) (1.6.1)
            for (auto accountPair : mapAccounts)
            {
                accountPair.second->setKeyPoolInternal.clear();
                accountPair.second->setKeyPoolExternal.clear();
            }
            // Note: can't top-up keypool here, because wallet is locked.
            // User will be prompted to unlock wallet the next operation
            // that requires a new key.
        }
    }

    if (nLoadWalletRet != DB_LOAD_OK)
        return nLoadWalletRet;

    uiInterface.LoadWallet(this);

    return DB_LOAD_OK;
}

DBErrors CWallet::ZapSelectTx(std::vector<uint256>& vHashIn, std::vector<uint256>& vHashOut)
{
    AssertLockHeld(cs_wallet); // mapWallet
    //vchDefaultKey = CPubKey();//GULDEN - no default key.
    DBErrors nZapSelectTxRet = CWalletDB(*dbw,"cr+").ZapSelectTx(vHashIn, vHashOut);
    for (uint256 hash : vHashOut)
        mapWallet.erase(hash);

    if (nZapSelectTxRet == DB_NEED_REWRITE)
    {
        if (dbw->Rewrite("\x04pool"))
        {
            //fixme: (GULDEN) (FUT) (1.6.1)
            for (auto accountPair : mapAccounts)
            {
                accountPair.second->setKeyPoolInternal.clear();
                accountPair.second->setKeyPoolExternal.clear();
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
            //fixme: (GULDEN) (FUT) (1.6.1)
            for (auto accountPair : mapAccounts)
            {
                accountPair.second->setKeyPoolInternal.clear();
                accountPair.second->setKeyPoolExternal.clear();
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


bool CWallet::SetAddressBook(const std::string& address, const std::string& strName, const std::string& strPurpose)
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
                             strPurpose, (fUpdated ? CT_UPDATED : CT_NEW) );
    if (!strPurpose.empty() && !CWalletDB(*dbw).WritePurpose(address, strPurpose))
        return false;
    return CWalletDB(*dbw).WriteName(address, strName);
}

bool CWallet::DelAddressBook(const std::string& address)
{
    {
        LOCK(cs_wallet); // mapAddressBook

        // Delete destdata tuples associated with address
        BOOST_FOREACH(const PAIRTYPE(std::string, std::string) &item, mapAddressBook[address].destdata)
        {
            CWalletDB(*dbw).EraseDestData(address, item.first);
        }
        mapAddressBook.erase(address);
    }

    NotifyAddressBookChanged(this, address, "", ::IsMine(*this, CBitcoinAddress(address).Get()) != ISMINE_NO, "", CT_DELETED);

    CWalletDB(*dbw).ErasePurpose(CBitcoinAddress(address).ToString());
    return CWalletDB(*dbw).EraseName(CBitcoinAddress(address).ToString());
}

/* GULDEN - no default key.
bool CWallet::SetDefaultKey(const CPubKey &vchPubKey)
{
    if (!CWalletDB(*dbw).WriteDefaultKey(vchPubKey))
        return false;
    vchDefaultKey = vchPubKey;
    return true;
}
*/

/**
 * Mark old keypool keys as used,
 * and generate all new keys
 */
bool CWallet::NewKeyPool()
{
    LogPrintf("keypool - newkeypool");
    {
        LOCK(cs_wallet);
        CWalletDB walletdb(*dbw);

        for (auto accountPair : mapAccounts)
        {
            if(!accountPair.second->IsHD())
            {
                BOOST_FOREACH(int64_t nIndex, accountPair.second->setKeyPoolInternal)
                    walletdb.ErasePool(this, nIndex);
                BOOST_FOREACH(int64_t nIndex, accountPair.second->setKeyPoolExternal)
                    walletdb.ErasePool(this, nIndex);

                accountPair.second->setKeyPoolInternal.clear();
                accountPair.second->setKeyPoolExternal.clear();
            }
        }

        if (IsLocked())
            return false;

        //Nothing else to do - the shadow thread will take care of the rest.
    }
    return true;
}

//fixme: (FUT)) GULDEN Note for HD this should actually care more about maintaining a gap above the last used address than it should about the size of the pool.
int CWallet::TopUpKeyPool(unsigned int nTargetKeypoolSize, unsigned int nMaxNewAllocations, CAccount* forAccount)
{
    unsigned int nNew = 0;
    {
        LOCK(cs_wallet);

        if (IsLocked())
            return -1;

        CWalletDB walletdb(*dbw);

        // Top up key pool
        unsigned int nTargetSize;
        if (nTargetKeypoolSize > 0)
            nTargetSize = nTargetKeypoolSize;
        else
            nTargetSize = GetArg("-keypool", 5);

        //Find current unique highest key index across *all* keypools.
        int64_t nIndex = 1;
        for (auto accountPair : mapAccounts)
        {
            for (auto keyChain : { KEYCHAIN_EXTERNAL, KEYCHAIN_CHANGE })
            {
                auto& keyPool = ( keyChain == KEYCHAIN_EXTERNAL ? accountPair.second->setKeyPoolExternal : accountPair.second->setKeyPoolInternal );
                if (!keyPool.empty())
                    nIndex = std::max( nIndex, *(--keyPool.end()) + 1 );
            }
        }

        for (auto accountPair : mapAccounts)
        {
            if (forAccount == nullptr || forAccount->getUUID() == accountPair.first)
            {
                unsigned int accountTargetSize = nTargetSize;
                for (auto& keyChain : { KEYCHAIN_EXTERNAL, KEYCHAIN_CHANGE })
                {
                    auto& keyPool = ( keyChain == KEYCHAIN_EXTERNAL ? accountPair.second->setKeyPoolExternal : accountPair.second->setKeyPoolInternal );
                    while (keyPool.size() < (accountTargetSize))
                    {
                        if (!walletdb.WritePool( ++nIndex, CKeyPool(GenerateNewKey(*accountPair.second, keyChain), getUUIDAsString(accountPair.first), keyChain ) ) )
                            throw std::runtime_error(std::string(__func__) + ": writing generated key failed");
                        keyPool.insert(nIndex);
                        LogPrintf("keypool [%s:%s] added key %d, size=%u\n", accountPair.second->getLabel(), (keyChain == KEYCHAIN_CHANGE ? "change" : "external"), nIndex, keyPool.size());

                        // Limit generation for this loop - rest will be generated later
                        ++nNew;
                        if (nMaxNewAllocations != 0 && nNew >= nMaxNewAllocations)
                            return nNew;
                    }
                }
            }
        }
    }
    return nNew;
}

void CWallet::ReserveKeyFromKeyPool(int64_t& nIndex, CKeyPool& keypoolentry, CAccount* forAccount, int64_t keyChain)
{
    nIndex = -1;
    keypoolentry.vchPubKey = CPubKey();
    {
        LOCK(cs_wallet);

        if (!IsLocked())
            TopUpKeyPool(1, 0, forAccount);//Only assign the bare minimum here, let the background thread do the rest.

        auto& keyPool = ( keyChain == KEYCHAIN_EXTERNAL ? forAccount->setKeyPoolExternal : forAccount->setKeyPoolInternal );

        // Get the oldest key
        if(keyPool.empty())
            return;

        CWalletDB walletdb(*dbw);

        nIndex = *(keyPool.begin());
        keyPool.erase(keyPool.begin());
        if (!walletdb.ReadPool(nIndex, keypoolentry))
            throw std::runtime_error(std::string(__func__) + ": read failed");
        if (!forAccount->HaveKey(keypoolentry.vchPubKey.GetID()))
            throw std::runtime_error(std::string(__func__) + ": unknown key in key pool");
        assert(keypoolentry.vchPubKey.IsValid());
        LogPrintf("keypool reserve %d\n", nIndex);
    }
}

void CWallet::KeepKey(int64_t nIndex)
{
    // Remove from key pool
    CWalletDB walletdb(*dbw);
        walletdb.ErasePool(this, nIndex);
    LogPrintf("keypool keep %d\n", nIndex);
}

//fixme: GULDEN - We should handle this MarkKeyUsed case better, have it broadcast an event to all reserve keys or something.
//And then remove the disk check below
void CWallet::ReturnKey(int64_t nIndex, CAccount* forAccount, int64_t keyChain)
{
    // Return to key pool - but only if it hasn't been used in the interim (If MarkKeyUsed has removed it from disk in the meantime then we don't return it)
    CKeyPool keypoolentry;

    CWalletDB walletdb(*dbw);
    if (!walletdb.ReadPool(nIndex, keypoolentry))
    {
        LogPrintf("keypool return - aborted as key already used %d\n", nIndex);
        return;
    }

    {
        LOCK(cs_wallet);
        auto& keyPool = ( keyChain == KEYCHAIN_EXTERNAL ? forAccount->setKeyPoolExternal : forAccount->setKeyPoolInternal );
        keyPool.insert(nIndex);
    }
    LogPrintf("keypool return %d\n", nIndex);
}

bool CWallet::GetKeyFromPool(CPubKey& result, CAccount* forAccount, int64_t keyChain)
{
    CKeyPool keypool;
    {
        LOCK(cs_wallet);
        int64_t nIndex = 0;
        ReserveKeyFromKeyPool(nIndex, keypool, forAccount, keyChain);
        if (nIndex == -1)
        {
            if (IsLocked()) return false;
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


    // load oldest key from keypool, get time and return
    CKeyPool keypoolentry;
    CWalletDB walletdb(*dbw);

    // if the keypool is empty, return <NOW>
    int64_t nTime = GetTime();
    for (const auto& accountItem : mapAccounts)
    {
        for (auto keyChain : { KEYCHAIN_EXTERNAL, KEYCHAIN_CHANGE })
        {
            const auto& keyPool = ( keyChain == KEYCHAIN_EXTERNAL ? accountItem.second->setKeyPoolExternal : accountItem.second->setKeyPoolInternal );
            if(!keyPool.empty())
            {
                int64_t nIndex = *(keyPool.begin());
                if (!walletdb.ReadPool(nIndex, keypoolentry))
                throw std::runtime_error(std::string(__func__) + ": read oldest key in keypool failed");
                assert(keypoolentry.vchPubKey.IsValid());
                nTime = std::min(nTime, keypoolentry.nTime);
            }
        }
    }
    return nTime;
}

std::map<CTxDestination, CAmount> CWallet::GetAddressBalances()
{
    std::map<CTxDestination, CAmount> balances;

    {
        LOCK(cs_wallet);
        for (const auto& walletEntry : mapWallet)
        {
            const CWalletTx *pcoin = &walletEntry.second;

            if (!pcoin->IsTrusted())
                continue;

            if (pcoin->IsCoinBase() && pcoin->GetBlocksToMaturity() > 0)
                continue;

            int nDepth = pcoin->GetDepthInMainChain();
            if (nDepth < (pcoin->IsFromMe(ISMINE_ALL) ? 0 : 1))
                continue;

            for (unsigned int i = 0; i < pcoin->tx->vout.size(); i++)
            {
                CTxDestination addr;
                if (!IsMine(pcoin->tx->vout[i]))
                    continue;
                if(!ExtractDestination(pcoin->tx->vout[i], addr))
                    continue;

                CAmount n = IsSpent(walletEntry.first, i) ? 0 : pcoin->tx->vout[i].nValue;

                if (!balances.count(addr))
                    balances[addr] = 0;
                balances[addr] += n;
            }
        }
    }

    return balances;
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
            BOOST_FOREACH(CTxIn txin, pcoin->tx->vin)
            {
                CTxDestination address;
                if(!IsMine(txin)) /* If this input isn't mine, ignore it */
                    continue;
                if(!ExtractDestination(mapWallet[txin.prevout.hash].tx->vout[txin.prevout.n], address))
                    continue;
                grouping.insert(address);
                any_mine = true;
            }

            // group change with input addresses
            if (any_mine)
            {
               BOOST_FOREACH(CTxOut txout, pcoin->tx->vout)
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
    BOOST_FOREACH(std::set<CTxDestination> _grouping, groupings)
    {
        // make a set of all the groups hit by this new group
        std::set< std::set<CTxDestination>* > hits;
        std::map< CTxDestination, std::set<CTxDestination>* >::iterator it;
        BOOST_FOREACH(CTxDestination address, _grouping)
            if ((it = setmap.find(address)) != setmap.end())
                hits.insert((*it).second);

        // merge all hit groups into a new single group and delete old groups
        std::set<CTxDestination>* merged = new std::set<CTxDestination>(_grouping);
        BOOST_FOREACH(std::set<CTxDestination>* hit, hits)
        {
            merged->insert(hit->begin(), hit->end());
            uniqueGroupings.erase(hit);
            delete hit;
        }
        uniqueGroupings.insert(merged);

        // update setmap
        BOOST_FOREACH(CTxDestination element, *merged)
            setmap[element] = merged;
    }

    std::set< std::set<CTxDestination> > ret;
    BOOST_FOREACH(std::set<CTxDestination>* uniqueGrouping, uniqueGroupings)
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
    BOOST_FOREACH(const PAIRTYPE(std::string, CAddressBookData)& item, mapAddressBook)
    {
        const std::string& address = item.first;
        const std::string& strName = item.second.name;
        if (strName == strAccount)
            result.insert(CBitcoinAddress(address).Get());
    }
    return result;
}

bool CReserveKey::GetReservedKey(CPubKey& pubkey)
{
    if (nIndex == -1)
    {
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

void CWallet::GetAllReserveKeys(std::set<CKeyID>& setAddress) const
{
    setAddress.clear();

    CWalletDB walletdb(*dbw);

    LOCK2(cs_main, cs_wallet);
    for (const auto& accountItem : mapAccounts)
    {
        for (auto keyChain : { KEYCHAIN_EXTERNAL, KEYCHAIN_CHANGE })
        {
            const auto& keyPool = ( keyChain == KEYCHAIN_EXTERNAL ? accountItem.second->setKeyPoolExternal : accountItem.second->setKeyPoolInternal );
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

void CWallet::GetScriptForMining(std::shared_ptr<CReserveScript> &script, CAccount* forAccount)
{
    //fixme: (GULDEN) (FUT) - Allow defaultmining account to be seperately selected?
    std::shared_ptr<CReserveKey> rKey;
    if (forAccount)
    {
        rKey = std::make_shared<CReserveKey>(this, forAccount, KEYCHAIN_EXTERNAL);
    }
    else
    {
        rKey = std::make_shared<CReserveKey>(this, activeAccount, KEYCHAIN_EXTERNAL);
    }
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

void CWallet::ListLockedCoins(std::vector<COutPoint>& vOutpts) const
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
    const CKeyStore &keystore;
    std::vector<CKeyID> &vKeys;

public:
    CAffectedKeysVisitor(const CKeyStore &keystoreIn, std::vector<CKeyID> &vKeysIn) : keystore(keystoreIn), vKeys(vKeysIn) {}

    void Process(const CTxOut &out) {
        txnouttype type;
        std::vector<CTxDestination> vDest;
        int nRequired;
        if (ExtractDestinations(out, type, vDest, nRequired)) {
            BOOST_FOREACH(const CTxDestination &dest, vDest)
                boost::apply_visitor(*this, dest);
        }
    }

    void operator()(const CKeyID &keyId) {
        if (keystore.HaveKey(keyId))
            vKeys.push_back(keyId);
    }

    void operator()(const CScriptID &scriptId) {
        //fixme: (GULDEN) (2.0)
        /*
        CScript script;
        if (keystore.GetCScript(scriptId, script))
            Process(script);
        */
    }

    void operator()(const CPoW2WitnessDestination &dest) {
        if (keystore.HaveKey(dest.witnessKey))
            vKeys.push_back(dest.witnessKey);
        if (keystore.HaveKey(dest.spendingKey))
            vKeys.push_back(dest.spendingKey);
    }

    void operator()(const CNoDestination &none) {}
};

void CWallet::GetKeyBirthTimes(std::map<CKeyID, int64_t> &mapKeyBirth) const {
    LOCK(cs_wallet);

    AssertLockHeld(cs_wallet); // mapKeyMetadata
    mapKeyBirth.clear();

    // get birth times for keys with metadata
    for (std::map<CKeyID, CKeyMetadata>::const_iterator it = mapKeyMetadata.begin(); it != mapKeyMetadata.end(); it++)
        if (it->second.nCreateTime)
            mapKeyBirth[it->first] = it->second.nCreateTime;

    // map in which we'll infer heights of other keys
    CBlockIndex *pindexMax = chainActive[std::max(0, chainActive.Height() - 144)]; // the tip can be reorganized; use a 144-block safety margin
    std::map<CKeyID, CBlockIndex*> mapKeyFirstBlock;
    std::set<CKeyID> setKeys;
    for (const auto accountPair : mapAccounts)
    {
        accountPair.second->GetKeys(setKeys);
        for(const auto keyid : setKeys)
        {
            if (mapKeyBirth.count(keyid) == 0)
                mapKeyFirstBlock[keyid] = pindexMax;
        }
        setKeys.clear();
    }

    // if there are no such keys, we're done
    if (mapKeyFirstBlock.empty())
        return;

    // find first block that affects those keys, if there are any left
    std::vector<CKeyID> vAffected;
    for (std::map<uint256, CWalletTx>::const_iterator it = mapWallet.begin(); it != mapWallet.end(); it++) {
        // iterate over all wallet transactions...
        const CWalletTx &wtx = (*it).second;
        BlockMap::const_iterator blit = mapBlockIndex.find(wtx.hashBlock);
        if (blit != mapBlockIndex.end() && chainActive.Contains(blit->second)) {
            // ... which are already in a block
            int nHeight = blit->second->nHeight;
            BOOST_FOREACH(const CTxOut &txout, wtx.tx->vout) {
                // iterate over all their outputs
                //fixme: (GULDEN) (FUT) (1.6.1)
                CAffectedKeysVisitor(activeAccount->externalKeyStore, vAffected).Process(txout);
                BOOST_FOREACH(const CKeyID &keyid, vAffected) {
                    // ... and all their affected keys
                    std::map<CKeyID, CBlockIndex*>::iterator rit = mapKeyFirstBlock.find(keyid);
                    if (rit != mapKeyFirstBlock.end() && nHeight < rit->second->nHeight)
                        rit->second = blit->second;
                }
                vAffected.clear();
            }
        }
    }

    // Extract block timestamps for those keys
    for (std::map<CKeyID, CBlockIndex*>::const_iterator it = mapKeyFirstBlock.begin(); it != mapKeyFirstBlock.end(); it++)
        mapKeyBirth[it->first] = it->second->GetBlockTime() - TIMESTAMP_WINDOW; // block times can be 2h off
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
 * - If receiving a block with a past timestamp, before the most recent known
 *   transaction (that we care about), assign all its (not already known)
 *   transactions' timestamps to the same timestamp as that most-recent-known
 *   transaction.
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
            int64_t latestEntry = 0;

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
                    latestEntry = nSmartTime;
                    if (nSmartTime > latestNow) {
                        latestNow = nSmartTime;
                    }
                    break;
                }
            }

            int64_t blocktime = mapBlockIndex[wtx.hashBlock]->GetBlockTime();
            nTimeSmart = std::max(latestEntry, std::min(blocktime, latestNow));
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

    mapAddressBook[CBitcoinAddress(dest).ToString()].destdata.insert(std::make_pair(key, value));
    return CWalletDB(*dbw).WriteDestData(CBitcoinAddress(dest).ToString(), key, value);
}

bool CWallet::EraseDestData(const CTxDestination &dest, const std::string &key)
{
    if (!mapAddressBook[CBitcoinAddress(dest).ToString()].destdata.erase(key))
        return false;
    return CWalletDB(*dbw).EraseDestData(CBitcoinAddress(dest).ToString(), key);
}

bool CWallet::LoadDestData(const CTxDestination &dest, const std::string &key, const std::string &value)
{
    mapAddressBook[CBitcoinAddress(dest).ToString()].destdata.insert(std::make_pair(key, value));
    return true;
}

bool CWallet::GetDestData(const CTxDestination &dest, const std::string &key, std::string *value) const
{
    std::map<std::string, CAddressBookData>::const_iterator i = mapAddressBook.find(CBitcoinAddress(dest).ToString());
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

std::string CWallet::GetWalletHelpString(bool showDebug)
{
    std::string strUsage = HelpMessageGroup(_("Wallet options:"));
    strUsage += HelpMessageOpt("-disablewallet", _("Do not load the wallet and disable wallet RPC calls"));
    strUsage += HelpMessageOpt("-keypool=<n>", strprintf(_("Set key pool size to <n> (default: %u)"), DEFAULT_KEYPOOL_SIZE));
    strUsage += HelpMessageOpt("-accountpool=<n>", strprintf(_("Set account pool size to <n> (default: %u)"), 10));
    strUsage += HelpMessageOpt("-fallbackfee=<amt>", strprintf(_("A fee rate (in %s/kB) that will be used when fee estimation has insufficient data (default: %s)"),
                                                               CURRENCY_UNIT, FormatMoney(DEFAULT_FALLBACK_FEE)));
    strUsage += HelpMessageOpt("-mintxfee=<amt>", strprintf(_("Fees (in %s/kB) smaller than this are considered zero fee for transaction creation (default: %s)"),
                                                            CURRENCY_UNIT, FormatMoney(DEFAULT_TRANSACTION_MINFEE)));
    strUsage += HelpMessageOpt("-paytxfee=<amt>", strprintf(_("Fee (in %s/kB) to add to transactions you send (default: %s)"),
                                                            CURRENCY_UNIT, FormatMoney(payTxFee.GetFeePerK())));
    strUsage += HelpMessageOpt("-rescan", _("Rescan the block chain for missing wallet transactions on startup"));
    strUsage += HelpMessageOpt("-salvagewallet", _("Attempt to recover private keys from a corrupt wallet on startup"));
    strUsage += HelpMessageOpt("-spendzeroconfchange", strprintf(_("Spend unconfirmed change when sending transactions (default: %u)"), DEFAULT_SPEND_ZEROCONF_CHANGE));
    strUsage += HelpMessageOpt("-txconfirmtarget=<n>", strprintf(_("If paytxfee is not set, include enough fee so transactions begin confirmation on average within n blocks (default: %u)"), DEFAULT_TX_CONFIRM_TARGET));
    strUsage += HelpMessageOpt("-usehd", _("Use hierarchical deterministic key generation (HD) after BIP32. Only has effect during wallet creation/first start") + " " + strprintf(_("(default: %u)"), DEFAULT_USE_HD_WALLET));
    strUsage += HelpMessageOpt("-walletrbf", strprintf(_("Send transactions with full-RBF opt-in enabled (default: %u)"), DEFAULT_WALLET_RBF));
    strUsage += HelpMessageOpt("-upgradewallet", _("Upgrade wallet to latest format on startup"));
    strUsage += HelpMessageOpt("-wallet=<file>", _("Specify wallet file (within data directory)") + " " + strprintf(_("(default: %s)"), DEFAULT_WALLET_DAT));
    strUsage += HelpMessageOpt("-walletbroadcast", _("Make the wallet broadcast transactions") + " " + strprintf(_("(default: %u)"), DEFAULT_WALLETBROADCAST));
    strUsage += HelpMessageOpt("-walletnotify=<cmd>", _("Execute command when a wallet transaction changes (%s in cmd is replaced by TxID)"));
    strUsage += HelpMessageOpt("-zapwallettxes=<mode>", _("Delete all wallet transactions and only recover those parts of the blockchain through -rescan on startup") +
                               " " + _("(1 = keep tx meta data e.g. account owner and payment request information, 2 = drop tx meta data)"));

    if (showDebug)
    {
        strUsage += HelpMessageGroup(_("Wallet debugging/testing options:"));

        strUsage += HelpMessageOpt("-dblogsize=<n>", strprintf("Flush wallet database activity from memory to disk log every <n> megabytes (default: %u)", DEFAULT_WALLET_DBLOGSIZE));
        strUsage += HelpMessageOpt("-flushwallet", strprintf("Run a thread to flush wallet periodically (default: %u)", DEFAULT_FLUSHWALLET));
        strUsage += HelpMessageOpt("-privdb", strprintf("Sets the DB_PRIVATE flag in the wallet db environment (default: %u)", DEFAULT_WALLET_PRIVDB));
        strUsage += HelpMessageOpt("-walletrejectlongchains", strprintf(_("Wallet will not create transactions that violate mempool chain limits (default: %u)"), DEFAULT_WALLET_REJECT_LONG_CHAINS));
    }

    return strUsage;
}

CWallet* CWallet::CreateWalletFromFile(const std::string walletFile)
{
    // needed to restore wallet transaction meta data after -zapwallettxes
    std::vector<CWalletTx> vWtx;

    if (GetBoolArg("-zapwallettxes", false)) {
        uiInterface.InitMessage(_("Zapping all transactions from wallet..."));

        std::unique_ptr<CWalletDBWrapper> dbw(new CWalletDBWrapper(&bitdb, walletFile));
        CWallet *tempWallet = new CWallet(std::move(dbw));
        DBErrors nZapWalletRet = tempWallet->ZapWalletTx(vWtx);
        if (nZapWalletRet != DB_LOAD_OK) {
            InitError(strprintf(_("Error loading %s: Wallet corrupted"), walletFile));
            return NULL;
        }

        delete tempWallet;
        tempWallet = NULL;
    }

    uiInterface.InitMessage(_("Loading wallet..."));

    int64_t nStart = GetTimeMillis();
    WalletLoadState loadState = NEW_WALLET;
    std::unique_ptr<CWalletDBWrapper> dbw(new CWalletDBWrapper(&bitdb, walletFile));
    CWallet *walletInstance = new CWallet(std::move(dbw));
    DBErrors nLoadWalletRet = walletInstance->LoadWallet(loadState);
    if (nLoadWalletRet != DB_LOAD_OK)
    {
        if (nLoadWalletRet == DB_CORRUPT) {
            InitError(strprintf(_("Error loading %s: Wallet corrupted"), walletFile));
            return NULL;
        }
        else if (nLoadWalletRet == DB_NONCRITICAL_ERROR)
        {
            InitWarning(strprintf(_("Error reading %s! All keys read correctly, but transaction data"
                                         " or address book entries might be missing or incorrect."),
                walletFile));
        }
        else if (nLoadWalletRet == DB_TOO_NEW) {
            InitError(strprintf(_("Error loading %s: Wallet requires newer version of %s"), walletFile, _(PACKAGE_NAME)));
            return NULL;
        }
        else if (nLoadWalletRet == DB_NEED_REWRITE)
        {
            InitError(strprintf(_("Wallet needed to be rewritten: restart %s to complete"), _(PACKAGE_NAME)));
            return NULL;
        }
        else {
            InitError(strprintf(_("Error loading %s"), walletFile));
            return NULL;
        }
    }

    if (GetBoolArg("-upgradewallet", loadState == NEW_WALLET))
    {
        int nMaxVersion = GetArg("-upgradewallet", 0);
        if (nMaxVersion == 0) // the -upgradewallet without argument case
        {
            LogPrintf("Performing wallet upgrade to %i\n", FEATURE_LATEST);
            nMaxVersion = CLIENT_VERSION;
            walletInstance->SetMinVersion(FEATURE_LATEST); // permanently upgrade the wallet immediately
        }
        else
            LogPrintf("Allowing wallet upgrade up to %i\n", nMaxVersion);
        if (nMaxVersion < walletInstance->GetVersion())
        {
            InitError(_("Cannot downgrade wallet"));
            return NULL;
        }
        walletInstance->SetMaxVersion(nMaxVersion);
    }

    if (loadState == NEW_WALLET)
    {
        // Create new keyUser and set as default key
        if (GetBoolArg("-usehd", DEFAULT_USE_HD_WALLET))
        {
            if (fNoUI)
            {
                std::vector<unsigned char> entropy(16);
                GetStrongRandBytes(&entropy[0], 16);
                GuldenApplication::gApp->setRecoveryPhrase(mnemonicFromEntropy(entropy, entropy.size()*8));
            }

            if (GuldenApplication::gApp->getRecoveryPhrase().size() == 0)
            {
                //Work around an issue with "non HD" wallets from older versions where active account may not be set in the wallet.
                if (!walletInstance->mapAccounts.empty())
                    walletInstance->setActiveAccount(walletInstance->mapAccounts.begin()->second);
                throw std::runtime_error("Invalid seed mnemonic");
            }

            // Generate a new primary seed and account (BIP44)
            walletInstance->activeSeed = new CHDSeed(GuldenApplication::gApp->getRecoveryPhrase().c_str(), CHDSeed::CHDSeed::BIP44);
            if (!CWalletDB(*walletInstance->dbw).WriteHDSeed(*walletInstance->activeSeed))
            {
                throw std::runtime_error("Writing seed failed");
            }
            walletInstance->mapSeeds[walletInstance->activeSeed->getUUID()] = walletInstance->activeSeed;
            walletInstance->activeAccount = walletInstance->GenerateNewAccount("My account", AccountType::Normal, AccountSubType::Desktop);

            // Now generate children shadow accounts to handle legacy transactions
            // Only for recovery wallets though, new ones don't need them
            if (GuldenApplication::gApp->isRecovery)
            {
                CHDSeed* seedBip32 = new CHDSeed(GuldenApplication::gApp->getRecoveryPhrase().c_str(), CHDSeed::CHDSeed::BIP32);
                if (!CWalletDB(*walletInstance->dbw).WriteHDSeed(*seedBip32))
                {
                    throw std::runtime_error("Writing bip32 seed failed");
                }
                CHDSeed* seedBip32Legacy = new CHDSeed(GuldenApplication::gApp->getRecoveryPhrase().c_str(), CHDSeed::CHDSeed::BIP32Legacy);
                if (!CWalletDB(*walletInstance->dbw).WriteHDSeed(*seedBip32Legacy))
                {
                    throw std::runtime_error("Writing bip32 legacy seed failed");
                }
                walletInstance->mapSeeds[seedBip32->getUUID()] = seedBip32;
                walletInstance->mapSeeds[seedBip32Legacy->getUUID()] = seedBip32Legacy;

                // Write new accounts
                CAccountHD* newAccountBip32 = seedBip32->GenerateAccount(AccountSubType::Desktop, NULL);
                newAccountBip32->m_Type = AccountType::ShadowChild;
                walletInstance->activeAccount->AddChild(newAccountBip32);
                walletInstance->addAccount(newAccountBip32, "BIP32 child account");

                // Write new accounts
                CAccountHD* newAccountBip32Legacy = seedBip32Legacy->GenerateAccount(AccountSubType::Desktop, NULL);
                newAccountBip32Legacy->m_Type = AccountType::ShadowChild;
                walletInstance->activeAccount->AddChild(newAccountBip32Legacy);
                walletInstance->addAccount(newAccountBip32Legacy, "BIP32 legacy child account");
            }

            // Write the seed last so that account index changes are reflected
            {
                CWalletDB walletdb(*walletInstance->dbw);
                walletdb.WritePrimarySeed(*walletInstance->activeSeed);
                walletdb.WritePrimaryAccount(walletInstance->activeAccount);
            }

            GuldenApplication::gApp->BurnRecoveryPhrase();

            //Assign the bare minimum keys here, let the rest take place in the background thread
            walletInstance->TopUpKeyPool(1);
        }
        else
        {
            walletInstance->activeAccount = new CAccount();
            walletInstance->activeAccount->m_Type = AccountType::Normal;
            walletInstance->activeAccount->m_SubType = AccountSubType::Desktop;

            // Write the primary account into wallet file
            {
                CWalletDB walletdb(*walletInstance->dbw);
                if (!walletdb.WriteAccount(getUUIDAsString(walletInstance->activeAccount->getUUID()), walletInstance->activeAccount))
                {
                    throw std::runtime_error("Writing legacy account failed");
                }
                walletdb.WritePrimaryAccount(walletInstance->activeAccount);
            }

            //Assign the bare minimum keys here, let the rest take place in the bakcground thread
            walletInstance->TopUpKeyPool(1);
        }

        pactiveWallet = walletInstance;
        walletInstance->SetBestChain(chainActive.GetLocatorPoW2());

        //fixme: (GULDEN) (MERGE)
        CWalletDB walletdb(*walletInstance->dbw);
    }
    else if (loadState == EXISTING_WALLET_OLDACCOUNTSYSTEM)
    {
        // HD upgrade.
        // Only perform upgrade if usehd is present (default for UI)
        // For daemon we force users to choose (for exchanges etc.)
        if (fNoUI)
        {
            if (!walletInstance->activeAccount->IsHD() && !walletInstance->activeSeed)
            {
                if (IsArgSet("-usehd"))
                {
                    throw std::runtime_error("Must specify -usehd=1 or -usehd=0, in order to allow or refuse HD upgrade.");
                }
            }
        }

        if (GetBoolArg("-usehd", DEFAULT_USE_HD_WALLET))
        {
            if (!walletInstance->activeAccount->IsHD() && !walletInstance->activeSeed)
            {
                while (true)
                {
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

                    //Force old legacy account to resave
                    {
                        CWalletDB walletdb(*walletInstance->dbw);
                        walletInstance->changeAccountName(walletInstance->activeAccount, _("Legacy account"), true);
                        if (!walletdb.WriteAccount(getUUIDAsString(walletInstance->activeAccount->getUUID()), walletInstance->activeAccount))
                        {
                            throw std::runtime_error("Writing legacy account failed");
                        }
                        if (walletWasCrypted && !walletInstance->activeAccount->internalKeyStore.IsCrypted())
                        {
                            walletInstance->activeAccount->internalKeyStore.SetCrypted();
                            walletInstance->activeAccount->internalKeyStore.Unlock(walletInstance->activeAccount->vMasterKey);
                        }
                        walletInstance->ForceRewriteKeys(*walletInstance->activeAccount);
                    }

                    // Generate a new primary seed and account (BIP44)
                    std::vector<unsigned char> entropy(16);
                    GetStrongRandBytes(&entropy[0], 16);
                    walletInstance->activeSeed = new CHDSeed(mnemonicFromEntropy(entropy, entropy.size()*8).c_str(), CHDSeed::CHDSeed::BIP44);
                    if (!CWalletDB(*walletInstance->dbw).WriteHDSeed(*walletInstance->activeSeed))
                    {
                        throw std::runtime_error("Writing seed failed");
                    }
                    if (walletWasCrypted)
                    {
                        if (!walletInstance->activeSeed->Encrypt(walletInstance->activeAccount->vMasterKey))
                        {
                            throw std::runtime_error("Encrypting seed failed");
                        }
                    }
                    walletInstance->mapSeeds[walletInstance->activeSeed->getUUID()] = walletInstance->activeSeed;
                    {
                        CWalletDB walletdb(*walletInstance->dbw);
                        walletdb.WritePrimarySeed(*walletInstance->activeSeed);
                    }
                    walletInstance->activeAccount = walletInstance->GenerateNewAccount(_("My account"), AccountType::Normal, AccountSubType::Desktop);
                    {
                        CWalletDB walletdb(*walletInstance->dbw);
                        walletdb.WritePrimaryAccount(walletInstance->activeAccount);
                    }
                }
            }
        }
        else
        {
            while (true)
            {
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

                //Force old legacy account to resave
                {
                    CWalletDB walletdb(*walletInstance->dbw);
                    walletInstance->changeAccountName(walletInstance->activeAccount, _("Legacy account"), true);
                    if (!walletdb.WriteAccount(getUUIDAsString(walletInstance->activeAccount->getUUID()), walletInstance->activeAccount))
                    {
                        throw std::runtime_error("Writing legacy account failed");
                    }
                    if (walletWasCrypted && !walletInstance->activeAccount->internalKeyStore.IsCrypted())
                    {
                        walletInstance->activeAccount->internalKeyStore.SetCrypted();
                        walletInstance->activeAccount->internalKeyStore.Unlock(walletInstance->activeAccount->vMasterKey);
                    }
                    walletInstance->ForceRewriteKeys(*walletInstance->activeAccount);
                }
            }
        }
    }
    else if (loadState == EXISTING_WALLET)
    {
        //Clean up a slight issue in 1.6.0 -> 1.6.3 wallets where a "usehd=0" account was created but no active account set.
        if (!walletInstance->activeAccount)
        {
            if (!walletInstance->mapAccounts.empty())
                walletInstance->setActiveAccount(walletInstance->mapAccounts.begin()->second);
            else
                throw std::runtime_error("Wallet contains no accounts, but is marked as upgraded.");
        }
    }
    else
    {
        throw std::runtime_error("Unknown wallet load state.");
    }

    if (GuldenApplication::gApp->isRecovery)
    {
        walletInstance->nTimeFirstKey = Params().GenesisBlock().nTime;
    }

    LogPrintf(" wallet      %15dms\n", GetTimeMillis() - nStart);

    RegisterValidationInterface(walletInstance);

    CBlockIndex *pindexRescan = chainActive.Tip();
    if (GetBoolArg("-rescan", false) || GuldenApplication::gApp->isRecovery)
    {
        pindexRescan = chainActive.Genesis();
    }
    else
    {
        CWalletDB walletdb(*walletInstance->dbw);
        CBlockLocator locator;
        if (walletdb.ReadBestBlock(locator))
            pindexRescan = FindForkInGlobalIndex(chainActive, locator);
    }
    if (chainActive.Tip() && chainActive.Tip() != pindexRescan)
    {
        //We can't rescan beyond non-pruned blocks, stop and throw an error
        //this might happen if a user uses a old wallet within a pruned node
        // or if he ran -disablewallet for a longer time, then decided to re-enable
        if (fPruneMode)
        {
            CBlockIndex *block = chainActive.Tip();
            while (block && block->pprev && (block->pprev->nStatus & BLOCK_HAVE_DATA) && block->pprev->nTx > 0 && pindexRescan != block)
                block = block->pprev;

            if (pindexRescan != block) {
                InitError(_("Prune: last wallet synchronisation goes beyond pruned data. You need to -reindex (download the whole blockchain again in case of pruned node)"));
                return NULL;
            }
        }

        uiInterface.InitMessage(_("Rescanning..."));
        LogPrintf("Rescanning last %i blocks (from block %i)...\n", chainActive.Height() - pindexRescan->nHeight, pindexRescan->nHeight);
        nStart = GetTimeMillis();
        walletInstance->ScanForWalletTransactions(pindexRescan, true);
        LogPrintf(" rescan      %15dms\n", GetTimeMillis() - nStart);
        walletInstance->SetBestChain(chainActive.GetLocatorPoW2());
        walletInstance->dbw->IncrementUpdateCounter();

        // Restore wallet transaction metadata after -zapwallettxes=1
        if (GetBoolArg("-zapwallettxes", false) && GetArg("-zapwallettxes", "1") != "2")
        {
            CWalletDB walletdb(*walletInstance->dbw);

            BOOST_FOREACH(const CWalletTx& wtxOld, vWtx)
            {
                uint256 hash = wtxOld.GetHash();
                std::map<uint256, CWalletTx>::iterator mi = walletInstance->mapWallet.find(hash);
                if (mi != walletInstance->mapWallet.end())
                {
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

    {
        LOCK(walletInstance->cs_wallet);
        //fixme: (GULDEN) - 'key pool size' concept for wallet doesn't really make sense anymore.
        LogPrintf("setKeyPool.size() = %u\n",      walletInstance->GetKeyPoolSize());
        LogPrintf("mapWallet.size() = %u\n",       walletInstance->mapWallet.size());
        LogPrintf("mapAddressBook.size() = %u\n",  walletInstance->mapAddressBook.size());
    }

    return walletInstance;
}

bool CWallet::InitLoadWallet()
{
    LOCK(cs_main);

    if (GetBoolArg("-disablewallet", DEFAULT_DISABLE_WALLET)) {
        LogPrintf("Wallet disabled!\n");
        return true;
    }

    for (const std::string& walletFile : gArgs.GetArgs("-wallet")) {
        CWallet * const pwallet = CreateWalletFromFile(walletFile);
        if (!pwallet) {
            return false;
        }
        vpwallets.push_back(pwallet);
        pactiveWallet = pwallet;
    }

    return true;
}

std::atomic<bool> CWallet::fFlushScheduled(false);

void CWallet::postInitProcess(CScheduler& scheduler)
{
    // Add wallet transactions that aren't already in a block to mempool
    // Do this here as mempool requires genesis block to be loaded
    ReacceptWalletTransactions();

    // Run a thread to flush wallet periodically
    if (!CWallet::fFlushScheduled.exchange(true)) {
        scheduler.scheduleEvery(MaybeCompactWalletDB, 500);
    }
}

bool CWallet::ParameterInteraction()
{
    SoftSetArg("-wallet", DEFAULT_WALLET_DAT);
    const bool is_multiwallet = gArgs.GetArgs("-wallet").size() > 1;

    if (GetBoolArg("-disablewallet", DEFAULT_DISABLE_WALLET))
        return true;

    if (GetBoolArg("-blocksonly", DEFAULT_BLOCKSONLY) && SoftSetBoolArg("-walletbroadcast", false)) {
        LogPrintf("%s: parameter interaction: -blocksonly=1 -> setting -walletbroadcast=0\n", __func__);
    }

    if (GetBoolArg("-salvagewallet", false) && SoftSetBoolArg("-rescan", true)) {
        if (is_multiwallet) {
            return InitError(strprintf("%s is only allowed with a single wallet file", "-salvagewallet"));
        }
        // Rewrite just private keys: rescan to find transactions
        LogPrintf("%s: parameter interaction: -salvagewallet=1 -> setting -rescan=1\n", __func__);
    }

    // -zapwallettx implies a rescan
    if (GetBoolArg("-zapwallettxes", false) && SoftSetBoolArg("-rescan", true)) {
        if (is_multiwallet) {
            return InitError(strprintf("%s is only allowed with a single wallet file", "-zapwallettxes"));
        }
        LogPrintf("%s: parameter interaction: -zapwallettxes=<mode> -> setting -rescan=1\n", __func__);
    }

    if (is_multiwallet) {
        if (GetBoolArg("-upgradewallet", false)) {
            return InitError(strprintf("%s is only allowed with a single wallet file", "-upgradewallet"));
        }
    }

    if (GetBoolArg("-sysperms", false))
        return InitError("-sysperms is not allowed in combination with enabled wallet functionality");
    if (GetArg("-prune", 0) && GetBoolArg("-rescan", false))
        return InitError(_("Rescans are not possible in pruned mode. You will need to use -reindex which will download the whole blockchain again."));

    if (::minRelayTxFee.GetFeePerK() > HIGH_TX_FEE_PER_KB)
        InitWarning(AmountHighWarn("-minrelaytxfee") + " " +
                    _("The wallet will avoid paying less than the minimum relay fee."));

    if (IsArgSet("-mintxfee"))
    {
        CAmount n = 0;
        if (!ParseMoney(GetArg("-mintxfee", ""), n) || 0 == n)
            return InitError(AmountErrMsg("mintxfee", GetArg("-mintxfee", "")));
        if (n > HIGH_TX_FEE_PER_KB)
            InitWarning(AmountHighWarn("-mintxfee") + " " +
                        _("This is the minimum transaction fee you pay on every transaction."));
        CWallet::minTxFee = CFeeRate(n);
    }
    if (IsArgSet("-fallbackfee"))
    {
        CAmount nFeePerK = 0;
        if (!ParseMoney(GetArg("-fallbackfee", ""), nFeePerK))
            return InitError(strprintf(_("Invalid amount for -fallbackfee=<amount>: '%s'"), GetArg("-fallbackfee", "")));
        if (nFeePerK > HIGH_TX_FEE_PER_KB)
            InitWarning(AmountHighWarn("-fallbackfee") + " " +
                        _("This is the transaction fee you may pay when fee estimates are not available."));
        CWallet::fallbackFee = CFeeRate(nFeePerK);
    }
    if (IsArgSet("-paytxfee"))
    {
        CAmount nFeePerK = 0;
        if (!ParseMoney(GetArg("-paytxfee", ""), nFeePerK))
            return InitError(AmountErrMsg("paytxfee", GetArg("-paytxfee", "")));
        if (nFeePerK > HIGH_TX_FEE_PER_KB)
            InitWarning(AmountHighWarn("-paytxfee") + " " +
                        _("This is the transaction fee you will pay if you send a transaction."));

        payTxFee = CFeeRate(nFeePerK, 1000);
        if (payTxFee < ::minRelayTxFee)
        {
            return InitError(strprintf(_("Invalid amount for -paytxfee=<amount>: '%s' (must be at least %s)"),
                                       GetArg("-paytxfee", ""), ::minRelayTxFee.ToString()));
        }
    }
    if (IsArgSet("-maxtxfee"))
    {
        CAmount nMaxFee = 0;
        if (!ParseMoney(GetArg("-maxtxfee", ""), nMaxFee))
            return InitError(AmountErrMsg("maxtxfee", GetArg("-maxtxfee", "")));
        if (nMaxFee > HIGH_MAX_TX_FEE)
            InitWarning(_("-maxtxfee is set very high! Fees this large could be paid on a single transaction."));
        maxTxFee = nMaxFee;
        if (CFeeRate(maxTxFee, 1000) < ::minRelayTxFee)
        {
            return InitError(strprintf(_("Invalid amount for -maxtxfee=<amount>: '%s' (must be at least the minrelay fee of %s to prevent stuck transactions)"),
                                       GetArg("-maxtxfee", ""), ::minRelayTxFee.ToString()));
        }
    }
    nTxConfirmTarget = GetArg("-txconfirmtarget", DEFAULT_TX_CONFIRM_TARGET);
    bSpendZeroConfChange = GetBoolArg("-spendzeroconfchange", DEFAULT_SPEND_ZEROCONF_CHANGE);
    fWalletRbf = GetBoolArg("-walletrbf", DEFAULT_WALLET_RBF);

    return true;
}

bool CWallet::BackupWallet(const std::string& strDest)
{
    return dbw->Backup(strDest);
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

void CMerkleTx::SetMerkleBranch(const CBlockIndex* pindex, int posInBlock)
{
    // Update the tx's hashBlock
    hashBlock = pindex->GetBlockHashPoW2();

    // set the position of the transaction in the block
    nIndex = posInBlock;
}

int CMerkleTx::GetDepthInMainChain(const CBlockIndex* &pindexRet) const
{
    if (hashUnset())
        return 0;

    AssertLockHeld(cs_main);

    // Find the block it claims to be in
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
    return std::max(0, (COINBASE_MATURITY+1) - GetDepthInMainChain());
}


bool CMerkleTx::AcceptToMemoryPool(const CAmount& nAbsurdFee, CValidationState& state)
{
    return ::AcceptToMemoryPool(mempool, state, tx, true, NULL, NULL, false, nAbsurdFee);
}
