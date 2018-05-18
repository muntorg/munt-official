// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// File contains modifications by: The Gulden developers
// All modifications:
// Copyright (c) 2016-2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#include "wallet/wallet.h"
#include "wallet/wallettx.h"

#include "validation.h"
#include "policy/policy.h"
#include "Gulden/util.h"

// Note that this function doesn't distinguish between a 0-valued input,
// and a not-"is mine" (according to the filter) input.
CAmount CWallet::GetDebit(const CTxIn &txin, const isminefilter& filter, CAccount* forAccount) const
{
    {
        LOCK(cs_wallet);
        std::map<uint256, CWalletTx>::const_iterator mi = mapWallet.find(txin.prevout.hash);
        if (mi != mapWallet.end())
        {
            const CWalletTx& prev = (*mi).second;
            if (txin.prevout.n < prev.tx->vout.size())
            {
                if ( (forAccount && (::IsMine(*forAccount, prev.tx->vout[txin.prevout.n]) & filter)) || (!forAccount && (IsMine(prev.tx->vout[txin.prevout.n]) & filter)) )
                {
                    return prev.tx->vout[txin.prevout.n].nValue;
                }
            }
        }
    }
    return 0;
}


CAmount CWallet::GetCredit(const CTxOut& txout, const isminefilter& filter, CAccount* forAccount) const
{
    if (!MoneyRange(txout.nValue))
        throw std::runtime_error(std::string(__func__) + ": value out of range");
    return ( ( (forAccount && ::IsMine(*forAccount, txout) & filter) || (!forAccount && IsMine(txout) & filter) ) ? txout.nValue : 0);
}

CAmount CWallet::GetChange(const CTxOut& txout) const
{
    if (!MoneyRange(txout.nValue))
        throw std::runtime_error(std::string(__func__) + ": value out of range");
    return (IsChange(txout) ? txout.nValue : 0);
}

// Note that this function doesn't distinguish between a 0-valued input,
// and a not-"is mine" (according to the filter) input.
CAmount CWallet::GetDebit(const CTransaction& tx, const isminefilter& filter, CAccount* forAccount) const
{
    CAmount nDebit = 0;
    for(const CTxIn& txin : tx.vin)
    {
        nDebit += GetDebit(txin, filter, forAccount);
        if (!MoneyRange(nDebit))
            throw std::runtime_error(std::string(__func__) + ": value out of range");
    }
    return nDebit;
}

CAmount CWallet::GetCredit(const CTransaction& tx, const isminefilter& filter, CAccount* forAccount) const
{
    CAmount nCredit = 0;
    for(const CTxOut& txout : tx.vout)
    {
        nCredit += GetCredit(txout, filter, forAccount);
        if (!MoneyRange(nCredit))
            throw std::runtime_error(std::string(__func__) + ": value out of range");
    }
    return nCredit;
}

CAmount CWallet::GetChange(const CTransaction& tx) const
{
    CAmount nChange = 0;
    for(const CTxOut& txout : tx.vout)
    {
        nChange += GetChange(txout);
        if (!MoneyRange(nChange))
            throw std::runtime_error(std::string(__func__) + ": value out of range");
    }
    return nChange;
}

CAmount CWalletTx::GetDebit(const isminefilter& filter, CAccount* forAccount) const
{
    if (tx->vin.empty())
        return 0;

    CAmount debit = 0;
    if(filter & ISMINE_SPENDABLE)
    {
        if (debitCached.find(forAccount) != debitCached.end())
            debit += debitCached[forAccount];
        else
        {
            debitCached[forAccount] = pwallet->GetDebit(*this, ISMINE_SPENDABLE, forAccount);
            debit += debitCached[forAccount];
        }
    }
    if(filter & ISMINE_WATCH_ONLY)
    {
        if (watchDebitCached.find(forAccount) != watchDebitCached.end())
            debit += watchDebitCached[forAccount];
        else
        {
            watchDebitCached[forAccount] = pwallet->GetDebit(*this, ISMINE_WATCH_ONLY, forAccount);
            debit += watchDebitCached[forAccount];
        }
    }
    return debit;
}

