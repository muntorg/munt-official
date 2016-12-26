// Copyright (c) 2012-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// File contains modifications by: The Gulden developers
// All modifications:
// Copyright (c) 2016 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#ifndef BITCOIN_VERSION_H
#define BITCOIN_VERSION_H

/**
 * network protocol versioning
 */

static const int PROTOCOL_VERSION = 70014;

//! initial proto version, to be increased after version/verack negotiation
static const int INIT_PROTO_VERSION = 209;

//! In this version, 'getheaders' was introduced.
static const int GETHEADERS_VERSION = 31800;

static const int MIN_PEER_PROTO_VERSION = 70014;

static const int CADDR_TIME_VERSION = 31402;

static const int BIP0031_VERSION = 60000;

static const int MEMPOOL_GD_VERSION = 60002;

static const int NO_BLOOM_VERSION = 70011;

static const int SENDHEADERS_VERSION = 70012;

static const int FEEFILTER_VERSION = 70013;

static const int SHORT_IDS_BLOCKS_VERSION = 70014;

#endif // BITCOIN_VERSION_H
