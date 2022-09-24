// Copyright (c) 2017-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// File contains modifications by: The Centure developers
// All modifications:
// Copyright (c) 2017-2022 The Centure developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the Libre Chain License, see the accompanying
// file COPYING

#include "tx_verify.h"

#include "consensus.h"
#include "consensus/validation.h"
#include "script/interpreter.h"
#include "validation/validation.h"
#include "validation/witnessvalidation.h"

// TODO remove the following dependencies
#include "chain.h"
#include "coins.h"
#include "util/moneystr.h"
#include "base58.h"

//Gulden dependencies
#include "witnessutil.h"

const uint64_t extraLockLengthAllowance=721;

bool IsFinalTx(const CTransaction &tx, int nBlockHeight, int64_t nBlockTime)
{
    if (tx.nLockTime == 0)
        return true;
    if ((int64_t)tx.nLockTime < ((int64_t)tx.nLockTime < LOCKTIME_THRESHOLD ? (int64_t)nBlockHeight : nBlockTime))
        return true;
    if (IsOldTransactionVersion(tx.nVersion))
    {
        for (const auto& txin : tx.vin) {
            if (!(txin.GetSequence(tx.nVersion) == CTxIn::SEQUENCE_FINAL))
                return false;
        }
    }
    else
    {
        for (const auto& txin : tx.vin) {
            if ( txin.FlagIsSet(CTxInFlags::OptInRBF) || txin.FlagIsSet(CTxInFlags::HasLock) )
                return false;
        }
    }
    return true;
}

std::pair<int, int64_t> CalculateSequenceLocks(const CTransaction &tx, int flags, std::vector<int>* prevHeights, const CBlockIndex& block)
{
    assert(prevHeights->size() == tx.vin.size());

    // Will be set to the equivalent height- and time-based nLockTime
    // values that would be necessary to satisfy all relative lock-
    // time constraints given our view of block chain history.
    // The semantics of nLockTime are the last invalid height/time, so
    // use -1 to have the effect of any height or time being valid.
    int nMinHeight = -1;
    int64_t nMinTime = -1;

    // tx.nVersion is signed integer so requires cast to unsigned otherwise
    // we would be doing a signed comparison and half the range of nVersion
    // wouldn't support BIP 68.
    bool fEnforceBIP68 = static_cast<uint32_t>(tx.nVersion) >= 2 && (flags & LOCKTIME_VERIFY_SEQUENCE);

    // Do not enforce sequence numbers as a relative lock time
    // unless we have been instructed to
    if (!fEnforceBIP68) {
        return std::pair(nMinHeight, nMinTime);
    }

    for (size_t txinIndex = 0; txinIndex < tx.vin.size(); txinIndex++)
    {
        const CTxIn& txin = tx.vin[txinIndex];

        // Gulden - segsig - if we aren't using relative locktime then we don't have a sequence number at all.
        // Sequence numbers with the most significant bit set are not
        // treated as relative lock-times, nor are they given any
        // consensus-enforced meaning at this point.
        bool test1 = IsOldTransactionVersion(tx.nVersion) && (txin.GetSequence(tx.nVersion) & CTxIn::SEQUENCE_LOCKTIME_DISABLE_FLAG);
        bool test2 = !IsOldTransactionVersion(tx.nVersion) && !txin.FlagIsSet(CTxInFlags::HasRelativeLock);
        if (test1 || test2)
        {
            // The height of this input is not relevant for sequence locks
            (*prevHeights)[txinIndex] = 0;
            continue;
        }

        int nCoinHeight = (*prevHeights)[txinIndex];

        bool test3 = IsOldTransactionVersion(tx.nVersion) && (txin.GetSequence(tx.nVersion) & CTxIn::SEQUENCE_LOCKTIME_TYPE_FLAG);
        bool test4 = !IsOldTransactionVersion(tx.nVersion) && txin.FlagIsSet(HasTimeBasedRelativeLock);
        if (test3 || test4)
        {
            int64_t nCoinTime = block.GetAncestor(std::max(nCoinHeight-1, 0))->GetMedianTimePast();
            // NOTE: Subtract 1 to maintain nLockTime semantics
            // BIP 68 relative lock times have the semantics of calculating
            // the first block or time at which the transaction would be
            // valid. When calculating the effective block time or height
            // for the entire transaction, we switch to using the
            // semantics of nLockTime which is the last invalid block
            // time or height.  Thus we subtract 1 from the calculated
            // time or height.

            // Time-based relative lock-times are measured from the
            // smallest allowed timestamp of the block containing the
            // txout being spent, which is the median time past of the
            // block prior.
            if (IsOldTransactionVersion(tx.nVersion))
            {
                nMinTime = std::max(nMinTime, nCoinTime + (int64_t)((txin.GetSequence(tx.nVersion) & CTxIn::SEQUENCE_LOCKTIME_MASK) << CTxIn::SEQUENCE_LOCKTIME_GRANULARITY) - 1);
            }
            else
            {
                nMinTime = std::max(nMinTime, nCoinTime + (txin.GetSequence(tx.nVersion) << CTxIn::SEQUENCE_LOCKTIME_GRANULARITY) - 1);
            }
        }
        else
        {
            if (IsOldTransactionVersion(tx.nVersion))
            {
                nMinHeight = std::max(nMinHeight, nCoinHeight + (int)(txin.GetSequence(tx.nVersion) & CTxIn::SEQUENCE_LOCKTIME_MASK) - 1);
            }
            else
            {
                nMinHeight = std::max(nMinHeight, nCoinHeight + (int)txin.GetSequence(tx.nVersion) - 1);
            }
        }
    }

    return std::pair(nMinHeight, nMinTime);
}

bool EvaluateSequenceLocks(const CBlockIndex& block, std::pair<int, int64_t> lockPair)
{
    assert(block.pprev);
    int64_t nBlockTime = block.pprev->GetMedianTimePast();
    if (lockPair.first >= block.nHeight || lockPair.second >= nBlockTime)
        return false;

    return true;
}