CAmount CWalletTx::GetCredit(const isminefilter& filter, CAccount* forAccount) const
{
    // Must wait until coinbase is safely deep enough in the chain before valuing it
    if (IsCoinBase() && GetBlocksToMaturity() > 0)
        return 0;

    CAmount credit = 0;
    if (filter & ISMINE_SPENDABLE)
    {
        // GetBalance can assume transactions in mapWallet won't change
        if (creditCached.find(forAccount) != creditCached.end())
            credit += creditCached[forAccount];
        else
        {
            creditCached[forAccount] = pwallet->GetCredit(*this, ISMINE_SPENDABLE, forAccount);
            credit += creditCached[forAccount];
        }
    }
    if (filter & ISMINE_WATCH_ONLY)
    {
        if (watchCreditCached.find(forAccount) != watchCreditCached.end())
            credit += watchCreditCached[forAccount];
        else
        {
            watchCreditCached[forAccount] = pwallet->GetCredit(*this, ISMINE_WATCH_ONLY, forAccount);
            credit += watchCreditCached[forAccount];
        }
    }
    return credit;
}

CAmount CWalletTx::GetImmatureCredit(bool fUseCache, const CAccount* forAccount) const
{
    if (IsCoinBase() && GetBlocksToMaturity() > 0 && IsInMainChain())
    {
        if (fUseCache && immatureCreditCached.find(forAccount) != immatureCreditCached.end())
            return immatureCreditCached[forAccount];
        immatureCreditCached[forAccount] = pwallet->GetCredit(*this, ISMINE_SPENDABLE);
        return immatureCreditCached[forAccount];
    }

    return 0;
}

CAmount CWalletTx::GetAvailableCredit(bool fUseCache, const CAccount* forAccount) const
{
    if (pwallet == 0)
        return 0;

    // Must wait until coinbase is safely deep enough in the chain before valuing it
    if (IsCoinBase() && GetBlocksToMaturity() > 0)
        return 0;

    if (fUseCache && availableCreditCached.find(forAccount) != availableCreditCached.end())
        return availableCreditCached[forAccount];

    CAmount nCredit = 0;
    uint256 hashTx = GetHash();
    for (unsigned int i = 0; i < tx->vout.size(); i++)
    {
        if (!pwallet->IsSpent(hashTx, i))
        {
            const CTxOut &txout = tx->vout[i];
            if (!forAccount || IsMine(*forAccount, txout))
            {
                if (!IsPoW2WitnessLocked(txout, chainActive.Tip()->nHeight))
                {
                    nCredit += pwallet->GetCredit(txout, ISMINE_SPENDABLE);
                    if (!MoneyRange(nCredit))
                        throw std::runtime_error("CWalletTx::GetAvailableCredit() : value out of range");
                }
            }
        }
    }

    availableCreditCached[forAccount] = nCredit;

    return nCredit;
}

CAmount CWalletTx::GetAvailableCreditIncludingLockedWitnesses(bool fUseCache, const CAccount* forAccount) const
{
    if (pwallet == 0)
        return 0;

    // Must wait until coinbase is safely deep enough in the chain before valuing it
    if (IsCoinBase() && GetBlocksToMaturity() > 0)
        return 0;

    if (fUseCache && availableCreditCachedIncludingLockedWitnesses.find(forAccount) != availableCreditCachedIncludingLockedWitnesses.end())
        return availableCreditCachedIncludingLockedWitnesses[forAccount];

    CAmount nCredit = 0;
    uint256 hashTx = GetHash();
    for (unsigned int i = 0; i < tx->vout.size(); i++)
    {
        if (!pwallet->IsSpent(hashTx, i))
        {
            const CTxOut &txout = tx->vout[i];
            if (!forAccount || IsMine(*forAccount, txout))
            {
                nCredit += pwallet->GetCredit(txout, ISMINE_SPENDABLE);
                if (!MoneyRange(nCredit))
                    throw std::runtime_error("CWalletTx::GetAvailableCredit() : value out of range");
            }
        }
    }

    availableCreditCachedIncludingLockedWitnesses[forAccount] = nCredit;

    return nCredit;
}

CAmount CWalletTx::GetImmatureWatchOnlyCredit(const bool& fUseCache, const CAccount* forAccount) const
{
    if (IsCoinBase() && GetBlocksToMaturity() > 0 && IsInMainChain())
    {
        if (fUseCache && immatureWatchCreditCached.find(forAccount) != immatureWatchCreditCached.end())
            return immatureWatchCreditCached[forAccount];
        immatureWatchCreditCached[forAccount] = pwallet->GetCredit(*this, ISMINE_WATCH_ONLY);
        return immatureWatchCreditCached[forAccount];
    }

    return 0;
}

