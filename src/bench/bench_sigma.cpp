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

        shavite3_256_opt_hashState ctx_echo;
        selected_shavite3_256_opt_Init(&ctx_echo);
        selected_shavite3_256_opt_Update(&ctx_echo, (uint8_t*)&data[0], data.size());
        selected_shavite3_256_opt_Final(&ctx_echo, &outHash[0]);
        
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


std::vector<std::pair<std::string, uint64_t> >
validHeaderTestVectorIn = {
    {"8dd1c77100000000907417953c11ce0cae088b6dea9a2361684c850000000000000000000000000000ea096eb74fc2cd1a0774b250d4eb9bd2975b000000000000000000e6e7665dffff3f1fb702eac2", 617327633},
    {"2a625146000000008001f4d57bde264346390c9a7902a0a40ad22c0000000000000000000000000060057ee29a88958d11e092f13d34c23f68b2570000000000000000006ae8665dffff3f1f9d0021d2", 778626333},
    {"c1cdd97100000000c0ecb306c68d62f969ee328074116f41352b4600000000000000000000000000d0c6076635d1e4c55f64aae885e818a1e61e0a00000000000000000078e8665dffff3f1f7d00c58d", 74198191},
    {"c973b5650000000000c0fa4613ec715e69446d33f56cef0e0ab0010000000000000000000000000058805be9017e7c2ae31567bc5655bf0a02ac0200000000000000000085e8665dffff3f1f4500da80", 1264517732},
    {"4e71594500000000a00d0dba5ec8d5e1ca23a067160d071f3ad21400000000000000000000000000b8e5e402245035902e101060a97098b5524e2400000000000000000090e8665dffff3f1f65007b1a", 253513492},
};
void testValidateValidHeaders(uint64_t& nTestFailCount)
{
    CBlockHeader header;
    for (const auto& [hash, height] : validHeaderTestVectorIn)
    {
        std::vector<unsigned char> data = ParseHex(hash);
        memcpy(&header.nVersion, &data[0], 80);
        sigma_context sigmaContext(arenaCpuCostRounds, slowHashCpuCostRounds, 1024*slowHashMemCostMb, 1024*1024*memCostGb, 1024*1024*std::min(memAllowGb, memCostGb), maxHashesPre, maxHashesPost, numThreads, numSigmaVerifyThreads, numUserVerifyThreads, fastHashMemCostBytes);
        if (!sigmaContext.arenaIsValid())
        {
            LogPrintf("Failed to allocate arena memory, try again with lower memory settings.\n");
            exit(EXIT_FAILURE);
        }
        if (!sigmaContext.verifyHeader(header, height))
        {
            LogPrintf("✘");
            ++nTestFailCount;
        }
        LogPrintf("✔");
    }
    LogPrintf("\n");
}

