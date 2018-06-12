// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// File contains modifications by: The Gulden developers
// All modifications:
// Copyright (c) 2018 The Gulden developers
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#ifndef GULDEN_VERSION_BITS_VALIDATION_H
#define GULDEN_VERSION_BITS_VALIDATION_H

#include "versionbits.h"

extern VersionBitsCache versionbitscache;

/** Get the BIP9 state for a given deployment at the current tip. */
ThresholdState VersionBitsTipState(const Consensus::Params& params, Consensus::DeploymentPos pos);

/** Get the numerical statistics for the BIP9 state for a given deployment at the current tip. */
BIP9Stats VersionBitsTipStatistics(const Consensus::Params& params, Consensus::DeploymentPos pos);

/** Get the block height at which the BIP9 deployment switched into the state for the block building on the current tip. */
int VersionBitsTipStateSinceHeight(const Consensus::Params& params, Consensus::DeploymentPos pos);

#endif // GULDEN_VERSION_BITS_VALIDATION_H
