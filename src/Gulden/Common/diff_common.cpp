// Copyright (c) 2015 The Gulden developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifdef __APPLE__
    #include <TargetConditionals.h>
#endif
#include "diff_common.h"
#include "diff_delta.h"
#include "diff_old.h"


#ifdef __JAVA__
package org.bitcoinj.core;
import org.bitcoinj.store.BlockStore;
import java.math.BigInteger;
import org.bitcoinj.store.BlockStoreException;
public class CommonDiff extends DeltaDiff {
public static int nDeltaSwitchoverBlock = DIFF_SWITCHOVER(350000, 1, 250000);
public static int nOldDiffSwitchoverBlock = DIFF_SWITCHOVER(200, 100, 260000);
public static
#endif
unsigned int GetNextWorkRequired(const INDEX_TYPE indexLast, const BLOCK_TYPE block, unsigned int nPowTargetSpacing, unsigned int nPowLimit
#ifdef __JAVA__
,final BlockStore blockStore
#endif	
)
{
    #ifndef __JAVA__
    static int nDeltaSwitchoverBlock = DIFF_SWITCHOVER(350000, 1, 250000);
    static int nOldDiffSwitchoverBlock = DIFF_SWITCHOVER(200, 100, 260000);
    #endif


    if (INDEX_HEIGHT(indexLast)+1 >= nOldDiffSwitchoverBlock)
    {
        if (INDEX_HEIGHT(indexLast)+1 >= nDeltaSwitchoverBlock)
        {
            #ifdef __JAVA__
            return GetNextWorkRequired_DELTA(indexLast, block, (int)nPowTargetSpacing, nPowLimit, nDeltaSwitchoverBlock, blockStore);
            #else
	    return GetNextWorkRequired_DELTA(indexLast, block, nPowTargetSpacing, nPowLimit, nDeltaSwitchoverBlock);
            #endif
        }
        else
        {
            return 524287999;
        }
    }
    return diff_old(INDEX_HEIGHT(indexLast)+1, nPowLimit);
}


#ifdef __JAVA__
}
#endif
