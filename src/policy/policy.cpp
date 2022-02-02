// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// File contains modifications by: The Gulden developers
// All modifications:
// Copyright (c) 2017-2020 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

// NOTE: This file is intended to be customised by the end user, and includes only local node policy logic

#include "policy/policy.h"

#include "validation/validation.h"
#include "coins.h"
#include "tinyformat.h"
#include "util.h"
#include "utilstrencodings.h"

//fixme: (PHASE5) - we can remove these includes
#include "witnessutil.h"

CAmount GetDustThreshold(const CTxOut& txout, const CFeeRate& dustRelayFeeIn)
{
    /*
    // "Dust" is defined in terms of dustRelayFee,
    // which has units satoshis-per-kilobyte.
    // If you'd pay more than 1/3 in fees
    // to spend something, then we consider it dust.
    */
    // Gulden: IsDust() detection disabled, allows any valid dust to be relayed.
    // The fees imposed on each dust txo is considered sufficient spam deterrant. 
    return 0;
}

bool IsDust(const CTxOut& txout, const CFeeRate& dustRelayFeeIn)
{
    //fixme: (POST-PHASE5) (LOW) - reconsider dust policy.
    //return (txout.nValue < GetDustThreshold(txout, dustRelayFeeIn));
    return false;
}

/**
    * Check transaction inputs to mitigate two
    * potential denial-of-service attacks:
    * 
    * 1. scriptSigs with extra data stuffed into them,
    *    not consumed by scriptPubKey (or P2SH script)
    * 2. P2SH scripts with a crazy number of expensive
    *    CHECKSIG/CHECKMULTISIG operations
    *
    * Why bother? To avoid denial-of-service attacks; an attacker
    * can submit a standard HASH... OP_EQUAL transaction,
    * which will get accepted into blocks. The redemption
    * script can be anything; an attacker could use a very
    * expensive-to-check-upon-redemption script like:
    *   DUP CHECKSIG DROP ... repeated 100 times... OP_1
    */
bool IsStandard(const CScript& scriptPubKey, txnouttype& whichType, const bool segsigEnabled)
{
    std::vector<std::vector<unsigned char> > vSolutions;
    if (!Solver(scriptPubKey, whichType, vSolutions))
        return false;

    if (whichType == TX_MULTISIG)
    {
        unsigned char m = vSolutions.front()[0];
        unsigned char n = vSolutions.back()[0];
        // Support up to x-of-3 multisig txns as standard
        if (n < 1 || n > 3)
            return false;
        if (m < 1 || m > n)
            return false;
    }
    else if (whichType == TX_NULL_DATA && (!fAcceptDatacarrier || scriptPubKey.size() > nMaxDatacarrierBytes))
    {
          return false;
    }

    return whichType != TX_NONSTANDARD;
}

