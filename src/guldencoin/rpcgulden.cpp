#include "rpcgulden.h"
#include "../miner.h"
#include "rpcserver.h"

#include <boost/assign/list_of.hpp>

using namespace json_spirit;

Value gethashps(const Array& params, bool fHelp)
{
    if (fHelp)
        throw std::runtime_error(
            "gethashps\n"
            "\nReturns the estimated hashes per second that this computer is mining at.\n");

    return dHashesPerSec;
}

Value sethashlimit(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw std::runtime_error(
            "sethashlimit  ( limit )\n"
            "\nSet the maximum number of hashes to calculate per second when mining.\n"
            "\nThis mainly exists for testing purposes but can also be used to limit CPU usage a little.\n"
            "\nArguments:\n"
            "1. limit     (numeric) The number of hashes to allow per second, or -1 to remove limit.\n"
            "\nExamples:\n"
            + HelpExampleCli("sethashlimit 500000", "")
            + HelpExampleRpc("sethashlimit 500000", ""));

    RPCTypeCheck(params, boost::assign::list_of(int_type));

    nHashThrottle = params[0].get_int();
    return nHashThrottle;
}



