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
            const uint256& hash = i.second;
            BlockMap::const_iterator t = mapBlockIndex.find(hash);
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

} // namespace Checkpoints
