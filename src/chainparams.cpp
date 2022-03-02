// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// File contains modifications by: The Gulden developers
// All modifications:
// Copyright (c) 2016-2020 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#include "chainparams.h"
#include "pow/pow.h"
#include "consensus/merkle.h"
#include "crypto/hash/sigma/sigma.h"

#include "tinyformat.h"
#include "util.h"
#include "utilstrencodings.h"

#include <assert.h>

#include <cstdio>
#include "chainparamsseeds.h"
#include <validation/witnessvalidation.h>

static CBlock CreateGenesisBlock(const std::vector<unsigned char>& timestamp, const CScript& genesisOutputScript, uint32_t nTime, uint32_t nNonce, uint32_t nBits, int32_t nVersion, const CAmount& genesisReward)
{
    CMutableTransaction txNew(1);
    txNew.nVersion = 1;
    txNew.vin.resize(1);
    txNew.vout.resize(1);
    txNew.vin[0].scriptSig = CScript() << 486604799 << CScriptNum(4) << timestamp;
    txNew.vout[0].nValue = genesisReward;
    txNew.vout[0].output.scriptPubKey = genesisOutputScript;

    CBlock genesis;
    genesis.nTime    = nTime;
    genesis.nBits    = nBits;
    genesis.nNonce   = nNonce;
    genesis.nVersion = nVersion;
    genesis.vtx.push_back(MakeTransactionRef(std::move(txNew)));
    genesis.hashPrevBlock.SetNull();
    genesis.hashMerkleRoot = BlockMerkleRoot(genesis.vtx.begin(), genesis.vtx.end());
    return genesis;
}

/**
 * Build the genesis block. Note that the output of its generation
 * transaction cannot be spent since it did not originally exist in the
 * database.
 *
 * CBlock(hash=000000000019d6, ver=1, hashPrevBlock=00000000000000, hashMerkleRoot=4a5e1e, nTime=1231006505, nBits=1d00ffff, nNonce=2083236893, vtx=1)
 *   CTransaction(hash=4a5e1e, ver=1, vin.size=1, vout.size=1, nLockTime=0)
 *     CTxIn(COutPoint(000000, -1), coinbase 04ffff001d0104455468652054696d65732030332f4a616e2f32303039204368616e63656c6c6f72206f6e206272696e6b206f66207365636f6e64206261696c6f757420666f722062616e6b73)
 *     CTxOut(nValue=50.00000000, scriptPubKey=0x5F1DF16B2B704C8A578D0B)
 *   vMerkleTree: 4a5e1e
 */
static CBlock CreateGenesisBlock(uint32_t nTime, uint32_t nNonce, uint32_t nBits, int32_t nVersion, const CAmount& genesisReward)
{
    const CScript genesisOutputScript = CScript() << 0x0 << OP_CHECKSIG;
    return CreateGenesisBlock(ParseHex("4f6e206a616e756172692031737420746865204475746368206c6f73742074686572652062656c6f7665642047756c64656e"), genesisOutputScript, nTime, nNonce, nBits, nVersion, genesisReward);
}

CChainParams::CChainParams(): fIsOfficialTestnetV1(false), fIsTestnet(false), fIsRegtestLegacy(false), fIsRegtest(false) {}

void CChainParams::UpdateVersionBitsParameters(Consensus::DeploymentPos d, int64_t nStartTime, int64_t nTimeout)
{
    consensus.vDeployments[d].nStartTime = nStartTime;
    consensus.vDeployments[d].nTimeout = nTimeout;
}

/**
 * Main network
 */
/**
 * What makes a good checkpoint block?
 * + Is surrounded by blocks with reasonable timestamps
 *   (no blocks before with a timestamp after, none after with
 *    timestamp before)
 * + Contains no strange transactions
 */

