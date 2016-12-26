// Copyright (c) 2015-2016 The Gulden developers
// Authored by: Frank (dt_cdog@yahoo.com) and Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING
//
// This file contains Delta, the Gulden Difficulty Re-adjustment algorithm developed by Frank (dt_cdog@yahoo.com) with various enhancements by Malcolm MacLeod (mmacleod@webmail.co.za)
// The core algorithm works by taking time measurements of four periods (last block; short window; medium window; long window) and then apply a weighting to them.
// This allows the algorithm to react to short term fluctuations while still taking long term block targets into account, which helps prevent it from overreacting.
//
// In addition to the core algorithm several extra rules are then applied in certain situations (e.g. multiple quick blocks) to enhance the behaviour.

#ifdef __APPLE__
#include <TargetConditionals.h>
#endif
#include "diff_common.h"
#include "diff_delta.h"
#ifndef __JAVA__
#ifdef BUILD_IOS
#include "NSData+Bitcoin.h"
#include "arith_uint256.h"
#import "BRMerkleBlock.h"
#import "BRPeerManager.h"
#endif
#include <stdint.h>
#endif

#ifdef __JAVA__
package org.bitcoinj.core;
import org.bitcoinj.store.BlockStore;
import java.math.BigInteger;
import org.bitcoinj.store.BlockStoreException;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
public
class DeltaDiff extends OldDiff {
public
    static
#endif
        unsigned int
        GetNextWorkRequired_DELTA(const INDEX_TYPE pindexLast, const BLOCK_TYPE block, int nPowTargetSpacing, unsigned int nPowLimit, unsigned int nFirstDeltaBlock
#ifdef __JAVA__
                                  ,
                                  final BlockStore blockStore
#endif
                                  )
    {

#ifndef BUILD_IOS
#ifndef __JAVA__

        static int64_t nPrevHeight = 0;
        static int64_t nPrevDifficulty = 0;

        std::string sLogInfo;
#endif
#endif

#ifdef __JAVA__
        try {
#endif

            int64_t nRetargetTimespan = nPowTargetSpacing;

            const unsigned int nProofOfWorkLimit = nPowLimit;

            const unsigned int nLastBlock = 1;
            const unsigned int nShortFrame = 3;
            const unsigned int nMiddleFrame = 24;
            const unsigned int nLongFrame = 576;

            const int64_t nLBWeight = 64;
            const int64_t nShortWeight = 8;
            int64_t nMiddleWeight = 2;
            int64_t nLongWeight = 1;

            const int64_t nLBMinGap = nRetargetTimespan / 6;
            const int64_t nLBMaxGap = nRetargetTimespan * 6;

            const int64_t nQBFrame = nShortFrame + 1;
            const int64_t nQBMinGap = (nRetargetTimespan * PERCENT_FACTOR / 120) * nQBFrame;

            const int64_t nBadTimeLimit = 0;
            const int64_t nBadTimeReplace = nRetargetTimespan / 10;

            const int64_t nLowTimeLimit = nRetargetTimespan * 90 / PERCENT_FACTOR;
            const int64_t nFloorTimeLimit = nRetargetTimespan * 65 / PERCENT_FACTOR;

            const int64_t nDrift = 1;
            int64_t nLongTimeLimit = ((6 * nDrift)) * 60;
            int64_t nLongTimeStep = nDrift * 60;

            unsigned int nMinimumAdjustLimit = (unsigned int)nRetargetTimespan * 75 / PERCENT_FACTOR;

            unsigned int nMaximumAdjustLimit = (unsigned int)nRetargetTimespan * 150 / PERCENT_FACTOR;

            int64_t nDeltaTimespan = 0;
            int64_t nLBTimespan = 0;
            int64_t nShortTimespan = 0;
            int64_t nMiddleTimespan = 0;
            int64_t nLongTimespan = 0;
            int64_t nQBTimespan = 0;

            int64_t nWeightedSum = 0;
            int64_t nWeightedDiv = 0;
            int64_t nWeightedTimespan = 0;

            const INDEX_TYPE pindexFirst = pindexLast; //multi algo - last block is selected on a per algo basis.

            if (pindexLast == NULL)
                return nProofOfWorkLimit;

            if (INDEX_HEIGHT(pindexLast) <= nQBFrame)
                return nProofOfWorkLimit;

            pindexFirst = INDEX_PREV(pindexLast);
            nLBTimespan = INDEX_TIME(pindexLast) - INDEX_TIME(pindexFirst);

            if (nLBTimespan > nBadTimeLimit && nLBTimespan < nLBMinGap)
                nLBTimespan = nLBTimespan * 50 / PERCENT_FACTOR;

            if (nLBTimespan <= nBadTimeLimit)
                nLBTimespan = nBadTimeReplace;

            if (nLBTimespan > nLBMaxGap)
                nLBTimespan = nLBTimespan * 150 / PERCENT_FACTOR;

            pindexFirst = pindexLast;
            for (unsigned int i = 1; pindexFirst != NULL && i <= nQBFrame; i++) {
                nDeltaTimespan = INDEX_TIME(pindexFirst) - INDEX_TIME(INDEX_PREV(pindexFirst));

                if (nDeltaTimespan <= nBadTimeLimit)
                    nDeltaTimespan = nBadTimeReplace;

                if (i <= nShortFrame)
                    nShortTimespan += nDeltaTimespan;
                nQBTimespan += nDeltaTimespan;
                pindexFirst = INDEX_PREV(pindexFirst);
            }

            if (INDEX_HEIGHT(pindexLast) - nFirstDeltaBlock <= nMiddleFrame) {
                nMiddleWeight = nMiddleTimespan = 0;
            } else {
                pindexFirst = pindexLast;
                for (unsigned int i = 1; pindexFirst != NULL && i <= nMiddleFrame; i++) {
                    nDeltaTimespan = INDEX_TIME(pindexFirst) - INDEX_TIME(INDEX_PREV(pindexFirst));

                    if (nDeltaTimespan <= nBadTimeLimit)
                        nDeltaTimespan = nBadTimeReplace;

                    nMiddleTimespan += nDeltaTimespan;
                    pindexFirst = INDEX_PREV(pindexFirst);
                }
            }

            if (INDEX_HEIGHT(pindexLast) - nFirstDeltaBlock <= nLongFrame) {
                nLongWeight = nLongTimespan = 0;
            } else {
                pindexFirst = pindexLast;
                for (unsigned int i = 1; pindexFirst != NULL && i <= nLongFrame; i++)
                    pindexFirst = INDEX_PREV(pindexFirst);

                nLongTimespan = INDEX_TIME(pindexLast) - INDEX_TIME(pindexFirst);
            }

            if ((nQBTimespan > nBadTimeLimit) && (nQBTimespan < nQBMinGap) && (nLBTimespan < nRetargetTimespan * 40 / PERCENT_FACTOR)) {
#ifndef __JAVA__
#ifndef BUILD_IOS
                if (fDebug && (nPrevHeight != INDEX_HEIGHT(pindexLast)))
                    sLogInfo += "<DELTA> Multiple fast blocks - ignoring long and medium weightings.\n";
#endif
#endif
                nMiddleWeight = nMiddleTimespan = nLongWeight = nLongTimespan = 0;
            }

            nWeightedSum = (nLBTimespan * nLBWeight) + (nShortTimespan * nShortWeight);
            nWeightedSum += (nMiddleTimespan * nMiddleWeight) + (nLongTimespan * nLongWeight);
            nWeightedDiv = (nLastBlock * nLBWeight) + (nShortFrame * nShortWeight);
            nWeightedDiv += (nMiddleFrame * nMiddleWeight) + (nLongFrame * nLongWeight);
            nWeightedTimespan = nWeightedSum / nWeightedDiv;

            if (DIFF_ABS(nLBTimespan - nRetargetTimespan) < nRetargetTimespan * 20 / PERCENT_FACTOR) {

                nMinimumAdjustLimit = (unsigned int)nRetargetTimespan * 90 / PERCENT_FACTOR;
                nMaximumAdjustLimit = (unsigned int)nRetargetTimespan * 110 / PERCENT_FACTOR;
            } else if (DIFF_ABS(nLBTimespan - nRetargetTimespan) < nRetargetTimespan * 30 / PERCENT_FACTOR) {

                nMinimumAdjustLimit = (unsigned int)nRetargetTimespan * 80 / PERCENT_FACTOR;
                nMaximumAdjustLimit = (unsigned int)nRetargetTimespan * 120 / PERCENT_FACTOR;
            }

            if (nWeightedTimespan < nMinimumAdjustLimit)
                nWeightedTimespan = nMinimumAdjustLimit;

            if (nWeightedTimespan > nMaximumAdjustLimit)
                nWeightedTimespan = nMaximumAdjustLimit;

            arith_uint256 bnNew;
            SET_COMPACT(bnNew, INDEX_TARGET(pindexLast));
            bnNew = BIGINT_MULTIPLY(bnNew, arith_uint256(nWeightedTimespan));
            bnNew = BIGINT_DIVIDE(bnNew, arith_uint256(nRetargetTimespan));

            nLBTimespan = INDEX_TIME(pindexLast) - INDEX_TIME(INDEX_PREV(pindexLast));
            arith_uint256 bnComp;
            SET_COMPACT(bnComp, INDEX_TARGET(pindexLast));
            if (nLBTimespan > 0 && nLBTimespan < nLowTimeLimit && BIGINT_GREATER_THAN(bnNew, bnComp)) {

                if (nLBTimespan < nFloorTimeLimit) {
                    SET_COMPACT(bnNew, INDEX_TARGET(pindexLast));
                    bnNew = BIGINT_MULTIPLY(bnNew, arith_uint256(95));
                    bnNew = BIGINT_DIVIDE(bnNew, arith_uint256(PERCENT_FACTOR));
#ifndef __JAVA__
#ifndef BUILD_IOS
                    if (fDebug && (nPrevHeight != INDEX_HEIGHT(pindexLast)))
                        sLogInfo += strprintf("<DELTA> Last block time [%ld] was far below target but adjustment still downward, forcing difficulty up by 5%% instead\n", nLBTimespan);
#endif
#endif
                } else {
                    SET_COMPACT(bnNew, INDEX_TARGET(pindexLast));
#ifndef __JAVA__
#ifndef BUILD_IOS
                    if (fDebug && (nPrevHeight != INDEX_HEIGHT(pindexLast)))
                        sLogInfo += strprintf("<DELTA> Last block time [%ld] below target but adjustment still downward, blocking downward adjustment\n", nLBTimespan);
#endif
#endif
                }
            }

            if ((BLOCK_TIME(block) - INDEX_TIME(pindexLast)) > nLongTimeLimit) {

                int64_t nNumMissedSteps = ((BLOCK_TIME(block) - INDEX_TIME(pindexLast) - nLongTimeLimit) / nLongTimeStep) + 1;
                for (int i = 0; i < nNumMissedSteps; ++i) {
                    bnNew = BIGINT_MULTIPLY(bnNew, arith_uint256(110));
                    bnNew = BIGINT_DIVIDE(bnNew, arith_uint256(PERCENT_FACTOR));
                }

#ifndef __JAVA__
#ifndef BUILD_IOS
                if (fDebug && (nPrevHeight != INDEX_HEIGHT(pindexLast) || GET_COMPACT(bnNew) != nPrevDifficulty))
                    sLogInfo += strprintf("<DELTA> Maximum block time hit - halving difficulty %08x %s\n", GET_COMPACT(bnNew), bnNew.ToString().c_str());
#endif
#endif
            }

            SET_COMPACT(bnComp, nProofOfWorkLimit);
            if (BIGINT_GREATER_THAN(bnNew, bnComp))
                SET_COMPACT(bnNew, nProofOfWorkLimit);

#ifndef BUILD_IOS
#ifndef __JAVA__
            if (fDebug) {
                if (nPrevHeight != INDEX_HEIGHT(pindexLast) || GET_COMPACT(bnNew) != nPrevDifficulty) {
                    static CCriticalSection logCS;
                    LOCK(logCS);
                    LogPrintf("<DELTA> Height= %d\n", INDEX_HEIGHT(pindexLast));
                    LogPrintf("%s", sLogInfo.c_str());
                    LogPrintf("<DELTA> nTargetTimespan = %ld nActualTimespan = %ld nWeightedTimespan = %ld \n", nRetargetTimespan, nLBTimespan, nWeightedTimespan);
                    LogPrintf("<DELTA> nShortTimespan/nShortFrame = %ld nMiddleTimespan/nMiddleFrame = %ld nLongTimespan/nLongFrame = %ld \n", nShortTimespan / nShortFrame, nMiddleTimespan / nMiddleFrame, nLongTimespan / nLongFrame);
                    LogPrintf("<DELTA> Before: %08x %s\n", INDEX_TARGET(pindexLast), arith_uint256().SetCompact(INDEX_TARGET(pindexLast)).ToString().c_str());
                    LogPrintf("<DELTA> After:  %08x %s\n", GET_COMPACT(bnNew), bnNew.ToString().c_str());
                    LogPrintf("<DELTA> Rough change percentage (human view): %lf%%\n", -(((bnNew.getdouble() - arith_uint256().SetCompact(INDEX_TARGET(pindexLast)).getdouble()) / arith_uint256().SetCompact(INDEX_TARGET(pindexLast)).getdouble()) * 100));
                }
                nPrevHeight = INDEX_HEIGHT(pindexLast);
                nPrevDifficulty = GET_COMPACT(bnNew);
            }
#endif
#endif

            return GET_COMPACT(bnNew);
#ifdef __JAVA__
        }
        catch (BlockStoreException e) {
            throw new RuntimeException(e);
        }
#endif
    }
#ifdef __JAVA__
} //class DeltaDiff
#endif
