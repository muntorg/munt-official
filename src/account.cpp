// Copyright (c) 2016 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#include "account.h"
#include "wallet/wallet.h"
#include <Gulden/mnemonic.h>
#include "base58.h"
#include "wallet/walletdb.h"
#include "wallet/crypter.h"

#include <map>

#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/nil_generator.hpp>



CHDSeed::CHDSeed()
: m_nAccountIndex(HDDesktopStartIndex)
, m_nAccountIndexMobi(HDMobileStartIndex)
, encrypted(false)
{
}

CHDSeed::CHDSeed(SecureString mnemonic, SeedType type)
: m_type(type)
, m_UUID(boost::uuids::nil_generator()())
, m_nAccountIndex(HDDesktopStartIndex)
, m_nAccountIndexMobi(HDMobileStartIndex)
, encrypted(false)
{
    //fixme: GULDEN (FUT) (1.6.1) Encrypt the seeds immediately upon creation so that they are never written to disk unencrypted.
    unencryptedMnemonic = mnemonic;
    Init();
}

void CHDSeed::Init()
{
    unsigned char* seed = seedFromMnemonic(unencryptedMnemonic);
    static const std::vector<unsigned char> hashkey = {'G','u','l','d','e','n',' ','b','i','p','3','2'};
    static const std::vector<unsigned char> hashkeylegacy = {'B','i','t','c','o','i','n',' ','s','e','e','d'};
    
    // Legacy support for old iOS and android 'Guldencoin' wallets.
    if (m_type == BIP32Legacy || m_type == BIP44External)
    {
        masterKeyPriv.SetMaster(hashkeylegacy, seed, 64);
    }
    else
    {
        masterKeyPriv.SetMaster(hashkey, seed, 64);
    }
    
    
    switch (m_type)
    {
        case BIP32:
        case BIP32Legacy:
            masterKeyPriv.Derive(purposeKeyPriv, 100 | BIP32_HARDENED_KEY_LIMIT);  //Unused - but we generate anyway so that we don't save predictable/blank encrypted data to disk (tiny security precaution)
            purposeKeyPriv.Derive(cointypeKeyPriv, 100 | BIP32_HARDENED_KEY_LIMIT);  //Unused - but we generate anyway so that we don't save predictable/blank encrypted data to disk (tiny security precaution)
            break;
        case BIP44:
            {
                masterKeyPriv.Derive(purposeKeyPriv, 44 | BIP32_HARDENED_KEY_LIMIT);  //m/44'
                purposeKeyPriv.Derive(cointypeKeyPriv, 87 | BIP32_HARDENED_KEY_LIMIT);  //m/44'/87'
            }
            break;
        default:
            assert(0);
    }
    masterKeyPub = masterKeyPriv.Neuter();
    purposeKeyPub = purposeKeyPriv.Neuter();
    cointypeKeyPub = cointypeKeyPriv.Neuter();
    
    if(m_UUID.is_nil())
    {
        m_UUID = boost::uuids::random_generator()();
    }
}





CAccountHD* CHDSeed::GenerateAccount(AccountSubType type, CWalletDB* Db)
{
    if (IsLocked())
        return NULL;
    
    CAccountHD* account = NULL;
    switch (type)
    {
        case Desktop:
            assert(m_nAccountIndex <= 99999);
            account = GenerateAccount( m_nAccountIndex++ );
            break;
        case Mobi:
            account = GenerateAccount( m_nAccountIndexMobi++ );
            break;
        default:
            assert(0);
            return NULL;
    }
    
    if (Db)
    {
        //fixme: Can we just set dirty or something and then it gets saved later?
        Db->WriteHDSeed(*this);
    }
    
    account->m_SubType = type;
    if (IsCrypted())
    {
        account->Encrypt(vMasterKey);
    }
    return account;
}

