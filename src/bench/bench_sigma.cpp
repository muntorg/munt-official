// Copyright (c) 2019 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#include "util.h"
#include <crypto/hash/sigma/sigma.h>
#include <iostream>
#include <boost/program_options.hpp>
#include <thread>

#include <crypto/hash/sigma/sigma.h>

#include <cryptopp/config.h>
#include <cryptopp/aes.h>
#include <cryptopp/modes.h>
#include <random.h>

// Are we running with all options at default or are any overriden by user.
bool defaultSigma = true;

// User paramaters (adjustable on individual machines)
uint64_t numThreads = std::thread::hardware_concurrency();
uint64_t memAllowGb;
uint64_t numUserVerifyThreads;
uint64_t numFullHashesTarget = 50000;
bool mineOnly=false;
    
using namespace boost::program_options;
int LogPrintStr(const std::string &str)
{
    std::cout << str;
    return 1;
}

std::vector<std::string> hashTestVector = {
    "A",
    "AA",
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ",
    "B33F761F0D3A86BB1051905AEC7A691BD0B5A24C3721F67D8E48D839",
    "D4773C6DAFE49604B4DF73725512483DB17578CD209C27ABB39782D8", 
    "15D9174CBD4D22F8C49FE874F45EBBF23806DEEC190B20BC67945833", 
    "262C26AD4EDCD692FFB1B859CE729AD61BA67D6FA72AC7A7509D92E8", 
    "6FBD09FCE4600327A97540BFF4DF7A99DA7C13F8CB13FA39838EC010"
};
// Test vector results generated against latest reference implementation(s) of SHAvite3 in supercop. Not the website version which appears to
std::vector shaviteTestVectorOut = {
    "9206c5e42ac244c08d8383caa80093fb6e4d88ae8d11edf9c2d08442cba89337",
    "4f4fd087b2831ca55ad51df0e18cd574bb30ec8089a42340142d2c2bb5b23ccc",
    "2e747b74196c20b94253ca56730a28dda2aa49420bd53e053ed77a329a8f8469",
    "df2318831b70ceb7986c722ba65df78fa51924bf5bd6582dd516b9357a47660f",
    "f75fa0c12a54b93e122a3c5d476dd489e96aab749344f7062814f662d0460fa1",
    "69a8a32cfe0f0f5e510e97f22a63ecb211425fe2f26267827e244e4ebe70774d",
    "d0316c6d924f45913a8f0328a448f218dd18a1b28e38bf5355d8201a65bcc0b2",
    "ad48f660cd6328ebbc5c40a73dfa365aab770e7ba08220b051633c5f8ee4b157",
};
std::vector echo256TestVectorOut = {
    "1ffdd51e506ea633440daf864f02e80eb3c5371da068c39c11e3e7d637f60659",
    "c5ddce445e589b14213707e68afda6d651736b6b4d223733bb5e68489492856e",
    "949e61e69a5c473996432f4f137a98d4774dfaf503020565232e246b73fb8a68",
    "960d74a005576c431a21f3f8ec8a4644094eb98e7ce14394ac92709d23097c03",
    "7c002e865862c73e78c616e36b2f96341abbe018383163f16c6be934c7bebfb0",
    "70de7acca4efe39c85e419964548155a212861dc9378b600f671f82be4ca4df0",
    "a10db9cf5e865376b06a38108dc47397e90b6ffce7d73cb5670450a9ca077611",
    "3d36df7adf6b181e5a6b3b10a09797917dbe2291b5548ec0652dde1e6f16a23c"
};

std::vector argonTestVectorOut = {
    "c71df45da212b4066959f6411b3abd52eea22f25720753fc",
    "3fe7f820f1f83699b882f59b7f977e84221afd31ad32953c",
    "e7ef1e8f80c8c2e60ae7a81ae4dbbfcf0f20ae05146b335c",
    "241e702180722bc5075941a4aafd04881e0701ddb7385cdd",
    "0fc93233552419a6d036617f891576c0df81e70cb610be45",
    "54098b89532a93f1f6055b05c5b561525c9d474bbc63fc0d",
    "94c1ff956631b7022b904118b0f25b5de2ad977e0bfa0a43",
    "33804bdf5e2bea6557e6a9a5cd100e8ad340ab212c88d8f8"
};

std::vector prngTestVectorOut = {
    "ff2c21f49ae9aa37a80646d739caf085cd94b822db0676413c62b84f51bde9",
    "0c61008393ca6ece11a192182a798cf49f3bafdfbe54c42ff3fa6fa86fc687",
    "30542c1cd581a3ca5d5a4456bac5275663a522bc10f228089cb1a8c6ad6f84",
    "28db8c95db562c79ef25fc9fb7cd640dfb4ae8afe1e5c3115f755d6337282d",
    "51eba29e2a7b21596f5149476f396b58af3cfdcb0f7b2f855f964c0d548324",
    "566af98eb1efe32fc8b07ec571e65f4e64f428c3b68708800133e4d8e920f5",
    "80f0f7bbf45342427938b4bac8c44c81d1dab08b85c73556acc2c6cbc84217",
    "a701a7d61279cbef20fa66c17ed67d7d6308a5035264de1ec64d066bcd6189"
};


