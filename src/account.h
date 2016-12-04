// Copyright (c) 2016 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#ifndef GULDEN_ACCOUNT_H
#define GULDEN_ACCOUNT_H

#include "amount.h"
#include "streams.h"
#include "tinyformat.h"
#include "utilstrencodings.h"
#include "validationinterface.h"
#include "script/ismine.h"
#include "wallet/crypter.h"
#include "account.h"

#include <algorithm>
#include <map>
#include <set>
#include <stdexcept>
#include <stdint.h>
#include <string>
#include <utility>
#include <vector>

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/lexical_cast.hpp>


#define KEYCHAIN_EXTERNAL 0
#define KEYCHAIN_CHANGE 1
const uint32_t BIP32_HARDENED_KEY_LIMIT = 0x80000000;

class CAccountHD;
class CWallet;
class CWalletDB;

enum AccountType
{
        Normal = 0,        // Standard account (or HD account)
        Shadow = 1,        // Shadow account (account remains invisible until it becomes active - either through account creation or a payment)
        ShadowChild = 2,   // Shadow child account (as above but a child of another account) - used to handle legacy accounts (e.g. BIP32 child of BIP44 account that shares the same seed)
        Deleted = 3        // An account that has been deleted - we keep it arround anyway in case it receives funds, if it receives funds then we re-activate it.
};
    
enum AccountSubType
{
        Desktop = 0,       // Standard desktop account.
        Mobi = 1           // Mobile phone.
};

const int HDDesktopStartIndex = 0;
const int HDMobileStartIndex = 100000;
    

class CHDSeed
{
public:
    enum SeedType
    {
            BIP44 = 0, // New Gulden standard based on BIP44 for /all/ Gulden wallets (desktop/iOS/android)
            BIP32 = 1, // Old Gulden android/iOS wallets
            BIP32Legacy = 2, // Old 'guldencoin' wallets
            BIP44External = 3 //External BIP44 wallets like 'coinomi'
    };

    CHDSeed();
    CHDSeed(SecureString mnemonic, SeedType type);
    void Init();
    CAccountHD* GenerateAccount(AccountSubType type, CWalletDB* Db);

    
    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        if (!(nType & SER_GETHASH))
            READWRITE(nVersion);
               
        if (ser_action.ForRead())
        {
            int type;
            READWRITE(type);
            m_type = (SeedType)type;
            std::string sUUID;
            READWRITE(sUUID);
            m_UUID = boost::lexical_cast<boost::uuids::uuid>(sUUID);
        }
        else
        {
            int type = (int)m_type;
            READWRITE(type);
            std::string sUUID = boost::uuids::to_string(m_UUID);
            READWRITE(sUUID);
        }
        
        READWRITE(m_nAccountIndex);
        READWRITE(m_nAccountIndexMobi);
        
        READWRITE(masterKeyPub);
        READWRITE(purposeKeyPub);
        READWRITE(cointypeKeyPub);
        
        READWRITE(encrypted);
        if(encrypted)
        {
            READWRITE(encryptedMnemonic);
            READWRITE(masterKeyPrivEncrypted);
            READWRITE(purposeKeyPrivEncrypted);
            READWRITE(cointypeKeyPrivEncrypted);
        }
        else
        {
            READWRITE(unencryptedMnemonic);
            READWRITE(masterKeyPriv);
            READWRITE(purposeKeyPriv);
            READWRITE(cointypeKeyPriv);
        }
    }
     
    std::string getUUID() const;    
    SecureString getMnemonic();

    virtual bool IsLocked() const;
    virtual bool IsCrypted() const;
    virtual bool Lock();
    virtual bool Unlock(const CKeyingMaterial& vMasterKeyIn);
    virtual bool Encrypt(CKeyingMaterial& vMasterKeyIn);
    
    //What type of seed this is (BIP32 or BIP44)
    SeedType m_type;
    
protected:
    CAccountHD* GenerateAccount(int nAccountIndex);
    
    //Unique seed identifier
    boost::uuids::uuid m_UUID;    

    //Index of next account to generate (normal accounts)
    int m_nAccountIndex;
    //Index of next account to generate (mobile accounts)
    int m_nAccountIndexMobi;

    //Always available
    CExtPubKey masterKeyPub;  //hd master key (m)      - BIP32 and BIP44
    CExtPubKey purposeKeyPub; //key at m/44'           - BIP44 only
    CExtPubKey cointypeKeyPub;//key at m/44'/87'       - BIP44 only
    
    bool encrypted;
    //These members are only valid when the account is unlocked/unencrypted.
    SecureString unencryptedMnemonic;
    CExtKey masterKeyPriv;  //hd master key (m)      - BIP32 and BIP44
    CExtKey purposeKeyPriv; //key at m/44'           - BIP44 only
    CExtKey cointypeKeyPriv;//key at m/44'/87'       - BIP44 only
    CKeyingMaterial vMasterKey;//Memory only.
    
    //Contains the encrypted versions of the above - only valid when the account is an encrypted one.
    std::vector<unsigned char> encryptedMnemonic;
    std::vector<unsigned char> masterKeyPrivEncrypted;
    std::vector<unsigned char> purposeKeyPrivEncrypted;
    std::vector<unsigned char> cointypeKeyPrivEncrypted;
};


