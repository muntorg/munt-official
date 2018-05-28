// Copyright (c) 2016-2018 The Gulden developers
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
#include <boost/uuid/string_generator.hpp>
#include <boost/uuid/nil_generator.hpp>


#define KEYCHAIN_EXTERNAL 0
#define KEYCHAIN_CHANGE 1
#define KEYCHAIN_WITNESS KEYCHAIN_CHANGE
#define KEYCHAIN_SPENDING KEYCHAIN_EXTERNAL
const uint32_t BIP32_HARDENED_KEY_LIMIT = 0x80000000;

class CAccountHD;
class CWallet;
class CWalletDB;
class CKeyMetadata;

enum AccountState
{
    Normal = 0,        // Standard account (or HD account)
    Shadow = 1,        // Shadow account (account remains invisible until it becomes active - either through account creation or a payment)
    ShadowChild = 2,   // Shadow child account (as above but a child of another account) - used to handle legacy accounts (e.g. BIP32 child of BIP44 account that shares the same seed)
    Deleted = 3        // An account that has been deleted - we keep it arround anyway in case it receives funds, if it receives funds then we re-activate it.
};

enum AccountType
{
    Desktop = 0,       // Standard desktop account.
    Mobi = 1,          // Mobile phone. (Android, iOS)
    PoW2Witness = 2    // PoW2 witness account.
};

std::string GetAccountStateString(AccountState state);
std::string getUUIDAsString(const boost::uuids::uuid& uuid);
boost::uuids::uuid getUUIDFromString(const std::string& uuid);
std::string GetAccountTypeString(AccountType type);

const int HDDesktopStartIndex = 0;
const int HDMobileStartIndex = 100000;

enum AccountStatus
{
    WitnessEmpty,       // New witness account no funds yet.
    WitnessPending,     // New witness account first funds received but not yet confirmed.
    Default,            // Funded witness account.
    WitnessExpired,     // Expired witness account.
    WitnessEnded        // Witness account with lock ended.
};

class CHDSeed
{
public:
    enum SeedType
    {
        BIP44 = 0, // New Gulden standard based on BIP44 for /all/ Gulden wallets (desktop/iOS/android)
        BIP32 = 1, // Old Gulden android/iOS wallets
        BIP32Legacy = 2, // Very old 'guldencoin' wallets
        BIP44External = 3, //External BIP44 wallets like 'coinomi' (uses a different hash that Gulden BIP44)
        BIP44NoHardening = 4 // Same as BIP44 however with no hardening on accounts (Users have to be more careful with key security but allows for synced read only wallets... whereas with normal BIP44 only accounts are possible)
    };

    CHDSeed();
    CHDSeed(SecureString mnemonic, SeedType type);
    CHDSeed(CExtPubKey& pubkey, SeedType type);
    virtual ~CHDSeed()
    {
        //fixme: (2.1) Check if any cleanup needed here?
    }

    void Init();
    void InitReadOnly();
    CAccountHD* GenerateAccount(AccountType type, CWalletDB* Db);
    bool GetPrivKeyForAccount(uint64_t nAccountIndex, CExtKey& accountKeyPriv);


    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        int nVersion = 2;
        READWRITE(nVersion);
        //fixme: (2.1) - Remove the below in future if we need to upgrade versions; but delay as long as possible to purge all ald wallets that may have had a corrupted version.
        if (nVersion > 3)
            nVersion = 1;

        if (ser_action.ForRead())
        {
            int type;
            READWRITE(type);
            m_type = (SeedType)type;
            std::string sUUID;
            READWRITE(sUUID);
            m_UUID = getUUIDFromString(sUUID);
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
        m_nAccountIndexWitness = 200000;
        if (nVersion >= 2)
        {
            READWRITE(m_nAccountIndexWitness);
        }

        READWRITE(masterKeyPub);
        READWRITE(purposeKeyPub);
        READWRITE(cointypeKeyPub);

        READWRITE(encrypted);
        if(encrypted)
        {
            READWRITECOMPACTSIZEVECTOR(encryptedMnemonic);
            READWRITECOMPACTSIZEVECTOR(masterKeyPrivEncrypted);
            READWRITECOMPACTSIZEVECTOR(purposeKeyPrivEncrypted);
            READWRITECOMPACTSIZEVECTOR(cointypeKeyPrivEncrypted);
        }
        else
        {
            READWRITE(unencryptedMnemonic);
            READWRITE(masterKeyPriv);
            READWRITE(purposeKeyPriv);
            READWRITE(cointypeKeyPriv);
        }

        // Read may fail on older wallets, this is not a problem.
        try
        {
            READWRITE(m_readOnly);
        }
        catch(...)
        {
        }
    }

    boost::uuids::uuid getUUID() const;
    SecureString getMnemonic();
    SecureString getPubkey();

    virtual bool IsLocked() const;
    virtual bool IsCrypted() const;
    virtual bool Lock();
    virtual bool Unlock(const CKeyingMaterial& vMasterKeyIn);
    virtual bool Encrypt(const CKeyingMaterial& vMasterKeyIn);
    virtual bool IsReadOnly() { return m_readOnly; };