class CMainParams : public CChainParams {
public:
    CMainParams() {
        strNetworkID = "main";
        consensus.BIP34Height = 1;
        consensus.BIP65Height = 1; 
        consensus.BIP66Height = 1; 
        consensus.powLimit =  uint256S("0x003fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.nPowTargetTimespan = 3.5 * 24 * 60 * 60; // 3.5 days
        consensus.nPowTargetSpacing = 300; // 5 minutes
        consensus.fPowAllowMinDifficultyBlocks = false;
        consensus.fPowNoRetargeting = false;
        consensus.nRuleChangeActivationThreshold = 1916; // 95% of 2016
        consensus.nMinerConfirmationWindow = 2016; // nPowTargetTimespan / nPowTargetSpacing
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 1199145601; // January 1, 2008
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = 1230767999; // December 31, 2008
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].type = Consensus::DEPLOYMENT_POW;

        // Deployment of BIP68, BIP112, and BIP113.
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].bit = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nStartTime = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nTimeout = Consensus::BIP9Deployment::ALWAYS_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].type = Consensus::DEPLOYMENT_POW;
       
        consensus.pow2Phase2FirstBlockHeight=0;
        consensus.pow2Phase3FirstBlockHeight=0;
        consensus.pow2Phase4FirstBlockHeight=0;
        consensus.pow2Phase5FirstBlockHeight=0;

        // Message start string to avoid accidental cross communication with other chains or software.
        pchMessageStart[0] = 0xfc; // 'N' + 0xb0
        pchMessageStart[1] = 0xfd; // 'O' + 0xb0
        pchMessageStart[2] = 0xfc; // 'V' + 0xb0
        pchMessageStart[3] = 0xfd; // 'O' + 0xb0
        vAlertPubKey = ParseHex("073513ffe7147aba88d33aea4da129d8a2829c545526d5d854ab51d5778f4d0625431ba1c5a3245bdfe8736b127fdfdb488de72640727d37355c4c3a66c547efad");
        nDefaultPort = 9233;
        nPruneAfterHeight = 200000;
        
        //PoW paramaters for SIGMA
        defaultSigmaSettings.arenaSizeKb = 12*1024*1024;
        defaultSigmaSettings.argonSlowHashRoundCost = 14;
        defaultSigmaSettings.fastHashSizeBytes = 400;
        defaultSigmaSettings.verify();

        {
            numGenesisWitnesses = 400;
            genesisWitnessWeightDivisor = 2000;
            
            // Don't bother creating the genesis block if we haven't started ECC yet (e.g. we are being called from the help text)
            // We can't initialise key anyway unless the app has first initialised ECC, and the help doesn't need the genesis block, creating it twice is a waste of cpu cycles
            {
                CMutableTransaction txNew(CTransaction::CURRENT_VERSION);
                txNew.vin.resize(1);
                txNew.vin[0].SetPrevOutNull();
                txNew.vin[0].segregatedSignatureData.stack.clear();
                txNew.vin[0].segregatedSignatureData.stack.push_back(std::vector<unsigned char>());
                CVectorWriter(0, 0, txNew.vin[0].segregatedSignatureData.stack[0], 0) << VARINT(0);
                txNew.vin[0].segregatedSignatureData.stack.push_back({'F','M'});
                
                {
                    CKeyID pubKeyIDWitness;
                    pubKeyIDWitness.SetHex("86cd9223dd76fcc70c897db31f6716036e00b9ce");
                    CKeyID pubKeyIDSpend;
                    pubKeyIDSpend.SetHex("1584c90cb7e3f4703be87ec643fc6f53a933ce43");

                    CTxOut renewedWitnessTxOutput;
                    renewedWitnessTxOutput.SetType(CTxOutType::PoW2WitnessOutput);
                    renewedWitnessTxOutput.output.witnessDetails.spendingKeyID = pubKeyIDSpend;
                    renewedWitnessTxOutput.output.witnessDetails.witnessKeyID = pubKeyIDWitness;
                    renewedWitnessTxOutput.output.witnessDetails.lockFromBlock = 1;
                    renewedWitnessTxOutput.output.witnessDetails.lockUntilBlock = std::numeric_limits<uint64_t>::max();
                    renewedWitnessTxOutput.output.witnessDetails.failCount = 0;
                    renewedWitnessTxOutput.output.witnessDetails.actionNonce = 1;
                    renewedWitnessTxOutput.nValue=0;
                    for (uint32_t i=0; i<numGenesisWitnesses;++i)
                    {
                        txNew.vout.push_back(renewedWitnessTxOutput);
                    }
                }
                
                {
                    std::vector<unsigned char> data(ParseHex(devSubsidyAddress));
                    CPubKey addressPubKey(data.begin(), data.end());
                    
                    // Premine; break into multiple parts so that its easier to use for ICO payouts (994.744.000 total)
                    CTxOut subsidyOutput;
                    subsidyOutput.SetType(CTxOutType::StandardKeyHashOutput);
                    subsidyOutput.output.standardKeyHash = CTxOutStandardKeyHash(addressPubKey.GetID());
                    subsidyOutput.nValue = 858'000'000*COIN;
                    txNew.vout.push_back(subsidyOutput);
                    subsidyOutput.nValue = 10000*COIN;
                    for (int i=0;i<10000;++i)
                    {
                        txNew.vout.push_back(subsidyOutput);
                    }
                    subsidyOutput.nValue = 2000*COIN;
                    for (int i=0;i<6000;++i)
                    {
                        txNew.vout.push_back(subsidyOutput);
                    }
                    subsidyOutput.nValue = 1000*COIN;
                    for (int i=0;i<24744;++i)
                    {
                        txNew.vout.push_back(subsidyOutput);
                    }
                }

                genesis.nTime    = 1593524095;
                genesis.nBits    = arith_uint256((~arith_uint256(0) >> 10)).GetCompact();
                genesis.nNonce   = 900005926;
                genesis.nVersion = 536870912;
                genesis.vtx.push_back(MakeTransactionRef(std::move(txNew)));
                genesis.hashPrevBlock.SetNull();
                genesis.hashMerkleRoot = BlockMerkleRoot(genesis.vtx.begin(), genesis.vtx.end());
                genesis.hashMerkleRootPoW2Witness = BlockMerkleRoot(genesis.vtx.begin(), genesis.vtx.end());
                genesis.witnessHeaderPoW2Sig.resize(65);
                
                genesis.nTimePoW2Witness = genesis.nTime+1;
                genesis.nVersionPoW2Witness = genesis.nVersion;
            }
        }
        
        consensus.hashGenesisBlock = genesis.GetHashPoW2();
        assert(consensus.hashGenesisBlock == uint256S("0xc866fdd44f0451466f20789210e1d53585ae94959f30d82ed2d6f02abab7711f"));
        assert(genesis.hashMerkleRoot == uint256S("0x2cb5137e16879fbe4f1180a0805f58d0acb75e55c1b2097f6d86fb2320b7f012"));
        assert(genesis.hashMerkleRootPoW2Witness == uint256S("0x2cb5137e16879fbe4f1180a0805f58d0acb75e55c1b2097f6d86fb2320b7f012"));

        vSeeds.push_back(CDNSSeedData("seed 0",  "seed1.florin.org", false));
        vSeeds.push_back(CDNSSeedData("seed 1",  "seed2.florin.org", false));       

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,53);// 'N'
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,113);// 'n'
        base58Prefixes[POW2_WITNESS_ADDRESS] = std::vector<unsigned char>(1,20);// 'W'
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,190);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x53, 0x13, 0x20, 0x90};
        base58Prefixes[EXT_SECRET_KEY] = {0x54, 0x14, 0x21, 0x91};

        vFixedSeeds = std::vector<SeedSpec6>(pnSeed6_main, pnSeed6_main + ARRAYLEN(pnSeed6_main));

        fDefaultConsistencyChecks = false;
        fRequireStandard = true;
        fMineBlocksOnDemand = false;

        checkpointData = (CCheckpointData)
        {
            {
                #include "data/chainparams_mainnet_static_checkpoint_data.cpp"
            }
        };

        // By default assume that the signatures in ancestors of this block are valid.
        if (!checkpointData.empty())
        {
            consensus.defaultAssumeValid = checkpointData.rbegin()->second.hash;
        }
        else
        {
            consensus.defaultAssumeValid = uint256S("");
        }
        
        // The best chain should have at least this much work.
        consensus.nMinimumChainWork = uint256S("0000000000000000000000000000000000000000000000000000000272bafe52");
    }
};