bool SequenceLocks(const CTransaction &tx, int flags, std::vector<int>* prevHeights, const CBlockIndex& block)
{
    return EvaluateSequenceLocks(block, CalculateSequenceLocks(tx, flags, prevHeights, block));
}

unsigned int GetLegacySigOpCount(const CTransaction& tx)
{
    unsigned int nSigOps = 0;
    for (const auto& txin : tx.vin)
    {
        nSigOps += txin.scriptSig.GetSigOpCount(false);
    }
    for (const auto& txout : tx.vout)
    {
        switch (txout.GetType())
        {
            case CTxOutType::ScriptLegacyOutput: nSigOps += txout.output.scriptPubKey.GetSigOpCount(false); break;
            case CTxOutType::StandardKeyHashOutput: nSigOps += 1; break;
            case CTxOutType::PoW2WitnessOutput: nSigOps += 1; break;
        }
    }
    return nSigOps;
}

unsigned int GetP2SHSigOpCount(const CTransaction& tx, const CCoinsViewCache& inputs)
{
    if (tx.IsCoinBase())
        return 0;

    unsigned int nSigOps = 0;
    for (unsigned int i = 0; i < tx.vin.size(); i++)
    {
        const CTxOut &prevout = inputs.AccessCoin(tx.vin[i].GetPrevOut()).out;
        if (prevout.GetType() <= CTxOutType::ScriptLegacyOutput && prevout.output.scriptPubKey.IsPayToScriptHash())
            nSigOps += prevout.output.scriptPubKey.GetSigOpCount(tx.vin[i].scriptSig);
    }
    return nSigOps;
}

int64_t GetTransactionSigOpCost(const CTransaction& tx, const CCoinsViewCache& inputs, int flags)
{
    int64_t nSigOps = GetLegacySigOpCount(tx);

    if (tx.IsCoinBase())
        return nSigOps;

    if (flags & SCRIPT_VERIFY_P2SH) {
        nSigOps += GetP2SHSigOpCount(tx, inputs);
    }

    for (unsigned int i = 0; i < tx.vin.size(); i++)
    {
        //fixme: (PHASE5) (SEGSIG) - Is this right? - make sure we are counting sigops in segsig scripts correctly
        const CTxOut &prevout = inputs.AccessCoin(tx.vin[i].GetPrevOut()).out;
        switch (prevout.GetType())
        {
            case CTxOutType::StandardKeyHashOutput: nSigOps += 1; break;
            case CTxOutType::PoW2WitnessOutput: nSigOps += 1; break;
            case CTxOutType::ScriptLegacyOutput: /*already handled by GetP2SHSigOpCount*/ break;
        }
    }
    return nSigOps;
}

bool CheckTransaction(const CTransaction& tx, CValidationState &state, bool fCheckDuplicateInputs)
{
    // Basic checks that don't depend on any context
    if (tx.vin.empty())
        return state.DoS(10, false, REJECT_INVALID, "bad-txns-vin-empty");
    if (tx.vout.empty())
        return state.DoS(10, false, REJECT_INVALID, "bad-txns-vout-empty");
    // Size limits (this doesn't take the witness into account, as that hasn't been checked for malleability)
    if (::GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION | SERIALIZE_TRANSACTION_NO_SEGREGATED_SIGNATURES) > MAX_BLOCK_BASE_SIZE)
        return state.DoS(100, false, REJECT_INVALID, "bad-txns-oversize");

    // Check for negative or overflow output values
    CAmount nValueOut = 0;
    for (const auto& txout : tx.vout)
    {
        if (txout.nValue < 0)
            return state.DoS(100, false, REJECT_INVALID, "bad-txns-vout-negative");
        if (txout.nValue > MAX_MONEY)
            return state.DoS(100, false, REJECT_INVALID, "bad-txns-vout-toolarge");
        nValueOut += txout.nValue;
        if (!MoneyRange(nValueOut))
            return state.DoS(100, false, REJECT_INVALID, "bad-txns-txouttotal-toolarge");
    }

    // Check for duplicate inputs - note that this check is slow so we skip it in CheckBlock
    if (fCheckDuplicateInputs) {
        std::set<COutPoint> vInOutPoints;
        for (const auto& txin : tx.vin)
        {
            if (!vInOutPoints.insert(txin.GetPrevOut()).second)
                return state.DoS(100, false, REJECT_INVALID, "bad-txns-inputs-duplicate");
        }
    }

    if (tx.IsCoinBase())
    {
        if (IsOldTransactionVersion(tx.nVersion))
        {
            if (tx.vin[0].scriptSig.size() < 2 || tx.vin[0].scriptSig.size() > 100)
                return state.DoS(100, false, REJECT_INVALID, "bad-cb-length");
        }
        else
        {
            if (tx.vin[0].scriptSig.size() != 0)
                return state.DoS(100, false, REJECT_INVALID, "bad-cb-length");
            if (tx.vin[0].segregatedSignatureData.stack.size() != 2)
                return state.DoS(100, false, REJECT_INVALID, "bad-cb-segdata");
        }
    }
    else
    {
        for (const auto& txin : tx.vin)
        {
            if (txin.GetPrevOut().IsNull())
                return state.DoS(10, false, REJECT_INVALID, "bad-txns-prevout-null");
        }
    }

    return true;
}


