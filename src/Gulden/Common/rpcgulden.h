#ifndef GULDEN_RPC_H
#define GULDEN_RPC_H

#include "json/json_spirit_utils.h"
#include "json/json_spirit_value.h"

// Get the hash per second that we are mining at.
json_spirit::Value gethashps(const json_spirit::Array& params, bool fHelp);

// Set the maximum hash per second to allow when mining.
json_spirit::Value sethashlimit(const json_spirit::Array& params, bool fHelp);

// Dump c++ code for old_diff.h that can be used to verify that the code has not been tampered with.
json_spirit::Value dumpdiffarray(const json_spirit::Array& params, bool fHelp);

#endif
