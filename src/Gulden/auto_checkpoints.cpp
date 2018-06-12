// Copyright (c) 2015-2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

// Portions of this code have been copied from PPCoin
// These portions are Copyright (c) 2011-2013 The PPCoin developers
// and are Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "checkpoints.h"
#include "auto_checkpoints.h"

#include "chainparams.h"
#include "validation/validation.h"
#include "uint256.h"
#include "util.h"
#include "key.h"
#include "pubkey.h"
#include "timedata.h"
#include "netmessagemaker.h"

#include <stdint.h>

#include <boost/filesystem/fstream.hpp>
#include "net_processing.h"


// Automatic checkpoint system.
// Based on the checkpoint system developed initially by peercoin, however modified to work for Gulden.

// How many blocks back the checkpoint should run
#define AUTO_CHECKPOINT_DEPTH 4

// ppcoin: synchronized checkpoint (centrally broadcasted)
namespace Checkpoints
{
    uint256 hashSyncCheckpoint = uint256();
    uint256 hashPendingCheckpoint = uint256();
    CSyncCheckpoint checkpointMessage;
    CSyncCheckpoint checkpointMessagePending;
    uint256 hashInvalidCheckpoint = uint256();
    CCriticalSection cs_hashSyncCheckpoint;


    // Get the highest auto synchronized checkpoint that we have received
    CBlockIndex* GetLastSyncCheckpoint()
    {
        LOCK(cs_hashSyncCheckpoint);
        if (!mapBlockIndex.count(hashSyncCheckpoint))
        {
            error("GetSyncCheckpoint: block index missing for current sync-checkpoint %s", hashSyncCheckpoint.ToString().c_str());
        }
        else
        {
            return mapBlockIndex[hashSyncCheckpoint];
        }
        return NULL;
    }

    // Check if an auto synchronized checkpoint we have received is valid
    // If it is on a fork (i.e. not a descendant of the current checkpoint) then we have a problem.
    bool ValidateSyncCheckpoint(uint256 hashCheckpoint)
    {
        if (!mapBlockIndex.count(hashCheckpoint))
        {
            return error("ValidateSyncCheckpoint: block index missing for received sync-checkpoint %s", hashCheckpoint.ToString().c_str());
        }
        if (hashSyncCheckpoint==uint256())
        {
            return true;
        }
        if (!mapBlockIndex.count(hashSyncCheckpoint))
        {
            return error("ValidateSyncCheckpoint: block index missing for current sync-checkpoint %s", hashSyncCheckpoint.ToString().c_str());
        }


        CBlockIndex* pindexSyncCheckpoint = mapBlockIndex[hashSyncCheckpoint];
        CBlockIndex* pindexCheckpointRecv = mapBlockIndex[hashCheckpoint];

        if (pindexCheckpointRecv->nHeight <= pindexSyncCheckpoint->nHeight)
        {
            // Received an older checkpoint, trace back from current checkpoint
            // to the same height of the received checkpoint to verify
            // that current checkpoint should be a descendant block
            CBlockIndex* pindex = pindexSyncCheckpoint;
            while (pindex->nHeight > pindexCheckpointRecv->nHeight)
            {
                if (!(pindex = pindex->pprev))
                    return error("ValidateSyncCheckpoint: pprev1 null - block index structure failure");
            }
            if (pindex->GetBlockHashLegacy() != hashCheckpoint)
            {
                hashInvalidCheckpoint = hashCheckpoint;
                return error("ValidateSyncCheckpoint: new sync-checkpoint %s is conflicting with current sync-checkpoint %s", hashCheckpoint.ToString().c_str(), hashSyncCheckpoint.ToString().c_str());
            }
            return false; // ignore older checkpoint
        }

        // Received checkpoint should be a descendant block of the current
        // checkpoint. Trace back to the same height of current checkpoint
        // to verify.
        CBlockIndex* pindex = pindexCheckpointRecv;
        while (pindex->nHeight > pindexSyncCheckpoint->nHeight)
        {
            if (!(pindex = pindex->pprev))
            {
                return error("ValidateSyncCheckpoint: pprev2 null - block index structure failure");
            }
        }
        if (pindex->GetBlockHashLegacy() != hashSyncCheckpoint)
        {
            hashInvalidCheckpoint = hashCheckpoint;
            return error("ValidateSyncCheckpoint: new sync-checkpoint %s is not a descendant of current sync-checkpoint %s", hashCheckpoint.ToString().c_str(), hashSyncCheckpoint.ToString().c_str());
        }

        return true;
    }