bool CheckTransactionContextual(const CTransaction& tx, CValidationState &state, int checkHeight)
{
    for (const CTxOut& txout : tx.vout)
    {
        if (IsPow2WitnessOutput(txout))
        {
            if ( txout.nValue < (gMinimumWitnessAmount * COIN) )
            {
                if (txout.output.witnessDetails.lockFromBlock != 1)
                {
                    return state.DoS(10, false, REJECT_INVALID, strprintf("PoW² witness output smaller than %d " GLOBAL_COIN_CODE " not allowed.", gMinimumWitnessAmount));
                }
            }

            CTxOutPoW2Witness witnessDetails; GetPow2WitnessOutput(txout, witnessDetails);
            uint64_t nUnused1, nUnused2;
            int64_t nLockLengthInBlocks = GetPoW2LockLengthInBlocksFromOutput(txout, checkHeight, nUnused1, nUnused2);
            if (nLockLengthInBlocks < int64_t(MinimumWitnessLockLength()))
                return state.DoS(10, false, REJECT_INVALID, "PoW² witness locked for less than minimum of 1 month.");
            if (nLockLengthInBlocks > int64_t(MaximumWitnessLockLength()+extraLockLengthAllowance))
            {
                if (txout.output.witnessDetails.lockFromBlock != 1)
                {
                    return state.DoS(10, false, REJECT_INVALID, "PoW² witness locked for greater than maximum of 3 years.");
                }
            }

            int64_t nWeight = GetPoW2RawWeightForAmount(txout.nValue, checkHeight, nLockLengthInBlocks);
            if (nWeight < gMinimumWitnessWeight)
            {
                if (txout.output.witnessDetails.lockFromBlock != 1)
                {
                    return state.DoS(10, false, REJECT_INVALID, "PoW² witness has insufficient weight.");
                }
            }
        }
    }
    return true;
}

inline bool IsLockFromConsistent(const CTxOutPoW2Witness& inputDetails, const CTxOutPoW2Witness& outputDetails, uint64_t nInputHeight)
{
    // If lockfrom was previously 0 it must become the current block height instead.
    if (inputDetails.lockFromBlock == 0)
    {
        if (outputDetails.lockFromBlock != nInputHeight)
            return false;
    }
    else if (inputDetails.lockFromBlock != outputDetails.lockFromBlock)
        return false;
    return true;
}

bool CWitnessTxBundle::IsLockFromConsistent()
{
    // all ouputs must match the actual lockFrom value of the inputs
    if (inputs.size() > 0 && inputsActualLockFromBlock == 0)
        return false;

    return std::all_of(outputs.begin(), outputs.end(), [&](const auto& output) {
        return std::get<1>(output).lockFromBlock == inputsActualLockFromBlock;
    });
}

//fixme: (PHASE5) define this with rest of global constants (centralise all constants together)
static const int gMaximumRenewalPenalty = COIN*20;
static const int gPerFailCountRenewalPenalty = (2*COIN)/100;

CAmount CalculateWitnessPenaltyFee(const CTxOut& output)
{
    CTxOutPoW2Witness witnessDestination;
    if (!GetPow2WitnessOutput(output, witnessDestination))
    {
        return 0;
    }
    CAmount nPenalty = (gPerFailCountRenewalPenalty) * witnessDestination.failCount;
    if (nPenalty > gMaximumRenewalPenalty)
        return gMaximumRenewalPenalty;
    return nPenalty;
}

void IncrementWitnessFailCount(uint64_t& failCount)
{
    // Increment fail count (Increments in a sequence: 1 2 4 7 11 17 26 40 61 92 139 209 314 472 709 1064 1597 2396 3595 5393 8090 ...)
    failCount += (failCount/3);
    ++failCount;
    // Avoid overflow - by the time fail count is this high it no longer matters if we don't increment it further
    if (failCount > std::numeric_limits<uint64_t>::max() / 3)
        failCount = std::numeric_limits<uint64_t>::max() / 3;
}

inline bool HasSpendKey(const CTxIn& input, uint64_t nSpendHeight)
{
    // At this point we only need to check that there are 2 signatures, spending key and witness key.
    // The rest is handled by later parts of the code.
    if (input.segregatedSignatureData.stack.size() != 2)
        return false;
    return true;
}

/*Witness transactions inputs/outputs fall into several different categories.
* 1) Creation of a new witnessing account: lockfrom must be 0.
* Output only.
*/

/*
* 2) Witnessing: amount should stay the same or increase, fail count can reduced by 1 or 0.
* If lockfrom was previously 0 it must be set to the height of the block that contains the input.
* Can only appear as a "witnesscoinbase" transaction.
* Input/Output.
* Signed by witness key.
*/
inline bool IsWitnessBundle(const CTxIn& input, const CTxOutPoW2Witness& inputDetails, const CTxOutPoW2Witness& outputDetails, CAmount nInputAmount, CAmount nOutputAmount, uint64_t nInputHeight, bool isOldTransactionVersion)
{
    //Phase 3 (no signatures)
    //Phase 4, 1 signature - stack will have either 1 or 2 items depending on whether its a script or proper PoW2-witness address.
    if (isOldTransactionVersion)
    {
        return false;
    }
    else
    {
        if (inputDetails.nType == CTxOutType::PoW2WitnessOutput)
        {
            if (input.segregatedSignatureData.stack.size() != 1)
                return false;
        }
        if (inputDetails.nType == ScriptLegacyOutput)
        {
            if (input.segregatedSignatureData.stack.size() != 2)
                return false;
        }
    }
    // Amount in address should stay the same or increase
    if (nInputAmount > nOutputAmount)
        return false;
    // Action nonce always increment
    if (inputDetails.actionNonce+1 != outputDetails.actionNonce)
        return false;
    // Keys and lock unchanged
    if (inputDetails.spendingKeyID != outputDetails.spendingKeyID)
        return false;
    if (inputDetails.witnessKeyID != outputDetails.witnessKeyID)
        return false;
    if (inputDetails.lockUntilBlock != outputDetails.lockUntilBlock)
        return false;
    if (!IsLockFromConsistent(inputDetails, outputDetails, nInputHeight))
        return false;
    // Fail must be reduced by 1 or 0 if it is already 0.
    if (inputDetails.failCount == outputDetails.failCount + 1)
        return true;
    if (outputDetails.failCount == 0)
        return true;
    return false;
}

