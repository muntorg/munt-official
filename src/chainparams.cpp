// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "chainparams.h"

#include "random.h"
#include "util.h"
#include "utilstrencodings.h"

#include <assert.h>

#include <boost/assign/list_of.hpp>

#include <cstdio>

using namespace std;

struct SeedSpec6 {
    uint8_t addr[16];
    uint16_t port;
};

#include "chainparamsseeds.h"

/**
 * Main network
 */

//! Convert the pnSeeds6 array into usable address objects.
static void convertSeed6(std::vector<CAddress> &vSeedsOut, const SeedSpec6 *data, unsigned int count)
{
    // It'll only connect to one or two seed nodes because once it connects,
    // it'll get a pile of addresses with newer timestamps.
    // Seed nodes are given a random 'last seen time' of between one and two
    // weeks ago.
    const int64_t nOneWeek = 7*24*60*60;
    for (unsigned int i = 0; i < count; i++)
    {
        struct in6_addr ip;
        memcpy(&ip, data[i].addr, sizeof(ip));
        CAddress addr(CService(ip, data[i].port));
        addr.nTime = GetTime() - GetRand(nOneWeek) - nOneWeek;
        vSeedsOut.push_back(addr);
    }
}

/**
 * What makes a good checkpoint block?
 * + Is surrounded by blocks with reasonable timestamps
 *   (no blocks before with a timestamp after, none after with
 *    timestamp before)
 * + Contains no strange transactions
 */
static Checkpoints::MapCheckpoints mapCheckpoints =
        boost::assign::map_list_of
        (     0, uint256S("0x6c5d71a461b5bff6742bb62e5be53978b8dec5103ce52d1aaab8c6a251582f92"))
        (  1000, uint256S("0x77676cde325930f1a2f3bdabf34e54f06445e7dfd8b85a6aab372f60a222fa30"))
        (  2000, uint256S("0x9732e5f8b9fec4f62f83171eaa033cffa11714ba56dbb1dd60df681b358c9dd2"))
        ( 10000, uint256S("0x25a619632ea07771156d61791245e7b3497ae987ef6be5348c41380291848974"))
        ( 15000, uint256S("0x944e0468c38392c5f32818f8f50c10aa6deb5986d85a72e9aaddfe94acc74a5c"))
        ( 19000, uint256S("0x2a9d91e8b61dc77b79ea43befeb72a1a8c89af3e8a40dbdba5b3a6b5f7510e91"))
        ( 30300, uint256S("0x6340483a4bdd4e3a519a292ae4bc424dc12b8c72ef8f3cf3762347afc0a699c0"))
        ( 35000, uint256S("0xe14bac6cfea31014bb057500160fb5a962e492ce16652b14fa07314fd9e523ff"))
        ( 45000, uint256S("0x97b4cff99eda714dbff09881e339d1159e5558486e31198affd712ca806f0b1d"))
        ( 86600, uint256S("0x9e3e0388b4712f2787cd443a7dbeeda12e90b98e909877cf814e7d5a60fc4b85"))
        ( 100000, uint256S("0x5e831ed155d05f6ac7f17635022dbc348bf73942309ac403c6f8c2990e2e0af1"))
        ( 125000, uint256S("0xee27d0f4b6596f302eb591072136ae196bb318d776c16625b23cc7383052b564"))
        ( 150000, uint256S("0x97fdb21189d5a958d42fcb58f8d300e737a20fad91878dabdd925d11fc614013"))
        ( 175000, uint256S("0xda6aa09113ddd62d871e9aacad6131831d5841a26968f1665a9b829fd30a29e3"))
        ( 200000, uint256S("0x4e80313f4eb23093a63218f3736379084d1eeae46c4343668f3cdc9c0c5ca260"))
        ( 212000, uint256S("0x1301ebdd83f6a9c224de33817d69e3fa339769acfd4401cbc3c3c88202c3dbdb"))
        ( 225000, uint256S("0xc9a5c5226d8f103972ffee38c31c3508189b694e0d4f93a394ccea2cac82ce49"))
        ( 233500, uint256S("0x7b16385152001b51c25004e04b1f62906088027d8753449bc36db88ef540aaaa"))
        ( 250000, uint256S("0xa6635e1dbce15cfb4be7f3f464f612205dd13ba96828535000b99ce04648500d"))
        ( 260000, uint256S("0x42c2254ffd8be411386b9089fec985fe3a06d5fc386ff0bd494b5a3aa292f107"))
        ( 280350, uint256S("0xf95b3e7f97a41db38a872bdd15d985aae252c5ab497a51319e5bd50161a48d18"))
        ;
