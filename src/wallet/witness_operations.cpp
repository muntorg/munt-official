// Copyright (c) 2016-2019 The Gulden developers
// Authored by: Willem de Jonge (willem@isnapp.nl)
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
#include "Gulden/util.h"
#include "coincontrol.h"
#include "net.h"
#include "alert.h"
#include "utilmoneystr.h"

static std::vector<std::tuple<CTxOut, uint64_t, COutPoint>> getCurrentOutputsForWitnessAccount(CAccount* forAccount)
{
    std::map<COutPoint, Coin> allWitnessCoins;
    if (!getAllUnspentWitnessCoins(chainActive, Params(), chainActive.Tip(), allWitnessCoins))
        throw std::runtime_error("Failed to enumerate all witness coins.");

    std::vector<std::tuple<CTxOut, uint64_t, COutPoint>> matchedOutputs;
    for (const auto& [outpoint, coin] : allWitnessCoins)
    {
        if (IsMine(*forAccount, coin.out))
        {
            matchedOutputs.push_back(std::tuple(coin.out, coin.nHeight, outpoint));
        }
    }
    return matchedOutputs;
}

void extendwitnessaddresshelper(CAccount* fundingAccount, std::vector<std::tuple<CTxOut, uint64_t, COutPoint>> unspentWitnessOutputs, CWallet* pwallet, CAmount requestedAmount, uint64_t requestedLockPeriodInBlocks, std::string* pTxid, CAmount* pFee)
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
    const auto& [currentWitnessTxOut, currentWitnessHeight, currentWitnessOutpoint] = unspentWitnessOutputs[0];
    //fixme: (PHASE4) - This check should go through the actual chain maturity stuff (via wtx) and not calculate directly.
    if (chainActive.Tip()->nHeight - currentWitnessHeight < (uint64_t)(COINBASE_MATURITY_PHASE4))
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
    int64_t newWeight = GetPoW2RawWeightForAmount(requestedAmount, requestedLockPeriodInBlocks);
    if (newWeight < gMinimumWitnessWeight)
    {
        throw witness_error(witness::RPC_INVALID_PARAMETER, "PoW² witness has insufficient weight.");
    }

    // Enforce new weight > old weight
    uint64_t notUsed1, notUsed2;
    int64_t oldWeight = GetPoW2RawWeightForAmount(currentWitnessTxOut.nValue, GetPoW2LockLengthInBlocksFromOutput(currentWitnessTxOut, currentWitnessHeight, notUsed1, notUsed2));
    if (newWeight <= oldWeight)
    {
        throw witness_error(witness::RPC_INVALID_PARAMETER, strprintf("New weight [%d] does not exceed old weight [%d].", newWeight, oldWeight));
    }

    // Find the account for this address
    CAccount* witnessAccount = pwallet->FindAccountForTransaction(currentWitnessTxOut);
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
        pwallet->AddTxInput(extendWitnessTransaction, CInputCoin(currentWitnessOutpoint, currentWitnessTxOut), false);

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

    uint256 finalTransactionHash;
    {
        LOCK2(cs_main, pwallet->cs_wallet);
        if (!pwallet->SignAndSubmitTransaction(reservekey, extendWitnessTransaction, reasonForFail, &finalTransactionHash))
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

void fundwitnessaccount(CWallet* pwallet, CAccount* fundingAccount, CAccount* witnessAccount, CAmount amount, uint64_t requestedPeriodInBlocks, bool fAllowMultiple, std::string* pAddress, std::string* pTxid)
{
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

    if (amount < (gMinimumWitnessAmount*COIN))
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

    // Enforce minimum weight
    int64_t nWeight = GetPoW2RawWeightForAmount(amount, requestedPeriodInBlocks);
    if (nWeight < gMinimumWitnessWeight)
        throw witness_error(witness::RPC_INVALID_PARAMETER, "PoW² witness has insufficient weight.");

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

    CAmount nFeeRequired;
    std::string strError;
    std::vector<CRecipient> vecSend;
    int nChangePosRet = -1;

    CRecipient recipient = ( IsSegSigEnabled(chainActive.TipPrev()) ? ( CRecipient(GetPoW2WitnessOutputFromWitnessDestination(destinationPoW2Witness), amount, false) ) : ( CRecipient(GetScriptForDestination(destinationPoW2Witness), amount, false) ) ) ;
    if (!IsSegSigEnabled(chainActive.TipPrev()))
    {
        // We have to copy this anyway even though we are using a CSCript as later code depends on it to grab the witness key id.
        recipient.witnessDetails.witnessKeyID = destinationPoW2Witness.witnessKey;
    }

    //NB! Setting this is -super- important, if we don't then encrypted wallets may fail to witness.
    recipient.witnessForAccount = witnessAccount;
    vecSend.push_back(recipient);

    CWalletTx wtx;
    CReserveKeyOrScript reservekey(pwallet, fundingAccount, KEYCHAIN_CHANGE);
    if (!pwallet->CreateTransaction(fundingAccount, vecSend, wtx, reservekey, nFeeRequired, nChangePosRet, strError))
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
    if (pAddress != nullptr) {
        CTxDestination dest;
        ExtractDestination(wtx.tx->vout[0], dest);
        *pAddress = CGuldenAddress(dest).ToString();
    }
    if (pTxid != nullptr)
        *pTxid = wtx.GetHash().GetHex();
}

void upgradewitnessaccount(CWallet* pwallet, CAccount* fundingAccount, CAccount* witnessAccount, std::string* pTxid, CAmount* pFee)
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
        throw witness_error(witness::RPC_MISC_ERROR, "Cannot split a witness-only account as spend key is required to do this.");
    }

    std::string strError;
    CMutableTransaction tx(CURRENT_TX_VERSION_POW2);
    CReserveKeyOrScript changeReserveKey(pactiveWallet, fundingAccount, KEYCHAIN_EXTERNAL);
    CAmount transactionFee;
    pwallet->PrepareUpgradeWitnessAccountTransaction(fundingAccount, witnessAccount, changeReserveKey, tx, transactionFee);

    uint256 finalTransactionHash;
    if (!pwallet->SignAndSubmitTransaction(changeReserveKey, tx, strError, &finalTransactionHash))
    {
        throw std::runtime_error(strprintf("Failed to sign transaction [%s]", strError.c_str()));
    }

    // Set result parameters
    if (pTxid != nullptr)
        *pTxid = finalTransactionHash.GetHex();
    if (pFee != nullptr)
        *pFee = transactionFee;
}

