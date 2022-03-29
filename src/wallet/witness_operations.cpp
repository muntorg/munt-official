// Copyright (c) 2016-2020 The Gulden developers
// Authored by: Willem de Jonge (willem@isnapp.nl)
// Modifications by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#include "witness_operations.h"

#include <stdexcept>
#include <numeric>
#include <algorithm>
#include <boost/uuid/uuid_io.hpp>
#include <cmath>
#include <inttypes.h>
#include "wallet.h"
#include "validation/witnessvalidation.h"
#include "consensus/consensus.h"
#include "consensus/validation.h"
#include "witnessutil.h"
#include "coincontrol.h"
#include "net.h"
#include "alert.h"
#include "appname.h"
#include "util/moneystr.h"

witnessOutputsInfoVector getCurrentOutputsForWitnessAccount(CAccount* forAccount)
{
    std::map<COutPoint, Coin> allWitnessCoins;
    if (!getAllUnspentWitnessCoins(chainActive, Params(), chainActive.Tip(), allWitnessCoins))
        throw std::runtime_error("Failed to enumerate all witness coins.");

    witnessOutputsInfoVector matchedOutputs;
    for (const auto& [outpoint, coin] : allWitnessCoins)
    {
        if (IsMine(*forAccount, coin.out))
        {
            matchedOutputs.push_back(std::tuple(coin.out, coin.nHeight, coin.nTxIndex, outpoint));
        }
    }
    return matchedOutputs;
}

void extendwitnessaddresshelper(CAccount* fundingAccount, witnessOutputsInfoVector unspentWitnessOutputs, CWallet* pwallet, CAmount requestedAmount, uint64_t requestedLockPeriodInBlocks, std::string* pTxid, CAmount* pFee)
{
    AssertLockHeld(cs_main);
    AssertLockHeld(pwallet->cs_wallet);

    if (unspentWitnessOutputs.size() > 1)
        throw witness_error(witness::RPC_INVALID_ADDRESS_OR_KEY, strprintf("Address has too many witness outputs cannot extend "));

    if (requestedAmount < (gMinimumWitnessAmount*COIN))
        throw witness_error(witness::RPC_TYPE_ERROR, strprintf("Witness amount must be %d or larger", gMinimumWitnessAmount));

    if (requestedLockPeriodInBlocks > MaximumWitnessLockLength())
        throw witness_error(witness::RPC_INVALID_PARAMETER, "Maximum lock period of 3 years exceeded.");

    if (requestedLockPeriodInBlocks < MinimumWitnessLockLength())
        throw witness_error(witness::RPC_INVALID_PARAMETER, "Minimum lock period of 1 month exceeded.");

    // Add a small buffer to give us time to enter the blockchain
    if (requestedLockPeriodInBlocks == MinimumWitnessLockLength())
        requestedLockPeriodInBlocks += 50;

    // Check for immaturity
    const auto& [currentWitnessTxOut, currentWitnessHeight, currentWitnessTxIndex, currentWitnessOutpoint] = unspentWitnessOutputs[0];
    //fixme: (PHASE5) - Minor code cleanup.
    //This check should go through the actual chain maturity stuff (via wtx) and not calculate maturity directly.
    if (chainActive.Tip()->nHeight - currentWitnessHeight < (uint64_t)(COINBASE_MATURITY))
        throw witness_error(witness::RPC_MISC_ERROR, "Cannot perform operation on immature transaction, please wait for transaction to mature and try again");

    // Check type (can't extend script type, must witness once or renew after phase 4 activated)
    if (currentWitnessTxOut.GetType() != CTxOutType::PoW2WitnessOutput)
        throw witness_error(witness::RPC_TYPE_ERROR, "Witness has to be type POW2WITNESS");

    // Calculate existing lock period
    CTxOutPoW2Witness currentWitnessDetails;
    GetPow2WitnessOutput(currentWitnessTxOut, currentWitnessDetails);
    uint64_t remainingLockDurationInBlocks = GetPoW2RemainingLockLengthInBlocks(currentWitnessDetails.lockUntilBlock, chainActive.Tip()->nHeight);
    if (remainingLockDurationInBlocks == 0)
    {
        throw witness_error(witness::RPC_INVALID_PARAMETER, "PoW² witness has already unlocked so cannot be extended.");
    }

    if (requestedLockPeriodInBlocks < remainingLockDurationInBlocks)
    {
        throw witness_error(witness::RPC_INVALID_PARAMETER, strprintf("New lock period [%d] does not exceed remaining lock period [%d]", requestedLockPeriodInBlocks, remainingLockDurationInBlocks));
    }

    if (requestedAmount < currentWitnessTxOut.nValue)
    {
        throw witness_error(witness::RPC_INVALID_PARAMETER, strprintf("New amount [%d] is smaller than current amount [%d]", requestedAmount, currentWitnessTxOut.nValue));
    }

    // Enforce minimum weight
    int64_t newWeight = GetPoW2RawWeightForAmount(requestedAmount, chainActive.Height(), requestedLockPeriodInBlocks);
    if (newWeight < gMinimumWitnessWeight)
    {
        throw witness_error(witness::RPC_INVALID_PARAMETER, "PoW² witness has insufficient weight.");
    }

    // Enforce new weight > old weight
    uint64_t notUsed1, notUsed2;
    int64_t oldWeight = GetPoW2RawWeightForAmount(currentWitnessTxOut.nValue, chainActive.Height(), GetPoW2LockLengthInBlocksFromOutput(currentWitnessTxOut, currentWitnessHeight, notUsed1, notUsed2));
    if (newWeight <= oldWeight)
    {
        throw witness_error(witness::RPC_INVALID_PARAMETER, strprintf("New weight [%d] does not exceed old weight [%d].", newWeight, oldWeight));
    }

    // Find the account for this address
    CAccount* witnessAccount = pwallet->FindBestWitnessAccountForTransaction(currentWitnessTxOut);
    
    if (!witnessAccount)
        throw witness_error(witness::RPC_MISC_ERROR, "Could not locate account for address");
    if ((!witnessAccount->IsPoW2Witness()) || witnessAccount->IsFixedKeyPool())
    {
        throw witness_error(witness::RPC_MISC_ERROR, "Cannot extend a witness-only account as spend key is required to do this.");
    }

    // Finally attempt to create and send the witness transaction.
    CReserveKeyOrScript reservekey(pwallet, fundingAccount, KEYCHAIN_CHANGE);
    std::string reasonForFail;
    CAmount transactionFee;
    CMutableTransaction extendWitnessTransaction(CTransaction::SEGSIG_ACTIVATION_VERSION);
    {
        // Add the existing witness output as an input
        pwallet->AddTxInput(extendWitnessTransaction, CInputCoin(currentWitnessOutpoint, currentWitnessTxOut, true, false, currentWitnessHeight, currentWitnessTxIndex), false);

        // Add new witness output
        CTxOut extendedWitnessTxOutput;
        extendedWitnessTxOutput.SetType(CTxOutType::PoW2WitnessOutput);
        // As we are extending the address we reset the "lock from" and we set the "lock until" everything else except the value remains unchanged.
        extendedWitnessTxOutput.output.witnessDetails.lockFromBlock = 0;
        extendedWitnessTxOutput.output.witnessDetails.lockUntilBlock = chainActive.Tip()->nHeight + requestedLockPeriodInBlocks;
        extendedWitnessTxOutput.output.witnessDetails.spendingKeyID = currentWitnessDetails.spendingKeyID;
        extendedWitnessTxOutput.output.witnessDetails.witnessKeyID = currentWitnessDetails.witnessKeyID;
        extendedWitnessTxOutput.output.witnessDetails.failCount = currentWitnessDetails.failCount;
        extendedWitnessTxOutput.output.witnessDetails.actionNonce = currentWitnessDetails.actionNonce+1;
        extendedWitnessTxOutput.nValue = requestedAmount;
        extendWitnessTransaction.vout.push_back(extendedWitnessTxOutput);

        // Fund the additional amount in the transaction (including fees)
        int changeOutputPosition = 1;
        std::set<int> subtractFeeFromOutputs; // Empty - we don't subtract fee from outputs
        CCoinControl coincontrol;
        if (!pwallet->FundTransaction(fundingAccount, extendWitnessTransaction, transactionFee, changeOutputPosition, reasonForFail, false, subtractFeeFromOutputs, coincontrol, reservekey))
        {
            throw witness_error(witness::RPC_MISC_ERROR, strprintf("Failed to fund transaction [%s]", reasonForFail.c_str()));
        }
    }

    uint256 extendTransactionHash;
    {
        LOCK2(cs_main, pwallet->cs_wallet);
        if (!pwallet->SignAndSubmitTransaction(reservekey, extendWitnessTransaction, reasonForFail, &extendTransactionHash))
        {
            throw witness_error(witness::RPC_MISC_ERROR, strprintf("Failed to commit transaction [%s]", reasonForFail.c_str()));
        }
    }

    // Set result parameters
    if (pTxid != nullptr)
        *pTxid = extendTransactionHash.GetHex();
    if (pFee != nullptr)
        *pFee = transactionFee;
}

