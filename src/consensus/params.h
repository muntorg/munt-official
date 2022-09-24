// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// File contains modifications by: The Centure developers
// All modifications:
// Copyright (c) 2017-2022 The Centure developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the Libre Chain License, see the accompanying
// file COPYING

#ifndef CONSENSUS_PARAMS_H
#define CONSENSUS_PARAMS_H

#include "uint256.h"
#include <map>
#include <string>

namespace Consensus {

enum DeploymentPos
{
    DEPLOYMENT_TESTDUMMY,
    DEPLOYMENT_CSV, // Deployment of BIP68, BIP112, and BIP113.
    // NOTE: Also add new deployments to VersionBitsDeploymentInfo in versionbits.cpp
    MAX_VERSION_BITS_DEPLOYMENTS
};

enum DeploymentType
{
    DEPLOYMENT_POW,
    DEPLOYMENT_WITNESS,
    DEPLOYMENT_BOTH
};

/**
 * Struct for each individual consensus rule change using BIP9.
 */
struct BIP9Deployment {
    BIP9Deployment()
    {
        requiredProtoUpgradePercent = 0;
        protoVersion = 0;
    };

    /** Bit position to select the particular bit in nVersion. */
    int bit;
    /** Start MedianTime for version bits miner confirmation. Can be a date in the past */
    int64_t nStartTime;
    /** Timeout/expiry MedianTime for the deployment attempt. */
    int64_t nTimeout;
    /** Whether to consider version bits of PoW header, version bits of witness header, or both */
    DeploymentType type;
    int requiredProtoUpgradePercent;
    int protoVersion;
};

/**
 * Parameters that influence chain consensus.
 */
struct Params {
    uint256 hashGenesisBlock;
    /** Block height and hash at which BIP34 becomes active */
    int BIP34Height;
    uint256 BIP34Hash;
    /** Block height at which BIP65 becomes active */
    int BIP65Height;
    /** Block height at which BIP66 becomes active */
    int BIP66Height;
    /**
     * Minimum blocks including miner confirmation of the total of 2016 blocks in a retargeting period,
     * (nPowTargetTimespan / nPowTargetSpacing) which is also used for BIP9 deployments.
     * Examples: 1916 for 95%, 1512 for testchains.
     */
    uint32_t nRuleChangeActivationThreshold;
    uint32_t nMinerConfirmationWindow;
    BIP9Deployment vDeployments[MAX_VERSION_BITS_DEPLOYMENTS];
    uint64_t fixedRewardIntroductionHeight;
    uint64_t fixedRewardReductionHeight;
    uint64_t pow2Phase2FirstBlockHeight;
    uint64_t pow2Phase3FirstBlockHeight;
    uint64_t devBlockSubsidyActivationHeight;
    uint64_t pow2Phase4FirstBlockHeight;
    uint64_t pow2Phase5FirstBlockHeight;
    uint64_t pow2WitnessSyncHeight;
    uint64_t halvingIntroductionHeight;
    uint64_t finalSubsidyBlockHeight;
    
    /** Proof of work parameters */
    uint256 powLimit;
    bool fPowAllowMinDifficultyBlocks;
    bool fPowNoRetargeting;
    int64_t nPowTargetSpacing;
    int64_t nPowTargetTimespan;
    uint256 nMinimumChainWork;
    uint256 defaultAssumeValid;
};
} // namespace Consensus

#endif
