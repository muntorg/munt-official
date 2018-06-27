// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// File contains modifications by: The Gulden developers
// All modifications:
// Copyright (c) 2016-2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#include "chainparams.h"
#include "consensus/merkle.h"

#include "tinyformat.h"
#include "util.h"
#include "utilstrencodings.h"

#include <assert.h>

#include <cstdio>
#include "chainparamsseeds.h"

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
        consensus.nSubsidyHalvingInterval = 840000;
        consensus.BIP34Height = 227931;
        consensus.BIP34Hash = uint256S("0x000000000000024b89b42a942fe0d9fea3bb44ab7bd1b19115dd6a759c0808b8");
        consensus.BIP65Height = 388381; // 000000000000000004c2b624ed5d7756c508d90fd0da2c7c679febfa6c4735f0
        consensus.BIP66Height = 363725; // 00000000000000000379eaa19dce8c9b722d46ae6a57c2f1a988119488b50931
        consensus.powLimit =  uint256S("0x00000fffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.nPowTargetTimespan = 3.5 * 24 * 60 * 60; // Gulden: 3.5 days
        consensus.nPowTargetSpacing = 150; // Gulden: 2.5 minutes
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
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nStartTime = 1462060800; // May 1st, 2016
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nTimeout = 1493596800; // May 1st, 2017
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].type = Consensus::DEPLOYMENT_POW;

        // Deployment of PoW2 - phase 2
        consensus.vDeployments[Consensus::DEPLOYMENT_POW2_PHASE2].bit = 27;
        consensus.vDeployments[Consensus::DEPLOYMENT_POW2_PHASE2].nStartTime = 1531223700; // July 10th 2018 - 11::55 UTC
        consensus.vDeployments[Consensus::DEPLOYMENT_POW2_PHASE2].nTimeout = 1533902100; // August 10th 2018 - 11::55 UTC
        consensus.vDeployments[Consensus::DEPLOYMENT_POW2_PHASE2].type = Consensus::DEPLOYMENT_POW;

        // Deployment of PoW2 - phase 4
        consensus.vDeployments[Consensus::DEPLOYMENT_POW2_PHASE4].bit = 26;
        consensus.vDeployments[Consensus::DEPLOYMENT_POW2_PHASE4].nStartTime = 999999999999ULL; // From timestamp of first phase 3 block
        consensus.vDeployments[Consensus::DEPLOYMENT_POW2_PHASE4].nTimeout = 1608033300; // 15 December 2020
        consensus.vDeployments[Consensus::DEPLOYMENT_POW2_PHASE4].type = Consensus::DEPLOYMENT_POW;
        consensus.vDeployments[Consensus::DEPLOYMENT_POW2_PHASE4].protoVersion = 70015;
        consensus.vDeployments[Consensus::DEPLOYMENT_POW2_PHASE4].requiredProtoUpgradePercent = 75;

        // The best chain should have at least this much work.
        consensus.nMinimumChainWork = uint256S("00000000000000000000000000000000000000000000000084a08786233bd0d1");

        // By default assume that the signatures in ancestors of this block are valid.
        consensus.defaultAssumeValid = uint256S("0x56bce924eb7613b6fd4ac859a06a13f7643817d6a593d19951ab293182a021cb"); //505000

        /**
         * The message start string is designed to be unlikely to occur in normal data.
         * The characters are rarely used upper ASCII, not valid as UTF-8, and produce
         * a large 32-bit integer with any alignment.
         */
        pchMessageStart[0] = 0xfc; // 'N' + 0xb0
        pchMessageStart[1] = 0xfe; // 'L' + 0xb0
        pchMessageStart[2] = 0xf7; // 'G' + 0xb0
        pchMessageStart[3] = 0xe0; // 0xe0 (e for "echt", testnet has 0x02 as last byte)
        vAlertPubKey = ParseHex("073513ffe7147aba88d33aea4da129d8a2829c545526d5d854ab51d5778f4d0625431ba1c5a3245bdfe8736b127fdfdb488de72640727d37355c4c3a66c547efad");
        nDefaultPort = 9231;
        nPruneAfterHeight = 100000;

        genesis = CreateGenesisBlock(1009843200, 2200095, 0x1e0ffff0, 1, 0);
        consensus.hashGenesisBlock = genesis.GetHashLegacy();
        assert(consensus.hashGenesisBlock == uint256S("0x6c5d71a461b5bff6742bb62e5be53978b8dec5103ce52d1aaab8c6a251582f92"));
        assert(genesis.hashMerkleRoot == uint256S("0x4bed0bcb3e6097445ae68d455137625bb66f0e7ba06d9db80290bf72e3d6dcf8"));

        vSeeds.push_back(CDNSSeedData("seed 0",  "seed.gulden.com", false));
