//Kimoto gravity well Thanks Dr Kimoto Chan

#ifndef GULDENCOIN_DIFF_KIMOTO_H
#define GULDENCOIN_DIFF_KIMOTO_H

unsigned int static KimotoGravityWell(const CBlockIndex* pindexLast, const CBlockHeader *pblock, uint64_t TargetBlocksSpacingSeconds, uint64_t PastBlocksMin, uint64_t PastBlocksMax, const Consensus::Params& params)
{
    /* current difficulty formula, megacoin - kimoto gravity well */
    const CBlockIndex *BlockLastSolved = pindexLast;
    const CBlockIndex *BlockReading = pindexLast;
    uint64_t PastBlocksMass = 0;
    int64_t PastRateActualSeconds = 0;
    int64_t PastRateTargetSeconds = 0;
    double PastRateAdjustmentRatio = double(1);
    arith_uint256 PastDifficultyAverage;
    arith_uint256 PastDifficultyAveragePrev;
    double EventHorizonDeviation;
    double EventHorizonDeviationFast;
    double EventHorizonDeviationSlow;
           
    if (BlockLastSolved == NULL || BlockLastSolved->nHeight == 0 || (uint64_t)BlockLastSolved->nHeight < PastBlocksMin)
        return params.powLimit.GetCompact();

    int64_t LatestBlockTime = BlockLastSolved->GetBlockTime();
    for (unsigned int i = 1; BlockReading && BlockReading->nHeight > 0; i++)
    {
        if (PastBlocksMax > 0 && i > PastBlocksMax)
            break;
    
	PastBlocksMass++;
                
        if (i == 1)
	{
            PastDifficultyAverage.SetCompact(BlockReading->nBits);
	}
        else
	{
            arith_uint256 BRDifficulty;
	    BRDifficulty.SetCompact(BlockReading->nBits);
            if (BRDifficulty > PastDifficultyAveragePrev)
	    {
                PastDifficultyAverage = PastDifficultyAveragePrev + ((BRDifficulty - PastDifficultyAveragePrev) / i);
            }
            else
	    {
                PastDifficultyAverage = PastDifficultyAveragePrev - ((PastDifficultyAveragePrev - BRDifficulty) / i);
            }
	}
        PastDifficultyAveragePrev = PastDifficultyAverage;
    
        if (LatestBlockTime < BlockReading->GetBlockTime())
        {
            if (BlockReading->nHeight > 200)
                LatestBlockTime = BlockReading->GetBlockTime();
        }
    
        PastRateActualSeconds = LatestBlockTime - BlockReading->GetBlockTime();
        PastRateTargetSeconds = TargetBlocksSpacingSeconds * PastBlocksMass;
        PastRateAdjustmentRatio = double(1);
    	if (BlockReading->nHeight > 200)
        {
            if (PastRateActualSeconds < 1)
                PastRateActualSeconds = 1;
        }
        else if (PastRateActualSeconds < 0)
        {  
            PastRateActualSeconds = 0;
        }
    
        if (PastRateActualSeconds != 0 && PastRateTargetSeconds != 0)
            PastRateAdjustmentRatio = double(PastRateTargetSeconds) / double(PastRateActualSeconds);
    
        EventHorizonDeviation = 1 + (0.7084 * pow((double(PastBlocksMass)/double(28.2)), -1.228));
        EventHorizonDeviationFast = EventHorizonDeviation;
        EventHorizonDeviationSlow = 1 / EventHorizonDeviation;
    
        if (PastBlocksMass >= PastBlocksMin && ((PastRateAdjustmentRatio <= EventHorizonDeviationSlow) || (PastRateAdjustmentRatio >= EventHorizonDeviationFast)))
        {
            assert(BlockReading);
            break;
        }
        if (BlockReading->pprev == NULL)
        {
            assert(BlockReading);
            break;
        }
        BlockReading = BlockReading->pprev;
    }
    arith_uint256 bnNew(PastDifficultyAverage);
        
    if (PastRateActualSeconds != 0 && PastRateTargetSeconds != 0)
    {
        bnNew *= arith_uint256(PastRateActualSeconds);
        bnNew /= arith_uint256(PastRateTargetSeconds);
    }
    
    if (bnNew > params.powLimit)
        bnNew = params.powLimit;
    
    if (fDebug)
    {
        LogPrintf("Difficulty Retarget - Kimoto Gravity Well\n");
        LogPrintf("Before: %08x %s\n", BlockLastSolved->nBits, arith_uint256().SetCompact(BlockLastSolved->nBits).ToString().c_str());
        LogPrintf("After: %08x %s\n", bnNew.GetCompact(), bnNew.ToString().c_str());
    }
    return bnNew.GetCompact();
}

unsigned int static GetNextWorkRequired_KGW(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params& params)
{
    static const int64_t BlocksTargetSpacing = .5 * 60;
    int64_t NewBlocksTargetSpacing = .5*60;
    if (pindexLast->nHeight > 30000)
        NewBlocksTargetSpacing = 150; // 150 seconds
    unsigned int TimeDaySeconds = 60 * 60 * 24;

    int64_t PastSecondsMin = TimeDaySeconds * 0.01;
    int64_t PastSecondsMax = TimeDaySeconds * 0.14;
    if (pindexLast->nHeight > 33300)
    {
        PastSecondsMin = TimeDaySeconds * 0.25;
        PastSecondsMax = TimeDaySeconds * 7;
    }
    uint64_t PastBlocksMin = PastSecondsMin / BlocksTargetSpacing;
    uint64_t PastBlocksMax = PastSecondsMax / BlocksTargetSpacing;
    if (pindexLast->nHeight > 33300)
    {
        PastBlocksMin = PastSecondsMin / NewBlocksTargetSpacing;
        PastBlocksMax = PastSecondsMax / NewBlocksTargetSpacing;
    } 

    return KimotoGravityWell(pindexLast, pblock, NewBlocksTargetSpacing, PastBlocksMin, PastBlocksMax, params);
}

#endif