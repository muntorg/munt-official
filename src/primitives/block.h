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

#ifndef GULDEN_PRIMITIVES_BLOCK_H
#define GULDEN_PRIMITIVES_BLOCK_H

#include "primitives/transaction.h"
#include "serialize.h"
#include "uint256.h"
#include "arith_uint256.h"
#include "utilstrencodings.h"
#include "util.h"
#include <Gulden/Common/hash/hash.h>

#define SERIALIZE_BLOCK_HEADER_NO_POW2_WITNESS     0x20000000
#define SERIALIZE_BLOCK_HEADER_NO_POW2_WITNESS_SIG 0x40000000


/** Nodes collect new transactions into a block, hash them into a hash tree,
 * and scan through nonce values to make the block's hash satisfy proof-of-work
 * requirements.  When they solve the proof-of-work, they broadcast the block
 * to everyone and the block is added to the block chain.  The first transaction
 * in the block is a special one that creates a new coin owned by the creator
 * of the block.
 */
class CBlockHeader
{
public:
    // PoW2 Witness header
    int32_t nVersionPoW2Witness;
    uint32_t nTimePoW2Witness;
    uint256 hashMerkleRootPoW2Witness;
    //fixme: (2.1) Optimisation - this is always 65 bits, we should use a fixed size data structure.
    std::vector<unsigned char> witnessHeaderPoW2Sig;


    // PoW header
    int32_t nVersion;
    uint256 hashPrevBlock;
    uint256 hashMerkleRoot;
    uint32_t nTime;
    uint32_t nBits;
    uint32_t nNonce;

    CBlockHeader()
    {
        SetNull();
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        //fixme: (2.1) Remove support for legacy nodes - no longer need this from 2.1 onwards.
        if (!(s.GetVersion() & SERIALIZE_BLOCK_HEADER_NO_POW2_WITNESS))
        {
            READWRITE(nVersionPoW2Witness);
            READWRITE(nTimePoW2Witness);
            READWRITE(hashMerkleRootPoW2Witness);
        }

        READWRITE(this->nVersion);
        READWRITE(hashPrevBlock);
        READWRITE(hashMerkleRoot);
        READWRITE(nTime);
        READWRITE(nBits);
        READWRITE(nNonce);

        //fixme: (2.1) Remove support for legacy nodes - no longer need this from 2.1 onwards.
        if (!(s.GetVersion() & SERIALIZE_BLOCK_HEADER_NO_POW2_WITNESS))
        {
            if (!(s.GetVersion() & SERIALIZE_BLOCK_HEADER_NO_POW2_WITNESS_SIG))
            {
                if (nVersionPoW2Witness != 0)
                {
                    if (ser_action.ForRead())
                        witnessHeaderPoW2Sig.resize(65);
                    READWRITENOSIZEVECTOR(witnessHeaderPoW2Sig);
                }
            }
        }
    }

    void SetNull()
    {
        nVersion = 0;
        hashPrevBlock.SetNull();
        hashMerkleRoot.SetNull();
        nTime = 0;
        nBits = 0;
        nNonce = 0;

        nVersionPoW2Witness = 0;
        nTimePoW2Witness = 0;
        hashMerkleRootPoW2Witness.SetNull();
        witnessHeaderPoW2Sig.clear();
    }

    bool IsNull() const
    {
        return (nBits == 0);
    }

    //fixme: (Post-2.1) SBSU - this gives speed boost
    //but causes issues for mining
    //Disabled out of concern it may cause issues for others as well
    //re-enable once carefully looking into the below assertion.
    //GuldenD: main.cpp:2341: bool ConnectBlock(const CBlock&, CValidationState&, CBlockIndex*, CCoinsViewCache&, const CChainParams&, bool): Assertion `hashPrevBlock == view.GetBestBlock()' failed.
    //mutable uint256 cachedHash;
    uint256 GetHashLegacy() const;

    uint256 GetHashPoW2(bool force=false) const;

    int64_t GetBlockTime() const
    {
        return (int64_t)nTime;
    }
};


class CBlock : public CBlockHeader
{
public:
    // network and disk
    std::vector<CTransactionRef> vtx;

    // memory only
    mutable bool fChecked;
    mutable bool fPOWChecked;

    CBlock()
    {
        SetNull();
    }

    CBlock(const CBlockHeader &header)
    {
        SetNull();
        *((CBlockHeader*)this) = header;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(*(CBlockHeader*)this);
        READWRITECOMPACTSIZEVECTOR(vtx);
    }

    void SetNull()
    {
        CBlockHeader::SetNull();
        vtx.clear();
        fChecked = false;
        fPOWChecked = false;
        //cachedHash = uint256();
        //cachedPOWHash = uint256();
    }

    //mutable uint256 cachedPOWHash;
    uint256 GetPoWHash() const;

    CBlockHeader GetBlockHeader() const
    {
        CBlockHeader block;
        block.nVersion       = nVersion;
        block.hashPrevBlock  = hashPrevBlock;
        block.hashMerkleRoot = hashMerkleRoot;
        block.nTime          = nTime;
        block.nBits          = nBits;
        block.nNonce         = nNonce;
        block.nVersionPoW2Witness = nVersionPoW2Witness;
        block.nTimePoW2Witness = nTimePoW2Witness;
        block.hashMerkleRootPoW2Witness = hashMerkleRootPoW2Witness;
        block.witnessHeaderPoW2Sig = witnessHeaderPoW2Sig;
        return block;
    }

    std::string ToString() const;
};

/** Describes a place in the block chain to another node such that if the
 * other node doesn't have the same branch, it can find a recent common trunk.
 * The further back it is, the further before the fork it may be.
 */
struct CBlockLocator
{
    std::vector<uint256> vHave;

    CBlockLocator() {}

    CBlockLocator(const std::vector<uint256>& vHaveIn) : vHave(vHaveIn) {}

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        int nVersion = s.GetVersion();
        if (!(s.GetType() & SER_GETHASH))
            READWRITE(nVersion);
        READWRITECOMPACTSIZEVECTOR(vHave);
    }

    void SetNull()
    {
        vHave.clear();
    }

    bool IsNull() const
    {
        return vHave.empty();
    }
};

/** Compute the consensus-critical block weight (see BIP 141). */
int64_t GetBlockWeight(const CBlock& tx);

#endif // GULDEN_PRIMITIVES_BLOCK_H