/*
* 3) Spending, only amount should change, entire amount should be consumed, current height must be.
* Input only
* Signed by spending key.
*/
inline bool CWitnessTxBundle::IsValidSpendBundle(uint64_t nCheckHeight, const CTransaction& tx)
{
    if (outputs.size() != 0)
        return false;
    if (inputs.size() != 1)
        return false;
    if (std::get<1>(inputs[0]).lockUntilBlock >= nCheckHeight)
        return false;

    return true;
}

/*
* 4) Renewal, only failcount should change, must be multiplied by 2 then increased by 1.
* If lockfrom was previously 0 it must be set to the height of the block that contains the input.
* Input/Output.
* Signed by spending key.
*/
inline bool IsRenewalBundle(const CTxIn& input, const CTxOutPoW2Witness& inputDetails, const CTxOutPoW2Witness& outputDetails, const CTxOut& prevOut, const CTxOut& output, uint64_t nInputHeight, uint64_t nSpendHeight)
{
    // Needs 2 signature (spending key)
    if (!HasSpendKey(input, nSpendHeight))
    {
        return false;
    }

    // Amount keys and lock unchanged.
    if (prevOut.nValue != output.nValue)
        return false;
    // Action nonce always increment
    if (inputDetails.actionNonce+1 != outputDetails.actionNonce)
        return false;
    if (inputDetails.spendingKeyID != outputDetails.spendingKeyID)
        return false;
    if (inputDetails.witnessKeyID != outputDetails.witnessKeyID)
        return false;
    if (inputDetails.lockUntilBlock != outputDetails.lockUntilBlock)
        return false;
    if (!IsLockFromConsistent(inputDetails, outputDetails, nInputHeight))
        return false;
    // Fail count must be incremented appropriately
    uint64_t compFailCount = inputDetails.failCount;
    IncrementWitnessFailCount(compFailCount);
    if (compFailCount == outputDetails.failCount)
        return true;
    return false;
}

bool CWitnessTxBundle::IsValidMultiRenewalBundle(uint64_t nSpendHeight)
{
    if (nSpendHeight <= Params().GetConsensus().pow2WitnessSyncHeight)
        return false;
        
    if (inputs.size() < 2)
        return false;
    if (outputs.size() < 2)
        return false;
    if (inputs.size() != outputs.size())
        return false;

    // Input and outputs must always match in order
    for (uint64_t inputIndex=0; inputIndex<inputs.size();++inputIndex)
    {
        const auto& input = inputs[inputIndex];
        const auto& output = outputs[inputIndex];
        
        // Amount keys and lock unchanged.
        if (std::get<0>(input).nValue != std::get<0>(output).nValue)
            return false;
        // Action nonce always increment
        if (std::get<1>(input).actionNonce+1 != std::get<1>(output).actionNonce)
            return false;
        if (std::get<1>(input).spendingKeyID != std::get<1>(output).spendingKeyID)
            return false;
        if (std::get<1>(input).witnessKeyID != std::get<1>(output).witnessKeyID)
            return false;
        if (std::get<1>(input).lockUntilBlock != std::get<1>(output).lockUntilBlock)
            return false;
        // Fail count must be incremented appropriately
        uint64_t compFailCount = std::get<1>(input).failCount;
        IncrementWitnessFailCount(compFailCount);
        if (compFailCount != std::get<1>(output).failCount)
            return false;
    }

    if (!IsLockFromConsistent())
        return false;

    return true;
}



/*
* 5) Increase amount and/or lock period. Reset lockfrom to 0. Lock period and/or amount must exceed or equal old period/amount.
* N inputs, M outputs.
* Signed by spending key.
* Precondition - we must have already verified externally from here that the scriptwitness stack for the input is of size 2.
*/
bool CWitnessTxBundle::IsValidIncreaseBundle()
{
    if (inputs.size() < 1)
        return false;
    if (outputs.size() < 1)
        return false;

    const auto& input0 = inputs[0];
    const auto& input0Details = std::get<1>(input0);

    const auto& output0 = outputs[0];
    const auto& output0Details = std::get<1>(output0);

    CAmount nInputValue = 0;
    CAmount nOutputValue = 0;

    uint64_t highestActionNonce = 0;
    uint64_t highestFailCount = 0;

    // A: verify inputs all have witness properties equal to output0 except lockUntilBlock and lockFromBlock
    // in addition determine highestActionNonce and highestFailCount
    for (const auto& input : inputs)
    {
        const auto& inputDetails = std::get<1>(input);
        if (inputDetails.spendingKeyID != output0Details.spendingKeyID)
            return false;
        if (inputDetails.witnessKeyID != output0Details.witnessKeyID)
            return false;
        if (inputDetails.actionNonce > highestActionNonce)
            highestActionNonce = inputDetails.actionNonce;
        if (inputDetails.failCount > highestFailCount)
            highestFailCount = inputDetails.failCount;
        nInputValue += std::get<0>(input).nValue;
    }

    // B: verify outputs all have witness properties equal to input0 except lockUntilBlock
    // verify values for actionNonce and failCount using highest input values and from block reset
    for (const auto& output : outputs)
    {
        const auto& outputDetails = std::get<1>(output);
        if (highestActionNonce + 1 != outputDetails.actionNonce)
            return false;
        if (highestFailCount != outputDetails.failCount)
            return false;
        if (input0Details.spendingKeyID != outputDetails.spendingKeyID)
            return false;
        if (input0Details.witnessKeyID != outputDetails.witnessKeyID)
            return false;
        if (0 != outputDetails.lockFromBlock)
            return false;
        nOutputValue += std::get<0>(output).nValue;
    }

    // A and B imply that all inputs and outputs have identical witness properties here (except

    // C: verify that all inputs have the same lockUntilBlock
    if (std::any_of(++inputs.begin(), inputs.end(), [&](const auto& input){ return std::get<1>(input).lockUntilBlock != input0Details.lockUntilBlock; }))
        return false;

    // D: verify that all outputs have the same lockUntilBlock
    if (std::any_of(++outputs.begin(), outputs.end(), [&](const auto& output){ return std::get<1>(output).lockUntilBlock != output0Details.lockUntilBlock; }))
        return false;

    // E: verify that input lockUntilBlock <= output lockUntilBlock
    if (input0Details.lockUntilBlock > output0Details.lockUntilBlock)
        return false;

    // F: verify that output value is at least input value
    if (nOutputValue < nInputValue)
        return false;

    return true;
}