    bool ReadCheckpointPubKey(std::string& strPubKey)
    {
        if( !fs::exists(GetDataDir() / "checkpoints") )
            return false;

        try
        {
            fs::ifstream checkpointFile( GetDataDir() / "checkpoints" / "curr_checkpoint_pubkey" );
            checkpointFile >> strPubKey;
            checkpointFile.close();
        }
        catch (...)
        {
            return false;
        }

        return true;
    }

    bool WriteCheckpointPubKey(std::string& strPubKey)
    {
        try
        {
            //First write to a new file, then overwrite the checkpoint file with a move operation
            //This ensures that the operation happens in an atomic-like fashion and cannot leave us with a corrupted checkpoint file (on most sane filesystems at least)
            //NB! We do not bother to force a disk flush - checkpoints come frequently and it doesn't matter if we are slightly out of date.
            if( !fs::exists(GetDataDir() / "autocheckpoints") )
            {
                if( !fs::create_directory(GetDataDir() / "autocheckpoints") )
                    return false;
            }

            fs::ofstream checkpointFile( GetDataDir() / "autocheckpoints" / "new_checkpoint_pubkey" );
            checkpointFile << strPubKey;
            checkpointFile.close();

            fs::rename( GetDataDir() / "autocheckpoints" / "new_checkpoint_pubkey", GetDataDir() / "autocheckpoints" / "curr_checkpoint_pubkey" );
        }
        catch (...)
        {
            return false;
        }

        return true;
    }

    // Read the current auto sync checkpoint from disk
    bool ReadSyncCheckpoint(uint256& hashCheckpoint)
    {
        if( !fs::exists(GetDataDir() / "autocheckpoints") )
            return false;

        try
        {
            fs::ifstream checkpointFile( GetDataDir() / "autocheckpoints" / "curr_checkpoint" );
            std::string temp;
            checkpointFile >> temp;
            hashCheckpoint = uint256S(temp);
            checkpointFile.close();
        }
        catch (...)
        {
            return false;
        }
        hashSyncCheckpoint = hashCheckpoint;

        return true;
    }

    // Save the current auto sync checkpoint to disk
    bool WriteSyncCheckpoint(const uint256& hashCheckpoint)
    {
        try
        {
            //First write to a new file, then overwrite the checkpoint file with a move operation
            //This ensures that the operation happens in an atomic-like fashion and cannot leave us with a corrupted checkpoint file (on most sane filesystems at least)
            //NB! We do not bother to force a disk flush - checkpoints come frequently and it doesn't matter if we are slightly out of date.
            if( !fs::exists(GetDataDir() / "autocheckpoints") )
            {
                if( !fs::create_directory(GetDataDir() / "autocheckpoints") )
                    return false;
            }

            fs::ofstream checkpointFile( GetDataDir() / "autocheckpoints" / "new_checkpoint" );
            checkpointFile << hashCheckpoint.ToString();
            checkpointFile.close();

            fs::rename( GetDataDir() / "autocheckpoints" / "new_checkpoint", GetDataDir() / "autocheckpoints" / "curr_checkpoint" );
        }
        catch (...)
        {
            return false;
        }
        hashSyncCheckpoint = hashCheckpoint;
        return true;
    }


