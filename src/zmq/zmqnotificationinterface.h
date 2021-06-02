// Copyright (c) 2015-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef ZMQ_ZMQNOTIFICATIONINTERFACE_H
#define ZMQ_ZMQNOTIFICATIONINTERFACE_H

#include "validation/validationinterface.h"
#include <string>
#include <map>
#include <list>

class CBlockIndex;
class CZMQAbstractNotifier;

class CZMQNotificationInterface : public CValidationInterface
{
public:
    virtual ~CZMQNotificationInterface();

    static CZMQNotificationInterface* Create();

protected:
    bool Initialize();
    void Shutdown();

    // CValidationInterface
    void StalledWitness(const CBlockIndex* pBlock, uint64_t nSeconds) override;
    void TransactionAddedToMempool(const CTransactionRef& tx) override;
    #ifdef ENABLE_WALLET
    void WalletTransactionAdded(CWallet* const pWallet, const CWalletTx& wtx) override;
    #endif
    void BlockConnected(const std::shared_ptr<const CBlock>& pblock, const CBlockIndex* pindexConnected, const std::vector<CTransactionRef>& vtxConflicted) override;
    void BlockDisconnected(const std::shared_ptr<const CBlock>& pblock) override;
    void UpdatedBlockTip(const CBlockIndex *pindexNew, const CBlockIndex *pindexFork, bool fInitialDownload) override;

private:
    CZMQNotificationInterface();

    void *pcontext;
    std::list<CZMQAbstractNotifier*> notifiers;
};

#endif // ZMQ_ZMQNOTIFICATIONINTERFACE_H