CAccountHD* CHDSeed::GenerateAccount(int nAccountIndex)
{
    if(IsLocked())
        return NULL;
    
    CExtKey accountKey;
    switch (m_type)
    {
        case BIP32:
        case BIP32Legacy:
            masterKeyPriv.Derive(accountKey, nAccountIndex | BIP32_HARDENED_KEY_LIMIT);  // m/n' (BIP32) 
            break;
        case BIP44:
        case BIP44External:
            cointypeKeyPriv.Derive(accountKey, nAccountIndex | BIP32_HARDENED_KEY_LIMIT);  // m/44'/87'/n' (BIP44)
            break;
        default:
            assert(0);
    }   
    
    return new CAccountHD(accountKey, m_UUID);
}


std::string CHDSeed::getUUID() const
{
    return boost::uuids::to_string(m_UUID);
}


SecureString CHDSeed::getMnemonic()
{
    return unencryptedMnemonic;
}

bool CHDSeed::IsLocked() const
{
    if (unencryptedMnemonic.size() > 0)
    {
        return false;
    }
    return true;
}

bool CHDSeed::IsCrypted() const
{
    return encrypted;
}

bool CHDSeed::Lock()
{
    // The below is a 'SecureString' - so no memory burn necessary, it should burn itself.
    unencryptedMnemonic = "";
    // The below is a 'SecureString' - so no memory burn necessary, it should burn itself.
    masterKeyPriv = CExtKey();
    purposeKeyPriv = CExtKey();
    cointypeKeyPriv = CExtKey();
    
    //fixme: HIGH - also burnt he memory just to be sure?
    vMasterKey.clear();
    
    return true;
}

bool CHDSeed::Unlock(const CKeyingMaterial& vMasterKeyIn)
{
    // Decrypt mnemonic
    assert(sizeof(m_UUID) == WALLET_CRYPTO_IV_SIZE);
    CKeyingMaterial vchMnemonic;
    if (!DecryptSecret(vMasterKeyIn, encryptedMnemonic, std::vector<unsigned char>(m_UUID.begin(), m_UUID.end()), vchMnemonic))
        return false;
    unencryptedMnemonic = SecureString(vchMnemonic.begin(), vchMnemonic.end());
    
    // Decrypt master key
    CKeyingMaterial vchMasterKeyPrivEncoded;
    if (!DecryptSecret(vMasterKeyIn, masterKeyPrivEncrypted, masterKeyPub.pubkey.GetHash(), vchMasterKeyPrivEncoded))
        return false;
    masterKeyPriv.Decode(vchMasterKeyPrivEncoded.data());
    
    // Decrypt purpose key
    CKeyingMaterial vchPurposeKeyPrivEncoded;
    if (!DecryptSecret(vMasterKeyIn, purposeKeyPrivEncrypted, purposeKeyPub.pubkey.GetHash(), vchPurposeKeyPrivEncoded))
        return false;
    purposeKeyPriv.Decode(vchPurposeKeyPrivEncoded.data());
    
    // Decrypt coin type key
    CKeyingMaterial vchCoinTypeKeyPrivEncoded;
    if (!DecryptSecret(vMasterKeyIn, cointypeKeyPrivEncrypted, cointypeKeyPub.pubkey.GetHash(), vchCoinTypeKeyPrivEncoded))
        return false;
    cointypeKeyPriv.Decode(vchCoinTypeKeyPrivEncoded.data());
    
    vMasterKey = vMasterKeyIn;
    
    return true;
}

