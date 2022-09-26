// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SCRIPT_CONSENSUS_H
#define SCRIPT_CONSENSUS_H

#include <stdint.h>

#if defined(BUILD_INTERNAL) && defined(HAVE_CONFIG_H)
#include "config/build-config.h"
  #if defined(_WIN32)
    #if defined(DLL_EXPORT)
      #if defined(HAVE_FUNC_ATTRIBUTE_DLLEXPORT)
        #define EXPORT_SYMBOL __declspec(dllexport)
      #else
        #define EXPORT_SYMBOL
      #endif
    #endif
  #elif defined(HAVE_FUNC_ATTRIBUTE_VISIBILITY)
    #define EXPORT_SYMBOL __attribute__ ((visibility ("default")))
  #endif
#elif defined(MSC_VER) && !defined(STATIC_LIBSCRIPTCONSENSUS)
  #define EXPORT_SYMBOL __declspec(dllimport)
#endif

#ifndef EXPORT_SYMBOL
  #define EXPORT_SYMBOL
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define SCRIPTCONSENSUS_API_VER 1

typedef enum script_consensus_error_t
{
    SCRIPT_CONSENSUS_ERR_OK = 0,
    SCRIPT_CONSENSUS_ERR_TX_INDEX,
    SCRIPT_CONSENSUS_ERR_TX_SIZE_MISMATCH,
    SCRIPT_CONSENSUS_ERR_TX_DESERIALIZE,
    SCRIPT_CONSENSUS_ERR_AMOUNT_REQUIRED,
    SCRIPT_CONSENSUS_ERR_INVALID_FLAGS,
} scriptconsensus_error;


/** Script verification flags */
enum
{
    SCRIPT_CONSENSUS_SCRIPT_FLAGS_VERIFY_NONE                = 0,
    SCRIPT_CONSENSUS_SCRIPT_FLAGS_VERIFY_P2SH                = (1U << 0), // evaluate P2SH (BIP16) subscripts
    SCRIPT_CONSENSUS_SCRIPT_FLAGS_VERIFY_DERSIG              = (1U << 2), // enforce strict DER (BIP66) compliance
    SCRIPT_CONSENSUS_SCRIPT_FLAGS_VERIFY_CHECKLOCKTIMEVERIFY = (1U << 8), // enable CHECKLOCKTIMEVERIFY (BIP65)
    SCRIPT_CONSENSUS_SCRIPT_FLAGS_VERIFY_CHECKSEQUENCEVERIFY = (1U << 9), // enable CHECKSEQUENCEVERIFY (BIP112)
    SCRIPT_CONSENSUS_SCRIPT_FLAGS_VERIFY_WITNESS             = (1U << 13), // enable WITNESS (BIP141)
    SCRIPT_CONSENSUS_SCRIPT_FLAGS_VERIFY_ALL                 = SCRIPT_CONSENSUS_SCRIPT_FLAGS_VERIFY_P2SH | SCRIPT_CONSENSUS_SCRIPT_FLAGS_VERIFY_DERSIG |
                                                               SCRIPT_CONSENSUS_SCRIPT_FLAGS_VERIFY_CHECKLOCKTIMEVERIFY |
                                                               SCRIPT_CONSENSUS_SCRIPT_FLAGS_VERIFY_CHECKSEQUENCEVERIFY | SCRIPT_CONSENSUS_SCRIPT_FLAGS_VERIFY_WITNESS
};

/// Returns 1 if the input nIn of the serialized transaction pointed to by
/// txTo correctly spends the scriptPubKey pointed to by scriptPubKey under
/// the additional constraints specified by flags.
/// If not NULL, err will contain an error/success code for the operation
EXPORT_SYMBOL int script_consensus_verify_script(const unsigned char *scriptPubKey, unsigned int scriptPubKeyLen,
                                                 const unsigned char *txTo        , unsigned int txToLen,
                                                 unsigned int nIn, unsigned int flags, scriptconsensus_error* err);

EXPORT_SYMBOL int script_consensus_verify_script_with_amount(const unsigned char *scriptPubKey, unsigned int scriptPubKeyLen, int64_t amount,
                                    const unsigned char *txTo        , unsigned int txToLen,
                                    unsigned int nIn, unsigned int flags, scriptconsensus_error* err);

EXPORT_SYMBOL unsigned int script_consensus_version();

#ifdef __cplusplus
} // extern "C"
#endif

#undef EXPORT_SYMBOL

#endif