void fundwitnessaccount(CWallet* pwallet, CAccount* fundingAccount, CAccount* witnessAccount, CAmount amount, uint64_t requestedPeriodInBlocks, bool fAllowMultiple, std::string* pTxid, CAmount* pFee)
{
    if (!IsPow2Phase2Active(chainActive.Tip()))
    {
        throw witness_error(witness::RPC_MISC_ERROR, strprintf("Can't create witness accounts before phase 2 activates"));
    }
    
    std::vector<CAmount> amounts;
    LOCK(cs_main);
    
    // For the sake of testnet we turn off 'splitting' for very short lock periods as it was triggering some testnet specific 'short period' bugs that aren't worth fixing for mainnet (where they can't occur)
    if (IsSegSigEnabled(chainActive.TipPrev()) && requestedPeriodInBlocks > 1000)
    {
        CGetWitnessInfo witnessInfo = GetWitnessInfoWrapper();
        amounts = optimalWitnessDistribution(amount, requestedPeriodInBlocks, witnessInfo.nTotalWeightEligibleRaw);
    }
    else
        amounts = { amount };

    fundwitnessaccount(pwallet, fundingAccount, witnessAccount, amounts, requestedPeriodInBlocks, fAllowMultiple, pTxid, pFee);
}

void fundwitnessaccount(CWallet* pwallet, CAccount* fundingAccount, CAccount* witnessAccount, const std::vector<CAmount>& amounts, uint64_t requestedPeriodInBlocks, bool fAllowMultiple, std::string* pTxid, CAmount* pFee)
{
    if (!IsPow2Phase2Active(chainActive.Tip()))
    {
        throw witness_error(witness::RPC_MISC_ERROR, strprintf("Can't create witness accounts before phase 2 activates"));
    }

    if (pwallet == nullptr || witnessAccount == nullptr || fundingAccount == nullptr)
        throw witness_error(witness::RPC_INVALID_PARAMETER, "Require non-null pwallet, fundingAccount, witnessAccount");

    LOCK2(cs_main, pwallet->cs_wallet);

    if (!witnessAccount->IsPoW2Witness())
        throw witness_error(witness::RPC_MISC_ERROR, strprintf("Specified account is not a witness account [%s].", boost::uuids::to_string(witnessAccount->getUUID())));
    if (witnessAccount->m_Type == WitnessOnlyWitnessAccount || !witnessAccount->IsHD())
        throw witness_error(witness::RPC_MISC_ERROR, strprintf("Cannot fund a witness only witness account [%s].", boost::uuids::to_string(witnessAccount->getUUID())));

    const auto& unspentWitnessOutputs = getCurrentOutputsForWitnessAccount(witnessAccount);
    if (unspentWitnessOutputs.size() > 0)
    {
        if (!fAllowMultiple)
            throw std::runtime_error("Account already has an active funded witness address. Perhaps you intended to 'extend' it? See: 'help extendwitnessaccount'");
    }

    if (std::any_of(amounts.begin(), amounts.end(), [](const auto& amount){ return amount < (gMinimumWitnessAmount*COIN); }))
        throw witness_error(witness::RPC_TYPE_ERROR, strprintf("Witness amount must be %d or larger", gMinimumWitnessAmount));

    if (requestedPeriodInBlocks == 0)
        throw witness_error(witness::RPC_TYPE_ERROR, "Invalid number passed for lock period.");

    if (requestedPeriodInBlocks > MaximumWitnessLockLength())
        throw witness_error(witness::RPC_INVALID_PARAMETER, "Maximum lock period of 3 years exceeded.");

    if (requestedPeriodInBlocks < MinimumWitnessLockLength())
        throw witness_error(witness::RPC_INVALID_PARAMETER, "Minimum lock period of 1 month exceeded.");

    // Add a small buffer to give us time to enter the blockchain
    if (requestedPeriodInBlocks == MinimumWitnessLockLength())
        requestedPeriodInBlocks += 50;

    // Enforce minimum weight for each amount
    if (std::any_of(amounts.begin(), amounts.end(), [&](const auto& amount){ return GetPoW2RawWeightForAmount(amount, chainActive.Height(), requestedPeriodInBlocks) < gMinimumWitnessWeight; }))
    {
        throw witness_error(witness::RPC_INVALID_PARAMETER, "PoW² witness has insufficient weight.");
    }

    // Finally attempt to create and send the witness transaction.
    CPoW2WitnessDestination destinationPoW2Witness;
    destinationPoW2Witness.lockFromBlock = 0;
    destinationPoW2Witness.lockUntilBlock = chainActive.Tip()->nHeight + requestedPeriodInBlocks;
    destinationPoW2Witness.failCount = 0;
    destinationPoW2Witness.actionNonce = 0;
    {
        CReserveKeyOrScript keyWitness(pactiveWallet, witnessAccount, KEYCHAIN_WITNESS);
        CPubKey pubWitnessKey;
        if (!keyWitness.GetReservedKey(pubWitnessKey))
            throw witness_error(witness::RPC_INVALID_ADDRESS_OR_KEY, "Error allocating witness key for witness account.");

        //We delibritely return the key here, so that if we fail we won't leak the key.
        //The key will be marked as used when the transaction is accepted anyway.
        keyWitness.ReturnKey();
        destinationPoW2Witness.witnessKey = pubWitnessKey.GetID();
    }
    {
        //Code should be refactored to only call 'KeepKey' -after- success, a bit tricky to get there though.
        CReserveKeyOrScript keySpending(pactiveWallet, witnessAccount, KEYCHAIN_SPENDING);
        CPubKey pubSpendingKey;
        if (!keySpending.GetReservedKey(pubSpendingKey))
            throw witness_error(witness::RPC_INVALID_ADDRESS_OR_KEY, "Error allocating spending key for witness account.");

        //We delibritely return the key here, so that if we fail we won't leak the key.
        //The key will be marked as used when the transaction is accepted anyway.
        keySpending.ReturnKey();
        destinationPoW2Witness.spendingKey = pubSpendingKey.GetID();
    }

    CAmount transactionFee;
    std::string strError;
    std::vector<CRecipient> vecSend;
    int nChangePosRet = -1;

    std::for_each(amounts.begin(), amounts.end(), [&](const CAmount& amount){
        CRecipient recipient = ( IsSegSigEnabled(chainActive.TipPrev()) ? ( CRecipient(GetPoW2WitnessOutputFromWitnessDestination(destinationPoW2Witness), amount, false) ) : ( CRecipient(GetScriptForDestination(destinationPoW2Witness), amount, false) ) ) ;
        if (!IsSegSigEnabled(chainActive.TipPrev()))
        {
            // We have to copy this anyway even though we are using a CSCript as later code depends on it to grab the witness key id.
            recipient.witnessDetails.witnessKeyID = destinationPoW2Witness.witnessKey;
        }

        //NB! Setting this is -super- important, if we don't then encrypted wallets may fail to witness.
        recipient.witnessForAccount = witnessAccount;
        vecSend.push_back(recipient);
    });

    CWalletTx wtx;
    CReserveKeyOrScript reservekey(pwallet, fundingAccount, KEYCHAIN_CHANGE);
    if (!pwallet->CreateTransaction(fundingAccount, vecSend, wtx, reservekey, transactionFee, nChangePosRet, strError))
    {
        throw witness_error(witness::RPC_WALLET_ERROR, strError);
    }

    CValidationState state;
    if (!pwallet->CommitTransaction(wtx, reservekey, g_connman.get(), state))
    {
        strError = strprintf("Error: The transaction was rejected! Reason given: %s", state.GetRejectReason());
        throw witness_error(witness::RPC_WALLET_ERROR, strError);
    }

    // Set result parameters
    if (pTxid != nullptr)
        *pTxid = wtx.GetHash().GetHex();
    if (pFee != nullptr)
        *pFee = transactionFee;
}