bool CHDSeed::Encrypt(CKeyingMaterial& vMasterKeyIn)
{  
    // Encrypt mnemonic
    assert(sizeof(m_UUID) == WALLET_CRYPTO_IV_SIZE);
    encryptedMnemonic.clear();
    if (!EncryptSecret(vMasterKeyIn, CKeyingMaterial(unencryptedMnemonic.begin(), unencryptedMnemonic.end()), std::vector<unsigned char>(m_UUID.begin(), m_UUID.end()), encryptedMnemonic))
        return false;
    
    // Encrypt master key
    SecureUnsignedCharVector masterKeyPrivEncoded(BIP32_EXTKEY_SIZE);
    masterKeyPriv.Encode(masterKeyPrivEncoded.data());
    if (!EncryptSecret(vMasterKeyIn, CKeyingMaterial(masterKeyPrivEncoded.begin(), masterKeyPrivEncoded.end()), masterKeyPub.pubkey.GetHash(), masterKeyPrivEncrypted))
        return false;
    
    // Encrypt purpose key
    SecureUnsignedCharVector purposeKeyPrivEncoded(BIP32_EXTKEY_SIZE);
    purposeKeyPriv.Encode(purposeKeyPrivEncoded.data());
    if (!EncryptSecret(vMasterKeyIn, CKeyingMaterial(purposeKeyPrivEncoded.begin(), purposeKeyPrivEncoded.end()), purposeKeyPub.pubkey.GetHash(), purposeKeyPrivEncrypted))
        return false;
    
    // Encrypt coin type key
    SecureUnsignedCharVector cointypeKeyPrivEncoded(BIP32_EXTKEY_SIZE);
    cointypeKeyPriv.Encode(cointypeKeyPrivEncoded.data());
    if (!EncryptSecret(vMasterKeyIn, CKeyingMaterial(cointypeKeyPrivEncoded.begin(), cointypeKeyPrivEncoded.end()), cointypeKeyPub.pubkey.GetHash(), cointypeKeyPrivEncrypted))
        return false;
    
    encrypted = true;
    vMasterKey = vMasterKeyIn;
    
    return true;
}



CAccountHD::CAccountHD(CExtKey accountKey_, boost::uuids::uuid seedID)
: CAccount()
, m_SeedID(seedID)
, m_nIndex(accountKey_.nChild)
, m_nNextChildIndex(0)
, m_nNextChangeIndex(0)
, encrypted(false)
, accountKeyPriv(accountKey_)
{
    accountKeyPriv.Derive(primaryChainKeyPriv, 0);  //a/0
    accountKeyPriv.Derive(changeChainKeyPriv, 1);  //a/1
    primaryChainKeyPub = primaryChainKeyPriv.Neuter();
    changeChainKeyPub = changeChainKeyPriv.Neuter();
}

void CAccountHD::GetKey(CExtKey& childKey, int nChain)
{
    if (nChain == KEYCHAIN_EXTERNAL)
    {
        primaryChainKeyPriv.Derive(childKey, m_nNextChildIndex++);
    }
    else
    {
        changeChainKeyPriv.Derive(childKey, m_nNextChangeIndex++);
    }
}

bool CAccountHD::GetKey(const CKeyID& keyID, CKey& key) const
{
    if(IsLocked())
        return false;
    
    int64_t nKeyIndex = -1;
    CExtKey privKey;
    if(externalKeyStore.GetKey(keyID, nKeyIndex))
    {
        primaryChainKeyPriv.Derive(privKey, nKeyIndex);
        if(privKey.Neuter().pubkey.GetID() != keyID)
            assert(0);
        key = privKey.key;
        return true;
    }
    if(internalKeyStore.GetKey(keyID, nKeyIndex))
    {
        changeChainKeyPriv.Derive(privKey, nKeyIndex);
        if(privKey.Neuter().pubkey.GetID() != keyID)
            assert(0);
        key = privKey.key;
        return true;
    }
    return false;
}

bool CAccountHD::GetKey(const CKeyID &address, std::vector<unsigned char>& encryptedKeyOut) const
{
    assert(0);
}

void CAccountHD::GetPubKey(CExtPubKey& childKey, int nChain) const
{
    if (nChain == KEYCHAIN_EXTERNAL)
    {
        primaryChainKeyPub.Derive(childKey, m_nNextChildIndex++);
    }
    else
    {
        changeChainKeyPub.Derive(childKey, m_nNextChangeIndex++);
    }
}

