// Copyright (c) 2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "zmqabstractnotifier.h"
#include "util.h"


CZMQAbstractNotifier::~CZMQAbstractNotifier()
{
    assert(!psocket);
}

bool CZMQAbstractNotifier::NotifyBlock(const CBlockIndex* /*CBlockIndex*/)
{
    return true;
}

bool CZMQAbstractNotifier::NotifyStalledWitness(const CBlockIndex* /*pDelayedIndex*/, uint64_t /*nSecondsDelayed*/)
{
    return true;
}

bool CZMQAbstractNotifier::NotifyTransaction(const CTransaction& /*transaction*/)
{
    return true;
}

#ifdef ENABLE_WALLET
bool CZMQAbstractNotifier::NotifyWalletTransaction(CWallet* const pWallet, const CWalletTx& /*wtx*/)
{
    return true;
}
#endif
