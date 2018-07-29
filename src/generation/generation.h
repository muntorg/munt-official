// Copyright (c) 2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#ifndef GULDEN_GENERATION_H
#define GULDEN_GENERATION_H

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
public:
    //fixme: (2.1): make private again.
    CAccount* account;
    CReserveKeyOrScript(CWallet* pwalletIn, CAccount* forAccount, int64_t forKeyChain);
    CReserveKeyOrScript(CScript& script);
    CReserveKeyOrScript() = default;
    CReserveKeyOrScript(const CReserveKeyOrScript&) = delete;
    CReserveKeyOrScript& operator=(const CReserveKeyOrScript&) = delete;
    CReserveKeyOrScript(CReserveKeyOrScript&&) = default;
    CReserveKeyOrScript& operator=(CReserveKeyOrScript&&) = default;
    virtual ~CReserveKeyOrScript();
    bool scriptOnly();
    void ReturnKey();
    bool GetReservedKey(CPubKey &pubkey);
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
extern CCriticalSection processBlockCS;

#endif