void testShaviteReference(uint64_t& nTestFailCount)
{
    for (unsigned int i=0;i<hashTestVector.size();++i)
    {
        std::string data = hashTestVector[i];
        std::vector<unsigned char> outHash(32);
        shavite3_ref_hashState ctx_shavite;
        shavite3_ref_Init(&ctx_shavite);
        shavite3_ref_Update(&ctx_shavite, (uint8_t*)&data[0], data.size());
        shavite3_ref_Final(&ctx_shavite, (uint8_t*)&outHash[0]);
        std::string outHashHex = HexStr(outHash.begin(), outHash.end()).c_str();
        std::string compare(shaviteTestVectorOut[i]);
        if (outHashHex == compare)
        {
            LogPrintf("✔");
        }
        else
        {
            ++nTestFailCount;
            LogPrintf("✘");
            LogPrintf("%s\n", outHashHex);
        }
    }
    LogPrintf("\n");
}

void testShaviteOptimised(uint64_t& nTestFailCount)
{
    for (unsigned int i=0;i<hashTestVector.size();++i)
    {
        std::string data = hashTestVector[i];
        std::vector<unsigned char> outHash(32);

        shavite3_256_opt_hashState ctx_shavite;
        selected_shavite3_256_opt_Init(&ctx_shavite);
        selected_shavite3_256_opt_Update(&ctx_shavite, (uint8_t*)&data[0], data.size());
        selected_shavite3_256_opt_Final(&ctx_shavite, &outHash[0]);
        
        std::string outHashHex = HexStr(outHash.begin(), outHash.end()).c_str();
        std::string compare(shaviteTestVectorOut[i]);
        if (outHashHex == compare)
        {
            LogPrintf("✔");
        }
        else
        {
            ++nTestFailCount;
            LogPrintf("✘");
            LogPrintf("%s\n", outHashHex);
        }
    }
    LogPrintf("\n");
}

void testEchoReference(uint64_t& nTestFailCount)
{
    for (unsigned int i=0;i<hashTestVector.size();++i)
    {
        std::string data = hashTestVector[i];
        std::vector<unsigned char> outHash(32);
        sph_echo256_context ctx_echo;
        sph_echo256_init(&ctx_echo);
        sph_echo256(&ctx_echo, (uint8_t*)&data[0], data.size());
        sph_echo256_close(&ctx_echo, (void*)&outHash[0]);
        std::string outHashHex = HexStr(outHash.begin(), outHash.end()).c_str();
        std::string compare(echo256TestVectorOut[i]);
        if (outHashHex == compare)
        {
            LogPrintf("✔");
        }
        else
        {
            ++nTestFailCount;
            LogPrintf("✘");
            LogPrintf("%s\n", outHashHex);
        }
    }
    LogPrintf("\n");
}

void testEchoOptimised(uint64_t& nTestFailCount)
{
    for (unsigned int i=0;i<hashTestVector.size();++i)
    {
        std::string data = hashTestVector[i];
        std::vector<unsigned char> outHash(32);
        
        echo256_opt_hashState ctx_echo;
        selected_echo256_opt_Init(&ctx_echo);
        selected_echo256_opt_Update(&ctx_echo, (uint8_t*)&data[0], data.size());
        selected_echo256_opt_Final(&ctx_echo, &outHash[0]); 
       
        std::string outHashHex = HexStr(outHash.begin(), outHash.end()).c_str();
        std::string compare(echo256TestVectorOut[i]);
        if (outHashHex == compare)
        {
            LogPrintf("✔");
        }
        else
        {
            ++nTestFailCount;
            LogPrintf("✘");
            LogPrintf("%s\n", outHashHex);
        }
    }
    LogPrintf("\n");
}

void testArgonReference(uint64_t& nTestFailCount)
{
    for (unsigned int i=0;i<hashTestVector.size();++i)
    {
        std::string data = hashTestVector[i];
        
        uint8_t argonScratch[1024*32];
        argon2_echo_context context;
        context.t_cost = 5;
        context.m_cost = 32;
        context.allocated_memory = argonScratch;
        context.pwd = (uint8_t*)&data[0];
        context.pwdlen = data.size();
        context.lanes = 4;
        context.threads = 1;
        
        argon2_echo_ctx_ref(&context, true);
            
        std::string outHashHex = HexStr((uint8_t*)(&context.outHash[0]), (uint8_t*)(&context.outHash[3])).c_str();
        std::string compare(argonTestVectorOut[i]);
        if (outHashHex == compare)
        {
            LogPrintf("✔");
        }
        else
        {
            ++nTestFailCount;
            LogPrintf("✘");
            LogPrintf("%s\n", outHashHex);
        }
    }
    LogPrintf("\n");
}

