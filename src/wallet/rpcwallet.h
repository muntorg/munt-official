// Copyright (c) 2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// File contains modifications by: The Centure developers
// All modifications:
// Copyright (c) 2016-2022 The Centure developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the Libre Chain License, see the accompanying
// file COPYING

#ifndef WALLET_RPCWALLET_H
#define WALLET_RPCWALLET_H

class CRPCTable;
class CAccount;
class JSONRPCRequest;
class UniValue;

CAccount* AccountFromValue(CWallet* pwallet, const UniValue& value, bool useDefaultIfEmpty);
void RegisterWalletRPCCommands(CRPCTable &t);

/**
 * Figures out what wallet, if any, to use for a JSONRPCRequest.
 *
 * @param[in] request JSONRPCRequest that wishes to access a wallet
 * @return NULL if no wallet should be used, or a pointer to the CWallet
 */
CWallet *GetWalletForJSONRPCRequest(const JSONRPCRequest&);

std::string HelpRequiringPassphrase(CWallet *);
void EnsureWalletIsUnlocked(CWallet *);
bool EnsureWalletIsAvailable(CWallet *, bool avoidException);

#endif