CAmount CWalletTx::GetAvailableWatchOnlyCredit(const bool& fUseCache, const CAccount* forAccount) const
{
    if (pwallet == 0)
        return 0;

    // Must wait until coinbase is safely deep enough in the chain before valuing it
    if (IsCoinBase() && GetBlocksToMaturity() > 0)
        return 0;

    if (fUseCache && availableWatchCreditCached.find(forAccount) != availableWatchCreditCached.end())
        return availableWatchCreditCached[forAccount];

    CAmount nCredit = 0;
    for (unsigned int i = 0; i < tx->vout.size(); i++)
    {
        if (!pwallet->IsSpent(GetHash(), i))
        {
            const CTxOut &txout = tx->vout[i];
            nCredit += pwallet->GetCredit(txout, ISMINE_WATCH_ONLY);
            if (!MoneyRange(nCredit))
                throw std::runtime_error("CWalletTx::GetAvailableCredit() : value out of range");
        }
    }

    availableWatchCreditCached[forAccount] = nCredit;
    return nCredit;
}

CAmount CWalletTx::GetChange() const
{
    if (fChangeCached)
        return nChangeCached;
    nChangeCached = pwallet->GetChange(*this);
    fChangeCached = true;
    return nChangeCached;
}

extern bool IsMine(const CAccount* forAccount, const CWalletTx& tx);

CAmount CWallet::GetBalance(const CAccount* forAccount, bool includePoW2LockedWitnesses, bool includeChildren) const
{
    CAmount nTotal = 0;
    {
        LOCK2(cs_main, cs_wallet);
        for (std::map<uint256, CWalletTx>::const_iterator it = mapWallet.begin(); it != mapWallet.end(); ++it)
        {
            const CWalletTx* pcoin = &(*it).second;
            //fixme: (Post-2.1) - is this okay? Should it be cached or something? (CBSU?)
            if (!forAccount || ::IsMine(forAccount, *pcoin))
            {
                if (pcoin->IsTrusted() && !pcoin->isAbandoned())
                {
                    nTotal += includePoW2LockedWitnesses ? pcoin->GetAvailableCreditIncludingLockedWitnesses(true, forAccount) : pcoin->GetAvailableCredit(true, forAccount);
                }
            }

        }
    }
    if (forAccount && includeChildren)
    {
        for (const auto& accountItem : mapAccounts)
        {
            const auto& childAccount = accountItem.second;
            if (childAccount->getParentUUID() == forAccount->getUUID())
            {
                nTotal += GetBalance(childAccount, includePoW2LockedWitnesses, false);
            }
        }
    }

    return nTotal;
}

CAmount CWallet::GetUnconfirmedBalance(const CAccount* forAccount, bool includeChildren) const
{
    CAmount nTotal = 0;
    {
        LOCK2(cs_main, cs_wallet);
        for (std::map<uint256, CWalletTx>::const_iterator it = mapWallet.begin(); it != mapWallet.end(); ++it)
        {
            const CWalletTx* pcoin = &(*it).second;
            //fixme: (Post-2.1)- is this okay? Should it be cached or something? (CBSU?)
            if (!forAccount || ::IsMine(forAccount, *pcoin))
            {
                if (!pcoin->IsTrusted() && pcoin->GetDepthInMainChain() == 0 && pcoin->InMempool())
                    nTotal += pcoin->GetAvailableCredit(true, forAccount);
            }
        }
    }
    if (forAccount && includeChildren)
    {
        for (const auto& accountItem : mapAccounts)
        {
            const auto& childAccount = accountItem.second;
            if (childAccount->getParentUUID() == forAccount->getUUID())
            {
                nTotal += GetUnconfirmedBalance(childAccount, false);
            }
        }
    }
    return nTotal;
}

CAmount CWallet::GetImmatureBalance(const CAccount* forAccount) const
{
    CAmount nTotal = 0;
    {
        LOCK2(cs_main, cs_wallet);
        for (std::map<uint256, CWalletTx>::const_iterator it = mapWallet.begin(); it != mapWallet.end(); ++it)
        {
            const CWalletTx* pcoin = &(*it).second;
            if (!forAccount || ::IsMine(forAccount, *pcoin))
            {
                nTotal += pcoin->GetImmatureCredit(true, forAccount);
            }
        }
    }
    return nTotal;
}

