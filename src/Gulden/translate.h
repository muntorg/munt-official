// Copyright (c) 2015-2016 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#ifndef GULDEN_TRANS_H
#define GULDEN_TRANS_H

#include <string>

// This macro is required to help with translating the JSON API
#define runtime_error(X) runtime_error(gulden_translate(X))

inline void str_replace(std::string& str, const std::string& from, const std::string& to)
{
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length();
    }
}

#define GULDEN_TRANSLATIONS_REPLACE(CMD, S) \
    CMD(S, "bitcoin.com", "Gulden.com");    \
    CMD(S, "bitcoin:", "Gulden:");          \
    CMD(S, "bitcoin.conf", "Gulden.conf");  \
    CMD(S, "Gulden-cli", "Gulden-cli");     \
    CMD(S, "Gulden-tx", "Gulden-tx");       \
    CMD(S, "bitcoin-qt", "Gulden");         \
    CMD(S, "GuldenD", "Gulden-daemon");     \
    CMD(S, "bitcoin core", "Gulden");       \
    CMD(S, "Bitcoin Core", "Gulden");       \
    CMD(S, "bitcoin Core", "Gulden");       \
    CMD(S, "Bitcoin core", "Gulden");       \
    CMD(S, "bitcoin", "Gulden");            \
    CMD(S, "Bitcoin", "Gulden");            \
    CMD(S, "BITCOIN", "GULDEN");            \
    CMD(S, "BTC", "NLG");                   \
    CMD(S, "btc", "NLG");                   \
    CMD(S, "guldens", "Gulden");            \
    CMD(S, "Guldens", "Gulden");            \
    CMD(S, "BCOIN", "Bitcoin");             \
    CMD(S, "^NLG", "^BTC");

inline std::string gulden_translate(std::string source)
{
    GULDEN_TRANSLATIONS_REPLACE(str_replace, source);

    return source;
}

#endif