void renewwitnessaccount(CWallet* pwallet, CAccount* fundingAccount, CAccount* witnessAccount, std::string* pTxid, CAmount* pFee)
{
    if (pwallet == nullptr || witnessAccount == nullptr || fundingAccount == nullptr)
        throw witness_error(witness::RPC_INVALID_PARAMETER, "Require non-null pwallet, fundingAccount, witnessAccount");

    LOCK2(cs_main, pwallet->cs_wallet);

    if (pwallet->IsLocked()) {
        throw std::runtime_error("Wallet locked");
    }

    if ((!witnessAccount->IsPoW2Witness()) || witnessAccount->IsFixedKeyPool())
    {
        throw witness_error(witness::RPC_MISC_ERROR, "Cannot renew a witness-only account as spend key is required to do this.");
    }

    std::string strError;
    CMutableTransaction tx(CURRENT_TX_VERSION_POW2);
    CReserveKeyOrScript changeReserveKey(pactiveWallet, fundingAccount, KEYCHAIN_EXTERNAL);
    CAmount transactionFee;
    if (!pwallet->PrepareRenewWitnessAccountTransaction(fundingAccount, witnessAccount, changeReserveKey, tx, transactionFee, strError))
    {
        throw std::runtime_error(strprintf("Failed to renew witness [%s]", strError.c_str()));
    }

    uint256 upgradeTransactionHash;
    if (!pwallet->SignAndSubmitTransaction(changeReserveKey, tx, strError, &upgradeTransactionHash))
    {
        LogPrintf("renewwitnessaccount Failed to sign transaction [%s]", strError.c_str());
        throw std::runtime_error(strprintf("Failed to sign transaction [%s]", strError.c_str()));
    }

    // Set result parameters
    if (pTxid != nullptr)
        *pTxid = upgradeTransactionHash.GetHex();
    if (pFee != nullptr)
        *pFee = transactionFee;
}

void rotatewitnessaddresshelper(CAccount* fundingAccount, witnessOutputsInfoVector unspentWitnessOutputs, CWallet* pwallet, std::string* pTxid, CAmount* pFee)
{
    if (unspentWitnessOutputs.size() == 0)
        throw witness_error(witness::RPC_INVALID_ADDRESS_OR_KEY, strprintf("Too few witness outputs cannot rotate."));

    // Check for immaturity
    for (const auto& [currentWitnessTxOut, currentWitnessHeight, currentWitnessTxIndex, currentWitnessOutpoint]: unspentWitnessOutputs)
    {
        (unused) currentWitnessTxOut;
        (unused) currentWitnessOutpoint;
        (unused) currentWitnessTxIndex;
        //fixme: (PHASE5) - Minor code cleanup.
        //This check should go through the actual chain maturity stuff (via wtx) and not calculate maturity directly.
        if (chainActive.Tip()->nHeight - currentWitnessHeight < (uint64_t)(COINBASE_MATURITY))
            throw witness_error(witness::RPC_MISC_ERROR, "Cannot perform operation on immature transaction, please wait for transaction to mature and try again");
    }


    // Find the account for this address
    CAccount* witnessAccount = nullptr;
    {
        CTxOutPoW2Witness currentWitnessDetails;
        const auto& currentWitnessTxOut = std::get<0>(unspentWitnessOutputs[0]);
        GetPow2WitnessOutput(currentWitnessTxOut, currentWitnessDetails);
        witnessAccount = pwallet->FindBestWitnessAccountForTransaction(currentWitnessTxOut);
    }
    if (!witnessAccount)
    {
        throw witness_error(witness::RPC_MISC_ERROR, "Could not locate account");
    }
    if ((!witnessAccount->IsPoW2Witness()) || witnessAccount->IsFixedKeyPool())
    {
        throw witness_error(witness::RPC_MISC_ERROR, "Cannot rotate a witness-only account as spend key is required to do this.");
    }

    // Finally attempt to create and send the witness transaction.
    CReserveKeyOrScript reservekey(pwallet, fundingAccount, KEYCHAIN_CHANGE);
    std::string reasonForFail;
    CAmount transactionFee;
    CMutableTransaction rotateWitnessTransaction(CTransaction::SEGSIG_ACTIVATION_VERSION);
    CPubKey pubWitnessKey;
    {
        // Get new witness key
        CReserveKeyOrScript keyWitness(pactiveWallet, witnessAccount, KEYCHAIN_WITNESS);
        if (!keyWitness.GetReservedKey(pubWitnessKey))
        {
            throw witness_error(witness::RPC_INVALID_ADDRESS_OR_KEY, "Error allocating witness key for witness account.");
        }

        //We delibritely return the key here, so that if we fail we won't leak the key.
        //The key will be marked as used when the transaction is accepted anyway.
        keyWitness.ReturnKey();
    }

    // Add all existing outputs as inputs and new outputs
    for (const auto& [currentWitnessTxOut, currentWitnessHeight, currentWitnessTxIndex, currentWitnessOutpoint]: unspentWitnessOutputs)
    {
        // Add input
        pwallet->AddTxInput(rotateWitnessTransaction, CInputCoin(currentWitnessOutpoint, currentWitnessTxOut, true, false, currentWitnessHeight, currentWitnessTxIndex), false);

        // Get witness details
        CTxOutPoW2Witness currentWitnessDetails;
        if (!GetPow2WitnessOutput(currentWitnessTxOut, currentWitnessDetails))
            throw witness_error(witness::RPC_MISC_ERROR, "Failure extracting witness details.");

        // Add rotated output
        CTxOut rotatedWitnessTxOutput;
        rotatedWitnessTxOutput.SetType(CTxOutType::PoW2WitnessOutput);
        rotatedWitnessTxOutput.output.witnessDetails.lockFromBlock = currentWitnessDetails.lockFromBlock;
        // Ensure consistent lock from
        if (rotatedWitnessTxOutput.output.witnessDetails.lockFromBlock == 0)
        {
            rotatedWitnessTxOutput.output.witnessDetails.lockFromBlock = currentWitnessHeight;
        }
        rotatedWitnessTxOutput.output.witnessDetails.lockUntilBlock = currentWitnessDetails.lockUntilBlock;
        rotatedWitnessTxOutput.output.witnessDetails.spendingKeyID = currentWitnessDetails.spendingKeyID;
        rotatedWitnessTxOutput.output.witnessDetails.witnessKeyID = pubWitnessKey.GetID();
        rotatedWitnessTxOutput.output.witnessDetails.failCount = currentWitnessDetails.failCount;
        rotatedWitnessTxOutput.output.witnessDetails.actionNonce = currentWitnessDetails.actionNonce+1;
        rotatedWitnessTxOutput.nValue = currentWitnessTxOut.nValue;
        rotateWitnessTransaction.vout.push_back(rotatedWitnessTxOutput);
    }

    // Fund the additional amount in the transaction (including fees)
    int changeOutputPosition = 1;
    std::set<int> subtractFeeFromOutputs; // Empty - we don't subtract fee from outputs
    CCoinControl coincontrol;
    if (!pwallet->FundTransaction(fundingAccount, rotateWitnessTransaction, transactionFee, changeOutputPosition, reasonForFail, false, subtractFeeFromOutputs, coincontrol, reservekey))
    {
        throw witness_error(witness::RPC_MISC_ERROR, strprintf("Failed to fund transaction [%s]", reasonForFail.c_str()));
    }


    //fixme: (PHASE5) - Minor code cleanup
    //Improve this a bit, we should make this 'atomic'
    //i.e. only perform the action that can't be rolled back (adding the key to the account) if the entire thing succeeds
    //So only after the signing of the transaction is a success
    //Also (low) this shares common code with CreateTransaction() - so it should be factored out into a common helper that both can use.
    CKey privWitnessKey;
    if (!witnessAccount->GetKey(pubWitnessKey.GetID(), privWitnessKey))
    {
        reasonForFail = strprintf("Wallet error, failed to retrieve private witness key.");
        throw witness_error(witness::RPC_MISC_ERROR, strprintf("Failed to fund transaction [%s]", reasonForFail.c_str()));
    }
    if (!pwallet->AddKeyPubKey(privWitnessKey, privWitnessKey.GetPubKey(), *witnessAccount, KEYCHAIN_WITNESS))
    {
        reasonForFail = strprintf("Wallet error, failed to store witness key.");
        throw witness_error(witness::RPC_MISC_ERROR, strprintf("Failed to fund transaction [%s]", reasonForFail.c_str()));
    }

    uint256 rotateTransactionHash;
    {
        LOCK2(cs_main, pactiveWallet->cs_wallet);
        if (!pwallet->SignAndSubmitTransaction(reservekey, rotateWitnessTransaction, reasonForFail, &rotateTransactionHash))
        {
            throw witness_error(witness::RPC_MISC_ERROR, strprintf("Failed to commit transaction [%s]", reasonForFail.c_str()));
        }
    }

    // Set result parameters
    if (pTxid != nullptr)
        *pTxid = rotateTransactionHash.GetHex();
    if (pFee != nullptr)
        *pFee = transactionFee;
}

