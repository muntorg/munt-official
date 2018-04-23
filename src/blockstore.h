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

//fixme: (GULDEN) (2.1) methods to support conversion of 1.6 block storage to 2.0 can be reverted once not needed anymore
// revert the changes in this commit for blockstore.h + .cpp back to 517362d123ce7c7e83de86a962ad099abbf199b0

#include <stdio.h>
#include "chain.h"
#include "protocol.h" // For CMessageHeader::MessageStartChars
#include "undo.h"

class CBlockStore
{
public:
    CBlockStore(bool legacy=false) : isLegacy(legacy) {}

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

    // block store format conversion support methods:

    /** Close all open files rename with prefix and use those */
    bool Rename(const std::string& prefix);

    /** Delete all block and undo files */
    bool Delete();


private:
    enum class BlockFileType { block, undo };
    fs::path GetBlockPosFilename(const CDiskBlockPos &pos, BlockFileType fileType);
    FILE* GetDiskFile(const CDiskBlockPos &pos, BlockFileType fileType, bool fNoCreate);

    struct BlockFilePair {
        FILE* blockfile = nullptr;
        FILE* undofile = nullptr;
        BlockFilePair() : blockfile(nullptr), undofile(nullptr) {}

        BlockFilePair(BlockFilePair&& other) :
            blockfile(other.blockfile),
            undofile(other.undofile)
        {
            other.blockfile = nullptr;
            other.undofile = nullptr;
        }

        ~BlockFilePair() {
            if (blockfile)
                fclose(blockfile);
            if (undofile)
                fclose(undofile);
        }
    };

    std::vector<BlockFilePair> vBlockfiles;

    // more block store format conversion support:
    fs::path GetBlockPosNewFilename(const CDiskBlockPos &pos, BlockFileType fileType, const std::string& newPrefix);
    bool isLegacy;
    std::string mainPrefix;
};

extern CBlockStore blockStore;

#endif // BLOCKSTORE_H
