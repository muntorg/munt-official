// Copyright (c) 2016-2019 The Gulden developers
// Authored by: Willem de Jonge (willem@isnapp.nl)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#include "witness_operations.h"

#include <stdexcept>
#include <numeric>
#include <algorithm>
#include <boost/uuid/uuid_io.hpp>
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

std::tuple<CAmount, int64_t, int64_t, bool> extendWitnessInfo(CWallet* pwallet, CAccount* witnessAccount)
{
    LOCK2(cs_main, pwallet->cs_wallet);

    const auto& unspentWitnessOutputs = getCurrentOutputsForWitnessAccount(witnessAccount);
    if (unspentWitnessOutputs.size() == 0)
        throw witness_error(witness::RPC_INVALID_ADDRESS_OR_KEY, strprintf("Account does not contain any witness outputs [%s].",
                                                                           boost::uuids::to_string(witnessAccount->getUUID())));

    // Check for immaturity
    bool immatureWitness = false;
    const auto& [currentWitnessTxOut, currentWitnessHeight, currentWitnessOutpoint] = unspentWitnessOutputs[0];
    (unused)currentWitnessOutpoint;

    //fixme: (PHASE4) - This check should go through the actual chain maturity stuff (via wtx) and not calculate directly.
    if (chainActive.Tip()->nHeight - currentWitnessHeight < (uint64_t)(COINBASE_MATURITY_PHASE4))
            immatureWitness = true;

    // Calculate existing lock period
    CTxOutPoW2Witness currentWitnessDetails;
    GetPow2WitnessOutput(currentWitnessTxOut, currentWitnessDetails);

    CAmount lockedAmount = currentWitnessTxOut.nValue;
    uint64_t remainingLockDurationInBlocks = GetPoW2RemainingLockLengthInBlocks(currentWitnessDetails.lockUntilBlock, chainActive.Tip()->nHeight);
    uint64_t notUsed1, notUsed2;
    int64_t weight = GetPoW2RawWeightForAmount(currentWitnessTxOut.nValue, GetPoW2LockLengthInBlocksFromOutput(currentWitnessTxOut, currentWitnessHeight, notUsed1, notUsed2));
    return std::tuple(lockedAmount, remainingLockDurationInBlocks, weight, immatureWitness);
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

void extendwitnessaccount(CWallet* pwallet, CAccount* fundingAccount, CAccount* witnessAccount, CAmount amount, uint64_t requestedLockPeriodInBlocks, std::string* pTxid, CAmount* pFee)
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
        throw witness_error(witness::RPC_MISC_ERROR, "Cannot extend a witness-only account as spend key is required to do this.");
    }

    const auto& unspentWitnessOutputs = getCurrentOutputsForWitnessAccount(witnessAccount);
    if (unspentWitnessOutputs.size() == 0)
        throw witness_error(witness::RPC_INVALID_ADDRESS_OR_KEY, strprintf("Account does not contain any witness outputs [%s].",
                                                                           boost::uuids::to_string(witnessAccount->getUUID())));

    extendwitnessaddresshelper(fundingAccount, unspentWitnessOutputs, pwallet, amount, requestedLockPeriodInBlocks, pTxid, pFee);
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
    if (unspentWitnessOutputs.size() > 1)
        throw witness_error(witness::RPC_INVALID_ADDRESS_OR_KEY, strprintf("Too many witness outputs cannot rotate."));

    // Check for immaturity
    const auto& [currentWitnessTxOut, currentWitnessHeight, currentWitnessOutpoint] = unspentWitnessOutputs[0];
    //fixme: (PHASE4) - This check should go through the actual chain maturity stuff (via wtx) and not calculate directly.
    //fixme: (PHASE4) - Look into shortening the maturity period here, the full period is too long.
    if (chainActive.Tip()->nHeight - currentWitnessHeight < (uint64_t)(COINBASE_MATURITY_PHASE4))
        throw witness_error(witness::RPC_MISC_ERROR, "Cannot perform operation on immature transaction, please wait for transaction to mature and try again");

    // Get the current witness details
    CTxOutPoW2Witness currentWitnessDetails;
    GetPow2WitnessOutput(currentWitnessTxOut, currentWitnessDetails);

    // Find the account for this address
    CAccount* witnessAccount = pwallet->FindAccountForTransaction(currentWitnessTxOut);
    if (!witnessAccount)
        throw witness_error(witness::RPC_MISC_ERROR, "Could not locate account for address");
    if ((!witnessAccount->IsPoW2Witness()) || witnessAccount->IsFixedKeyPool())
    {
        throw witness_error(witness::RPC_MISC_ERROR, "Cannot rotate a witness-only account as spend key is required to do this.");
    }

    //fixme: (PHASE4) factor this all out into a helper.
    // Finally attempt to create and send the witness transaction.
    CReserveKeyOrScript reservekey(pwallet, fundingAccount, KEYCHAIN_CHANGE);
    std::string reasonForFail;
    CAmount transactionFee;
    CMutableTransaction rotateWitnessTransaction(CTransaction::SEGSIG_ACTIVATION_VERSION);
    CTxOut rotatedWitnessTxOutput;
    {
        // Add the existing witness output as an input
        pwallet->AddTxInput(rotateWitnessTransaction, CInputCoin(currentWitnessOutpoint, currentWitnessTxOut), false);

        // Add new witness output
        rotatedWitnessTxOutput.SetType(CTxOutType::PoW2WitnessOutput);
        // As we are rotating the witness key we reset the "lock from" and we set the "lock until" everything else except the value remains unchanged.
        rotatedWitnessTxOutput.output.witnessDetails.lockFromBlock = currentWitnessDetails.lockFromBlock;
        rotatedWitnessTxOutput.output.witnessDetails.lockUntilBlock = currentWitnessDetails.lockUntilBlock;
        rotatedWitnessTxOutput.output.witnessDetails.spendingKeyID = currentWitnessDetails.spendingKeyID;
        {
            CReserveKeyOrScript keyWitness(pactiveWallet, witnessAccount, KEYCHAIN_WITNESS);
            CPubKey pubWitnessKey;
            if (!keyWitness.GetReservedKey(pubWitnessKey))
                throw witness_error(witness::RPC_INVALID_ADDRESS_OR_KEY, "Error allocating witness key for witness account.");

            //We delibritely return the key here, so that if we fail we won't leak the key.
            //The key will be marked as used when the transaction is accepted anyway.
            keyWitness.ReturnKey();
            rotatedWitnessTxOutput.output.witnessDetails.witnessKeyID = pubWitnessKey.GetID();
        }
        rotatedWitnessTxOutput.output.witnessDetails.failCount = currentWitnessDetails.failCount;
        rotatedWitnessTxOutput.output.witnessDetails.actionNonce = currentWitnessDetails.actionNonce+1;
        rotatedWitnessTxOutput.nValue = currentWitnessTxOut.nValue;
        rotateWitnessTransaction.vout.push_back(rotatedWitnessTxOutput);

        // Fund the additional amount in the transaction (including fees)
        int changeOutputPosition = 1;
        std::set<int> subtractFeeFromOutputs; // Empty - we don't subtract fee from outputs
        CCoinControl coincontrol;
        if (!pwallet->FundTransaction(fundingAccount, rotateWitnessTransaction, transactionFee, changeOutputPosition, reasonForFail, false, subtractFeeFromOutputs, coincontrol, reservekey))
        {
            throw witness_error(witness::RPC_MISC_ERROR, strprintf("Failed to fund transaction [%s]", reasonForFail.c_str()));
        }
    }

    //fixme: (PHASE4) Improve this, it should only happen if Sign transaction is a success.
    //Also We must make sure that the UI version of this command does the same
    //Also (low) this shares common code with CreateTransaction() - it should be factored out into a common helper.
    CKey privWitnessKey;
    if (!witnessAccount->GetKey(rotatedWitnessTxOutput.output.witnessDetails.witnessKeyID, privWitnessKey))
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
        if (!GetWitnessInfo(chainActive, Params(), nullptr, chainActive.Tip()->pprev, block, witnessInfo, chainActive.Tip()->nHeight))
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
    if (unspentWitnessOutputs.size() > 0)
    {
        CTxOutPoW2Witness witnessDetails0;
        if (!GetPow2WitnessOutput(std::get<0>(unspentWitnessOutputs[0]), witnessDetails0))
            throw std::runtime_error("Failure extracting witness details.");

        for (unsigned int i = 1; i < unspentWitnessOutputs.size(); i++) {
            CTxOutPoW2Witness witnessCompare;
            if (!GetPow2WitnessOutput(std::get<0>(unspentWitnessOutputs[i]), witnessCompare))
                throw std::runtime_error("Failure extracting witness details.");
            if (witnessCompare.lockFromBlock != witnessDetails0.lockFromBlock ||
                witnessCompare.lockUntilBlock != witnessDetails0.lockUntilBlock ||
                witnessCompare.spendingKeyID != witnessDetails0.spendingKeyID ||
                witnessCompare.witnessKeyID != witnessDetails0.witnessKeyID)
            {
                throw std::runtime_error("Multiple addresses with different witness characteristics in account. Use RPC to furter handle this account.");
            }
        }

    }
}