void rotatewitnessaccount(CWallet* pwallet, CAccount* fundingAccount, CAccount* witnessAccount, std::string* pTxid, CAmount* pFee)
{
    if (pwallet == nullptr || witnessAccount == nullptr || fundingAccount == nullptr)
        throw witness_error(witness::RPC_INVALID_PARAMETER, "Require non-null pwallet, fundingAccount, witnessAccount");

    LOCK2(cs_main, pwallet->cs_wallet);

    if (!IsSegSigEnabled(chainActive.TipPrev()))
        throw std::runtime_error("Cannot use this command before segsig activates");

    if (pwallet->IsLocked()) {
        throw std::runtime_error("Wallet locked");
    }

    if ((!witnessAccount->IsPoW2Witness()) || witnessAccount->IsFixedKeyPool())
    {
        throw witness_error(witness::RPC_MISC_ERROR, "Cannot rotate a witness-only account as spend key is required to do this.");
    }

    const auto& unspentWitnessOutputs = getCurrentOutputsForWitnessAccount(witnessAccount);
    if (unspentWitnessOutputs.size() == 0)
        throw witness_error(witness::RPC_INVALID_ADDRESS_OR_KEY, strprintf("Account does not contain any witness outputs [%s].",
                                                                           boost::uuids::to_string(witnessAccount->getUUID())));

    rotatewitnessaddresshelper(fundingAccount, unspentWitnessOutputs, pwallet, pTxid, pFee);
}

CGetWitnessInfo GetWitnessInfoWrapper()
{
    CGetWitnessInfo witnessInfo;

    CBlock block;
    {
        LOCK(cs_main); // Required for ReadBlockFromDisk as well as GetWitnessInfo.
        if (!ReadBlockFromDisk(block, chainActive.Tip(), Params()))
        {
            std::string strErrorMessage = "Error in GetWitnessInfoWrapper, failed to read block from disk";
            CAlert::Notify(strErrorMessage, true, true);
            LogPrintf("%s", strErrorMessage.c_str());
            throw std::runtime_error(strErrorMessage);
        }
        if (!GetWitness(chainActive, Params(), nullptr, chainActive.Tip()->pprev, block, witnessInfo))
        {
            std::string strErrorMessage = "Error in GetWitnessInfoWrapper, failed to retrieve witness info";
            CAlert::Notify(strErrorMessage, true, true);
            LogPrintf("%s", strErrorMessage.c_str());
            throw std::runtime_error(strErrorMessage);
        }
    }

    return witnessInfo;
}

void EnsureMatchingWitnessCharacteristics(const witnessOutputsInfoVector& unspentWitnessOutputs)
{
    // Properties should beidentical, except lockFromBlock is special.
    // Extending (or initial funding) of a multiple-part witness can result in different lockFromBlock values, which are test as follows:
    // A) All outputs that have a non-zero lockFromBlock should share the same non-zero value.
    // B) All outputs with a zero lockFromBlock should share the same coin height.
    // C) And finally the non-zero value from A and B should be the same.
    // When a new witness is created it enters with a lockFromBlock == 0, which is replaced by the height the tx entered the chain
    // in a later operation (ie. a witness operation, rearrange, renew etc.)
    if (unspentWitnessOutputs.size() > 0)
    {
        CTxOutPoW2Witness witnessDetails0;
        if (!GetPow2WitnessOutput(std::get<0>(unspentWitnessOutputs[0]), witnessDetails0))
            throw std::runtime_error("Failure extracting witness details.");

        // collect values for A and B tests
        uint64_t nonZeroFrom = 0;
        uint64_t nonZeroCoin = 0;
        for (unsigned int i = 0; i < unspentWitnessOutputs.size(); i++) {
            CTxOutPoW2Witness witnessDetails;
            if (!GetPow2WitnessOutput(std::get<0>(unspentWitnessOutputs[i]), witnessDetails))
                throw std::runtime_error("Failure extracting witness details.");
            if (witnessDetails.lockFromBlock != 0)
                nonZeroFrom = witnessDetails.lockFromBlock;
            else
                nonZeroCoin = std::get<1>(unspentWitnessOutputs[i]);

            if (nonZeroFrom != 0 && nonZeroCoin != 0)
                break;
        }

        // test C
        if (nonZeroFrom != 0 && nonZeroCoin != 0 && nonZeroFrom != nonZeroCoin)
            throw std::runtime_error("Multiple addresses with different witness characteristics in account. Use RPC to furter handle this account.");

        for (unsigned int i = 0; i < unspentWitnessOutputs.size(); i++) {
            CTxOutPoW2Witness witnessCompare;
            if (!GetPow2WitnessOutput(std::get<0>(unspentWitnessOutputs[i]), witnessCompare))
                throw std::runtime_error("Failure extracting witness details.");
            if (witnessCompare.lockUntilBlock != witnessDetails0.lockUntilBlock ||
                witnessCompare.spendingKeyID != witnessDetails0.spendingKeyID ||
                witnessCompare.witnessKeyID != witnessDetails0.witnessKeyID ||
                (witnessCompare.lockFromBlock != 0 && witnessCompare.lockFromBlock != nonZeroFrom) || // test A
                (witnessCompare.lockFromBlock == 0 && std::get<1>(unspentWitnessOutputs[i]) != nonZeroCoin)) // test B
            {
                throw std::runtime_error("Multiple addresses with different witness characteristics in account. Use RPC to furter handle this account.");
            }
        }

    }
}


