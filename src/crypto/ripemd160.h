// Copyright (c) 2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CRYPTO_RIPEMD160_H
#define BITCOIN_CRYPTO_RIPEMD160_H

//fixme: (GULDEN) (HIGH) - Not the greatest solution, but silence annoying fortify source warnings for now so that we can see the real warnings
#ifndef NDEBUG
#undef _FORTIFY_SOURCE
#endif

#include <stdint.h>
#include <stdlib.h>

/** A hasher class for RIPEMD-160. */
class CRIPEMD160
{
private:
    uint32_t s[5];
    unsigned char buf[64];
    uint64_t bytes;

public:
    static const size_t OUTPUT_SIZE = 20;

    CRIPEMD160();
    CRIPEMD160& Write(const unsigned char* data, size_t len);
    void Finalize(unsigned char hash[OUTPUT_SIZE]);
    CRIPEMD160& Reset();
};

#endif // BITCOIN_CRYPTO_RIPEMD160_H
