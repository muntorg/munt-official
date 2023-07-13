// Copyright (c) 2017-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// File contains modifications by: The Centure developers
// All modifications:
// Copyright (c) 2017-2022 The Centure developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the GNU Lesser General Public License v3, see the accompanying
// file COPYING

#ifndef CONSENSUS_TX_VERIFY_H
#define CONSENSUS_TX_VERIFY_H

#include "primitives/transaction.h"

#include <stdint.h>
#include <vector>
#include <functional>

class CBlockIndex;
class CCoinsViewCache;
class CTransaction;
class CValidationState;

/*Witness transactions inputs/outputs fall into several different categories.
* Creating a new witness account
* Perform a witness operation
* Spending coins (when the lock is expired only)
* Reactivating (renewing) a witness that has been kicked out for non-participation
* Increasing the lock amount/time on an existing account
* Rearrange existing accounts where the weight exceeds the ideal taking N inputs and M outputs sharing the same properties and equal total amount
* Changing the witness key if it has been compromised
* 
* Depending on the type of operation that is being performed, different constraints apply - can only spend when lock expired but other actions allowed all the time etc.
* Worse, because we can have multiple inputs/outputs mixed in a transaction we could have one transaction performing more than one operation.
* When doing CheckTransactionContextual/CheckInputs we first perform an algorithm to try identify the type of operation and categorise the inputs/outputs into 'bundles' of the types.
* And then ensure the constraints for those types apply.
* It is important that this acts in a deterministic way.
* 
* WitnessTxBundle class is used to do this - for more specific details on the constraints/detection of the various types see the details comments in tx_verify.cpp starting at function HasSpendKey()
*/ 
struct CWitnessTxBundle
{
    enum WitnessTxType
    {
        CreationType,
        WitnessType,
        SpendType,
        RenewType,
        IncreaseType,
        RearrangeType,
        ChangeWitnessKeyType
    };
    CWitnessTxBundle(WitnessTxType bundleType_, std::tuple<const CTxOut, CTxOutPoW2Witness, COutPoint> output)
    : bundleType(bundleType_)
    {
        outputs.push_back(output);
    }
    CWitnessTxBundle(WitnessTxType bundleType_) : bundleType(bundleType_) {}
    CWitnessTxBundle() {}

    inline bool IsValidRearrangeBundle();
    inline bool IsValidMultiRenewalBundle(uint64_t nHeight);
    inline bool IsValidSpendBundle(uint64_t nHeight, const CTransaction& transaction);
    inline bool IsValidChangeWitnessKeyBundle();
    inline bool IsValidIncreaseBundle();

    bool IsLockFromConsistent();

    WitnessTxType bundleType=CreationType;
    uint64_t inputsActualLockFromBlock = 0;

    //NB! The COutPoint's below are not guaranteed to always be available; if we have been serialised/unserialised from disk they will be set as (0, 0, 0)
    //Any code that relies on the OutPoints should ensure it is working on unserialised copies (or update the serialisation code if appropriate)
    std::vector<std::tuple<const CTxOut, CTxOutPoW2Witness, COutPoint>> inputs;
    std::vector<std::tuple<const CTxOut, CTxOutPoW2Witness, COutPoint>> outputs;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(bundleType);
        READWRITE(inputsActualLockFromBlock);
        if (ser_action.ForRead())
        {
            uint64_t inputSize = ReadCompactSize(s);
            for (uint64_t i=0; i<inputSize; ++i)
            {
                std::pair<const CTxOut, CTxOutPoW2Witness> item;
                CTxOut& txOut = REF(item.first);
                txOut.ReadFromStream(s, CTxOut::NEW_FORMAT_VERSION);
                STRREAD(item.second);
                inputs.push_back(std::tuple(item.first, item.second, COutPoint()));
            }
            uint64_t outputSize = ReadCompactSize(s);
            for (uint64_t i=0; i<outputSize; ++i)
            {
                std::pair<const CTxOut, CTxOutPoW2Witness> item;
                CTxOut& txOut = REF(item.first);
                txOut.ReadFromStream(s, CTxOut::NEW_FORMAT_VERSION);
                STRREAD(item.second);
                outputs.push_back(std::tuple(item.first, item.second, COutPoint()));
            }
        }
        else
        {
            WriteCompactSize(s, inputs.size());
            for (const auto& [txOut, txOutWitness, outPoint] : inputs)
            {
                (void) outPoint;
                STRWRITE(std::pair(txOut, txOutWitness));
            }
            WriteCompactSize(s, outputs.size());
            for (const auto& [txOut, txOutWitness, outPoint] : outputs)
            {
                (void) outPoint;
                STRWRITE(std::pair(txOut, txOutWitness));
            }
        }
    }
};