    bool AcceptPendingSyncCheckpoint(const CChainParams& chainparams)
    {
        LOCK2(cs_main, cs_hashSyncCheckpoint); // cs_main lock required for ReadBlockFromDisk

        if (hashPendingCheckpoint != uint256() && mapBlockIndex.count(hashPendingCheckpoint))
        {
            if (!ValidateSyncCheckpoint(hashPendingCheckpoint))
            {
                hashPendingCheckpoint = uint256();
                checkpointMessagePending.SetNull();
                return false;
            }

            CBlockIndex* pindexCheckpoint = mapBlockIndex[hashPendingCheckpoint];
            if (! (chainActive.Contains(pindexCheckpoint)) )
            {
                CBlock* pblock = new CBlock();
                if (!ReadBlockFromDisk(*pblock, pindexCheckpoint, chainparams.GetConsensus()))
                {
                    return error("AcceptPendingSyncCheckpoint: ReadBlockFromDisk failed for sync checkpoint %s", hashPendingCheckpoint.ToString().c_str());
                }
                CValidationState State;
                if (!ActivateBestChain(State, chainparams, std::shared_ptr<CBlock>(pblock)))
                {
                    hashInvalidCheckpoint = hashPendingCheckpoint;
                    return error("AcceptPendingSyncCheckpoint: SetBestChain failed for sync checkpoint %s", hashPendingCheckpoint.ToString().c_str());
                }
            }

            if (!WriteSyncCheckpoint(hashPendingCheckpoint))
            {
                return error("AcceptPendingSyncCheckpoint(): failed to write sync checkpoint %s", hashPendingCheckpoint.ToString().c_str());
            }
            hashPendingCheckpoint = uint256();
            checkpointMessage = checkpointMessagePending;
            checkpointMessagePending.SetNull();
            LogPrintf("AcceptPendingSyncCheckpoint : sync-checkpoint at %s\n", hashSyncCheckpoint.ToString().c_str());

            // relay the checkpoint
            if (!checkpointMessage.IsNull())
            {
                g_connman->ForEachNode([](CNode* pnode) {
                    checkpointMessage.RelayTo(pnode);
                });
            }
            return true;
        }
        return false;
    }

    // Check against synchronized checkpoint
    bool CheckSync(const uint256& hashBlock, const CBlockIndex* pindexPrev)
    {
        int nHeight = pindexPrev->nHeight + 1;
        LOCK(cs_hashSyncCheckpoint);
        if (hashSyncCheckpoint==uint256())
        {
            return true;
        }

        // sync-checkpoint should always be accepted block - if not reset it and return
        if (mapBlockIndex.count(hashSyncCheckpoint) == 0)
        {
            hashSyncCheckpoint = uint256();
            return false;
        }

        const CBlockIndex* pindexSync = mapBlockIndex[hashSyncCheckpoint];

        if (nHeight > pindexSync->nHeight)
        {
            // trace back to same height as sync-checkpoint
            const CBlockIndex* pindex = pindexPrev;

            //Causes performance issues during the initial download
            if (!IsInitialBlockDownload())
            {
                while (pindex->nHeight > pindexSync->nHeight)
                {
                    if (!(pindex = pindex->pprev))
                    {
                        return error("CheckSync: pprev null - block index structure failure");
                    }
                }
                if (pindex->nHeight < pindexSync->nHeight || pindex->GetBlockHashLegacy() != hashSyncCheckpoint)
                {
                    return false; // only descendant of sync-checkpoint can pass check
                }
            }
        }
        if (nHeight == pindexSync->nHeight && hashBlock != hashSyncCheckpoint)
        {
            return false; // same height with sync-checkpoint
        }
        if (nHeight < pindexSync->nHeight && !mapBlockIndex.count(hashBlock))
        {
            return false; // lower height than sync-checkpoint
        }
        return true;
    }

    bool IsSecuredBySyncCheckpoint(const uint256& hashBlock)
    {
        if (hashSyncCheckpoint==uint256())
            return false;
        // sync-checkpoint should always be accepted block - if not reset it and return
        if (mapBlockIndex.count(hashSyncCheckpoint) == 0)
        {
            hashSyncCheckpoint = uint256();
            return false;
        }
        const CBlockIndex* pindexSync = mapBlockIndex[hashSyncCheckpoint];
        const CBlockIndex* pindex = mapBlockIndex[hashBlock];

        if(!pindexSync || !pindex || pindex->nHeight >= pindexSync->nHeight)
            return false;

        return true;
    }

    bool WantedByPendingSyncCheckpoint(uint256 hashBlock)
    {
        LOCK(cs_hashSyncCheckpoint);
        if (hashPendingCheckpoint == uint256())
        {
            return false;
        }
        if (hashBlock == hashPendingCheckpoint)
        {
            return true;
        }
        return false;
    }

