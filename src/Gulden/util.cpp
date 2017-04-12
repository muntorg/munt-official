// Copyright (c) 2015-2017 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#include "util.h"
#include "wallet/wallet.h"
#include "main.h"

void rescanThread()
{
    pwalletMain->ScanForWalletTransactions(chainActive.Genesis(), true);
}
