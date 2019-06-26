// Copyright (c) 2016-2019 The Gulden developers
// Authored by: Willem de Jonge (willem@isnapp.nl)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#include "witness_operations.h"

#include <stdexcept>
#include <boost/uuid/uuid_io.hpp>
#include "wallet.h"
#include "validation/witnessvalidation.h"
#include "consensus/consensus.h"
#include "consensus/validation.h"
#include "Gulden/util.h"
#include "coincontrol.h"

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

std::pair<CAmount, int64_t> witnessAmountAndRemainingDuration(CWallet* pwallet, CAccount* witnessAccount)
{
    const auto& unspentWitnessOutputs = getCurrentOutputsForWitnessAccount(witnessAccount);
    if (unspentWitnessOutputs.size() == 0)
        throw witness_error(witness::RPC_INVALID_ADDRESS_OR_KEY, strprintf("Account does not contain any witness outputs [%s].",
                                                                           boost::uuids::to_string(witnessAccount->getUUID())));

    // Check for immaturity
    const auto& [currentWitnessTxOut, currentWitnessHeight, currentWitnessOutpoint] = unspentWitnessOutputs[0];

    //fixme: (2.1) - This check should go through the actual chain maturity stuff (via wtx) and not calculate directly.
    if (chainActive.Tip()->nHeight - currentWitnessHeight < (uint64_t)(COINBASE_MATURITY))
        throw witness_error(witness::RPC_MISC_ERROR, "Cannot perform operation on immature transaction, please wait for transaction to mature and try again");

    // Calculate existing lock period
    CTxOutPoW2Witness currentWitnessDetails;
    GetPow2WitnessOutput(currentWitnessTxOut, currentWitnessDetails);

    CAmount lockedAmount = currentWitnessTxOut.nValue;
    uint64_t remainingLockDurationInBlocks = GetPoW2RemainingLockLengthInBlocks(currentWitnessDetails.lockUntilBlock, chainActive.Tip()->nHeight);

    return std::pair(lockedAmount, remainingLockDurationInBlocks);
}

static void extendwitnessaddresshelper(CAccount* fundingAccount, std::vector<std::tuple<CTxOut, uint64_t, COutPoint>> unspentWitnessOutputs, CWallet* pwallet, CAmount requestedAmount, uint64_t requestedLockPeriodInBlocks, std::string* pTxid, CAmount* pFee)
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
    //fixme: (2.1) - This check should go through the actual chain maturity stuff (via wtx) and not calculate directly.
    if (chainActive.Tip()->nHeight - currentWitnessHeight < (uint64_t)(COINBASE_MATURITY))
        throw witness_error(witness::RPC_MISC_ERROR, "Cannot perform operation on immature transaction, please wait for transaction to mature and try again");

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