static const Checkpoints::CCheckpointData data = {
        &mapCheckpoints,
        1443116050, // * UNIX timestamp of last checkpoint block
        680226,    // * total number of transactions between genesis and last checkpoint
                    //   (the tx=... number in the SetBestChain debug.log lines)
        350.0     // * estimated number of transactions per day after checkpoint
    };

static Checkpoints::MapCheckpoints mapCheckpointsTestnet =
        boost::assign::map_list_of
        ( 0, uint256S("0xbff0fcf9a89d4d4d6e00414e1d67ef495608c6569f7fbb5276cd20a46127f329"))
        (  50000, uint256S("0x57656e366d3ac7bee3cea4cffec0fdae54274774aa82acd03e3a2b08423f6d64"))
        ( 100000, uint256S("0x65e0d7c58bdd5632bd64c374146c8bee7d4ef16ff5add401cb9250e78329abae"))
        ( 150000, uint256S("0xa5ec2fe07194495c8b9e02e660f130f28f40f36d01c1bf932673fba021cfa43e"))
        ( 200000, uint256S("0x18e82b93c0526a99adb5522e311b71742f1731552eecc29edd7e283a9213eaa9"))
        ( 250000, uint256S("0xbf110b2a5b3520d6d25c4fb592ccc552744338c283b7501bf42baadfca25ace0"))
        ;
static const Checkpoints::CCheckpointData dataTestnet = {
        &mapCheckpointsTestnet,
        1399759200,
        0,
        1
    };

static Checkpoints::MapCheckpoints mapCheckpointsRegtest =
        boost::assign::map_list_of
        ( 0, uint256S("0x6dbdcc5f450c07c61b51b492021dee6b4bd246c8dd578fd73e8f6c28cfe0393b"))
        ;
static const Checkpoints::CCheckpointData dataRegtest = {
        &mapCheckpointsRegtest,
        1296688602,
        0,
        0
    };

class CMainParams : public CChainParams {
public:
    CMainParams() {
        strNetworkID = "main";
        consensus.nSubsidyHalvingInterval = 840000;
        consensus.nMajorityEnforceBlockUpgrade = 750;
        consensus.nMajorityRejectBlockOutdated = 950;
        consensus.nMajorityWindow = 1000;
        consensus.powLimit = ~arith_uint256(0) >> 20;// Gulden: starting difficulty is 1 / 2^12
        consensus.nPowTargetTimespan = 3.5 * 24 * 60 * 60; // Gulden: 3.5 days
        consensus.nPowTargetSpacing = 2.5 * 60; // Gulden: 2.5 minutes
        consensus.fPowAllowMinDifficultyBlocks = false;
        /** 
         * The message start string is designed to be unlikely to occur in normal data.
         * The characters are rarely used upper ASCII, not valid as UTF-8, and produce
         * a large 4-byte int at any alignment.
         */
        pchMessageStart[0] = 0xfc; // 'N' + 0xb0
        pchMessageStart[1] = 0xfe; // 'L' + 0xb0
        pchMessageStart[2] = 0xf7; // 'G' + 0xb0
        pchMessageStart[3] = 0xe0; // 0xe0 (e for "echt", testnet has 0x00 as last byte)
        vAlertPubKey = ParseHex("073513ffe7147aba88d33aea4da129d8a2829c545526d5d854ab51d5778f4d0625431ba1c5a3245bdfe8736b127fdfdb488de72640727d37355c4c3a66c547efad");
        nDefaultPort = 9231;
        nMinerThreads = 0;

        /**
         * Build the genesis block. Note that the output of the genesis coinbase cannot
         * be spent as it did not originally exist in the database.
         * 
         * CBlock(hash=000000000019d6, ver=1, hashPrevBlock=00000000000000, hashMerkleRoot=4a5e1e, nTime=1231006505, nBits=1d00ffff, nNonce=2083236893, vtx=1)
         *   CTransaction(hash=4a5e1e, ver=1, vin.size=1, vout.size=1, nLockTime=0)
         *     CTxIn(COutPoint(000000, -1), coinbase 04ffff001d0104455468652054696d65732030332f4a616e2f32303039204368616e63656c6c6f72206f6e206272696e6b206f66207365636f6e64206261696c6f757420666f722062616e6b73)
         *     CTxOut(nValue=50.00000000, scriptPubKey=0x5F1DF16B2B704C8A578D0B)
         *   vMerkleTree: 4a5e1e
         */
        const char* pszTimestamp = "On januari 1st the Dutch lost there beloved Gulden";
        CMutableTransaction txNew;
        txNew.vin.resize(1);
        txNew.vout.resize(1);
        txNew.vin[0].scriptSig = CScript() << 486604799 << CScriptNum(4) << vector<unsigned char>((const unsigned char*)pszTimestamp, (const unsigned char*)pszTimestamp + strlen(pszTimestamp));
        txNew.vout[0].nValue = 0;
        txNew.vout[0].scriptPubKey = CScript() << 0x0 << OP_CHECKSIG; // a privkey for that 'vanity' pubkey would be interesting ;)
        genesis.vtx.push_back(txNew);
        genesis.hashPrevBlock.SetNull();
        genesis.hashMerkleRoot = genesis.BuildMerkleTree();
        genesis.nVersion = 1;
        genesis.nTime    = 1009843200;
        genesis.nBits    = 0x1e0ffff0;
        genesis.nNonce   = 2200095;

        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock == uint256S("0x6c5d71a461b5bff6742bb62e5be53978b8dec5103ce52d1aaab8c6a251582f92"));
        assert(genesis.hashMerkleRoot == uint256S("0x4bed0bcb3e6097445ae68d455137625bb66f0e7ba06d9db80290bf72e3d6dcf8"));