/** 
 * Account information.
 * Stored in wallet with key "acc"+string account name.
 */
class CAccount : public CCryptoKeyStore
{
public:
    CPubKey vchPubKey;

    CAccount();
    void SetNull();    
       
    //fixme: (GULDEN) (CLEANUP)
    virtual void GetKey(CExtKey& childKey, int nChain) {};
    virtual CPubKey GenerateNewKey(CWallet& wallet, int keyChain);
    virtual bool IsHD() const {return false;};
    virtual bool IsMobi() const {return m_SubType == Mobi;}
       
    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
     void SerializationOpBase(Stream& s, Operation ser_action, int nType, int nVersion) {
        if (ser_action.ForRead())
        {
            int nType;
            int nSubType;
            READWRITE(nType);
            READWRITE(nSubType);
            m_Type = (AccountType)nType;
            m_SubType = (AccountSubType)nSubType;
            if (m_Type == AccountType::ShadowChild)
            {
                std::string sParentUUID;
                READWRITE(sParentUUID);
                parentUUID = boost::lexical_cast<boost::uuids::uuid>(sParentUUID);
            }
        }
        else
        {
            int nType = (int)m_Type;
            int nSubType = (int)m_SubType;
            READWRITE(nType);
            READWRITE(nSubType);
            if (m_Type == AccountType::ShadowChild)
            {
                std::string sParentUUID = boost::uuids::to_string(parentUUID);
                READWRITE(sParentUUID);
            }
        }
        READWRITE(earliestPossibleCreationTime);
    }
    
    template <typename Stream, typename Operation>
     void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        if (!(nType & SER_GETHASH))
            READWRITE(nVersion);
        READWRITE(vchPubKey);
        SerializationOpBase(s, ser_action, nType, nVersion);
    }
    
    
    virtual bool HaveKey(const CKeyID &address) const override;
    virtual bool HaveWatchOnly(const CScript &dest) const override;
    virtual bool HaveWatchOnly() const override;
    virtual bool HaveCScript(const CScriptID &hash) const override;
    virtual bool GetCScript(const CScriptID &hash, CScript& redeemScriptOut) const override;
    virtual bool IsLocked() const override;
    virtual bool IsCrypted() const override;
    virtual bool Lock() override;
    virtual bool Unlock(const CKeyingMaterial& vMasterKeyIn) override;
    virtual bool GetKey(const CKeyID& keyID, CKey& key) const override;
    virtual bool GetKey(const CKeyID &address, std::vector<unsigned char>& encryptedKeyOut) const override;
    virtual void GetKeys(std::set<CKeyID> &setAddress) const override;
    virtual bool EncryptKeys(CKeyingMaterial& vMasterKeyIn) override;
    virtual bool Encrypt(CKeyingMaterial& vMasterKeyIn);
    virtual bool GetPubKey(const CKeyID &address, CPubKey& vchPubKeyOut) const override;
    virtual bool AddWatchOnly(const CScript &dest) override;
    virtual bool RemoveWatchOnly(const CScript &dest) override;
    virtual bool AddCScript(const CScript& redeemScript) override;
    virtual bool AddCryptedKey(const CPubKey &vchPubKey, const std::vector<unsigned char> &vchCryptedSecret, int64_t nKeyChain);

    virtual bool AddKeyPubKey(const CKey& key, const CPubKey &pubkey) override {assert(0);};//Must never be called directly
    virtual bool AddKeyPubKey(int64_t HDKeyIndex, const CPubKey &pubkey) override {assert(0);};//Must never be called directly
    
    virtual bool HaveWalletTx(const CTransaction& tx);
    virtual bool AddKeyPubKey(const CKey& key, const CPubKey &pubkey, int keyChain);
    virtual bool AddKeyPubKey(int64_t HDKeyIndex, const CPubKey &pubkey, int keyChain);
    void AddChild(CAccount* childAccount);
    
    unsigned int GetKeyPoolSize();
    std::string getLabel() const;
    void setLabel(const std::string& label, CWalletDB* Db);
    std::string getUUID() const;
    void setUUID(const std::string& stringUUID);
    std::string getParentUUID() const;
    
    CCryptoKeyStore externalKeyStore;
    CCryptoKeyStore internalKeyStore;
    mutable CCriticalSection cs_keypool;
    std::set<int64_t> setKeyPoolInternal;
    std::set<int64_t> setKeyPoolExternal;
    AccountType m_Type;
    AccountSubType m_SubType;
    
    void possiblyUpdateEarliestTime(uint64_t creationTime, CWalletDB* Db);
    uint64_t getEarliestPossibleCreationTime();
