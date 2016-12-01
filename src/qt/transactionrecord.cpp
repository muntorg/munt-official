// Copyright (c) 2011-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "transactionrecord.h"

#include "base58.h"
#include "consensus/consensus.h"
#include "main.h"
#include "timedata.h"
#include <Gulden/auto_checkpoints.h>
#include "wallet/wallet.h"
#include "account.h"
#include "script/ismine.h"

#include <stdint.h>

#include <boost/foreach.hpp>

/* Return positive answer if transaction should be shown in list.
 */
bool TransactionRecord::showTransaction(const CWalletTx &wtx)
{
    if (wtx.IsCoinBase())
    {
        // Ensures we show generated coins / mined transactions at depth 1
        if (!wtx.IsInMainChain())
        {
            return false;
        }
    }
    return true;
}

/*
 * Decompose CWallet transaction to model transaction records.
 */
QList<TransactionRecord> TransactionRecord::decomposeTransaction(const CWallet *wallet, const CWalletTx &wtx)
{
    QList<TransactionRecord> parts;
    int64_t nTime = wtx.GetTxTime();
    CAmount nCredit = wtx.GetCredit(ISMINE_ALL);
    CAmount nDebit = wtx.GetDebit(ISMINE_ALL);
    CAmount nNet = nCredit - nDebit;
    uint256 hash = wtx.GetHash();
    std::map<std::string, std::string> mapValue = wtx.mapValue;

    LOCK(wallet->cs_wallet);
    
    if (nNet > 0 || wtx.IsCoinBase())
    {
        //
        // Credit
        //
        for( const auto& accountPair : wallet->mapAccounts )
        {
            CAccount* account = accountPair.second;
            BOOST_FOREACH(const CTxOut& txout, wtx.vout)
            {
                
                isminetype mine = IsMine(*account, txout);
                if(mine)
                {
                    TransactionRecord sub(hash, nTime);
                    sub.actionAccountUUID = sub.receiveAccountUUID = account->getUUID();
                    sub.actionAccountParentUUID = sub.receiveAccountParentUUID = account->getParentUUID();
                    CTxDestination address;
                    sub.idx = parts.size(); // sequence number
                    sub.credit = txout.nValue;
                    // Payment could partially be coming from us - deduct that from amount.
                    std::string addressIn;
                    for (const CTxIn& txin : wtx.vin)
                    {
                        //fixme: rather just have a CAccount::GetDebit function?
                        isminetype mine = wallet->IsMine(*account, txin);
                        if (mine == ISMINE_SPENDABLE)
                        {
                            sub.credit -= wallet->GetDebit(txin, ISMINE_SPENDABLE);
                            //fixme: Add a 'payment to self' sub here as well.
                        }
                        
                        const CWalletTx* parent = wallet->GetWalletTx(txin.prevout.hash);   
                        if (parent != NULL)
                        {
                            CTxDestination senderAddress;
                            if (ExtractDestination(parent->vout[txin.prevout.n].scriptPubKey, senderAddress))
                            {
                                if (!addressIn.empty())
                                    addressIn += ", ";
                                addressIn += CBitcoinAddress(senderAddress).ToString();
                            }
                        }
                    }
                    if (!addressIn.empty())
                    {
                        sub.address = addressIn;
                    }
                    sub.involvesWatchAddress = mine & ISMINE_WATCH_ONLY;
                    if (ExtractDestination(txout.scriptPubKey, address) && IsMine(*account, address))
                    {
                        // Received by Bitcoin Address
                        sub.type = TransactionRecord::RecvWithAddress;
                        //sub.address = CBitcoinAddress(address).ToString();
                    }
                    else
                    {
                        // Received by IP connection (deprecated features), or a multisignature or other non-simple transaction
                        sub.type = TransactionRecord::RecvFromOther;
                        //sub.address = mapValue["from"];
                    }
                    if (wtx.IsCoinBase())
                    {
                        // Generated
                        sub.type = TransactionRecord::Generated;
                    }

                    parts.append(sub);
                }
            }
        }
    }
    else
    {
        for( const auto& accountPair : wallet->mapAccounts )
        {
            CAccount* account = accountPair.second;
            bool involvesWatchAddress = false;
            isminetype fAllFromMe = ISMINE_SPENDABLE;
            BOOST_FOREACH(const CTxIn& txin, wtx.vin)
            {
                isminetype mine = wallet->IsMine(*account, txin);
                if(mine & ISMINE_WATCH_ONLY) involvesWatchAddress = true;
                if(fAllFromMe > mine) fAllFromMe = mine;
            }

            isminetype fAllToMe = ISMINE_SPENDABLE;
            std::vector<CTxOut> vNotToMe;
            BOOST_FOREACH(const CTxOut& txout, wtx.vout)
            {
                isminetype mine = IsMine(*account, txout);
                if(mine & ISMINE_WATCH_ONLY) involvesWatchAddress = true;
                if (mine != ISMINE_SPENDABLE)
                    vNotToMe.push_back(txout);
                if(fAllToMe > mine) fAllToMe = mine;
            }

            if (fAllFromMe && fAllToMe)
            {
                // Payment to self
                CAmount nChange = wtx.GetChange();

                TransactionRecord sub(TransactionRecord(hash, nTime, TransactionRecord::SendToSelf, "",
                                -(nDebit - nChange), nCredit - nChange));

                sub.actionAccountUUID = sub.receiveAccountUUID = sub.fromAccountUUID = account->getUUID();
                sub.actionAccountParentUUID = sub.receiveAccountParentUUID = sub.fromAccountParentUUID = account->getParentUUID();
                
                parts.append(sub);
                parts.last().involvesWatchAddress = involvesWatchAddress;   // maybe pass to TransactionRecord as constructor argument
            }
            else if (!fAllFromMe && !fAllToMe && vNotToMe.size() == 1 && wallet->IsMine(vNotToMe[0]))
            {
                //Handle the 'receieve' part of an internal send between two accounts
                TransactionRecord sub(hash, nTime);
                
                //We don't bother filling in the sender details here for now
                //The sender will get it's own transaction record and then we try 'automatically' match the two up and 'swap details' in a loop at the bottom of this function.
                
                //Fill in all the details for the receive part of the transaction.
                sub.actionAccountUUID = sub.receiveAccountUUID = account->getUUID();
                sub.actionAccountParentUUID = sub.receiveAccountParentUUID = account->getParentUUID();
                sub.idx = parts.size();
                sub.involvesWatchAddress = involvesWatchAddress;
                sub.credit = sub.debit = 0;
                for (unsigned int nOut = 0; nOut < wtx.vout.size(); nOut++)
                {
                    const CTxOut& txout = wtx.vout[nOut];
                    if(!IsMine(*account, txout))
                    {
                        continue;
                    }

                    CAmount nValue = txout.nValue;
                    sub.credit += nValue;
                }
                parts.append(sub);
            }
            else if (fAllFromMe)
            {
                //
                // Debit
                //
                CAmount nTxFee = nDebit - wtx.GetValueOut();

                for (unsigned int nOut = 0; nOut < wtx.vout.size(); nOut++)
                {
                    const CTxOut& txout = wtx.vout[nOut];
                    TransactionRecord sub(hash, nTime);
                    sub.actionAccountUUID = sub.fromAccountUUID = account->getUUID();
                    sub.actionAccountParentUUID = sub.fromAccountParentUUID = account->getParentUUID();
                    sub.idx = parts.size();
                    sub.involvesWatchAddress = involvesWatchAddress;

                    if(IsMine(*account, txout))
                    {
                        // Ignore parts sent to self, as this is usually the change
                        // from a transaction sent back to our own address.
                        //fixme: GULDEN HIGH - Are there cases when this isn't true?!?!?!?
                        continue;
                    }

                    CTxDestination address;
                    if (ExtractDestination(txout.scriptPubKey, address))
                    {
                        // Sent to Bitcoin Address
                        sub.type = TransactionRecord::SendToAddress;
                        sub.address = CBitcoinAddress(address).ToString();
                    }
                    else
                    {
                        // Sent to IP, or other non-address transaction like OP_EVAL
                        sub.type = TransactionRecord::SendToOther;
                        sub.address = mapValue["to"];
                    }

                    CAmount nValue = txout.nValue;
                    /* Add fee to first output */
                    if (nTxFee > 0)
                    {
                        nValue += nTxFee;
                        sub.fee = nTxFee;
                        nTxFee = 0;
                    }
                    sub.debit = nValue;

                    parts.append(sub);
                }
            }
            else
            {
                //fixme: GULDEN HIGH - This can be displayed better...
                CAmount nNetMixed = 0;
                for (const CTxOut& txout : wtx.vout)
                {
                    isminetype mine = IsMine(*account, txout);
                    if (mine == ISMINE_SPENDABLE)
                    {
                        nNetMixed += txout.nValue;
                    }
                }
                for (const CTxIn& txin : wtx.vin)
                {
                    //fixme: rather just have a CAccount::GetDebit function?
                    isminetype mine = wallet->IsMine(*account, txin);
                    if (mine == ISMINE_SPENDABLE)
                    {
                        nNetMixed -= wallet->GetDebit(txin, ISMINE_SPENDABLE);
                        //fixme: Add a 'payment to self' sub here as well.
                    }
                }
                if (nNetMixed != 0)
                {
                    //
                    // Mixed debit transaction, can't break down payees
                    //
                    TransactionRecord sub(hash, nTime, TransactionRecord::Other, "", 0, 0);
                    if (nNetMixed > 0)
                    {
                        sub.credit = nNetMixed;
                        sub.actionAccountUUID = sub.receiveAccountUUID = account->getUUID();
                        sub.actionAccountParentUUID = sub.receiveAccountParentUUID = account->getParentUUID();
                    }
                    else
                    {
                        sub.debit = -nNetMixed;
                        sub.actionAccountUUID = sub.fromAccountUUID = account->getUUID();
                        sub.actionAccountParentUUID = sub.fromAccountParentUUID = account->getParentUUID();
                    }
                    
                    parts.append(sub);
                    parts.last().involvesWatchAddress = involvesWatchAddress;
                }
            }
        }
    }
    
    // A bit hairy/gross, but try to tie up sender/receiver for internal transfer between accounts.
    //fixme: Can this try harder?
    for (TransactionRecord& record : parts)
    {
        if (record.fromAccountUUID == "")
        {
            for (TransactionRecord& comp : parts)
            {
                if(&record != &comp)
                {
                    if(comp.fromAccountUUID != "" && comp.receiveAccountUUID == "")
                    {
                        if(record.time == comp.time)
                        {
                            if(record.credit == comp.debit - comp.fee)
                            {
                                record.type = comp.type = TransactionRecord::InternalTransfer;
                                record.fromAccountUUID = comp.fromAccountUUID;
                                record.fromAccountParentUUID = comp.fromAccountParentUUID;
                                comp.receiveAccountUUID = record.receiveAccountUUID;
                                comp.receiveAccountParentUUID = record.receiveAccountParentUUID;
                            }
                        }
                    }
                }
            }
        }
        else if(record.receiveAccountUUID == "")
        {
            for (TransactionRecord& comp : parts)
            {
                if(&record != &comp)
                {
                    if(comp.receiveAccountUUID != "" && comp.fromAccountUUID == "")
                    {
                        if(record.time == comp.time)
                        {
                            if(record.debit - record.fee == comp.credit)
                            {
                                record.type = comp.type = TransactionRecord::InternalTransfer;
                                record.receiveAccountUUID = comp.receiveAccountUUID;
                                record.receiveAccountParentUUID = comp.receiveAccountParentUUID;
                                comp.fromAccountUUID = record.fromAccountUUID;
                                comp.fromAccountParentUUID = record.fromAccountParentUUID;
                            }
                        }
                    }
                }
            }
        }
    }

    return parts;
}