bool CAccountHD::GetPubKey(const CKeyID &address, CPubKey& vchPubKeyOut) const
{
    int64_t nKeyIndex = -1;
    if(externalKeyStore.GetKey(address, nKeyIndex))
    {
        CExtPubKey extPubKey;
        primaryChainKeyPub.Derive(extPubKey, nKeyIndex);
        vchPubKeyOut = extPubKey.pubkey;
        return true;
    }
    if(internalKeyStore.GetKey(address, nKeyIndex))
    {
        CExtPubKey extPubKey;
        changeChainKeyPub.Derive(extPubKey, nKeyIndex);
        vchPubKeyOut = extPubKey.pubkey;
        return true;
    }
    return false;
}

bool CAccountHD::Lock()
{
    // We can't lock if we are not encrypted, nothing to do.
    if (!encrypted)
        return false;
    
    //We don't encrypt the keystores for HD accounts - as they only contain public keys.
    //if (!CAccount::Lock())
        //return false;
    
    //fixme: GULDEN (FUT) (1.6.1) - burn?
    accountKeyPriv = CExtKey();
    primaryChainKeyPriv = CExtKey();
    changeChainKeyPriv = CExtKey();
    
    return true;
}

bool CAccountHD::Unlock(const CKeyingMaterial& vMasterKeyIn)
{
    assert(sizeof(accountUUID) == WALLET_CRYPTO_IV_SIZE);
    
    //We don't encrypt the keystores for HD accounts - as they only contain public keys.
    //if (!CAccount::Unlock(vMasterKeyIn))
    //    return false;
    
    // Decrypt account key
    CKeyingMaterial vchAccountKeyPrivEncoded;
    if (!DecryptSecret(vMasterKeyIn, accountKeyPrivEncrypted, std::vector<unsigned char>(accountUUID.begin(), accountUUID.end()), vchAccountKeyPrivEncoded))
        return false;
    accountKeyPriv.Decode(vchAccountKeyPrivEncoded.data());
    
    // Decrypt primary chain key
    CKeyingMaterial vchPrimaryChainKeyPrivEncoded;
    if (!DecryptSecret(vMasterKeyIn, primaryChainKeyEncrypted, primaryChainKeyPub.pubkey.GetHash(), vchPrimaryChainKeyPrivEncoded))
        return false;
    primaryChainKeyPriv.Decode(vchPrimaryChainKeyPrivEncoded.data());
    
    // Decrypt change chain key
    CKeyingMaterial vchChangeChainKeyPrivEncoded;
    if (!DecryptSecret(vMasterKeyIn, changeChainKeyEncrypted, changeChainKeyPub.pubkey.GetHash(), vchChangeChainKeyPrivEncoded))
        return false;
    changeChainKeyPriv.Decode(vchChangeChainKeyPrivEncoded.data());
    
    return true;
}

bool CAccountHD::Encrypt(CKeyingMaterial& vMasterKeyIn)
{
    assert(sizeof(accountUUID) == WALLET_CRYPTO_IV_SIZE);
    
    //We don't encrypt the keystores for HD accounts - as they only contain public keys.
    //if (!CAccount::Encrypt(vMasterKeyIn))
        //return false;    
    
    // Encrypt account key
    SecureUnsignedCharVector accountKeyPrivEncoded(BIP32_EXTKEY_SIZE);
    accountKeyPriv.Encode(accountKeyPrivEncoded.data());
    if (!EncryptSecret(vMasterKeyIn, CKeyingMaterial(accountKeyPrivEncoded.begin(), accountKeyPrivEncoded.end()), std::vector<unsigned char>(accountUUID.begin(), accountUUID.end()), accountKeyPrivEncrypted))
        return false;
    
    // Encrypt primary chain key
    SecureUnsignedCharVector primaryChainKeyPrivEncoded(BIP32_EXTKEY_SIZE);
    primaryChainKeyPriv.Encode(primaryChainKeyPrivEncoded.data());
    if (!EncryptSecret(vMasterKeyIn, CKeyingMaterial(primaryChainKeyPrivEncoded.begin(), primaryChainKeyPrivEncoded.end()), primaryChainKeyPub.pubkey.GetHash(), primaryChainKeyEncrypted))
        return false;
    
    // Encrypt change chain key
    SecureUnsignedCharVector changeChainKeyPrivEncoded(BIP32_EXTKEY_SIZE);
    changeChainKeyPriv.Encode(changeChainKeyPrivEncoded.data());
    if (!EncryptSecret(vMasterKeyIn, CKeyingMaterial(changeChainKeyPrivEncoded.begin(), changeChainKeyPrivEncoded.end()), changeChainKeyPub.pubkey.GetHash(), changeChainKeyEncrypted))
        return false;
        
    encrypted = true;
    
    return true;
}