void testArgonOptimised(uint64_t& nTestFailCount)
{
    for (unsigned int i=0;i<hashTestVector.size();++i)
    {
        std::string data = hashTestVector[i];
        
        uint8_t argonScratch[1024*32];
        argon2_echo_context context;
        context.t_cost = 5;
        context.m_cost = 32;
        context.allocated_memory = argonScratch;
        context.pwd = (uint8_t*)&data[0];
        context.pwdlen = data.size();
        context.lanes = 4;
        context.threads = 1;
        
        selected_argon2_echo_hash(&context, true);
            
        std::string outHashHex = HexStr((uint8_t*)(&context.outHash[0]), (uint8_t*)(&context.outHash[3])).c_str();
        std::string compare(argonTestVectorOut[i]);
        if (outHashHex == compare)
        {
            LogPrintf("✔");
        }
        else
        {
            ++nTestFailCount;
            LogPrintf("✘");
            LogPrintf("%s\n", outHashHex);
        }
    }
    LogPrintf("\n");
}

void testPRNG(uint64_t& nTestFailCount)
{
    CryptoPP::ECB_Mode<CryptoPP::AES>::Encryption prng;
    
    
    for (unsigned int i=0;i<hashTestVector.size();++i)
    {
        std::string data = hashTestVector[i];
        if (data.length() < 32)
        {
            data = data + std::string(32-data.length(), 'a');
        }
        
        prng.SetKey((const unsigned char*)&data[0], 32);
        unsigned char ciphered[32];
        prng.ProcessData((unsigned char*)&ciphered[0], (const unsigned char*)&data[0], 32);
        memcpy(&data[0], &ciphered[0], (size_t)32);
        prng.ProcessData((unsigned char*)&ciphered[0], (const unsigned char*)&data[0], 32);
        memcpy(&data[0], &ciphered[0], (size_t)32);
        prng.ProcessData((unsigned char*)&ciphered[0], (const unsigned char*)&data[0], 32);
        memcpy(&data[0], &ciphered[0], (size_t)32);
                   
        std::string outHashHex = HexStr(&ciphered[0], &ciphered[31]).c_str();
        std::string compare(prngTestVectorOut[i]);
        if (outHashHex == compare)
        {
            LogPrintf("✔");
        }
        else
        {
            ++nTestFailCount;
            LogPrintf("✘");
            LogPrintf("%s\n", outHashHex);
        }
    }
    LogPrintf("\n");
}


std::vector<std::string>
validHeaderTestVectorIn = {
    "a0da8101c2c1f7ae98536a4c215f48e38ca17e925f61234c9aa602205d6fd5e27eb0c885452abf7e8515e3e7c4758cefd55c9331760b7c711280a451067c6b01e0e46234a7a5845dffff3f1f1700ee6c",
    "b0d6563c856c89be804b189b9a36856b795770243b2f2b4efab1b8f021962c3129da01148537c2f19b7d52321415d834cc04ddf199022f105ebf194d4e6470f8bab50a21aba5845dffff3f1f0e00b01f",
    "47a5866c8b1e9eeff7f62b654d0cd2e70785cd13c2e1446cc74231df906ba4f4113413a5b56e88336447db61875cf4862e5587ed25d26de8a0c61c8c23070a4a62eea35eaea5845dffff3f1f1a003d51",
    "72b6032ae5c1db9982e2d9135052ec3cc47c7414947da6e55a76e47b4b2ec6744a3140ba0baf06a69dece43f9dbba5008ca490a3098c41d06279af6811c386080ad0b1cdb2a5845dffff3f1f1200d425",
    "73d7b147d565af53774df3252c7dbf358fb8a8328ead1b656c3d216b533a2bffcc16869309825486c5945373a11d4219531948a9d8d5af501b1cdc0fe83c2ebb5f1d37d8b5a5845dffff3f1f1800f632"
};
void testValidateValidHeaders(sigma_settings settings, uint64_t& nTestFailCount)
{
    CBlockHeader header;
    for (const auto& hash : validHeaderTestVectorIn)
    {
        std::vector<unsigned char> data = ParseHex(hash);
        memcpy(&header.nVersion, &data[0], 80);
        sigma_verify_context verify(settings, numUserVerifyThreads);
        if (verify.verifyHeader(header))
        {
            LogPrintf("✔");
        }
        else
        {
            LogPrintf("✘");
            ++nTestFailCount;
        }
        
    }
    LogPrintf("\n");
}

