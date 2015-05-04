// Copyright (c) 2015 The Guldencoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef GULDENCOIN_DIFF_DELTA_H
#define GULDENCOIN_DIFF_DELTA_H

#define PERCENT_FACTOR 100

// Delta, the Guldencoin Difficulty Re-adjustment
//
unsigned int static GetNextWorkRequired_DELTA (const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params& params)
{
    // These two variables are not used in the calculation at all, but only for logging when -debug is set, to prevent logging the same calculation repeatedly.
    static int64_t nPrevHeight     = 0;
    static int64_t nPrevDifficulty = 0;

    // The spacing we want our blocks to come in at.
    int64_t nRetargetTimespan      = params.nPowTargetSpacing;

    // The minimum difficulty that is allowed, this is set on a per algorithm basis.
    const unsigned int nProofOfWorkLimit = params.powLimit.GetCompact();

    // How many blocks to use to calculate each of the four algo specific time windows (last block; short window; middle window; long window)
    const int nLastBlock           =   1;
    const int nShortFrame          =   3;
    const int nMiddleFrame         =  24;
    const int nLongFrame           = 576;

    // How many blocks to use for the fifth window, fifth window is across all algorithms.
    #ifdef MULTI_ALGO
    const int nDayFrame            = nLongFrame;
    #endif

    // Weighting to use for each of the four algo specific time windows.
    const int64_t nLBWeight        =  8;
    const int64_t nShortWeight     =  4;
    int64_t nMiddleWeight          =  2;
    int64_t nLongWeight            =  1;

    // Minimum and maximum threshold for the last block, if it exceeds these thresholds then favour a larger swing in difficulty.
    const int64_t nLBMinGap        = nRetargetTimespan / 6;
    const int64_t nLBMaxGap        = nRetargetTimespan * 6;
    
    // Minimum threshold for the short window, if it exceeds these thresholds then favour a larger swing in difficult.
    const int64_t nSFMinGap        = nRetargetTimespan / 1.5 * nShortFrame;

    // Any block with a time lower than nBadTimeLimit is considered to have a 'bad' time, the time is replaced with the value of nBadTimeReplace.
    const int64_t nBadTimeLimit    = 0;
    const int64_t nBadTimeReplace  = nRetargetTimespan / 10;
    
    // Used for 'exception 1' (see code below), if block is lower than 'nLowTimeLimit' then prevent the algorithm from decreasing difficulty any further.
    // If block is lower than 'nFloorTimeLimit' then impose a minor increase in difficulty.
    // This helps to prevent the algorithm from generating and giving away too many sudden/easy 'quick blocks' after a long block or two have occured, and instead forces things to be recovered more gently over time without intefering with other desirable properties of the algorithm.
    const int64_t nLowTimeLimit    = nRetargetTimespan * 95 / PERCENT_FACTOR;
    const int64_t nFloorTimeLimit  = nRetargetTimespan * 80 / PERCENT_FACTOR;
    
    // Used for 'exception 2' (see code below), if a block has taken longer than nLongTimeLimit we perform a difficulty reduction by taking the original difficulty and dividing by nLongTimeStep
    // NB!!! nLongTimeLimit MUST ALWAYS EXCEED THE THE MAXIMUM DRIFT ALLOWED (IN BOTH THE POSITIVE AND NEGATIVE DIRECTION)
    // SO AT LEAST DRIFT X2 OR MORE - OR ELSE CLIENTS CAN FORCE LOW DIFFICULTY BLOCKS BY MESSING WITH THE BLOCK TIMES.
    const int64_t nLongTimeLimit   = 2 * 16 * 60;
    const int64_t nLongTimeStep    = 15 * 60;

    
    // Variables used in calculation
    int64_t nDeltaTimespan         = 0;
    int64_t nLBTimespan            = 0;
    int64_t nShortTimespan         = 0;
    int64_t nMiddleTimespan        = 0;
    int64_t nLongTimespan          = 0;
    #ifdef MULTI_ALGO
    int64_t nDayTimespan           = 0;
    #endif

    int64_t nWeightedSum           = 0;
    int64_t nWeightedDiv           = 0;
    int64_t nWeightedTimespan      = 0;
    #ifdef MULTI_ALGO
    int64_t nDailyPercentage       = 0;
    #endif

    const CBlockIndex* pindexFirst = pindexLast; //multi algo - last block is selected on a per algo basis.

    // Genesis block
    if (pindexLast == NULL)
        return nProofOfWorkLimit;

    // -- Use a fixed difficuly until we have enough blocks to work with (multi algo - this is calculated on a per algo basis)
    if (pindexLast->nHeight <= nShortFrame)
        return nProofOfWorkLimit;

    // -- Calculate timespan for last block window (multi algo - this is calculated on a per algo basis)
    pindexFirst = pindexLast->pprev;
    nLBTimespan = pindexLast->GetBlockTime() - pindexFirst->GetBlockTime();
   
    // Check for very short and long blocks (algo specific)
    // If last block was far too short, let difficulty raise faster
    // by cutting 50% of last block time
    if (nLBTimespan > nBadTimeLimit && nLBTimespan < nLBMinGap)
        nLBTimespan = nLBTimespan - (nLBTimespan / 2);
    // Prevent bad/negative block times - switch them for a fixed time.
    if (nLBTimespan <= nBadTimeLimit)
        nLBTimespan = nBadTimeReplace;
    // If last block took far too long, let difficulty drop faster
    // by adding 50% of last block time
    if (nLBTimespan > nLBMaxGap)
        nLBTimespan = nLBTimespan + (nLBTimespan / 2);


    // -- Calculate timespan for short window (multi algo - this is calculated on a per algo basis)
    pindexFirst = pindexLast;
    for (int i = 1; pindexFirst && i <= nShortFrame; i++)
    {
        nDeltaTimespan = pindexFirst->GetBlockTime() - pindexFirst->pprev->GetBlockTime();
        // Prevent bad/negative block times - switch them for a fixed time.
        if (nDeltaTimespan <= nBadTimeLimit)
            nDeltaTimespan = nBadTimeReplace;

        nShortTimespan += nDeltaTimespan;
        pindexFirst = pindexFirst->pprev;
    }
       
    // -- Calculate time interval for middle window (multi algo - this is calculated on a per algo basis)
    if (pindexLast->nHeight <= nMiddleFrame)
    {
        nMiddleWeight = 0;
    }
    else
    {
        pindexFirst = pindexLast;
        for (int i = 1; pindexFirst && i <= nMiddleFrame; i++)
        {
            nDeltaTimespan = pindexFirst->GetBlockTime() - pindexFirst->pprev->GetBlockTime();
            // Prevent bad/negative block times - switch them for a fixed time.
            if (nDeltaTimespan <= nBadTimeLimit)
                nDeltaTimespan = nBadTimeReplace;

            nMiddleTimespan += nDeltaTimespan;
            pindexFirst = pindexFirst->pprev;
        }
    }


    // -- Calculate timespan for long window (multi algo - this is calculated on a per algo basis)
    // NB! No need to worry about single negative block times as it has no significant influence over this many blocks.
    if (pindexLast->nHeight <= nLongFrame)
    {
        nLongWeight = 0;
    }
    else
    {
        pindexFirst = pindexLast;
        for (int i = 1; pindexFirst && i <= nLongFrame; i++)
            pindexFirst = pindexFirst->pprev;

        nLongTimespan = pindexLast->GetBlockTime() - pindexFirst->GetBlockTime();
    }


    // -- Combine all the timespans and weights to get a weighted timespan
    nWeightedSum  = (nLBTimespan * 100) * nLBWeight + (nShortTimespan * 100 / nShortFrame) * nShortWeight;
    nWeightedSum += (nMiddleTimespan * 100 / nMiddleFrame) * nMiddleWeight + (nLongTimespan * 100 / nLongFrame) * nLongWeight;
    nWeightedDiv  = nLBWeight + nShortWeight + nMiddleWeight + nLongWeight;
    nWeightedTimespan = nWeightedSum / nWeightedDiv / 100;

    // Check for multiple very short blocks (algo specific)
    // If last few blocks were far too short, then calculate difficulty based on short blocks alone.
    if (nShortTimespan > nBadTimeLimit && nShortTimespan < nSFMinGap )
    {
        if (fDebug && (nPrevHeight != pindexLast->nHeight) )
            LogPrintf("<DELTA> Multiple fast blocks - bypassing \n");
        nWeightedSum      = (nLBTimespan * nLBWeight) + (nShortTimespan * nShortWeight);
        nWeightedDiv      = (nLastBlock * nLBWeight) + (nShortFrame * nShortWeight);
        nWeightedTimespan = nWeightedSum / nWeightedDiv;
    }

    // Limit adjustment amount to try prevent jumping too far in either direction.
    // The same limits as DIGI difficulty algorithm are used.
    // min 75% of default time; diff up to 133.3% of previous
    // max 150% of default time; diff down to 66.7% of previous
    // min
    if (nWeightedTimespan < (nRetargetTimespan - (nRetargetTimespan/4)))
        nWeightedTimespan = (nRetargetTimespan - (nRetargetTimespan/4));
    // max
    if (nWeightedTimespan > (nRetargetTimespan + (nRetargetTimespan/2)))
        nWeightedTimespan = (nRetargetTimespan + (nRetargetTimespan/2));


    #ifdef MULTI_ALGO
    // -- Day interval (1 day; general over all algos)
    // so other algos can take blocks that 1 algo does not use
    // in case there are still longer gaps (high spikes)
    if (pindexLast->nHeight <= nDayFrame)
    {
        nDayTimespan = nRetargetTimespan * nDayFrame;
    }
    else
    {
        pindexFirst = pindexLast;
        for (int i = 1; pindexFirst && i <= nDayFrame; i++)
            pindexFirst = pindexFirst->pprev;

        nDayTimespan = pindexLast->GetBlockTime() - pindexFirst->GetBlockTime();
    }
    nDailyPercentage = (nDayTimespan * PERCENT_FACTOR) / (nRetargetTimespan * nDayFrame);

    // Limit day interval adjustment to 10%
    if (nDailyPercentage > 110)
        nDailyPercentage = 110;
    if (nDailyPercentage <  90)
        nDailyPercentage =  90;


    // Adjust Time based on day interval
    nWeightedTimespan *= nDailyPercentage;
    nWeightedTimespan /= PERCENT_FACTOR;
    #endif

    
    // Finally calculate and set the new difficulty.
    arith_uint256 bnNew;
    bnNew.SetCompact(pindexLast->nBits);
    bnNew *= arith_uint256(nWeightedTimespan);
    bnNew /= arith_uint256(nRetargetTimespan);

    
    // Now that we have the difficulty we run a last few 'special purpose' exception rules which have the ability to override the calculation.
    // Exception 1 - Never adjust difficulty downward (human view) if previous block generation was faster than what we wanted.
    nLBTimespan = pindexLast->GetBlockTime() - pindexLast->pprev->GetBlockTime();
    if (nLBTimespan > 0 && nLBTimespan < nLowTimeLimit && bnNew > arith_uint256().SetCompact(pindexLast->nBits))
    {
        // If it is this low then we actually give it a slight nudge upwards - 5%
        if (nLBTimespan < nFloorTimeLimit)
        {
            bnNew.SetCompact(pindexLast->nBits);
            bnNew *= arith_uint256(95);
            bnNew /= arith_uint256(PERCENT_FACTOR);
            if (fDebug && (nPrevHeight != pindexLast->nHeight) )
                LogPrintf("<DELTA> Last block time [%ld] was far below target but adjustment still downward, forcing difficulty up by 5%% instead\n", nLBTimespan);
        }
        else
        {
            bnNew.SetCompact(pindexLast->nBits);
            if (fDebug && (nPrevHeight != pindexLast->nHeight) )
                LogPrintf("<DELTA> Last block time [%ld] below target but adjustment still downward, blocking downward adjustment\n", nLBTimespan);
        }
    }

    
    // Exception 2 - Reduce difficulty if current block generation time has already exceeded maximum time limit. (NB! nLongTimeLimit must exceed maximum possible drift in both positive and negative direction)
    if ((pblock->nTime - pindexLast->GetBlockTime()) > nLongTimeLimit)
    {
        bnNew *= arith_uint256( (pblock->nTime - pindexLast->GetBlockTime()) / nLongTimeStep );
        if (fDebug && (nPrevHeight != pindexLast->nHeight ||  bnNew.GetCompact() != nPrevDifficulty) )
            LogPrintf("<DELTA> Maximum block time hit - halving difficulty %08x %s\n", bnNew.GetCompact(), bnNew.ToString().c_str());
    }

    
    // Exception 3 - Difficulty should never go below (human view) the starting difficulty, so if it has we force it back to the limit.
    if (bnNew > arith_uint256().SetCompact(nProofOfWorkLimit))
        bnNew.SetCompact(nProofOfWorkLimit);

    
    if (fDebug)
    {
        if (nPrevHeight != pindexLast->nHeight ||  bnNew.GetCompact() != nPrevDifficulty)
        {
            LogPrintf("<DELTA> Height= %d\n" , pindexLast->nHeight);
            LogPrintf("<DELTA> nTargetTimespan = %ld nActualTimespan = %ld nWeightedTimespan = %ld \n", nRetargetTimespan, nLBTimespan, nWeightedTimespan);
            LogPrintf("<DELTA> nShortTimespan/nShortFrame = %ld nMiddleTimespan/nMiddleFrame = %ld nLongTimespan/nLongFrame = %ld \n", nShortTimespan/nShortFrame, nMiddleTimespan/nMiddleFrame, nLongTimespan/nLongFrame);
            LogPrintf("<DELTA> Before: %08x %s\n", pindexLast->nBits, arith_uint256().SetCompact(pindexLast->nBits).ToString().c_str());
            LogPrintf("<DELTA> After:  %08x %s\n", bnNew.GetCompact(), bnNew.ToString().c_str());
            LogPrintf("<DELTA> Percentage of previous= %lf\n", (arith_uint256().SetCompact(pindexLast->nBits).getdouble() / bnNew.getdouble()) * 100);
        }
        nPrevHeight = pindexLast->nHeight;
        nPrevDifficulty = bnNew.GetCompact();
    }

    // Difficulty is returned in compact form.
    return bnNew.GetCompact();
}

#endif