        // To check the status of the seeds visit: https://seeds.guldencoin.com

        vSeeds.push_back(CDNSSeedData("seed 0",  "seed-000.gulden.com"));
        vSeeds.push_back(CDNSSeedData("seed 1",  "seed-001.gulden.blue"));
        vSeeds.push_back(CDNSSeedData("seed 2",  "seed-002.gulden.network"));
        vSeeds.push_back(CDNSSeedData("seed 3",  "seed-003.gulden.com"));
        vSeeds.push_back(CDNSSeedData("seed 4",  "seed-004.gulden.blue"));
        vSeeds.push_back(CDNSSeedData("seed 5",  "seed-005.gulden.network"));

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,38);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,5);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,38+128);
        //Gulden: Comment out for now, until we can address at a later date what we want to do with these https://github.com/nlgcoin/gulden-dev/issues/17
        //base58Prefixes[EXT_PUBLIC_KEY] = boost::assign::list_of(0x04)(0x88)(0xB2)(0x1E).convert_to_container<std::vector<unsigned char> >();
        //base58Prefixes[EXT_SECRET_KEY] = boost::assign::list_of(0x04)(0x88)(0xAD)(0xE4).convert_to_container<std::vector<unsigned char> >();

        convertSeed6(vFixedSeeds, pnSeed6_main, ARRAYLEN(pnSeed6_main));

        fRequireRPCPassword = true;
        fMiningRequiresPeers = true;
        fDefaultCheckMemPool = false;
        fRequireStandard = true;
        fMineBlocksOnDemand = false;
        fTestnetToBeDeprecatedFieldRPC = false;
    }

    const Checkpoints::CCheckpointData& Checkpoints() const 
    {
        return data;
    }
};
static CMainParams mainParams;

/**
 * Testnet (v3)
 */
