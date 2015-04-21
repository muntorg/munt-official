#ifndef GULDENCOIN_RPC_H
#define GULDENCOIN_RPC_H

#include "json/json_spirit_utils.h"
#include "json/json_spirit_value.h"

// Get the hash per second that we are mining at.
json_spirit::Value gethashps(const json_spirit::Array& params, bool fHelp);

// Set the maximum hash per second to allow when mining.
json_spirit::Value sethashlimit(const json_spirit::Array& params, bool fHelp);

#endif