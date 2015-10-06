/* current difficulty formula, darkcoin - DarkGravity v3, written by Evan Duffield - evan@darkcoin.io */

#ifndef GULDEN_DIFF_DGW_H
#define GULDEN_DIFF_DGW_H

unsigned int static DarkGravityWave3(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params& params)
{    
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



unsigned int static GetNextWorkRequired_DGW3(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params& params)
{
    return DarkGravityWave3(pindexLast, pblock, params);
}

#endif