/*
* 6) Rearrange, everything must stay the same, but input(s) may be split or merged into a number of outpus(s) identical outputs.
* N inputs, M outputs.
* Signed by spending key.
* Precondition - we must have already verified externally from here that the scriptwitness stack for the input is of size 2.
*/
bool CWitnessTxBundle::IsValidRearrangeBundle()
{
    if (inputs.size() < 1)
        return false;
    if (outputs.size() < 1)
        return false;

    const auto& input0 = inputs[0];
    const auto& input0Details = std::get<1>(input0);

    const auto& output0 = outputs[0];
    const auto& output0Details = std::get<1>(output0);

    CAmount nInputValue = 0;
    CAmount nOutputValue = 0;

    uint64_t highestActionNonce = 0;
    uint64_t highestFailCount = 0;

    // A: verify inputs all have witness properties equal to output0 (except lockFromBlock)
    for (const auto& input : inputs)
    {
        const auto& inputDetails = std::get<1>(input);
        if (inputDetails.spendingKeyID != output0Details.spendingKeyID)
            return false;
        if (inputDetails.witnessKeyID != output0Details.witnessKeyID)
            return false;
        if (inputDetails.lockUntilBlock != output0Details.lockUntilBlock)
            return false;
        if (inputDetails.actionNonce > highestActionNonce)
            highestActionNonce = inputDetails.actionNonce;
        if (inputDetails.failCount > highestFailCount)
            highestFailCount = inputDetails.failCount;
        nInputValue += std::get<0>(input).nValue;
    }

    // B: verify outputs all have witness properties equal to input0
    for (const auto& output : outputs)
    {
        const auto& outputDetails = std::get<1>(output);
        // Action nonce always increment
        if (highestActionNonce + 1 != outputDetails.actionNonce)
            return false;
        if (highestFailCount != outputDetails.failCount)
            return false;
        if (input0Details.spendingKeyID != outputDetails.spendingKeyID)
            return false;
        if (input0Details.witnessKeyID != outputDetails.witnessKeyID)
            return false;
        if (input0Details.lockUntilBlock != outputDetails.lockUntilBlock)
            return false;
        nOutputValue += std::get<0>(output).nValue;
    }

    // A and B imply that all inputs and outputs have identical witness properties here

    // C: verify input and output value
    if (nInputValue != nOutputValue)
        return false;

    // D: verify outputs lockfrom matches actual lockfrom
    if (!IsLockFromConsistent())
        return false;

    return true;
}

/*
* 7) Update, identical inputs/outputs but witness key changes.
* M inputs, M outputs.
* Signed by spending key.
* Precondition - we must have already verified externally from here that the scriptwitness stack for the input is of size 2.
*/
inline bool CWitnessTxBundle::IsValidChangeWitnessKeyBundle()
{
    // # of witness inputs equal to # outputs
    if (inputs.size() < 1 || inputs.size() != outputs.size())
        return false;

    const auto& output0 = outputs[0];
    const auto& output0Details = std::get<1>(output0);

    // for every input there is at least one output that matches
    for (const auto& input : inputs)
    {
        const auto& inputDetails = std::get<1>(input);

        if (1 > std::count_if(outputs.begin(), outputs.end(), [&](const auto& output)
        {
                const auto& outputDetails = std::get<1>(output);
                if (std::get<0>(input).nValue != std::get<0>(output).nValue)
                    return false;

                if (inputDetails.spendingKeyID != outputDetails.spendingKeyID)
                    return false;
                if (inputDetails.witnessKeyID == outputDetails.witnessKeyID)
                    return false;
                if (inputDetails.lockUntilBlock != outputDetails.lockUntilBlock)
                    return false;
                if (inputDetails.actionNonce + 1 != outputDetails.actionNonce)
                    return false;
                if (inputDetails.failCount != outputDetails.failCount)
                    return false;
                return true;
            }))
        {
            return false;
        }
    }

    // verify outputs lockfrom matches actual lockfrom
    if (!IsLockFromConsistent())
        return false;

    // all outputs have the same (new) witness key and all outputs have the same spend key
    if (std::any_of(++outputs.begin(), outputs.end(), [&](const auto& output) {
            const auto& outputDetails = std::get<1>(output);
            return (outputDetails.spendingKeyID != output0Details.spendingKeyID) || (outputDetails.witnessKeyID != output0Details.witnessKeyID); }))
    {
        return false;
    }

    // output witness key is different from spend key
    if (output0Details.witnessKeyID == output0Details.spendingKeyID)
        return false;

    return true;
}