bool CAccountHD::IsCrypted() const
{
    return encrypted;
}

bool CAccountHD::IsLocked() const
{
    return !accountKeyPriv.key.IsValid();
}

bool CAccountHD::AddKeyPubKey(const CKey& key, const CPubKey &pubkey, int keyChain)
{
    //This should never be called on a HD account.
    //We don't store private keys for HD we just generate them as needed.
    assert(0);
    return true;
}
    
bool CAccountHD::AddKeyPubKey(int64_t HDKeyIndex, const CPubKey &pubkey, int keyChain)
{
    if(keyChain == KEYCHAIN_EXTERNAL)
        return externalKeyStore.AddKeyPubKey(HDKeyIndex, pubkey);
    else
        return internalKeyStore.AddKeyPubKey(HDKeyIndex, pubkey);
}


CPubKey CAccountHD::GenerateNewKey(CWallet& wallet, int keyChain)
{
    CExtPubKey childKey;
    do
    {
        GetPubKey(childKey, keyChain);
    }
    while( wallet.HaveKey(childKey.pubkey.GetID()) );//fixme: (GULDEN) (BIP44) No longer need wallet here.
    
    LogPrintf("CAccount::GenerateNewKey(): NewHDKey [%s]\n", CBitcoinAddress(childKey.pubkey.GetID()).ToString());
    
    if (!wallet.AddKeyPubKey(childKey.nChild, childKey.pubkey, *this, keyChain))
        throw std::runtime_error("CAccount::GenerateNewKey(): AddKeyPubKey failed");

    return childKey.pubkey;
}


CExtKey* CAccountHD::GetAccountMasterPrivKey()
{
    if (IsLocked())
        return NULL;
    
    return &accountKeyPriv;
}

std::string CAccountHD::getSeedUUID() const
{
    return boost::uuids::to_string(m_SeedID);
}

uint32_t CAccountHD::getIndex()
{
    return m_nIndex;
}


CAccount::CAccount()
{
    SetNull();
    //Start at current time and go backwards as we find transactions.
    earliestPossibleCreationTime = GetTime();
    accountUUID = boost::uuids::random_generator()();
    parentUUID = boost::uuids::nil_generator()();
    m_Type = AccountType::Normal;
    m_SubType = AccountSubType::Desktop;
}

void CAccount::SetNull()
{
    vchPubKey = CPubKey();
}

CPubKey CAccount::GenerateNewKey(CWallet& wallet, int keyChain)
{    
    CKey secret;
    secret.MakeNewKey(true);   

    CPubKey pubkey = secret.GetPubKey();
    assert(secret.VerifyPubKey(pubkey));
    
    if (!wallet.AddKeyPubKey(secret, pubkey, *this, keyChain))
        throw std::runtime_error("CAccount::GenerateNewKey(): AddKeyPubKey failed");

    return pubkey;
}


