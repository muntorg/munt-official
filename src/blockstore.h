// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// File contains modifications by: The Gulden developers
// All modifications:
// Copyright (c) 2018 The Gulden developers
// Authored by: Willem de Jonge (willem@isnapp.nl)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#ifndef BLOCKSTORE_H
#define BLOCKSTORE_H

#include <stdio.h>
#include "chain.h"
#include "protocol.h" // For CMessageHeader::MessageStartChars
#include "undo.h"

bool BlockFileExists(const CDiskBlockPos &pos);

/** Open a block file (blk?????.dat), creating it if needed.
    Ownership is NOT transferred, so do not close the file.
*/
FILE* GetBlockFile(const CDiskBlockPos &pos, bool fNoCreate = false);

/** Open an undo file (rev?????.dat), creating it if needed.
    Ownership is NOT transferred, so do not close the file.
*/
FILE* GetUndoFile(const CDiskBlockPos &pos, bool fNoCreate = false);

/** Closes all open block and undo files */
void CloseBlockFiles();


bool WriteBlockToDisk(const CBlock& block, CDiskBlockPos& pos, const CMessageHeader::MessageStartChars& messageStart);
bool ReadBlockFromDisk(CBlock& block, const CDiskBlockPos& pos, const Consensus::Params& consensusParams);

bool UndoWriteToDisk(const CBlockUndo& blockundo, CDiskBlockPos& pos, const uint256& hashBlock, const CMessageHeader::MessageStartChars& messageStart);
bool UndoReadFromDisk(CBlockUndo& blockundo, const CDiskBlockPos& pos, const uint256& hashBlock);

/**
 *  Actually unlink the specified files
 */
void UnlinkPrunedFiles(const std::set<int>& setFilesToPrune);

#endif // BLOCKSTORE_H
