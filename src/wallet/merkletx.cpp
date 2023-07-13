// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// File contains modifications by: The Centure developers
// All modifications:
// Copyright (c) 2016-2022 The Centure developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the GNU Lesser General Public License v3, see the accompanying
// file COPYING

#include "wallet/wallet.h"
#include "wallet/merkletx.h"
#include "validation/validation.h" //cs_main
#include <consensus/consensus.h>
#include "witnessutil.h"

const uint256 CMerkleTx::ABANDON_HASH(uint256S("0000000000000000000000000000000000000000000000000000000000000001"));

void CMerkleTx::SetMerkleBranch(const CBlockIndex* pindex, int posInBlock)
{
    // Update the tx's hashBlock
    hashBlock = pindex->GetBlockHashPoW2();

    // Record block height and timestamp
    nHeight = pindex->nHeight;
    nBlockTime = pindex->nTime;

    // set the position of the transaction in the block
    nIndex = posInBlock;
}

int CMerkleTx::GetDepthInMainChain(const CBlockIndex* &pindexRet) const
{
    if (hashUnset())
        return 0;

    AssertLockHeld(cs_main);

    // Find the block it claims to be in
    BlockMap::iterator mi = mapBlockIndex.find(hashBlock);
    if (mi != mapBlockIndex.end())
    {

        CBlockIndex* pindex = (*mi).second;
        if (pindex == nullptr)
            return 0;

        pindexRet = pindex;
        bool conflicts = nIndex == -1 || !(chainActive.Contains(pindex) || partialChain.Contains(pindex));
        CChain& chain = IsPartialSyncActive() && partialChain.Height() > chainActive.Height() ? partialChain : chainActive;
        return (conflicts ? (-1) : 1) * (chain.Height() - pindex->nHeight + 1);
    }
    else
    {
        // fall back to using nHeight, only allowed in spv/partial sync
        if (IsPartialSyncActive() && nHeight >= 0)
        {

            pindexRet = nullptr;

            // tx conflicts if it was abandoned or block is orphaned which is detected here if the tx height >= partial chain offset
            // and the block is not in the index so it must have been purged by spv
            bool conflicts = nIndex == -1 || nHeight >= partialChain.HeightOffset();
            return (conflicts ? (-1) : 1) * (partialChain.Height() - nHeight + 1);
        }
        else
            return 0;
    }
}

int CMerkleTx::GetBlocksToMaturity() const
{
    if (!IsCoinBase())
        return 0;
    return std::max(0, (COINBASE_MATURITY+1) - GetDepthInMainChain());
}


bool CMerkleTx::AcceptToMemoryPool(const CAmount& nAbsurdFee, CValidationState& state)
{
    return ::AcceptToMemoryPool(mempool, state, tx, true, NULL, NULL, false, nAbsurdFee);
}