CWitnessBundlesRef GetWitnessBundles(const CWalletTx& wtx)
{
    CWitnessBundlesRef pWitnessBundles = std::make_shared<CWitnessBundles>();
    if (wtx.witnessBundles)
    {
        pWitnessBundles = wtx.witnessBundles;
        return pWitnessBundles;
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
                    const CWalletTx* txPrev = pactiveWallet->GetWalletTx(outpoint.getTransactionHash());
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
            CWalletDB walletdb(*pactiveWallet->dbw);
            walletdb.WriteTx(w);
        }
    }

    return pWitnessBundles;
}
    
    
CWitnessAccountStatus GetWitnessAccountStatus(CWallet* pWallet, CAccount* account, CGetWitnessInfo* pWitnessInfo)
{
    WitnessStatus status;

    LOCK2(cs_main, pWallet->cs_wallet);

    CGetWitnessInfo witnessInfo;

    if (chainActive.Height() > 0 && IsPow2Phase5Active(chainActive.Height())) {
        witnessInfo = GetWitnessInfoWrapper();
    }

    // Collect uspent witnesses coins for the account
    std::vector<RouletteItem> accountItems;
    for (const auto& item : witnessInfo.witnessSelectionPoolUnfiltered)
    {
        if (IsMine(*account, item.coin.out))
            accountItems.push_back(item);
    }

    bool haveUnspentWitnessUtxo = accountItems.size() > 0;

    CTxOutPoW2Witness witnessDetails0;
    if (haveUnspentWitnessUtxo && !GetPow2WitnessOutput(accountItems[0].coin.out, witnessDetails0))
        throw std::runtime_error("Failure extracting witness details.");

    uint64_t nLockFromBlock = 0;
    uint64_t nLockUntilBlock = 0;
    uint64_t nLockPeriodInBlocks = accountItems.size() > 0 ? GetPoW2LockLengthInBlocksFromOutput(accountItems[0].coin.out, accountItems[0].coin.nHeight, nLockFromBlock, nLockUntilBlock) : 0;

    const auto& unspentWitnessOutputs = getCurrentOutputsForWitnessAccount(account);
    EnsureMatchingWitnessCharacteristics(unspentWitnessOutputs);

    bool hasUnconfirmedBalance = pactiveWallet->GetUnconfirmedBalance(account, true, true) > 0;
    bool hasImmatureBalance = pactiveWallet->GetImmatureBalance(account, true, true) > 0;
    CAmount lockedBalance = pactiveWallet->GetLockedBalance(account, true);
    bool hasLockedBalance = lockedBalance > 0;
    bool hasMatureBalance = pactiveWallet->GetBalance(account, true, false, true) > 0;
    
    bool hasBalance = hasUnconfirmedBalance||hasImmatureBalance||hasMatureBalance||hasLockedBalance;

    bool isLocked = haveUnspentWitnessUtxo && IsPoW2WitnessLocked(witnessDetails0, chainActive.Tip()->nHeight);

    uint64_t networkWeight = witnessInfo.nTotalWeightRaw;
    bool isExpired = haveUnspentWitnessUtxo && std::any_of(accountItems.begin(), accountItems.end(), [=](const RouletteItem& ri){
                         return witnessHasExpired(ri.nAge, ri.nWeight, networkWeight);
                     });

    if (!haveUnspentWitnessUtxo && (hasImmatureBalance||hasUnconfirmedBalance||hasLockedBalance))
    {
        status = WitnessStatus::Pending;
    }
    else if (!haveUnspentWitnessUtxo && !hasBalance)
    {
        status = WitnessStatus::Empty;
    }
    else if (!haveUnspentWitnessUtxo && hasMatureBalance && !(hasImmatureBalance || hasUnconfirmedBalance))
    {
        status = WitnessStatus::EmptyWithRemainder;
    }
    else if (haveUnspentWitnessUtxo && hasBalance && isLocked && isExpired)
    {
        status = WitnessStatus::Expired;
    }
    else if (haveUnspentWitnessUtxo && hasBalance && isLocked && !isExpired)
    {
        status = WitnessStatus::Witnessing;
    }
    else if (haveUnspentWitnessUtxo && hasBalance && isLocked && isExpired)
    {
        status = WitnessStatus::Expired;
    }
    else if (haveUnspentWitnessUtxo && hasBalance && !isLocked)
    {
        status = WitnessStatus::Ended;
    }
    else if (haveUnspentWitnessUtxo && !hasBalance && !isLocked)
    {
        status = WitnessStatus::Emptying;
    }
    else
    {
        throw std::runtime_error("Unable to determine witness state.");
    }

    CAmount originAmountLocked= 0;
    uint64_t originWeight = 0;
    bool hasUnconfirmedWittnessTx = false;
    for (const auto& it: pWallet->mapWallet)
    {
        const auto& wtx = it.second;

        // NOTE: assuming any unconfirmed tx here is a witness; avoid getting the witness bundles and testing those, this will almost always be correct. Any edge cases where this fails will automatically resolve once the tx confirms.
        if (!hasUnconfirmedWittnessTx && account->HaveWalletTx(it.second) && it.second.InMempool())
        {
            hasUnconfirmedWittnessTx = true;
        }
        
        // Coinbase can only be generation not lock
        if (!wtx.IsPoW2WitnessCoinBase())
        {
            const auto& bundles = GetWitnessBundles(wtx);
            if (bundles && bundles->size() > 0)
            {
                for (const CWitnessTxBundle& witnessBundle : *bundles)
                {
                    switch (witnessBundle.bundleType)
                    {
                        case CWitnessTxBundle::WitnessTxType::CreationType:
                        {
                            originAmountLocked += std::accumulate(witnessBundle.outputs.begin(), witnessBundle.outputs.end(), CAmount(0), 
                            [&account](const CAmount acc, const auto& txIter)
                                {
                                    const CTxOut& txOut = std::get<0>(txIter);
                                    if (IsMine(*account, txOut))
                                    {
                                        return acc + txOut.nValue;
                                    }
                                    return acc;
                                }
                            );
                        }
                        break;
                        case CWitnessTxBundle::WitnessTxType::IncreaseType:
                        {
                            //fixme: IMPLEMENT
                        }
                        break;
                        case CWitnessTxBundle::WitnessTxType::RearrangeType:
                        {
                        //fixme: IMPLEMENT
                        }
                        break;
                        default:
                        break;
                    } 
                }
            }
        }
    }

    // hasUnconfirmedWittnessTx -> renewed, extended ...
    if (status == WitnessStatus::Expired && hasUnconfirmedWittnessTx)
        status = WitnessStatus::Witnessing;

    bool hasScriptLegacyOutput = std::any_of(accountItems.begin(), accountItems.end(), [](const RouletteItem& ri){ return ri.coin.out.GetType() == CTxOutType::ScriptLegacyOutput; });

    std::vector<uint64_t> parts;
    std::transform(accountItems.begin(), accountItems.end(), std::back_inserter(parts), [](const auto& ri) { return ri.nWeight; });

    CWitnessAccountStatus result {
        account,
        status,
        networkWeight,
        //NB! We always want the account weight (even if expired) - otherwise how do we e.g. draw a historical graph of the expected earnings for the expired account?
        std::accumulate(accountItems.begin(), accountItems.end(), uint64_t(0), [](const uint64_t acc, const RouletteItem& ri){ return acc + ri.nWeight; }),
        originWeight,
        lockedBalance,
        originAmountLocked,
        hasScriptLegacyOutput,
        hasUnconfirmedWittnessTx,
        nLockFromBlock,
        nLockUntilBlock,
        nLockPeriodInBlocks,
        parts
    };

    if (pWitnessInfo && IsPow2Phase3Active(chainActive.Height()))
        *pWitnessInfo = std::move(witnessInfo);

    return result;
}

