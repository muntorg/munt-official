#ifndef GULDEN_RPC_H
#define GULDEN_RPC_H

#include <univalue.h>

// Get the hash per second that we are mining at.
UniValue gethashps(const UniValue& params, bool fHelp);

// Set the maximum hash per second to allow when mining.
UniValue sethashlimit(const UniValue& params, bool fHelp);

// Dump c++ code for old_diff.h that can be used to verify that the code has not been tampered with.
UniValue dumpdiffarray(const UniValue& params, bool fHelp);

#endif