class CTestNetParams : public CMainParams {
public:
    CTestNetParams() {
        strNetworkID = "test";
        consensus.nMajorityEnforceBlockUpgrade = 51;
        consensus.nMajorityRejectBlockOutdated = 75;
        consensus.nMajorityWindow = 100;
        consensus.powLimit = ~arith_uint256(0) >> 10;
        consensus.fPowAllowMinDifficultyBlocks = true;
        pchMessageStart[0] = 0xfc; // 'N' + 0xb0
        pchMessageStart[1] = 0xfe; // 'L' + 0xb0
        pchMessageStart[2] = 0xf7; // 'G' + 0xb0
        pchMessageStart[3] = 0x00; // 0x00
        vAlertPubKey = ParseHex("06087071e40ddf2ecbdf1ae40f536fa8f78e9383006c710dd3ecce957a3cb9292038d0840e3be5042a6b863f75dfbe1cae8755a0f7887ae459af689f66caacab52");
        nDefaultPort = 9923;
        nMinerThreads = 0;

        //! Modify the testnet genesis block so the timestamp is valid for a later start.
        genesis.nTime = 1399759200;
        genesis.nNonce = 397616;
        genesis.nBits = arith_uint256((~arith_uint256(0) >> 10)).GetCompact();
        consensus.hashGenesisBlock = genesis.GetHash();


        assert(consensus.hashGenesisBlock == uint256S("0xbff0fcf9a89d4d4d6e00414e1d67ef495608c6569f7fbb5276cd20a46127f329"));

        #if 0
        arith_uint256 thash;
        char scratchpad[SCRYPT_SCRATCHPAD_SIZE];
        scrypt_1024_1_1_256_sp(BEGIN(genesis.nVersion), BEGIN(thash), scratchpad);
        arith_uint256 foo;
        while(thash > consensus.powLimit)
        {
            genesis.nNonce++;
            scrypt_1024_1_1_256_sp(BEGIN(genesis.nVersion), BEGIN(thash), scratchpad);
            if(genesis.nNonce == 0)
                genesis.nTime++;
        }
        printf("%d\n",genesis.nNonce);
        printf("%d\n",genesis.nTime);
        printf("%d\n",genesis.nBits);
        consensus.hashGenesisBlock = genesis.GetHash();
        printf("%s\n", consensus.hashGenesisBlock.ToString().c_str());
        exit(1);
        #endif


        vFixedSeeds.clear();
        vSeeds.clear();
        vSeeds.push_back(CDNSSeedData("seed 0",  "testseed-00.gulden.blue"));
        vSeeds.push_back(CDNSSeedData("seed 1",  "testseed-01.gulden.network"));


        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,98);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,196);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,98+128);
        //Gulden: Comment out for now, until we can address at a later date what we want to do with these https://github.com/nlgcoin/gulden-dev/issues/17
        //base58Prefixes[EXT_PUBLIC_KEY] = boost::assign::list_of(0x04)(0x88)(0xB2)(0x1E).convert_to_container<std::vector<unsigned char> >();
        //base58Prefixes[EXT_SECRET_KEY] = boost::assign::list_of(0x04)(0x88)(0xAD)(0xE4).convert_to_container<std::vector<unsigned char> >();


        convertSeed6(vFixedSeeds, pnSeed6_test, ARRAYLEN(pnSeed6_test));

        fRequireRPCPassword = true;
        fMiningRequiresPeers = true;
        fDefaultCheckMemPool = false;
        fRequireStandard = false;
        fMineBlocksOnDemand = false;
        fTestnetToBeDeprecatedFieldRPC = true;
    }
    const Checkpoints::CCheckpointData& Checkpoints() const 
    {
        return dataTestnet;
    }
};
static CTestNetParams testNetParams;


/**
 * Testnet - accelerated
 */
#include <cstdio>
#include "arith_uint256.h"
class CTestNetAcceleratedParams : public CMainParams {
public:
    virtual void ParseCommandLine()
    {
        consensus.nPowTargetSpacing = GetArg("-targetspeed", consensus.nPowTargetSpacing);
    }
    CTestNetAcceleratedParams() {
        strNetworkID = "testnetaccel";
        consensus.nMajorityEnforceBlockUpgrade = 51;
        consensus.nMajorityRejectBlockOutdated = 75;
        consensus.nMajorityWindow = 100;
        consensus.powLimit = ~arith_uint256(0) >> 10;
        consensus.nPowTargetSpacing = 150;
        consensus.fPowAllowMinDifficultyBlocks = true;
        pchMessageStart[0] = 0xfc; // 'N' + 0xb0
        pchMessageStart[1] = 0xfe; // 'L' + 0xb0
        pchMessageStart[2] = 0xf7; // 'G' + 0xb0
        pchMessageStart[3] = 0x01; // 0x01
        vAlertPubKey = ParseHex("06087071e40ddf2ecbdf1ae40f536fa8f78e9383006c710dd3ecce957a3cb9292038d0840e3be5042a6b863f75dfbe1cae8755a0f7887ae459af689f66caacab52");
        nDefaultPort = 9928;
        nMinerThreads = 0;

        //! Modify the testnet genesis block so the timestamp is valid for a later start.
        genesis.nTime = 1430359579;
        genesis.nNonce = 821434;
        genesis.nBits = arith_uint256((~arith_uint256(0) >> 10)).GetCompact();
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock == uint256S("0x6366b541deb79f066c65dfe451f676f07e3179e4cf91ae5dd055e230543eb5a3"));

        #if 0
        arith_uint256 thash;
        hash_city(BEGIN(genesis.nVersion), thash);
        arith_uint256 foo;
        while(thash > consensus.powLimit)
        {
            if(genesis.nNonce % 20000 == 0)
            {
                printf("%s  %s\n",thash.ToString().c_str(), consensus.powLimit.ToString().c_str());
                printf("%d\n", genesis.nNonce);
            }

            genesis.nNonce++;
            hash_city(BEGIN(genesis.nVersion), thash);
            if(genesis.nNonce == 0)
                genesis.nTime++;
        }
        printf("%d\n",genesis.nNonce);
        printf("%d\n",genesis.nTime);
        consensus.hashGenesisBlock = genesis.GetHash();
        printf("%s\n", consensus.hashGenesisBlock.ToString().c_str());
        exit(1);
        #endif