void rotatewitnessaddresshelper(CAccount* fundingAccount, std::vector<std::tuple<CTxOut, uint64_t, COutPoint>> unspentWitnessOutputs, CWallet* pwallet, std::string* pTxid, CAmount* pFee)
{
    if (unspentWitnessOutputs.size() == 0)
        throw witness_error(witness::RPC_INVALID_ADDRESS_OR_KEY, strprintf("Too few witness outputs cannot rotate."));

    // Check for immaturity
    for ( const auto& [currentWitnessTxOut, currentWitnessHeight, currentWitnessOutpoint] : unspentWitnessOutputs )
    {
        (unused) currentWitnessTxOut;
        (unused) currentWitnessOutpoint;
        //fixme: (PHASE4) - This check should go through the actual chain maturity stuff (via wtx) and not calculate directly.
        //fixme: (PHASE4) - Look into shortening the maturity period here, the full period is too long.
        if (chainActive.Tip()->nHeight - currentWitnessHeight < (uint64_t)(COINBASE_MATURITY_PHASE4))
            throw witness_error(witness::RPC_MISC_ERROR, "Cannot perform operation on immature transaction, please wait for transaction to mature and try again");
    }


    // Find the account for this address
    CAccount* witnessAccount = nullptr;
    {
        CTxOutPoW2Witness currentWitnessDetails;
        const auto& currentWitnessTxOut = std::get<0>(unspentWitnessOutputs[0]);
        GetPow2WitnessOutput(currentWitnessTxOut, currentWitnessDetails);
        witnessAccount = pwallet->FindAccountForTransaction(currentWitnessTxOut);
    }
    if (!witnessAccount)
        throw witness_error(witness::RPC_MISC_ERROR, "Could not locate account");
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
            throw witness_error(witness::RPC_INVALID_ADDRESS_OR_KEY, "Error allocating witness key for witness account.");

        //We delibritely return the key here, so that if we fail we won't leak the key.
        //The key will be marked as used when the transaction is accepted anyway.
        keyWitness.ReturnKey();
    }

    // Add all existing outputs as inputs and new outputs
    for (const auto&it: unspentWitnessOutputs)
    {
        // Add input
        const CTxOut& txOut = std::get<0>(it);
        const COutPoint outPoint = std::get<2>(it);
        pwallet->AddTxInput(rotateWitnessTransaction, CInputCoin(outPoint, txOut), false);

        // Get witness details
        CTxOutPoW2Witness currentWitnessDetails;
        if (!GetPow2WitnessOutput(txOut, currentWitnessDetails))
            throw witness_error(witness::RPC_MISC_ERROR, "Failure extracting witness details.");

        // Add rotated output
        CTxOut rotatedWitnessTxOutput;
        rotatedWitnessTxOutput.SetType(CTxOutType::PoW2WitnessOutput);
        rotatedWitnessTxOutput.output.witnessDetails.lockFromBlock = currentWitnessDetails.lockFromBlock;
        rotatedWitnessTxOutput.output.witnessDetails.lockUntilBlock = currentWitnessDetails.lockUntilBlock;
        rotatedWitnessTxOutput.output.witnessDetails.spendingKeyID = currentWitnessDetails.spendingKeyID;
        rotatedWitnessTxOutput.output.witnessDetails.witnessKeyID = pubWitnessKey.GetID();
        rotatedWitnessTxOutput.output.witnessDetails.failCount = currentWitnessDetails.failCount;
        rotatedWitnessTxOutput.output.witnessDetails.actionNonce = currentWitnessDetails.actionNonce+1;
        rotatedWitnessTxOutput.nValue = txOut.nValue;
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


    //fixme: (PHASE4) Improve this, it should only happen if Sign transaction is a success.
    //Also (low) this shares common code with CreateTransaction() - it should be factored out into a common helper.
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

    uint256 finalTransactionHash;
    {
        LOCK2(cs_main, pactiveWallet->cs_wallet);
        if (!pwallet->SignAndSubmitTransaction(reservekey, rotateWitnessTransaction, reasonForFail, &finalTransactionHash))
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

void EnsureMatchingWitnessCharacteristics(const std::vector<std::tuple<CTxOut, uint64_t, COutPoint>>& unspentWitnessOutputs)
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
        for (unsigned int i = 1; i < unspentWitnessOutputs.size(); i++) {
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

CWitnessAccountStatus GetWitnessAccountStatus(CWallet* pWallet, CAccount* account, const CGetWitnessInfo& witnessInfo)
{
    WitnessStatus status;

    LOCK2(cs_main, pWallet->cs_wallet);

    // Collect uspent witnesses coins on for the account
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

    bool hasBalance = pactiveWallet->GetBalance(account, true, true, true) +
                          pactiveWallet->GetImmatureBalance(account, true, true) +
                          pactiveWallet->GetUnconfirmedBalance(account, true, true) > 0;

    bool isLocked = haveUnspentWitnessUtxo && IsPoW2WitnessLocked(witnessDetails0, chainActive.Tip()->nHeight);

    uint64_t networkWeight = witnessInfo.nTotalWeightRaw;
    bool isExpired = haveUnspentWitnessUtxo && std::any_of(accountItems.begin(), accountItems.end(), [=](const RouletteItem& ri){
                         return witnessHasExpired(ri.nAge, ri.nWeight, networkWeight);
                     });

    if (!haveUnspentWitnessUtxo && hasBalance) status = WitnessStatus::Pending;
    else if (!haveUnspentWitnessUtxo && !hasBalance) status = WitnessStatus::Empty;
    else if (haveUnspentWitnessUtxo && hasBalance && isLocked && isExpired) status = WitnessStatus::Expired;
    else if (haveUnspentWitnessUtxo && hasBalance && isLocked && !isExpired) status = WitnessStatus::Witnessing;
    else if (haveUnspentWitnessUtxo && hasBalance && isLocked && isExpired) status = WitnessStatus::Expired;
    else if (haveUnspentWitnessUtxo && hasBalance && !isLocked) status = WitnessStatus::Ended;
    else if (haveUnspentWitnessUtxo && !hasBalance && !isLocked) status = WitnessStatus::Emptying;
    else throw std::runtime_error("Unable to determine witness state.");

    // NOTE: assuming any unconfirmed tx here is a witness one to avoid getting the witness bundles and testing those, this will almost always be
    // correct. Any edge cases where this fails will automatically resolve once the tx confirms.
    bool hasUnconfirmedWittnessTx = std::any_of(pWallet->mapWallet.begin(), pWallet->mapWallet.end(), [=](const auto& it){
        const auto& wtx = it.second;
        return account->HaveWalletTx(wtx) && wtx.InMempool();
    });

    // hasUnconfirmedWittnessTx -> renewed, extended ...
    if (status == WitnessStatus::Expired && hasUnconfirmedWittnessTx)
        status = WitnessStatus::Witnessing;

    bool hasScriptLegacyOutput = std::any_of(accountItems.begin(), accountItems.end(), [](const RouletteItem& ri){ return ri.coin.out.GetType() == CTxOutType::ScriptLegacyOutput; });

    std::vector<uint64_t> parts;
    std::transform(accountItems.begin(), accountItems.end(), std::back_inserter(parts), [](const auto& ri) { return ri.nWeight; });

    CWitnessAccountStatus result {
        account,
        status,
        witnessInfo.nTotalWeightRaw,
        isLocked ? std::accumulate(accountItems.begin(), accountItems.end(), uint64_t(0), [](const uint64_t acc, const RouletteItem& ri){ return acc + ri.nWeight; }) : uint64_t(0),
        hasScriptLegacyOutput,
        hasUnconfirmedWittnessTx,
        nLockFromBlock,
        nLockUntilBlock,
        nLockPeriodInBlocks,
        parts
    };

    return result;
}

void redistributeandextendwitnessaccount(CWallet* pwallet, CAccount* fundingAccount, CAccount* witnessAccount, const std::vector<CAmount>& redistributionAmounts, uint64_t requestedLockPeriodInBlocks, std::string* pTxid, CAmount* pFee)
{
    if (pwallet == nullptr || witnessAccount == nullptr || fundingAccount == nullptr)
        throw witness_error(witness::RPC_INVALID_PARAMETER, "Require non-null pwallet, fundingAccount, witnessAccount");

    LOCK2(cs_main, pwallet->cs_wallet);

    if (!IsSegSigEnabled(chainActive.TipPrev()))
        throw std::runtime_error("Segsig not activated");

    if (pwallet->IsLocked()) {
        throw std::runtime_error("Wallet locked");
    }

    if ((!witnessAccount->IsPoW2Witness()) || witnessAccount->IsFixedKeyPool())
    {
        throw witness_error(witness::RPC_MISC_ERROR, "Cannot operate on witness-only account as spend key is required to do this.");
    }

    const auto& unspentWitnessOutputs = getCurrentOutputsForWitnessAccount(witnessAccount);
    if (unspentWitnessOutputs.size() == 0)
        throw witness_error(witness::RPC_INVALID_ADDRESS_OR_KEY, strprintf("Account does not contain any witness outputs [%s].",
                                                                           boost::uuids::to_string(witnessAccount->getUUID())));

    EnsureMatchingWitnessCharacteristics(unspentWitnessOutputs);

    const auto& [currentWitnessTxOut, currentWitnessHeight, currentWitnessOutpoint] = unspentWitnessOutputs[0];
    (unused)currentWitnessOutpoint;
    (unused)currentWitnessHeight;

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
    if (requestedLockPeriodInBlocks != 0) {
        if (requestedLockPeriodInBlocks > MaximumWitnessLockLength())
            throw witness_error(witness::RPC_INVALID_PARAMETER, "Maximum lock period of 3 years exceeded.");

        if (requestedLockPeriodInBlocks < MinimumWitnessLockLength())
            throw witness_error(witness::RPC_INVALID_PARAMETER, "Minimum lock period of 1 month exceeded.");

        // Add a small buffer to give us time to enter the blockchain
        if (requestedLockPeriodInBlocks == MinimumWitnessLockLength())
            requestedLockPeriodInBlocks += 50;

        if (requestedLockPeriodInBlocks < remainingLockDurationInBlocks)
        {
            throw witness_error(witness::RPC_INVALID_PARAMETER, strprintf("New lock period [%d] does not exceed remaining lock period [%d]", requestedLockPeriodInBlocks, remainingLockDurationInBlocks));
        }

        // Enforce minimum weight for each amount
        if (std::any_of(redistributionAmounts.begin(), redistributionAmounts.end(), [&](const auto& amount){
                return GetPoW2RawWeightForAmount(amount, requestedLockPeriodInBlocks) < gMinimumWitnessWeight; }))
            throw witness_error(witness::RPC_TYPE_ERROR, strprintf("Witness amount must be %d or larger", gMinimumWitnessAmount));

        // Enforce new combined weight > old combined weight
        const CGetWitnessInfo witnessInfo = GetWitnessInfoWrapper();
        uint64_t networkWeight = witnessInfo.nTotalWeightEligibleAdjusted;
        uint64_t dummyLockFrom, dummyLockUntil;
        uint64_t oldLockPeriod = GetPoW2LockLengthInBlocksFromOutput(currentWitnessTxOut, currentWitnessHeight, dummyLockFrom, dummyLockUntil);
        std::vector<CAmount> oldAmounts;
        std::transform(unspentWitnessOutputs.begin(), unspentWitnessOutputs.end(), std::back_inserter(oldAmounts), [](const auto& it) { return std::get<0>(it).nValue; });
        uint64_t oldCombinedWeight = combinedWeight(oldAmounts, oldLockPeriod, networkWeight);
        uint64_t newCombinedWeight = combinedWeight(redistributionAmounts, requestedLockPeriodInBlocks, networkWeight);
        if (oldCombinedWeight >= newCombinedWeight)
            throw witness_error(witness::RPC_TYPE_ERROR, strprintf("New combined witness weight (%" PRIu64 ") must exceed old (%" PRIu64 ")", newCombinedWeight, oldCombinedWeight));
    }

    // Check for immaturity
    for ( const auto& [currentWitnessTxOut, currentWitnessHeight, currentWitnessOutpoint] : unspentWitnessOutputs )
    {
        (unused) currentWitnessTxOut;
        (unused) currentWitnessOutpoint;
        //fixme: (PHASE4) - This check should go through the actual chain maturity stuff (via wtx) and not calculate directly.
        //fixme: (PHASE4) - Look into shortening the maturity period here, the full period is too long.
        if (chainActive.Tip()->nHeight - currentWitnessHeight < (uint64_t)(COINBASE_MATURITY_PHASE4))
            throw witness_error(witness::RPC_MISC_ERROR, "Cannot perform operation on immature transaction, please wait for transaction to mature and try again");
    }

    // Check type (can't extend script type, must witness once or renew/upgrade after phase 4 activated)
    if (std::any_of(unspentWitnessOutputs.begin(), unspentWitnessOutputs.end(), [](const auto& it){ return std::get<0>(it).GetType() !=  CTxOutType::PoW2WitnessOutput; }))
        throw witness_error(witness::RPC_TYPE_ERROR, "Witness has to be type POW2WITNESS, renew or upgrade account");

    // Check that total of new value is at least old total
    CAmount redistributionTotal = std::accumulate(redistributionAmounts.begin(), redistributionAmounts.end(), CAmount(0), [](const CAmount acc, const CAmount amount){ return acc + amount; });
    CAmount oldTotal = std::accumulate(unspentWitnessOutputs.begin(), unspentWitnessOutputs.end(), CAmount(0), [](const CAmount acc, const auto& it){
        const CTxOut& txOut = std::get<0>(it);
        return acc + txOut.nValue;
    });
    if (redistributionTotal < oldTotal)
        throw witness_error(witness::RPC_INVALID_PARAMETER, strprintf("New amount [%s] is smaller than current amount [%s]", FormatMoney(redistributionTotal), FormatMoney(oldTotal)));


    // Create the redistribution transaction
    CReserveKeyOrScript reservekey(pwallet, fundingAccount, KEYCHAIN_CHANGE);
    std::string reasonForFail;
    CAmount transactionFee;
    CMutableTransaction witnessTransaction(CTransaction::SEGSIG_ACTIVATION_VERSION);
    {

        uint64_t highestActionNonce = 0;
        uint64_t highestFailCount = 0;

        // Add all original outputs as inputs
        for (const auto&it: unspentWitnessOutputs)
        {
            const CTxOut& txOut = std::get<0>(it);
            const COutPoint outPoint = std::get<2>(it);
            pwallet->AddTxInput(witnessTransaction, CInputCoin(outPoint, txOut), false);

            CTxOutPoW2Witness details;
            if (!GetPow2WitnessOutput(txOut, details))
                throw witness_error(witness::RPC_MISC_ERROR, "Failure extracting witness details.");

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
             if (requestedLockPeriodInBlocks != 0) {
                 // extend
                 distTxOutput.output.witnessDetails.lockFromBlock = 0;
                 distTxOutput.output.witnessDetails.lockUntilBlock = chainActive.Tip()->nHeight + requestedLockPeriodInBlocks;
             }
             else {
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
    auto distribution = optimalWitnessDistribution(amount, requestedLockPeriodInBlocks, witnessInfo.nTotalWeightEligibleAdjusted);
    redistributeandextendwitnessaccount(pwallet, fundingAccount, witnessAccount, distribution, requestedLockPeriodInBlocks, pTxid, pFee);
}

uint64_t adjustedWeightForAmount(const CAmount amount, const uint64_t duration, uint64_t networkWeight)
{
    uint64_t maxWeight = networkWeight / 100;
    uint64_t rawWeight = GetPoW2RawWeightForAmount(amount, duration);
    uint64_t weight = std::min(rawWeight, maxWeight);
    return weight;
}

/** Estimated witnessing count expressed as a fraction per block for a single amount */
double witnessFraction(const CAmount amount, const uint64_t duration, const uint64_t totalWeight)
{
    uint64_t weight = adjustedWeightForAmount(amount, duration, totalWeight);

    // election probability
    const double p = double(weight) / totalWeight;

    // adjust for cooldown
    double fraction = 1.0 / (gMinimumParticipationAge + 1.0/p);

    return fraction;
}

/** Estimated witnessing count expressed as a fraction per block for an amount distribution */
double witnessFraction(const std::vector<CAmount>& amounts, const uint64_t duration, const uint64_t totalWeight)
{
    double fraction = std::accumulate(amounts.begin(), amounts.end(), 0.0, [=](double acc, CAmount amount) {
        return acc + witnessFraction(amount, duration, totalWeight);
    });
    return fraction;
}

/** Amount where maximum weight is reached and so any extra on top of that will not produce any gains */
CAmount maxWorkableWitnessAmount(uint64_t lockDuration, uint64_t totalWeight)
{
    uint64_t weight = totalWeight / 100;

    // note that yearly_blocks is only correct for mainnet and should be parametrized for testnet, however it is consistent with
    // with other parts of the code that also use mainnet characteristics for weight determination when on testnet
    const double yearly_blocks = 365 * 576;
    const double K = 100000.0;
    const double T = (1.0 + lockDuration / (yearly_blocks));

    // solve quadratic to find amount that will match weight
    const double A = T / K;
    const double B = T;
    const double C = - (double)(weight);

    double amount = (-B + sqrt(B*B - 4.0 * A * C)) / (2.0 * A);

    return CAmount (amount * COIN);
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

std::vector<CAmount> optimalWitnessDistribution(CAmount totalAmount, uint64_t duration, uint64_t totalWeight)
{
    std::vector<CAmount> distribution;

    CAmount minAmount = gMinimumWitnessAmount * COIN;

    CAmount partMax = maxWorkableWitnessAmount(duration, totalWeight);

    // Divide int parts into 90% of maximum workable amount. Staying (well) below the maximum workable amount
    // does not reduce the expected witness rewards but it leaves some room for:
    // a) adding remaining part if it is below minimum witness amount (5000) without going over max workable (which would hurt expected rewards)
    // b) leaves some room when total network witness weight changes
    CAmount partTarget = (90 * partMax) / 100;

    // ensure minimum criterium is met (on mainnet this is not expected to happen)
    if (partTarget < minAmount)
        partTarget = minAmount;

    int wholeParts = totalAmount / partTarget;

    for (int i=0; i< wholeParts; i++)
        distribution.push_back(partTarget);

    CAmount remainder = totalAmount - wholeParts * partTarget;

    // add remainder to last part if it is small and fits within workable amount
    // or remainder is too small (which is only expected to happen on testnet)
    if (distribution.size() > 0 && (distribution[0] + remainder <= partMax || remainder < minAmount))
        distribution[0] += remainder;
    else
        distribution.push_back(remainder);

    return distribution;
}

bool isWitnessDistributionNearOptimal(CWallet* pWallet, CAccount* account, const CGetWitnessInfo& witnessInfo)
{
    uint64_t totalWeight = witnessInfo.nTotalWeightEligibleAdjusted;

    auto [currentDistribution, duration, totalAmount] = witnessDistribution(pWallet, account);
    double currentFraction = witnessFraction(currentDistribution, duration, totalWeight);

    auto optimalDistribution = optimalWitnessDistribution(totalAmount, duration, totalWeight);
    double optimalFraction = witnessFraction(optimalDistribution, duration, totalWeight);

    const double OPTIMAL_DISTRIBUTION_THRESHOLD = 0.95;
    bool nearOptimal = currentFraction / optimalFraction >= OPTIMAL_DISTRIBUTION_THRESHOLD;
    return nearOptimal;
}

uint64_t combinedWeight(const std::vector<CAmount> amounts, uint64_t duration, uint64_t totalWeight)
{
    return std::accumulate(amounts.begin(), amounts.end(), uint64_t(0), [=](uint64_t acc, CAmount amount) {
        return acc + adjustedWeightForAmount(amount, duration, totalWeight);
    });
}
