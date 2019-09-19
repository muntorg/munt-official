// Copyright (c) 2019 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#include "util.h"
#include <crypto/hash/sigma/sigma.h>
#include <iostream>
#include <boost/program_options.hpp>
#include <thread>

#include <crypto/hash/shavite3_256/shavite3_256_opt.h>
#include <crypto/hash/shavite3_256/ref/shavite3_ref.h>
#include <crypto/hash/echo256/sphlib/sph_echo.h>
#include <crypto/hash/echo256/echo256_opt.h>
#include <crypto/hash/sigma/argon_echo/argon_echo.h>

#include <cryptopp/config.h>
#include <cryptopp/aes.h>
#include <cryptopp/modes.h>

// SIGMA paramaters (centrally set by network)
uint64_t arenaCpuCostRounds = 8;
uint64_t slowHashCpuCostRounds = 12;
uint64_t memCostGb = 4;   
uint64_t slowHashMemCostMb = 16;
uint64_t fastHashMemCostBytes = 300;
uint64_t maxHashesPre = 65536;
uint64_t maxHashesPost = 65536;
uint64_t numSigmaVerifyThreads = 4;
bool defaultSigma = true;

// User paramaters (adjustable on individual machines)
uint64_t numThreads = std::thread::hardware_concurrency();
uint64_t memAllowGb = memCostGb;
uint64_t numUserVerifyThreads = std::min(numSigmaVerifyThreads, (uint64_t)std::thread::hardware_concurrency());
uint64_t numFullHashesTarget = 50000;
bool mineOnly=false;
    
using namespace boost::program_options;
int LogPrintStr(const std::string &str)
{
    std::cout << str;
    return 1;
}