//        vSeeds.push_back(CDNSSeedData("seed 1",  "amsterdam.gulden.com"));
//        vSeeds.push_back(CDNSSeedData("seed 2",  "seed.gulden.network"));
        vSeeds.push_back(CDNSSeedData("seed 3",  "rotterdam.gulden.network", false));
//        vSeeds.push_back(CDNSSeedData("seed 4",  "seed.gulden.blue"));

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,38);// 'G'
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,98);// 'g'
        base58Prefixes[POW2_WITNESS_ADDRESS] = std::vector<unsigned char>(1,73);// 'W'
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,38+128);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x88, 0xB2, 0x1E};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x88, 0xAD, 0xE4};

        vFixedSeeds = std::vector<SeedSpec6>(pnSeed6_main, pnSeed6_main + ARRAYLEN(pnSeed6_main));

        fDefaultConsistencyChecks = false;
        fRequireStandard = true;
        fMineBlocksOnDemand = false;
        fUseSyncCheckpoints = true;

        checkpointData = (CCheckpointData){
            {
                {     0, uint256S("0x6c5d71a461b5bff6742bb62e5be53978b8dec5103ce52d1aaab8c6a251582f92")},
                {  1000, uint256S("0x77676cde325930f1a2f3bdabf34e54f06445e7dfd8b85a6aab372f60a222fa30")},
                {  2000, uint256S("0x9732e5f8b9fec4f62f83171eaa033cffa11714ba56dbb1dd60df681b358c9dd2")},
                { 10000, uint256S("0x25a619632ea07771156d61791245e7b3497ae987ef6be5348c41380291848974")},
                { 15000, uint256S("0x944e0468c38392c5f32818f8f50c10aa6deb5986d85a72e9aaddfe94acc74a5c")},
                { 19000, uint256S("0x2a9d91e8b61dc77b79ea43befeb72a1a8c89af3e8a40dbdba5b3a6b5f7510e91")},
                { 30300, uint256S("0x6340483a4bdd4e3a519a292ae4bc424dc12b8c72ef8f3cf3762347afc0a699c0")},
                { 35000, uint256S("0xe14bac6cfea31014bb057500160fb5a962e492ce16652b14fa07314fd9e523ff")},
                { 45000, uint256S("0x97b4cff99eda714dbff09881e339d1159e5558486e31198affd712ca806f0b1d")},
                { 86600, uint256S("0x9e3e0388b4712f2787cd443a7dbeeda12e90b98e909877cf814e7d5a60fc4b85")},
                { 100000, uint256S("0x5e831ed155d05f6ac7f17635022dbc348bf73942309ac403c6f8c2990e2e0af1")},
                { 125000, uint256S("0xee27d0f4b6596f302eb591072136ae196bb318d776c16625b23cc7383052b564")},
                { 150000, uint256S("0x97fdb21189d5a958d42fcb58f8d300e737a20fad91878dabdd925d11fc614013")},
                { 175000, uint256S("0xda6aa09113ddd62d871e9aacad6131831d5841a26968f1665a9b829fd30a29e3")},
                { 200000, uint256S("0x4e80313f4eb23093a63218f3736379084d1eeae46c4343668f3cdc9c0c5ca260")},
                { 225000, uint256S("0xc9a5c5226d8f103972ffee38c31c3508189b694e0d4f93a394ccea2cac82ce49")},
                { 250000, uint256S("0xa6635e1dbce15cfb4be7f3f464f612205dd13ba96828535000b99ce04648500d")},
                { 260000, uint256S("0x42c2254ffd8be411386b9089fec985fe3a06d5fc386ff0bd494b5a3aa292f107")},
                { 280350, uint256S("0xf95b3e7f97a41db38a872bdd15d985aae252c5ab497a51319e5bd50161a48d18")},
                { 300000, uint256S("0xf0f99e78c90d20ac4e376ffd1a7e8a89cd1bd152ad1a40f7be31bbf8e0b492c5")},
                { 325000, uint256S("0xa00bccd7a68495771f03856633786a16d3106b38adeafc4d610918b5b118ec9e")},
                { 350000, uint256S("0xa4c92744d47ada905f5cacc7ee91d86f0e646d52d5d8cafdab5d288490002196")},
                { 375000, uint256S("0xa1c3e45a3b4bbf823a35433f604d33c89749a3950a793becc90cddbecd03409c")},
                { 400000, uint256S("0xe4b072d2861b8041f42dcf2e8f5d1caaaff36bc952518e99f6d3fac89e1e1133")},
                { 425000, uint256S("0x0dd5aa1302943e7b4fff1963ce1c32a1aa3db15d652515a3ca8bf4aad84952d7")},
                { 450000, uint256S("0xfa4f46c846b053104ce5956578d72f9a5fa87c4ae49a6450e5d66c4fd37d6d66")},
                { 475000, uint256S("0xa730a89a11332ee0133c458c5feccbda857ace9572ab5a048701cccb0239cf4c")},
                { 500000, uint256S("0xd30e19c8b8c567b23c09fc022b4f5ca8014a8cb3c1504782e9a68af349757afa")},
                { 505000, uint256S("0x56bce924eb7613b6fd4ac859a06a13f7643817d6a593d19951ab293182a021cb")},
                { 525000, uint256S("0x1d3f4de346830365474b24f7b9dd7b332215dbe0e9e175c70df040446e2c4185")},
                { 550000, uint256S("0x3d7fc31f64905e2e6559298b826b914a3716ae160caea485ae46230890a4144d")},
                { 575000, uint256S("0x728d0d7edd0c1fcc0fa0f80bfbf8c06e2afb9a8ba0358fa434d3114a982003e6")},
                { 600000, uint256S("0xd6f49c4bf3b3a0e82392809b8925c485a518acdeacf8bdd9a9326e4c68456d6b")},
                { 625000, uint256S("0x0d067e2b98621356f67c0d969c8dbccd62a6bea757a006a6640e42becfe54740")},
                { 650000, uint256S("0x0fcd45353b44036efdabed53931b6a49db8f44fa91ede6be64189bccc5f2af53")},
                { 675000, uint256S("0xed520161b5954c1a800497fe334e320bc481f2b103f78e8319a0db6cbbd2dcb1")},
                { 700000, uint256S("0x7cdbb7bef28741aa682570703ca03cd77a6524011aed588fd0aabe5f0038f124")},
                { 750000, uint256S("0x22330a217c970fce0ac14f954793f0116df6931b1fd9f2c9e469884a71ef4d96")},

            }
        };

        chainTxData = ChainTxData{
            1527932637, // * UNIX timestamp of last checkpoint block
            1917021,    // * total number of transactions between genesis and last checkpoint
                        //   (the tx=... number in the SetBestChain debug.log lines)
            0.1         // * estimated number of transactions per second after that timestamp
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
        consensus.nSubsidyHalvingInterval = 210000;
        consensus.BIP34Height = 21111;
        consensus.BIP34Hash = uint256S("0x0000000023b3a96d3484e5abb3755c413e7d41500f8e2a5c3f0dd01299cd8ef8");
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

            // Deployment of PoW2 - phase 2
            consensus.vDeployments[Consensus::DEPLOYMENT_POW2_PHASE2].bit = 27;
            consensus.vDeployments[Consensus::DEPLOYMENT_POW2_PHASE2].nStartTime = seedTimestamp; 
            consensus.vDeployments[Consensus::DEPLOYMENT_POW2_PHASE2].nTimeout = seedTimestamp + (1 * 30 * 24 * 60 * 60); //1 month.
            consensus.vDeployments[Consensus::DEPLOYMENT_POW2_PHASE2].type = Consensus::DEPLOYMENT_POW;

            // Deployment of PoW2 - phase 4
            consensus.vDeployments[Consensus::DEPLOYMENT_POW2_PHASE4].bit = 26;
            consensus.vDeployments[Consensus::DEPLOYMENT_POW2_PHASE4].nStartTime = 999999999999ULL; // From timestamp of first phase 3 block
            consensus.vDeployments[Consensus::DEPLOYMENT_POW2_PHASE4].nTimeout = seedTimestamp + (2 * 30 * 24 * 60 * 60); //2 month.
            consensus.vDeployments[Consensus::DEPLOYMENT_POW2_PHASE4].type = Consensus::DEPLOYMENT_POW;
            consensus.vDeployments[Consensus::DEPLOYMENT_POW2_PHASE4].protoVersion = 70015;
            consensus.vDeployments[Consensus::DEPLOYMENT_POW2_PHASE4].requiredProtoUpgradePercent = 95; 

            // The best chain should have at least this much work.
            consensus.nMinimumChainWork = uint256S("");

            // By default assume that the signatures in ancestors of this block are valid.
            consensus.defaultAssumeValid = uint256S("");

            genesis = CreateGenesisBlock(seedTimestamp, 0, UintToArith256(consensus.powLimit).GetCompact(), 1, 0);
            genesis.nBits = arith_uint256((~arith_uint256(0) >> 10)).GetCompact();

            while(UintToArith256(genesis.GetPoWHash()) > UintToArith256(consensus.powLimit))
            {
                genesis.nNonce++;
                if(genesis.nNonce == 0)
                    genesis.nTime++;
            }
            consensus.hashGenesisBlock = genesis.GetHashLegacy();
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
        nDefaultPort = 9923;
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


        checkpointData = (CCheckpointData){
            {
                { 0, genesis.GetHashPoW2() }
            }
        };

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
        consensus.nSubsidyHalvingInterval = 150;
        consensus.BIP34Height = 100000000; // BIP34 has not activated on regtest (far in the future so block v1 are not rejected in tests)
        consensus.BIP34Hash = uint256();
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


        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].bit = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nStartTime = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nTimeout = 999999999999ULL;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].type = Consensus::DEPLOYMENT_POW;

        // Deployment of PoW2 - phase 2
        consensus.vDeployments[Consensus::DEPLOYMENT_POW2_PHASE2].bit = 27;
        consensus.vDeployments[Consensus::DEPLOYMENT_POW2_PHASE2].nStartTime = 999999999999ULL; // July 1st 2017
        consensus.vDeployments[Consensus::DEPLOYMENT_POW2_PHASE2].nTimeout = 1504051200; // August 30th 2017
        consensus.vDeployments[Consensus::DEPLOYMENT_POW2_PHASE2].type = Consensus::DEPLOYMENT_POW;

        // Deployment of PoW2 - phase 4
        consensus.vDeployments[Consensus::DEPLOYMENT_POW2_PHASE4].bit = 26;
        consensus.vDeployments[Consensus::DEPLOYMENT_POW2_PHASE4].nStartTime = 999999999999ULL; // From timestamp of first phase 3 block
        consensus.vDeployments[Consensus::DEPLOYMENT_POW2_PHASE4].nTimeout = 1504051200; // August 30th 2017
        consensus.vDeployments[Consensus::DEPLOYMENT_POW2_PHASE4].type =Consensus::DEPLOYMENT_POW;
        consensus.vDeployments[Consensus::DEPLOYMENT_POW2_PHASE4].protoVersion = 70015;
        consensus.vDeployments[Consensus::DEPLOYMENT_POW2_PHASE4].requiredProtoUpgradePercent = 95; 


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
        consensus.hashGenesisBlock = genesis.GetHashLegacy();
        assert(consensus.hashGenesisBlock == uint256S("0x3e4b830e0f75f7b72060ae5ebcc22fdf5df57c7e2350a2669ac4f8a2d734e1bc"));
        assert(genesis.hashMerkleRoot == uint256S("0x4bed0bcb3e6097445ae68d455137625bb66f0e7ba06d9db80290bf72e3d6dcf8"));

        vFixedSeeds.clear(); //!< Regtest mode doesn't have any fixed seeds.
        vSeeds.clear();      //!< Regtest mode doesn't have any DNS seeds.

        fDefaultConsistencyChecks = true;
        fRequireStandard = false;
        fMineBlocksOnDemand = true;
        fUseSyncCheckpoints = false;

        checkpointData = (CCheckpointData) {
            {
                {0, genesis.GetHashLegacy()},
            }
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

void UpdateVersionBitsParameters(Consensus::DeploymentPos d, int64_t nStartTime, int64_t nTimeout)
{
    globalChainParams->UpdateVersionBitsParameters(d, nStartTime, nTimeout);
}
