// Copyright (c) 2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// File contains modifications by: The Gulden developers
// All modifications:
// Copyright (c) 2017-2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#include "policy/rbf.h"

//fixme: (2.1) we can make this the only behaviour and remove the previous behaviour.
static bool SignalsOptInRBFSegSig(const CTransaction &tx)
{
    for(const CTxIn &txin : tx.vin)
    {
        if (txin.FlagIsSet(CTxInFlags::OptInRBF))
        {
            return true;
        }
    }
    return false;
}

bool SignalsOptInRBF(const CTransaction &tx)
{
    if (!IsOldTransactionVersion(tx.nVersion))
        return SignalsOptInRBFSegSig(tx);

    for(const CTxIn &txin : tx.vin)
    {
        if (txin.GetSequence(tx.nVersion) < std::numeric_limits<unsigned int>::max()-1)
        {
            return true;
        }
    }
    return false;
}

RBFTransactionState IsRBFOptIn(const CTransaction &tx, CTxMemPool &pool)
{
    AssertLockHeld(pool.cs);

    CTxMemPool::setEntries setAncestors;

    // First check the transaction itself.
    if (SignalsOptInRBF(tx))
    {
        return RBF_TRANSACTIONSTATE_REPLACEABLE_BIP125;
    }

    // If this transaction is not in our mempool, then we can't be sure
    // we will know about all its inputs.
    if (!pool.exists(tx.GetHash()))
    {
        return RBF_TRANSACTIONSTATE_UNKNOWN;
    }

    // If all the inputs have nSequence >= maxint-1, it still might be
    // signaled for RBF if any unconfirmed parents have signaled.
    uint64_t noLimit = std::numeric_limits<uint64_t>::max();
    std::string dummy;
    CTxMemPoolEntry entry = *pool.mapTx.find(tx.GetHash());
    pool.CalculateMemPoolAncestors(entry, setAncestors, noLimit, noLimit, noLimit, noLimit, dummy, false);

    for(CTxMemPool::txiter it : setAncestors)
    {
        if (SignalsOptInRBF(it->GetTx()))
        {
            return RBF_TRANSACTIONSTATE_REPLACEABLE_BIP125;
        }
    }
    return RBF_TRANSACTIONSTATE_FINAL;
}