std::tuple<WitnessStatus, uint64_t, uint64_t, bool, bool> AccountWitnessStatus(CWallet* pWallet, CAccount* account, const CGetWitnessInfo& witnessInfo)
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

    const auto& unspentWitnessOutputs = getCurrentOutputsForWitnessAccount(account);
    EnsureMatchingWitnessCharacteristics(unspentWitnessOutputs);

    bool hasBalance = pactiveWallet->GetBalance(account, true, true, true) +
                          pactiveWallet->GetImmatureBalance(account, true, true) +
                          pactiveWallet->GetUnconfirmedBalance(account, true, true) > 0;

    bool isLocked = haveUnspentWitnessUtxo && IsPoW2WitnessLocked(witnessDetails0, chainActive.Tip()->nHeight);

    bool isExpired = haveUnspentWitnessUtxo && witnessHasExpired(accountItems[0].nAge, accountItems[0].nWeight, witnessInfo.nTotalWeightRaw);

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

    return std::tuple(status,
                      witnessInfo.nTotalWeightRaw,
                      isLocked ? std::accumulate(accountItems.begin(), accountItems.end(), 0, [](const uint64_t acc, const RouletteItem& ri){ return acc + ri.nWeight; }) : 0,
                      hasScriptLegacyOutput,
                      hasUnconfirmedWittnessTx);
}

