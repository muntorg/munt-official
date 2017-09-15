// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// File contains modifications by: The Gulden developers
// All modifications:
// Copyright (c) 2016-2017 The Gulden developers
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
    
    //cachedHash = SerializeHash(*this);
    //return cachedHash;
    return SerializeHash(*this);
}

std::string CBlock::ToString() const
{
    std::stringstream s;
    s << strprintf("CBlock(hash=%s, input=%s, PoW=%s, ver=0x%08x, hashPrevBlock=%s, hashMerkleRoot=%s, nTime=%u, nBits=%08x, nNonce=%u, vtx=%u)\n",
        HexStr(BEGIN(nVersion),BEGIN(nVersion)+80,false).c_str(),
        GetPoWHash().ToString().c_str(),
        GetHash().ToString(),
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
    // This implements the weight = (stripped_size * 4) + witness_size formula,
    // using only serialization with and without witness data. As witness_size
    // is equal to total_size - stripped_size, this formula is identical to:
    // weight = (stripped_size * 3) + total_size.
    return ::GetSerializeSize(block, SER_NETWORK, PROTOCOL_VERSION | SERIALIZE_TRANSACTION_NO_WITNESS) * (WITNESS_SCALE_FACTOR - 1) + ::GetSerializeSize(block, SER_NETWORK, PROTOCOL_VERSION);
}

uint256 CBlock::GetPoWHash() const
{
    //if (!cachedPOWHash.IsNull())
        //return cachedPOWHash;

      //CBSU - maybe use a static functor or something here instead of having the branch 
    static bool hashCity = IsArgSet("-testnet") ? ( GetArg("-testnet", "")[0] == 'C' ? true : false ) : false;
    arith_uint256 thash;
    if (hashCity)
    {
        hash_city(BEGIN(nVersion), thash);
    }
    else
    {
        char scratchpad[SCRYPT_SCRATCHPAD_SIZE];
        scrypt_1024_1_1_256_sp(BEGIN(nVersion), BEGIN(thash), scratchpad);
    }
    return ArithToUint256(thash);
}