    // Reset synchronized checkpoint to last hardened checkpoint (to be used if checkpoint key is changed for whatever reason - e.g. a stalled chain)
    bool ResetSyncCheckpoint(const CChainParams& chainparams)
    {
        LOCK2(cs_main, cs_hashSyncCheckpoint);// cs_main lock required for ReadBlockFromDisk

        const uint256& hash = GetLastCheckpoint(chainparams.Checkpoints())->GetBlockHashLegacy();
        if (mapBlockIndex.count(hash) && !chainActive.Contains(mapBlockIndex[hash]))
        {
            // checkpoint block accepted but not yet in main chain
            LogPrintf("ResetSyncCheckpoint: SetBestChain to hardened checkpoint %s\n", hash.ToString().c_str());
            CBlock* pblock = new CBlock();
            if (!ReadBlockFromDisk(*pblock,mapBlockIndex[hash], chainparams.GetConsensus()))
            {
                return error("ResetSyncCheckpoint: ReadBlockFromDisk failed for hardened checkpoint %s", hash.ToString().c_str());
            }
            CValidationState State;
            if (!ActivateBestChain(State, chainparams, std::shared_ptr<CBlock>(pblock)))
            {
                return error("ResetSyncCheckpoint: ActivateBestChain failed for hardened checkpoint %s", hash.ToString().c_str());
            }
        }
        if(mapBlockIndex.count(hash) && chainActive.Contains(mapBlockIndex[hash]))
        {
            if (!WriteSyncCheckpoint(hash))
            {
                return error("ResetSyncCheckpoint: failed to write sync checkpoint %s", hash.ToString().c_str());
            }
            LogPrintf("ResetSyncCheckpoint: sync-checkpoint reset to %s\n", hashSyncCheckpoint.ToString().c_str());
            return true;
        }
        return false;
    }

    void AskForPendingSyncCheckpoint(CNode* pfrom)
    {
        LOCK(cs_hashSyncCheckpoint);
        if (pfrom && hashPendingCheckpoint != uint256() && (!mapBlockIndex.count(hashPendingCheckpoint)))
        {
            pfrom->AskFor(CInv(MSG_BLOCK, hashPendingCheckpoint));
        }
    }

    // Automatically select a suitable sync-checkpoint to broadcast [Checkpoint server only]
    uint256 AutoSelectSyncCheckpoint()
    {
        // Proof-of-work blocks are immediately checkpointed
        // to defend against 51% attack which rejects other miners block

        // Select the last proof-of-work block
        CBlockIndex *pindex = chainActive.Tip();
        if (pindex->nHeight < AUTO_CHECKPOINT_DEPTH+1)
        {
            return pindex->GetBlockHashLegacy();
        }

        // Search backwards AUTO_CHECKPOINT_DEPTH blocks
        for(int i=0;i<AUTO_CHECKPOINT_DEPTH;i++)
        {
            pindex = pindex->pprev;
        }

        return pindex->GetBlockHashLegacy();
    }

    // Set the private key with which to broadcast checkpoints [Checkpoint server only]
    bool SetCheckpointPrivKey(std::string strPrivKey)
    {
        // Test signing a sync-checkpoint with genesis block
        CSyncCheckpoint checkpoint;
        checkpoint.hashCheckpoint = uint256();
        CDataStream sMsg(SER_NETWORK, PROTOCOL_VERSION);
        sMsg << (CUnsignedSyncCheckpoint)checkpoint;
        checkpoint.vchMsg = std::vector<unsigned char>(sMsg.begin(), sMsg.end());

        std::vector<unsigned char> vchPrivKey = ParseHex(strPrivKey);
        CKey key;
        key.Set(vchPrivKey.begin(), vchPrivKey.end(), false); // if key is not correct openssl may crash
        if (!key.Sign(Hash(checkpoint.vchMsg.begin(), checkpoint.vchMsg.end()), checkpoint.vchSig))
        {
            return false;
        }

        // Test signing successful, proceed
        CSyncCheckpoint::strMasterPrivKey = strPrivKey;
        return true;
    }