protected:
    //We keep a UUID for internal referencing - and a label for user purposes, this way the user can freely change the label without us having to rewrite huge parts of the database on disk.
    boost::uuids::uuid accountUUID;
    boost::uuids::uuid parentUUID;
    std::string accountLabel;
    uint64_t earliestPossibleCreationTime;
    
    CKeyingMaterial vMasterKey;//Memory only.
    friend class CWallet;
};



class CAccountHD: public CAccount
{
public:
    //Normal construction.
    CAccountHD(CExtKey accountKey, boost::uuids::uuid seedID);
    //For serialization only.
    CAccountHD(){};
    
    virtual void GetKey(CExtKey& childKey, int nChain) override;
    virtual bool GetKey(const CKeyID& keyID, CKey& key) const override;
    virtual bool GetKey(const CKeyID &address, std::vector<unsigned char>& encryptedKeyOut) const override;
    virtual bool GetPubKey(const CKeyID &address, CPubKey& vchPubKeyOut) const override;
    virtual CPubKey GenerateNewKey(CWallet& wallet, int keyChain) override;
    virtual bool Lock() override;
    virtual bool Unlock(const CKeyingMaterial& vMasterKeyIn) override;
    virtual bool Encrypt(CKeyingMaterial& vMasterKeyIn) override;
    virtual bool IsCrypted() const override;
    virtual bool IsLocked() const override;
    virtual bool AddKeyPubKey(const CKey& key, const CPubKey &pubkey, int keyChain) override;
    virtual bool AddKeyPubKey(int64_t HDKeyIndex, const CPubKey &pubkey, int keyChain) override;

    void GetPubKey(CExtPubKey& childKey, int nChain) const;
    bool IsHD() const override {return true;};
    uint32_t getIndex();
    std::string getSeedUUID() const;
    CExtKey* GetAccountMasterPrivKey();   
    
    
    ADD_SERIALIZE_METHODS;
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        if (!(nType & SER_GETHASH))
            READWRITE(nVersion);
            if (ser_action.ForRead())
            {
                std::string sUUID;
                READWRITE(sUUID);
                m_SeedID = boost::lexical_cast<boost::uuids::uuid>(sUUID);
            }
            else
            {
                std::string sUUID = boost::uuids::to_string(m_SeedID);
                READWRITE(sUUID);
            }
            READWRITE(m_nIndex);
            READWRITE(m_nNextChildIndex);
            READWRITE(m_nNextChangeIndex);
            
            READWRITE(primaryChainKeyPub);
            READWRITE(changeChainKeyPub);
            
            READWRITE(encrypted);
            
            if ( IsCrypted() )
            {
                READWRITE(accountKeyPrivEncrypted);
                READWRITE(primaryChainKeyEncrypted);
                READWRITE(changeChainKeyEncrypted);
            }
            else
            {
                READWRITE(accountKeyPriv);
                READWRITE(primaryChainKeyPriv);
                READWRITE(changeChainKeyPriv);
            }
            
            SerializationOpBase(s, ser_action, nType, nVersion);
    }
private:
    boost::uuids::uuid m_SeedID;
    uint32_t m_nIndex;
    mutable uint32_t m_nNextChildIndex;
    mutable uint32_t m_nNextChangeIndex;
    
    //These members are always valid.
    CExtPubKey primaryChainKeyPub;
    CExtPubKey changeChainKeyPub;
    bool encrypted;
    
    //These members are only valid when the account is unlocked/unencrypted.
    CExtKey accountKeyPriv;         //key at m/0' (bip32) or m/44'/87'/0' (bip44)
    CExtKey primaryChainKeyPriv;    //key at m/0'/0 (bip32) or m/44'/87'/0'/0 (bip44)
    CExtKey changeChainKeyPriv;     //key at m/0'/1 (bip32) or m/44'/87'/0'/1 (bip44)
    
    //Contains the encrypted versions of the above - only valid when the account is an encrypted one.
    std::vector<unsigned char> accountKeyPrivEncrypted;
    std::vector<unsigned char> primaryChainKeyEncrypted;
    std::vector<unsigned char> changeChainKeyEncrypted;
};


#endif // GULDEN_ACCOUNT_H
