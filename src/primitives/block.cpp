// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// File contains modifications by: The Gulden developers
// All modifications:
// Copyright (c) 2016 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#include "primitives/block.h"

#include "hash.h"
#include "tinyformat.h"
#include "utilstrencodings.h"
#include "crypto/common.h"

uint256 CBlockHeader::GetHash() const
{
    //if (!cachedHash.IsNull())
    //return cachedHash;

    return SerializeHash(*this);
}

std::string CBlock::ToString() const
{
    std::stringstream s;
    s << strprintf("CBlock(hash=%s, input=%s, PoW=%s, ver=0x%08x, hashPrevBlock=%s, hashMerkleRoot=%s, nTime=%u, nBits=%08x, nNonce=%u, vtx=%u)\n",
                   HexStr(BEGIN(nVersion), BEGIN(nVersion) + 80, false).c_str(),
                   GetPoWHash().ToString().c_str(),
                   GetHash().ToString(),
                   nVersion,
                   hashPrevBlock.ToString(),
                   hashMerkleRoot.ToString(),
                   nTime, nBits, nNonce,
                   vtx.size());
    for (unsigned int i = 0; i < vtx.size(); i++) {
        s << "  " << vtx[i].ToString() << "\n";
    }
    return s.str();
}

uint256 CBlock::GetPoWHash() const
{
    uint256 hashRet;

    static bool hashCity = IsArgSet("-testnet") ? (GetArg("-testnet", "")[0] == 'C' ? true : false) : false;
    if (hashCity) {
        arith_uint256 thash;
        hash_city(BEGIN(nVersion), thash);
        hashRet = ArithToUint256(thash);
    } else {
        char scratchpad[SCRYPT_SCRATCHPAD_SIZE];
        scrypt_1024_1_1_256_sp(BEGIN(nVersion), BEGIN(hashRet), scratchpad);
    }

    return hashRet;
}

int64_t GetBlockWeight(const CBlock& block)
{

    return ::GetSerializeSize(block, SER_NETWORK, PROTOCOL_VERSION | SERIALIZE_TRANSACTION_NO_WITNESS) * (WITNESS_SCALE_FACTOR - 1) + ::GetSerializeSize(block, SER_NETWORK, PROTOCOL_VERSION);
}
