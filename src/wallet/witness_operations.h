// Copyright (c) 2016-2019 The Gulden developers
// Authored by: Willem de Jonge (willem@isnapp.nl)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#ifndef WITNESS_OPERATIONS_H
#define WITNESS_OPERATIONS_H

#include "amount.h"
#include <string>
#include <cstdint>
#include "primitives/transaction.h"

class CWallet;
class CAccount;

// Error codes match exaclty with the codes used in rpc (plain copy)
// This is now local to the witness operations but perhaps it could be promotoed to application wide
// errors at some point. Scoped in namespace here to prevent collision and polution.
namespace witness {
    enum ErrorCode {
        //! General application defined errors
        RPC_MISC_ERROR                  = -1,  //!< std::exception thrown in command handling
        RPC_FORBIDDEN_BY_SAFE_MODE      = -2,  //!< Server is in safe mode, and command is not allowed in safe mode
        RPC_TYPE_ERROR                  = -3,  //!< Unexpected type was passed as parameter
        RPC_INVALID_ADDRESS_OR_KEY      = -5,  //!< Invalid address or key
        RPC_OUT_OF_MEMORY               = -7,  //!< Ran out of memory during operation
        RPC_INVALID_PARAMETER           = -8,  //!< Invalid, missing or duplicate parameter
        RPC_DATABASE_ERROR              = -20, //!< Database error
        RPC_DESERIALIZATION_ERROR       = -22, //!< Error parsing or validating structure in raw format
        RPC_VERIFY_ERROR                = -25, //!< General error during transaction or block submission
        RPC_VERIFY_REJECTED             = -26, //!< Transaction or block was rejected by network rules
        RPC_VERIFY_ALREADY_IN_CHAIN     = -27, //!< Transaction already in chain
        RPC_IN_WARMUP                   = -28, //!< Client still warming up
    };
}

class witness_error: public std::runtime_error
{
public:
    explicit witness_error(int c, const std::string& what) : std::runtime_error(what), _code(c) {}

    int code() const { return _code; }

private:
    int _code;
};

// throw on failure
void extendwitnessaccount(CWallet* pwallet, CAccount* fundingAccount, CAccount* witnessAccount, CAmount amount, uint64_t requestedLockPeriodInBlocks, std::string* pTxid, CAmount* pFee);
void extendwitnessaddresshelper(CAccount* fundingAccount, std::vector<std::tuple<CTxOut, uint64_t, COutPoint>> unspentWitnessOutputs, CWallet* pwallet, CAmount requestedAmount, uint64_t requestedLockPeriodInBlocks, std::string* pTxid, CAmount* pFee);

/** Get tuple (locked amount, remaining locking duration, weight, immature witness) with details for witness extending */
std::tuple<CAmount, int64_t, int64_t, bool> extendWitnessInfo(CWallet* pwallet, CAccount* witnessAccount);

#endif // WITNESS_OPERATIONS_H
