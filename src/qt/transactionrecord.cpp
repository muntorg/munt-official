// Copyright (c) 2011-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// File contains modifications by: The Gulden developers
// All modifications:
// Copyright (c) 2016-2020 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#include "transactionrecord.h"

#include "base58.h"
#include "consensus/consensus.h"
#include "consensus/validation.h"
#include "validation/validation.h"
#include "timedata.h"
#include "wallet/wallet.h"
#include "wallet/account.h"
#include "script/ismine.h"
#include "script/script.h"
#include <witnessutil.h>

#include <stdint.h>

#include <consensus/tx_verify.h>


/* Return positive answer if transaction should be shown in list.
 */
bool TransactionRecord::showTransaction(const CWalletTx &wtx)
{
    AssertLockHeld(pactiveWallet->cs_wallet);

    //fixme: (PHASE5) - We can potentially remove this; depending on how we handle wallet upgrades. (If we delete all old instances of this in a wallet upgrade after phase4 is locked in then we no longer need to do this after that)
    // Hide orphaned phase 3 witness earnings when they were orphaned by a subsequent PoW block that contain the same earnings.
    if (wtx.IsCoinBase() && wtx.mapValue.count("replaced_by_txid") > 0)
    {
        return false;
    }
    return true;
}

bool AddAddressForOutPoint(const CWallet *wallet, const COutPoint& outpoint, std::string address)
{
    CTransactionRef tx;
    if (outpoint.isHash)
    {
        const CWalletTx* wtx = wallet->GetWalletTx(outpoint.getTransactionHash());
        if (wtx)
        {
            tx = wtx->tx;
        }
    }
    else
    {
        CBlock block;
        LOCK(cs_main);
        if (chainActive.Tip() && (uint64_t)outpoint.getTransactionBlockNumber() < (uint64_t)chainActive.Tip()->nHeight)
        {
            if (ReadBlockFromDisk(block, chainActive[outpoint.getTransactionBlockNumber()], Params()) && block.vtx.size() > outpoint.getTransactionIndex())
            {
                tx = block.vtx[outpoint.getTransactionIndex()];
            }
        }
    }

    CTxDestination destination;

    if (tx && tx->vout.size() > outpoint.n && ExtractDestination(tx->vout[outpoint.n], destination))
    {
        if (!address.empty())
            address += ", ";
        address += CNativeAddress(destination).ToString();
        return true;
    }

    return false;
}