std::vector<std::string>
invalidHeaderTestVectorIn = {
    "c6ee0071f1f3ffffd5485a7a43264e0e10b6c760154a082117c7b0671339c55dfec8574c7e3e5158e7fba254588c0be72e5df6d502206aa9c43216f529e71ac83e84f512c4a63589ea7f1c2c1018ad0a",
    "f10b00e0f1f3ffffd5485aba12a05bab8f0746e4d491fbe501f220991fdd40cc438d51412325d7b91f2c73584110ad9213c269120f8b1bafe4b2f2b342077597b65863a9b981b408eb98c658c3656d0b",
    "15d300a1f1f3ffffd5485aeae53aee26a4a07032c8c16c0a8ed62d52de7855e2684fc57816bd74463388e65b5a3143114f4ab609fd13247cc6441e2c31dc58707e2dc0d456b26f7ffee9e1b8c6685a74",
    "524d0021f1f3ffffd5485a2bdc1b0da080683c1186fa97260d14c8903a094ac8005abbd9f34eced96a60fab0ab0413a4476ce2b4b74e67a55e6ad7494147c74cc3ce2505319d2e2899bd1c5ea2306b27",
    "236f0081f1f3ffffd5485a5b8d73d1f5bbe2c38ef0cdc1b105fa5d8d9a8491359124d11a3735495c68452890396861ccffb2a335b612d3c656b1dae8238a8bf853fbd7c2523fd47735fa565d741b7d37"
};
void testValidateInvalidHeaders(sigma_settings settings, uint64_t& nTestFailCount)
{
    CBlockHeader header;
    for (const auto& hash : invalidHeaderTestVectorIn)
    {
        std::vector<unsigned char> data = ParseHex(hash);
        memcpy(&header.nVersion, &data[0], 80);
        sigma_verify_context verify(settings, numUserVerifyThreads);
        if (!verify.verifyHeader(header))
        {
            LogPrintf("✔");
        }
        else
        {
            LogPrintf("✘");
            ++nTestFailCount;
        }
    }
    LogPrintf("\n");
}

double calculateSustainedHashrateForTimePeriod(uint64_t maxHashesPre, uint64_t maxHashesPost, double nHalfHashAverage, uint64_t nArenaSetuptime, uint64_t nTimePeriodSeconds)
{
    // We need to recalculate the arenas every time we exhaust the hash space, or when a new block comes in, whichever comes first.
    // Block target is 2.5 minutes, so we assume that the maximum time we can mine in between arena calculations is 3 minutes,
    // or the full hash space, whichever is lower.
    uint64_t maxHalfHashes = maxHashesPre*maxHashesPost;
    maxHalfHashes = std::min(maxHalfHashes, (uint64_t)((nTimePeriodSeconds*1000000)/nHalfHashAverage));
    
    // The total time is then the arena setup time plus the time that calculating maxHalfHashes would take (all in microseconds)
    uint64_t nTimeTotal = nArenaSetuptime + (maxHalfHashes*nHalfHashAverage);
    
    // Finally we can get a sustained hashrate by calculating the time per hash and then calculating how many hashes per second.
    double nSustainedMicrosecondsPerHash = ((double)nTimeTotal / (double)maxHalfHashes);
    double nSustainedHashesPerMicrosecond = (1/nSustainedMicrosecondsPerHash);
    return nSustainedHashesPerMicrosecond * 1000000;
}
    
