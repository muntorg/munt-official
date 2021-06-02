// Copyright (c) 2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef ZMQ_ZMQABSTRACTNOTIFIER_H
#define ZMQ_ZMQABSTRACTNOTIFIER_H

#include "zmqconfig.h"
#ifdef ENABLE_WALLET
#include <wallet/wallet.h>
#endif

class CBlockIndex;
class CZMQAbstractNotifier;

typedef CZMQAbstractNotifier* (*CZMQNotifierFactory)();

class CZMQAbstractNotifier
{
public:
    CZMQAbstractNotifier() : psocket(0) { }
    virtual ~CZMQAbstractNotifier();

    template <typename T>
    static CZMQAbstractNotifier* Create()
    {
        return new T();
    }

    std::string GetType() const { return type; }
    void SetType(const std::string &t) { type = t; }
    std::string GetAddress() const { return address; }
    void SetAddress(const std::string &a) { address = a; }

    virtual bool Initialize(void *pcontext) = 0;
    virtual void Shutdown() = 0;

    virtual bool NotifyBlock(const CBlockIndex *pindex);
    virtual bool NotifyStalledWitness(const CBlockIndex* pDelayedIndex, uint64_t nSecondsDelayed);
    virtual bool NotifyTransaction(const CTransaction &transaction);
    #ifdef ENABLE_WALLET
    virtual bool NotifyWalletTransaction(CWallet* const pWallet, const CWalletTx &wtx);
    #endif

protected:
    void *psocket;
    std::string type;
    std::string address;
};

#endif // ZMQ_ZMQABSTRACTNOTIFIER_H