bool IsStandardTx(const CTransaction& tx, std::string& reason, int nPoW2Version, const bool segsigEnabled)
{
    if (tx.nVersion > CTransaction::MAX_STANDARD_VERSION || tx.nVersion < CTransaction::SEGSIG_ACTIVATION_VERSION) {
        reason = "version";
        return false;
    }

    // Extremely large transactions with lots of inputs can cost the network
    // almost as much to process as they cost the sender in fees, because
    // computing signature hashes is O(ninputs*txsize). Limiting transactions
    // to MAX_STANDARD_TX_WEIGHT mitigates CPU exhaustion attacks.
    unsigned int sz = GetTransactionWeight(tx);
    if (sz >= MAX_STANDARD_TX_WEIGHT)
    {
        reason = "tx-size";
        return false;
    }

    if (!IsOldTransactionVersion(tx.nVersion))
    {
        if (tx.flags[HasExtraFlags] != 0)
        {
            reason = "tx-has-extraflags";
        }
        //fixme: (PHASE4POSTREL) (SEGSIG) (LOCKTIME) (SEQUENCE) - Look into this post release
        if (tx.flags[HasLockTime] != 0)
        {
            reason = "tx-has-locktime";
        }
    }

    for(const CTxIn& txin : tx.vin)
    {
        // Biggest 'standard' txin is a 15-of-15 P2SH multisig with compressed
        // keys (remember the 520 byte limit on redeemScript size). That works
        // out to a (15*(33+1))+3=513 byte redeemScript, 513+1+15*(73+1)+3=1627
        // bytes of scriptSig, which we round off to 1650 bytes for some minor
        // future-proofing. That's also enough to spend a 20-of-20
        // CHECKMULTISIG scriptPubKey, though such a scriptPubKey is not
        // considered standard.
        if (txin.scriptSig.size() > 1650)
        {
            reason = "scriptsig-size";
            return false;
        }
        if (!txin.scriptSig.IsPushOnly())
        {
            reason = "scriptsig-not-pushonly";
            return false;
        }
        if (txin.GetType() != CTxInType::CURRENT_TX_IN_TYPE)
        {
            reason = "unsupported-ctxin-type";
        }
        //fixme: (PHASE4POSTREL) (SEGSIG) (LOCKTIME) (SEQUENCE) - Look into this post release
        if (txin.FlagIsSet(HasLock) != 0)
        {
            reason = "txin-has-lock";
        }
    }

    unsigned int nDataOut = 0;
    txnouttype whichType;
    for(const CTxOut& txout : tx.vout)
    {
        switch (txout.GetType())
        {
            case CTxOutType::ScriptLegacyOutput:
            {
                if (!::IsStandard(txout.output.scriptPubKey, whichType, segsigEnabled))
                {
                    reason = "scriptpubkey";
                    return false;
                }
                break;
            }
            case CTxOutType::StandardKeyHashOutput:
                break;
            case CTxOutType::PoW2WitnessOutput:
                break;
            default:
            {
                reason = "unsupported-ctxout-type";
            }
        }

        if (whichType == TX_NULL_DATA)
        {
            nDataOut++;
        }
        else if ((whichType == TX_MULTISIG) && (!fIsBareMultisigStd))
        {
            reason = "bare-multisig";
            return false;
        }
        else if (IsDust(txout, ::dustRelayFee))
        {
            reason = "dust";
            return false;
        }
    }

    // only one OP_RETURN txout is permitted
    if (nDataOut > 1)
    {
        reason = "multi-op-return";
        return false;
    }

    return true;
}

//fixme: (PHASE5) de-dupe
typedef std::vector<unsigned char> valtype;
static CScript PushAll(const std::vector<valtype>& values)
{
    CScript result;
    for(const valtype& v : values) {
        if (v.size() == 0) {
            result << OP_0;
        } else if (v.size() == 1 && v[0] >= 1 && v[0] <= 16) {
            result << CScript::EncodeOP_N(v[0]);
        } else {
            result << v;
        }
    }
    return result;
}

bool AreInputsStandard(const CTransaction& tx, const CCoinsViewCache& mapInputs)
{
    if (tx.IsCoinBase())
        return true; // Coinbases don't use vin normally

    for (unsigned int i = 0; i < tx.vin.size(); i++)
    {
        const CTxOut& prev = mapInputs.AccessCoin(tx.vin[i].GetPrevOut()).out;

        std::vector<std::vector<unsigned char> > vSolutions;
        txnouttype whichType;
        // get the scriptPubKey corresponding to this input:
        if (prev.GetType() <= CTxOutType::ScriptLegacyOutput)
        {
            const CScript& prevScript = prev.output.scriptPubKey;
            if (!Solver(prevScript, whichType, vSolutions))
                return false;

            if (whichType == TX_SCRIPTHASH)
            {
                std::vector<std::vector<unsigned char> > stack;
                // convert the scriptSig into a stack, so we can inspect the redeemScript
                if (IsOldTransactionVersion(tx.nVersion))
                {
                    if (!EvalScript(stack, tx.vin[i].scriptSig, SCRIPT_VERIFY_NONE, BaseSignatureChecker(CKeyID(), CKeyID()), SCRIPT_V1))
                        return false;
                }
                else
                {
                    if (tx.vin[i].scriptSig.size() != 0)
                        return false;
                    CScript scriptSigTemp = PushAll(tx.vin[i].segregatedSignatureData.stack);
                    if (!EvalScript(stack, scriptSigTemp, SCRIPT_VERIFY_NONE, BaseSignatureChecker(CKeyID(), CKeyID()), IsOldTransactionVersion(tx.nVersion) ? SCRIPT_V1 : SCRIPT_V2))
                        return false;
                }
                if (stack.empty())
                    return false;
                CScript subscript(stack.back().begin(), stack.back().end());
                if (subscript.GetSigOpCount(true) > MAX_P2SH_SIGOPS)
                {
                    return false;
                }
            }
        }
    }

    return true;
}

