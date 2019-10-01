// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// File contains modifications by: The Gulden developers
// All modifications:
// Copyright (c) 2016 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#include "pow.h"

#include "arith_uint256.h"
#include "chain.h"
#include "primitives/block.h"
#include "crypto/hash/sigma/sigma.h"
#include <boost/thread.hpp>
#include "uint256.h"
#include "random.h"


unsigned int GetNextWorkRequired_original(const CBlockIndex* pindexLast, const CBlockHeader* pblock, const Consensus::Params& params)
{
    unsigned int nProofOfWorkLimit = UintToArith256(params.powLimit).GetCompact();

    // Genesis block
    if (pindexLast == NULL)
        return nProofOfWorkLimit;

    if ((pindexLast->nHeight + 1) % params.DifficultyAdjustmentInterval() != 0) {
        if (params.fPowAllowMinDifficultyBlocks) {

            if (pblock->GetBlockTime() > pindexLast->GetBlockTime() + params.nPowTargetSpacing * 2)
                return nProofOfWorkLimit;
            else {

                const CBlockIndex* pindex = pindexLast;
                while (pindex->pprev && pindex->nHeight % params.DifficultyAdjustmentInterval() != 0 && pindex->nBits == nProofOfWorkLimit)
                    pindex = pindex->pprev;
                return pindex->nBits;
            }
        }
        return pindexLast->nBits;
    }

    int nHeightFirst = pindexLast->nHeight - (params.DifficultyAdjustmentInterval() - 1);
    assert(nHeightFirst >= 0);
    const CBlockIndex* pindexFirst = pindexLast->GetAncestor(nHeightFirst);
    assert(pindexFirst);

    return CalculateNextWorkRequired(pindexLast, pindexFirst->GetBlockTime(), params);
}

unsigned int CalculateNextWorkRequired(const CBlockIndex* pindexLast, int64_t nFirstBlockTime, const Consensus::Params& params)
{
    if (params.fPowNoRetargeting)
        return pindexLast->nBits;

    int64_t nActualTimespan = pindexLast->GetBlockTime() - nFirstBlockTime;
    if (nActualTimespan < params.nPowTargetTimespan / 4)
        nActualTimespan = params.nPowTargetTimespan / 4;
    if (nActualTimespan > params.nPowTargetTimespan * 4)
        nActualTimespan = params.nPowTargetTimespan * 4;

    const arith_uint256 bnPowLimit = UintToArith256(params.powLimit);
    arith_uint256 bnNew;
    bnNew.SetCompact(pindexLast->nBits);
    bnNew *= nActualTimespan;
    bnNew /= params.nPowTargetTimespan;

    if (bnNew > bnPowLimit)
        bnNew = bnPowLimit;

    return bnNew.GetCompact();
}

bool CheckProofOfWork(const CBlock* block, const Consensus::Params& params)
{    
    bool fNegative;
    bool fOverflow;
    arith_uint256 bnTarget;

    bnTarget.SetCompact(block->nBits, &fNegative, &fOverflow);

    // Check range
    if (fNegative || bnTarget == 0 || fOverflow || bnTarget > UintToArith256(params.powLimit))
        return false;

    //fixme: (SIGMA) - Post activation we can simplify this.
    // Check proof of work matches claimed amount
    if (block->nTime > defaultSigmaSettings.activationDate)
    {
        //fixme: (SIGMA) Consider keeping a single context always available - with a mutex to protect it that way memory allocation is constant.
        sigma_verify_context verify(defaultSigmaSettings,std::min(defaultSigmaSettings.numVerifyThreads, (uint64_t)boost::thread::hardware_concurrency()));
        
        //fixme: (SIGMA) - Detect faster machines and disable this optimisation for them, this will further increase network security.
        // We speed up verification by doing a half verify 40% of the time instead of a full verify
        // As a half verify has a 50% chance of detecting a 'half valid' hash an attacker has only a 20% chance of a node accepting his header without banning him
        // This should provide a ~20% speed up for slow machines
        int verifyLevel = GetRand(100);
        if (verifyLevel < 20)
        {
            return verify.verifyHeader<1>(*block);
        }
        else if (verifyLevel < 40)
        {
            return verify.verifyHeader<2>(*block);
        }
        return verify.verifyHeader<0>(*block);
    }
    else
    {
        if (UintToArith256(block->GetPoWHash()) > bnTarget)
            return false;
    }

    return true;
}
