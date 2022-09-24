// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// File contains modifications by: The Centure developers
// All modifications:
// Copyright (c) 2016-2022 The Centure developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the Libre Chain License, see the accompanying
// file COPYING

#include "pow.h"

#include "arith_uint256.h"
#include "chain.h"
#include "primitives/block.h"
#include "uint256.h"
#include "crypto/hash/sigma/sigma.h"
#include "random.h"
#include <thread>

#include "chainparams.h"
#include <validation/validation.h> //For VALIDATION_MOBILE

uint64_t verifyFactor=200;

bool CheckProofOfWork(const CBlock* block, const Consensus::Params& params)
{    
    bool fNegative;
    bool fOverflow;
    arith_uint256 bnTarget;

    bnTarget.SetCompact(block->nBits, &fNegative, &fOverflow);

    static bool fRegTest = Params().IsRegtest();
    static bool fRegTestLegacy = Params().IsRegtestLegacy();
    if (!(fRegTest||fRegTestLegacy) && block->nTime > 1571320800)
    {
        uint256 newProofOfWorkLimit = uint256S("0x003fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        // Check range
        if (fNegative || bnTarget == 0 || fOverflow || bnTarget > UintToArith256(newProofOfWorkLimit))
            return false;
    }
    else
    {
        // Check range
        if (fNegative || bnTarget == 0 || fOverflow || bnTarget > UintToArith256(params.powLimit))
            return false;
    }

    //fixme: (SIGMA) - Post activation we can simplify this.
    // Check proof of work matches claimed amount
    if (block->nTime > defaultSigmaSettings.activationDate)
    {
        #ifdef VALIDATION_MOBILE
            //fixme: (SIGMA) (PHASE5) (HIGH) Remove/improve this once we have witness-header-sync; this is a temporary measure to keep SPV performance adequate on low power devices for now.
            // Benchmarking on 6 core mobile device showed roughly double performance when using 2 threads instead of 1
            // when further increasing the number of threads (3 and 4) performance stayed roughly the same though at the cost of
            // higher cpu and energy consumption and overall app/device responsiveness.
            // As such the number of threads is limited to 2 (if reported by the OS). Further research and benchmarking on a wider range of devices
            // would be needed to create a solution that gets the most out of a wide range of OS and devices. This might not be worth it though
            // as for mobile/SPV the witness-header-sync will probably completely skip the pow check in the future.
            uint32_t numVerifyThreads = std::min(defaultSigmaSettings.numVerifyThreads, (uint64_t)std::max(1, std::min(2, (int)std::thread::hardware_concurrency())));
            static sigma_verify_context verify(defaultSigmaSettings, numVerifyThreads);
            static RecursiveMutex csPOW;
            LOCK(csPOW);

            int verifyLevel = GetRand(verifyFactor);
            if (verifyLevel == 0)
            {
                return verify.verifyHeader<1>(*block);
            }
            else if (verifyLevel == 1)
            {
                return verify.verifyHeader<2>(*block);
            }
            else
            {
                return true;
            }
        #else
            static sigma_verify_context verify(defaultSigmaSettings,std::min(defaultSigmaSettings.numVerifyThreads, (uint64_t)std::thread::hardware_concurrency()));
            static RecursiveMutex csPOW;
            LOCK(csPOW);

            // Testnet optimisation - only verify last 5 days worth of blocks
            if (Params().IsOfficialTestnetV1() && (block->nTime < GetTime() - 86400*5))
            {
                return true;
            }
            //fixme: (SIGMA) - Detect faster machines and disable this optimisation for them, this will further increase network security.
            // We speed up verification by doing a half verify 40% of the time instead of a full verify
            // As a half verify has a 50% chance of detecting a 'half valid' hash an attacker has only a 20% chance of a node accepting his header without banning him
            // This should provide a ~20% speed up for slow machines
            int verifyLevel = GetRand(100);
            if (verifyLevel < 20)
            {
                return verify.verifyHeader<1>(*block);
            }
            else if (verifyLevel < 40)
            {
                return verify.verifyHeader<2>(*block);
            }
            return verify.verifyHeader<0>(*block);
        #endif
    }
    else
    {
        if (UintToArith256(block->GetPoWHash()) > bnTarget)
            return false;
    }

    return true;
}