void redistributeandextendwitnessaccount(CWallet* pwallet, CAccount* fundingAccount, CAccount* witnessAccount, const std::vector<CAmount>& redistributionAmounts, uint64_t requestedLockPeriodInBlocks, std::string* pTxid, CAmount* pFee)
{
    if (pwallet == nullptr || witnessAccount == nullptr || fundingAccount == nullptr)
    {
        throw witness_error(witness::RPC_INVALID_PARAMETER, "Require non-null pwallet, fundingAccount, witnessAccount");
    }

    LOCK2(cs_main, pwallet->cs_wallet);

    if (!IsSegSigEnabled(chainActive.TipPrev()))
    {
        throw std::runtime_error("Segsig not activated");
    }

    if (pwallet->IsLocked())
    {
        throw std::runtime_error("Wallet locked");
    }

    if ((!witnessAccount->IsPoW2Witness()) || witnessAccount->IsFixedKeyPool())
    {
        throw witness_error(witness::RPC_MISC_ERROR, "Cannot operate on witness-only account as spend key is required to do this.");
    }

    const auto& unspentWitnessOutputs = getCurrentOutputsForWitnessAccount(witnessAccount);
    if (unspentWitnessOutputs.size() == 0)
    {
        throw witness_error(witness::RPC_INVALID_ADDRESS_OR_KEY, strprintf("Account does not contain any witness outputs [%s].", boost::uuids::to_string(witnessAccount->getUUID())));
    }

    EnsureMatchingWitnessCharacteristics(unspentWitnessOutputs);

    const auto& [currentWitnessTxOut, currentWitnessHeight, currentWitnessTxIndex, currentWitnessOutpoint] = unspentWitnessOutputs[0];
    (unused)currentWitnessHeight;
    (unused)currentWitnessTxIndex;
    (unused)currentWitnessOutpoint;

    // Get the current witness details
    CTxOutPoW2Witness currentWitnessDetails;
    GetPow2WitnessOutput(currentWitnessTxOut, currentWitnessDetails);

    if (std::any_of(redistributionAmounts.begin(), redistributionAmounts.end(), [](const auto& amount){ return amount < (gMinimumWitnessAmount*COIN); }))
        throw witness_error(witness::RPC_TYPE_ERROR, strprintf("Witness amount must be %d or larger", gMinimumWitnessAmount));

    uint64_t remainingLockDurationInBlocks = GetPoW2RemainingLockLengthInBlocks(currentWitnessDetails.lockUntilBlock, chainActive.Tip()->nHeight);
    if (remainingLockDurationInBlocks == 0)
    {
        throw witness_error(witness::RPC_INVALID_PARAMETER, "PoW² witness has already unlocked.");
    }

    // extend specifics
    if (requestedLockPeriodInBlocks != 0)
    {
        if (requestedLockPeriodInBlocks > MaximumWitnessLockLength())
        {
            throw witness_error(witness::RPC_INVALID_PARAMETER, "Maximum lock period of 3 years exceeded.");
        }

        if (requestedLockPeriodInBlocks < MinimumWitnessLockLength())
        {
            throw witness_error(witness::RPC_INVALID_PARAMETER, "Minimum lock period of 1 month exceeded.");
        }

        // Add a small buffer to give us time to enter the blockchain
        if (requestedLockPeriodInBlocks == MinimumWitnessLockLength())
        {
            requestedLockPeriodInBlocks += 50;
        }

        if (requestedLockPeriodInBlocks < remainingLockDurationInBlocks)
        {
            throw witness_error(witness::RPC_INVALID_PARAMETER, strprintf("New lock period [%d] does not exceed remaining lock period [%d]", requestedLockPeriodInBlocks, remainingLockDurationInBlocks));
        }

        // Enforce minimum weight for each amount
        auto tooSmallTest = [&] (const auto& amount){ return GetPoW2RawWeightForAmount(amount, chainActive.Height(), requestedLockPeriodInBlocks) < gMinimumWitnessWeight; };
        bool witnessAmountTooSmall = std::any_of(redistributionAmounts.begin(), redistributionAmounts.end(), tooSmallTest);
        if (witnessAmountTooSmall)
        {
            throw witness_error(witness::RPC_TYPE_ERROR, strprintf("Witness amount must be %d or larger", gMinimumWitnessAmount));
        }

        // Enforce new combined weight > old combined weight
        const CGetWitnessInfo witnessInfo = GetWitnessInfoWrapper();
        uint64_t dummyLockFrom, dummyLockUntil;
        uint64_t oldLockPeriod = GetPoW2LockLengthInBlocksFromOutput(currentWitnessTxOut, currentWitnessHeight, dummyLockFrom, dummyLockUntil);
        std::vector<CAmount> oldAmounts;
        std::transform(unspentWitnessOutputs.begin(), unspentWitnessOutputs.end(), std::back_inserter(oldAmounts), [](const auto& it) { return std::get<0>(it).nValue; });
        uint64_t oldCombinedWeight = combinedWeight(oldAmounts, currentWitnessHeight, oldLockPeriod);
        uint64_t newCombinedWeight = combinedWeight(redistributionAmounts, currentWitnessHeight, requestedLockPeriodInBlocks);
        if (oldCombinedWeight >= newCombinedWeight)
        {
            throw witness_error(witness::RPC_TYPE_ERROR, strprintf("New combined witness weight (%" PRIu64 ") must exceed old (%" PRIu64 ")", newCombinedWeight, oldCombinedWeight));
        }
    }

    // Check for immaturity
    for ( const auto& [currentWitnessTxOut, currentWitnessHeight, currentWitnessTxIndex, currentWitnessOutpoint] : unspentWitnessOutputs )
    {
        (unused) currentWitnessTxOut;
        (unused) currentWitnessOutpoint;
        (unused) currentWitnessTxIndex;
        //fixme: (PHASE5) - Minor code cleanup.
        //This check should go through the actual chain maturity stuff (via wtx) and not calculate maturity directly.
        if (chainActive.Tip()->nHeight - currentWitnessHeight < (uint64_t)(COINBASE_MATURITY))
        {
            throw witness_error(witness::RPC_MISC_ERROR, "Cannot perform operation on immature transaction, please wait for transaction to mature and try again");
        }
    }

    // Check type (can't extend script type, must witness once or renew/upgrade after phase 4 activated)
    if (std::any_of(unspentWitnessOutputs.begin(), unspentWitnessOutputs.end(), [](const auto& it){ return std::get<0>(it).GetType() !=  CTxOutType::PoW2WitnessOutput; }))
    {
        throw witness_error(witness::RPC_TYPE_ERROR, "Witness has to be type POW2WITNESS, renew or upgrade account");
    }

    // Check that total of new value is at least old total
    CAmount redistributionTotal = std::accumulate(redistributionAmounts.begin(), redistributionAmounts.end(), CAmount(0), [](const CAmount acc, const CAmount amount){ return acc + amount; });
    CAmount oldTotal = std::accumulate(unspentWitnessOutputs.begin(), unspentWitnessOutputs.end(), CAmount(0), [](const CAmount acc, const auto& it){
        const CTxOut& txOut = std::get<0>(it);
        return acc + txOut.nValue;
    });
    if (redistributionTotal < oldTotal)
    {
        throw witness_error(witness::RPC_INVALID_PARAMETER, strprintf("New amount [%s] is smaller than current amount [%s]", FormatMoney(redistributionTotal), FormatMoney(oldTotal)));
    }


    // Create the redistribution transaction
    CReserveKeyOrScript reservekey(pwallet, fundingAccount, KEYCHAIN_CHANGE);
    std::string reasonForFail;
    CAmount transactionFee;
    CMutableTransaction witnessTransaction(CTransaction::SEGSIG_ACTIVATION_VERSION);
    {

        uint64_t highestActionNonce = 0;
        uint64_t highestFailCount = 0;

        // Add all original outputs as inputs
        for (const auto& [currentWitnessTxOut, currentWitnessHeight, currentWitnessTxIndex, currentWitnessOutpoint]: unspentWitnessOutputs)
        {
            pwallet->AddTxInput(witnessTransaction, CInputCoin(currentWitnessOutpoint, currentWitnessTxOut, true, false, currentWitnessHeight, currentWitnessTxIndex), false);

            CTxOutPoW2Witness details;
            if (!GetPow2WitnessOutput(currentWitnessTxOut, details))
            {
                throw witness_error(witness::RPC_MISC_ERROR, "Failure extracting witness details.");
            }
            if (details.actionNonce > highestActionNonce)
                highestActionNonce = details.actionNonce;
            if (details.failCount > highestFailCount)
                highestFailCount = details.failCount;
        }

        // Add new witness outputs
        for (const CAmount& distAmount : redistributionAmounts)
        {
            CTxOut distTxOutput;
            distTxOutput.SetType(CTxOutType::PoW2WitnessOutput);
             if (requestedLockPeriodInBlocks != 0)
             {
                 // extend
                 distTxOutput.output.witnessDetails.lockFromBlock = 0;
                 distTxOutput.output.witnessDetails.lockUntilBlock = chainActive.Tip()->nHeight + requestedLockPeriodInBlocks;
             }
             else
             {
                 // rearrange
                 distTxOutput.output.witnessDetails.lockFromBlock = currentWitnessDetails.lockFromBlock;
                 distTxOutput.output.witnessDetails.lockUntilBlock = currentWitnessDetails.lockUntilBlock;
             }
            distTxOutput.output.witnessDetails.spendingKeyID = currentWitnessDetails.spendingKeyID;
            distTxOutput.output.witnessDetails.witnessKeyID = currentWitnessDetails.witnessKeyID;
            distTxOutput.output.witnessDetails.failCount = highestFailCount;
            distTxOutput.output.witnessDetails.actionNonce = highestActionNonce + 1;
            distTxOutput.nValue = distAmount;
            witnessTransaction.vout.push_back(distTxOutput);
        }

        // Fund the additional amount in the transaction (including fees)
        int changeOutputPosition = 1;
        std::set<int> subtractFeeFromOutputs; // Empty - we don't subtract fee from outputs
        CCoinControl coincontrol;
        if (!pwallet->FundTransaction(fundingAccount, witnessTransaction, transactionFee, changeOutputPosition, reasonForFail, false, subtractFeeFromOutputs, coincontrol, reservekey))
        {
            throw witness_error(witness::RPC_MISC_ERROR, strprintf("Failed to fund transaction [%s]", reasonForFail.c_str()));
        }
    }

    uint256 finalTransactionHash;
    {
        LOCK2(cs_main, pwallet->cs_wallet);
        if (!pwallet->SignAndSubmitTransaction(reservekey, witnessTransaction, reasonForFail, &finalTransactionHash))
        {
            throw witness_error(witness::RPC_MISC_ERROR, strprintf("Failed to commit transaction [%s]", reasonForFail.c_str()));
        }
    }

    // Set result parameters
    if (pTxid != nullptr)
        *pTxid = finalTransactionHash.GetHex();
    if (pFee != nullptr)
        *pFee = transactionFee;
}