void GenerateGenesisBlock(CBlock& genesis, std::string seedKey, CKey& genesisWitnessPrivKey, uint32_t numGenesisWitnesses, uint64_t nTime, uint64_t nBits, uint64_t nNonce, bool blockNeedsGeneration, bool useSigma, Consensus::Params& consensus)
{
    // We MUST have ECC active for key generation
    ECC_Start();
    {
        CMutableTransaction txNew(CTransaction::CURRENT_VERSION);
        txNew.vin.resize(1);
        txNew.vin[0].SetPrevOutNull();
        txNew.vin[0].segregatedSignatureData.stack.clear();
        txNew.vin[0].segregatedSignatureData.stack.push_back(std::vector<unsigned char>());
        CVectorWriter(0, 0, txNew.vin[0].segregatedSignatureData.stack[0], 0) << VARINT(0);
        txNew.vin[0].segregatedSignatureData.stack.push_back(ParseHex("4f6e206a616e756172692031737420746865204475746368206c6f73742074686572652062656c6f7665642047756c64656e"));
        
        {
            std::string sKey = seedKey;
            sKey.resize(32, 0);
            genesisWitnessPrivKey.Set((unsigned char*)&sKey[0],(unsigned char*)&sKey[0]+32, true);
            
            CTxOut renewedWitnessTxOutput;
            renewedWitnessTxOutput.SetType(CTxOutType::PoW2WitnessOutput);
            renewedWitnessTxOutput.output.witnessDetails.spendingKeyID = genesisWitnessPrivKey.GetPubKey().GetID();
            renewedWitnessTxOutput.output.witnessDetails.witnessKeyID = genesisWitnessPrivKey.GetPubKey().GetID();
            renewedWitnessTxOutput.output.witnessDetails.lockFromBlock = 1;
            renewedWitnessTxOutput.output.witnessDetails.lockUntilBlock = 900000;
            renewedWitnessTxOutput.output.witnessDetails.failCount = 0;
            renewedWitnessTxOutput.output.witnessDetails.actionNonce = 1;
            renewedWitnessTxOutput.nValue=0;
            for (uint32_t i=0; i<numGenesisWitnesses;++i)
            {
                txNew.vout.push_back(renewedWitnessTxOutput);
            }
        }

        genesis.nTime    = nTime;
        genesis.nBits    = nBits;
        genesis.nNonce   = nNonce;
            
        genesis.nVersion = 536870912;
        genesis.vtx.push_back(MakeTransactionRef(std::move(txNew)));
        genesis.hashPrevBlock.SetNull();
        genesis.hashMerkleRoot = BlockMerkleRoot(genesis.vtx.begin(), genesis.vtx.end());
        genesis.hashMerkleRootPoW2Witness = BlockMerkleRoot(genesis.vtx.begin(), genesis.vtx.end());
        genesis.witnessHeaderPoW2Sig.resize(65);

        if (blockNeedsGeneration)
        {
            if (useSigma)
            {
                uint256 foundBlockHash;
                std::atomic<uint64_t> halfHashCounter=0;
                bool interrupt=false;
                sigma_context generateContext(defaultSigmaSettings, defaultSigmaSettings.arenaSizeKb, std::max(GetNumCores(), 1), std::max(GetNumCores(), 1));
                generateContext.prepareArenas(genesis);
                generateContext.mineBlock(&genesis, halfHashCounter, foundBlockHash, interrupt);
            }
            else
            {
                while(UintToArith256(genesis.GetPoWHash()) > UintToArith256(consensus.powLimit))
                {
                    ++genesis.nNonce;
                }
            }
        }
        genesis.nTimePoW2Witness = genesis.nTime+1;
        genesis.nVersionPoW2Witness = genesis.nVersion;
    }
    ECC_Stop();
            
}

