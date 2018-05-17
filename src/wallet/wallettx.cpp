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

int64_t CWalletTx::GetTxTime() const
{
    int64_t n = nTimeSmart;
    return n ? n : nTimeReceived;
}

int CWalletTx::GetRequestCount() const
{
    // Returns -1 if it wasn't being tracked
    int nRequests = -1;
    {
        LOCK(pwallet->cs_wallet);
        if (IsCoinBase())
        {
            // Generated block
            if (!hashUnset())
            {
                std::map<uint256, int>::const_iterator mi = pwallet->mapRequestCount.find(hashBlock);
                if (mi != pwallet->mapRequestCount.end())
                    nRequests = (*mi).second;
            }
        }
        else
        {
            // Did anyone request this transaction?
            std::map<uint256, int>::const_iterator mi = pwallet->mapRequestCount.find(GetHash());
            if (mi != pwallet->mapRequestCount.end())
            {
                nRequests = (*mi).second;

                // How about the block it's in?
                if (nRequests == 0 && !hashUnset())
                {
                    std::map<uint256, int>::const_iterator _mi = pwallet->mapRequestCount.find(hashBlock);
                    if (_mi != pwallet->mapRequestCount.end())
                        nRequests = (*_mi).second;
                    else
                        nRequests = 1; // If it's in someone else's block it must have got out
                }
            }
        }
    }
    return nRequests;
}

void CWalletTx::GetAmounts(std::list<COutputEntry>& listReceived,
                           std::list<COutputEntry>& listSent, CAmount& nFee, const isminefilter& filter, CKeyStore* from) const
{
    if (!from)
        from = pwallet->activeAccount;

    nFee = 0;
    listReceived.clear();
    listSent.clear();

    // Compute fee:
    CAmount nDebit = GetDebit(filter);
    if (nDebit > 0) // debit>0 means we signed/sent this transaction
    {
        CAmount nValueOut = tx->GetValueOut();
        nFee = nDebit - nValueOut;
    }

    // Sent.
    for (unsigned int i = 0; i < tx->vin.size(); ++i)
    {
        const CTxIn& txin = tx->vin[i];

        std::map<uint256, CWalletTx>::const_iterator mi = pwallet->mapWallet.find(txin.prevout.hash);
        if (mi != pwallet->mapWallet.end())
        {
            const CWalletTx& prev = (*mi).second;
            if (txin.prevout.n < prev.tx->vout.size())
            {
                const auto& prevOut =  prev.tx->vout[txin.prevout.n];

                isminetype fIsMine = IsMine(*from, prevOut);

                if ((fIsMine & filter))
                {
                    CTxDestination address;
                    if (!ExtractDestination(prevOut, address) && !prevOut.IsUnspendable())
                    {
                        LogPrintf("CWalletTx::GetAmounts: Unknown transaction type found, txid %s\n",
                                this->GetHash().ToString());
                        address = CNoDestination();
                    }

                    //fixme: (Post-2.1) - There should be a seperate CInputEntry class/array here or something.
                    COutputEntry output = {address, prevOut.nValue, (int)i};

                    // We are debited by the transaction, add the output as a "sent" entry
                    listSent.push_back(output);
                }
            }
        }
    }

    // received.
    for (unsigned int i = 0; i < tx->vout.size(); ++i)
    {
        const CTxOut& txout = tx->vout[i];
        isminetype fIsMine = IsMine(*from, txout);
        if (!(fIsMine & filter))
            continue;

        // Get the destination address
        CTxDestination address;
        if (!ExtractDestination(txout, address) && !txout.IsUnspendable())
        {
            LogPrintf("CWalletTx::GetAmounts: Unknown transaction type found, txid %s\n",
                     this->GetHash().ToString());
            address = CNoDestination();
        }

        COutputEntry output = {address, txout.nValue, (int)i};

        // We are receiving the output, add it as a "received" entry
        listReceived.push_back(output);
    }

}
