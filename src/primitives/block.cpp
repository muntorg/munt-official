// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// File contains modifications by: The Gulden developers
// All modifications:
// Copyright (c) 2016-2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#include "primitives/block.h"

#include "hash.h"
#include "tinyformat.h"
#include "utilstrencodings.h"
#include "crypto/common.h"

uint256 CBlockHeader::GetHashLegacy() const
{
    //if (!cachedHash.IsNull())
        //return cachedHash;

    //cachedHash = SerializeHash(*this);
    //return cachedHash;
    return SerializeHash(*this, SER_GETHASH, SERIALIZE_BLOCK_HEADER_NO_POW2_WITNESS);
}

uint256 CBlockHeader::GetHashPoW2(bool force) const
{
    if (force)
        assert(nVersionPoW2Witness != 0 || nTimePoW2Witness != 0);

    if (nVersionPoW2Witness == 0 || nTimePoW2Witness == 0)
        return SerializeHash(*this, SER_GETHASH, SERIALIZE_BLOCK_HEADER_NO_POW2_WITNESS);

    return SerializeHash(*this, SER_GETHASH, SERIALIZE_BLOCK_HEADER_NO_POW2_WITNESS_SIG);
}

uint256 CBlock::GetPoWHash() const
{
    //if (!cachedPOWHash.IsNull())
        //return cachedPOWHash;

    uint256 hashRet;

    //CBSU - maybe use a static functor or something here instead of having the branch 
    static bool hashCity = IsArgSet("-testnet") ? ( GetArg("-testnet", "")[0] == 'C' ? true : false ) : false;
    if (hashCity)
    {
        arith_uint256 thash;
        hash_city(BEGIN(nVersion), thash);
        hashRet = ArithToUint256(thash);
    }
    else
    {
        char scratchpad[SCRYPT_SCRATCHPAD_SIZE];
        scrypt_1024_1_1_256_sp(BEGIN(nVersion), BEGIN(hashRet), scratchpad);
    }
    //cachedPOWHash = ArithToUint256(thash);
    //return cachedPOWHash;
    return hashRet;
}

std::string CBlock::ToString() const
{
    std::stringstream s;
    s << strprintf("CBlock(hashlegacy=%s, hashpow2=%s, powhash=%s, ver=0x%08x, hashPrevBlock=%s, hashMerkleRoot=%s, nTime=%u, nBits=%08x, nNonce=%u, vtx=%u)\n",
        GetHashLegacy().ToString(),
        GetHashPoW2().ToString(),
        GetPoWHash().ToString().c_str(),
        nVersion,
        hashPrevBlock.ToString(),
        hashMerkleRoot.ToString(),
        nTime, nBits, nNonce,
        vtx.size());
    for (unsigned int i = 0; i < vtx.size(); i++)
    {
        s << "  " << vtx[i]->ToString() << "\n";
    }
    return s.str();
}

int64_t GetBlockWeight(const CBlock& block)
{
    // segsig: block weight = block size, including the size of the segregated signatures - no complicated segwit weighting shenanigans necessary.
    return ::GetSerializeSize(block, SER_NETWORK, PROTOCOL_VERSION);
}