/**
 * Testnet
 */
class CTestNetParams : public CChainParams {
public:
    CTestNetParams() {
        fIsTestnet = true;

        strNetworkID = "test";
        consensus.BIP34Height = 21111;
        consensus.BIP65Height = 581885; // 00000000007f6655f22f98e72ed80d8b06dc761d5da09df0fa1dc4be4f861eb6
        consensus.BIP66Height = 330776; // 000000002104c8c45e99a8853285a3b592602a3ccde2b832481da85e9e4ba182
        consensus.powLimit =  uint256S("0x003fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.nPowTargetTimespan = 14 * 24 * 60 * 60; // two weeks

        //PoW paramaters for SIGMA
        defaultSigmaSettings.arenaSizeKb = 4*1024*1024;
        defaultSigmaSettings.argonSlowHashRoundCost = 12;
        defaultSigmaSettings.fastHashSizeBytes = 300;
        defaultSigmaSettings.verify();

        std::string sTestnetParams = GetArg("-testnet", "");
        if (!sTestnetParams.empty())
        {
            assert(sTestnetParams.find(":")!=std::string::npos);
            assert(sTestnetParams[0] == 'S' || sTestnetParams[0] == 'C');

            int targetInterval = atoi(sTestnetParams.substr(sTestnetParams.find(":")+1));
            int64_t seedTimestamp = atoi64(sTestnetParams.substr(1,sTestnetParams.find(":")));

            if (sTestnetParams == "S1595347850:60")
            {
                fIsOfficialTestnetV1 = true;
            }
            defaultSigmaSettings.activationDate = seedTimestamp+300;

            consensus.nPowTargetSpacing = targetInterval;
            consensus.fPowAllowMinDifficultyBlocks = false;
            consensus.fPowNoRetargeting = false;
            consensus.nRuleChangeActivationThreshold = 15; // 75% for testchains
            consensus.nMinerConfirmationWindow = 20; // nPowTargetTimespan / nPowTargetSpacing

            consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 0;
            consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 0;
            consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = 999999999999ULL;
            consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].type = Consensus::DEPLOYMENT_POW;

            // Deployment of BIP68, BIP112, and BIP113.
            consensus.vDeployments[Consensus::DEPLOYMENT_CSV].bit = 0;
            consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nStartTime = 0;
            consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nTimeout = 999999999999ULL;
            consensus.vDeployments[Consensus::DEPLOYMENT_CSV].type = Consensus::DEPLOYMENT_POW;

            // The best chain should have at least this much work.
            consensus.nMinimumChainWork = uint256S("");

            // By default assume that the signatures in ancestors of this block are valid.
            if (!checkpointData.empty())
            {
                consensus.defaultAssumeValid = checkpointData.rbegin()->second.hash;
            }
            else
            {
                consensus.defaultAssumeValid = uint256S("");
            }

            consensus.pow2Phase2FirstBlockHeight=0;
            consensus.pow2Phase3FirstBlockHeight=0;
            consensus.pow2Phase4FirstBlockHeight=0;
            consensus.pow2Phase5FirstBlockHeight=0;

            numGenesisWitnesses = 10;
            genesisWitnessWeightDivisor = 100;
                
            // Don't bother creating the genesis block if we haven't started ECC yet (e.g. we are being called from the help text)
            // We can't initialise key anyway unless the app has first initialised ECC, and the help doesn't need the genesis block, creating it twice is a waste of cpu cycles
            {
                ECC_Start();
                {
                    CMutableTransaction txNew(CTransaction::CURRENT_VERSION);
                    txNew.vin.resize(1);
                    txNew.vin[0].SetPrevOutNull();
                    txNew.vin[0].segregatedSignatureData.stack.clear();
                    txNew.vin[0].segregatedSignatureData.stack.push_back(std::vector<unsigned char>());
                    CVectorWriter(0, 0, txNew.vin[0].segregatedSignatureData.stack[0], 0) << VARINT(0);
                    txNew.vin[0].segregatedSignatureData.stack.push_back(ParseHex("4f6e206a616e756172692031737420746865204475746368206c6f73742074686572652062656c6f7665642047756c64656e"));
                    
                    {
                        std::string sKey = sTestnetParams;
                        sKey.resize(32, 0);
                        genesisWitnessPrivKey.Set((unsigned char*)&sKey[0],(unsigned char*)&sKey[0]+32, true);
                        
                        CTxOut renewedWitnessTxOutput;
                        renewedWitnessTxOutput.SetType(CTxOutType::PoW2WitnessOutput);
                        renewedWitnessTxOutput.output.witnessDetails.spendingKeyID = genesisWitnessPrivKey.GetPubKey().GetID();
                        renewedWitnessTxOutput.output.witnessDetails.witnessKeyID = genesisWitnessPrivKey.GetPubKey().GetID();
                        renewedWitnessTxOutput.output.witnessDetails.lockFromBlock = 1;
                        renewedWitnessTxOutput.output.witnessDetails.lockUntilBlock = 900000;
                        renewedWitnessTxOutput.output.witnessDetails.failCount = 0;
                        renewedWitnessTxOutput.output.witnessDetails.actionNonce = 1;
                        renewedWitnessTxOutput.nValue=0;
                        for (uint32_t i=0; i<numGenesisWitnesses;++i)
                        {
                            txNew.vout.push_back(renewedWitnessTxOutput);
                        }
                    }

                    if (fIsOfficialTestnetV1)
                    {
                        genesis.nTime    = 1595347850;
                        genesis.nBits    = 524287999;
                        genesis.nNonce   = 1449001013;
                    }
                    else
                    {
                        genesis.nTime    = seedTimestamp;
                        genesis.nBits    = arith_uint256((~arith_uint256(0) >> 10)).GetCompact();
                        genesis.nNonce   = 0;
                    }
                    genesis.nVersion = 536870912;
                    genesis.vtx.push_back(MakeTransactionRef(std::move(txNew)));
                    genesis.hashPrevBlock.SetNull();
                    genesis.hashMerkleRoot = BlockMerkleRoot(genesis.vtx.begin(), genesis.vtx.end());
                    genesis.hashMerkleRootPoW2Witness = BlockMerkleRoot(genesis.vtx.begin(), genesis.vtx.end());
                    genesis.witnessHeaderPoW2Sig.resize(65);

                    if (!fIsOfficialTestnetV1)
                    {
                        uint256 foundBlockHash;
                        std::atomic<uint64_t> halfHashCounter=0;
                        std::atomic<uint64_t> nThreadCounter=0;
                        bool interrupt=false;
                        sigma_context generateContext(defaultSigmaSettings, defaultSigmaSettings.arenaSizeKb, std::max(GetNumCores(), 1), std::max(GetNumCores(), 1));
                        generateContext.prepareArenas(genesis);
                        generateContext.mineBlock(&genesis, halfHashCounter, foundBlockHash, interrupt);
                    }
                    genesis.nTimePoW2Witness = genesis.nTime+1;
                    genesis.nVersionPoW2Witness = genesis.nVersion;
                }
                ECC_Stop();
            }
            consensus.hashGenesisBlock = genesis.GetHashPoW2();
            LogPrintf("genesis nonce: %d\n",genesis.nNonce);
            LogPrintf("genesis time: %d\n",genesis.nTime);
            LogPrintf("genesis bits: %d\n",genesis.nBits);
            LogPrintf("genesis hash: %s\n", consensus.hashGenesisBlock.ToString().c_str());

            pchMessageStart[0] = targetInterval & 0xFF;
            pchMessageStart[1] = (seedTimestamp >> 8) & 0xFF;
            pchMessageStart[2] = (seedTimestamp >> 16) & 0xFF;
            pchMessageStart[3] = sTestnetParams[0];

            LogPrintf("pchMessageStart (aka magic bytes). decimal:[%d %d %d %d] hex:[%#x %#x %#x %#x]\n", pchMessageStart[0], pchMessageStart[1], pchMessageStart[2], pchMessageStart[3], pchMessageStart[0], pchMessageStart[1], pchMessageStart[2], pchMessageStart[3]);
        }

        vAlertPubKey = ParseHex("06087071e40ddf2ecbdf1ae40f536fa8f78e9383006c710dd3ecce957a3cb9292038d0840e3be5042a6b863f75dfbe1cae8755a0f7887ae459af689f66caacab52");
        nDefaultPort = 9235;
        nPruneAfterHeight = 1000;

        vFixedSeeds.clear();
        vSeeds.clear();
        //vSeeds.push_back(CDNSSeedData("seed 0", "testseed.gulden.blue"));

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,65);// 'T'
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,127);// 't'
        base58Prefixes[POW2_WITNESS_ADDRESS] = std::vector<unsigned char>(1,63);// 'S'
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,65+128);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x35, 0x87, 0xCF};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x35, 0x83, 0x94};

        vFixedSeeds = std::vector<SeedSpec6>(pnSeed6_test, pnSeed6_test + ARRAYLEN(pnSeed6_test));

        fDefaultConsistencyChecks = false;
        fRequireStandard = true;
        fMineBlocksOnDemand = false;

        checkpointData = {
            { 0, { genesis.GetHashPoW2(), genesis.nTime } }
        };

        if (fIsOfficialTestnetV1)
        {
            checkpointData = {
            {      0,  { uint256S("0xbcb2ed2904a0800f8284919f8b941a96517e2858c6850a252d121e5c4ebb9ce9"), 1595347850 } },
            {      10, { uint256S("0x7604417c69c0a20f87a5ff3d6c1c924e7ef47006b615e473d6709fed60934c30"), 1638997402 } },
            };
            if (!checkpointData.empty())
            {
                consensus.defaultAssumeValid = checkpointData.rbegin()->second.hash;
            }
            else
            {
                consensus.defaultAssumeValid = uint256S("");
            }
        }
    }
};

