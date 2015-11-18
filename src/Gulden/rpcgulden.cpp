#include "rpcgulden.h"
#include "../miner.h"
#include "rpcserver.h"
#include "main.h"

#include <boost/assign/list_of.hpp>

using namespace json_spirit;

#ifdef ENABLE_WALLET
Value gethashps(const Array& params, bool fHelp)
{
    if (fHelp)
        throw std::runtime_error(
            "gethashps\n"
            "\nReturns the estimated hashes per second that this computer is mining at.\n");

    if (dHashesPerSec > 1000000)
        return strprintf("%lf mh/s (%lf)", dHashesPerSec/1000000, dBestHashesPerSec/1000000);
    else if (dHashesPerSec > 1000)
        return strprintf("%lf kh/s (%ls)", dHashesPerSec/1000, dBestHashesPerSec/1000);
    else
        return strprintf("%lf h/s (%lf)", dHashesPerSec, dBestHashesPerSec);
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

    LogPrintf("<DELTA> hash throttle %ld\n", nHashThrottle);

    return nHashThrottle;
}

json_spirit::Value dumpdiffarray(const json_spirit::Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw std::runtime_error(
            "dumpdiffarray  ( height )\n"
            "\nDump code for a c++ array containing 'height' integers, where each integer represents the difficulty (nBits) of a block.\n"
            "\nThis mainly exists for testing and development purposes, and can be used to help verify that your client has not been tampered with.\n"
            "\nArguments:\n"
            "1. height     (numeric) The number of blocks to add to the array.\n"
            "\nExamples:\n"
            + HelpExampleCli("dumpdiffarray 260000", ""));

    RPCTypeCheck(params, boost::assign::list_of(int_type));

    std::string reverseOutBuffer;
    std::string scratchBuffer;
    int nNumToOutput = params[0].get_int();
    reverseOutBuffer.reserve(16*nNumToOutput);

    CBlockIndex* pBlock = chainActive.Tip();
    while(pBlock->pprev && pBlock->nHeight>nNumToOutput)
        pBlock = pBlock->pprev;

    int count=1;
    while(pBlock)
    {
        --nNumToOutput;
        scratchBuffer = itostr(pBlock->nBits);
        std::reverse(scratchBuffer.begin(), scratchBuffer.end());
        if (count!= 1)
            reverseOutBuffer += " ,";
        reverseOutBuffer += scratchBuffer;
        if (count%10 == 0)
            reverseOutBuffer += "        \n";
        pBlock = pBlock->pprev;
        count=count+1;
    }

    std::reverse(reverseOutBuffer.begin(), reverseOutBuffer.end());
    LogPrintf("%s",reverseOutBuffer);

    return reverseOutBuffer;
}

#endif


