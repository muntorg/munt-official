// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// File contains modifications by: The Gulden developers
// All modifications:
// Copyright (c) 2016-2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

/**
 * Why base-58 instead of standard base-64 encoding?
 * - Don't want 0OIl characters that look the same in some fonts and
 *      could be used to create visually identical looking data.
 * - A string with non-alphanumeric characters is not as easily accepted as input.
 * - E-mail usually won't line-break if there's no punctuation to break at.
 * - Double-clicking selects the whole string as one word if it's all alphanumeric.
 */
#ifndef GULDEN_BASE58_H
#define GULDEN_BASE58_H

#include "chainparams.h"
#include "key.h"
#include "pubkey.h"
#include "script/script.h"
#include "script/standard.h"
#include "support/allocators/zeroafterfree.h"

#include <string>
#include <vector>

/**
 * Encode a byte sequence as a base58-encoded string.
 * pbegin and pend cannot be NULL, unless both are.
 */
std::string EncodeBase58(const unsigned char* pbegin, const unsigned char* pend);

/**
 * Encode a byte vector as a base58-encoded string
 */
std::string EncodeBase58(const std::vector<unsigned char>& vch);

/**
 * Decode a base58-encoded string (psz) into a byte vector (vchRet).
 * return true if decoding is successful.
 * psz cannot be NULL.
 */
bool DecodeBase58(const char* psz, std::vector<unsigned char>& vchRet);

/**
 * Decode a base58-encoded string (str) into a byte vector (vchRet).
 * return true if decoding is successful.
 */
bool DecodeBase58(const std::string& str, std::vector<unsigned char>& vchRet);

/**
 * Encode a byte vector into a base58-encoded string, including checksum
 */
std::string EncodeBase58Check(const std::vector<unsigned char>& vchIn);

/**
 * Decode a base58-encoded string (psz) that includes a checksum into a byte
 * vector (vchRet), return true if decoding is successful
 */
inline bool DecodeBase58Check(const char* psz, std::vector<unsigned char>& vchRet);

/**
 * Decode a base58-encoded string (str) that includes a checksum into a byte
 * vector (vchRet), return true if decoding is successful
 */
inline bool DecodeBase58Check(const std::string& str, std::vector<unsigned char>& vchRet);

/**
 * Base class for all base58-encoded data
 */
class CBase58Data
{
protected:
    //! the version byte(s)
    std::vector<unsigned char> vchVersion;

    //! the actually encoded data
    typedef std::vector<unsigned char, zero_after_free_allocator<unsigned char> > vector_uchar;
    vector_uchar vchData;

    CBase58Data();
    void SetData(const std::vector<unsigned char> &vchVersionIn, const void* pdata, size_t nSize);
    void SetData(const std::vector<unsigned char> &vchVersionIn, const unsigned char *pbegin, const unsigned char *pend);

public:
    bool SetString(const char* psz, unsigned int nVersionBytes = 1);
    bool SetString(const std::string& str);
    std::string ToString() const;
    int CompareTo(const CBase58Data& b58) const;

    bool operator==(const CBase58Data& b58) const { return CompareTo(b58) == 0; }
    bool operator<=(const CBase58Data& b58) const { return CompareTo(b58) <= 0; }
    bool operator>=(const CBase58Data& b58) const { return CompareTo(b58) >= 0; }
    bool operator< (const CBase58Data& b58) const { return CompareTo(b58) <  0; }
    bool operator> (const CBase58Data& b58) const { return CompareTo(b58) >  0; }
};

/** base58-encoded Gulden addresses.
 * Public-key-hash-addresses have version 0 (or 111 testnet).
 * The data vector contains RIPEMD160(SHA256(pubkey)), where pubkey is the serialized public key.
 * Script-hash-addresses have version 5 (or 196 testnet).
 * The data vector contains RIPEMD160(SHA256(cscript)), where cscript is the serialized redemption script.
 */
class CGuldenAddress : public CBase58Data {
public:
    bool Set(const CKeyID& spendingKeyID, const CKeyID& witnessKeyID);
    bool Set(const CKeyID &id);
    bool Set(const CScriptID &id);
    bool Set(const CTxDestination &dest);
    bool IsValid() const;
    bool IsValid(const CChainParams &params) const;
    bool IsValidWitness(const CChainParams& params) const;
    bool IsValidBitcoin() const;