//fixme: (PHASE5) (HIGH) Implement unit test code for this function.
bool CheckTxInputAgainstWitnessBundles(CValidationState& state, std::vector<CWitnessTxBundle>* pWitnessBundles, const CTxOut& prevOut, const CTxIn input, uint64_t inputTxHeight, uint64_t inputTxIndex, uint64_t inputTxOutputIndex, uint64_t nSpendHeight, bool isOldTransactionVersion)
{
    if (pWitnessBundles)
    {
        if (IsPow2WitnessOutput(prevOut))
        {
            CTxOutPoW2Witness inputDetails;
            GetPow2WitnessOutput(prevOut, inputDetails);
            // At this point all bundles are currently marked either Creation/Witness - modify the types as we pair them up.
            bool matchedExistingBundle = false;
            for (auto& bundle : *pWitnessBundles)
            {
                if (bundle.outputs.size() == 1)
                {
                    const auto& outputDetails = std::get<1>(bundle.outputs[0]);

                    // Witnessing: amount should stay the same or increase, fail count can reduced by 1 or 0, if lockfrom was previously 0 it must be set to the current block height. Can only appear as a "witnesscoinbase" transaction.
                    if ( IsWitnessBundle(input, inputDetails, outputDetails, prevOut.nValue, std::get<0>(bundle.outputs[0]).nValue, inputTxHeight, isOldTransactionVersion) )
                    {
                        if (std::get<0>(bundle.outputs[0]).nValue - prevOut.nValue > gMaximumWitnessCompoundAmount * COIN)
                        {
                            return state.DoS(50, false, REJECT_INVALID, "witness-coinbase-compounding-too-much");
                        }

                        matchedExistingBundle = true;
                        bundle.inputs.push_back(std::tuple(prevOut, std::move(inputDetails), COutPoint(inputTxHeight, inputTxIndex, inputTxOutputIndex)));
                        bundle.bundleType = CWitnessTxBundle::WitnessTxType::WitnessType;
                        break;
                    }
                    else if ( IsRenewalBundle(input, inputDetails, outputDetails, prevOut, std::get<0>(bundle.outputs[0]), inputTxHeight, nSpendHeight) )
                    {
                        matchedExistingBundle = true;
                        bundle.inputs.push_back(std::tuple(prevOut, std::move(inputDetails), COutPoint(inputTxHeight, inputTxIndex, inputTxOutputIndex)));
                        bundle.bundleType = CWitnessTxBundle::WitnessTxType::RenewType;
                        break;
                    }
                }
            }
            if (!matchedExistingBundle)
            {
                //NB! We -must- check here that we have the spending key (2 items on stack) as when we later check the built up RearrangeType bundles, or multi renewal bundles, we have no way to check it then.
                //So this check is very important, must not be skipped and must come before the bundle creation for these bundle types.
                //Exception for phase 3 inputs which use the ScriptLegacyOutput, not allowing this can get your witness "stuck", ie. not being able to empty it
                if (!HasSpendKey(input, nSpendHeight))
                {
                    if (prevOut.GetType() != CTxOutType::ScriptLegacyOutput) // accept phase 3 inputs
                        return state.DoS(100, false, REJECT_INVALID, "bad-txns-in-witness-missing-spend-key");
                }

                bool matchedExistingBundle = false;
                uint64_t actualLockFromBlock = inputDetails.lockFromBlock == 0 ? inputTxHeight : inputDetails.lockFromBlock;
                for (auto& bundle : *pWitnessBundles)
                {
                    if (bundle.outputs.size() == 0)
                        continue;
                    if ( (bundle.bundleType == CWitnessTxBundle::WitnessTxType::SpendType || bundle.bundleType == CWitnessTxBundle::WitnessTxType::RearrangeType  || bundle.bundleType == CWitnessTxBundle::WitnessTxType::IncreaseType) && (std::get<1>(bundle.outputs[0]).witnessKeyID == inputDetails.witnessKeyID) && (std::get<1>(bundle.outputs[0]).spendingKeyID == inputDetails.spendingKeyID) )
                    {
                        bundle.bundleType = std::get<1>(bundle.outputs[0]).lockFromBlock == 0 ? CWitnessTxBundle::WitnessTxType::IncreaseType : CWitnessTxBundle::WitnessTxType::RearrangeType;
                        if (bundle.inputsActualLockFromBlock == 0)
                            bundle.inputsActualLockFromBlock = actualLockFromBlock;
                        else if (bundle.inputsActualLockFromBlock != actualLockFromBlock)
                            return state.DoS(100, false, REJECT_INVALID, "bad-txns-in-witness-bundle-mismatching-lockfrom");
                        bundle.inputs.push_back(std::tuple(prevOut, std::move(inputDetails), COutPoint(inputTxHeight, inputTxIndex, inputTxOutputIndex)));
                        matchedExistingBundle = true;
                    }
                    else if ( (bundle.bundleType == CWitnessTxBundle::WitnessTxType::SpendType || bundle.bundleType == CWitnessTxBundle::WitnessTxType::ChangeWitnessKeyType || bundle.bundleType == CWitnessTxBundle::WitnessTxType::RearrangeType) && (std::get<1>(bundle.outputs[0]).witnessKeyID != inputDetails.witnessKeyID) && (std::get<1>(bundle.outputs[0]).spendingKeyID == inputDetails.spendingKeyID) )
                    {
                        bundle.bundleType = CWitnessTxBundle::WitnessTxType::ChangeWitnessKeyType;
                        if (bundle.inputsActualLockFromBlock == 0)
                            bundle.inputsActualLockFromBlock = actualLockFromBlock;
                        else if (bundle.inputsActualLockFromBlock != actualLockFromBlock)
                            return state.DoS(100, false, REJECT_INVALID, "bad-txns-in-witness-bundle-mismatching-lockfrom");
                        bundle.inputs.push_back(std::tuple(prevOut, std::move(inputDetails), COutPoint(inputTxHeight, inputTxIndex, inputTxOutputIndex)));
                        matchedExistingBundle = true;
                    }
                }
                if (!matchedExistingBundle)
                {
                    CWitnessTxBundle spendBundle = CWitnessTxBundle(CWitnessTxBundle::WitnessTxType::SpendType);
                    spendBundle.inputsActualLockFromBlock = actualLockFromBlock;
                    spendBundle.inputs.push_back(std::tuple(prevOut, std::move(inputDetails), COutPoint(inputTxHeight, inputTxIndex, inputTxOutputIndex)));
                    pWitnessBundles->push_back(spendBundle);
                }
            }
        }
    }
    return true;
}