void redistributewitnessaccount(CWallet* pwallet, CAccount* fundingAccount, CAccount* witnessAccount, const std::vector<CAmount>& redistributionAmounts, std::string* pTxid, CAmount* pFee)
{
    redistributeandextendwitnessaccount(pwallet, fundingAccount, witnessAccount, redistributionAmounts, 0, pTxid, pFee);
}

void extendwitnessaccount(CWallet* pwallet, CAccount* fundingAccount, CAccount* witnessAccount, CAmount amount, uint64_t requestedLockPeriodInBlocks, std::string* pTxid, CAmount* pFee)
{
    CGetWitnessInfo witnessInfo = GetWitnessInfoWrapper();
    auto distribution = optimalWitnessDistribution(amount, requestedLockPeriodInBlocks, witnessInfo.nTotalWeightEligibleRaw);
    redistributeandextendwitnessaccount(pwallet, fundingAccount, witnessAccount, distribution, requestedLockPeriodInBlocks, pTxid, pFee);
}

uint64_t adjustedWeightForAmount(const CAmount amount, const uint64_t nHeight, const uint64_t duration, uint64_t networkWeight)
{
    uint64_t maxWeight = networkWeight / 100;
    uint64_t rawWeight = GetPoW2RawWeightForAmount(amount, nHeight, duration);
    uint64_t weight = std::min(rawWeight, maxWeight);
    return weight;
}

/** Estimated witnessing count expressed as a fraction per block for a single amount */
double witnessFraction(const CAmount amount, const uint64_t nHeight, const uint64_t duration, const uint64_t totalWeight)
{
    uint64_t weight = adjustedWeightForAmount(amount, nHeight, duration, totalWeight);

    // election probability
    const double p = double(weight) / totalWeight;

    // adjust for cooldown
    double fraction = 1.0 / (gMinimumParticipationAge + 1.0/p);

    return fraction;
}

/** Estimated witnessing count expressed as a fraction per block for an amount distribution */
double witnessFraction(const std::vector<CAmount>& amounts, const uint64_t nHeight, const uint64_t duration, const uint64_t totalWeight)
{
    double fraction = std::accumulate(amounts.begin(), amounts.end(), 0.0, [=](double acc, CAmount amount) {
        return acc + witnessFraction(amount, nHeight, duration, totalWeight);
    });
    return fraction;
}

