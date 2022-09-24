// Copyright (c) 2016-2022 The Centure developers
// Authored by: Willem de Jonge (willem@isnapp.nl)
// Modified by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the Libre Chain License, see the accompanying
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

        //! Wallet errors
        RPC_WALLET_ERROR                = -4,  //!< Unspecified problem with wallet (key not found etc.)
        RPC_WALLET_INSUFFICIENT_FUNDS   = -6,  //!< Not enough funds in wallet or account
        RPC_WALLET_INVALID_ACCOUNT_NAME = -11, //!< Invalid account name
        RPC_WALLET_KEYPOOL_RAN_OUT      = -12, //!< Keypool ran out, call keypoolrefill first
        RPC_WALLET_UNLOCK_NEEDED        = -13, //!< Enter the wallet passphrase with walletpassphrase first
        RPC_WALLET_PASSPHRASE_INCORRECT = -14, //!< The wallet passphrase entered was incorrect
        RPC_WALLET_WRONG_ENC_STATE      = -15, //!< Command given in wrong wallet encryption state (encrypting an encrypted wallet etc.)
        RPC_WALLET_ENCRYPTION_FAILED    = -16, //!< Failed to encrypt the wallet
        RPC_WALLET_ALREADY_UNLOCKED     = -17, //!< Wallet is already unlocked
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

typedef std::vector<std::tuple<CTxOut, uint64_t, uint64_t, COutPoint>> witnessOutputsInfoVector;

// throw on failure
void extendwitnessaccount(CWallet* pwallet, CAccount* fundingAccount, CAccount* witnessAccount, CAmount amount, uint64_t requestedLockPeriodInBlocks, std::string* pTxid, CAmount* pFee);
void extendwitnessaddresshelper(CAccount* fundingAccount, witnessOutputsInfoVector unspentWitnessOutputs, CWallet* pwallet, CAmount requestedAmount, uint64_t requestedLockPeriodInBlocks, std::string* pTxid, CAmount* pFee);
void renewwitnessaccount(CWallet* pwallet, CAccount* fundingAccount, CAccount* witnessAccount, std::string* pTxid, CAmount* pFee);
void fundwitnessaccount(CWallet* pwallet, CAccount* fundingAccount, CAccount* witnessAccount, CAmount amount, uint64_t requestedPeriodInBlocks, bool fAllowMultiple, std::string* pTxid, CAmount* pFee);
void fundwitnessaccount(CWallet* pwallet, CAccount* fundingAccount, CAccount* witnessAccount, const std::vector<CAmount>& amounts, uint64_t requestedPeriodInBlocks, bool fAllowMultiple, std::string* pTxid, CAmount* pFee);
void rotatewitnessaccount(CWallet* pwallet, CAccount* fundingAccount, CAccount* witnessAccount, std::string* pTxid, CAmount* pFee);
void rotatewitnessaddresshelper(CAccount* fundingAccount, witnessOutputsInfoVector unspentWitnessOutputs, CWallet* pwallet, std::string* pTxid, CAmount* pFee);
void redistributewitnessaccount(CWallet* pwallet, CAccount* fundingAccount, CAccount* witnessAccount, const std::vector<CAmount>& redistributionAmounts, std::string* pTxid, CAmount* pFee);

struct CGetWitnessInfo;

enum class WitnessStatus {
    Empty,
    EmptyWithRemainder,
    Pending,
    Witnessing,
    Ended,
    Expired,
    Emptying
};

CGetWitnessInfo GetWitnessInfoWrapper();

struct CWitnessAccountStatus
{
    CAccount* account;
    WitnessStatus status;
    uint64_t networkWeight;
    uint64_t currentWeight;
    uint64_t originWeight;
    CAmount currentAmountLocked;
    CAmount originAmountLocked;
    bool hasScriptLegacyOutput;
    bool hasUnconfirmedWittnessTx;
    uint64_t nLockFromBlock;
    uint64_t nLockUntilBlock;
    uint64_t nLockPeriodInBlocks;
    std::vector<uint64_t> parts; // individual weights of all parts
};

/** Get account witness status and accompanying details
 * hasScriptLegacyOutput IFF any of the outputs is CTxOutType::ScriptLegacyOutput
 * hasUnconfirmedWittnessTx IFF unconfirmed witness tx for the account (not actually checked for witness type, see implementation note)
 * pWitnessInfo if != nullptr it will be filled with the witnessInfo if available
*/
CWitnessAccountStatus GetWitnessAccountStatus(CWallet* pWallet, CAccount* account, CGetWitnessInfo* pWitnessInfo = nullptr);

witnessOutputsInfoVector getCurrentOutputsForWitnessAccount(CAccount* forAccount);

bool isWitnessDistributionNearOptimal(CWallet* pWallet, CAccount* account, const CGetWitnessInfo& witnessInfo);
uint64_t adjustedWeightForAmount(const CAmount amount, const uint64_t nHeight, const uint64_t duration, uint64_t networkWeight);
std::tuple<std::vector<CAmount>, uint64_t, CAmount> witnessDistribution(CWallet* pWallet, CAccount* account);
std::vector<CAmount> optimalWitnessDistribution(CAmount totalAmount, uint64_t duration, uint64_t totalWeight);
uint64_t combinedWeight(const std::vector<CAmount> amounts, uint64_t nHeight, uint64_t duration);
double witnessFraction(const std::vector<CAmount>& amounts, uint64_t nHeight, const uint64_t duration, const uint64_t totalWeight);
std::string witnessAddressForAccount(CWallet* pWallet, CAccount* account);
std::string witnessKeysLinkUrlForAccount(CWallet* pWallet, CAccount* account);

#endif // WITNESS_OPERATIONS_H