    // Broadcast a new checkpoint to the network [Checkpoint server only]
    bool SendSyncCheckpoint(uint256 hashCheckpoint, const CChainParams& chainparams)
    {
        CSyncCheckpoint checkpoint;
        checkpoint.hashCheckpoint = hashCheckpoint;
        CDataStream sMsg(SER_NETWORK, PROTOCOL_VERSION);
        sMsg << (CUnsignedSyncCheckpoint)checkpoint;
        checkpoint.vchMsg = std::vector<unsigned char>(sMsg.begin(), sMsg.end());

        if (CSyncCheckpoint::strMasterPrivKey.empty())
        {
            return error("SendSyncCheckpoint: Checkpoint master key unavailable.");
        }

        std::vector<unsigned char> vchPrivKey = ParseHex(CSyncCheckpoint::strMasterPrivKey);
        CKey key;
        key.Set(vchPrivKey.begin(), vchPrivKey.end(), false); // if key is not correct openssl may crash

        if (!key.Sign(Hash(checkpoint.vchMsg.begin(), checkpoint.vchMsg.end()), checkpoint.vchSig))
        {
            return error("SendSyncCheckpoint: Unable to sign checkpoint, check private key?");
        }

        if(!checkpoint.ProcessSyncCheckpoint(NULL, chainparams))
        {
            LogPrintf("WARNING: SendSyncCheckpoint: Failed to process checkpoint.\n");
            return false;
        }
        else
        {
            LogPrintf("SendSyncCheckpoint: SUCCESS.\n");
        }

        // Relay checkpoint
        {
            g_connman->ForEachNode([&checkpoint](CNode* pnode) {
                    checkpoint.RelayTo(pnode);
                });
        }
        return true;
    }

    // Is the sync-checkpoint too old?
    bool IsSyncCheckpointTooOld(unsigned int nSeconds)
    {
        if (IsArgSet("-testnet"))
            return false;

        LOCK(cs_hashSyncCheckpoint);
        if (hashSyncCheckpoint==uint256())
        {
            return true;
        }
        // sync-checkpoint should always be accepted block - if not reset it and return
        if (mapBlockIndex.count(hashSyncCheckpoint) == 0)
        {
            hashSyncCheckpoint = uint256();
            return true;
        }
        const CBlockIndex* pindexSync = mapBlockIndex[hashSyncCheckpoint];
        return (pindexSync->GetBlockTime() + nSeconds < GetAdjustedTime());
    }
}

//Gulden checkpoint key public signature
const std::string CSyncCheckpoint::strMasterPubKey		= "04ff1007ffe5cffe8889b1842c7bd2d82894f9a5a946fbec95f5f748a190fcb724c29ab304b026ba2040fca893c617ca09f7b67f53be0f18146ee93638ab39c0dc";
const std::string CSyncCheckpoint::strMasterPubKeyTestnet  	= "042785eba41f699847a7afa0fd3d70485065b5d04d726925e5a7827ca11b5a0479ea8f90dc74e10500adff05ca695f1590bb6bd17ad3cccea80441d4c2e9a4e5e5";
const std::string CSyncCheckpoint::strMasterPubKeyOld             = "043872e04721dc342fee16ca8f76e0d0bee23170e000523241f83e5190dece6e259a465e8efd4194c0b8f208967c59089fc2d85fcb9847764568833021197344b0";

std::string CSyncCheckpoint::strMasterPrivKey = "";

// ppcoin: verify signature of sync-checkpoint message
bool CSyncCheckpoint::CheckSignature(CNode* pfrom)
{
    CPubKey key(ParseHex(IsArgSet("-testnet") ? CSyncCheckpoint::strMasterPubKeyTestnet : CSyncCheckpoint::strMasterPubKey));
    if (!key.IsValid())
    {
        return error("CSyncCheckpoint::CheckSignature() : SetPubKey failed");
    }
    if (!key.Verify(Hash(vchMsg.begin(), vchMsg.end()), vchSig))
    {
        CPubKey keyOld(ParseHex(strMasterPubKeyOld));
        if (!keyOld.Verify(Hash(vchMsg.begin(), vchMsg.end()), vchSig))
        {
            Misbehaving(pfrom->GetId(), 10);
            return error("CSyncCheckpoint::CheckSignature() : verify signature failed");
        }
        return false;
    }

    // Now unserialize the data
    CDataStream sMsg(vchMsg, SER_NETWORK, PROTOCOL_VERSION);
    sMsg >> *(CUnsignedSyncCheckpoint*)this;
    return true;
}