    // What type of seed this is (BIP32 or BIP44)
    SeedType m_type;

protected:
    CAccountHD* GenerateAccount(int nAccountIndex, AccountType type);
    // Worker function for GetPrivKeyForAccount that doesn't contain the safety checks which GetPrivKeyForAccount has.
    // Other classes should call GetPrivKeyForAccount while internally we call this to avoid unnecessary duplicate safety checks.
    bool GetPrivKeyForAccountInternal(uint64_t nAccountIndex, CExtKey& accountKeyPriv);

    // Unique seed identifier
    boost::uuids::uuid m_UUID;

    // Index of next account to generate (normal accounts)
    int m_nAccountIndex;
    // Index of next account to generate (mobile accounts)
    int m_nAccountIndexMobi;
    // Index of next account to generate (witness accounts)
    int m_nAccountIndexWitness;

    //Always available
    CExtPubKey masterKeyPub;  //hd master key (m)      - BIP32 and BIP44
    CExtPubKey purposeKeyPub; //key at m/44'           - BIP44 only
    CExtPubKey cointypeKeyPub;//key at m/44'/87'       - BIP44 only

    bool encrypted;
    // These members are only valid when the account is unlocked/unencrypted.
    SecureString unencryptedMnemonic;
    CExtKey masterKeyPriv;  //hd master key (m)      - BIP32 and BIP44
    CExtKey purposeKeyPriv; //key at m/44'           - BIP44 only
    CExtKey cointypeKeyPriv;//key at m/44'/87'       - BIP44 only
    CKeyingMaterial vMasterKey;//Memory only.

    bool m_readOnly;

    // Contains the encrypted versions of the above - only valid when the account is an encrypted one.
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

    //fixme: (2.1) (CLEANUP)
    virtual void GetKey([[maybe_unused]] CExtKey& childKey, [[maybe_unused]] int nChain) {};
    virtual CPubKey GenerateNewKey(CWallet& wallet, CKeyMetadata& metadata, int keyChain);
    virtual bool IsHD() const {return false;};
    virtual bool IsMobi() const {return m_Type == Mobi;}
    virtual bool IsPoW2Witness() const {return m_Type == PoW2Witness;}

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        if (ser_action.ForRead())
        {
            int32_t nState;
            int32_t nType;
            READWRITE(nState);
            READWRITE(nType);
            m_State = (AccountState)nState;
            m_Type = (AccountType)nType;
            if (m_State == AccountState::ShadowChild)
            {
                std::string sParentUUID;
                READWRITE(sParentUUID);
                parentUUID = getUUIDFromString(sParentUUID);
            }
        }
        else
        {
            int32_t nState = (int)m_State;
            int32_t nType = (int)m_Type;
            READWRITE(nState);
            READWRITE(nType);
            if (m_State == AccountState::ShadowChild)
            {
                std::string sParentUUID = boost::uuids::to_string(parentUUID);
                READWRITE(sParentUUID);
            }
        }
        READWRITE(earliestPossibleCreationTime);

        // Read may fail on older wallets, this is not a problem.
        try
        {
            READWRITE(m_readOnly);
        }
        catch(...)
        {
        }
    }


    virtual bool HaveKey(const CKeyID &address) const override;
    virtual bool HaveWatchOnly(const CScript &dest) const override;
    virtual bool HaveWatchOnly() const override;
    virtual bool HaveCScript(const CScriptID &hash) const override;
    virtual bool GetCScript(const CScriptID &hash, CScript& redeemScriptOut) const override;
    virtual bool IsLocked() const override;
    virtual bool IsCrypted() const override;
    virtual bool Lock() override;
    virtual bool Unlock(const CKeyingMaterial& vMasterKeyIn, bool& needsWriteToDisk) override;
    virtual bool GetKey(const CKeyID& keyID, CKey& key) const override;
    virtual bool GetKey(const CKeyID &address, std::vector<unsigned char>& encryptedKeyOut) const override;
    virtual void GetKeys(std::set<CKeyID> &setAddress) const override;
    virtual bool EncryptKeys(const CKeyingMaterial& vMasterKeyIn) override;
    virtual bool Encrypt(const CKeyingMaterial& vMasterKeyIn);
    virtual bool GetPubKey(const CKeyID &address, CPubKey& vchPubKeyOut) const override;
    virtual bool AddWatchOnly(const CScript &dest) override;
    virtual bool RemoveWatchOnly(const CScript &dest) override;
    virtual bool AddCScript(const CScript& redeemScript) override;
    virtual bool AddCryptedKey(const CPubKey &vchPubKey, const std::vector<unsigned char> &vchCryptedSecret, int64_t nKeyChain);

    virtual bool AddKeyPubKey([[maybe_unused]] const CKey& key, [[maybe_unused]] const CPubKey &pubkey) override {assert(0);};//Must never be called directly
    virtual bool AddKeyPubKey([[maybe_unused]] int64_t HDKeyIndex, [[maybe_unused]] const CPubKey &pubkey) override {assert(0);};//Must never be called directly

    virtual bool HaveWalletTx(const CTransaction& tx);
    virtual bool AddKeyPubKey(const CKey& key, const CPubKey &pubkey, int keyChain);
    virtual bool AddKeyPubKey(int64_t HDKeyIndex, const CPubKey &pubkey, int keyChain);
    void AddChild(CAccount* childAccount);

    unsigned int GetKeyPoolSize();
    std::string getLabel() const;
    void setLabel(const std::string& label, CWalletDB* Db);
    boost::uuids::uuid getUUID() const;
    void setUUID(const std::string& stringUUID);
    boost::uuids::uuid getParentUUID() const;

    bool IsReadOnly() { return m_readOnly; };

    AccountStatus GetWarningState() { return nWarningState; };
    void SetWarningState(AccountStatus nWarningState_) { nWarningState = nWarningState_; };

    CCryptoKeyStore externalKeyStore;
    CCryptoKeyStore internalKeyStore;
    mutable CCriticalSection cs_keypool;
    std::set<int64_t> setKeyPoolInternal;
    std::set<int64_t> setKeyPoolExternal;
    mutable std::map<uint256, isminetype> isminecache;
    AccountState m_State;
    AccountType m_Type;

    void possiblyUpdateEarliestTime(uint64_t creationTime, CWalletDB* Db);
    uint64_t getEarliestPossibleCreationTime();