struct CWitnessBundles: std::vector<CWitnessTxBundle>
{
    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        std::vector<CWitnessTxBundle>& vBundles = *this;
        READWRITECOMPACTSIZEVECTOR(vBundles);
    }

    CWitnessBundles() {}

    /** This deserializing constructor to support unserialize of const objects. */
    template <typename Stream>
    CWitnessBundles(deserialize_type, Stream& s)
    {
        Unserialize(s);
    }

};

bool CheckTxInputAgainstWitnessBundles(CValidationState& state, std::vector<CWitnessTxBundle>* pWitnessBundles, const CTxOut& prevOut, const CTxIn input, uint64_t nInputHeight, uint64_t nSpendHeight, bool isOldTransactionVersion);

CAmount CalculateWitnessPenaltyFee(const CTxOut& output);
void IncrementWitnessFailCount(uint64_t& failCount);

/** Transaction validation functions */

/** Context-independent validity checks */
bool CheckTransaction(const CTransaction& tx, CValidationState& state, bool fCheckDuplicateInputs=true);

/** Context-dependent validity checks */
bool CheckTransactionContextual(const CTransaction& tx, CValidationState& state, int checkHeight);

/** Build witness bundles and witness related validity checks */
bool BuildWitnessBundles(const CTransaction& tx, CValidationState& state, uint64_t nSpendHeight, uint64_t transactionIndex, std::function<bool(const COutPoint&, CTxOut&, uint64_t&, uint64_t&, uint64_t&)> getTxOut, std::vector<CWitnessTxBundle>& bundles);

namespace Consensus {
/**
 * Check whether all inputs of this transaction are valid (no double spends and amounts)
 * This does not modify the UTXO set. This does not check scripts and sigs.
 * Also verifies that all witness input/outputs are groupable in a 'valid' way in terms of the constraints outlined for CWitnessTxBundle for their given type of operation (spend/witness/renew etc.) if witnessBundles
 * witnessBundles should be prepopulated for outputs first by calling CheckTransactionContextual
 * Preconditions: tx.IsCoinBase() is false. witnessBundles is not null if witness transactions are to be verified.
 */
bool CheckTxInputs(const CTransaction& tx, CValidationState& state, const CCoinsViewCache& inputs, int nSpendHeight, const std::vector<CWitnessTxBundle>* pWitnessBundles);
} // namespace Consensus

/** Auxiliary functions for transaction validation (ideally should not be exposed) */

/**
 * Count ECDSA signature operations the old-fashioned (pre-0.6) way
 * @return number of sigops this transaction's outputs will produce when spent
 * @see CTransaction::FetchInputs
 */
unsigned int GetLegacySigOpCount(const CTransaction& tx);

/**
 * Count ECDSA signature operations in pay-to-script-hash inputs.
 * 
 * @param[in] mapInputs Map of previous transactions that have outputs we're spending
 * @return maximum number of sigops required to validate this transaction's inputs
 * @see CTransaction::FetchInputs
 */
unsigned int GetP2SHSigOpCount(const CTransaction& tx, const CCoinsViewCache& mapInputs);

/**
 * Compute total signature operation cost of a transaction.
 * @param[in] tx     Transaction for which we are computing the cost
 * @param[in] inputs Map of previous transactions that have outputs we're spending
 * @param[out] flags Script verification flags
 * @return Total signature operation cost of tx
 */
int64_t GetTransactionSigOpCost(const CTransaction& tx, const CCoinsViewCache& inputs, int flags);

/**
 * Check if transaction is final and can be included in a block with the
 * specified height and time. Consensus critical.
 */
bool IsFinalTx(const CTransaction &tx, int nBlockHeight, int64_t nBlockTime);

/**
 * Calculates the block height and previous block's median time past at
 * which the transaction will be considered final in the context of BIP 68.
 * Also removes from the vector of input heights any entries which did not
 * correspond to sequence locked inputs as they do not affect the calculation.
 */
std::pair<int, int64_t> CalculateSequenceLocks(const CTransaction &tx, int flags, std::vector<int>* prevHeights, const CBlockIndex& block);

bool EvaluateSequenceLocks(const CBlockIndex& block, std::pair<int, int64_t> lockPair);
/**
 * Check if transaction is final per BIP 68 sequence numbers and can be included in a block.
 * Consensus critical. Takes as input a list of heights at which tx's inputs (in order) confirmed.
 */
bool SequenceLocks(const CTransaction &tx, int flags, std::vector<int>* prevHeights, const CBlockIndex& block);

#endif