std::tuple<std::vector<CAmount>, uint64_t, CAmount> witnessDistribution(CWallet* pWallet, CAccount* account)
{
    // assumes all account witness outputs have identical characteristics

    LOCK2(cs_main, pWallet->cs_wallet);

    const auto unspentWitnessOutputs = getCurrentOutputsForWitnessAccount(account);

    std::vector<CAmount> distribution;
    std::transform(unspentWitnessOutputs.begin(), unspentWitnessOutputs.end(), std::back_inserter(distribution), [](const auto& it) {
        const CTxOut& txOut = std::get<0>(it);
        return txOut.nValue;
    });

    CAmount total = std::accumulate(unspentWitnessOutputs.begin(), unspentWitnessOutputs.end(), CAmount(0), [](const CAmount acc, const auto& it){
        const CTxOut& txOut = std::get<0>(it);
        return acc + txOut.nValue;
    });

    int64_t duration = 0;
    if (unspentWitnessOutputs.size() > 0) {
        const CTxOut& txOut = std::get<0>(unspentWitnessOutputs[0]);
        uint64_t nUnused1, nUnused2;
        duration = GetPoW2LockLengthInBlocksFromOutput(txOut, std::get<1>(unspentWitnessOutputs[0]), nUnused1, nUnused2);
    }

    return std::tuple(distribution, duration, total);
}

CAmount maxAmountForDurationAndWeight(uint64_t lockDuration, uint64_t nHeight, uint64_t targetWeight)
{
    (unused) nHeight;
    // note that yearly_blocks is only correct for mainnet and should be parametrized for testnet, however it is consistent with
    // with other parts of the code that also use mainnet characteristics for weight determination when on testnet
    const double yearly_blocks = 365 * gRefactorDailyBlocksUsage;
    const double K = 100000.0;
    const double T = (1.0 + lockDuration / (yearly_blocks));

    // solve quadratic to find amount that will match weight
    const double A = T / K;
    const double B = T;
    const double C = - (double)(targetWeight);

    double amount = (-B + sqrt(B*B - 4.0 * A * C)) / (2.0 * A);

    return CAmount (amount * COIN);
}

std::vector<CAmount> optimalWitnessDistribution(CAmount totalAmount, uint64_t duration, uint64_t totalWeight)
{
    std::vector<CAmount> distribution;

    // Amount where maximum weight is reached and so any extra on top of that will not produce any gains
    CAmount partMax = maxAmountForDurationAndWeight(duration, chainActive.Height(), totalWeight/100);
    // Amount we have to be above to be a valid part of chain
    CAmount partMin = maxAmountForDurationAndWeight(duration, chainActive.Height(), gMinimumWitnessWeight)+1;
    if (partMin < gMinimumWitnessAmount*COIN)
        partMin = gMinimumWitnessAmount*COIN+1;

    // Divide int parts into 96% of maximum workable amount.
    // Leaves some room for:
    // a) leaves some room for entwork weight fluctuatiopns
    // b) leaves some room when total network witness weight changes
    CAmount partTarget = (96 * partMax) / 100;
    
    // Don't divide at all if parts are smaller than this
    CAmount partTargetMin = (60 * partMax) / 100;

    // ensure minimum criterium is met (on mainnet this is not expected to happen)
    if (partTarget < partMin)
        partTarget = partMin;

    int wholeParts = totalAmount / partTarget;
    
    if (wholeParts > 1)
    {
        CAmount remainder = totalAmount - wholeParts * partTarget;
        CAmount partRemainder = remainder/wholeParts;

        for (int i=0; i< wholeParts; i++)
        {
            distribution.push_back(partTarget+partRemainder);
            remainder -= partRemainder;
        }

        // add any final remainder to first part
        distribution[0] += remainder;
    }
    else if (wholeParts == 1 && (totalAmount > partTarget) && (totalAmount / 2 > partTargetMin))
    {
        CAmount remainder = totalAmount - (totalAmount/2*2);
        distribution.push_back((totalAmount/2)+remainder);
        distribution.push_back(totalAmount/2);
    }
    else
    {
        distribution.push_back(totalAmount);
    }
    return distribution;
}

bool isWitnessDistributionNearOptimal(CWallet* pWallet, CAccount* account, const CGetWitnessInfo& witnessInfo)
{
    // NB! We prefer the raw weight to the eligible weight as the raw weight fluctuates less
    // And over time the two should become close anyway
    uint64_t totalWeight = witnessInfo.nTotalWeightEligibleRaw;

    auto [currentDistribution, duration, totalAmount] = witnessDistribution(pWallet, account);
    double currentFraction = witnessFraction(currentDistribution, chainActive.Height(), duration, totalWeight);

    auto optimalDistribution = optimalWitnessDistribution(totalAmount, duration, totalWeight);
    double optimalFraction = witnessFraction(optimalDistribution, chainActive.Height(), duration, totalWeight);

    const double OPTIMAL_DISTRIBUTION_THRESHOLD = 0.95;
    bool nearOptimal = currentFraction / optimalFraction >= OPTIMAL_DISTRIBUTION_THRESHOLD;
    return nearOptimal;
}

uint64_t combinedWeight(const std::vector<CAmount> amounts, uint64_t height, uint64_t duration)
{
    return std::accumulate(amounts.begin(), amounts.end(), uint64_t(0), [=](uint64_t acc, CAmount amount)
    {
        return acc + GetPoW2RawWeightForAmount(amount, height, duration);
    });
}

std::string witnessAddressForAccount(CWallet* pWallet, CAccount* account)
{
    LOCK2(cs_main, pWallet->cs_wallet);

    if (chainActive.Tip())
    {
        std::map<COutPoint, Coin> allWitnessCoins;
        if (getAllUnspentWitnessCoins(chainActive, Params(), chainActive.Tip(), allWitnessCoins))
        {
            for (const auto& [witnessOutPoint, witnessCoin] : allWitnessCoins)
            {
                (unused)witnessOutPoint;
                CTxOutPoW2Witness witnessDetails;
                GetPow2WitnessOutput(witnessCoin.out, witnessDetails);
                if (account->HaveKey(witnessDetails.witnessKeyID))
                {
                    return CNativeAddress(CPoW2WitnessDestination(witnessDetails.spendingKeyID, witnessDetails.witnessKeyID)).ToString();
                }
            }
        }
    }

    return "";
}

std::string witnessKeysLinkUrlForAccount(CWallet* pWallet, CAccount* account)
{
    std::set<CKeyID> keys;

    LOCK2(cs_main, pWallet->cs_wallet);

    if (chainActive.Tip())
    {
        std::map<COutPoint, Coin> allWitnessCoins;
        if (getAllUnspentWitnessCoins(chainActive, Params(), chainActive.Tip(), allWitnessCoins))
        {
            for (const auto& [witnessOutPoint, witnessCoin] : allWitnessCoins)
            {
                (unused)witnessOutPoint;
                CTxOutPoW2Witness witnessDetails;
                GetPow2WitnessOutput(witnessCoin.out, witnessDetails);
                if (account->HaveKey(witnessDetails.witnessKeyID))
                {
                    keys.insert(witnessDetails.witnessKeyID);
                }
            }
        }
    }

    std::string witnessAccountKeys = "";
    for (const CKeyID& key: keys) {
        //fixme: (FUT) - to be 100% correct we should export the creation time of the actual key (where available) and not getEarliestPossibleCreationTime - however getEarliestPossibleCreationTime will do for now.
        CKey witnessPrivKey;
        if (account->GetKey(key, witnessPrivKey))
        witnessAccountKeys += CEncodedSecretKey(witnessPrivKey).ToString() + strprintf("#%s", account->getEarliestPossibleCreationTime());
        witnessAccountKeys += ":";
    }

    if (!witnessAccountKeys.empty())
    {
        witnessAccountKeys.pop_back();
        witnessAccountKeys = GLOBAL_APP_URIPREFIX"://witnesskeys?keys=" + witnessAccountKeys;
    }

    return witnessAccountKeys;
}