/**
 * Regression test
 */
class CRegTestLegacyParams : public CChainParams {
public:
    CRegTestLegacyParams() {
        fIsRegtestLegacy = true;
        strNetworkID = "regtestlegacy";
        consensus.BIP34Height = 100000000; // BIP34 has not activated on regtest (far in the future so block v1 are not rejected in tests)
        consensus.BIP65Height = 1351; // BIP65 activated on regtest (Used in rpc activation tests)
        consensus.BIP66Height = 1251; // BIP66 activated on regtest (Used in rpc activation tests)
        consensus.powLimit = uint256S("0x7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.nPowTargetTimespan = 14 * 24 * 60 * 60; // two weeks
        consensus.nPowTargetSpacing = 60;//1 minute
        consensus.fPowAllowMinDifficultyBlocks = true;
        consensus.fPowNoRetargeting = true;
        consensus.nRuleChangeActivationThreshold = 108; // 75% for testchains
        consensus.nMinerConfirmationWindow = 144; // Faster than normal for regtest (144 instead of 2016)
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = 999999999999ULL;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].type = Consensus::DEPLOYMENT_POW;
        
        //Never activate
        defaultSigmaSettings.activationDate = std::numeric_limits<uint64_t>::max();


        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].bit = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nStartTime = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nTimeout = 999999999999ULL;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].type = Consensus::DEPLOYMENT_POW;

        consensus.pow2Phase2FirstBlockHeight=40800;
        consensus.pow2Phase3FirstBlockHeight=50000;
        consensus.pow2Phase4FirstBlockHeight=50500;
        consensus.pow2Phase5FirstBlockHeight=50500;

        // The best chain should have at least this much work.
        consensus.nMinimumChainWork = uint256S("0x00");

        // By default assume that the signatures in ancestors of this block are valid.
        if (!checkpointData.empty())
        {
            consensus.defaultAssumeValid = checkpointData.rbegin()->second.hash;
        }
        else
        {
            consensus.defaultAssumeValid = uint256S("");
        }

        pchMessageStart[0] = 0xfc; // 'N' + 0xb0
        pchMessageStart[1] = 0xfe; // 'L' + 0xb0
        pchMessageStart[2] = 0xf7; // 'G' + 0xb0
        pchMessageStart[3] = 0xFF; // 0xFF
        nDefaultPort = 18444;
        nPruneAfterHeight = 1000;

        genesis = CreateGenesisBlock(1296688602, 2, UintToArith256(consensus.powLimit).GetCompact(), 1, 0);
        consensus.hashGenesisBlock = genesis.GetHashPoW2();
        assert(consensus.hashGenesisBlock == uint256S("0x3e4b830e0f75f7b72060ae5ebcc22fdf5df57c7e2350a2669ac4f8a2d734e1bc"));
        assert(genesis.hashMerkleRoot == uint256S("0x4bed0bcb3e6097445ae68d455137625bb66f0e7ba06d9db80290bf72e3d6dcf8"));

        vFixedSeeds.clear(); //!< Regtest mode doesn't have any fixed seeds.
        vSeeds.clear();      //!< Regtest mode doesn't have any DNS seeds.

        fDefaultConsistencyChecks = true;
        fRequireStandard = false;
        fMineBlocksOnDemand = true;

        checkpointData = {
            { 0, { genesis.GetHashPoW2(), genesis.nTime } }
        };

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,60);// 'R'
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,122);// 'r'
        base58Prefixes[POW2_WITNESS_ADDRESS] = std::vector<unsigned char>(1,123);// 'r'
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,60+128);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x35, 0x87, 0xCF};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x35, 0x83, 0x94};
    }
};