    CGuldenAddress() {}
    CGuldenAddress(const CTxDestination &dest) { Set(dest); }
    CGuldenAddress(const std::string& strAddress) { SetString(strAddress); }
    CGuldenAddress(const char* pszAddress) { SetString(pszAddress); }

    CTxDestination Get() const;
    bool GetKeyID(CKeyID &keyID) const;
    bool IsScript() const;
};

/**
 * A base58-encoded secret key
 */
class CGuldenSecret : public CBase58Data
{
public:
    void SetKey(const CKey& vchSecret);
    CKey GetKey();
    bool IsValid() const;
    bool SetString(const char* pszSecret);
    bool SetString(const std::string& strSecret);

    CGuldenSecret(const CKey& vchSecret) { SetKey(vchSecret); }
    CGuldenSecret() {}
};

/**
 * A combination base58 and hex encoded secret extended key
 */
template <typename KeyType> class CGuldenSecretExt 
{
public:
    void SetKey(const KeyType& vchSecret) { key = vchSecret; }
    KeyType GetKey()
    {
        //fixme: (Bitcoin) Strip creationTime and payAccount
        KeyType retExt;

        SecureString secretKey = SecureString(secret.begin(), secret.begin() + secret.find('-'));
        SecureString secretCode = SecureString(secret.begin() + secret.find('-') + 1, secret.end());

        std::vector<unsigned char> vchSecretKey;
        std::vector<unsigned char> vchSecretCode;
        DecodeBase58(secretKey.c_str(), vchSecretKey);
        DecodeBase58(secretCode.c_str(), vchSecretCode);

        if (vchSecretCode.size() == 32)
        {
            retExt.GetMutableKey().Set(vchSecretKey.begin(), vchSecretKey.end());
            retExt.chaincode = uint256(vchSecretCode);
        }

        return retExt;
    }

    bool SetString(const char* pszSecret)
    {
        secret = pszSecret;
        return true;
    }


    bool SetString(const std::string& strSecret)
    {
        secret = strSecret;
        return true;
    }

    std::string ToString(std::string creationtime, std::string payAccount) const
    {
        std::string ret = ToString();

        const unsigned char* creationTimeRaw = (const unsigned char*)creationtime.c_str();
        ret = ret + ":" + EncodeBase58( creationTimeRaw, creationTimeRaw + creationtime.length() );

        ret = ret + ";" + payAccount;

        return ret;
    }

    std::string ToString() const
    {
        std::string ret =  EncodeBase58( (const unsigned char*)key.GetKey().begin(), (const unsigned char*)key.GetKey().end() );
        ret = ret + "-" + EncodeBase58( key.chaincode.begin(), key.chaincode.end() );

        return ret;
    }

    CGuldenSecretExt(const KeyType& vchSecret) { SetKey(vchSecret); }
    CGuldenSecretExt(const std::string& strSecret) { SetString(strSecret); }
    CGuldenSecretExt() {}

private:
    KeyType key;
    std::string secret;
};

template<typename K, int Size, CChainParams::Base58Type Type> class CGuldenExtKeyBase : public CBase58Data
{
public:
    void SetKey(const K &key) {
        unsigned char vch[Size];
        key.Encode(vch);
        SetData(Params().Base58Prefix(Type), vch, vch+Size);
    }

    K GetKey() {
        K ret;
        if (vchData.size() == Size) {
            // If base58 encoded data does not hold an ext key, return a !IsValid() key
            ret.Decode(&vchData[0]);
        }
        return ret;
    }

    CGuldenExtKeyBase(const K &key) {
        SetKey(key);
    }

    CGuldenExtKeyBase(const std::string& strBase58c) {
        SetString(strBase58c.c_str(), Params().Base58Prefix(Type).size());
    }

    CGuldenExtKeyBase() {}
};

typedef CGuldenExtKeyBase<CExtKey, BIP32_EXTKEY_SIZE, CChainParams::EXT_SECRET_KEY> CGuldenExtKey;
typedef CGuldenExtKeyBase<CExtPubKey, BIP32_EXTKEY_SIZE, CChainParams::EXT_PUBLIC_KEY> CGuldenExtPubKey;

#endif // GULDEN_BASE58_H
