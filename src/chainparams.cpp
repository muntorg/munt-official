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

CChainParams::CChainParams(): fIsOfficialTestnetV1(false) {}

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

        {
            numGenesisWitnesses = 400;
            genesisWitnessWeightDivisor = 2000;
            
            // Don't bother creating the genesis block if we haven't started ECC yet (e.g. we are being called from the help text)
            // We can't initialise key anyway unless the app has first initialised ECC, and the help doesn't need the genesis block, creating it twice is a waste of cpu cycles
            {
                CMutableTransaction txNew(CTransaction::CURRENT_VERSION);
                txNew.vin.resize(1);
                txNew.vin[0].prevout.SetNull();
                txNew.vin[0].segregatedSignatureData.stack.clear();
                txNew.vin[0].segregatedSignatureData.stack.push_back(std::vector<unsigned char>());
                CVectorWriter(0, 0, txNew.vin[0].segregatedSignatureData.stack[0], 0) << VARINT(0);
                txNew.vin[0].segregatedSignatureData.stack.push_back({'F','M'});
                
                {
                    CKeyID pubKeyIDWitness;
                    pubKeyIDWitness.SetHex("b3a447d0cbdd174893d1e52df169c703583e8339");

                    CTxOut renewedWitnessTxOutput;
                    renewedWitnessTxOutput.SetType(CTxOutType::PoW2WitnessOutput);
                    renewedWitnessTxOutput.output.witnessDetails.spendingKeyID = pubKeyIDWitness;
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

                genesis.nTime    = 1592990387;
                genesis.nBits    = arith_uint256((~arith_uint256(0) >> 10)).GetCompact();
                genesis.nNonce   = 465305602;
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
        assert(consensus.hashGenesisBlock == uint256S("0x4d3e8d58df656808efbdd6eafe18ac5ad0678303be1cfdac0a0e0b8fdc6da9e3"));
        assert(genesis.hashMerkleRoot == uint256S("0x3cfac9091626d7f3bf25fdb9829e8df001a59a45f0d2426c0cf76e2a7a3d0c46"));
        assert(genesis.hashMerkleRootPoW2Witness == uint256S("0x3cfac9091626d7f3bf25fdb9829e8df001a59a45f0d2426c0cf76e2a7a3d0c46"));

        vSeeds.push_back(CDNSSeedData("seed 0",  "seed1.novocurrency.com", false));
        vSeeds.push_back(CDNSSeedData("seed 1",  "seed2.novocurrency.com", false));       

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
        fUseSyncCheckpoints = true;

        checkpointData = {
            { 0, { genesis.GetHashPoW2(), genesis.nTime } }
        };

        // By default assume that the signatures in ancestors of this block are valid.
        consensus.defaultAssumeValid = uint256S("");
        
        // The best chain should have at least this much work.
        consensus.nMinimumChainWork = uint256S("");
        
        chainTxData = ChainTxData{
            0,
            0,
            0
        };
    }
};

/**
 * Testnet
 */
class CTestNetParams : public CChainParams {
public:
    CTestNetParams() {
        strNetworkID = "test";
        consensus.BIP34Height = 21111;
        consensus.BIP65Height = 581885; // 00000000007f6655f22f98e72ed80d8b06dc761d5da09df0fa1dc4be4f861eb6
        consensus.BIP66Height = 330776; // 000000002104c8c45e99a8853285a3b592602a3ccde2b832481da85e9e4ba182
        consensus.powLimit =  uint256S("0x003fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.nPowTargetTimespan = 14 * 24 * 60 * 60; // two weeks

        std::string sTestnetParams = GetArg("-testnet", "");
        if (!sTestnetParams.empty())
        {
            assert(sTestnetParams.find(":")!=std::string::npos);
            assert(sTestnetParams[0] == 'S' || sTestnetParams[0] == 'C');

            int targetInterval = atoi(sTestnetParams.substr(sTestnetParams.find(":")+1));
            int64_t seedTimestamp = atoi64(sTestnetParams.substr(1,sTestnetParams.find(":")));

            if (sTestnetParams == "C1534687770:60")
            {
                fIsOfficialTestnetV1 = true;
                defaultSigmaSettings.activationDate = 1570032000;
            }
            else
            {
                defaultSigmaSettings.activationDate = seedTimestamp+300;
            }

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
            consensus.defaultAssumeValid = uint256S("");

            if (fIsOfficialTestnetV1)
            {
                consensus.pow2Phase2FirstBlockHeight=21;
                consensus.pow2Phase3FirstBlockHeight=51;
                consensus.pow2Phase4FirstBlockHeight=528762;
                consensus.pow2Phase5FirstBlockHeight=528762;

                genesis = CreateGenesisBlock(seedTimestamp, 0, UintToArith256(consensus.powLimit).GetCompact(), 1, 0);
                genesis.nBits = arith_uint256((~arith_uint256(0) >> 10)).GetCompact();
                genesis.nNonce = 928;
                genesis.nTime = 1534687770;
            }
            else
            {
                consensus.pow2Phase2FirstBlockHeight=0;
                consensus.pow2Phase3FirstBlockHeight=0;
                consensus.pow2Phase4FirstBlockHeight=0;
                consensus.pow2Phase5FirstBlockHeight=0;

                numGenesisWitnesses = 10;
                genesisWitnessWeightDivisor = 100;
                
                // Don't bother creating the genesis block if we haven't started ECC yet (e.g. we are being called from the help text)
                // We can't initialise key anyway unless the app has first initialised ECC, and the help doesn't need the genesis block, creating it twice is a waste of cpu cycles
                ECC_Start();
                {
                    CMutableTransaction txNew(CTransaction::CURRENT_VERSION);
                    txNew.vin.resize(1);
                    txNew.vin[0].prevout.SetNull();
                    txNew.vin[0].segregatedSignatureData.stack.clear();
                    txNew.vin[0].segregatedSignatureData.stack.push_back(std::vector<unsigned char>());
                    CVectorWriter(0, 0, txNew.vin[0].segregatedSignatureData.stack[0], 0) << VARINT(0);
                    txNew.vin[0].segregatedSignatureData.stack.push_back(ParseHex("4f6e206a616e756172692031737420746865204475746368206c6f73742074686572652062656c6f7665642047756c64656e"));
                    
                    {
                        std::string sKey = std::string(sTestnetParams, 1, 8);
                        sKey += sKey;
                        sKey += sKey;
                        genesisWitnessPrivKey.Set((unsigned char*)&sTestnetParams[0],(unsigned char*)&sTestnetParams[0]+32, true);
                        
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

                    genesis.nTime    = seedTimestamp;
                    genesis.nBits    = arith_uint256((~arith_uint256(0) >> 10)).GetCompact();
                    genesis.nNonce   = 0;
                    genesis.nVersion = 536870912;
                    genesis.vtx.push_back(MakeTransactionRef(std::move(txNew)));
                    genesis.hashPrevBlock.SetNull();
                    genesis.hashMerkleRoot = BlockMerkleRoot(genesis.vtx.begin(), genesis.vtx.end());
                    genesis.hashMerkleRootPoW2Witness = BlockMerkleRoot(genesis.vtx.begin(), genesis.vtx.end());
                    genesis.witnessHeaderPoW2Sig.resize(65);

                    uint256 foundBlockHash;
                    std::atomic<uint64_t> halfHashCounter=0;
                    std::atomic<uint64_t> nThreadCounter=0;
                    bool interrupt=false;
                    sigma_context generateContext(defaultSigmaSettings, defaultSigmaSettings.arenaSizeKb, std::max(GetNumCores(), 1));
                    generateContext.prepareArenas(genesis);
                    generateContext.mineBlock(&genesis, halfHashCounter, foundBlockHash, interrupt);
                    
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
        fUseSyncCheckpoints = false;


        checkpointData = {
            { 0, { genesis.GetHashPoW2(), genesis.nTime } }
        };

        if (fIsOfficialTestnetV1)
        {
            checkpointData = {
            {      0, { uint256S("0x52b6c4e959a5e18c9fbc949561ed59481f50b012f9b5fe25cd58de65e07e2a87"), 1534687770 } },
            {   5000, { uint256S("0x5645cb5dbbbeed00c799e303d6c4e2c901f6ba7f9ba8bdba93aea2a91ee569d6"), 1535010157 } },
            {  10000, { uint256S("0x1f1a9f22fa9da34844a18cfbc72868e72efa072ba56160c93dbacdf4ed1163dd"), 1536108393 } },
            {  15000, { uint256S("0x2cb89a55489f4e0ebc02f04e166bb39a35bf07ee5b0f49864dc3b38d2ca92c3c"), 1537808886 } },
            {  20000, { uint256S("0xa0051ef2c5283c6770d149f49e8cad71dc302ba74a3b583f8f0c751827436b22"), 1538128272 } },
            {  25000, { uint256S("0x1c65ccca24be2222d12c566aaaf4f7324046f0ed988d80ddce33186a0d0b33bc"), 1538445993 } },
            {  30000, { uint256S("0xc9b656cbcac084fca88ce4edd0ec9847e6cebf17ec3f8e674993465c31055cf7"), 1538763420 } },
            {  35000, { uint256S("0x99b4150d987ccd14be80e8dcd8262b45a2dc1d3370b929c1efc5ce993dbf2f91"), 1539079981 } },
            {  40000, { uint256S("0x2c1942d7fa8e713b9e3d46c42dd3c6b88640b367bd36fc3a230c9901c69ba83a"), 1539397292 } },
            {  45000, { uint256S("0xc3356fbba262b2560018c8e41a0ad2b56f291cca22eeebbce3534f13ffebcce4"), 1539710360 } },
            {  50000, { uint256S("0x8ebe5d2aa1a3d5bec694c7f68a127d86fbe9c3acdc8d43b41c30b64951f8e8ed"), 1540030199 } },
            {  55000, { uint256S("0xa92c9216e521886b7bb21122216d4e764b0d0e8f7e178aa0179ac65f2508fbce"), 1540349467 } },
            {  60000, { uint256S("0xc5f3f9283a88ddcd8c76e8f7ad913eb7ed026bc46fe1ecddb6be2703d3de6fcd"), 1540670080 } },
            {  65000, { uint256S("0x61c2b543790968d99c0abc271eda404ea07b9e2b661e8e33f0839e87afd1a23d"), 1540989319 } },
            {  70000, { uint256S("0xdc6fcba1e93c425ed79d636b2a70c697115153d23311c4876fdc9ac41be031db"), 1541307040 } },
            {  75000, { uint256S("0xef1be658833a6ed267840598dcd3754a727b0369e38c4ebc5f4f7683fa9c5a8e"), 1541625273 } },
            {  80000, { uint256S("0xf6d65ed2a52367bf0dbff586f23062a67d73a5fd97727edf618f58e68638e45f"), 1541942706 } },
            {  85000, { uint256S("0x1350e7ab72f649f505a93dc728ba0927750af3ed3fb0947045b9944d052c115f"), 1542261051 } },
            {  90000, { uint256S("0xb95cbebc3493c289a304a57b39f3842e98173d42b2c52e250dba22e4acb36e40"), 1542581413 } },
            {  95000, { uint256S("0x7d328a542759c2c8dedcde2dad19138979ca155c07625b6b7001d48cbf539ad5"), 1543698585 } },
            { 100000, { uint256S("0x659c77ab1e3434617ca186c61d575ddb3748817cdf6b1a40d59181aeab6ba026"), 1545381741 } },
            { 105000, { uint256S("0x2f4d53ced87100a5d050a1a08964a313fd2cbd307b59332b2c5ac59de41da488"), 1547153405 } },
            { 110000, { uint256S("0x0df2b54c0938cb9107ee5b808f05f53c0be9eb6390a63c09b2bcf04c5d1cfb6b"), 1547888099 } },
            { 115000, { uint256S("0xa6eb6aab6f670aa60dfbf681a93e8da586d31ab25341555315eea79f93c7cbdf"), 1548229204 } },
            { 120000, { uint256S("0x86896682580a544aac0987c8a30037bed1bfb81d1b9370e9a0fb32fce0645293"), 1548550481 } },
            { 125000, { uint256S("0xc65b792dd1c918fd05596eccc93795ecd429fb30bcabe80a0e081ded13356059"), 1548870842 } },
            { 130000, { uint256S("0xe6526b88456142631c17ad01346363345cd7fe150273a5de7cb57d3a53f952c9"), 1549190172 } },
            { 135000, { uint256S("0xdc86f6fb66c27740c83fd5a87101f3fd3ced6ce4297f709eb5be60de0d8f04bf"), 1549509647 } },
            { 140000, { uint256S("0x113cf18699d3292a4b0993ce752f398dfb9475e46ceff54d2fa73ddcf2c6ccda"), 1549844797 } },
            { 145000, { uint256S("0xcb9b469c1c38601c2f565a2435afbd94081b1d45dfc1820da6f3d1e4809c5897"), 1550162868 } },
            { 150000, { uint256S("0xd640d2adce20d43a40e54cdb4ebae8bb681fa112dcf76c89514fdefc9aa34bd0"), 1550482186 } },
            { 155000, { uint256S("0xb341a4f33b3676857cfea50b62ef757f675dc6261b1ff4a976651a93dd521bae"), 1550803917 } },
            { 160000, { uint256S("0x116470792af525b47b4273fe7d593ce997d8fc1494e42e7d67c263f763258cae"), 1551122931 } },
            { 165000, { uint256S("0x12955f55f3b8fbd4f39c3444098ec6d1a9584b51217bbe35e595d54d81dcfeda"), 1551442048 } },
            { 170000, { uint256S("0xcb374f60e2da4a797d9db14617e67ffb765f18e0bbb8980d715b055e1debe2d6"), 1551760272 } },
            { 175000, { uint256S("0xe3b8377d3cea2cec0dee485fb9069b114a095c7d8ad212ff5fa43c9992e82ef4"), 1552077583 } },
            { 180000, { uint256S("0x6e74ba2f8895a87ba7f7a1d4edbbf6b99182d80a7cf3b9081174128c3d6ae913"), 1552396405 } },
            { 185000, { uint256S("0x6bd91f7b2473b2c660e89c7fb078fcd861295813267851dccd0228585476fa0f"), 1552714091 } },
            { 190000, { uint256S("0x434fa4bd1511b775239b6a6d31e86c4f42ef4356c4cb9ee6b8208c38d416a49a"), 1553032006 } },
            { 195000, { uint256S("0xb27ba8314fc5cc23c1ed9939c14b8d650b814767a8be651a8a9ceed8ff68abd0"), 1553350349 } },
            { 200000, { uint256S("0x8814374e1e5f2b802c10cb75e3670e8d07441bbd59fda5d22df4ac9f781752db"), 1553682205 } },
            { 205000, { uint256S("0x2f5270824d6bbe31af77b48d7dc322c54ebab40b119abd096a0481abb4bf9000"), 1554001000 } },
            { 210000, { uint256S("0x40fb5ad3d69e77b69fece6a60a28fe667a8e4dec1a37891e47373e9b17e4a06f"), 1554314065 } },
            { 215000, { uint256S("0x1077c47fa07e969273b242e6b3cfa4e2cac4eb8cda5af5ff86eaa763d86d3784"), 1554623067 } },
            { 220000, { uint256S("0x6beb4a1495bea7a9f531595a1bc6a824151d5f75b55d755d51197f0ddbade472"), 1554932858 } },
            { 225000, { uint256S("0xa0315944181689e1b1b6c2c6835915b83c880386130e0ef5be6d80e1e7d23ac7"), 1555253165 } },
            { 230000, { uint256S("0x896fc3850540bf20fbed8308237c7b2862f95d64bae8c28d5e70836f675383b1"), 1555572596 } },
            { 235000, { uint256S("0xb8f59feb778fefe2c46095f03cc986913abacda7f45c96db9abc42dd7a1295a9"), 1555890777 } },
            { 240000, { uint256S("0x86e24c45f7b2931319f9de33394c5e37153a24825b5ea3583c301c5bf32f223e"), 1556207255 } },
            { 245000, { uint256S("0x0196498daec7385b82c283ed54bb26af09a82772a27c4a2c239ecfb147c41e03"), 1556518460 } },
            { 250000, { uint256S("0x5a0b1ac37c5e4f5e0534357d004df3bb5d1140da60c145fd8bc149b3d2710ecd"), 1556834442 } },
            { 255000, { uint256S("0x85c55474d6ffe32a94ed5dea52f360b4e776274352643ae2229bfa57383fc9f2"), 1557150137 } },
            { 260000, { uint256S("0x39bea0ae604813398dadf406fecd8b405aeda61ffc93ecb9b6fa5c2e1721a7d8"), 1557466875 } },
            { 265000, { uint256S("0xfc168cf4a3aa40302c6cab1fd27f93a3f0ab8b1e36f92ff6329084c287fdd0de"), 1557780808 } },
            { 270000, { uint256S("0xb5f69145c1d271cf7e1576984d41061ab1e3db93d6a284ecbb76856c50d0728d"), 1558096233 } },
            { 275000, { uint256S("0xb32c8f39d2e3257f912b2a2af4ebbbcdd806d8e417b4c999ea711c528e36cfdf"), 1558412157 } },
            { 280000, { uint256S("0xd805dab30f0c305e00938b7254dbb3a1d5bacd11919f69ce26ef8f948c7324ca"), 1558727676 } },
            { 285000, { uint256S("0xeb1af295ffd6f00a79b620da2e4a28bfa5ed22b0e40484688101842f08fa1e17"), 1560381936 } },
            { 290000, { uint256S("0x269aa74631ca3c7c07c2da598907aac926392d55a9d648537f2681c9d376a35f"), 1560702992 } },
            { 295000, { uint256S("0x3b70b6e10de508ff0014476415e23e105b30adaf980f70a1ca2a11bdd761b0c2"), 1561022834 } },
            { 300000, { uint256S("0xbaa0d3762a8b7eb0059e77196f71704b0614bc6652d040a1e3c7e9cf1364cb84"), 1561342034 } },
            { 305000, { uint256S("0xe8fcdf9ece8fc930de3bb1bd0d32c459511fb84c64164a1abfd8fd783ca453e8"), 1561663114 } },
            { 310000, { uint256S("0xd5786f475a7493ddc5aa1340a77ee5580018902848f2009764b20e093ec4291d"), 1561991193 } },
            { 315000, { uint256S("0xf5ea7cf54c682f7d2d5cbb6137270a914b39f1d92247b7da2b942de2b72bb95f"), 1562567363 } },
            { 320000, { uint256S("0xb93f4994f3b2e3ad3bea5c7bb1e14be25c46f80adca4918df9c5e128f0b28724"), 1563537469 } },
            { 325000, { uint256S("0xe49f33e1fa2df1e21b5d7b0d65a88a6222280b0fdf02411b64fd357050028ef5"), 1563858985 } },
            { 330000, { uint256S("0x98abce8d58f89ccde016cb733165339126c96c3146892c20773a63ec197cd20e"), 1564179171 } },
            { 335000, { uint256S("0x1808465e3caa98b99519c49a4a806995c32f862207bbc1d03b37911dbbc44649"), 1564501068 } },
            { 340000, { uint256S("0x335d480f03eab72d885e08392ba465ca850260dc354364c5d3ed987968674ae8"), 1564820788 } },
            { 345000, { uint256S("0x7c3c824842ee3a984a832f9a7b38b2cbdcf545f6c35a2e9d1ba63327d755e8e0"), 1565142367 } },
            { 350000, { uint256S("0x4efbd8ccfd3ee233416491f724e763621836079ebbcb6cbb9578a34e78b814f2"), 1565462921 } },
            { 355000, { uint256S("0x0f32cc3de754cc073d49130fed0391e4db9bbc0b11920a00f42d8c0bc90ac439"), 1565782781 } },
            { 360000, { uint256S("0x2b63ac00062b68a87f669e2486297e5a4e49c86d9b6f5158a2edf2065c4f94bf"), 1566102666 } },
            { 365000, { uint256S("0x3ac0ce33f4e4940408586787c6cda6013fdcd454d7c5c29557f62dedfa71d645"), 1566424897 } },
            { 370000, { uint256S("0x3165432192cc1db1ab57e9eb8551a05d9171f5cd985049c5e826b7f5999235f1"), 1566745486 } },
            { 375000, { uint256S("0x1d79ed6fbb36db34ecebcf34501aced0fc5f619824ea14249c03545b379b531d"), 1567066520 } },
            { 380000, { uint256S("0xce497faeb41ddde322a2dfacdae430ca0271f42c2fd9c653b12ce2b33f43a2fc"), 1567386770 } },
            { 385000, { uint256S("0xd1c4251311b95d3d37f617bc6c097bfb7cd565f321192569175011143dc6dd69"), 1567707724 } },
            { 390000, { uint256S("0x69d6540523bdf059aeb7ca8299a4afa0ea9ba7cfc69a4d2dace902b57676c946"), 1568030288 } },
            { 395000, { uint256S("0x9cd43399097d7fad2b4596e3e921c342b43fdd3203b66c53bec808b4dca20f95"), 1568351003 } },
            { 400000, { uint256S("0x7f4b627481fe3deb93014d0b8b950416b98124ac1d2c55fef752881c6dc3e1d0"), 1568671529 } },
            { 405000, { uint256S("0x71a7d1e8196ad368a56822a44c49e2e67d283864dae8fecedc7f0def17476e5d"), 1568993383 } },
            { 410000, { uint256S("0x3a46851a8adce01bc3b6e84bb2b32eb6b7e57b4afa165992420fa0b64ae98830"), 1569313959 } },
            { 415000, { uint256S("0xc4146aa89e2baa264bc46f86fb63fba3f6e08c46aa627d4848531c411eb25d7a"), 1570205159 } },
            { 420000, { uint256S("0xa2da448100b52df36c903c01eb3e8e5b70d1ca88560b027259e21c1ed821ca40"), 1570522428 } },
            { 425000, { uint256S("0xad541d906eb47e1bcd2ccb23524bc4e467304e46fec5f3f85a22f4dc0c7a435c"), 1570839391 } },
            { 430000, { uint256S("0xdecd9ceb0acd2b0140ecff44b1259f07ed4b64ef2007328919c8cb6ef66b1c47"), 1571147668 } },
            { 435000, { uint256S("0x450ef851520f9963b7060ddf92905fe6643f2285e17437330c40cafc73deec64"), 1571963150 } },
            { 440000, { uint256S("0xab478b1b8011519c5637f498521eb85a51a33f1d7c69ff2d7795b83410e5d948"), 1572879470 } },
            { 445000, { uint256S("0xf7375f1df339a9f11810c2515e62e7d55bdc471c91d5908943d7c62c12112405"), 1573696114 } },
            { 450000, { uint256S("0xfa8309bc062ecf03e619d0de0d9069a759678615bd33888071a5a2724931e4b6"), 1574053479 } },
            { 455000, { uint256S("0x0500e3150191413550d89b66fff60c52489904d23d662b921264da33355968b1"), 1574411183 } },
            { 460000, { uint256S("0x129a6b2e7db2f8d847770e33b389302220a7f100755dbb8cdddea898eb547eed"), 1574764360 } },
            { 465000, { uint256S("0xfa3f72aa11fd156866f41a96692a5483a312ea0ce703982bd4a44e260d3dcd0b"), 1575115675 } },
            { 470000, { uint256S("0xcd54a59557acf7d14e2082ce06b3b8ca8379a5b2974cd86705adc9bc3af12f87"), 1575667108 } },
            { 475000, { uint256S("0x93506ac5bbe965d85687c397b5e19ba7cbbe68df522a2d4f464bc3aef217efc8"), 1577121315 } },
            { 480000, { uint256S("0xbd5b22c463a2534fedf1f11906e610b7c193a7fa17940b2626f75750b35e1f53"), 1579348054 } },
            { 485000, { uint256S("0x3e4ba76778f2ddcb1ed8f48bab60fffd7c562156442b7009cf9e092e9c872dcc"), 1579788994 } },
            { 490000, { uint256S("0xb5f21ca51c1b9487161f33b115d8766b7c38c893f8faa868524cf4590d673786"), 1580494850 } },
            { 495000, { uint256S("0x569f6c4b35a8c3add22d104223d2db81aac2a6ebaa893860b4861323b5bb68be"), 1580870448 } },
            { 500000, { uint256S("0x06c05daf6e7b329490e17d3bef72aa74b86567b6a42425df23a39b58a4a43cc8"), 1581265467 } },
            { 505000, { uint256S("0x2aa1559b266d2d7c5d51dc679482bd75403948280be2ca4afa8acb1af7524ab8"), 1581636798 } },
            { 510000, { uint256S("0xf0acfcde258da6e5d3c435e950f29ce70c0826d305ade9dd78dae8604d566844"), 1582498504 } },
            { 515000, { uint256S("0x40602dda1a37cef49b42145777ce99f89efeff9436658784d33c26d7bec6d45d"), 1582971617 } },
            { 520000, { uint256S("0x873f56621ac71d1bc747c28022e306bd738db0e48295206a4cfa4a2572cacab6"), 1583422438 } },
            { 525000, { uint256S("0x1215c8c8e196c922a021dbc6ca413fec0fa08b5280c179bab4ff18034a643a27"), 1583863369 } },
            { 526274, { uint256S("0x36bb6227d4535ad251ef019f4780d85a0ea732cc437f2c751ae50cfab132a8f3"), 1583973659 } },
            { 527426, { uint256S("0x8e2b07256b389b923030e5820aac3c762cddfdac272836be7a9a8150e6f73ee5"), 1584661592 } },
            };
            consensus.defaultAssumeValid = uint256S("0x8e2b07256b389b923030e5820aac3c762cddfdac272836be7a9a8150e6f73ee5");
        }
        

        chainTxData = ChainTxData{
            0,
            0,
            0
        };

    }
};

/**
 * Regression test
 */
class CRegTestParams : public CChainParams {
public:
    CRegTestParams() {
        strNetworkID = "regtest";
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
        consensus.defaultAssumeValid = uint256S("0x00");

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
        fUseSyncCheckpoints = false;

        checkpointData = {
            { 0, { genesis.GetHashPoW2(), genesis.nTime } }
        };

        chainTxData = ChainTxData{
            0,
            0,
            0
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
