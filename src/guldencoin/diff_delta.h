#ifndef GULDENCOIN_DIFF_DELTA_H
#define GULDENCOIN_DIFF_DELTA_H


// Delta, the Guldencoin Difficulty Re-adjustment
// unsigned int static GetNextWorkRequired_Delta (const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params& params, int algo)
unsigned int static GetNextWorkRequired_DELTA (const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params& params)
{
    int64_t nRetargetTimespan = params.nPowTargetSpacing;         // algo: actual default block time

    unsigned int nProofOfWorkLimit = params.powLimit.GetCompact();      // algo specific

    int nLastBlock   =   1;                             // algo: use last  1 blocks
    int nShortFrame  =   3;                             // algo: use last  3 blocks
    int nMiddleFrame =  24;                             // algo: use last 24 blocks
    int nLongFrame   = 576;                             // algo: use default blocks per day

    int nDayFrame    = 576;                             // all:  use default blocks per day

    int64_t nLBWeight     = 16;                         // tweak: weight of last block
    int64_t nShortWeight  =  4;                         // tweak: weight of short frame
    int64_t nMiddleWeight =  2;
    int64_t nLongWeight   =  1;

    int64_t nShortMinGap    = nRetargetTimespan / 6;    // default time / 6
    int64_t nShortMaxGap    = nRetargetTimespan * 6;    // default time * 6

    int64_t nBadTimeLimit   = 0;                        // replace times <= limit
    int64_t nBadTimeReplace = nRetargetTimespan / 10;   // default time / 10

    int64_t nPercentFactor = 100;

    int64_t nDeltaTimespan  = 0;
    int64_t nLBTimespan     = 0;
    int64_t nShortTimespan  = 0;
    int64_t nMiddleTimespan = 0;
    int64_t nLongTimespan   = 0;
    int64_t nDayTimespan    = 0;

    int64_t nWeightedSum       = 0;
    int64_t nWeightedDiv       = 0;
    int64_t nWeightedTimespan  = 0;
    int64_t nDailyPercentage   = 0;

    const CBlockIndex* pindexFirst = pindexLast;        // needs algo check
    const CBlockIndex* pindexPrev  = pindexLast;        // needs algo check


    // Genesis block
    if (pindexLast == NULL)
        return nProofOfWorkLimit;

    // Testnet rule, a simple copy and paste
    // no change beside nIntervall replaced by nShortFrame
    //fixme: disable for now
    if (GetBoolArg("-testnet", false))                                       // algo specific
    {
        // Special difficulty rule for testnet:
        // If the new block's timestamp is more than 20 * default block time
        // then allow mining of a min-difficulty block.
        if (pblock->nTime > pindexLast->nTime + (20 * nRetargetTimespan))
        {
            printf("Testnet 20 * default time hit: nTargetTimespan = %ld nActualTimespan = %ld\n" , nRetargetTimespan, nLBTimespan);
            return nProofOfWorkLimit;
        }
        else
        {
            // Return the last non-special-min-difficulty-rules-block
            const CBlockIndex* pindex = pindexLast;
            while (pindex->pprev && pindex->nHeight % nShortFrame != 0 && pindex->nBits == nProofOfWorkLimit)
                pindex = pindex->pprev;
            return pindex->nBits;
        }
    }


    // -- return early if not enough blocks (algo specific)
    if (pindexLast->nHeight <= nShortFrame)
        return nProofOfWorkLimit;


    // -- last block interval (algo specific)
    pindexFirst = pindexLast->pprev;
    nLBTimespan = pindexLast->GetBlockTime() - pindexFirst->GetBlockTime();

    // check for very short and long blocks (algo specific)
    // if last block was far too short, let diff raise faster
    // by cutting 50% of last block time
    if (nLBTimespan > nBadTimeLimit && nLBTimespan < nShortMinGap)
        nLBTimespan = nLBTimespan - (nLBTimespan / 2);
    // prevent falling for bad/negative block times
    if (nLBTimespan <= nBadTimeLimit)
        nLBTimespan = nBadTimeReplace;
    // if last block took far too long, let diff drop faster
    // by adding 50% of last block time
    if (nLBTimespan > nShortMaxGap)
        nLBTimespan = nLBTimespan + (nLBTimespan / 2);


    // -- short interval (algo specific)
    pindexFirst = pindexLast;
    for (int i = 1; pindexFirst && i <= nShortFrame; i++)
    {
        pindexPrev  = pindexFirst;
        pindexFirst = pindexFirst->pprev;
        nDeltaTimespan = pindexPrev->GetBlockTime() - pindexFirst->GetBlockTime();
        // prevent falling for bad/negative block times
        if (nDeltaTimespan <= nBadTimeLimit)
            nDeltaTimespan = nBadTimeReplace;

        nShortTimespan += nDeltaTimespan;
    }


    // -- middle interval (algo specific)
    if (pindexLast->nHeight <= nMiddleFrame)
    {
        nMiddleWeight = 0;
    }
    else
    {
        pindexFirst = pindexLast;
        for (int i = 1; pindexFirst && i <= nMiddleFrame; i++)
        {
            pindexPrev  = pindexFirst;
            pindexFirst = pindexFirst->pprev;
            nDeltaTimespan = pindexPrev->GetBlockTime() - pindexFirst->GetBlockTime();
            // prevent falling for bad/negative block times
            if (nDeltaTimespan <= nBadTimeLimit)
                nDeltaTimespan = nBadTimeReplace;

            nMiddleTimespan += nDeltaTimespan;
        }
    }


    // -- long interval (algo specific)
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


    // -- now use the values ...
    nWeightedSum  = nLBTimespan * nLBWeight + nShortTimespan * nShortWeight;
    nWeightedSum += nMiddleTimespan * nMiddleWeight + nLongTimespan * nLongWeight;
    nWeightedDiv  = nLastBlock * nLBWeight + nShortFrame * nShortWeight;
    nWeightedDiv += nMiddleFrame * nMiddleWeight + nLongFrame * nLongWeight;
    nWeightedTimespan = nWeightedSum / nWeightedDiv;


    // limit adjustment just in case (DIGI limits used)
    // use min 75% of default time; diff up to 133.3% of previous
    if (nWeightedTimespan < (nRetargetTimespan - (nRetargetTimespan/4)))
        nWeightedTimespan = (nRetargetTimespan - (nRetargetTimespan/4));
    // use max 150% of default time; diff down to 66.7% of previous
    if (nWeightedTimespan > (nRetargetTimespan + (nRetargetTimespan/2)))
        nWeightedTimespan = (nRetargetTimespan + (nRetargetTimespan/2));


    // -- day interval (1 day; general over all algos)
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
    nDailyPercentage = (nDayTimespan * nPercentFactor) / (nRetargetTimespan * nDayFrame);

    // limit adjustment to 10%
    if (nDailyPercentage > 110)
        nDailyPercentage = 110;
    if (nDailyPercentage <  90)
        nDailyPercentage =  90;


    // adjust Time based on day interval
    nWeightedTimespan *= nDailyPercentage;
    nWeightedTimespan /= nPercentFactor;

    
    // calculate new difficulty
    arith_uint256 bnNew;
    bnNew.SetCompact(pindexLast->nBits);            // pindexLast of algo
    bnNew *= arith_uint256(nWeightedTimespan);
    bnNew /= arith_uint256(nRetargetTimespan);

    // difficulty should never go below (human view) the starting difficulty
    if (bnNew > arith_uint256().SetCompact(nProofOfWorkLimit))
        bnNew = arith_uint256().SetCompact(nProofOfWorkLimit);

    if(fDebug)
    {
        printf("nWeightedTimespan = %ld nRetargetTimespan = %ld\n" , nWeightedTimespan, nRetargetTimespan);
        printf("nTargetTimespan = %ld nActualTimespan = %ld\n" , nRetargetTimespan, nLBTimespan);
        printf("Before: %08x %s\n", pindexLast->nBits, arith_uint256().SetCompact(pindexLast->nBits).ToString().c_str());
        printf("After: %08x %s\n", bnNew.GetCompact(), bnNew.ToString().c_str());
    }

    // return compact (uint) difficulty
    return bnNew.GetCompact();
}

#endif