bool witnessOutputsToReceiveRecord(const CWallet *wallet, const CWalletTx &wtx, const CWitnessTxBundle& witnessBundle, TransactionRecord& subReceive, TransactionRecord::Type recType, int index)
{
    // determine account and fill fields shared by all outputs
    CAccount* account = nullptr;
    for (const auto& [txOut, witnessDetails, witnessOutPoint] : witnessBundle.outputs)
    {
        (unused) witnessDetails;
        (unused) witnessOutPoint;
        for( const auto& accountPair : wallet->mapAccounts )
        {
            if (!accountPair.second->IsPoW2Witness())
                continue;

            isminetype mine = IsMine(*accountPair.second, txOut);
            if (mine)
            {
                account = accountPair.second;
                CTxDestination getAddress;
                if (ExtractDestination(txOut, getAddress))
                {
                    subReceive.address = CNativeAddress(getAddress).ToString();
                }
                subReceive.type = recType;
                subReceive.involvesWatchAddress = mine & ISMINE_WATCH_ONLY;
                subReceive.actionAccountUUID = subReceive.receiveAccountUUID = account->getUUID();
                subReceive.actionAccountParentUUID = subReceive.receiveAccountParentUUID = account->getParentUUID();
                subReceive.debit = 0;
                subReceive.idx = index; // sequence number
                break;
            }
        }
        if (account)
            break;
    }

    // sum witness amount
    if (account)
    {
        subReceive.credit = std::accumulate(witnessBundle.outputs.begin(), witnessBundle.outputs.end(), CAmount(0), [](const CAmount acc, const auto& it){
                const CTxOut& txOut = std::get<0>(it);
                return acc + txOut.nValue;
            });
        subReceive.debit = 0;
        return true;
    }

    return false;
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

    // New method to decompose witness generation tx
    // The existing code below produces odd/unexpected results for some witness generation tx.
    // Instead of fixing up the code below (which has more issues) simpler new code was introduced here to handle it.
    if (wtx.IsPoW2WitnessCoinBase())
    {
        for( const auto& accountPair : wallet->mapAccounts )
        {
            CAccount* account = accountPair.second;
            CAmount received = 0;
            std::string outAddresses;
            bool involvesWatchAddress = false;
            for(const CTxOut& txout : wtx.tx->vout)
            {
                isminetype mine = IsMine(*account, txout);
                if (mine)
                {
                    received += txout.nValue;
                    if (mine & ISMINE_WATCH_ONLY)
                        involvesWatchAddress = true;
                    if (IsPow2WitnessOutput(txout))
                    {
                        // Output is witness and ours => witness input is also ours, subtract input value as we only want to show received rewards
                        received -= nDebit;
                        // Prevent old phase3 rewards from displaying incorrectly 
                        if (nDebit == 0 && txout.nValue > 4000 * COIN)
                        {
                            received -= txout.nValue;
                        }
                    }
                    CTxDestination address;
                    if (ExtractDestination(txout, address))
                    {
                        if (!outAddresses.empty())
                            outAddresses += " ";
                        outAddresses += CNativeAddress(address).ToString();
                    }
                }
            }
            
            if (received > 0) {
                TransactionRecord sub(hash, nTime, TransactionRecord::GeneratedWitness, outAddresses, 0, received);
                sub.actionAccountUUID = sub.receiveAccountUUID = account->getUUID();
                sub.actionAccountParentUUID = sub.receiveAccountParentUUID = account->getParentUUID();
                sub.idx = parts.size();
                sub.involvesWatchAddress = involvesWatchAddress;
                parts.append(sub);
            }
        }
        return parts;
    }

    // Get witness bundles from tx as computed by validation, or build and persist them if not available.
    CWitnessBundlesRef pWitnessBundles = std::make_shared<CWitnessBundles>();
    if (wtx.witnessBundles)
    {
        pWitnessBundles = wtx.witnessBundles;
    }
    else
    {
        bool haveBundles = false;

        if (wtx.tx->witnessBundles)
        {
            pWitnessBundles = wtx.tx->witnessBundles;
            haveBundles = true;
        }
        else
        {
            CValidationState state;
            int nWalletTxBlockHeight = chainActive.Tip()?chainActive.Tip()->nHeight:0;
            const auto& findIter = mapBlockIndex.find(wtx.hashBlock);
            if (findIter != mapBlockIndex.end())
                nWalletTxBlockHeight = findIter->second->nHeight;

            CWitnessBundles bundles;
            haveBundles = BuildWitnessBundles(*wtx.tx, state, nWalletTxBlockHeight, wtx.nIndex,
                [&](const COutPoint& outpoint, CTxOut& txOut, uint64_t& txHeight, uint64_t& txIndex, uint64_t& txIndexOutput) {
                    CTransactionRef txRef;
                    int nInputHeight = -1;
                    LOCK(cs_main);
                    if (outpoint.isHash) {
                        const CWalletTx* txPrev = wallet->GetWalletTx(outpoint.getTransactionHash());
                        if (!txPrev || txPrev->tx->vout.size() == 0)
                            return false;

                        const auto& findIter = mapBlockIndex.find(txPrev->hashBlock);
                        if (findIter != mapBlockIndex.end()) {
                            nInputHeight = findIter->second->nHeight;
                        }

                        txRef = txPrev->tx;
                    }
                    else
                    {
                        nInputHeight = outpoint.getTransactionBlockNumber();
                        CBlock block;
                        if (chainActive.Height() >= nInputHeight && ReadBlockFromDisk(block, chainActive[nInputHeight], Params())) {
                            txRef = block.vtx[outpoint.getTransactionIndex()];
                        }
                        else
                            return false;
                    }
                    txOut = txRef->vout[outpoint.n];
                    txHeight = nInputHeight;
                    return true;
                },
                bundles);
            pWitnessBundles = std::make_shared<CWitnessBundles>(bundles);
        }

        if (haveBundles)
        {
            CWalletTx& w = REF(wtx);
            w.witnessBundles = pWitnessBundles;
            CWalletDB walletdb(*wallet->dbw);
            walletdb.WriteTx(w);
        }
    }

    const CWitnessBundles& witnessBundles = *pWitnessBundles;

    std::vector<CTxOut> outputs = wtx.tx->vout;
    std::vector<CTxIn> inputs = wtx.tx->vin;
    if (witnessBundles.size() > 0)
    {
        for (const CWitnessTxBundle& witnessBundle : witnessBundles)
        {
            switch (witnessBundle.bundleType)
            {
                case CWitnessTxBundle::WitnessTxType::CreationType:
                {
                    TransactionRecord subSend(hash, nTime);
                    TransactionRecord subReceive(hash, nTime);
                    subReceive.idx = -1;

                    if (witnessOutputsToReceiveRecord(wallet, wtx, witnessBundle, subReceive, TransactionRecord::WitnessFundRecv, parts.size()))
                        parts.append(subReceive);

                    if (subReceive.idx != -1)
                    {
                        for( const auto& [accountUUID, account] : wallet->mapAccounts )
                        {
                            (unused) accountUUID;
                            isminetype mine = static_cast<const CExtWallet*>(wallet)->IsMine(*account, inputs[0]);
                            if (mine)
                            {
                                AddAddressForOutPoint(wallet, inputs[0].GetPrevOut(), subSend.address);
                                subSend.type = TransactionRecord::WitnessFundSend;
                                subSend.involvesWatchAddress = mine & ISMINE_WATCH_ONLY;
                                subSend.actionAccountUUID = subSend.fromAccountUUID = account->getUUID();
                                subSend.actionAccountParentUUID = subSend.fromAccountParentUUID = account->getParentUUID();
                                subSend.receiveAccountUUID = subSend.receiveAccountParentUUID = subReceive.receiveAccountUUID;
                                parts[subReceive.idx].fromAccountUUID = parts[subReceive.idx].fromAccountParentUUID = subSend.fromAccountUUID;
                                subSend.credit = 0;
                                subSend.debit = subReceive.credit;
                                subSend.idx = parts.size(); // sequence number
                                parts.append(subSend);
                                break;
                            }
                        }
                    }
                }
                break;
                case CWitnessTxBundle::WitnessTxType::WitnessType:
                {
                    //We never reach here because this is special cased at top of function
                }
                break;
                case CWitnessTxBundle::WitnessTxType::SpendType:
                {
                    TransactionRecord subSend(hash, nTime);
                    TransactionRecord subReceive(hash, nTime);
                    subReceive.idx = -1;

                    // Locate the input for the spend bundle
                    for (const auto& output : outputs)
                    {
                        for( const auto& [accountUUID, account] : wallet->mapAccounts )
                        {
                            //NB! This will 'break' if someone pays from one witness account to another
                            //Its an acceptable tradeoff for now, because UI forbids this anyway.
                            if (account->IsPoW2Witness())
                                continue;

                            (unused) accountUUID;
                            isminetype mine = IsMine(*account, output);
                            if (mine)
                            {
                                CTxDestination getAddress;
                                if (ExtractDestination(output, getAddress))
                                {
                                    subReceive.address = CNativeAddress(getAddress).ToString();
                                }
                                subReceive.type = TransactionRecord::WitnessEmptyRecv;
                                subReceive.involvesWatchAddress = mine & ISMINE_WATCH_ONLY;
                                subReceive.actionAccountUUID = subReceive.receiveAccountUUID = account->getUUID();
                                subReceive.actionAccountParentUUID = subReceive.receiveAccountParentUUID = account->getParentUUID();
                                subReceive.credit = output.nValue;
                                subReceive.debit = 0;
                                subReceive.idx = parts.size(); // sequence number
                                parts.append(subReceive);

                                // Remove the witness related outputs so that the remaining decomposition code can ignore it.
                                outputs.erase(std::remove_if(outputs.begin(), outputs.end(),[&](CTxOut x){return x == output;}));

                                break;
                            }
                        }
                        if (subReceive.idx != -1)
                            break;
                    }
                    // Locate the output for the spend bundle
                    if (subReceive.idx != -1)
                    {
                        CAccount* account = nullptr;
                        for (auto const& [txOut, txOutWitness, txOutPoint]: witnessBundle.inputs)
                        {
                            (unused) txOutPoint;
                            (unused) txOutWitness;
                            for(const auto& [current_uuid, current_account] : wallet->mapAccounts)
                            {
                                (unused) current_uuid;
                                isminetype mine = ::IsMine(*current_account, txOut);
                                if (mine)
                                {
                                    account = current_account;
                                    CTxDestination destination;
                                    ExtractDestination(txOut, destination);
                                    subSend.address = CNativeAddress(destination).ToString();
                                    subSend.type = TransactionRecord::WitnessEmptySend;
                                    subSend.involvesWatchAddress = mine & ISMINE_WATCH_ONLY;
                                    subSend.actionAccountUUID = subSend.fromAccountUUID = account->getUUID();
                                    subSend.actionAccountParentUUID = subSend.fromAccountParentUUID = account->getParentUUID();
                                    subSend.receiveAccountUUID = subSend.receiveAccountParentUUID = subReceive.receiveAccountUUID;
                                    parts[subReceive.idx].fromAccountUUID = parts[subReceive.idx].fromAccountParentUUID = subSend.fromAccountUUID;
                                    subSend.credit = 0;
                                    subSend.idx = parts.size(); // sequence number
                                    subSend.debit = subReceive.credit;
                                    parts.append(subSend);
                                    break;
                                }
                            }
                        }
                    }
                }
                break;
                case CWitnessTxBundle::WitnessTxType::RenewType:
                {
                    TransactionRecord subReceive(hash, nTime);
                    if (witnessOutputsToReceiveRecord(wallet, wtx, witnessBundle, subReceive, TransactionRecord::WitnessRenew, parts.size()))
                    {
                        subReceive.debit = subReceive.credit;
                        parts.append(subReceive);
                    }

                    //fixme: (FUT) - Add fee transaction to sender account (we should relook an optional "display fee" button though.
                    //It isn't 100% clear if/when we should show fees and when not.
                }
                break;
                case CWitnessTxBundle::WitnessTxType::IncreaseType:
                {
                    TransactionRecord subSend(hash, nTime);
                    TransactionRecord subReceive(hash, nTime);
                    subReceive.idx = -1;

                    if (witnessOutputsToReceiveRecord(wallet, wtx, witnessBundle, subReceive, TransactionRecord::WitnessIncreaseRecv, parts.size()))
                    {
                        subReceive.debit = std::accumulate(witnessBundle.inputs.begin(), witnessBundle.inputs.end(), CAmount(0), [](const CAmount acc, const auto& it){
                            const CTxOut& txOut = std::get<0>(it);
                            return acc + txOut.nValue;
                        });
                        parts.append(subReceive);
                    }

                    if (subReceive.idx != -1)
                    {
                        for (const auto& walIt: wallet->mapAccounts)
                        {
                            const CAccount* account = walIt.second;
                            if (account->IsPoW2Witness() || account->getUUID() == subReceive.actionAccountUUID)
                                continue;
                            isminetype mine = ISMINE_NO;
                            const CTxIn* myInput = nullptr;
                            for (const CTxIn& input: inputs) {
                                isminetype isthismine = static_cast<const CExtWallet*>(wallet)->IsMine(*account, input);
                                if (isthismine) {
                                    mine = isthismine;
                                    myInput = &input;
                                    break;
                                }
                            }
                            if (myInput)
                            {
                                AddAddressForOutPoint(wallet, myInput->GetPrevOut(), subSend.address);
                                subSend.type = TransactionRecord::WitnessIncreaseSend;
                                subSend.involvesWatchAddress = mine & ISMINE_WATCH_ONLY;
                                subSend.actionAccountUUID = subSend.fromAccountUUID = account->getUUID();
                                subSend.actionAccountParentUUID = subSend.fromAccountParentUUID = account->getParentUUID();
                                subSend.receiveAccountUUID = subSend.receiveAccountParentUUID = subReceive.receiveAccountUUID;
                                parts[subReceive.idx].fromAccountUUID = parts[subReceive.idx].fromAccountParentUUID = subSend.fromAccountUUID;
                                CAmount nTxFee = nDebit - wtx.tx->GetValueOut();
                                subSend.credit = 0;
                                subSend.debit = parts[subReceive.idx].credit - parts[subReceive.idx].debit + nTxFee;
                                subSend.idx = parts.size(); // sequence number
                                parts.append(subSend);
                                break;
                            }
                        }
                    }
                }
                break;
                case CWitnessTxBundle::WitnessTxType::RearrangeType:
                {
                    TransactionRecord subReceive(hash, nTime);
                    if (witnessOutputsToReceiveRecord(wallet, wtx, witnessBundle, subReceive, TransactionRecord::WitnessRearrangeRecv, parts.size()))
                    {
                        subReceive.debit = subReceive.credit;
                        parts.append(subReceive);
                    }

                    //fixme: (FUT) - Add fee transaction to sender account (we should relook an optional "display fee" button though.
                    //It isn't 100% clear if/when we should show fees and when not.
                }
                break;
                case CWitnessTxBundle::WitnessTxType::ChangeWitnessKeyType:
                {
                    TransactionRecord subReceive(hash, nTime);
                    if (witnessOutputsToReceiveRecord(wallet, wtx, witnessBundle, subReceive, TransactionRecord::WitnessChangeKeyRecv, parts.size()))
                    {
                        subReceive.debit = subReceive.credit;
                        parts.append(subReceive);
                    }

                    //fixme: (FUT) - Add fee transaction to sender account (we should relook an optional "display fee" button though.
                    //It isn't 100% clear if/when we should show fees and when not.
                }
                break;
            }
        }
        //fixme: (FUT) - we should try deal with "complex" (mixed) transactions by removing the below return and still trying to match remaining inputs/outputs.
        return parts;
    }


    LOCK(wallet->cs_wallet);
    if (nNet > 0 && !wtx.IsCoinBase())
    {
        //
        // Credit
        //
        for( const auto& accountPair : wallet->mapAccounts )
        {
            CAccount* account = accountPair.second;
            for(const CTxOut& txout: outputs)
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
                    for (const CTxIn& txin : inputs)
                    {
                        //fixme: (FUT) rather just have a CAccount::GetDebit function?
                        isminetype txinMine = static_cast<const CExtWallet*>(wallet)->IsMine(*account, txin);
                        if (txinMine == ISMINE_SPENDABLE)
                        {
                            sub.credit -= wallet->GetDebit(txin, ISMINE_SPENDABLE);
                            //fixme: (FUT) Add a 'payment to self' sub here as well.
                        }
                        AddAddressForOutPoint(wallet, txin.GetPrevOut(), sub.address);
                    }
                    sub.involvesWatchAddress = mine & ISMINE_WATCH_ONLY;
                    std:: string addressOut;
                    if (ExtractDestination(txout, address) && IsMine(*account, address))
                    {
                        // Received by Gulden Address
                        sub.type = TransactionRecord::RecvWithAddress;
                        addressOut = CNativeAddress(address).ToString();
                    }
                    else
                    {
                        // Received by IP connection (deprecated features), or a multisignature or other non-simple transaction
                        sub.type = TransactionRecord::RecvFromOther;
                        //sub.address = mapValue["from"];
                    }
                    if (wtx.IsPoW2WitnessCoinBase())
                    {
                        // Prevent old phase3 rewards from displaying incorrectly
                        if (sub.credit > 4000 * COIN)
                        {
                            sub.type = TransactionRecord::WitnessRenew;
                            sub.debit= nDebit-wtx.tx->GetValueOut();;
                            sub.credit=0;
                        }
                        else
                        {
                            sub.type = TransactionRecord::GeneratedWitness;
                            sub.address = addressOut;
                        }
                    }
                    else if (wtx.IsCoinBase())
                    {
                        sub.address = addressOut;
                        // Generated
                        // Prevent old phase3 rewards from displaying incorrectly
                        if (sub.credit > 4000 * COIN)
                        {
                            sub.type = TransactionRecord::WitnessRenew;
                            sub.debit= nDebit-wtx.tx->GetValueOut();;
                            sub.credit=0;
                        }
                        else
                        {
                            if (sub.credit == 20 * COIN && sub.debit == 0)
                            {
                                sub.type = TransactionRecord::GeneratedWitness;
                            }
                            else
                            {
                                sub.type = TransactionRecord::Generated;
                            }
                        }
                    }

                    if (sub.credit == 0 && sub.debit == 0)
                    {
                        continue;
                    }
                    parts.append(sub);
                }
            }
        }
    }
    else
    {
        for( const auto& [accountUUID, account] : wallet->mapAccounts )
        {
            bool involvesWatchAddress = false;
            isminetype fAllFromMe = ISMINE_SPENDABLE;
            std::vector<CTxIn> vNotFromMe;
            for(const CTxIn& txin : inputs)
            {
                isminetype mine = static_cast<const CExtWallet*>(wallet)->IsMine(*account, txin);
                if(mine & ISMINE_WATCH_ONLY)
                {
                    involvesWatchAddress = true;
                }
                if (mine != ISMINE_SPENDABLE)
                {
                    vNotFromMe.push_back(txin);
                }
                if(fAllFromMe > mine)
                {
                    fAllFromMe = mine;
                }
            }

            
            isminetype fAllToMe = ISMINE_SPENDABLE;
            std::vector<CTxOut> vNotToMe;
            for(const CTxOut& txout : outputs)
            {
                isminetype mine = IsMine(*account, txout);
                if(mine & ISMINE_WATCH_ONLY)
                {
                    involvesWatchAddress = true;
                }
                if (mine != ISMINE_SPENDABLE)
                {
                    vNotToMe.push_back(txout);
                }
                if(fAllToMe > mine)
                {
                    fAllToMe = mine;
                }
            }

            if (fAllFromMe && fAllToMe)
            {
                // Payment to self
                CAmount nChange = wtx.GetChange();

                TransactionRecord sub(TransactionRecord(hash, nTime, TransactionRecord::SendToSelf, "",
                                -(nDebit - nChange), nCredit - nChange));

                if (wtx.IsPoW2WitnessCoinBase() && sub.credit == 0 && sub.debit == 0)
                    continue;

                sub.actionAccountUUID = sub.receiveAccountUUID = sub.fromAccountUUID = accountUUID;
                sub.actionAccountParentUUID = sub.receiveAccountParentUUID = sub.fromAccountParentUUID = account->getParentUUID();

                if (sub.credit == 0 && sub.debit == 0)
                {
                    continue;
                }
                parts.append(sub);
                parts.last().involvesWatchAddress = involvesWatchAddress;   // maybe pass to TransactionRecord as constructor argument
            }
            else if (!fAllFromMe && !fAllToMe && vNotToMe.size() == 1 && wallet->IsMine(vNotToMe[0]))
            {
                //Handle the 'receive' part of an internal send between two accounts
                TransactionRecord sub(hash, nTime);

                //We don't bother filling in the sender details here for now
                //The sender will get its own transaction record and then we try 'automatically' match the two up and 'swap details' in a loop at the bottom of this function.

                //Fill in all the details for the receive part of the transaction.
                sub.actionAccountUUID = sub.receiveAccountUUID = accountUUID;
                sub.actionAccountParentUUID = sub.receiveAccountParentUUID = account->getParentUUID();
                sub.idx = parts.size();
                sub.involvesWatchAddress = involvesWatchAddress;
                sub.credit = sub.debit = 0;
                bool anyAreMine = false;
                for (unsigned int nOut = 0; nOut < outputs.size(); nOut++)
                {
                    const CTxOut& txout = outputs[nOut];
                    if(!IsMine(*account, txout))
                    {
                        continue;
                    }
                    anyAreMine = true;

                    CAmount nValue = txout.nValue;
                    sub.credit += nValue;
                }
                if (anyAreMine)
                    parts.append(sub);
            }
            else if (fAllFromMe)
            {
                //
                // Debit
                //
                CAmount nTxFee = nDebit - wtx.tx->GetValueOut();

                for (unsigned int nOut = 0; nOut < outputs.size(); nOut++)
                {
                    const CTxOut& txout = outputs[nOut];
                    TransactionRecord sub(hash, nTime);
                    sub.actionAccountUUID = sub.fromAccountUUID = accountUUID;
                    sub.actionAccountParentUUID = sub.fromAccountParentUUID = account->getParentUUID();
                    sub.idx = parts.size();
                    sub.involvesWatchAddress = involvesWatchAddress;

                    if(IsMine(*account, txout))
                    {
                        // Ignore parts sent to self, as this is usually the change
                        // from a transaction sent back to our own address.
                        //fixme: (FUT) (HIGH) (REFACTOR) - Are there cases when this isn't true?!?!?!? - if not we can clean the code after this up.
                        continue;
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

                    if (sub.credit == 0 && sub.debit == 0)
                    {
                        continue;
                    }

                    // Fix up display of old phase 3 funding transactions
                    CTxOutPoW2Witness phase3WitnessInfo;
                    if (txout.GetType() == CTxOutType::ScriptLegacyOutput && txout.output.scriptPubKey.ExtractPoW2WitnessFromScript(phase3WitnessInfo))
                    {
                        sub.type = TransactionRecord::WitnessFundSend;
                        CPoW2WitnessDestination witnessDest(phase3WitnessInfo.spendingKeyID, phase3WitnessInfo.witnessKeyID);
                        sub.address = CNativeAddress(witnessDest).ToString();
                        for(const auto& accountPair : wallet->mapAccounts)
                        {
                            if (IsMine(*accountPair.second, witnessDest) == ISMINE_SPENDABLE)
                            {
                                sub.receiveAccountUUID = accountPair.first;
                                
                                TransactionRecord subReceive(hash, nTime);
                                subReceive.type = TransactionRecord::WitnessFundRecv;
                                subReceive.address = CNativeAddress(witnessDest).ToString();
                                subReceive.receiveAccountUUID = subReceive.actionAccountUUID = accountPair.first;
                                subReceive.actionAccountParentUUID = accountPair.second->getParentUUID();
                                subReceive.fromAccountUUID = sub.fromAccountUUID;
                                subReceive.fromAccountParentUUID = sub.fromAccountParentUUID;
                                subReceive.involvesWatchAddress = involvesWatchAddress;
                                subReceive.idx = parts.size();
                                subReceive.credit = sub.debit;
                                subReceive.debit = sub.credit;
                                sub.idx = parts.size()+1;
                                parts.append(subReceive);
                                
                                break;
                            }
                        }
                    }
                    else
                    {
                        CTxDestination address;
                        if (ExtractDestination(txout, address))
                        {
                            // Sent to Gulden Address
                            sub.type = TransactionRecord::SendToAddress;
                            sub.address = CNativeAddress(address).ToString();
                        }
                        else
                        {
                            // Sent to IP, or other non-address transaction like OP_EVAL
                            sub.type = TransactionRecord::SendToOther;
                            sub.address = mapValue["to"];
                        }
                    }

                    parts.append(sub);
                }
            }
            else
            {
                //fixme: (FUT) (HIGH) - This can be displayed better...
                CAmount nNetMixed = 0;
                std::string outAddresses;
                for (const CTxOut& txout : outputs)
                {
                    isminetype mine = IsMine(*account, txout);
                    if (mine == ISMINE_SPENDABLE)
                    {
                        nNetMixed += txout.nValue;
                    }

                    CTxDestination address;
                    if (ExtractDestination(txout, address))
                    {
                        // Sent to Gulden Address
                        if (!outAddresses.empty())
                            outAddresses += " ";
                        outAddresses += CNativeAddress(address).ToString();
                    }
                }
                for (const CTxIn& txin : inputs)
                {
                    //fixme: (FUT) rather just have a CAccount::GetDebit function?
                    isminetype mine = static_cast<const CExtWallet*>(wallet)->IsMine(*account, txin);
                    if (mine == ISMINE_SPENDABLE)
                    {
                        nNetMixed -= wallet->GetDebit(txin, ISMINE_SPENDABLE);
                        //fixme: (FUT) Add a 'payment to self' sub here as well.
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
                        sub.actionAccountUUID = sub.receiveAccountUUID = accountUUID;
                        sub.actionAccountParentUUID = sub.receiveAccountParentUUID = account->getParentUUID();
                    }
                    else
                    {
                        sub.debit = -nNetMixed;
                        sub.actionAccountUUID = sub.fromAccountUUID = accountUUID;
                        sub.actionAccountParentUUID = sub.fromAccountParentUUID = account->getParentUUID();
                    }

                    if (wtx.IsPoW2WitnessCoinBase())
                    {
                        // Prevent old phase3 rewards from displaying incorrectly
                        if (nNetMixed > 4000 * COIN)
                        {
                            sub.type = TransactionRecord::WitnessRenew;
                            sub.debit= nDebit-wtx.tx->GetValueOut();;
                            sub.credit=0;
                        }
                        else
                        {
                            sub.type = TransactionRecord::GeneratedWitness;
                            sub.address = outAddresses;
                        }
                    }
                    else if (wtx.IsCoinBase())
                    {
                        // Prevent old phase3 rewards from displaying incorrectly
                        if (nNetMixed > 4000 * COIN)
                        {
                            sub.type = TransactionRecord::WitnessRenew;
                            sub.debit= nDebit-wtx.tx->GetValueOut();;
                            sub.credit=0;
                        }
                        else
                        {
                            sub.address = outAddresses;
                            if (sub.credit == 20 * COIN && sub.debit == 0)
                            {
                                sub.type = TransactionRecord::GeneratedWitness;
                            }
                            else
                            {
                                sub.type = TransactionRecord::Generated;
                            }
                        }
                    }

                    if (sub.credit == 0 && sub.debit == 0)
                    {
                        continue;
                    }
                    parts.append(sub);
                    parts.last().involvesWatchAddress = involvesWatchAddress;
                }
            }
        }
    }

    // A bit hairy/gross, but try to tie up sender/receiver for internal transfer between accounts.
    //fixme: (FUT) It might be possible to make this try even harder to make a match...
    for (TransactionRecord& record : parts)
    {
        if (record.fromAccountUUID == boost::uuids::nil_generator()())
        {
            for (TransactionRecord& comp : parts)
            {
                if(&record != &comp)
                {
                    if(comp.fromAccountUUID != boost::uuids::nil_generator()() && comp.receiveAccountUUID == boost::uuids::nil_generator()())
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
        else if(record.receiveAccountUUID == boost::uuids::nil_generator()())
        {
            for (TransactionRecord& comp : parts)
            {
                if(&record != &comp)
                {
                    if(comp.receiveAccountUUID != boost::uuids::nil_generator()() && comp.fromAccountUUID == boost::uuids::nil_generator()())
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
    status.cur_num_blocks = ChainHeight();

    if (!CheckFinalTx(wtx, chainActive))
    {
        if (wtx.tx->nLockTime < LOCKTIME_THRESHOLD)
        {
            status.status = TransactionStatus::OpenUntilBlock;
            status.open_for = wtx.tx->nLockTime - chainActive.Height();
        }
        else
        {
            status.status = TransactionStatus::OpenUntilDate;
            status.open_for = wtx.tx->nLockTime;
        }
    }
    // For generated transactions, determine maturity
    else if(type == TransactionRecord::Generated || type == TransactionRecord::GeneratedWitness)
    {
        if (wtx.GetBlocksToMaturity() > 0)
        {
            status.status = TransactionStatus::Immature;

            if (wtx.IsInMainChain())
            {
                status.matures_in = wtx.GetBlocksToMaturity();
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
        else if (status.depth == 0)
        {
            status.status = TransactionStatus::Unconfirmed;
            if (wtx.isAbandoned())
                status.status = TransactionStatus::Abandoned;
        }
        else if (status.depth < RecommendedNumConfirmations)
        {
            status.status = TransactionStatus::Confirming;
        }
        else
        {
            status.status = TransactionStatus::Confirmed;
        }
    }
    status.needsUpdate = false;
}

bool TransactionRecord::statusUpdateNeeded()
{
    AssertLockHeld(cs_main);
    return status.cur_num_blocks != ChainHeight() || status.needsUpdate;
}

QString TransactionRecord::getTxID() const
{
    return QString::fromStdString(hash.ToString());
}

int TransactionRecord::getOutputIndex() const
{
    return idx;
}