        vFixedSeeds.clear();
        vSeeds.clear();
        vSeeds.push_back(CDNSSeedData("seed 0",  "testseed-00.gulden.blue"));
        vSeeds.push_back(CDNSSeedData("seed 1",  "testseed-01.gulden.network"));

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,111);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,196);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,111+128);
        //Gulden: Comment out for now, until we can address at a later date what we want to do with these https://github.com/nlgcoin/gulden-dev/issues/17
        //base58Prefixes[EXT_PUBLIC_KEY] = boost::assign::list_of(0x04)(0x88)(0xB2)(0x1E).convert_to_container<std::vector<unsigned char> >();
        //base58Prefixes[EXT_SECRET_KEY] = boost::assign::list_of(0x04)(0x88)(0xAD)(0xE4).convert_to_container<std::vector<unsigned char> >();


        convertSeed6(vFixedSeeds, pnSeed6_test, ARRAYLEN(pnSeed6_test));

        fRequireRPCPassword = true;
        fMiningRequiresPeers = false;
        fDefaultCheckMemPool = false;
        fRequireStandard = false;
        fMineBlocksOnDemand = false;
        fTestnetToBeDeprecatedFieldRPC = true;
    }
    const Checkpoints::CCheckpointData& Checkpoints() const 
    {
        return dataTestnet;
    }
};
static CTestNetAcceleratedParams testNetAcceletaredParams;

/**
 * Regression test
 */
class CRegTestParams : public CTestNetParams {
public:
    CRegTestParams() {
        strNetworkID = "regtest";
        consensus.nSubsidyHalvingInterval = 150;
        consensus.nMajorityEnforceBlockUpgrade = 750;
        consensus.nMajorityRejectBlockOutdated = 950;
        consensus.nMajorityWindow = 1000;
        consensus.powLimit = ~arith_uint256(0) >> 1;
        pchMessageStart[0] = 0xfc; // 'N' + 0xb0
        pchMessageStart[1] = 0xfe; // 'L' + 0xb0
        pchMessageStart[2] = 0xf7; // 'G' + 0xb0
        pchMessageStart[3] = 0x00; // 0x00
        nMinerThreads = 1;
        genesis.nTime = 1296688602;
        genesis.nBits = 0x1e0ffff0;
        genesis.nNonce = 2;
        consensus.hashGenesisBlock = genesis.GetHash();
        nDefaultPort = 18444;
        assert(consensus.hashGenesisBlock == uint256S("0x6dbdcc5f450c07c61b51b492021dee6b4bd246c8dd578fd73e8f6c28cfe0393b"));

        vFixedSeeds.clear(); //! Regtest mode doesn't have any fixed seeds.
        vSeeds.clear();  //! Regtest mode doesn't have any DNS seeds.

        fRequireRPCPassword = false;
        fMiningRequiresPeers = false;
        fDefaultCheckMemPool = true;
        fRequireStandard = false;
        fMineBlocksOnDemand = true;
        fTestnetToBeDeprecatedFieldRPC = false;
    }
    const Checkpoints::CCheckpointData& Checkpoints() const 
    {
        return dataRegtest;
    }
};
static CRegTestParams regTestParams;

static CChainParams *pCurrentParams = 0;

const CChainParams &Params() {
    assert(pCurrentParams);
    return *pCurrentParams;
}

CChainParams &Params(CBaseChainParams::Network network) {
    switch (network) {
        case CBaseChainParams::MAIN:
            return mainParams;
        case CBaseChainParams::TESTNET:
            return testNetParams;
        case CBaseChainParams::TESTNET_ACCELERATED:
            return testNetAcceletaredParams;
        case CBaseChainParams::REGTEST:
            return regTestParams;
        default:
            assert(false && "Unimplemented network");
            return mainParams;
    }
}

void SelectParams(CBaseChainParams::Network network) {
    SelectBaseParams(network);
    pCurrentParams = &Params(network);
}

bool SelectParamsFromCommandLine()
{
    CBaseChainParams::Network network = NetworkIdFromCommandLine();
    if (network == CBaseChainParams::MAX_NETWORK_TYPES)
        return false;

    SelectParams(network);
    pCurrentParams->ParseCommandLine();
    return true;
}