void redistributewitnessaccount(CWallet* pwallet, CAccount* fundingAccount, CAccount* witnessAccount, const std::vector<CAmount>& redistributionAmounts, std::string* pTxid, CAmount* pFee)
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

    // Check for immaturity
    const auto& [currentWitnessTxOut, currentWitnessHeight, currentWitnessOutpoint] = unspentWitnessOutputs[0];
    //fixme: (PHASE4) - This check should go through the actual chain maturity stuff (via wtx) and not calculate directly.
    //fixme: (PHASE4) - Look into shortening the maturity period here, the full period is too long.
    if (chainActive.Tip()->nHeight - currentWitnessHeight < (uint64_t)(COINBASE_MATURITY_PHASE4))
        throw witness_error(witness::RPC_MISC_ERROR, "Cannot perform operation on immature transaction, please wait for transaction to mature and try again");

    EnsureMatchingWitnessCharacteristics(unspentWitnessOutputs);

    // Check that new distribution value matches old
    CAmount redistributionTotal = std::accumulate(redistributionAmounts.begin(), redistributionAmounts.end(), 0, [](const CAmount acc, const CAmount amount){ return acc + amount; });
    CAmount oldTotal = std::accumulate(unspentWitnessOutputs.begin(), unspentWitnessOutputs.end(), 0, [](const CAmount acc, const auto& it){
        const CTxOut& txOut = std::get<0>(it);
        return acc + txOut.nValue;
    });
    if (redistributionTotal != oldTotal)
        throw witness_error(witness::RPC_MISC_ERROR, strprintf("New distribution value [%s] doesn't match original value [%s]", FormatMoney(redistributionTotal), FormatMoney(oldTotal)));

    // Get the current witness details
    CTxOutPoW2Witness currentWitnessDetails;
    GetPow2WitnessOutput(currentWitnessTxOut, currentWitnessDetails);

    // Create the redistribution transaction
    CReserveKeyOrScript reservekey(pwallet, fundingAccount, KEYCHAIN_CHANGE);
    std::string reasonForFail;
    CAmount transactionFee;
    CMutableTransaction witnessTransaction(CTransaction::SEGSIG_ACTIVATION_VERSION);
    {
        // Add all original outputs as inputs
        for (const auto&it: unspentWitnessOutputs)
        {
            const CTxOut& txOut = std::get<0>(it);
            const COutPoint outPoint = std::get<2>(it);
            pwallet->AddTxInput(witnessTransaction, CInputCoin(outPoint, txOut), false);

        }

        // Add new witness outputs
        for (const CAmount& distAmount : redistributionAmounts)
        {
            CTxOut distTxOutput;
            distTxOutput.SetType(CTxOutType::PoW2WitnessOutput);
            // As we are splitting the amount, only the amount may change.
            distTxOutput.output.witnessDetails.lockFromBlock = currentWitnessDetails.lockFromBlock;
            distTxOutput.output.witnessDetails.lockUntilBlock = currentWitnessDetails.lockUntilBlock;
            distTxOutput.output.witnessDetails.spendingKeyID = currentWitnessDetails.spendingKeyID;
            distTxOutput.output.witnessDetails.witnessKeyID = currentWitnessDetails.witnessKeyID;
            distTxOutput.output.witnessDetails.failCount = currentWitnessDetails.failCount;
            distTxOutput.output.witnessDetails.actionNonce = currentWitnessDetails.actionNonce+1;
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