/**
 * Regression test
 */
class CRegTestParams : public CChainParams {
public:
    CRegTestParams() {
        fIsRegtest = true;
        strNetworkID = "regtest";
        consensus.BIP34Height = 0;
        consensus.BIP65Height = 1351; // BIP65 activated on regtest (Used in rpc activation tests)
        consensus.BIP66Height = 1251; // BIP66 activated on regtest (Used in rpc activation tests)
        consensus.powLimit = uint256S("0x7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.nPowTargetTimespan = 14 * 24 * 60 * 60; // two weeks
        consensus.nPowTargetSpacing = 60;//1 minute
        consensus.fPowAllowMinDifficultyBlocks = true;
        consensus.fPowNoRetargeting = true;
        consensus.nRuleChangeActivationThreshold = 108; // 75% for testchains
        consensus.nMinerConfirmationWindow = 144; // Faster than normal for regtest (144 instead of 2016)
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = 999999999999ULL;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].type = Consensus::DEPLOYMENT_POW;
        
        //Never activate
        defaultSigmaSettings.activationDate = std::numeric_limits<uint64_t>::max();


        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].bit = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nStartTime = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nTimeout = 999999999999ULL;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].type = Consensus::DEPLOYMENT_POW;

        consensus.pow2Phase2FirstBlockHeight=0;
        consensus.pow2Phase3FirstBlockHeight=0;
        consensus.pow2Phase4FirstBlockHeight=0;
        consensus.pow2Phase5FirstBlockHeight=0;
        numGenesisWitnesses = 10;
        genesisWitnessWeightDivisor = 100;


        // The best chain should have at least this much work.
        consensus.nMinimumChainWork = uint256S("0x00");

        // By default assume that the signatures in ancestors of this block are valid.
        if (!checkpointData.empty())
        {
            consensus.defaultAssumeValid = checkpointData.rbegin()->second.hash;
        }
        else
        {
            consensus.defaultAssumeValid = uint256S("");
        }

        pchMessageStart[0] = 0xfc; // 'N' + 0xb0
        pchMessageStart[1] = 0xfe; // 'L' + 0xb0
        pchMessageStart[2] = 0xf7; // 'G' + 0xb0
        pchMessageStart[3] = 0xFF; // 0xFF
        nDefaultPort = 18444;
        nPruneAfterHeight = 1000;

        GenerateGenesisBlock(genesis, "regtestregtestregtestregtest", genesisWitnessPrivKey, numGenesisWitnesses, 1296688602, UintToArith256(consensus.powLimit).GetCompact(), 0, true, false, consensus);

        consensus.hashGenesisBlock = genesis.GetHashPoW2();
        //assert(consensus.hashGenesisBlock == uint256S("0x3e4b830e0f75f7b72060ae5ebcc22fdf5df57c7e2350a2669ac4f8a2d734e1bc"));
        //assert(genesis.hashMerkleRoot == uint256S("0x4bed0bcb3e6097445ae68d455137625bb66f0e7ba06d9db80290bf72e3d6dcf8"));

        vFixedSeeds.clear(); //!< Regtest mode doesn't have any fixed seeds.
        vSeeds.clear();      //!< Regtest mode doesn't have any DNS seeds.

        fDefaultConsistencyChecks = false;
        //fixme: (MED) Turn this back on, its causing issues for witnessing on regtest mode
        //fDefaultConsistencyChecks = true;
        fRequireStandard = false;
        fMineBlocksOnDemand = true;

        checkpointData = {
            { 0, { genesis.GetHashPoW2(), genesis.nTime } }
        };

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,60);// 'R'
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,122);// 'r'
        base58Prefixes[POW2_WITNESS_ADDRESS] = std::vector<unsigned char>(1,123);// 'r'
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,60+128);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x35, 0x87, 0xCF};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x35, 0x83, 0x94};
    }
};

static std::unique_ptr<CChainParams> globalChainParams;

const CChainParams &Params() {
    assert(globalChainParams);
    return *globalChainParams;
}

std::unique_ptr<CChainParams> CreateChainParams(const std::string& chain)
{
    if (chain == CBaseChainParams::MAIN)
        return std::unique_ptr<CChainParams>(new CMainParams());
    else if (chain == CBaseChainParams::TESTNET)
        return std::unique_ptr<CChainParams>(new CTestNetParams());
    else if (chain == CBaseChainParams::REGTEST)
        return std::unique_ptr<CChainParams>(new CRegTestParams());
    else if (chain == CBaseChainParams::REGTESTLEGACY)
        return std::unique_ptr<CChainParams>(new CRegTestLegacyParams());
    throw std::runtime_error(strprintf("%s: Unknown chain %s.", __func__, chain));
}

void SelectParams(const std::string& network)
{
    SelectBaseParams(network);
    globalChainParams = CreateChainParams(network);
}

void FreeParams()
{
    globalChainParams = nullptr;
}

void UpdateVersionBitsParameters(Consensus::DeploymentPos d, int64_t nStartTime, int64_t nTimeout)
{
    globalChainParams->UpdateVersionBitsParameters(d, nStartTime, nTimeout);
}