//fixme: WATCHONLY
bool CAccount::HaveWalletTx(const CTransaction& tx)
{
    for(const CTxOut& txout : tx.vout)
    {
        isminetype ret = isminetype::ISMINE_NO;
        for (auto keyChain : { KEYCHAIN_EXTERNAL, KEYCHAIN_CHANGE })
        {
            isminetype temp = ( keyChain == KEYCHAIN_EXTERNAL ? IsMine(externalKeyStore, txout.scriptPubKey) : IsMine(internalKeyStore, txout.scriptPubKey) );
            if (temp > ret)
                ret = temp;
        }
        if (ret >= isminetype::ISMINE_NO)
            return true;
    }
    for(const CTxIn& txin : tx.vin)
    {
        isminetype ret = isminetype::ISMINE_NO;
        std::map<uint256, CWalletTx>::const_iterator mi = pwalletMain->mapWallet.find(txin.prevout.hash);
        if (mi != pwalletMain->mapWallet.end())
        {
            const CWalletTx& prev = (*mi).second;
            if (txin.prevout.n < prev.vout.size())
            {
                for(const CTxOut& txout : prev.vout)
                {
                    for (auto keyChain : { KEYCHAIN_EXTERNAL, KEYCHAIN_CHANGE })
                    {
                        isminetype temp = ( keyChain == KEYCHAIN_EXTERNAL ? IsMine(externalKeyStore, txout.scriptPubKey) : IsMine(internalKeyStore, txout.scriptPubKey) );
                        if (temp > ret)
                            ret = temp;
                    }
                }
            }
        }
        if (ret >= isminetype::ISMINE_NO)
            return true;
    }
    return false;
}


bool CAccount::HaveKey(const CKeyID &address) const
{
    return externalKeyStore.HaveKey(address) || internalKeyStore.HaveKey(address);
}

bool CAccount::HaveWatchOnly(const CScript &dest) const
{
    return externalKeyStore.HaveWatchOnly(dest) || internalKeyStore.HaveWatchOnly(dest);
}

bool CAccount::HaveWatchOnly() const
{
    return externalKeyStore.HaveWatchOnly() || internalKeyStore.HaveWatchOnly();
}

bool CAccount::HaveCScript(const CScriptID &hash) const
{
    return externalKeyStore.HaveCScript(hash) || internalKeyStore.HaveCScript(hash);
}

bool CAccount::GetCScript(const CScriptID &hash, CScript& redeemScriptOut) const
{
    return externalKeyStore.GetCScript(hash, redeemScriptOut) || internalKeyStore.GetCScript(hash, redeemScriptOut);
}

bool CAccount::IsLocked() const
{
    return externalKeyStore.IsLocked() || internalKeyStore.IsLocked();
}

bool CAccount::IsCrypted() const
{
    return externalKeyStore.IsCrypted() || internalKeyStore.IsCrypted();
}

bool CAccount::Lock()
{
    //fixme: HIGH - also burnt he memory just to be sure?
    vMasterKey.clear();
    
    return externalKeyStore.Lock() && internalKeyStore.Lock();
}

bool CAccount::Unlock(const CKeyingMaterial& vMasterKeyIn)
{
    vMasterKey = vMasterKeyIn;
    
    return externalKeyStore.Unlock(vMasterKeyIn) && internalKeyStore.Unlock(vMasterKeyIn);
}
    
bool CAccount::GetKey(const CKeyID& keyID, CKey& key) const
{
    return externalKeyStore.GetKey(keyID, key) || internalKeyStore.GetKey(keyID, key);
}

bool CAccount::GetKey(const CKeyID &address, std::vector<unsigned char>& encryptedKeyOut) const
{
    return externalKeyStore.GetKey(address, encryptedKeyOut) || internalKeyStore.GetKey(address, encryptedKeyOut);
}