// ppcoin: process synchronized checkpoint
bool CSyncCheckpoint::ProcessSyncCheckpoint(CNode* pfrom, const CChainParams& chainparams)
{
    if (!CheckSignature(pfrom))
    {
        return false;
    }

    LOCK2(cs_main, Checkpoints::cs_hashSyncCheckpoint);// cs_main lock required for ReadBlockFromDisk

    if (!mapBlockIndex.count(hashCheckpoint))
    {
        // We haven't received the checkpoint chain, keep the checkpoint as pending
        Checkpoints::hashPendingCheckpoint = hashCheckpoint;
        Checkpoints::checkpointMessagePending = *this;
        LogPrintf("ProcessSyncCheckpoint: pending for sync-checkpoint %s\n", hashCheckpoint.ToString().c_str());
        return false;
    }

    if (!Checkpoints::ValidateSyncCheckpoint(hashCheckpoint))
    {
        return false;
    }

    CBlockIndex* pindexCheckpoint = mapBlockIndex[hashCheckpoint];
    if (!chainActive.Contains(pindexCheckpoint))
    {
        // checkpoint chain received but not yet main chain
        CBlock* pblock = new CBlock();
        if (!ReadBlockFromDisk(*pblock, pindexCheckpoint, chainparams.GetConsensus()))
        {
            return error("ProcessSyncCheckpoint: ReadBlockFromDisk failed for sync checkpoint %s", hashCheckpoint.ToString().c_str());
        }

        CValidationState State;
        if (!ActivateBestChain(State, chainparams, std::shared_ptr<CBlock>(pblock)))
        {
            Checkpoints::hashInvalidCheckpoint = hashCheckpoint;
            return error("ProcessSyncCheckpoint: ActivateBestChain failed for sync checkpoint %s", hashCheckpoint.ToString().c_str());
        }
    }

    if (!Checkpoints::WriteSyncCheckpoint(hashCheckpoint))
    {
        return error("ProcessSyncCheckpoint(): failed to write sync checkpoint %s", hashCheckpoint.ToString().c_str());
    }

    Checkpoints::checkpointMessage = *this;
    Checkpoints::hashPendingCheckpoint = uint256();
    Checkpoints::checkpointMessagePending.SetNull();
    LogPrintf("ProcessSyncCheckpoint: sync-checkpoint at %s\n", hashCheckpoint.ToString().c_str());
    return true;
}


void CUnsignedSyncCheckpoint::SetNull()
{
    nVersion = 1;
    hashCheckpoint = uint256();
}

std::string CUnsignedSyncCheckpoint::ToString() const
{
    return strprintf("CSyncCheckpoint(\n    nVersion       = %d\n    hashCheckpoint = %s\n)\n", nVersion, hashCheckpoint.ToString().c_str());
}

void CUnsignedSyncCheckpoint::print() const
{
    LogPrintf("%s", ToString().c_str());
}

CSyncCheckpoint::CSyncCheckpoint()
{
    SetNull();
}

void CSyncCheckpoint::SetNull()
{
    CUnsignedSyncCheckpoint::SetNull();
    vchMsg.clear();
    vchSig.clear();
}

bool CSyncCheckpoint::IsNull() const
{
    return (hashCheckpoint == uint256());
}

uint256 CSyncCheckpoint::GetHash() const
{
    return SerializeHash(*this);
}

bool CSyncCheckpoint::RelayTo(CNode* pnode) const
{
    // returns true if wasn't already sent
    if (pnode->hashCheckpointKnown != hashCheckpoint)
    {
        pnode->hashCheckpointKnown = hashCheckpoint;
        g_connman->PushMessage(pnode, CNetMsgMaker(pnode->GetSendVersion()).Make(NetMsgType::CHECKPOINT, *this));
        return true;
    }
    return false;
}
