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

#ifndef GULDEN_UNDO_H
#define GULDEN_UNDO_H

#include "coins.h"
#include "compressor.h" 
#include "consensus/consensus.h"
#include "primitives/transaction.h"
#include "serialize.h"

static const int SERIALIZE_TXUNDO_LEGACY_COMPRESSION = 0x40000000;

/** Undo information for a CTxIn
 *
 *  Contains the prevout's CTxOut being spent, and its metadata as well
 *  (coinbase or not, height). The serialization contains a dummy value of
 *  zero. This is be compatible with older versions which expect to see
 *  the transaction version there.
 */
class TxInUndoSerializer
{
    const Coin* txout;

public:
    template<typename Stream>
    void Serialize(Stream &s) const {
        uint32_t code = (txout->nHeight<<2) | (txout->fSegSig << 1) | txout->fCoinBase;
        ::Serialize(s, VARINT(code));
        txout->out.WriteToStream(s, (txout->fSegSig ? CTransaction::SEGSIG_ACTIVATION_VERSION : CTransaction::SEGSIG_ACTIVATION_VERSION-1));
    }
    TxInUndoSerializer(const Coin* coin) : txout(coin) {}
};

class TxInUndoDeserializer
{
    Coin* txout;

public:
    template<typename Stream>
    void Unserialize(Stream &s) {
        uint32_t nCode = 0;
        if (s.GetVersion() & SERIALIZE_TXUNDO_LEGACY_COMPRESSION)
        {
            ::Unserialize(s, VARINT(nCode));
            txout->nHeight = nCode / 2;
            txout->fCoinBase = nCode & 1;
            txout->fSegSig = 0;
            if (txout->nHeight > 0) {
                // Old versions stored the version number for the last spend of
                // a transaction's outputs. Non-final spends were indicated with
                // height = 0.
                int nVersionDummy;
                ::Unserialize(s, VARINT(nVersionDummy));
            }
        }
        else
        {
            ::Unserialize(s, VARINT(nCode));
            txout->nHeight   = ( ((nCode & 0b11111111111111111111111111111100) >> 2));
            txout->fSegSig   = (  (nCode & 0b00000000000000000000000000000010)  > 0 );
            txout->fCoinBase = (  (nCode & 0b00000000000000000000000000000001)  > 0 );
        }

        //fixme: (2.1) CBSU - possibly remove this if statement by just duplicating code for the legacy case
        // (Or eventually just drop the legacy case)
        if (s.GetVersion() & SERIALIZE_TXUNDO_LEGACY_COMPRESSION)
        {
            ::Unserialize(s, REF(CTxOutCompressorLegacy(REF(txout->out))));
        }
        else
        {
            txout->out.ReadFromStream(s, (txout->fSegSig ? CTransaction::SEGSIG_ACTIVATION_VERSION : CTransaction::SEGSIG_ACTIVATION_VERSION-1));
        }
    }

    TxInUndoDeserializer(Coin* coin) : txout(coin) {}
};

//fixme: (2.1) This can potentially be improved.
//6 is the lower bound for the size of a SegSign txin
static const size_t MAX_INPUTS_PER_BLOCK = MAX_BLOCK_BASE_SIZE / 6; // TODO: merge with similar definition in undo.h.

/** Undo information for a CTransaction */
class CTxUndo
{
public:
    // undo information for all txins
    std::vector<Coin> vprevout;

    template <typename Stream>
    void Serialize(Stream& s) const {
        // TODO: avoid reimplementing vector serializer
        uint64_t count = vprevout.size();
        ::Serialize(s, COMPACTSIZE(REF(count)));
        for (const auto& prevout : vprevout) {
            ::Serialize(s, REF(TxInUndoSerializer(&prevout)));
        }
    }

    template <typename Stream>
    void Unserialize(Stream& s) {
        // TODO: avoid reimplementing vector deserializer
        uint64_t count = 0;
        ::Unserialize(s, COMPACTSIZE(count));
        if (count > MAX_INPUTS_PER_BLOCK) {
            throw std::ios_base::failure("Too many input undo records");
        }
        vprevout.resize(count);
        for (auto& prevout : vprevout) {
            ::Unserialize(s, REF(TxInUndoDeserializer(&prevout)));
        }
    }
};

/** Undo information for a CBlock */
class CBlockUndo
{
public:
    std::vector<CTxUndo> vtxundo; // for all but the coinbase

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITECOMPACTSIZEVECTOR(vtxundo);
    }
};

#endif // GULDEN_UNDO_H