protected:
    //We keep a UUID for internal referencing - and a label for user purposes, this way the user can freely change the label without us having to rewrite huge parts of the database on disk.
    boost::uuids::uuid accountUUID;
    boost::uuids::uuid parentUUID;
    std::string accountLabel;
    uint64_t earliestPossibleCreationTime;

    bool m_readOnly;

    CKeyingMaterial vMasterKey; // Memory only.
    friend class CGuldenWallet;
    friend class CWallet;

    AccountStatus nWarningState = AccountStatus::Default; // Memory only
};



class CAccountHD: public CAccount
{
public:
    //Normal construction.
    CAccountHD(CExtKey accountKey, boost::uuids::uuid seedID, AccountType type);
    //Read only construction.
    CAccountHD(CExtPubKey accountKey, boost::uuids::uuid seedID, AccountType type);
    //For serialization only.
    CAccountHD(){};

    virtual void GetKey(CExtKey& childKey, int nChain) override;
    virtual bool GetKey(const CKeyID& keyID, CKey& key) const override;
    virtual bool GetKey(const CKeyID &address, std::vector<unsigned char>& encryptedKeyOut) const override;
    virtual bool GetPubKey(const CKeyID &address, CPubKey& vchPubKeyOut) const override;
    virtual CPubKey GenerateNewKey(CWallet& wallet, CKeyMetadata& metadata, int keyChain) override;
    virtual bool Lock() override;
    virtual bool Unlock(const CKeyingMaterial& vMasterKeyIn, bool& needsWriteToDisk) override;
    virtual bool Encrypt(const CKeyingMaterial& vMasterKeyIn) override;
    virtual bool IsCrypted() const override;
    virtual bool IsLocked() const override;
    virtual bool AddKeyPubKey(const CKey& key, const CPubKey &pubkey, int keyChain) override;
    virtual bool AddKeyPubKey(int64_t HDKeyIndex, const CPubKey &pubkey, int keyChain) override;

    void GetPubKey(CExtPubKey& childKey, int nChain) const;
    bool IsHD() const override {return true;};
    uint32_t getIndex();
    boost::uuids::uuid getSeedUUID() const;
    CExtKey* GetAccountMasterPrivKey();
    SecureString GetAccountMasterPubKeyEncoded();


    ADD_SERIALIZE_METHODS;
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        int nVersion = 2;
        READWRITE(nVersion);
        if (nVersion > 3)
            nVersion = 1;


        if (ser_action.ForRead())
        {
            std::string sUUID;
            READWRITE(sUUID);
            m_SeedID = getUUIDFromString(sUUID);
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
            READWRITECOMPACTSIZEVECTOR(accountKeyPrivEncrypted);
            READWRITECOMPACTSIZEVECTOR(primaryChainKeyEncrypted);
            READWRITECOMPACTSIZEVECTOR(changeChainKeyEncrypted);
        }
        else
        {
            READWRITE(accountKeyPriv);
            READWRITE(primaryChainKeyPriv);
            READWRITE(changeChainKeyPriv);
        }

        CAccount::SerializationOp(s, ser_action);
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
    CExtKey accountKeyPriv;         //key at m/0' (bip32) or m/44'/87'/n' (bip44)
    CExtKey primaryChainKeyPriv;    //key at m/0'/0 (bip32) or m/44'/87'/0'/0 (bip44)
    CExtKey changeChainKeyPriv;     //key at m/0'/1 (bip32) or m/44'/87'/0'/1 (bip44)

    //Contains the encrypted versions of the above - only valid when the account is an encrypted one.
    std::vector<unsigned char> accountKeyPrivEncrypted;
    std::vector<unsigned char> primaryChainKeyEncrypted;
    std::vector<unsigned char> changeChainKeyEncrypted;
};


#endif // GULDEN_ACCOUNT_H
