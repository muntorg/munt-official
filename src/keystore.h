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

#ifndef BITCOIN_KEYSTORE_H
#define BITCOIN_KEYSTORE_H

#include "key.h"
#include "pubkey.h"
#include "script/script.h"
#include "script/standard.h"
#include "sync.h"

#include "util.h"

#include <boost/signals2/signal.hpp>
#include <boost/variant.hpp>

#include "base58.h"

/** A virtual base class for key stores */
class CKeyStore {
protected:
    mutable CCriticalSection cs_KeyStore;

public:
    virtual ~CKeyStore() {}

    virtual bool AddKeyPubKey(const CKey& key, const CPubKey& pubkey) = 0;
    virtual bool AddKeyPubKey(int64_t HDKeyIndex, const CPubKey& pubkey) = 0;
    virtual bool AddKey(const CKey& key);

    virtual bool HaveKey(const CKeyID& address) const = 0;
    virtual bool GetKey(const CKeyID& address, CKey& keyOut) const = 0;
    virtual bool GetKey(const CKeyID& address, int64_t& HDKeyIndex) const = 0;
    virtual void GetKeys(std::set<CKeyID>& setAddress) const = 0;
    virtual bool GetPubKey(const CKeyID& address, CPubKey& vchPubKeyOut) const = 0;

    virtual bool AddCScript(const CScript& redeemScript) = 0;
    virtual bool HaveCScript(const CScriptID& hash) const = 0;
    virtual bool GetCScript(const CScriptID& hash, CScript& redeemScriptOut) const = 0;

    virtual bool AddWatchOnly(const CScript& dest) = 0;
    virtual bool RemoveWatchOnly(const CScript& dest) = 0;
    virtual bool HaveWatchOnly(const CScript& dest) const = 0;
    virtual bool HaveWatchOnly() const = 0;
};

typedef std::map<CKeyID, CKey> KeyMap;
typedef std::map<CKeyID, int64_t> KeyMapHD;
typedef std::map<CKeyID, CPubKey> WatchKeyMap;
typedef std::map<CScriptID, CScript> ScriptMap;
typedef std::set<CScript> WatchOnlySet;

/** Basic key store, that keeps keys in an address->secret map */
class CBasicKeyStore : public CKeyStore {
protected:
    KeyMap mapKeys;
    KeyMapHD mapHDKeys;
    WatchKeyMap mapWatchKeys;
    ScriptMap mapScripts;
    WatchOnlySet setWatchOnly;

public:
    bool AddKeyPubKey(const CKey& key, const CPubKey& pubkey);
    bool AddKeyPubKey(int64_t HDKeyIndex, const CPubKey& pubkey);
    bool GetPubKey(const CKeyID& address, CPubKey& vchPubKeyOut) const;
    bool HaveKey(const CKeyID& address) const
    {
        bool result = false;
        {
            LOCK(cs_KeyStore);
            result = (mapKeys.count(address) > 0);
        }
        if (!result) {
            LOCK(cs_KeyStore);
            result = (mapHDKeys.count(address) > 0);
        }

        return result;
    }
    void GetKeys(std::set<CKeyID>& setAddress) const
    {
        setAddress.clear();
        {
            LOCK(cs_KeyStore);
            KeyMap::const_iterator mi = mapKeys.begin();
            while (mi != mapKeys.end()) {
                setAddress.insert((*mi).first);
                mi++;
            }
            KeyMapHD::const_iterator mi2 = mapHDKeys.begin();
            while (mi2 != mapHDKeys.end()) {
                setAddress.insert((*mi2).first);
                mi2++;
            }
        }
    }
    bool GetKey(const CKeyID& address, CKey& keyOut) const
    {
        {
            LOCK(cs_KeyStore);
            KeyMap::const_iterator mi = mapKeys.find(address);
            if (mi != mapKeys.end()) {
                keyOut = mi->second;
                return true;
            }
        }
        return false;
    }
    bool GetKey(const CKeyID& address, int64_t& HDKeyIndex) const
    {
        {
            LOCK(cs_KeyStore);
            KeyMapHD::const_iterator mi = mapHDKeys.find(address);
            if (mi != mapHDKeys.end()) {
                HDKeyIndex = mi->second;
                return true;
            }
        }
        return false;
    }
    virtual bool AddCScript(const CScript& redeemScript);
    virtual bool HaveCScript(const CScriptID& hash) const;
    virtual bool GetCScript(const CScriptID& hash, CScript& redeemScriptOut) const;

    virtual bool AddWatchOnly(const CScript& dest);
    virtual bool RemoveWatchOnly(const CScript& dest);
    virtual bool HaveWatchOnly(const CScript& dest) const;
    virtual bool HaveWatchOnly() const;
};

typedef std::vector<unsigned char, secure_allocator<unsigned char> > CKeyingMaterial;
typedef std::map<CKeyID, std::pair<CPubKey, std::vector<unsigned char> > > CryptedKeyMap;

#endif // BITCOIN_KEYSTORE_H
