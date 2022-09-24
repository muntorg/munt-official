// Copyright (c) 2018-2022 The Centure developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the Libre Chain License, see the accompanying
// file COPYING

#ifndef GENERATION_GENERATION_H
#define GENERATION_GENERATION_H

#include "sync.h" //CCriticalSection
#include "script/script.h"

#ifdef ENABLE_WALLET
#include "pubkey.h"

class CWallet;
class CAccount;
/** A key allocated from the key pool. */
class CReserveKeyOrScript : public CReserveScript
{
protected:
    CWallet* pwallet;
    int64_t nIndex;
    int64_t nKeyChain;
    CPubKey vchPubKey;
    CKeyID pubKeyID;
public:
    //fixme: (POST-PHASE5): make private again.
    CAccount* account;
    CReserveKeyOrScript(CWallet* pwalletIn, CAccount* forAccount, int64_t forKeyChain);
    CReserveKeyOrScript(CScript& script);
    CReserveKeyOrScript(CPubKey &pubkey);
    CReserveKeyOrScript(CKeyID &pubKeyID_);
    CReserveKeyOrScript() = default;
    CReserveKeyOrScript(const CReserveKeyOrScript&) = delete;
    CReserveKeyOrScript& operator=(const CReserveKeyOrScript&) = delete;
    CReserveKeyOrScript(CReserveKeyOrScript&&) = default;
    CReserveKeyOrScript& operator=(CReserveKeyOrScript&&) = default;
    virtual ~CReserveKeyOrScript();
    bool scriptOnly();
    void ReturnKey();
    bool GetReservedKey(CPubKey &pubkey);
    bool GetReservedKeyID(CKeyID &pubKeyID_);
    void KeepKey();
    void KeepScript() override;
    void keepScriptOnDestroy() override;
private:
    bool shouldKeepOnDestroy=false;
};
#else
typedef CReserveScript CReserveKeyOrScript;
#endif

//! Prevent both mining and witnessing from trying to process a block at the same time.
extern RecursiveMutex processBlockCS;

#endif
