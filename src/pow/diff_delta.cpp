// Copyright (c) 2015-2022 The Centure developers
// Authored by: Frank (dt_cdog@yahoo.com) and Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the GNU Lesser General Public License v3, see the accompanying
// file COPYING
//
// This file contains Delta, the Munt Difficulty Re-adjustment algorithm developed by Frank (dt_cdog@yahoo.com) with various enhancements by Malcolm MacLeod (mmacleod@gmx.com)
// The core algorithm works by taking time measurements of four periods (last block; short window; medium window; long window) and then apply a weighting to them.
// This allows the algorithm to react to short term fluctuations while still taking long term block targets into account, which helps prevent it from overreacting.
//
// In addition to the core algorithm several extra rules are then applied in certain situations (e.g. multiple quick blocks) to enhance the behaviour.

#include "diff_common.h"
#include "diff_delta.h"
#include "chainparams.h"
#include <stdint.h>

unsigned int GetNextWorkRequired_DELTA (const CBlockIndex* pindexLast, const CBlockHeader* block, int nPowTargetSpacing, unsigned int nPowLimit, unsigned int nFirstDeltaBlock)
{
    // These two variables are not used in the calculation at all, but only for logging when -debug is set, to prevent logging the same calculation repeatedly.
    static int64_t nPrevHeight     = 0;
    static int64_t nPrevDifficulty = 0;
    static bool debugLogging = LogAcceptCategory(BCLog::DELTA);

    std::string sLogInfo;
    
    //uint32_t nDeltaVersion=3;

    // The spacing we want our blocks to come in at.
    int64_t nRetargetTimespan      = nPowTargetSpacing;

    // The minimum difficulty that is allowed, this is set on a per algorithm basis.
    const unsigned int nProofOfWorkLimit = nPowLimit;

    // How many blocks to use to calculate each of the four algo specific time windows (last block; short window; middle window; long window)
    const unsigned int nLastBlock           =   1;
    const unsigned int nShortFrame          =   3;
    const unsigned int nMiddleFrame         =  24;
    const unsigned int nLongFrame           = 576;


    // Weighting to use for each of the four algo specific time windows.
    const int64_t nLBWeight        =  64;
    const int64_t nShortWeight     =  8;
    int64_t nMiddleWeight          =  2;
    int64_t nLongWeight            =  1;

    // Minimum threshold for the short window, if it exceeds these thresholds then favour a larger swing in difficulty.
    const int64_t nQBFrame         = nShortFrame + 1;

    // Any block with a time lower than nBadTimeLimit is considered to have a 'bad' time, the time is replaced with the value of nBadTimeReplace.
    const int64_t nBadTimeLimit    = 0;
    const int64_t nBadTimeReplace  = nRetargetTimespan / 10;

    // Used for 'exception 1' (see code below), if block is lower than 'nLowTimeLimit' then prevent the algorithm from decreasing difficulty any further.
    // If block is lower than 'nFloorTimeLimit' then impose a minor increase in difficulty.
    // This helps to prevent the algorithm from generating and giving away too many sudden/easy 'quick blocks' after a long block or two have occured, and instead forces things to be recovered more gently over time without intefering with other desirable properties of the algorithm.
    const int64_t nLowTimeLimit    = nRetargetTimespan * 90 / PERCENT_FACTOR;
    const int64_t nFloorTimeLimit  = nRetargetTimespan * 65 / PERCENT_FACTOR;

    // Used for 'exception 2' (see code below), if a block has taken longer than nLongTimeLimit we perform a difficulty reduction, which increases over time based on nLongTimeStep
    // NB!!! nLongTimeLimit MUST ALWAYS EXCEED THE THE MAXIMUM DRIFT ALLOWED (IN BOTH THE POSITIVE AND NEGATIVE DIRECTION)
    // SO AT LEAST DRIFT X2 OR MORE - OR ELSE CLIENTS CAN FORCE LOW DIFFICULTY BLOCKS BY MESSING WITH THE BLOCK TIMES.
    const int64_t nDrift   = 60; //Drift in seconds
    int64_t nLongTimeLimit = (3 * nDrift);
    int64_t nLongTimeStep  = nDrift;

    // Limit adjustment amount to try prevent jumping too far in either direction.
    // min 75% of default time; 33.3% difficulty increase
    unsigned int nMinimumAdjustLimit = (unsigned int)nRetargetTimespan * 75 / PERCENT_FACTOR;
    // max 150% of default time; 33.3% difficuly decrease
    unsigned int nMaximumAdjustLimit = (unsigned int)nRetargetTimespan * 150 / PERCENT_FACTOR;

    // Variables used in calculation
    int64_t nQBTimespan            = 0;

    int64_t nWeightedSum           = 0;
    int64_t nWeightedDiv           = 0;
    int64_t nWeightedTimespan      = 0;

    const CBlockIndex* pindexFirst = pindexLast;

    // Genesis block
    if (pindexLast == NULL)
        return nProofOfWorkLimit;

    // -- Use a fixed difficulty until we have enough blocks to work with
    if (pindexLast->nHeight <= nQBFrame)
        return nProofOfWorkLimit;

    // -- Calculate timespan for last block window
    int64_t nLBTimespan = 0;
    {
        int64_t nLBTimespanPoW = 0;
        pindexFirst = pindexLast->pprev;
        nLBTimespanPoW = pindexLast->GetBlockTime() - pindexFirst->GetBlockTime();
        nLBTimespan = pindexLast->GetBlockTimePoW2Witness() - pindexFirst->GetBlockTimePoW2Witness();

        // Prevent bad/negative block times - switch them for a fixed time.
        if (nLBTimespan <= nBadTimeLimit)
            nLBTimespan = nBadTimeReplace;
        
        // If last block was 'long block' with difficulty adjustment, treat it as a faster block at the lower difficulty
        if (nLBTimespanPoW > (nLongTimeLimit + nLongTimeStep))
        {
            nLBTimespanPoW = (nLBTimespanPoW-nLongTimeLimit)%nLongTimeStep;
            nLBTimespan = std::min(nLBTimespan, nLBTimespanPoW);
        }
    }


    // -- Calculate timespan for short window
    int64_t nShortTimespan = 0;
    {
        int64_t nDeltaTimespan = 0;
        int64_t nDeltaTimespanPoW = 0;
        pindexFirst = pindexLast;
        for (unsigned int i = 1; pindexFirst != NULL && i <= nQBFrame; i++)
        {
            nDeltaTimespanPoW = pindexFirst->GetBlockTime() - pindexFirst->pprev->GetBlockTime();
            nDeltaTimespan = pindexFirst->GetBlockTimePoW2Witness() - pindexFirst->pprev->GetBlockTimePoW2Witness();

            // Prevent bad/negative block times - switch them for a fixed time.
            if (nDeltaTimespan <= nBadTimeLimit)
                nDeltaTimespan = nBadTimeReplace;
            
            // If last block was 'long block' with difficulty adjustment, treat it as a faster block at the lower difficulty
            if (nDeltaTimespanPoW > (nLongTimeLimit + nLongTimeStep))
            {
                nDeltaTimespanPoW = (nDeltaTimespanPoW-nLongTimeLimit)%nLongTimeStep;
                nDeltaTimespan = std::min(nDeltaTimespan, nDeltaTimespanPoW);
            }

            if (i<= nShortFrame)
                nShortTimespan += nDeltaTimespan;
            nQBTimespan += nDeltaTimespan;
            pindexFirst = pindexFirst->pprev;
        }
    }

    // -- Calculate time interval for middle window
    int64_t nMiddleTimespan = 0;
    {
        int64_t nDeltaTimespan = 0;
        int64_t nDeltaTimespanPoW = 0;
        if (pindexLast->nHeight - (int)nFirstDeltaBlock <= (int)nMiddleFrame)
        {
            nMiddleWeight = nMiddleTimespan = 0;
        }
        else
        {
            pindexFirst = pindexLast;
            for (unsigned int i = 1; pindexFirst != NULL && i <= nMiddleFrame; i++)
            {
                nDeltaTimespan = pindexFirst->GetBlockTimePoW2Witness() - pindexFirst->pprev->GetBlockTimePoW2Witness();

                // Prevent bad/negative block times - switch them for a fixed time.
                if (nDeltaTimespan <= nBadTimeLimit)
                    nDeltaTimespan = nBadTimeReplace;
                
                // If last block was 'long block' with difficulty adjustment, treat it as a faster block at the lower difficulty
                if (nDeltaTimespanPoW > (nLongTimeLimit + nLongTimeStep))
                {
                    nDeltaTimespanPoW = (nDeltaTimespanPoW-nLongTimeLimit)%nLongTimeStep;
                    nDeltaTimespan = std::min(nDeltaTimespan, nDeltaTimespanPoW);
                }

                nMiddleTimespan += nDeltaTimespan;
                pindexFirst = pindexFirst->pprev;
            }
        }
    }


    // -- Calculate timespan for long window
    int64_t nLongTimespan = 0;
    {
        // NB! No need to worry about single negative block times as it has no significant influence over this many blocks.
        if ((int)pindexLast->nHeight - (int)nFirstDeltaBlock <= (int)nLongFrame)
        {
            nLongWeight = nLongTimespan = 0;
        }
        else
        {
            pindexFirst = pindexLast;
            for (unsigned int i = 1; pindexFirst != NULL && i <= nLongFrame; i++)
            {
                pindexFirst = pindexFirst->pprev;
            }
            nLongTimespan = pindexLast->GetBlockTimePoW2Witness() - pindexFirst->GetBlockTimePoW2Witness();
        }
    }

    // -- Combine all the timespans and weights to get a weighted timespan
    nWeightedSum      = (nLBTimespan * nLBWeight) + (nShortTimespan * nShortWeight);
    nWeightedSum     += (nMiddleTimespan * nMiddleWeight) + (nLongTimespan * nLongWeight);
    nWeightedDiv      = (nLastBlock * nLBWeight) + (nShortFrame * nShortWeight);
    nWeightedDiv     += (nMiddleFrame * nMiddleWeight) + (nLongFrame * nLongWeight);
    nWeightedTimespan = nWeightedSum / nWeightedDiv;

    // If we are close to target time then reduce the adjustment limits to smooth things off a little bit more.
    {
        if (std::abs(nLBTimespan - nRetargetTimespan) < nRetargetTimespan * 20 / PERCENT_FACTOR)
        {
            // 90% of target
            // 11.11111111111111% difficulty increase
            // 10% difficulty decrease.
            nMinimumAdjustLimit = (unsigned int)nRetargetTimespan * 90 / PERCENT_FACTOR;
            nMaximumAdjustLimit = (unsigned int)nRetargetTimespan * 110 / PERCENT_FACTOR;
        }
        else if (std::abs(nLBTimespan - nRetargetTimespan) < nRetargetTimespan * 30 / PERCENT_FACTOR)
        {
            // 80% of target - 25% difficulty increase/decrease maximum.
            nMinimumAdjustLimit = (unsigned int)nRetargetTimespan * 80 / PERCENT_FACTOR;
            nMaximumAdjustLimit = (unsigned int)nRetargetTimespan * 120 / PERCENT_FACTOR;
        }
    }

    // -- Apply the adjustment limits
    {
        // min
        if (nWeightedTimespan < nMinimumAdjustLimit)
            nWeightedTimespan = nMinimumAdjustLimit;
        // max
        if (nWeightedTimespan > nMaximumAdjustLimit)
            nWeightedTimespan = nMaximumAdjustLimit;
    }



    // -- Finally calculate and set the new difficulty.
    arith_uint256 bnNew;
    bnNew.SetCompact(pindexLast->nBits);
    bnNew =  bnNew * arith_uint256(nWeightedTimespan);
    bnNew = bnNew / arith_uint256(nRetargetTimespan);


    // Now that we have the difficulty we run a last few 'special purpose' exception rules which have the ability to override the calculation:
    // Exception 1 - Never adjust difficulty downward (human view) if previous block generation was already faster than what we wanted.
    {
        nLBTimespan = pindexLast->GetBlockTimePoW2Witness() - pindexLast->pprev->GetBlockTimePoW2Witness();
        
        arith_uint256 bnComp;
        bnComp.SetCompact(pindexLast->nBits);
        if (nLBTimespan > 0 && nLBTimespan < nLowTimeLimit && (bnNew > bnComp))
        {
            // If it is this low then we actually give it a slight nudge upwards - 5%
            if (nLBTimespan < nFloorTimeLimit)
            {
                bnNew.SetCompact(pindexLast->nBits);
                bnNew = bnNew * arith_uint256(95);
                bnNew = bnNew / arith_uint256(PERCENT_FACTOR);
                if (debugLogging && (nPrevHeight != pindexLast->nHeight) )
                    sLogInfo +=  strprintf("<DELTA> Last block time [%ld] was far below target but adjustment still downward, forcing difficulty up by 5%% instead\n", nLBTimespan);
            }
            else
            {
                bnNew.SetCompact(pindexLast->nBits);
                if (debugLogging && (nPrevHeight != pindexLast->nHeight) )
                    sLogInfo += strprintf("<DELTA> Last block time [%ld] below target but adjustment still downward, blocking downward adjustment\n", nLBTimespan);
            }
        }
    }

    // Exception 2 - Reduce difficulty if current block generation time has already exceeded maximum time limit. (NB! nLongTimeLimit must exceed maximum possible drift in both positive and negative direction)
    {
        int64_t lastBlockTime=pindexLast->GetBlockTimePoW2Witness();
        if ((block->nTime - lastBlockTime) > nLongTimeLimit)
        {
            // Fixed reduction for each missed step. 10% pre-SIGMA, 30% after SIGMA, 10% delta V3
            int32_t nDeltaDropPerStep=110;
            int64_t nNumMissedSteps = ((block->nTime - lastBlockTime - nLongTimeLimit) / nLongTimeStep) + 1;
            for(int i=0;i < nNumMissedSteps; ++i)
            {
                bnNew = bnNew * arith_uint256(nDeltaDropPerStep);
                bnNew = bnNew / arith_uint256(PERCENT_FACTOR);
            }

            if (debugLogging && (nPrevHeight != pindexLast->nHeight ||  bnNew.GetCompact() != nPrevDifficulty) )
                sLogInfo +=  strprintf("<DELTA> Maximum block time hit - dropping difficulty %08x %s\n", bnNew.GetCompact(), bnNew.ToString().c_str());
        }
    }

    // Exception 3 - Difficulty should never go below (human view) the starting difficulty, so if it has we force it back to the limit.    
    {
        arith_uint256 bnComp;
        if (block->nTime > 1571320800)
        {
            const unsigned int newProofOfWorkLimit = UintToArith256(uint256S("0x003fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff")).GetCompact();
            bnComp.SetCompact(newProofOfWorkLimit);
            if (bnNew > bnComp)
                bnNew.SetCompact(newProofOfWorkLimit);
        }
        else
        {
            bnComp.SetCompact(nProofOfWorkLimit);
            if (bnNew > bnComp)
                bnNew.SetCompact(nProofOfWorkLimit);
        }
    }

    if (debugLogging)
    {
        if (nPrevHeight != pindexLast->nHeight ||  bnNew.GetCompact() != nPrevDifficulty)
        {
            static RecursiveMutex logCS;
            LOCK(logCS);
            LogPrintf("<DELTA> Height= %d\n" , pindexLast->nHeight);
            LogPrintf("%s" , sLogInfo.c_str());
            LogPrintf("<DELTA> nTargetTimespan = %ld nActualTimespan = %ld nWeightedTimespan = %ld \n", nRetargetTimespan, nLBTimespan, nWeightedTimespan);
            LogPrintf("<DELTA> nShortTimespan/nShortFrame = %ld nMiddleTimespan/nMiddleFrame = %ld nLongTimespan/nLongFrame = %ld \n", nShortTimespan/nShortFrame, nMiddleTimespan/nMiddleFrame, nLongTimespan/nLongFrame);
            LogPrintf("<DELTA> Before: %08x %s\n", pindexLast->nBits, arith_uint256().SetCompact(pindexLast->nBits).ToString().c_str());
            LogPrintf("<DELTA> After:  %08x %s\n", bnNew.GetCompact(), bnNew.ToString().c_str());
            LogPrintf("<DELTA> Rough change percentage (human view): %lf%%\n", -( ( (bnNew.getdouble() - arith_uint256().SetCompact(pindexLast->nBits).getdouble()) / arith_uint256().SetCompact(pindexLast->nBits).getdouble()) * 100) );
        }
        nPrevHeight = pindexLast->nHeight;
        nPrevDifficulty = bnNew.GetCompact();
    }

    // Difficulty is returned in compact form.
    return bnNew.GetCompact();
}