void TransactionRecord::updateStatus(const CWalletTx &wtx)
{
    AssertLockHeld(cs_main);
    // Determine transaction status

    // Find the block the tx is in
    CBlockIndex* pindex = NULL;
    BlockMap::iterator mi = mapBlockIndex.find(wtx.hashBlock);
    if (mi != mapBlockIndex.end())
        pindex = (*mi).second;

    // Sort order, unrecorded transactions sort to the top
    status.sortKey = strprintf("%010d-%01d-%010u-%03d",
        (pindex ? pindex->nHeight : std::numeric_limits<int>::max()),
        (wtx.IsCoinBase() ? 1 : 0),
        wtx.nTimeReceived,
        idx);
    status.countsForBalance = wtx.IsTrusted() && !(wtx.GetBlocksToMaturity() > 0);
    status.depth = wtx.GetDepthInMainChain();
    status.cur_num_blocks = chainActive.Height();

    if (!CheckFinalTx(wtx))
    {
        if (wtx.nLockTime < LOCKTIME_THRESHOLD)
        {
            status.status = TransactionStatus::OpenUntilBlock;
            status.open_for = wtx.nLockTime - chainActive.Height();
        }
        else
        {
            status.status = TransactionStatus::OpenUntilDate;
            status.open_for = wtx.nLockTime;
        }
    }
    // For generated transactions, determine maturity
    else if(type == TransactionRecord::Generated)
    {
        if (wtx.GetBlocksToMaturity() > 0)
        {
            status.status = TransactionStatus::Immature;

            if (wtx.IsInMainChain())
            {
                status.matures_in = wtx.GetBlocksToMaturity();

                // Check if the block was requested by anyone
                if (GetAdjustedTime() - wtx.nTimeReceived > 2 * 60 && wtx.GetRequestCount() == 0)
                    status.status = TransactionStatus::MaturesWarning;
            }
            else
            {
                status.status = TransactionStatus::NotAccepted;
            }
        }
        else
        {
            status.status = TransactionStatus::Confirmed;
        }
    }
    else
    {
        if (status.depth < 0)
        {
            status.status = TransactionStatus::Conflicted;
        }
        else if (GetAdjustedTime() - wtx.nTimeReceived > 2 * 60 && wtx.GetRequestCount() == 0)
        {
            status.status = TransactionStatus::Offline;
        }
        else if (status.depth == 0)
        {
            status.status = TransactionStatus::Unconfirmed;
            if (wtx.isAbandoned())
                status.status = TransactionStatus::Abandoned;
        }
        else if (status.depth < RecommendedNumConfirmations || !Checkpoints::IsSecuredBySyncCheckpoint(wtx.hashBlock))
        {
            status.status = TransactionStatus::Confirming;
        }
        else
        {
            status.status = TransactionStatus::Confirmed;
        }
    }

}

bool TransactionRecord::statusUpdateNeeded()
{
    AssertLockHeld(cs_main);
    return status.cur_num_blocks != chainActive.Height();
}

QString TransactionRecord::getTxID() const
{
    return QString::fromStdString(hash.ToString());
}

int TransactionRecord::getOutputIndex() const
{
    return idx;
}