void selectLargesHashUnit(double& nSustainedHashesPerSecond, std::string& label)
{
    if (nSustainedHashesPerSecond > 1000)
    {
        label = "Kh";
        nSustainedHashesPerSecond /= 1000.0;
    }
    if (nSustainedHashesPerSecond > 1000)
    {
        label = "Mh";
        nSustainedHashesPerSecond /= 1000.0;
    }
    if (nSustainedHashesPerSecond > 1000)
    {
        label = "Gh";
        nSustainedHashesPerSecond /= 1000.0;
    }
    if (nSustainedHashesPerSecond > 1000)
    {
        label = "Th";
        nSustainedHashesPerSecond /= 1000.0;
    }
    if (nSustainedHashesPerSecond > 1000)
    {
        label = "Ph";
        nSustainedHashesPerSecond /= 1000.0;
    }
    if (nSustainedHashesPerSecond > 1000)
    {
        label = "Eh";
        nSustainedHashesPerSecond /= 1000.0;
    }
    if (nSustainedHashesPerSecond > 1000)
    {
        label = "Zh";
        nSustainedHashesPerSecond /= 1000.0;
    }
    if (nSustainedHashesPerSecond > 1000)
    {
        label = "yh";
        nSustainedHashesPerSecond /= 1000.0;
    }
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


std::vector<std::pair<std::string, uint64_t> >
validHeaderTestVectorIn = {
    {"e346fe62000000004080b46f23c5d0cdb2b54bcf7018935a3717000000000000000000000000000000ce7cb9f3053879602b3e592db673d63aa678000000000000000000fb43835dffff3f1fdb004a9f", 1872090435},
    {"9d54723a00000000c0ce73c1404f01e9467ae665575e98bfde100200000000000000000000000000b2cb01383f7bc2efae41af524342fd2726e0040000000000000000009445835dffff3f1fb300d58d", 735962043},
    {"5c9192160000000090693022fe6c8793d7ff42620abc5baf0eee6900000000000000000000000000c0f73c897b9f1143ad51115e6b92a594d8d00d000000000000000000aa45835dffff3f1f6000cdf6", 106466628},
    {"af5c892d00000000441c66cd50e0bff46791aef0cd45510cc3031f0000000000000000000000000000d1a45e9bf88e76249026fcbcbfd2bbcee302000000000000000000bc45835dffff3f1f680086b1", 991073506},
    {"58359a2a00000000a01b5eceae5be377509bce63063042b9adb30000000000000000000000000000f0a714e69d3d9a00e787a025482f3ab4c7ce1f000000000000000000ce45835dffff3f1fee006814", 628156664},
};
void testValidateValidHeaders(sigma_settings settings, uint64_t& nTestFailCount)
{
    CBlockHeader header;
    for (const auto& [hash, height] : validHeaderTestVectorIn)
    {
        std::vector<unsigned char> data = ParseHex(hash);
        memcpy(&header.nVersion, &data[0], 80);
        sigma_verify_context verify(settings, numUserVerifyThreads);
        if (verify.verifyHeader(header, height))
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

void testValidateInvalidHeaders(sigma_settings settings, uint64_t& nTestFailCount)
{
    CBlockHeader header;
    for (const auto& [hash, height] : validHeaderTestVectorIn)
    {
        std::vector<unsigned char> data = ParseHex(hash);
        memcpy(&header.nVersion, &data[0], 80);
        sigma_verify_context verify(settings, numUserVerifyThreads);
        if (!verify.verifyHeader(header, height+1))
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
    
HashReturn (*selected_echo256_opt_Init)(echo256_opt_hashState* state);
HashReturn (*selected_echo256_opt_Update)(echo256_opt_hashState* state, const unsigned char* data, uint64_t databitlen);
HashReturn (*selected_echo256_opt_Final)(echo256_opt_hashState* state, unsigned char* hashval);
HashReturn (*selected_echo256_opt_UpdateFinal)(echo256_opt_hashState* state, unsigned char* hashval, const unsigned char* data, uint64_t databitlen);

bool (*selected_shavite3_256_opt_Init)(shavite3_256_opt_hashState* state);
bool (*selected_shavite3_256_opt_Update)(shavite3_256_opt_hashState* state, const unsigned char* data, uint64_t dataLenBytes);
bool (*selected_shavite3_256_opt_Final)(shavite3_256_opt_hashState* state, unsigned char* hashval);

int (*selected_argon2_echo_hash)(argon2_echo_context* context, bool doHash);

uint64_t nNumShaviteTrials=100;
uint64_t nNumEchoTrials=100;
uint64_t nNumArgonTrials=10;
#define SELECT_OPTIMISED_SHAVITE(CPU, IDX) \
{\
    uint64_t nStart = GetTimeMicros();\
    shavite3_256_opt_##CPU##_Init(&ctx_shavite);\
    for (uint64_t i=0;i<nNumShaviteTrials;++i)\
    {\
        shavite3_256_opt_##CPU##_Update(&ctx_shavite, (uint8_t*)&data[0], data.size());\
    }\
    shavite3_256_opt_##CPU##_Final(&ctx_shavite, (uint8_t*)&outHash[0]);\
    uint64_t nTime = GetTimeMicros() - nStart;\
    if (nTime < nBestTimeShavite)\
    {\
        selected_shavite3_256_opt_Init   = shavite3_256_opt_##CPU##_Init;\
        selected_shavite3_256_opt_Update = shavite3_256_opt_##CPU##_Update;\
        selected_shavite3_256_opt_Final  = shavite3_256_opt_##CPU##_Final;\
        nBestTimeShavite=nTime;\
        nSelShavite=IDX;\
    }\
}
#define SELECT_OPTIMISED_ECHO(CPU, IDX) \
{\
    uint64_t nStart = GetTimeMicros();\
    echo256_opt_##CPU##_Init(&ctx_echo);\
    for (uint64_t i=0;i<nNumEchoTrials;++i)\
    {\
        echo256_opt_##CPU##_Update(&ctx_echo, (uint8_t*)&data[0], data.size());\
    }\
    echo256_opt_##CPU##_Final(&ctx_echo, (uint8_t*)&outHash[0]);\
    uint64_t nTime = GetTimeMicros() - nStart;\
    if (nTime < nBestTimeEcho)\
    {\
        selected_echo256_opt_Init        = echo256_opt_##CPU##_Init;\
        selected_echo256_opt_Update      = echo256_opt_##CPU##_Update;\
        selected_echo256_opt_Final       = echo256_opt_##CPU##_Final;\
        selected_echo256_opt_UpdateFinal = echo256_opt_##CPU##_UpdateFinal;\
        nBestTimeEcho=nTime;\
        nSelEcho=IDX;\
    }\
}      
#define SELECT_OPTIMISED_ARGON(CPU, IDX) \
{\
    uint8_t argonScratch[1024];\
    argon2_echo_context context;\
    context.t_cost = 5;\
    context.m_cost = 1;\
    context.allocated_memory = argonScratch;\
    context.pwd = (uint8_t*)&data[0];\
    context.pwdlen = data.size();\
    context.lanes = 4;\
    context.threads = 1;\
    uint64_t nStart = GetTimeMicros();\
    for (uint64_t i=0;i<nNumArgonTrials;++i)\
    {\
        argon2_echo_ctx_##CPU(&context, true);\
    }\
    uint64_t nTime = GetTimeMicros() - nStart;\
    if (nTime < nBestTimeArgon)\
    {\
        selected_argon2_echo_hash = argon2_echo_ctx_##CPU;\
        nBestTimeArgon=nTime;\
        nSelArgon=IDX;\
    }\
}

#ifdef ARCH_CPU_X86_FAMILY
void LogSelection(uint64_t nSel, std::string sAlgoName)
{
    switch (nSel)
    {
        case 0:
            LogPrintf("[%d] Selected reference implementation as fastest\n", sAlgoName); break;
        case 1:
            LogPrintf("[%d] Selected avx512f-aes as fastest\n", sAlgoName); break;
        case 2:
            LogPrintf("[%d] Selected avx2-aes as fastest\n", sAlgoName); break;
        case 3:
            LogPrintf("[%d] Selected avx-aes as fastest\n", sAlgoName); break;
        case 4:
            LogPrintf("[%d] Selected sse4-aes as fastest\n", sAlgoName); break;
        case 5:
            LogPrintf("[%d] Selected sse3-aes as fastest\n", sAlgoName); break;
        case 6:
            LogPrintf("[%d] Selected sse2-aes as fastest\n", sAlgoName); break;
        case 7:
            LogPrintf("[%d] Selected avx512f as fastest\n", sAlgoName); break;
        case 8:
            LogPrintf("[%d] Selected avx2 as fastest\n", sAlgoName); break;
        case 9:
            LogPrintf("[%d] Selected avx as fastest\n", sAlgoName); break;
        case 10:
            LogPrintf("[%d] Selected sse4 as fastest\n", sAlgoName); break;
        case 11:
            LogPrintf("[%d] Selected sse3 as fastest\n", sAlgoName); break;
        case 12:
            LogPrintf("[%d] Selected sse2 as fastest\n", sAlgoName); break;
        case 9999:
            LogPrintf("[%d] Selected hybrid implementation as fastest\n", sAlgoName); break;
    }
}
#endif

#ifdef ARCH_CPU_ARM_FAMILY
void LogSelection(uint64_t nSel, std::string sAlgoName)
{
    switch (nSel)
    {
        case 0:
            LogPrintf("[%d] Selected reference implementation as fastest (no NEON support)\n", sAlgoName); break;
        case 1:
            LogPrintf("[%d] Running with Cortex-A53 optimised NEON support (no hardware AES)\n", sAlgoName); break;
        case 2:
            LogPrintf("[%d] Running with Cortex-A57 optimised NEON support (no hardware AES)\n", sAlgoName); break;
        case 3:
            LogPrintf("[%d] Running with Cortex-A72 optimised NEON support (no hardware AES)\n", sAlgoName); break;
        case 4:
            LogPrintf("[%d] Running with Cortex-A53 optimised NEON+AES support\n", sAlgoName); break;
        case 5:
            LogPrintf("[%d] Running with Cortex-A57 optimised NEON+AES support\n", sAlgoName); break;
        case 6:
            LogPrintf("[%d] Running with Cortex-A72 optimised NEON+AES support\n", sAlgoName); break;
        case 7:
            LogPrintf("[%d] Running with Thunderx optimised NEON+AES support\n", sAlgoName); break;
        case 9999:
            LogPrintf("[%d] Running in hybrid mode.\n", sAlgoName); break;
    }
}
#include <sys/auxv.h>
#endif

void selectOptimisedImplementations()
{    
    std::string data = "zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz";
    std::vector<unsigned char> outHash(32);
    shavite3_256_opt_hashState ctx_shavite;
    echo256_opt_hashState ctx_echo;

    uint64_t nBestTimeShavite = std::numeric_limits<uint64_t>::max();
    uint64_t nBestTimeEcho = std::numeric_limits<uint64_t>::max();
    uint64_t nBestTimeArgon = std::numeric_limits<uint64_t>::max();
    uint64_t nSelShavite=0;
    uint64_t nSelEcho=0;
    uint64_t nSelArgon=0;
    
    {
        uint64_t nStart = GetTimeMicros();
        shavite3_ref_hashState ctx_shavite_ref;
        shavite3_ref_Init(&ctx_shavite_ref);
        for (uint64_t i=0;i<nNumShaviteTrials;++i)
        {
            shavite3_ref_Update(&ctx_shavite_ref, (uint8_t*)&data[0], data.size());
        }
        shavite3_ref_Final(&ctx_shavite_ref, (uint8_t*)&outHash[0]);
        nBestTimeShavite = GetTimeMicros() - nStart;
    }
    {
        uint64_t nStart = GetTimeMicros();
        sph_echo256_context ctx_echo_ref;
        sph_echo256_init(&ctx_echo_ref);
        for (uint64_t i=0;i<nNumEchoTrials;++i)
        {
            sph_echo256(&ctx_echo_ref, (uint8_t*)&data[0], data.size());
        }
        sph_echo256_close(&ctx_echo_ref, (uint8_t*)&outHash[0]);
        nBestTimeEcho = GetTimeMicros() - nStart;
    }
    {
        uint8_t argonScratch[1024];
        argon2_echo_context context;
        context.t_cost = 5;
        context.m_cost = 1;
        context.allocated_memory = argonScratch;
        context.pwd = (uint8_t*)&data[0];
        context.pwdlen = data.size();
        context.lanes = 4;
        context.threads = 1;
        
        uint64_t nStart = GetTimeMicros();       
        for (uint64_t i=0;i<nNumArgonTrials;++i)
        {
            argon2_echo_ctx_ref(&context, true);
        }
        nBestTimeArgon = GetTimeMicros() - nStart;
    }
  
    #ifdef ARCH_CPU_X86_FAMILY
    {
        #if defined(COMPILER_HAS_AES)
        if (__builtin_cpu_supports("aes"))
        {
            #if defined(COMPILER_HAS_AVX512F)
            if (__builtin_cpu_supports("avx512f"))
            {
                SELECT_OPTIMISED_SHAVITE(avx512f_aes, 1);
                SELECT_OPTIMISED_ECHO   (avx512f_aes, 1);
                SELECT_OPTIMISED_ARGON  (avx512f_aes, 1);
            }
            #endif
            #if defined(COMPILER_HAS_AVX2)
            if (__builtin_cpu_supports("avx2"))
            {
                SELECT_OPTIMISED_SHAVITE(avx2_aes, 2);
                SELECT_OPTIMISED_ECHO   (avx2_aes, 2);
                SELECT_OPTIMISED_ARGON  (avx2_aes, 2);
            }
            #endif
            #if defined(COMPILER_HAS_AVX)
            if (__builtin_cpu_supports("avx"))
            {
                SELECT_OPTIMISED_SHAVITE(avx_aes, 3);
                SELECT_OPTIMISED_ECHO   (avx_aes, 3);
                SELECT_OPTIMISED_ARGON  (avx_aes, 3);
            }
            #endif
            #if defined(COMPILER_HAS_SSE4)
            if (__builtin_cpu_supports("sse4.2"))
            {
                SELECT_OPTIMISED_SHAVITE(sse4_aes, 4);
                SELECT_OPTIMISED_ECHO   (sse4_aes, 4);
                SELECT_OPTIMISED_ARGON  (sse4_aes, 4);
            }
            #endif
            #if defined(COMPILER_HAS_SSE3)
            if (__builtin_cpu_supports("sse3"))
            {
                SELECT_OPTIMISED_SHAVITE(sse3_aes, 5);
                SELECT_OPTIMISED_ECHO   (sse3_aes, 5);
                SELECT_OPTIMISED_ARGON  (sse3_aes, 5);
            }
            #endif
            #if defined(COMPILER_HAS_SSE2)
            #if 0
            //fixme: (SIGMA)
            if (__builtin_cpu_supports("sse2"))
            {
                SELECT_OPTIMISED_SHAVITE(sse2_aes, 6);
                SELECT_OPTIMISED_ECHO   (sse2_aes, 6);
                SELECT_OPTIMISED_ARGON  (sse2_aes, 6);
            }
            #endif
            #endif
        }
        else
        #endif
        {
            #if defined(COMPILER_HAS_AVX512F)
            if (__builtin_cpu_supports("avx512f"))
            {
                SELECT_OPTIMISED_SHAVITE(avx512f, 7);
                SELECT_OPTIMISED_ECHO   (avx512f, 7);
                SELECT_OPTIMISED_ARGON  (avx512f, 7);
            }
            #endif
            #if defined(COMPILER_HAS_AVX2)
            if (__builtin_cpu_supports("avx2"))
            {
                SELECT_OPTIMISED_SHAVITE(avx2, 8);
                SELECT_OPTIMISED_ECHO   (avx2, 8);
                SELECT_OPTIMISED_ARGON  (avx2, 8);
            }
            #endif
            #if defined(COMPILER_HAS_AVX)
            if (__builtin_cpu_supports("avx"))
            {
                SELECT_OPTIMISED_SHAVITE(avx, 9);
                SELECT_OPTIMISED_ECHO   (avx, 9);
                SELECT_OPTIMISED_ARGON  (avx, 9);
            }
            #endif
            #if defined(COMPILER_HAS_SSE4)
            if (__builtin_cpu_supports("sse4.2"))
            {
                SELECT_OPTIMISED_SHAVITE(sse4, 10);
                SELECT_OPTIMISED_ECHO   (sse4, 10);
                SELECT_OPTIMISED_ARGON  (sse4, 10);
            }
            #endif
            #if defined(COMPILER_HAS_SSE3)
            if (__builtin_cpu_supports("sse3"))
            {
                SELECT_OPTIMISED_SHAVITE(sse3, 11);
                SELECT_OPTIMISED_ECHO   (sse3, 11);
                SELECT_OPTIMISED_ARGON  (sse3, 11);
            }
            #endif
            #if defined(COMPILER_HAS_SSE2)
            #if 0
            //fixme: (SIGMA)
            else if (__builtin_cpu_supports("sse2"))
            {
                SELECT_OPTIMISED_SHAVITE(sse2, 12);
                SELECT_OPTIMISED_ECHO   (sse2, 12);
                SELECT_OPTIMISED_ARGON  (sse2, 12);
            }
            #endif
            #endif
        }
    }
    #elif defined (ARCH_CPU_ARM_FAMILY)
    {
        bool haveAES=false;
        #if defined HWCAP_AES
        long hwcaps2 = getauxval(AT_HWCAP);
        if(hwcaps2 & HWCAP_AES)
        {
            haveAES=true;
        }
        #elif defined HWCAP2_AES
        long hwcaps2 = getauxval(AT_HWCAP2);
        if(hwcaps2 & HWCAP2_AES)
        {
            haveAES=true;
        }
        #endif
        #ifdef COMPILER_HAS_CORTEX53
        SELECT_OPTIMISED_SHAVITE(arm_cortex_a53, 1);
        SELECT_OPTIMISED_ECHO(arm_cortex_a53, 1);
        SELECT_OPTIMISED_ARGON(arm_cortex_a53, 1);
        #endif
        #ifdef COMPILER_HAS_CORTEX57
        SELECT_OPTIMISED_SHAVITE(arm_cortex_a57, 2);
        SELECT_OPTIMISED_ECHO(arm_cortex_a57, 2);
        SELECT_OPTIMISED_ARGON(arm_cortex_a57, 2);
        #endif
        #ifdef COMPILER_HAS_CORTEX72
        SELECT_OPTIMISED_SHAVITE(arm_cortex_a72, 3);
        SELECT_OPTIMISED_ECHO(arm_cortex_a72, 3);
        SELECT_OPTIMISED_ARGON(arm_cortex_a72, 3);
        #endif
        if (haveAES)
        {
            #ifdef COMPILER_HAS_CORTEX53_AES
            SELECT_OPTIMISED_SHAVITE(arm_cortex_a53_aes, 4);
            SELECT_OPTIMISED_ECHO(arm_cortex_a53_aes, 4);
            SELECT_OPTIMISED_ARGON(arm_cortex_a53_aes, 4);
            #endif
            #ifdef COMPILER_HAS_CORTEX57_AES
            SELECT_OPTIMISED_SHAVITE(arm_cortex_a57_aes, 5);
            SELECT_OPTIMISED_ECHO(arm_cortex_a57_aes, 5);
            SELECT_OPTIMISED_ARGON(arm_cortex_a57_aes, 5);
            #endif
            #ifdef COMPILER_HAS_CORTEX72_AES
            SELECT_OPTIMISED_SHAVITE(arm_cortex_a72_aes, 6);
            SELECT_OPTIMISED_ECHO(arm_cortex_a72_aes, 6);
            SELECT_OPTIMISED_ARGON(arm_cortex_a72_aes, 6);
            #endif
            #ifdef COMPILER_HAS_THUNDERX_AES
            SELECT_OPTIMISED_SHAVITE(arm_thunderx_aes, 7);
            SELECT_OPTIMISED_ECHO(arm_thunderx_aes, 7);
            SELECT_OPTIMISED_ARGON(arm_thunderx_aes, 7);
            #endif
        }
    }
    #endif
    
    // Finally (only after we have fastest echo implementation) give the hybrid echo a go
    // Just in case it happens to be faster.
    SELECT_OPTIMISED_ARGON(hybrid, 9999);
    
    LogSelection(nSelShavite, "shavite");
    LogSelection(nSelEcho, "echo");
    LogSelection(nSelArgon, "argon");
}


int main(int argc, char** argv)
{
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
        memCostGb = vm["sigma-global-mem"].as<int64_t>();
        defaultSigma = false;
    }
    memAllowGb = memCostGb;
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
        maxHashesPre = vm["sigma-num-slow"].as<int64_t>();
    }
    if (vm.count("sigma-slowhash-mem"))
    {
        slowHashMemCostMb = vm["sigma-slowhash-mem"].as<int64_t>();
        defaultSigma = false;
    }
    if (vm.count("sigma-arena-cpucost"))
    {
        arenaCpuCostRounds = vm["sigma-arena-cpucost"].as<int64_t>();
        defaultSigma = false;
    }
    if (vm.count("sigma-slowhash-cpucost"))
    {
        slowHashCpuCostRounds = vm["sigma-slowhash-cpucost"].as<int64_t>();
        defaultSigma = false;
    }
    if (vm.count("sigma-num-fast"))
    {
        maxHashesPost = vm["sigma-num-fast"].as<int64_t>();
        defaultSigma = false;
    }
    if (vm.count("sigma-fasthash-mem"))
    {
        fastHashMemCostBytes = vm["sigma-fasthash-mem"].as<int64_t>();
        defaultSigma = false;
    }
    if (vm.count("sigma-verify-threads"))
    {
        numSigmaVerifyThreads = vm["sigma-verify-threads"].as<int64_t>();
        defaultSigma = false;
    }
    
    
    if (numUserVerifyThreads > numSigmaVerifyThreads)
    {
        LogPrintf("Number of user verify threads may not exceed number of sigma verify threads");
        return 1;
    }
    
    LogPrintf("Configuration=====================================================\n\n");
    LogPrintf("NETWORK:\nGlobal memory cost [%dgb]\nArgon_echo cpu cost for arenas [%d rounds]\nArgon_echo cpu cost for slow hash [%d rounds]\nArgon_echo mem cost [%dMb]\nEcho/Shavite digest size [%d bytes]\nNumber of fast hashes per slow hash [%d]\nNumber of slow hashes per global arena [%d]\nNumber of verify threads [%d]\n\n", memCostGb, arenaCpuCostRounds ,slowHashCpuCostRounds, slowHashMemCostMb, fastHashMemCostBytes, maxHashesPost, maxHashesPre, numSigmaVerifyThreads);
    LogPrintf("USER:\nMining with [%d] threads\nMining with [%d gb] memory.\nVerifying with [%s] threads.\n\n", numThreads, memAllowGb, numUserVerifyThreads);
    
    uint64_t memAllowKb = memAllowGb*1024*1024;
    if (memAllowKb == 0)
    {
        memAllowKb = 512*1024;
    }
    
    sigma_settings sigmaSettings(arenaCpuCostRounds, slowHashCpuCostRounds, 1024*slowHashMemCostMb, 1024*1024*memCostGb, maxHashesPre, maxHashesPost, numSigmaVerifyThreads, fastHashMemCostBytes);
    
    // If we are using the default params then perform some tests to ensure everything runs the same across different machines
    if (defaultSigma)
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
        testValidateValidHeaders(sigmaSettings, nTestFailCount);
        
        LogPrintf("Verify validation of invalid headers\n");
        testValidateInvalidHeaders(sigmaSettings, nTestFailCount);
        
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
    header.hashPrevBlock = ArithToUint256(((((((((arith_uint256(rand()) << 8) * rand()) << 8) * rand()) << 8) * rand()) << 8) * rand()));
    header.hashMerkleRoot = ArithToUint256(((((((((arith_uint256(rand()) << 8) * rand()) << 8) * rand()) << 8) * rand()) << 8) * rand()));
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
            sigma_context sigmaContext(sigmaSettings, std::min(memAllowKb, 1024*1024*memCostGb), numThreads);
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
                std::vector<unsigned char> hashData3(sigmaSettings.fastHashSizeBytes);
                for (uint64_t i=0;i<sigmaSettings.fastHashSizeBytes;++i)
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
                std::vector<unsigned char> hashData3(sigmaSettings.fastHashSizeBytes);
                for (uint64_t i=0;i<sigmaSettings.fastHashSizeBytes;++i)
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
                LogPrintf("Bench global arena priming [cpu_cost %drounds] [mem_cost %dgb]:\n", arenaCpuCostRounds, memCostGb );
                uint64_t nStart = GetTimeMicros(); 
                uint64_t numArenas=4;
                for (uint64_t i=0; i<numArenas; ++i)
                {
                    sigmaContext.prepareArenas(header, rand());
                }
                LogPrintf("total [%.2f micros] per round: [%.2f micros]\n\n", (GetTimeMicros() - nStart), ((GetTimeMicros() - nStart)) / (double)numArenas);
            }  
        }
        {
            {
                sigma_verify_context verify(sigmaSettings, numUserVerifyThreads);
                LogPrintf("Bench verify [single thread]\n");
                uint64_t nVerifyNumber=100;
                uint64_t nCountValid=0;
                uint64_t nStart = GetTimeMicros();
                for (uint64_t i =0; i< nVerifyNumber; ++i)
                {
                    header.nNonce = rand();
                    nStart = GetTimeMicros();
                    // Count and log number of successes to avoid possibility of compiler optimising the call out.
                    if (verify.verifyHeader(header, rand()))
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
        uint64_t nMemoryAllocated=0;
        
        while (nMemoryAllocated < memAllowKb)
        {
            uint64_t nMemoryChunk = std::min((memAllowKb-nMemoryAllocated), 1024*1024*memCostGb);
            nMemoryAllocated += nMemoryChunk;
            sigmaMemorySizes.emplace_back(nMemoryChunk);
        }
        for (auto instanceMemorySizeKb : sigmaMemorySizes)
        {
            sigmaContexts.push_back(new sigma_context(sigmaSettings, instanceMemorySizeKb, numThreads/sigmaMemorySizes.size()));
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
                    sigmaContext->prepareArenas(header, rand());
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
        double nSustainedHashesPerSecond = calculateSustainedHashrateForTimePeriod(maxHashesPre, maxHashesPost, nHalfHashAverage, nArenaSetuptime, 150);
        double nSustainedHashesPerSecond30s = calculateSustainedHashrateForTimePeriod(maxHashesPre, maxHashesPost, nHalfHashAverage, nArenaSetuptime, 30);
        double nSustainedHashesPerSecond60s = calculateSustainedHashrateForTimePeriod(maxHashesPre, maxHashesPost, nHalfHashAverage, nArenaSetuptime, 60);
        double nSustainedHashesPerSecond120s = calculateSustainedHashrateForTimePeriod(maxHashesPre, maxHashesPost, nHalfHashAverage, nArenaSetuptime, 120);
        double nSustainedHashesPerSecond240s = calculateSustainedHashrateForTimePeriod(maxHashesPre, maxHashesPost, nHalfHashAverage, nArenaSetuptime, 240);
        double nSustainedHashesPerSecond480s = calculateSustainedHashrateForTimePeriod(maxHashesPre, maxHashesPost, nHalfHashAverage, nArenaSetuptime, 480);
        
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
