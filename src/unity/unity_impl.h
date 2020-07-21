// Copyright (c) 2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#ifndef UNITY_IMPL
#define UNITY_IMPL

#include "unified_backend.hpp"
#include "wallet/wallet.h"
#include "transaction_record.hpp"

extern std::shared_ptr<UnifiedFrontend> signalHandler;

extern TransactionRecord calculateTransactionRecordForWalletTransaction(const CWalletTx& wtx, std::vector<CAccount*>& forAccounts);
extern std::vector<TransactionRecord> getTransactionHistoryForAccount(CAccount* forAccount);
extern std::vector<MutationRecord> getMutationHistoryForAccount(CAccount* forAccount);

#endif
