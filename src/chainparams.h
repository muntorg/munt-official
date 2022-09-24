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

#ifndef CHAINPARAMS_H
#define CHAINPARAMS_H

#include "chainparamsbase.h"
#include "consensus/params.h"
#include "primitives/block.h"
#include "protocol.h"
#include "key.h"

#include <memory>
#include <vector>

struct CDNSSeedData {
    std::string name, host;
    bool supportsServiceBitsFiltering;
    CDNSSeedData(const std::string &strName, const std::string &strHost, bool supportsServiceBitsFilteringIn = false) : name(strName), host(strHost), supportsServiceBitsFiltering(supportsServiceBitsFilteringIn) {}
};

struct SeedSpec6 {
    uint8_t addr[16];
    uint16_t port;
};

struct CheckPointEntry
{
    CheckPointEntry() : nTime(0) {}
    CheckPointEntry(uint256 _hash, int64_t _nTime) : hash(_hash), nTime(_nTime) {}
    uint256 hash;
    int64_t nTime;
};

using CCheckpointData = std::map<int, CheckPointEntry>;

/**
 * CChainParams defines various tweakable parameters of a given instance of the
 * Gulden system. There are three: the main network on which people trade goods
 * and services, the public test network which gets reset from time to time and
 * a regression test mode which is intended for private networks only. It has
 * minimal difficulty to ensure that blocks can be found instantly.
 */
class CChainParams
{
public:
    enum Base58Type {
        PUBKEY_ADDRESS,
        SCRIPT_ADDRESS,
        POW2_WITNESS_ADDRESS,
        SECRET_KEY,
        EXT_PUBLIC_KEY,
        EXT_SECRET_KEY,

        MAX_BASE58_TYPES
    };

    const Consensus::Params& GetConsensus() const { return consensus; }
    const CMessageHeader::MessageStartChars& MessageStart() const { return pchMessageStart; }
    const std::vector<unsigned char>& AlertKey() const { return vAlertPubKey; }
    int GetDefaultPort() const { return nDefaultPort; }

    const CBlock& GenesisBlock() const { return genesis; }
    /** Default value for -checkmempool and -checkblockindex argument */
    bool DefaultConsistencyChecks() const { return fDefaultConsistencyChecks; }
    /** Policy: Filter transactions that do not match well-defined patterns */
    bool RequireStandard() const { return fRequireStandard; }
    uint64_t PruneAfterHeight() const { return nPruneAfterHeight; }
    /** Make miner stop after a block is found. In RPC, don't return until nGenProcLimit blocks are generated */
    bool MineBlocksOnDemand() const { return fMineBlocksOnDemand; }
    /** Return the BIP70 network string (main, test or regtest) */
    std::string NetworkIDString() const { return strNetworkID; }
    const std::vector<CDNSSeedData>& DNSSeeds() const { return vSeeds; }
    const std::vector<unsigned char>& Base58Prefix(Base58Type type) const { return base58Prefixes[type]; }
    const std::vector<SeedSpec6>& FixedSeeds() const { return vFixedSeeds; }
    const CCheckpointData& Checkpoints() const { return checkpointData; }
    void UpdateVersionBitsParameters(Consensus::DeploymentPos d, int64_t nStartTime, int64_t nTimeout);

    bool IsTestnet() const { return fIsTestnet; }
    bool IsRegtest() const { return fIsRegtest; }
    bool IsRegtestLegacy() const { return fIsRegtestLegacy; }
    //fixme: (testnet) remove after official testnet restarted
    bool IsOfficialTestnetV1() const { return fIsOfficialTestnetV1; }
    
    // Testnet only variable (allow anyone to witness using genesis witness accounts)
    CKey genesisWitnessPrivKey;
    // Testnet only variable - specify number of 'genesis witness' accounts to create
    uint32_t numGenesisWitnesses=0;
    // Testnet only variable - specify how much each individual 'genesis witness' should be in terms of network weight
    // NB! This is used as a divisor of the network weight. So '100' == 1% of network weight per account, with 'numGenesisWitnesses' of 10 this would result in 10% total over all accounts
    uint32_t genesisWitnessWeightDivisor=0;
    
protected:
    CChainParams();

    Consensus::Params consensus;
    CMessageHeader::MessageStartChars pchMessageStart;
    //! Raw pub key bytes for the broadcast alert signing key.
    std::vector<unsigned char> vAlertPubKey;
    int nDefaultPort;
    uint64_t nPruneAfterHeight;
    std::vector<CDNSSeedData> vSeeds;
    std::vector<unsigned char> base58Prefixes[MAX_BASE58_TYPES];
    std::string strNetworkID;
    CBlock genesis;
    std::vector<SeedSpec6> vFixedSeeds;
    bool fDefaultConsistencyChecks;
    bool fRequireStandard;
    bool fMineBlocksOnDemand;
    CCheckpointData checkpointData;
    bool fIsOfficialTestnetV1;
    bool fIsTestnet;
    bool fIsRegtest;
    bool fIsRegtestLegacy;
};

/**
 * Creates and returns a std::unique_ptr<CChainParams> of the chosen chain.
 * @returns a CChainParams* of the chosen chain.
 * @throws a std::runtime_error if the chain is not supported.
 */
std::unique_ptr<CChainParams> CreateChainParams(const std::string& chain);

/**
 * Return the currently selected parameters. This won't change after app
 * startup, except for unit tests.
 */
const CChainParams &Params();

/**
 * Sets the params returned by Params() to those for the given BIP70 chain name.
 * @throws std::runtime_error when the chain is not supported.
 */
void SelectParams(const std::string& chain);

/**
 * Free any globally stored chain params for app shutdown
 */
void FreeParams();

/**
 * Allows modifying the Version Bits regtest parameters.
 */
void UpdateVersionBitsParameters(Consensus::DeploymentPos d, int64_t nStartTime, int64_t nTimeout);

#endif