CAmount CWallet::GetWatchOnlyBalance() const
{
    CAmount nTotal = 0;
    {
        LOCK2(cs_main, cs_wallet);
        for (std::map<uint256, CWalletTx>::const_iterator it = mapWallet.begin(); it != mapWallet.end(); ++it)
        {
            const CWalletTx* pcoin = &(*it).second;
            if (pcoin->IsTrusted())
                nTotal += pcoin->GetAvailableWatchOnlyCredit();
        }
    }

    return nTotal;
}

CAmount CWallet::GetUnconfirmedWatchOnlyBalance() const
{
    CAmount nTotal = 0;
    {
        LOCK2(cs_main, cs_wallet);
        for (std::map<uint256, CWalletTx>::const_iterator it = mapWallet.begin(); it != mapWallet.end(); ++it)
        {
            const CWalletTx* pcoin = &(*it).second;
            if (!pcoin->IsTrusted() && pcoin->GetDepthInMainChain() == 0 && pcoin->InMempool())
                nTotal += pcoin->GetAvailableWatchOnlyCredit();
        }
    }
    return nTotal;
}

CAmount CWallet::GetImmatureWatchOnlyBalance() const
{
    CAmount nTotal = 0;
    {
        LOCK2(cs_main, cs_wallet);
        for (std::map<uint256, CWalletTx>::const_iterator it = mapWallet.begin(); it != mapWallet.end(); ++it)
        {
            const CWalletTx* pcoin = &(*it).second;
            nTotal += pcoin->GetImmatureWatchOnlyCredit();
        }
    }
    return nTotal;
}

// Calculate total balance in a different way from GetBalance. The biggest
// difference is that GetBalance sums up all unspent TxOuts paying to the
// wallet, while this sums up both spent and unspent TxOuts paying to the
// wallet, and then subtracts the values of TxIns spending from the wallet. This
// also has fewer restrictions on which unconfirmed transactions are considered
// trusted.
CAmount CWallet::GetLegacyBalance(const isminefilter& filter, int minDepth, const boost::uuids::uuid* accountUUID) const
{
    LOCK2(cs_main, cs_wallet);

    CAccount* forAccount = NULL;
    if (accountUUID && mapAccounts.find(*accountUUID) != mapAccounts.end())
        forAccount = mapAccounts.find(*accountUUID)->second;

    CAmount balance = 0;


    //checkme: (GULDEN) - Is fee handled right?
    for (const auto& entry : mapWallet) {
        const CWalletTx& wtx = entry.second;
        const int depth = wtx.GetDepthInMainChain();
        if (depth < 0 || !CheckFinalTx(*wtx.tx) || wtx.GetBlocksToMaturity() > 0 || wtx.isAbandoned()) {
            continue;
        }

        if (depth >= minDepth)
        {
            balance += wtx.GetCredit(filter, forAccount);
            balance -= wtx.GetDebit(filter, forAccount);
        }
    }

    return balance;
}

CAmount CWallet::GetAvailableBalance(CAccount* forAccount, const CCoinControl* coinControl) const
{
    LOCK2(cs_main, cs_wallet);

    CAmount balance = 0;
    std::vector<COutput> vCoins;
    AvailableCoins(forAccount, vCoins, true, coinControl);
    for (const COutput& out : vCoins) {
        if (out.fSpendable) {
            balance += out.tx->tx->vout[out.i].nValue;
        }
    }
    return balance;
}


std::map<CTxDestination, CAmount> CWallet::GetAddressBalances()
{
    std::map<CTxDestination, CAmount> balances;

    {
        LOCK(cs_wallet);
        for (const auto& walletEntry : mapWallet)
        {
            const CWalletTx *pcoin = &walletEntry.second;

            if (!pcoin->IsTrusted())
                continue;

            if (pcoin->IsCoinBase() && pcoin->GetBlocksToMaturity() > 0)
                continue;

            int nDepth = pcoin->GetDepthInMainChain();
            if (nDepth < (pcoin->IsFromMe(ISMINE_ALL) ? 0 : 1))
                continue;

            for (unsigned int i = 0; i < pcoin->tx->vout.size(); i++)
            {
                CTxDestination addr;
                if (!IsMine(pcoin->tx->vout[i]))
                    continue;
                if(!ExtractDestination(pcoin->tx->vout[i], addr))
                    continue;

                CAmount n = IsSpent(walletEntry.first, i) ? 0 : pcoin->tx->vout[i].nValue;

                if (!balances.count(addr))
                    balances[addr] = 0;
                balances[addr] += n;
            }
        }
    }

    return balances;
}
