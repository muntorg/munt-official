// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_SCRIPT_INTERPRETER_H
#define BITCOIN_SCRIPT_INTERPRETER_H

#include "script_error.h"
#include "primitives/transaction.h"

#include <vector>
#include <stdint.h>
#include <string>

class CPubKey;
class CScript;
class CTransaction;
class uint256;

/** Signature hash types/flags */
enum {
    SIGHASH_ALL = 1,
    SIGHASH_NONE = 2,
    SIGHASH_SINGLE = 3,
    SIGHASH_ANYONECANPAY = 0x80,
};

/** Script verification flags */
enum {
    SCRIPT_VERIFY_NONE = 0,

    SCRIPT_VERIFY_P2SH = (1U << 0),

    SCRIPT_VERIFY_STRICTENC = (1U << 1),

    SCRIPT_VERIFY_DERSIG = (1U << 2),

    SCRIPT_VERIFY_LOW_S = (1U << 3),

    SCRIPT_VERIFY_NULLDUMMY = (1U << 4),

    SCRIPT_VERIFY_SIGPUSHONLY = (1U << 5),

    SCRIPT_VERIFY_MINIMALDATA = (1U << 6),

    SCRIPT_VERIFY_DISCOURAGE_UPGRADABLE_NOPS = (1U << 7),

    SCRIPT_VERIFY_CLEANSTACK = (1U << 8),

    SCRIPT_VERIFY_CHECKLOCKTIMEVERIFY = (1U << 9),

    SCRIPT_VERIFY_CHECKSEQUENCEVERIFY = (1U << 10),

    SCRIPT_VERIFY_WITNESS = (1U << 11),

    SCRIPT_VERIFY_DISCOURAGE_UPGRADABLE_WITNESS_PROGRAM = (1U << 12),
};

bool CheckSignatureEncoding(const std::vector<unsigned char>& vchSig, unsigned int flags, ScriptError* serror);

struct PrecomputedTransactionData {
    uint256 hashPrevouts, hashSequence, hashOutputs;

    PrecomputedTransactionData(const CTransaction& tx);
};

enum SigVersion {
    SIGVERSION_BASE = 0,
    SIGVERSION_WITNESS_V0 = 1,
};

uint256 SignatureHash(const CScript& scriptCode, const CTransaction& txTo, unsigned int nIn, int nHashType, const CAmount& amount, SigVersion sigversion, const PrecomputedTransactionData* cache = NULL);

class BaseSignatureChecker {
public:
    virtual bool CheckSig(const std::vector<unsigned char>& scriptSig, const std::vector<unsigned char>& vchPubKey, const CScript& scriptCode, SigVersion sigversion) const
    {
        return false;
    }

    virtual bool CheckLockTime(const CScriptNum& nLockTime) const
    {
        return false;
    }

    virtual bool CheckSequence(const CScriptNum& nSequence) const
    {
        return false;
    }

    virtual ~BaseSignatureChecker() {}
};

class TransactionSignatureChecker : public BaseSignatureChecker {
private:
    const CTransaction* txTo;
    unsigned int nIn;
    const CAmount amount;
    const PrecomputedTransactionData* txdata;

protected:
    virtual bool VerifySignature(const std::vector<unsigned char>& vchSig, const CPubKey& vchPubKey, const uint256& sighash) const;

public:
    TransactionSignatureChecker(const CTransaction* txToIn, unsigned int nInIn, const CAmount& amountIn)
        : txTo(txToIn)
        , nIn(nInIn)
        , amount(amountIn)
        , txdata(NULL)
    {
    }
    TransactionSignatureChecker(const CTransaction* txToIn, unsigned int nInIn, const CAmount& amountIn, const PrecomputedTransactionData& txdataIn)
        : txTo(txToIn)
        , nIn(nInIn)
        , amount(amountIn)
        , txdata(&txdataIn)
    {
    }
    bool CheckSig(const std::vector<unsigned char>& scriptSig, const std::vector<unsigned char>& vchPubKey, const CScript& scriptCode, SigVersion sigversion) const;
    bool CheckLockTime(const CScriptNum& nLockTime) const;
    bool CheckSequence(const CScriptNum& nSequence) const;
};

class MutableTransactionSignatureChecker : public TransactionSignatureChecker {
private:
    const CTransaction txTo;

public:
    MutableTransactionSignatureChecker(const CMutableTransaction* txToIn, unsigned int nInIn, const CAmount& amount)
        : TransactionSignatureChecker(&txTo, nInIn, amount)
        , txTo(*txToIn)
    {
    }
};

bool EvalScript(std::vector<std::vector<unsigned char> >& stack, const CScript& script, unsigned int flags, const BaseSignatureChecker& checker, SigVersion sigversion, ScriptError* error = NULL);
bool VerifyScript(const CScript& scriptSig, const CScript& scriptPubKey, const CScriptWitness* witness, unsigned int flags, const BaseSignatureChecker& checker, ScriptError* serror = NULL);

size_t CountWitnessSigOps(const CScript& scriptSig, const CScript& scriptPubKey, const CScriptWitness* witness, unsigned int flags);

#endif // BITCOIN_SCRIPT_INTERPRETER_H
