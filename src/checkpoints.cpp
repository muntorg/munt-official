// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "checkpoints.h"

#include "chain.h"
#include "chainparams.h"
#include "validation/validation.h"
#include "uint256.h"

#include <boost/range/adaptor/reversed.hpp>
#include <stdint.h>

namespace Checkpoints {

    CBlockIndex* GetLastCheckpointIndex()
    {
        for (const auto& i: boost::adaptors::reverse(Params().Checkpoints()))
        {
            BlockMap::const_iterator t = mapBlockIndex.find(i.second.hash);
            if (t != mapBlockIndex.end())
                return t->second;
        }
        return nullptr;
    }

    int LastCheckPointHeight()
    {
        auto lastCheckpoint = Params().Checkpoints().rbegin();
        return lastCheckpoint->first;
    }
    
    int LastCheckpointBefore(int64_t beforeTime, CheckPointEntry& entry)
    {
        for (const auto& i: boost::adaptors::reverse(Params().Checkpoints()))
        {
            if (i.second.nTime < beforeTime)
            {
                entry = i.second;
                return i.first;
            }
        }
        return -1;
    }

} // namespace Checkpoints