int main(int argc, char** argv)
{
    memAllowGb = defaultSigmaSettings.arenaSizeKb/1024/1024;
    numUserVerifyThreads = defaultSigmaSettings.numVerifyThreads;
    selected_argon2_echo_hash = argon2_echo_ctx_ref;
    selectOptimisedImplementations();

    srand(GetTimeMicros());
    
    // Declare the supported options.
    options_description desc("Allowed options");
    desc.add_options()
    ("help", "produce help message")
    ("mine-threads", value<int64_t>(), "Set number of threads to use for mining")
    ("mine-memory", value<int64_t>(), "Set how much memory in gb to mine with")
    ("mine-num-hashes", value<int64_t>(), "How many full hash attempts to run mining for (default 50000)")
    ("mine-only", value<bool>()->implicit_value(true), "Only benchmark actual mining, skip other benchmarks")
    ("verify-threads", value<int64_t>(), "How many threads to use for verification, may not exceed sigma_verify_threads (defaults to same as sigma_verify_threads)")
    ("sigma-global-mem", value<int64_t>(), "How much global memory optimal mining should require (in gigabytes)")
    ("sigma-num-slow", value<int64_t>(), "How many slow hash attempts to allow for each global memory allocation  (maximum 65536)")
    ("sigma-slowhash-mem", value<int64_t>(), "How much memory each slow hash should consume (in megabytes)")
    ("sigma-slowhash-cpucost", value<int64_t>(), "How many rounds of computation to use in the argon slow hash computation (default 12)")
    ("sigma-arena-cpucost", value<int64_t>(), "How many rounds of computation to use in the arena hash computation (default 8)")
    ("sigma-num-fast", value<int64_t>(), "How many fast hash attempts to allow for each slow hash (maximum 65536)")
    ("sigma-fasthash-mem", value<int64_t>(), "How much of the global memory to digest for each slow hash (bytes - should not exceed the size of a single slow hash)")
    ("sigma-verify-threads", value<int64_t>(), "How many threads to allow for slow hashes and therefore verification. (Default 4)");
    
    
    variables_map vm;
    try
    {
        store(parse_command_line(argc, argv, desc), vm);
        notify(vm);    
    }
    catch (std::exception& e)
    {
        LogPrintf("%s\n", e.what());
        LogPrintf("%s", desc);
        return 1;
    }

    if (vm.size() == 0)
    {
        LogPrintf("Using default options use '--help' to see a list of possible options.\n\n");
    }
    else
    {
        LogPrintf("Using non default options use '--help' to see a list of possible options.\n\n");
    }
    
    if (vm.count("help"))
    {
        LogPrintf("%s", desc);
        return 1;
    }

    if (vm.count("mine-threads"))
    {
        numThreads = vm["mine-threads"].as<int64_t>();
    }
    if (vm.count("sigma-global-mem"))
    {
        defaultSigmaSettings.arenaSizeKb = vm["sigma-global-mem"].as<int64_t>() * 1024 * 1024;
        defaultSigma = false;
    }
    memAllowGb = defaultSigmaSettings.arenaSizeKb / 1024 / 1024;
    if (vm.count("mine-memory"))
    {
        memAllowGb = vm["mine-memory"].as<int64_t>();
    }
    if (vm.count("mine-num-hashes"))
    {
        numFullHashesTarget = vm["mine-num-hashes"].as<int64_t>();
    }
    if (vm.count("mine-only"))
    {
        mineOnly = vm["mine-only"].as<bool>();;
        defaultSigma = false;
    }
    if (vm.count("verify-threads"))
    {
        numUserVerifyThreads = vm["verify-threads"].as<int64_t>();
    }
    if (vm.count("sigma-num-slow"))
    {
        defaultSigma = false;
        defaultSigmaSettings.numHashesPre = vm["sigma-num-slow"].as<int64_t>();
    }
    if (vm.count("sigma-slowhash-mem"))
    {
        defaultSigmaSettings.argonMemoryCostKb = vm["sigma-slowhash-mem"].as<int64_t>()*1024;
        defaultSigma = false;
    }
    if (vm.count("sigma-arena-cpucost"))
    {
        defaultSigmaSettings.argonArenaRoundCost = vm["sigma-arena-cpucost"].as<int64_t>();
        defaultSigma = false;
    }
    if (vm.count("sigma-slowhash-cpucost"))
    {
        defaultSigmaSettings.argonSlowHashRoundCost = vm["sigma-slowhash-cpucost"].as<int64_t>();
        defaultSigma = false;
    }
    if (vm.count("sigma-num-fast"))
    {
        defaultSigmaSettings.numHashesPost = vm["sigma-num-fast"].as<int64_t>();
        defaultSigma = false;
    }
    if (vm.count("sigma-fasthash-mem"))
    {
        defaultSigmaSettings.fastHashSizeBytes = vm["sigma-fasthash-mem"].as<int64_t>();
        defaultSigma = false;
    }
    if (vm.count("sigma-verify-threads"))
    {
        defaultSigmaSettings.numVerifyThreads = vm["sigma-verify-threads"].as<int64_t>();
        defaultSigma = false;
    }
    
    
    if (numUserVerifyThreads > defaultSigmaSettings.numVerifyThreads)
    {
        LogPrintf("Number of user verify threads may not exceed number of sigma verify threads");
        return 1;
    }
    
    LogPrintf("Configuration=====================================================\n\n");
    LogPrintf("NETWORK:\nGlobal memory cost [%dgb]\nArgon_echo cpu cost for arenas [%d rounds]\nArgon_echo cpu cost for slow hash [%d rounds]\nArgon_echo mem cost [%dMb]\nEcho/Shavite digest size [%d bytes]\nNumber of fast hashes per slow hash [%d]\nNumber of slow hashes per global arena [%d]\nNumber of verify threads [%d]\n\n", defaultSigmaSettings.arenaSizeKb/1024/1024, defaultSigmaSettings.argonArenaRoundCost ,defaultSigmaSettings.argonSlowHashRoundCost, defaultSigmaSettings.argonMemoryCostKb/1024, defaultSigmaSettings.fastHashSizeBytes, defaultSigmaSettings.numHashesPost, defaultSigmaSettings.numHashesPre, defaultSigmaSettings.numVerifyThreads);
    LogPrintf("USER:\nMining with [%d] threads\nMining with [%d gb] memory.\nVerifying with [%s] threads.\n\n", numThreads, memAllowGb, numUserVerifyThreads);
    
    uint64_t memAllowKb = memAllowGb*1024*1024;
    if (memAllowKb == 0)
    {
        memAllowKb = 512*1024;
    }
        
    // If we are using the default params then perform some tests to ensure everything runs the same across different machines
    if (!defaultSigma)
    {
        defaultSigmaSettings.verify();
    }
    else
    {
        LogPrintf("Tests=============================================================\n\n");
        uint64_t nTestFailCount=0;
        
        LogPrintf("Verify shavite reference operation\n");
        testShaviteReference(nTestFailCount);
        
        if (selected_shavite3_256_opt_Final)
        {
            LogPrintf("Verify shavite optimised operation\n");
            testShaviteOptimised(nTestFailCount);
        }
        
        LogPrintf("Verify echo reference operation\n");
        testEchoReference(nTestFailCount);
        
        if (selected_echo256_opt_Final)
        {
            LogPrintf("Verify echo optimised operation\n");
            testEchoOptimised(nTestFailCount);
        }
        
        LogPrintf("Verify argon reference operation\n");
        testArgonReference(nTestFailCount);
        
        if (selected_argon2_echo_hash)
        {
            LogPrintf("Verify argon optimised operation\n");
            testArgonOptimised(nTestFailCount);
        }
        
        LogPrintf("Verify PRNG\n");
        testPRNG(nTestFailCount);
        
        LogPrintf("Verify validation of valid headers\n");
        testValidateValidHeaders(defaultSigmaSettings, nTestFailCount);
        
        LogPrintf("Verify validation of invalid headers\n");
        testValidateInvalidHeaders(defaultSigmaSettings, nTestFailCount);
        
        if (nTestFailCount > 0)
        {
            LogPrintf("Aborting due to [%d] failed tests.\n", nTestFailCount);
            exit(EXIT_FAILURE);
        }
        LogPrintf("\n");
    }
    
    //Random header to benchmark with, we will randomly change it more throughout the tests.
    CBlockHeader header;
    header.nVersion = rand();
    header.hashPrevBlock = GetRandHash();
    header.hashMerkleRoot = GetRandHash();
    header.nTime = rand();
    header.nBits = rand();
    header.nNonce = rand();
    
    if (!mineOnly)
    {
        LogPrintf("Scrypt============================================================\n\n");
        uint256 hash;    
        {
            LogPrintf("Bench cost [single thread]:\n");
            uint64_t nStart = GetTimeMicros(); 
            uint64_t numHashes = 20;
            char scratchpad[SCRYPT_SCRATCHPAD_SIZE];
            for (uint64_t i=0; i< numHashes; ++i)
            {
                header.nNonce = i;
                scrypt_1024_1_1_256_sp(BEGIN(header.nVersion), BEGIN(hash), scratchpad);
            }
            LogPrintf("total [%.2f micros] per hash: [%.2f micros]\n\n", (GetTimeMicros() - nStart), ((GetTimeMicros() - nStart)) / (double)numHashes);
        }
        
        {
            LogPrintf("Bench cost [%d threads]:\n", numThreads);
            uint64_t nStart = GetTimeMicros();
            uint64_t numHashes = 100;
            auto workerThreads = new boost::asio::thread_pool(numThreads);
            for (uint64_t i = 0; i <= numHashes;++i)
            {
                boost::asio::post(*workerThreads, [=]() mutable
                {
                    char scratchpad[SCRYPT_SCRATCHPAD_SIZE];
                    header.nNonce = i;
                    scrypt_1024_1_1_256_sp(BEGIN(header.nVersion), BEGIN(hash), scratchpad);    
                });
            }
            workerThreads->join();
            LogPrintf("total [%.2f micros] per hash [%.2f micros]\n\n", (GetTimeMicros() - nStart), ((GetTimeMicros() - nStart)) / (double)numHashes);
        }
        
        LogPrintf("SIGMA=============================================================\n\n");
        {
            sigma_context sigmaContext(defaultSigmaSettings, std::min(memAllowKb, defaultSigmaSettings.arenaSizeKb), numThreads);
            if (!sigmaContext.arenaIsValid())
            {
                LogPrintf("Failed to allocate arena memory, try again with lower memory settings.\n");
                exit(EXIT_FAILURE);
            }
            
            #if defined(ARCH_CPU_X86_FAMILY) || defined(ARCH_CPU_ARM_FAMILY)
            {
                LogPrintf("Bench fast hashes optimised [single thread]:\n");
                uint8_t hashData1[80];
                for (int i=0;i<80;++i)
                {
                    hashData1[i] = rand();
                }
                uint8_t hashData2[32];
                for (int i=0;i<32;++i)
                {
                    hashData2[i] = rand();
                }
                std::vector<unsigned char> hashData3(defaultSigmaSettings.fastHashSizeBytes);
                for (uint64_t i=0;i<defaultSigmaSettings.fastHashSizeBytes;++i)
                {
                    hashData3[i] = rand();
                }
                uint64_t nStart = GetTimeMicros();
                uint64_t numFastHashes = 20000;
                sigmaContext.benchmarkFastHashes(hashData1, hashData2, &hashData3[0], numFastHashes);
                LogPrintf("total [%.2f micros] per hash [%.4f micros]\n\n", (GetTimeMicros() - nStart), ((GetTimeMicros() - nStart)) / (double)numFastHashes);
            }
            #endif
            {
                LogPrintf("Bench fast hashes reference [single thread]:\n");
                uint8_t hashData1[80];
                for (int i=0;i<80;++i)
                {
                    hashData1[i] = rand();
                }
                uint8_t hashData2[32];
                for (int i=0;i<32;++i)
                {
                    hashData2[i] = rand();
                }
                std::vector<unsigned char> hashData3(defaultSigmaSettings.fastHashSizeBytes);
                for (uint64_t i=0;i<defaultSigmaSettings.fastHashSizeBytes;++i)
                {
                    hashData3[i] = rand();
                }
                uint64_t nStart = GetTimeMicros();
                uint64_t numFastHashes = 20000;
                sigmaContext.benchmarkFastHashesRef(hashData1, hashData2, &hashData3[0], numFastHashes);
                LogPrintf("total [%.2f micros] per hash [%.4f micros]\n\n", (GetTimeMicros() - nStart), ((GetTimeMicros() - nStart)) / (double)numFastHashes);
            }
            
            {
                LogPrintf("Bench slow hashes [single thread]:\n");
                uint8_t hashData[80];
                for (int i=0;i<80;++i)
                {
                    hashData[i] = rand();
                }
                    
                uint64_t nStart = GetTimeMicros();
                uint64_t numSlowHashes = 100;
                sigmaContext.benchmarkSlowHashes(hashData, numSlowHashes);
                LogPrintf("total [%.2f micros] per hash [%.2f micros]\n\n", (GetTimeMicros() - nStart), ((GetTimeMicros() - nStart)) / (double)numSlowHashes);
            }
            
            {
                LogPrintf("Bench global arena priming [cpu_cost %drounds] [mem_cost %dgb]:\n", defaultSigmaSettings.argonArenaRoundCost, defaultSigmaSettings.argonMemoryCostKb/1024/1024 );
                uint64_t nStart = GetTimeMicros(); 
                uint64_t numArenas=4;
                for (uint64_t i=0; i<numArenas; ++i)
                {
                    sigmaContext.prepareArenas(header);
                }
                LogPrintf("total [%.2f micros] per round: [%.2f micros]\n\n", (GetTimeMicros() - nStart), ((GetTimeMicros() - nStart)) / (double)numArenas);
            }  
        }
        {
            {
                sigma_verify_context verify(defaultSigmaSettings, numUserVerifyThreads);
                LogPrintf("Bench verify [single thread]\n");
                uint64_t nVerifyNumber=100;
                uint64_t nCountValid=0;
                uint64_t nStart = GetTimeMicros();
                for (uint64_t i =0; i< nVerifyNumber; ++i)
                {
                    header.nNonce = rand();
                    nStart = GetTimeMicros();
                    // Count and log number of successes to avoid possibility of compiler optimising the call out.
                    if (verify.verifyHeader(header))
                    {
                        ++nCountValid;
                    }
                }
                LogPrintf("total [%.2f micros] per verification [%.2f micros] found [%d] valid random hashes\n\n", (GetTimeMicros() - nStart), ((GetTimeMicros() - nStart)) / (double)nVerifyNumber, nCountValid);
            }
        }
    }
    
    uint64_t nArenaSetuptime=0;
    uint64_t nMineStart = GetTimeMicros();
    {
        if (mineOnly)
        {
            LogPrintf("SIGMA=============================================================\n\n");
        }
        header.nTime = GetTime();
        header.nVersion = rand();
        header.nBits = arith_uint256((~arith_uint256(0) >> 10)).GetCompact();
        
        std::vector<sigma_context*> sigmaContexts;
        std::vector<uint64_t> sigmaMemorySizes;
        uint64_t nMemoryAllocatedKb=0;
        
        while (nMemoryAllocatedKb < memAllowKb)
        {
            uint64_t nMemoryChunkKb = std::min((memAllowKb-nMemoryAllocatedKb), defaultSigmaSettings.arenaSizeKb);
            nMemoryAllocatedKb += nMemoryChunkKb;
            sigmaMemorySizes.emplace_back(nMemoryChunkKb);
        }
        for (auto instanceMemorySizeKb : sigmaMemorySizes)
        {
            sigmaContexts.push_back(new sigma_context(defaultSigmaSettings, instanceMemorySizeKb, numThreads/sigmaMemorySizes.size()));
        }
        
        LogPrintf("Bench mining for low difficulty target\n");
        uint64_t nStart = GetTimeMicros();
        std::atomic<uint64_t> slowHashCounter = 0;
        std::atomic<uint64_t> halfHashCounter = 0;
        std::atomic<uint64_t> skippedHashCounter = 0;
        std::atomic<uint64_t> hashCounter = 0;
        std::atomic<uint64_t> blockCounter = 0;
        {
            auto workerThreads = new boost::asio::thread_pool(numThreads);
            for (auto sigmaContext : sigmaContexts)
            {
                boost::asio::post(*workerThreads, [&, header, sigmaContext]() mutable
                {
                    sigmaContext->prepareArenas(header);
                });
            }
            workerThreads->join();
        }
        double nHalfHashAverage=0;
        nArenaSetuptime = (GetTimeMicros() - nStart);
        LogPrintf("Arena setup time [%.2f micros]\n", nArenaSetuptime);
        nStart = GetTimeMicros();
        {
            auto workerThreads = new boost::asio::thread_pool(numThreads);
            for (auto sigmaContext : sigmaContexts)
            {
                boost::asio::post(*workerThreads, [&, header, sigmaContext]() mutable
                {
                    sigmaContext->benchmarkMining(header, slowHashCounter, halfHashCounter, skippedHashCounter, hashCounter, blockCounter, numFullHashesTarget);
                });
            }
            workerThreads->join();
        }
        nHalfHashAverage = ((GetTimeMicros() - nStart)) / (double)halfHashCounter;
        LogPrintf("slow-hashes [%d] half-hashes[%d] skipped-hashes [%d] full-hashes [%d] blocks [%d] total [%.2f micros] per half-hash[%.2f micros] per hash [%.2f micros]\n\n", slowHashCounter, halfHashCounter, skippedHashCounter, hashCounter, blockCounter, (GetTimeMicros() - nStart), nHalfHashAverage, ((GetTimeMicros() - nStart)) / (double)hashCounter);
        
        //Extrapolate sustained hashing speed for various time intervals
        double nSustainedHashesPerSecond30s  = calculateSustainedHashrateForTimePeriod(defaultSigmaSettings.numHashesPre, defaultSigmaSettings.numHashesPost, nHalfHashAverage, nArenaSetuptime, 30);
        double nSustainedHashesPerSecond60s  = calculateSustainedHashrateForTimePeriod(defaultSigmaSettings.numHashesPre, defaultSigmaSettings.numHashesPost, nHalfHashAverage, nArenaSetuptime, 60);
        double nSustainedHashesPerSecond120s = calculateSustainedHashrateForTimePeriod(defaultSigmaSettings.numHashesPre, defaultSigmaSettings.numHashesPost, nHalfHashAverage, nArenaSetuptime, 120);
        double nSustainedHashesPerSecond     = calculateSustainedHashrateForTimePeriod(defaultSigmaSettings.numHashesPre, defaultSigmaSettings.numHashesPost, nHalfHashAverage, nArenaSetuptime, 150);
        double nSustainedHashesPerSecond240s = calculateSustainedHashrateForTimePeriod(defaultSigmaSettings.numHashesPre, defaultSigmaSettings.numHashesPost, nHalfHashAverage, nArenaSetuptime, 240);
        double nSustainedHashesPerSecond480s = calculateSustainedHashrateForTimePeriod(defaultSigmaSettings.numHashesPre, defaultSigmaSettings.numHashesPost, nHalfHashAverage, nArenaSetuptime, 480);
        
        // Convert to the largest unit we can so that output is easier to read
        std::string labelSustained = " h";        
        selectLargesHashUnit(nSustainedHashesPerSecond, labelSustained);
        std::string labelSustained30s = " h";        
        selectLargesHashUnit(nSustainedHashesPerSecond30s, labelSustained30s);
        std::string labelSustained60s = " h";        
        selectLargesHashUnit(nSustainedHashesPerSecond60s, labelSustained60s);
        std::string labelSustained120s = " h";        
        selectLargesHashUnit(nSustainedHashesPerSecond120s, labelSustained120s);
        std::string labelSustained240s = " h";        
        selectLargesHashUnit(nSustainedHashesPerSecond240s, labelSustained240s);
        std::string labelSustained480s = " h";        
        selectLargesHashUnit(nSustainedHashesPerSecond480s, labelSustained480s);
        
        // Log extrapolated speeds
        LogPrintf("Extrapolate sustained hashrates for block timings\n");
        LogPrintf("Estimated 30s hashrate %.2f %s/s\n", nSustainedHashesPerSecond30s, labelSustained30s);
        LogPrintf("Estimated 1m hashrate  %.2f %s/s\n", nSustainedHashesPerSecond60s, labelSustained60s);
        LogPrintf("Estimated 2m hashrate  %.2f %s/s\n", nSustainedHashesPerSecond120s, labelSustained120s);
        LogPrintf("Estimated 4m hashrate  %.2f %s/s\n", nSustainedHashesPerSecond240s, labelSustained240s);
        LogPrintf("Estimated 8m hashrate  %.2f %s/s\n", nSustainedHashesPerSecond480s, labelSustained480s);
        
        // Log a highly noticeable number for users who just want a number to compare without all the gritty details.
        LogPrintf("\n===========================================================");
        LogPrintf("\n* Estimated continuous sustained hashrate %10.2f %s/s *", nSustainedHashesPerSecond, labelSustained);
        LogPrintf("\n===========================================================\n");
    }
    
    uint64_t nMineEnd = GetTimeMicros();
    LogPrintf("\nBenchmarks finished in [%d seconds]\n", (nMineEnd-nMineStart)*0.000001);
    
    if ((nMineEnd-nMineStart)*0.000001<30)
    {
        // Calculate hash target to spend 40 seconds running and suggest user set that.
        // NB! We delibritely test for 30 but calculate on 40 to prevent making people run the program multiple times unnecessarily.
        uint64_t nTimeSpentMining = (nMineEnd - (nArenaSetuptime+nMineStart));
        double nMultiplier = ((40*1000000) / nTimeSpentMining);
        
        LogPrintf("Mining benchmark too fast to be accurate recommend running with `--mine-num-hashes=%d` or larger for at least 30 seconds of benchmarking.\n", (uint64_t)(numFullHashesTarget*nMultiplier));
    }
    //NB! We leak sigmaContexts here, we don't really care because this is a trivial benchmark program its faster for the user to just exit than to actually free them.
}

//fixme: (HIGH)
//Super gross workaround - for some reason our macos build keeps failing to provide '___cpu_model' symbol, so we just define it ourselves as a workaround until we can fix the issue.
#ifdef MAC_OSX
#include "llvm-cpumodel-hack.cpp"
#endif
