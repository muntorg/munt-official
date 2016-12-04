// Copyright (c) 2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// File contains modifications by: The Gulden developers
// All modifications:
// Copyright (c) 2016 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#ifndef BITCOIN_WALLET_RPCWALLET_H
#define BITCOIN_WALLET_RPCWALLET_H

class CRPCTable;
class CAccount;
class UniValue;

CAccount* AccountFromValue(const UniValue& value, bool useDefaultIfEmpty);
void RegisterWalletRPCCommands(CRPCTable &tableRPC);

#endif //BITCOIN_WALLET_RPCWALLET_H