bool IsSegregatedSignatureDataStandard(const CTransaction& tx, const CCoinsViewCache& mapInputs)
{
    //fixme: (PHASE5) - Look into any other restrictions we may want to add here.
    // Don't think there is anything - but check again against bitcoin equivalent (IsWitnessStandard) to be sure
    
    // Old transaction format  has no segregated signature data
    if (IsOldTransactionVersion(tx.nVersion))
    {
        if (tx.HasSegregatedSignatures())
            return false;
        return true;
    }

    static const unsigned int MAX_STANDARD_SEGREGATED_SIGNATURE_STACK_ITEMS = 100;
    static const unsigned int MAX_STANDARD_SEGREGATED_SIGNATURE_STACK_ITEM_SIZE = 80;

    for (uint64_t i=0; i<tx.vin.size(); ++i)
    {
        size_t sizeSegregatedSignatureStack = tx.vin[i].segregatedSignatureData.stack.size();
        if (sizeSegregatedSignatureStack > MAX_STANDARD_SEGREGATED_SIGNATURE_STACK_ITEMS)
            return false;

        for (unsigned int j = 0; j < sizeSegregatedSignatureStack; j++)
        {
            if (tx.vin[i].segregatedSignatureData.stack[j].size() > MAX_STANDARD_SEGREGATED_SIGNATURE_STACK_ITEM_SIZE)
                return false;
        }
    }
    
    return true;
}

CFeeRate incrementalRelayFee = CFeeRate(DEFAULT_INCREMENTAL_RELAY_FEE);
CFeeRate dustRelayFee = CFeeRate(DUST_RELAY_TX_FEE);
unsigned int nBytesPerSigOp = DEFAULT_BYTES_PER_SIGOP;

int64_t GetVirtualTransactionSize(int64_t nWeight, int64_t nSigOpCost)
{
    return (std::max(nWeight, nSigOpCost * nBytesPerSigOp));
}

int64_t GetVirtualTransactionSize(const CTransaction& tx, int64_t nSigOpCost)
{
    return GetVirtualTransactionSize(GetTransactionWeight(tx), nSigOpCost);
}

const uint64_t STANDARD_COINBASE_INPUT_SIZE_DISCOUNT = 60;
const uint64_t STANDARD_COINBASE_SIGOP_COST_DISCOUNT = 1;   
int64_t GetVirtualTransactionSizeDiscounted(int64_t nWeight, uint64_t numCoinbaseInputs, int64_t nSigOpCost)
{
    uint64_t coinbaseInputsToDiscount=(numCoinbaseInputs>2?numCoinbaseInputs-2:numCoinbaseInputs);
    uint64_t weightDiscount = STANDARD_COINBASE_INPUT_SIZE_DISCOUNT * coinbaseInputsToDiscount;
    uint64_t sigOpCostDiscount = STANDARD_COINBASE_SIGOP_COST_DISCOUNT * coinbaseInputsToDiscount;
    uint64_t discountedWeight = nWeight>weightDiscount?nWeight-weightDiscount:nWeight;
    uint64_t discountedSigOps = nSigOpCost>sigOpCostDiscount?nSigOpCost-sigOpCostDiscount:nSigOpCost;
    
    return (std::max(discountedWeight, discountedSigOps * nBytesPerSigOp));
}


int64_t GetVirtualTransactionSizeDiscounted(const CTransaction& tx, uint64_t numCoinbaseInputs, int64_t nSigOpCost)
{
    uint64_t nTxWeight = GetTransactionWeight(tx);    
    return GetVirtualTransactionSizeDiscounted(nTxWeight, numCoinbaseInputs, nSigOpCost);
}
