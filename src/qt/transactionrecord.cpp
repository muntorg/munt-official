// Copyright (c) 2011-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// File contains modifications by: The Gulden developers
// All modifications:
// Copyright (c) 2016-2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#include "transactionrecord.h"

#include "base58.h"
#include "consensus/consensus.h"
#include "validation/validation.h"
#include "timedata.h"
#include <Gulden/auto_checkpoints.h>
#include "wallet/wallet.h"
#include "account.h"
#include "script/ismine.h"

#include <stdint.h>

#include <consensus/tx_verify.h>


/* Return positive answer if transaction should be shown in list.
 */
bool TransactionRecord::showTransaction(const CWalletTx &wtx)
{
    AssertLockHeld(pactiveWallet->cs_wallet);

    //fixme: (2.1) - We can potentially remove this for 2.1; depending on how 2.1 handles wallet upgrades.
    // Hide orphaned phase 3 witness earnings when they are orphaned by a subsequent PoW block.
    // We don't want slow/complex "IsPhase3" lookups here etc.
    // So we do this in a rather round about way.
    if (!wtx.IsInMainChain() && wtx.IsCoinBase() && wtx.tx->vout.size() >= 2 && wtx.nIndex == -1)
    {
        // Find the height of our block.
        const auto& findIter = mapBlockIndex.find(wtx.hashBlock);
        if (findIter != mapBlockIndex.end())
        {
            for (const auto& iter : pactiveWallet->mapWallet)
            {
                // Find any transactions from the block that potentially replaces us.
                if (iter.second.hashBlock == wtx.hashBlock)
                {
                    // Scan the outputs of the transaction and if it matches our output then we know we have been orphaned by it and can safely hide.
                    for (const auto& txOut : iter.second.tx->vout)
                    {
                        if (txOut.output == wtx.tx->vout[1].output)
                        {
                            return false;
                        }
                    }
                }
            }
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

    //fixme: (2.1) - This isn't very efficient, we do the same calculations already when verifying the inputs; Ideally we should just cache this information as part of the wtx to avoid recalc.
    // Deal with special "complex" witness transaction types first.
    std::vector<CWitnessTxBundle> witnessBundles;
    CValidationState state;
    int nWalletTxBlockHeight = 0;
    const auto& findIter = mapBlockIndex.find(wtx.hashBlock);
    if (findIter != mapBlockIndex.end())
        nWalletTxBlockHeight = findIter->second->nHeight;
    if (CheckTransactionContextual(*wtx.tx, state, nWalletTxBlockHeight, &witnessBundles))
    {
        for (const auto& txInRef : wtx.tx->vin)
        {
            const COutPoint &prevout = txInRef.prevout;
            if (prevout.IsNull() && wtx.tx->IsPoW2WitnessCoinBase())
                continue;

            const CWalletTx* txPrev = wallet->GetWalletTx(txInRef.prevout.getHash());
            if (!txPrev)
                break;

            if (!CheckTxInputAgainstWitnessBundles(state, &witnessBundles, txPrev->tx->vout[txInRef.prevout.n], txInRef, nWalletTxBlockHeight))
                break;
        }
    }
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
                    for (const auto& [txOut, witnessDetails] : witnessBundle.outputs)
                    {
                        (unused) witnessDetails;
                        for( const auto& accountPair : wallet->mapAccounts )
                        {
                            CAccount* account = accountPair.second;
                            isminetype mine = IsMine(*account, txOut);
                            if (mine)
                            {
                                CTxDestination getAddress;
                                if (ExtractDestination(txOut, getAddress))
                                {
                                    subReceive.address = CGuldenAddress(getAddress).ToString();
                                }
                                subReceive.type = TransactionRecord::WitnessFundRecv;
                                subReceive.involvesWatchAddress = mine & ISMINE_WATCH_ONLY;
                                subReceive.actionAccountUUID = subReceive.receiveAccountUUID = account->getUUID();
                                subReceive.actionAccountParentUUID = subReceive.receiveAccountParentUUID = account->getParentUUID();
                                subReceive.credit = txOut.nValue;
                                subReceive.debit = 0;
                                subReceive.idx = parts.size(); // sequence number
                                parts.append(subReceive);

                                // Remove the witness related outputs so that the remaining decomposition code can ignore it.
                                const auto& txOutRef = txOut;
                                outputs.erase(std::remove_if(outputs.begin(), outputs.end(),[&](CTxOut x){return x == txOutRef;}));

                                break;
                            }
                        }
                        if (subReceive.idx != -1)
                            break;
                    }
                    if (subReceive.idx != -1)
                    {
                        for( const auto& [accountUUID, account] : wallet->mapAccounts )
                        {
                            (unused) accountUUID;
                            isminetype mine = static_cast<const CGuldenWallet*>(wallet)->IsMine(*account, inputs[0]);
                            if (mine)
                            {
                                CTxDestination getAddress;
                                const CWalletTx* parent = wallet->GetWalletTx(inputs[0].prevout.getHash());
                                if (parent != NULL)
                                {
                                    if (ExtractDestination(parent->tx->vout[inputs[0].prevout.n], getAddress))
                                    {
                                        subSend.address = CGuldenAddress(getAddress).ToString();
                                    }
                                }
                                subSend.type = TransactionRecord::WitnessFundSend;
                                subSend.involvesWatchAddress = mine & ISMINE_WATCH_ONLY;
                                subSend.actionAccountUUID = subSend.fromAccountUUID = account->getUUID();
                                subSend.actionAccountParentUUID = subSend.fromAccountParentUUID = account->getParentUUID();
                                subSend.receiveAccountUUID = subSend.receiveAccountParentUUID = subReceive.receiveAccountUUID;
                                parts[subReceive.idx].fromAccountUUID = parts[subReceive.idx].fromAccountParentUUID = subSend.fromAccountUUID;
                                subSend.credit = 0;
                                subSend.debit = subReceive.credit;
                                subSend.idx = parts.size(); // sequence number
                                if (subSend.fromAccountUUID == subSend.receiveAccountUUID)
                                {
                                    parts.pop_back();
                                }
                                else
                                {
                                    parts.append(subSend);
                                }
                                break;
                            }
                        }
                    }
                }
                break;
                case CWitnessTxBundle::WitnessTxType::WitnessType:
                {
                    //fixme: (2.1) - this can change into a break once we apply other fixes described in other fixmes in this function.
                    goto continuedecompose;
                }
                break;
                case CWitnessTxBundle::WitnessTxType::SpendType:
                {
                    TransactionRecord subSend(hash, nTime);
                    TransactionRecord subReceive(hash, nTime);
                    subReceive.idx = -1;
                    for (const auto& output : outputs)
                    {
                        for( const auto& [accountUUID, account] : wallet->mapAccounts )
                        {
                            (unused) accountUUID;
                            isminetype mine = IsMine(*account, output);
                            if (mine)
                            {
                                CTxDestination getAddress;
                                if (ExtractDestination(output, getAddress))
                                {
                                    subReceive.address = CGuldenAddress(getAddress).ToString();
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
                    if (subReceive.idx != -1)
                    {
                        for( const auto& [accountUUID, account] : wallet->mapAccounts )
                        {
                            (unused) accountUUID;
                            isminetype mine = static_cast<const CGuldenWallet*>(wallet)->IsMine(*account, inputs[0]);
                            if (mine)
                            {
                                const CWalletTx* parent = wallet->GetWalletTx(inputs[0].prevout.getHash());
                                if (parent != NULL)
                                {
                                    CTxDestination getAddress;
                                    if (ExtractDestination(parent->tx->vout[inputs[0].prevout.n], getAddress))
                                    {
                                        subSend.address = CGuldenAddress(getAddress).ToString();
                                    }
                                }
                                subSend.type = TransactionRecord::WitnessEmptySend;
                                subSend.involvesWatchAddress = mine & ISMINE_WATCH_ONLY;
                                subSend.actionAccountUUID = subSend.fromAccountUUID = account->getUUID();
                                subSend.actionAccountParentUUID = subSend.fromAccountParentUUID = account->getParentUUID();
                                subSend.receiveAccountUUID = subSend.receiveAccountParentUUID = subReceive.receiveAccountUUID;
                                parts[subReceive.idx].fromAccountUUID = parts[subReceive.idx].fromAccountParentUUID = subSend.fromAccountUUID;
                                subSend.credit = 0;
                                subSend.debit = nDebit;
                                subSend.idx = parts.size(); // sequence number
                                //Phase 3 earnings.
                                if (subSend.fromAccountUUID == subSend.receiveAccountUUID)
                                {
                                    parts.pop_back();
                                }
                                else
                                {
                                    parts.append(subSend);
                                }
                                break;
                            }
                        }
                    }
                }
                break;
                case CWitnessTxBundle::WitnessTxType::RenewType:
                {
                    TransactionRecord sub(hash, nTime);
                    CAmount totalAmountLocked = 0;
                    for (const auto& [txOut, witnessDetails] : witnessBundle.outputs)
                    {
                        (unused)witnessDetails;
                        for( const auto& [accountUUID, account] : wallet->mapAccounts )
                        {
                            (unused)accountUUID;
                            isminetype mine = IsMine(*account, txOut);
                            if (mine)
                            {
                                totalAmountLocked = txOut.nValue;
                                CTxDestination getAddress;
                                if (ExtractDestination(txOut, getAddress))
                                {
                                    sub.address = CGuldenAddress(getAddress).ToString();
                                }
                                sub.type = TransactionRecord::WitnessRenew;
                                sub.involvesWatchAddress = mine & ISMINE_WATCH_ONLY;
                                sub.actionAccountUUID = sub.receiveAccountUUID = account->getUUID();
                                sub.actionAccountParentUUID = sub.receiveAccountParentUUID = account->getParentUUID();
                                sub.idx = parts.size(); // sequence number
                                sub.credit = totalAmountLocked;
                                sub.debit = totalAmountLocked;
                                parts.append(sub);

                                // Remove the witness related outputs so that the remaining decomposition code can ignore it.
                                const auto& txOutRef = txOut;
                                outputs.erase(std::remove_if(outputs.begin(), outputs.end(),[&](CTxOut x){return x == txOutRef;}));
                                //fixme: (2.1) - Remove the inputs as well.

                                break;
                            }
                        }
                    }
                    //fixme: match inputs with outputs
                    /*for (const auto& input : witnessBundle.inputs)
                    {
                        // Remove the witness related inputs so that the remaining decomposition code can ignore it.
                        //inputs.erase(std::remove_if(inputs.begin(), inputs.end(),[&](CTxIn x){return x == input.first;}));
                    }*/
                }
                break;
                case CWitnessTxBundle::WitnessTxType::IncreaseType:
                {
                    TransactionRecord subSend(hash, nTime);
                    TransactionRecord subReceive(hash, nTime);
                    CAmount totalAmountLocked = 0;
                    subReceive.idx = -1;
                    for (const auto& [txOut, witnessDetails] : witnessBundle.outputs)
                    {
                        (unused) witnessDetails;
                        for( const auto& accountPair : wallet->mapAccounts )
                        {
                            CAccount* account = accountPair.second;
                            isminetype mine = IsMine(*account, txOut);
                            if (mine)
                            {
                                totalAmountLocked = txOut.nValue;
                                CTxDestination getAddress;
                                if (ExtractDestination(txOut, getAddress))
                                {
                                    subReceive.address = CGuldenAddress(getAddress).ToString();
                                }
                                subReceive.type = TransactionRecord::WitnessIncreaseRecv;
                                subReceive.involvesWatchAddress = mine & ISMINE_WATCH_ONLY;
                                subReceive.actionAccountUUID = subReceive.receiveAccountUUID = account->getUUID();
                                subReceive.actionAccountParentUUID = subReceive.receiveAccountParentUUID = account->getParentUUID();
                                subReceive.credit = totalAmountLocked;
                                subReceive.debit = 0;

                                isminetype mine = static_cast<const CGuldenWallet*>(wallet)->IsMine(*account, inputs[0]);
                                if (mine)
                                {
                                    const CWalletTx* parent = wallet->GetWalletTx(inputs[0].prevout.getHash());
                                    if (parent != NULL)
                                    {
                                        subReceive.debit = parent->tx->vout[inputs[0].prevout.n].nValue;
                                    }
                                }

                                subReceive.idx = parts.size(); // sequence number
                                parts.append(subReceive);

                                // Remove the witness related outputs so that the remaining decomposition code can ignore it.
                                const auto& txOutRef = txOut;
                                outputs.erase(std::remove_if(outputs.begin(), outputs.end(),[&](CTxOut x){return x == txOutRef;}));

                                break;
                            }
                        }
                        if (subReceive.idx != -1)
                            break;
                    }
                    if (subReceive.idx != -1)
                    {
                        for( const auto& [accountUUID, account] : wallet->mapAccounts )
                        {
                            (unused) accountUUID;
                            isminetype mine = static_cast<const CGuldenWallet*>(wallet)->IsMine(*account, inputs[1]);
                            if (mine)
                            {
                                CTxDestination getAddress;
                                const CWalletTx* parent = wallet->GetWalletTx(inputs[1].prevout.getHash());
                                if (parent != NULL)
                                {
                                    if (ExtractDestination(parent->tx->vout[inputs[1].prevout.n], getAddress))
                                    {
                                        subSend.address = CGuldenAddress(getAddress).ToString();
                                    }
                                }
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
                case CWitnessTxBundle::WitnessTxType::SplitType:
                {
                    TransactionRecord subSend(hash, nTime);
                    TransactionRecord subReceive(hash, nTime);
                    CAmount totalAmountLocked = 0;
                    subReceive.idx = -1;
                    for (const auto& [txOut, witnessDetails] : witnessBundle.outputs)
                    {
                        (unused) witnessDetails;
                        for( const auto& accountPair : wallet->mapAccounts )
                        {
                            CAccount* account = accountPair.second;
                            isminetype mine = IsMine(*account, txOut);
                            if (mine)
                            {
                                totalAmountLocked += txOut.nValue;
                            }
                        }
                    }
                    for (const auto& [txOut, witnessDetails] : witnessBundle.outputs)
                    {
                        (unused) witnessDetails;
                        for( const auto& accountPair : wallet->mapAccounts )
                        {
                            CAccount* account = accountPair.second;
                            isminetype mine = IsMine(*account, txOut);
                            if (mine)
                            {
                                CTxDestination getAddress;
                                if (ExtractDestination(txOut, getAddress))
                                {
                                    subReceive.address = CGuldenAddress(getAddress).ToString();
                                }
                                subReceive.type = TransactionRecord::WitnessSplitRecv;
                                subReceive.involvesWatchAddress = mine & ISMINE_WATCH_ONLY;
                                subReceive.actionAccountUUID = subReceive.receiveAccountUUID = account->getUUID();
                                subReceive.actionAccountParentUUID = subReceive.receiveAccountParentUUID = account->getParentUUID();
                                subReceive.credit = totalAmountLocked;
                                subReceive.debit = totalAmountLocked;

                                subReceive.idx = parts.size(); // sequence number
                                parts.append(subReceive);

                                // Remove the witness related outputs so that the remaining decomposition code can ignore it.
                                const auto& txOutRef = txOut;
                                outputs.erase(std::remove_if(outputs.begin(), outputs.end(),[&](CTxOut x){return x == txOutRef;}));

                                break;
                            }
                        }
                        if (subReceive.idx != -1)
                            break;
                    }
                    //fixme: (2.1) - Add fee transaction to sender account (we should relook an optional "display fee" button though.
                    //It isn't 100% clear if/when we should show fees and when not.
                }
                break;
                case CWitnessTxBundle::WitnessTxType::MergeType:
                {
                    TransactionRecord subSend(hash, nTime);
                    TransactionRecord subReceive(hash, nTime);
                    CAmount totalAmountLocked = 0;
                    subReceive.idx = -1;
                    for (const auto& [txOut, witnessDetails] : witnessBundle.outputs)
                    {
                        (unused) witnessDetails;
                        for( const auto& accountPair : wallet->mapAccounts )
                        {
                            CAccount* account = accountPair.second;
                            isminetype mine = IsMine(*account, txOut);
                            if (mine)
                            {
                                totalAmountLocked = txOut.nValue;
                                CTxDestination getAddress;
                                if (ExtractDestination(txOut, getAddress))
                                {
                                    subReceive.address = CGuldenAddress(getAddress).ToString();
                                }
                                subReceive.type = TransactionRecord::WitnessMergeRecv;
                                subReceive.involvesWatchAddress = mine & ISMINE_WATCH_ONLY;
                                subReceive.actionAccountUUID = subReceive.receiveAccountUUID = account->getUUID();
                                subReceive.actionAccountParentUUID = subReceive.receiveAccountParentUUID = account->getParentUUID();
                                subReceive.credit = totalAmountLocked;
                                subReceive.debit = totalAmountLocked;

                                subReceive.idx = parts.size(); // sequence number
                                parts.append(subReceive);

                                // Remove the witness related outputs so that the remaining decomposition code can ignore it.
                                const auto& txOutRef = txOut;
                                outputs.erase(std::remove_if(outputs.begin(), outputs.end(),[&](CTxOut x){return x == txOutRef;}));

                                break;
                            }
                        }
                        if (subReceive.idx != -1)
                            break;
                    }
                    //fixme: (2.1) - Add fee transaction to sender account (we should relook an optional "display fee" button though.
                    //It isn't 100% clear if/when we should show fees and when not.
                }
                break;
                case CWitnessTxBundle::WitnessTxType::ChangeWitnessKeyType:
                {
                    TransactionRecord subSend(hash, nTime);
                    TransactionRecord subReceive(hash, nTime);
                    CAmount totalAmountLocked = 0;
                    subReceive.idx = -1;
                    for (const auto& [txOut, witnessDetails] : witnessBundle.outputs)
                    {
                        (unused) witnessDetails;
                        for( const auto& accountPair : wallet->mapAccounts )
                        {
                            CAccount* account = accountPair.second;
                            isminetype mine = IsMine(*account, txOut);
                            if (mine)
                            {
                                totalAmountLocked = txOut.nValue;
                                CTxDestination getAddress;
                                if (ExtractDestination(txOut, getAddress))
                                {
                                    subReceive.address = CGuldenAddress(getAddress).ToString();
                                }
                                subReceive.type = TransactionRecord::WitnessChangeKeyRecv;
                                subReceive.involvesWatchAddress = mine & ISMINE_WATCH_ONLY;
                                subReceive.actionAccountUUID = subReceive.receiveAccountUUID = account->getUUID();
                                subReceive.actionAccountParentUUID = subReceive.receiveAccountParentUUID = account->getParentUUID();
                                subReceive.credit = totalAmountLocked;
                                subReceive.debit = totalAmountLocked;

                                subReceive.idx = parts.size(); // sequence number
                                parts.append(subReceive);

                                // Remove the witness related outputs so that the remaining decomposition code can ignore it.
                                const auto& txOutRef = txOut;
                                outputs.erase(std::remove_if(outputs.begin(), outputs.end(),[&](CTxOut x){return x == txOutRef;}));

                                break;
                            }
                        }
                        if (subReceive.idx != -1)
                            break;
                    }
                    //fixme: (2.1) - Add fee transaction to sender account (we should relook an optional "display fee" button though.
                    //It isn't 100% clear if/when we should show fees and when not.
                }
                break;
            }
        }
        //fixme: (2.1) - we should try deal with "complex" (mixed) transactions by removing the below return and still trying to match remaining inputs/outputs.
        return parts;
    }
    continuedecompose:


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
                        //fixme: (2.1) rather just have a CAccount::GetDebit function?
                        isminetype txinMine = static_cast<const CGuldenWallet*>(wallet)->IsMine(*account, txin);
                        if (txinMine == ISMINE_SPENDABLE)
                        {
                            sub.credit -= wallet->GetDebit(txin, ISMINE_SPENDABLE);
                            //fixme: (2.1) Add a 'payment to self' sub here as well.
                        }

                        const CWalletTx* parent = wallet->GetWalletTx(txin.prevout.getHash());
                        if (parent != NULL)
                        {
                            CTxDestination senderAddress;
                            if (ExtractDestination(parent->tx->vout[txin.prevout.n], senderAddress))
                            {
                                if (!addressIn.empty())
                                    addressIn += ", ";
                                addressIn += CGuldenAddress(senderAddress).ToString();
                            }
                        }
                    }
                    if (!addressIn.empty())
                    {
                        sub.address = addressIn;
                    }
                    sub.involvesWatchAddress = mine & ISMINE_WATCH_ONLY;
                    std:: string addressOut;
                    if (ExtractDestination(txout, address) && IsMine(*account, address))
                    {
                        // Received by Gulden Address
                        sub.type = TransactionRecord::RecvWithAddress;
                        addressOut = CGuldenAddress(address).ToString();
                    }
                    else
                    {
                        // Received by IP connection (deprecated features), or a multisignature or other non-simple transaction
                        sub.type = TransactionRecord::RecvFromOther;
                        //sub.address = mapValue["from"];
                    }
                    if (wtx.IsPoW2WitnessCoinBase())
                    {
                        sub.type = TransactionRecord::GeneratedWitness;
                        sub.address = addressOut;
                    }
                    else if (wtx.IsCoinBase())
                    {
                        sub.address = addressOut;
                        // Generated
                        if (sub.credit == 20 * COIN && sub.debit == 0)
                        {
                            sub.type = TransactionRecord::GeneratedWitness;
                        }
                        else
                        {
                            sub.type = TransactionRecord::Generated;
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
        for( const auto& accountPair : wallet->mapAccounts )
        {
            CAccount* account = accountPair.second;
            bool involvesWatchAddress = false;
            isminetype fAllFromMe = ISMINE_SPENDABLE;
            std::vector<CTxIn> vNotFromMe;
            for(const CTxIn& txin : inputs)
            {
                isminetype mine = static_cast<const CGuldenWallet*>(wallet)->IsMine(*account, txin);
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

                sub.actionAccountUUID = sub.receiveAccountUUID = sub.fromAccountUUID = account->getUUID();
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
                //Handle the 'receieve' part of an internal send between two accounts
                TransactionRecord sub(hash, nTime);

                //We don't bother filling in the sender details here for now
                //The sender will get its own transaction record and then we try 'automatically' match the two up and 'swap details' in a loop at the bottom of this function.

                //Fill in all the details for the receive part of the transaction.
                sub.actionAccountUUID = sub.receiveAccountUUID = account->getUUID();
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
                    sub.actionAccountUUID = sub.fromAccountUUID = account->getUUID();
                    sub.actionAccountParentUUID = sub.fromAccountParentUUID = account->getParentUUID();
                    sub.idx = parts.size();
                    sub.involvesWatchAddress = involvesWatchAddress;

                    if(IsMine(*account, txout))
                    {
                        // Ignore parts sent to self, as this is usually the change
                        // from a transaction sent back to our own address.
                        //fixme: (2.1) (HIGH) (REFACTOR) - Are there cases when this isn't true?!?!?!? - if not we can clean the code after this up.
                        continue;
                    }

                    CTxDestination address;
                    if (ExtractDestination(txout, address))
                    {
                        // Sent to Gulden Address
                        sub.type = TransactionRecord::SendToAddress;
                        sub.address = CGuldenAddress(address).ToString();
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

                    if (sub.credit == 0 && sub.debit == 0)
                    {
                        continue;
                    }
                    parts.append(sub);
                }
            }
            else
            {
                //fixme: (2.1) (HIGH) - This can be displayed better...
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
                        outAddresses += CGuldenAddress(address).ToString();
                    }
                }
                for (const CTxIn& txin : inputs)
                {
                    //fixme: (2.1) rather just have a CAccount::GetDebit function?
                    isminetype mine = static_cast<const CGuldenWallet*>(wallet)->IsMine(*account, txin);
                    if (mine == ISMINE_SPENDABLE)
                    {
                        nNetMixed -= wallet->GetDebit(txin, ISMINE_SPENDABLE);
                        //fixme: (2.1) Add a 'payment to self' sub here as well.
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

                    if (wtx.IsPoW2WitnessCoinBase())
                    {
                        sub.type = TransactionRecord::GeneratedWitness;
                        sub.address = outAddresses;
                    }
                    else if (wtx.IsCoinBase())
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
    //fixme: (2.1) It might be possible to make this try even harder to make a match...
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
    status.cur_num_blocks = chainActive.Height();

    if (!CheckFinalTx(wtx))
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
        //fixme: (2.1) (CHECKPOINTS)- Remove this when we remove checkpoints.
        else if (status.depth < RecommendedNumConfirmations || (!IsArgSet("-testnet") && !Checkpoints::IsSecuredBySyncCheckpoint(wtx.hashBlock)))
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
    return status.cur_num_blocks != chainActive.Height() || status.needsUpdate;
}

QString TransactionRecord::getTxID() const
{
    return QString::fromStdString(hash.ToString());
}

int TransactionRecord::getOutputIndex() const
{
    return idx;
}
