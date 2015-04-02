#ifndef GULDENCOIN_WORK_H
#define GULDENCOIN_WORK_H

#include "../consensus/params.h"
#include "../arith_uint256.h"
#include "util.h"

//Forward declare
unsigned int GetNextWorkRequired_original(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params&);


//ADD kimoto gravity well Thanks Dr Kimoto Chan
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

unsigned int static DarkGravityWave3(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params& params)
{
    /* current difficulty formula, darkcoin - DarkGravity v3, written by Evan Duffield - evan@darkcoin.io */
    const CBlockIndex *BlockLastSolved = pindexLast;
    const CBlockIndex *BlockReading = pindexLast;
    int64_t nActualTimespan = 0;
    int64_t LastBlockTime = 0;
    int64_t PastBlocksMin = 24;
    int64_t PastBlocksMax = 24;
    int64_t CountBlocks = 0;
    arith_uint256 PastDifficultyAverage;
    arith_uint256 PastDifficultyAveragePrev;

    if (BlockLastSolved == NULL || BlockLastSolved->nHeight == 0 || BlockLastSolved->nHeight < PastBlocksMin)
    {
        // This is the first block or the height is < PastBlocksMin
        // Return minimal required work. (1e0fffff)
        return params.powLimit.GetCompact(); 
    }
    
    // loop over the past n blocks, where n == PastBlocksMax
    for (unsigned int i = 1; BlockReading && BlockReading->nHeight > 0; i++)
    {
        if (PastBlocksMax > 0 && i > PastBlocksMax)
            break;
        CountBlocks++;

        // Calculate average difficulty based on the blocks we iterate over in this for loop
        if(CountBlocks <= PastBlocksMin)
        {
            if (CountBlocks == 1)
                PastDifficultyAverage.SetCompact(BlockReading->nBits);
            else
                PastDifficultyAverage = ((PastDifficultyAveragePrev * CountBlocks)+(arith_uint256().SetCompact(BlockReading->nBits))) / (CountBlocks+1);
            PastDifficultyAveragePrev = PastDifficultyAverage;
        }

        // If this is the second iteration (LastBlockTime was set)
        if(LastBlockTime > 0)
        {
            // Calculate time difference between previous block and current block
            int64_t Diff = (LastBlockTime - BlockReading->GetBlockTime());
            // Increment the actual timespan
            nActualTimespan += Diff;
        }
        // Set LasBlockTime to the block time for the block in current iteration
        LastBlockTime = BlockReading->GetBlockTime();      

        if (BlockReading->pprev == NULL)
        {
            assert(BlockReading);
            break;
        }
        BlockReading = BlockReading->pprev;
    }
    
    // bnNew is the difficulty
    arith_uint256 bnNew(PastDifficultyAverage);

    // nTargetTimespan is the time that the CountBlocks should have taken to be generated.
    int64_t nTargetTimespan = CountBlocks*params.nPowTargetSpacing;

    // Limit the re-adjustment to 3x or 0.33x
    // We don't want to increase/decrease diff too much.
    if (nActualTimespan < nTargetTimespan/3)
        nActualTimespan = nTargetTimespan/3;
    if (nActualTimespan > nTargetTimespan*3)
        nActualTimespan = nTargetTimespan*3;

    // Calculate the new difficulty based on actual and target timespan.
    bnNew *= nActualTimespan;
    bnNew /= nTargetTimespan;

    // If calculated difficulty is lower than the minimal diff, set the new difficulty to be the minimal diff.
    if (bnNew > params.powLimit)
        bnNew = params.powLimit;
    
    if (fDebug)
    {
        LogPrintf("Difficulty Retarget - Dark Gravity Wave 3\n");
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

unsigned int static GetNextWorkRequired_DGW3(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params& params)
{
    return DarkGravityWave3(pindexLast, pblock, params);
}

// Digi algorithm should never be used until at least 2 blocks are mined.
// Contains code by RealSolid & WDC
// Cleaned up for use in Guldencoin by GeertJohan (dead code removal since Guldencoin retargets every block)
unsigned int static GetNextWorkRequired_DIGI(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params& params)
{
    // retarget timespan is set to a single block spacing because there is a retarget every block
    int64_t retargetTimespan = params.nPowTargetSpacing;

    // get previous block
    const CBlockIndex* pindexPrev = pindexLast->pprev;
    assert(pindexPrev);

    // calculate actual timestpan between last block and previous block
    int64_t nActualTimespan = pindexLast->GetBlockTime() - pindexPrev->GetBlockTime();
    if (fDebug)
    {
        LogPrintf("Digishield retarget\n");
        LogPrintf("nActualTimespan = %ld before bounds\n", nActualTimespan);
    }

    // limit difficulty changes between 50% and 125% (human view)
    if (nActualTimespan < (retargetTimespan - (retargetTimespan/4)) )
        nActualTimespan = (retargetTimespan - (retargetTimespan/4));
    if (nActualTimespan > (retargetTimespan + (retargetTimespan/2)) )
        nActualTimespan = (retargetTimespan + (retargetTimespan/2));

    // calculate new difficulty
    arith_uint256 bnNew;
    bnNew.SetCompact(pindexLast->nBits);
    bnNew *= nActualTimespan;
    bnNew /= retargetTimespan;

    // difficulty should never go below (human view) the starting difficulty
    if (bnNew > params.powLimit)
        bnNew = params.powLimit;

    if (fDebug)
    {
        LogPrintf("nTargetTimespan = %ld nActualTimespan = %ld\n" , retargetTimespan, nActualTimespan);
        LogPrintf("Before: %08x %s\n", pindexLast->nBits, arith_uint256().SetCompact(pindexLast->nBits).ToString().c_str());
        LogPrintf("After: %08x %s\n", bnNew.GetCompact(), bnNew.ToString().c_str());
    }

    return bnNew.GetCompact();
}


unsigned int static GetNextWorkRequired(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params& params)
{   
    int DiffMode = 1;
    if (GetBoolArg("-testnet", false))
    {
        if (pindexLast->nHeight+1 >= 30)
            DiffMode = 4;
        else if (pindexLast->nHeight+1 >= 15)
            DiffMode = 3;
        else if (pindexLast->nHeight+1 >= 5)
            DiffMode = 2;
    }
    else
    {
        if (pindexLast->nHeight+1 >= 194300)
            DiffMode = 4;
        else if (pindexLast->nHeight+1 >= 123793)
            DiffMode = 3;
        else if (pindexLast->nHeight+1 >= 10)
            DiffMode = 2; // KGW will kick in at block 10, so pretty much direct from start.
    }

    switch(DiffMode)
    {
        // Original 'bitcoin' targetting algo - because diff never changed before this was switched out we can just return a hardcoded diff instead.
        case 1: return 504365040;;
        case 2: return GetNextWorkRequired_KGW(pindexLast, pblock, params);
        case 3: return GetNextWorkRequired_DGW3(pindexLast, pblock, params);
        case 4: return GetNextWorkRequired_DIGI(pindexLast, pblock, params);
        default: return GetNextWorkRequired_DIGI(pindexLast, pblock, params);
    }
}


#endif

