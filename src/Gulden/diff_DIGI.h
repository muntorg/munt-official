// Digi algorithm should never be used until at least 2 blocks are mined.
// Contains code by RealSolid & WDC
// Cleaned up for use in Gulden by GeertJohan (dead code removal since Gulden retargets every block)

#ifndef GULDEN_DIFF_DIGI_H
#define GULDEN_DIFF_DIGI_H

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

#endif