void CAccount::GetKeys(std::set<CKeyID> &setAddress) const
{
    std::set<CKeyID> setExternal;
    externalKeyStore.GetKeys(setExternal);
    std::set<CKeyID> setInternal;
    internalKeyStore.GetKeys(setInternal);
    setAddress.clear();
    setAddress.insert(setExternal.begin(), setExternal.end());
    setAddress.insert(setInternal.begin(), setInternal.end());
}

bool CAccount::EncryptKeys(CKeyingMaterial& vMasterKeyIn)
{
    if (!externalKeyStore.EncryptKeys(vMasterKeyIn))
        return false;
    if (!internalKeyStore.EncryptKeys(vMasterKeyIn))
        return false;
    
    if (pwalletMain)
    {
        if (!pwalletMain->fFileBacked)
            return true;
        {
            std::set<CKeyID> setAddress;
            GetKeys(setAddress);
            
            LOCK(pwalletMain->cs_wallet);                
            for (const auto& keyID : setAddress)
            {
                CPubKey pubKey;
                if (!GetPubKey(keyID, pubKey))
                { 
                    LogPrintf("CAccount::EncryptKeys(): Failed to get pubkey");
                    return false;
                }
                if (pwalletMain->pwalletdbEncryption)
                    pwalletMain->pwalletdbEncryption->EraseKey(pubKey);
                else
                    CWalletDB(pwalletMain->strWalletFile).EraseKey(pubKey);
                
                std::vector<unsigned char> secret;
                if (!GetKey(keyID, secret))
                { 
                    LogPrintf("CAccount::EncryptKeys(): Failed to get crypted key");
                    return false;
                }
                if (pwalletMain->pwalletdbEncryption)
                {
                    if (!pwalletMain->pwalletdbEncryption->WriteCryptedKey(pubKey, secret, pwalletMain->mapKeyMetadata[keyID], getUUID(), KEYCHAIN_EXTERNAL))
                    {
                        LogPrintf("CAccount::EncryptKeys(): Failed to write key");
                        return false;
                    }
                }
                else
                {
                    if (!CWalletDB(pwalletMain->strWalletFile).WriteCryptedKey(pubKey, secret, pwalletMain->mapKeyMetadata[keyID], getUUID(), KEYCHAIN_EXTERNAL))
                    {
                        LogPrintf("CAccount::EncryptKeys(): Failed to write key");
                        return false;
                    }
                }
                
            }
        }
    }
    return true;
}

bool CAccount::Encrypt(CKeyingMaterial& vMasterKeyIn)
{
    return EncryptKeys(vMasterKeyIn) /*&& SetCrypted()*/;
}



bool CAccount::GetPubKey(const CKeyID &address, CPubKey& vchPubKeyOut) const
{
    return externalKeyStore.GetPubKey(address, vchPubKeyOut) || internalKeyStore.GetPubKey(address, vchPubKeyOut);
}


bool CAccount::AddKeyPubKey(const CKey& key, const CPubKey &pubkey, int keyChain)
{
    std::vector<unsigned char> encryptedKeyOut;
    if(keyChain == KEYCHAIN_EXTERNAL)
    {
        return externalKeyStore.AddKeyPubKey(key, pubkey);
        /*if (externalKeyStore.AddKeyPubKey(key, pubkey))
        {
            if (externalKeyStore.GetKey(pubkey.GetID(), encryptedKeyOut))
            {
                return AddCryptedKey(pubkey, encryptedKeyOut, keyChain);
            }
        }
        return false;*/
    }
    else
    {
        return internalKeyStore.AddKeyPubKey(key, pubkey);
    }    
}

bool CAccount::AddKeyPubKey(int64_t HDKeyIndex, const CPubKey &pubkey, int keyChain)
{
    //This should never be called on a non-HD wallet
    assert(0);
    return true;
}

bool CAccount::AddWatchOnly(const CScript &dest)
{
    //fixme: (GULDEN) (REIMPLEMENT AS SPECIAL WATCH ACCOUNT)
    assert(0);
    return externalKeyStore.AddWatchOnly(dest);
}