bool Consensus::CheckTxInputs(const CTransaction& tx, CValidationState& state, const CCoinsViewCache& inputs, int nSpendHeight, const std::vector<CWitnessTxBundle>* pWitnessBundles)
{
        // This doesn't trigger the DoS code on purpose; if it did, it would make it easier
        // for an attacker to attempt to split the network.
        if (tx.IsPoW2WitnessCoinBase())
        {
            for (unsigned int i = 0; i < tx.vin.size(); i++)
            {
                if (!tx.vin[i].GetPrevOut().IsNull())
                {
                    if (!inputs.HaveCoin(tx.vin[i].GetPrevOut()))
                    {
                        return state.Invalid(false, 0, "", "Inputs unavailable");
                    }
                }
            }
        }
        else
        {
            if (!inputs.HaveInputs(tx))
                return state.Invalid(false, 0, "", "Inputs unavailable");
        }
        
        CAmount nValueIn = 0;
        CAmount nFees = 0;
        for (unsigned int i = 0; i < tx.vin.size(); i++)
        {
            const COutPoint &prevout = tx.vin[i].GetPrevOut();
            if (prevout.IsNull() && tx.IsPoW2WitnessCoinBase())
                continue;
            const Coin& coin = inputs.AccessCoin(prevout);
            assert(!coin.IsSpent());
            
            // If prev is index based, then ensure it has sufficient maturity. (For the sake of simplicity we keep this the same depth as mining maturity)
            if (!prevout.isHash)
            {
                // NB! If we ever change the maturity depth here to a different one than that of coinbase maturity - then we also need to change the below 'else if' control block into an 'if' control block.
                if (nSpendHeight - coin.nHeight < COINBASE_MATURITY)
                {
                    return state.Invalid(false, REJECT_INVALID, "bad-txns-insufficient-depth-index-input", strprintf("tried to spend input via index at depth %d; Only allowed at depth %d", nSpendHeight - coin.nHeight, COINBASE_MATURITY));
                }
            }
            // If prev is coinbase, check that it's matured
            else if (coin.IsCoinBase())
            {
                // NB! If we ever change the maturity depth here to a different one than that of prevout-index maturity (control block above this one) - then we also need to change the above 'else if' for this control block into an 'if' instead.
                if (nSpendHeight - coin.nHeight < COINBASE_MATURITY)
                {
                    return state.Invalid(false, REJECT_INVALID, "bad-txns-premature-spend-of-coinbase", strprintf("tried to spend coinbase at depth %d", nSpendHeight - coin.nHeight));
                }
            }

            // Check for negative or overflow input values
            nValueIn += coin.out.nValue;
            if (!MoneyRange(coin.out.nValue) || !MoneyRange(nValueIn))
                return state.DoS(100, false, REJECT_INVALID, "bad-txns-inputvalues-outofrange");
        }

        CAmount witnessPenaltyFee = 0;
        if (pWitnessBundles)
        {
            for (auto& bundle : *pWitnessBundles)
            {
                if(bundle.bundleType == CWitnessTxBundle::WitnessTxType::RenewType)
                    witnessPenaltyFee += CalculateWitnessPenaltyFee(std::get<0>(bundle.outputs[0]));
            }
        }

        if (nValueIn < tx.GetValueOut())
            return state.DoS(100, false, REJECT_INVALID, "bad-txns-in-belowout", false,
                strprintf("value in (%s) < value out (%s)", FormatMoney(nValueIn), FormatMoney(tx.GetValueOut())));

        // Tally transaction fees
        CAmount nTxFee = nValueIn - tx.GetValueOut();
        if (nTxFee < 0)
            return state.DoS(100, false, REJECT_INVALID, "bad-txns-fee-negative");
        if (nTxFee < witnessPenaltyFee)
            return state.DoS(100, false, REJECT_INVALID, "bad-txns-fee-missing-witness-renewal-penalty-fee");
        nFees += nTxFee;
        if (!MoneyRange(nFees))
            return state.DoS(100, false, REJECT_INVALID, "bad-txns-fee-outofrange");
    return true;
}

