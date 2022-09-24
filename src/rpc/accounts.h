// Copyright (c) 2015-2022 The Centure developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the Libre Chain License, see the accompanying
// file COPYING

#ifndef RPC_ACCOUNTS_H
#define RPC_ACCOUNTS_H

#include <univalue.h>

// Get the hash per second that we are mining at.
UniValue gethashps(const UniValue& params, bool fHelp);

// Set the maximum hash per second to allow when mining.
UniValue sethashlimit(const UniValue& params, bool fHelp);

// Dump c++ code for old_diff.h that can be used to verify that the code has not been tampered with.
UniValue dumpdiffarray(const UniValue& params, bool fHelp);

#endif
