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

#include "blockstore.h"
#include "streams.h"
#include "clientversion.h"

CBlockStore blockStore;

fs::path CBlockStore::GetBlockPosFilename(const CDiskBlockPos &pos, BlockFileType fileType)
{
    const char *prefix = fileType == BlockFileType::block ? "blk" : "rev";
    return GetDataDir() / "blocks" / strprintf("%s%05u.dat", prefix, pos.nFile);
}

bool CBlockStore::BlockFileExists(const CDiskBlockPos &pos)
{
    return fs::exists(GetBlockPosFilename(pos, BlockFileType::block));
}

FILE* CBlockStore::GetDiskFile(const CDiskBlockPos &pos, BlockFileType fileType, bool fNoCreate)
{
    if (pos.IsNull())
        return NULL;

    if (int(vBlockfiles.size()) <= pos.nFile) {
        vBlockfiles.resize(pos.nFile + 1);
    }

    FILE*& file = fileType == BlockFileType::block ? vBlockfiles[pos.nFile].blockfile
                                                    : vBlockfiles[pos.nFile].undofile;
    if (!file) {
        fs::path path = GetBlockPosFilename(pos, fileType);
        fs::create_directories(path.parent_path());
        file = fsbridge::fopen(path, "rb+");
        if (!file && !fNoCreate)
            file = fsbridge::fopen(path, "wb+");
        if (!file) {
            LogPrintf("Unable to open file %s\n", path.string());
            return NULL;
        }
    }

    // always fseek, when file will stay open we may switch writing and reading
    // which has to be interleaved with positioning function
    // also a previous call left the position not where we want it
    if (fseek(file, pos.nPos, SEEK_SET)) {
        LogPrintf("Unable to seek to position %u of %s\n", pos.nPos, GetBlockPosFilename(pos, fileType).string());
        fclose(file);
        file = nullptr;
        return NULL;
    }

    return file;
}

FILE* CBlockStore::GetBlockFile(const CDiskBlockPos &pos, bool fNoCreate) {
    return GetDiskFile(pos, BlockFileType::block, fNoCreate);
}

FILE* CBlockStore::GetUndoFile(const CDiskBlockPos &pos, bool fNoCreate) {
    return GetDiskFile(pos, BlockFileType::undo, fNoCreate);
}

void CBlockStore::CloseBlockFiles()
{
    vBlockfiles.clear();
    LogPrintStr("Block and undo files closed\n");
}

bool CBlockStore::WriteBlockToDisk(const CBlock& block, CDiskBlockPos& pos, const CMessageHeader::MessageStartChars& messageStart)
{
    // Open history file to append
    CFile fileout(GetBlockFile(pos), SER_DISK, CLIENT_VERSION);
    if (fileout.IsNull())
        return error("WriteBlockToDisk: OpenBlockFile failed");

    // Write index header
    unsigned int nSize = ::GetSerializeSize(fileout, block);

    fileout << FLATDATA(messageStart) << nSize;

    // Write block
    long fileOutPos = ftell(fileout.Get());
    if (fileOutPos < 0)
        return error("WriteBlockToDisk: ftell failed");
    pos.nPos = (unsigned int)fileOutPos;
    fileout << block;

    return true;
}

bool CBlockStore::ReadBlockFromDisk(CBlock& block, const CDiskBlockPos& pos, const Consensus::Params& consensusParams)
{
    block.SetNull();

    // Open history file to read
    CFile filein(GetBlockFile(pos, true), SER_DISK, CLIENT_VERSION);
    if (filein.IsNull())
        return error("ReadBlockFromDisk: OpenBlockFile failed for %s", pos.ToString());

    // Read block
    try {
        filein >> block;
    }
    catch (const std::exception& e) {
        return error("%s: Deserialize or I/O error - %s at %s", __func__, e.what(), pos.ToString());
    }

    // Check the header
    if (!CheckProofOfWork(block.GetPoWHash(), block.nBits, consensusParams))
        return error("ReadBlockFromDisk: Errors in block header at %s", pos.ToString());

    block.fPOWChecked = true;

    return true;
}

bool CBlockStore::UndoWriteToDisk(const CBlockUndo& blockundo, CDiskBlockPos& pos, const uint256& hashBlock, const CMessageHeader::MessageStartChars& messageStart)
{
    // Open history file to append
    CFile fileout(GetUndoFile(pos), SER_DISK, CLIENT_VERSION);
    if (fileout.IsNull())
        return error("%s: OpenUndoFile failed", __func__);

    // Write index header
    unsigned int nSize = ::GetSerializeSize(fileout, blockundo);
    fileout << FLATDATA(messageStart) << nSize;

    // Write undo data
    long fileOutPos = ftell(fileout.Get());
    if (fileOutPos < 0)
        return error("%s: ftell failed", __func__);
    pos.nPos = (unsigned int)fileOutPos;
    fileout << blockundo;

    // calculate & write checksum
    CHashWriter hasher(SER_GETHASH, PROTOCOL_VERSION);
    hasher << hashBlock;
    hasher << blockundo;
    fileout << hasher.GetHash();

    return true;
}

bool CBlockStore::UndoReadFromDisk(CBlockUndo& blockundo, const CDiskBlockPos& pos, const uint256& hashBlock)
{
    // Open history file to read
    CFile filein(GetUndoFile(pos, true), SER_DISK, CLIENT_VERSION);
    if (filein.IsNull())
        return error("%s: OpenUndoFile failed", __func__);

    // Read block
    uint256 hashChecksum;
    CHashVerifier<CAutoFile> verifier(&filein); // We need a CHashVerifier as reserializing may lose data
    try {
        verifier << hashBlock;
        verifier >> blockundo;
        filein >> hashChecksum;
    }
    catch (const std::exception& e) {
        return error("%s: Deserialize or I/O error - %s", __func__, e.what());
    }

    // Verify checksum
    if (hashChecksum != verifier.GetHash())
        return error("%s: Checksum mismatch", __func__);

    return true;
}

void CBlockStore::UnlinkPrunedFiles(const std::set<int>& setFilesToPrune)
{
    for (std::set<int>::iterator it = setFilesToPrune.begin(); it != setFilesToPrune.end(); ++it) {
        int nFile = *it;
        if (nFile >= int(vBlockfiles.size()))
            break;
        BlockFilePair& p = vBlockfiles[nFile];
        if (p.blockfile) {
            fclose(p.blockfile);
            p.blockfile = nullptr;
        }
        if (p.undofile) {
            fclose(p.undofile);
            p.undofile = nullptr;
        }
        CDiskBlockPos pos(nFile, 0);
        fs::remove(GetBlockPosFilename(pos, BlockFileType::block));
        fs::remove(GetBlockPosFilename(pos, BlockFileType::undo));
        LogPrintf("Prune: %s deleted blk/rev (%05u)\n", __func__, *it);
    }
}