bool BuildWitnessBundles(const CTransaction& tx, CValidationState& state, uint64_t nSpendHeight, uint64_t transactionIndex, std::function<bool(const COutPoint&, CTxOut&, uint64_t&, uint64_t&, uint64_t&)> getTxOut, std::vector<CWitnessTxBundle>& bundles)
{
    bundles.clear();

    std::vector<CWitnessTxBundle> resultBundles;

    uint64_t transactionOutputIndex=0;
    for (const CTxOut& txout : tx.vout)
    {
        if (IsPow2WitnessOutput(txout))
        {
            CTxOutPoW2Witness witnessDetails; GetPow2WitnessOutput(txout, witnessDetails);
            if ( txout.nValue < (gMinimumWitnessAmount * COIN) )
            {
                if (witnessDetails.lockFromBlock != 1)
                {
                    return state.DoS(10, false, REJECT_INVALID, strprintf("PoW² witness output smaller than %d " GLOBAL_COIN_CODE " not allowed.", gMinimumWitnessAmount));
                }
            }
            
            uint64_t nUnused1, nUnused2;
            int64_t nLockLengthInBlocks = GetPoW2LockLengthInBlocksFromOutput(txout, nSpendHeight, nUnused1, nUnused2);
            if (nLockLengthInBlocks < int64_t(MinimumWitnessLockLength()))
            {
                return state.DoS(10, false, REJECT_INVALID, "PoW² witness locked for less than minimum of 1 month.");
            }
            if (nLockLengthInBlocks > int64_t(MaximumWitnessLockLength()+extraLockLengthAllowance))
            {
                if (witnessDetails.lockFromBlock != 1)
                {
                    return state.DoS(10, false, REJECT_INVALID, "PoW² witness locked for greater than maximum of 3 years.");
                }
            }

            int64_t nWeight = GetPoW2RawWeightForAmount(txout.nValue, nSpendHeight, nLockLengthInBlocks);
            if (nWeight < gMinimumWitnessWeight)
            {
                if (witnessDetails.lockFromBlock != 1)
                {
                    return state.DoS(10, false, REJECT_INVALID, "PoW² witness has insufficient weight.");
                }
            }

            if (tx.IsPoW2WitnessCoinBase())
            {
                resultBundles.push_back(CWitnessTxBundle(CWitnessTxBundle::WitnessTxType::WitnessType, std::tuple(txout, std::move(witnessDetails), COutPoint(nSpendHeight, transactionIndex, transactionOutputIndex))));
            }
            else
            {
                bool matchedExistingBundle = false;
                for (auto& bundle: resultBundles)
                {
                    if (bundle.bundleType == CWitnessTxBundle::WitnessTxType::CreationType && witnessDetails.lockFromBlock == 0 && witnessDetails.actionNonce == 0 && std::get<1>(bundle.outputs[0]).witnessKeyID == witnessDetails.witnessKeyID && std::get<1>(bundle.outputs[0]).spendingKeyID == witnessDetails.spendingKeyID)
                    {
                        bundle.outputs.push_back(std::tuple(txout, std::move(witnessDetails), COutPoint(nSpendHeight, transactionIndex, transactionOutputIndex)));
                        matchedExistingBundle = true;
                    }
                    else if ( (bundle.bundleType == CWitnessTxBundle::WitnessTxType::SpendType || bundle.bundleType == CWitnessTxBundle::WitnessTxType::RearrangeType) && (std::get<1>(bundle.outputs[0]).witnessKeyID == witnessDetails.witnessKeyID) && std::get<1>(bundle.outputs[0]).spendingKeyID == witnessDetails.spendingKeyID )
                    {
                        bundle.bundleType = CWitnessTxBundle::WitnessTxType::RearrangeType;
                        bundle.outputs.push_back(std::tuple(txout, std::move(witnessDetails), COutPoint(nSpendHeight, transactionIndex, transactionOutputIndex)));
                        matchedExistingBundle = true;
                        break;
                    }
                }
                if (!matchedExistingBundle)
                {
                    if (witnessDetails.lockFromBlock == 0 && witnessDetails.actionNonce == 0)
                    {
                        resultBundles.push_back(CWitnessTxBundle(CWitnessTxBundle::WitnessTxType::CreationType, std::tuple(txout, std::move(witnessDetails), COutPoint(nSpendHeight, transactionIndex, transactionOutputIndex))));
                    }
                    else
                    {
                        // if not matched to an existing bundle and the output is a witness output, how can it ever be a spend bundle?? it has to be followed by input checking to correct this.
                        // perhaps better to introduce new bundle type indicating "undetermined" or something similar.
                        CWitnessTxBundle spendBundle = CWitnessTxBundle(CWitnessTxBundle::WitnessTxType::SpendType, std::tuple(txout, std::move(witnessDetails), COutPoint(nSpendHeight, transactionIndex, transactionOutputIndex)));
                        resultBundles.push_back(spendBundle);
                    }
                }
            }
        }
        ++transactionOutputIndex;
    }

    // match tx inputs to existing bundles or create new
    for (unsigned int i = 0; i < tx.vin.size(); i++)
    {
        const COutPoint &prevout = tx.vin[i].GetPrevOut();
        if (prevout.IsNull() && tx.IsCoinBase())
        {
            continue;
        }

        CTxOut inputTxOut;
        uint64_t inputTxHeight;
        uint64_t inputTxIndex;
        uint64_t inputTxOutputIndex;
        if (!getTxOut(prevout, inputTxOut, inputTxHeight, inputTxIndex, inputTxOutputIndex))
        {
            return false;
        }

        if (!CheckTxInputAgainstWitnessBundles(state, &resultBundles, inputTxOut, tx.vin[i], inputTxHeight, inputTxIndex, inputTxOutputIndex, nSpendHeight, IsOldTransactionVersion(tx.nVersion)))
        {
            return false;
        }
    }

    for (auto& bundle: resultBundles)
    {
        if (bundle.bundleType == CWitnessTxBundle::WitnessTxType::RearrangeType)
        {
            if (!bundle.IsValidRearrangeBundle())
            {
                if (bundle.IsValidMultiRenewalBundle(nSpendHeight))
                {
                    bundle.bundleType = CWitnessTxBundle::WitnessTxType::RenewType;
                }
                else
                {
                    return state.DoS(100, false, REJECT_INVALID, "bad-txns-in-witness-invalid-rearrange-bundle");
                }
            }
        }
        else if(bundle.bundleType == CWitnessTxBundle::WitnessTxType::SpendType)
        {
            if (!bundle.IsValidSpendBundle(nSpendHeight, tx))
            {
                return state.DoS(100, false, REJECT_INVALID, "bad-txns-in-witness-invalid-spend-bundle");
            }
        }
        else if(bundle.bundleType == CWitnessTxBundle::WitnessTxType::IncreaseType)
        {
            if (!bundle.IsValidIncreaseBundle())
            {
                return state.DoS(100, false, REJECT_INVALID, "bad-txns-in-witness-invalid-increase-bundle");
            }
        }
        else if(bundle.bundleType == CWitnessTxBundle::WitnessTxType::ChangeWitnessKeyType)
        {
            if (!bundle.IsValidChangeWitnessKeyBundle())
            {
                return state.DoS(100, false, REJECT_INVALID, "bad-txns-in-witness-invalid-changewitnesskey-bundle");
            }
        }
        else if(bundle.bundleType == CWitnessTxBundle::WitnessTxType::RenewType)
        {
            // nop, this case already passed ::IsRenewalBundle()
        }
    }

    bundles = std::move(resultBundles);
    return true;
}

