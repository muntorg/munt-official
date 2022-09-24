// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// File contains modifications by: The Centure developers
// All modifications:
// Copyright (c) 2016-2022 The Centure developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the Libre Chain License, see the accompanying
// file COPYING

#ifndef TRANSACTION_UNDO_H
#define TRANSACTION_UNDO_H

#include "coins.h"
#include "compressor.h" 
#include "consensus/consensus.h"
#include "primitives/transaction.h"
#include "serialize.h"

//fixme: (PHASE5) This can potentially be improved.
//6 is the lower bound for the size of a SegSig txin
static const size_t MAX_INPUTS_PER_BLOCK = MAX_BLOCK_BASE_SIZE / 6; // TODO: merge with similar definition in undo.h.

/** Undo information for a CTransaction */
class CTxUndo
{
public:
    // undo information for all txins
    std::vector<CoinUndo> vprevout;

    template <typename Stream>
    void Serialize(Stream& s) const {
        // TODO: avoid reimplementing vector serializer
        uint64_t count = vprevout.size();
        ::Serialize(s, COMPACTSIZE(REF(count)));
        for (const auto& prevout : vprevout) {
            ::Serialize(s, prevout);
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
            ::Unserialize(s, prevout);
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

#endif
