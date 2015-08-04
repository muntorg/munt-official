// Copyright (c) 2015 The Guldencoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef GULDENCOIN_DIFF_H
#define GULDENCOIN_DIFF_H

#include <string>

// This macro is required to help with translating the JSON API
#define runtime_error(X) runtime_error(guldencoin_translate(X))

inline void str_replace(std::string& str, const std::string& from, const std::string& to)
{
    size_t start_pos = 0;
    while((start_pos = str.find(from, start_pos)) != std::string::npos)
    {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length();
    }
}

// We use this to replace various 'bitcoin' strings on the fly, so that we have less conflicts to worry about when merging.
// NB!! Changes here must also be made in src/qt/guldencoin/QGuldencoinTranslator.cpp
inline std::string guldencoin_translate(std::string source)
{
    str_replace(source,"bitcoin:","guldencoin:");
    str_replace(source,"bitcoin.conf","guldencoin.conf");
    str_replace(source,"bitcoin-cli","guldencoin-cli");
    str_replace(source,"bitcoin-tx","guldencoin-tx");
    str_replace(source,"bitcoind","guldencoind");
    str_replace(source,"bitcoin","gulden");
    str_replace(source,"Bitcoin","Gulden");
    str_replace(source,"BITCOIN","GULDEN");
    str_replace(source,"BTC","NLG");
    str_replace(source,"btc","nlg");

    return source;
}

#endif