bool CAccount::RemoveWatchOnly(const CScript &dest)
{
    //fixme: (GULDEN) (REIMPLEMENT AS SPECIAL WATCH ACCOUNT)
    assert(0);
    return externalKeyStore.RemoveWatchOnly(dest) || internalKeyStore.RemoveWatchOnly(dest);
}

//checkme: CHILD ACCOUNTS - DOES THIS END UP IN THE RIGHT PLACE?
//fixme: (GULDEN) (WATCHONLY)
bool CAccount::AddCScript(const CScript& redeemScript)
{
    return externalKeyStore.AddCScript(redeemScript);
}

bool CAccount::AddCryptedKey(const CPubKey &vchPubKey, const std::vector<unsigned char> &vchCryptedSecret, int64_t nKeyChain)
{
    //fixme: GULDEN This is essentially dead code now - it has been replaced at the bottom of CWallet::AddKeyPubKey
    //For technical reasons (wallet upgrade)
    
    //This should never be called on a non-HD wallet
    assert(!IsHD());
    
    if (nKeyChain == KEYCHAIN_EXTERNAL)
    {
        if (!externalKeyStore.AddCryptedKey(vchPubKey, vchCryptedSecret))
            return false;
    }
    else
    {
        if (!internalKeyStore.AddCryptedKey(vchPubKey, vchCryptedSecret))
            return false;
    }
    
    // If we don't have a wallet yet (busy during wallet upgrade) - then the below not being called is fine as the wallet does a 'force resave' of all keys at the end of the upgrade.
    if (pwalletMain)
    {
        if (!pwalletMain->fFileBacked)
            return true;
        {
            LOCK(pwalletMain->cs_wallet);
            if (pwalletMain->pwalletdbEncryption)
                return pwalletMain->pwalletdbEncryption->WriteCryptedKey(vchPubKey, vchCryptedSecret, pwalletMain->mapKeyMetadata[vchPubKey.GetID()], getUUID(), nKeyChain);
            else
                return CWalletDB(pwalletMain->strWalletFile).WriteCryptedKey(vchPubKey, vchCryptedSecret, pwalletMain->mapKeyMetadata[vchPubKey.GetID()], getUUID(), nKeyChain);
        }
    }
    else
    {
        return true;
    }
    return false;
}

void CAccount::AddChild(CAccount* childAccount)
{
    childAccount->parentUUID = accountUUID;
}

void CAccount::possiblyUpdateEarliestTime(uint64_t creationTime, CWalletDB* Db)
{
    if (creationTime < earliestPossibleCreationTime)
        earliestPossibleCreationTime = creationTime;
    
    //fixme: GULDEN Can we just set dirty or something and then it gets saved later?
    if (Db)
    {
        Db->WriteAccount(getUUID(), this);
    }
}

uint64_t CAccount::getEarliestPossibleCreationTime()
{
    return earliestPossibleCreationTime;
}

//fixme: (GULDEN) (CLEANUP)
unsigned int CAccount::GetKeyPoolSize()
{
    AssertLockHeld(cs_keypool); // setKeyPool
    return setKeyPoolExternal.size();
}

std::string CAccount::getLabel() const
{
    return accountLabel;
}

void CAccount::setLabel(const std::string& label, CWalletDB* Db)
{
    accountLabel = label;
    if (Db)
    {
        Db->EraseAccountLabel(getUUID());
        Db->WriteAccountLabel(getUUID(), label);
    }
}

std::string CAccount::getUUID() const
{
    return boost::uuids::to_string(accountUUID);
}

void CAccount::setUUID(const std::string& stringUUID)
{
    accountUUID = boost::lexical_cast<boost::uuids::uuid>(stringUUID);
}
        
std::string CAccount::getParentUUID() const
{
    if (parentUUID == boost::uuids::nil_generator()())
        return "";
    return boost::uuids::to_string(parentUUID);
}