void testValidateInvalidHeaders(uint64_t& nTestFailCount)
{
    CBlockHeader header;
    for (const auto& [hash, height] : validHeaderTestVectorIn)
    {
        std::vector<unsigned char> data = ParseHex(hash);
        memcpy(&header.nVersion, &data[0], 80);
        sigma_context sigmaContext(arenaCpuCostRounds, slowHashCpuCostRounds, 1024*slowHashMemCostMb, 1024*1024*memCostGb, 1024*1024*std::min(memAllowGb, memCostGb), maxHashesPre, maxHashesPost, numThreads, numSigmaVerifyThreads, numUserVerifyThreads, fastHashMemCostBytes);
        if (!sigmaContext.arenaIsValid())
        {
            LogPrintf("Failed to allocate arena memory, try again with lower memory settings.\n");
            exit(EXIT_FAILURE);
        }
        if (sigmaContext.verifyHeader(header, height+1))
        {
            LogPrintf("✘");
            ++nTestFailCount;
        }
        LogPrintf("✔");
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

#ifdef ARCH_CPU_X86_FAMILY
void selectOptimisedImplementations()
{
    #if defined(COMPILER_HAS_AES)
    if (__builtin_cpu_supports("aes"))
    {
        #if defined(COMPILER_HAS_AVX512F)
        if (__builtin_cpu_supports("avx512f"))
        {
            selected_echo256_opt_Init         =      echo256_opt_avx512f_aes_Init;
            selected_echo256_opt_Update       =      echo256_opt_avx512f_aes_Update;
            selected_echo256_opt_Final        =      echo256_opt_avx512f_aes_Final;
            selected_echo256_opt_UpdateFinal  =      echo256_opt_avx512f_aes_UpdateFinal;
            selected_shavite3_256_opt_Init    = shavite3_256_opt_avx512f_aes_Init;
            selected_shavite3_256_opt_Update  = shavite3_256_opt_avx512f_aes_Update;
            selected_shavite3_256_opt_Final   = shavite3_256_opt_avx512f_aes_Final;
            selected_argon2_echo_hash         =  argon2_echo_ctx_avx512f_aes;
        }
        #endif
        #if defined(COMPILER_HAS_AVX2)
        if (__builtin_cpu_supports("avx2"))
        {
            selected_echo256_opt_Init         =      echo256_opt_avx2_aes_Init;
            selected_echo256_opt_Update       =      echo256_opt_avx2_aes_Update;
            selected_echo256_opt_Final        =      echo256_opt_avx2_aes_Final;
            selected_echo256_opt_UpdateFinal  =      echo256_opt_avx2_aes_UpdateFinal;
            selected_shavite3_256_opt_Init    = shavite3_256_opt_avx2_aes_Init;
            selected_shavite3_256_opt_Update  = shavite3_256_opt_avx2_aes_Update;
            selected_shavite3_256_opt_Final   = shavite3_256_opt_avx2_aes_Final;
            selected_argon2_echo_hash         =  argon2_echo_ctx_avx2_aes;
            selected_argon2_echo_hash         =  argon2_echo_ctx_avx2_aes;
        }
        #endif
        #if defined(COMPILER_HAS_AVX)
        if (__builtin_cpu_supports("avx"))
        {
            selected_echo256_opt_Init         =      echo256_opt_avx_aes_Init;
            selected_echo256_opt_Update       =      echo256_opt_avx_aes_Update;
            selected_echo256_opt_Final        =      echo256_opt_avx_aes_Final;
            selected_echo256_opt_UpdateFinal  =      echo256_opt_avx_aes_UpdateFinal;
            selected_shavite3_256_opt_Init    = shavite3_256_opt_avx_aes_Init;
            selected_shavite3_256_opt_Update  = shavite3_256_opt_avx_aes_Update;
            selected_shavite3_256_opt_Final   = shavite3_256_opt_avx_aes_Final;
            selected_argon2_echo_hash         =  argon2_echo_ctx_avx_aes;
        }
        #endif
        #if defined(COMPILER_HAS_SSE4)
        if (__builtin_cpu_supports("sse4.2"))
        {
            selected_echo256_opt_Init         =      echo256_opt_sse4_aes_Init;
            selected_echo256_opt_Update       =      echo256_opt_sse4_aes_Update;
            selected_echo256_opt_Final        =      echo256_opt_sse4_aes_Final;
            selected_echo256_opt_UpdateFinal  =      echo256_opt_sse4_aes_UpdateFinal;
            selected_shavite3_256_opt_Init    = shavite3_256_opt_sse4_aes_Init;
            selected_shavite3_256_opt_Update  = shavite3_256_opt_sse4_aes_Update;
            selected_shavite3_256_opt_Final   = shavite3_256_opt_sse4_aes_Final;
            selected_argon2_echo_hash         =  argon2_echo_ctx_sse4_aes;
        }
        #endif
        #if defined(COMPILER_HAS_SSE3)
        if (__builtin_cpu_supports("sse3"))
        {
            selected_echo256_opt_Init         =      echo256_opt_sse3_aes_Init;
            selected_echo256_opt_Update       =      echo256_opt_sse3_aes_Update;
            selected_echo256_opt_Final        =      echo256_opt_sse3_aes_Final;
            selected_echo256_opt_UpdateFinal  =      echo256_opt_sse3_aes_UpdateFinal;
            selected_shavite3_256_opt_Init    = shavite3_256_opt_sse3_aes_Init;
            selected_shavite3_256_opt_Update  = shavite3_256_opt_sse3_aes_Update;
            selected_shavite3_256_opt_Final   = shavite3_256_opt_sse3_aes_Final;
            selected_argon2_echo_hash         =  argon2_echo_ctx_sse3_aes;
        }
        #endif
        #if defined(COMPILER_HAS_SSE2)
        #if 0
        //fixme: (SIGMA)
        if (__builtin_cpu_supports("sse2"))
        {
            selected_echo256_opt_Init         =      echo256_opt_sse2_aes_Init;
            selected_echo256_opt_Update       =      echo256_opt_sse2_aes_Update;
            selected_echo256_opt_Final        =      echo256_opt_sse2_aes_Final;
            selected_echo256_opt_UpdateFinal  =      echo256_opt_sse2_aes_UpdateFinal;
            selected_shavite3_256_opt_Init    = shavite3_256_opt_sse2_aes_Init;
            selected_shavite3_256_opt_Update  = shavite3_256_opt_sse2_aes_Update;
            selected_shavite3_256_opt_Final   = shavite3_256_opt_sse2_aes_Final;
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
            selected_echo256_opt_Init         =      echo256_opt_avx512f_Init;
            selected_echo256_opt_Update       =      echo256_opt_avx512f_Update;
            selected_echo256_opt_Final        =      echo256_opt_avx512f_Final;
            selected_echo256_opt_UpdateFinal  =      echo256_opt_avx512f_UpdateFinal;
            selected_shavite3_256_opt_Init    = shavite3_256_opt_avx512f_Init;
            selected_shavite3_256_opt_Update  = shavite3_256_opt_avx512f_Update;
            selected_shavite3_256_opt_Final   = shavite3_256_opt_avx512f_Final;
            selected_argon2_echo_hash         =  argon2_echo_ctx_avx512f;
        }
        #endif
        #if defined(COMPILER_HAS_AVX2)
        if (__builtin_cpu_supports("avx2"))
        {
            selected_echo256_opt_Init         =      echo256_opt_avx2_Init;
            selected_echo256_opt_Update       =      echo256_opt_avx2_Update;
            selected_echo256_opt_Final        =      echo256_opt_avx2_Final;
            selected_echo256_opt_UpdateFinal  =      echo256_opt_avx2_UpdateFinal;
            selected_shavite3_256_opt_Init    = shavite3_256_opt_avx2_Init;
            selected_shavite3_256_opt_Update  = shavite3_256_opt_avx2_Update;
            selected_shavite3_256_opt_Final   = shavite3_256_opt_avx2_Final;
            selected_argon2_echo_hash         =  argon2_echo_ctx_avx2;
        }
        #endif
        #if defined(COMPILER_HAS_AVX)
        if (__builtin_cpu_supports("avx"))
        {
            selected_echo256_opt_Init         =      echo256_opt_avx_Init;
            selected_echo256_opt_Update       =      echo256_opt_avx_Update;
            selected_echo256_opt_Final        =      echo256_opt_avx_Final;
            selected_echo256_opt_UpdateFinal  =      echo256_opt_avx_UpdateFinal;
            selected_shavite3_256_opt_Init    = shavite3_256_opt_avx_Init;
            selected_shavite3_256_opt_Update  = shavite3_256_opt_avx_Update;
            selected_shavite3_256_opt_Final   = shavite3_256_opt_avx_Final;
            selected_argon2_echo_hash         =  argon2_echo_ctx_avx;
        }
        #endif
        #if defined(COMPILER_HAS_SSE4)
        if (__builtin_cpu_supports("sse4.2"))
        {
            selected_echo256_opt_Init         =      echo256_opt_sse4_Init;
            selected_echo256_opt_Update       =      echo256_opt_sse4_Update;
            selected_echo256_opt_Final        =      echo256_opt_sse4_Final;
            selected_echo256_opt_UpdateFinal  =      echo256_opt_sse4_UpdateFinal;
            selected_shavite3_256_opt_Init    = shavite3_256_opt_sse4_Init;
            selected_shavite3_256_opt_Update  = shavite3_256_opt_sse4_Update;
            selected_shavite3_256_opt_Final   = shavite3_256_opt_sse4_Final;
            selected_argon2_echo_hash         =  argon2_echo_ctx_sse4;
        }
        #endif
        #if defined(COMPILER_HAS_SSE3)
        if (__builtin_cpu_supports("sse3"))
        {
            selected_echo256_opt_Init         =      echo256_opt_sse3_Init;
            selected_echo256_opt_Update       =      echo256_opt_sse3_Update;
            selected_echo256_opt_Final        =      echo256_opt_sse3_Final;
            selected_echo256_opt_UpdateFinal  =      echo256_opt_sse3_UpdateFinal;
            selected_shavite3_256_opt_Init    = shavite3_256_opt_sse3_Init;
            selected_shavite3_256_opt_Update  = shavite3_256_opt_sse3_Update;
            selected_shavite3_256_opt_Final   = shavite3_256_opt_sse3_Final;
            selected_argon2_echo_hash         =  argon2_echo_ctx_sse3;
        }
        #endif
        #if defined(COMPILER_HAS_SSE2)
        #if 0
        //fixme: (SIGMA)
        else if (__builtin_cpu_supports("sse2"))
        {
            selected_echo256_opt_Init         =      echo256_opt_sse2_Init;
            selected_echo256_opt_Update       =      echo256_opt_sse2_Update;
            selected_echo256_opt_Final        =      echo256_opt_sse2_Final;
            selected_echo256_opt_UpdateFinal  =      echo256_opt_sse2_UpdateFinal;
            selected_shavite3_256_opt_Init    = shavite3_256_opt_sse2_Init;
            selected_shavite3_256_opt_Update  = shavite3_256_opt_sse2_Update;
            selected_shavite3_256_opt_Final   = shavite3_256_opt_sse2_Final;
            selected_argon2_echo_hash         =  argon2_echo_ctx_sse2;
        }
        #endif
        #endif
    }
}
#endif

#ifdef ARCH_CPU_ARM_FAMILY
void selectOptimisedImplementations()
{
    bool haveAES=false;
    long hwcaps2 = getauxval(AT_HWCAP2);
    if(hwcaps2 & HWCAP2_AES)
    {
        haveAES=true;
    }
    std::string data = "zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz";
    std::vector<unsigned char> outHash(32);
    shavite3_ref_hashState ctx_shavite;
    uint64_t nBestTime;
    uint64_t nSel=0;
    
    {
        uint64_t nStart = GetTimeMicros();
        shavite3_ref_hashState ctx_shavite;
        shavite3_ref_Init(&ctx_shavite);
        for (int i=0;i<10;++i)
        {
            shavite3_ref_Update(&ctx_shavite, (uint8_t*)&data[0], data.size());
        }
        shavite3_ref_Final(&ctx_shavite, (uint8_t*)&outHash[0]);
        nBestTime = GetTimeMicros() - nStart;
    }
    
    #ifdef COMPILER_HAS_CORTEX53
    {
        shavite3_256_opt_arm_cortex_a53_Init(&ctx_shavite);
        for (int i=0;i<10;++i)
        {
            shavite3_256_opt_arm_cortex_a53_Update(&ctx_shavite, (uint8_t*)&data[0], data.size());
        }
        shavite3_256_opt_arm_cortex_a53_Final(&ctx_shavite, (uint8_t*)&outHash[0]);
        uint64_t nTime = GetTimeMicros() - nStart;
        
        if (nTime < nBestTime)
        {
            selected_echo256_opt_Init         =      echo256_opt_arm_cortex_a53_Init;
            selected_echo256_opt_Update       =      echo256_opt_arm_cortex_a53_Update;
            selected_echo256_opt_Final        =      echo256_opt_arm_cortex_a53_Final;
            selected_echo256_opt_UpdateFinal  =      echo256_opt_arm_cortex_a53_UpdateFinal;
            selected_shavite3_256_opt_Init    = shavite3_256_opt_arm_cortex_a53_Init;
            selected_shavite3_256_opt_Update  = shavite3_256_opt_arm_cortex_a53_Update;
            selected_shavite3_256_opt_Final   = shavite3_256_opt_arm_cortex_a53_Final;
            selected_argon2_echo_hash         =  argon2_echo_ctx_arm_cortex_a53;
            nBestTime=nTime;
            nSel=1;
        }
    }
    #endif
    #ifdef COMPILER_HAS_CORTEX72
    {
        shavite3_256_opt_arm_cortex_a72_Init(&ctx_shavite);
        for (int i=0;i<10;++i)
        {
            shavite3_256_opt_arm_cortex_a72_Update(&ctx_shavite, (uint8_t*)&data[0], data.size());
        }
        shavite3_256_opt_arm_cortex_a72_Final(&ctx_shavite, (uint8_t*)&outHash[0]);
        uint64_t nTime = GetTimeMicros() - nStart;
        
        if (nTime < nBestTime)
        {
            selected_echo256_opt_Init         =      echo256_opt_arm_cortex_a72_Init;
            selected_echo256_opt_Update       =      echo256_opt_arm_cortex_a72_Update;
            selected_echo256_opt_Final        =      echo256_opt_arm_cortex_a72_Final;
            selected_echo256_opt_UpdateFinal  =      echo256_opt_arm_cortex_a72_UpdateFinal;
            selected_shavite3_256_opt_Init    = shavite3_256_opt_arm_cortex_a72_Init;
            selected_shavite3_256_opt_Update  = shavite3_256_opt_arm_cortex_a72_Update;
            selected_shavite3_256_opt_Final   = shavite3_256_opt_arm_cortex_a72_Final;
            selected_argon2_echo_hash         =  argon2_echo_ctx_arm_cortex_a72;
            nBestTime=nTime;
            nSel=2;
        }
    }
    #endif
    if (haveAES)
    {
        {
            shavite3_256_opt_arm_cortex_a53_aes_Init(&ctx_shavite);
            for (int i=0;i<10;++i)
            {
                shavite3_256_opt_arm_cortex_a53_aes_Update(&ctx_shavite, (uint8_t*)&data[0], data.size());
            }
            shavite3_256_opt_arm_cortex_a53_aes_Final(&ctx_shavite, (uint8_t*)&outHash[0]);
            uint64_t nTime = GetTimeMicros() - nStart;
            
            if (nTime < nBestTime)
            {
                selected_echo256_opt_Init         =      echo256_opt_arm_cortex_a53_aes_Init;
                selected_echo256_opt_Update       =      echo256_opt_arm_cortex_a53_aes_Update;
                selected_echo256_opt_Final        =      echo256_opt_arm_cortex_a53_aes_Final;
                selected_echo256_opt_UpdateFinal  =      echo256_opt_arm_cortex_a53_aes_UpdateFinal;
                selected_shavite3_256_opt_Init    = shavite3_256_opt_arm_cortex_a53_aes_Init;
                selected_shavite3_256_opt_Update  = shavite3_256_opt_arm_cortex_a53_aes_Update;
                selected_shavite3_256_opt_Final   = shavite3_256_opt_arm_cortex_a53_aes_Final;
                selected_argon2_echo_hash         =  argon2_echo_ctx_arm_cortex_a53_aes;
                nBestTime=nTime;
                nSel=3;
            }
        }
        #ifdef COMPILER_HAS_CORTEX72
        {
            shavite3_256_opt_arm_cortex_a72_aes_Init(&ctx_shavite);
            for (int i=0;i<10;++i)
            {
                shavite3_256_opt_arm_cortex_a72_aes_Update(&ctx_shavite, (uint8_t*)&data[0], data.size());
            }
            shavite3_256_opt_arm_cortex_a72_Final(&ctx_shavite, (uint8_t*)&outHash[0]);
            uint64_t nTime = GetTimeMicros() - nStart;
            
            if (nTime < nBestTime)
            {
                selected_echo256_opt_Init         =      echo256_opt_arm_cortex_a72_aes_Init;
                selected_echo256_opt_Update       =      echo256_opt_arm_cortex_a72_aes_Update;
                selected_echo256_opt_Final        =      echo256_opt_arm_cortex_a72_aes_Final;
                selected_echo256_opt_UpdateFinal  =      echo256_opt_arm_cortex_a72_aes_UpdateFinal;
                selected_shavite3_256_opt_Init    = shavite3_256_opt_arm_cortex_a72_aes_Init;
                selected_shavite3_256_opt_Update  = shavite3_256_opt_arm_cortex_a72_aes_Update;
                selected_shavite3_256_opt_Final   = shavite3_256_opt_arm_cortex_a72_aes_Final;
                selected_argon2_echo_hash         =  argon2_echo_ctx_arm_cortex_a72_aes;
                nBestTime=nTime;
                nSel=4;
            }
        }
        #endif
    }
    
    switch (nSel)
    {
        case 0:
            LogPrintf("Selected reference implementation as fastest (no NEON support)\n"); break;
        case 1:
            LogPrintf("Running with Cortex-A53 optimised NEON support (no hardware AES)\n"); break;
        case 2:
            LogPrintf("Running with Cortex-A72 optimised NEON support (no hardware AES)\n"); break;
        case 3:
            LogPrintf("Running with Cortex-A53 optimised NEON+AES support\n"); break;
        case 4:
            LogPrintf("Running with Cortex-A72 optimised NEON+AES support\n"); break;
    }
}
#endif


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
        
        LogPrintf("Verify validation of valid headers\n");
        testValidateValidHeaders(nTestFailCount);
        
        LogPrintf("Verify validation of invalid headers\n");
        testValidateInvalidHeaders(nTestFailCount);
        
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
            sigma_context sigmaContext(arenaCpuCostRounds, slowHashCpuCostRounds, 1024*slowHashMemCostMb, 1024*1024*memCostGb, std::min(memAllowKb, 1024*1024*memCostGb), maxHashesPre, maxHashesPost, numThreads, numSigmaVerifyThreads, numUserVerifyThreads, fastHashMemCostBytes);
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
                std::vector<unsigned char> hashData3(sigmaContext.fastHashSizeBytes);
                for (uint64_t i=0;i<sigmaContext.fastHashSizeBytes;++i)
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
                std::vector<unsigned char> hashData3(sigmaContext.fastHashSizeBytes);
                for (uint64_t i=0;i<sigmaContext.fastHashSizeBytes;++i)
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
            
            {
                LogPrintf("Bench verify [single thread]\n");
                uint64_t nVerifyNumber=100;
                uint64_t nCountValid=0;
                uint64_t nStart = GetTimeMicros();
                for (uint64_t i =0; i< nVerifyNumber; ++i)
                {
                    header.nNonce = rand();
                    nStart = GetTimeMicros();
                    // Count and log number of successes to avoid possibility of compiler optimising the call out.
                    if (sigmaContext.verifyHeader(header, rand()))
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
            sigmaContexts.push_back(new sigma_context(arenaCpuCostRounds, slowHashCpuCostRounds, 1024*slowHashMemCostMb, 1024*1024*memCostGb, instanceMemorySizeKb, maxHashesPre, maxHashesPost, numThreads/sigmaMemorySizes.size(), numSigmaVerifyThreads, numUserVerifyThreads, fastHashMemCostBytes));
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
