// Copyright (c) 2019 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#include "sigma.h"

#include <compat/arch.h>
#include "util.h"
#include "primitives/block.h"

#include <cryptopp/config.h>
#include <cryptopp/aes.h>
#include <cryptopp/modes.h>

#include <stddef.h>
#include <limits.h>
#include <stdlib.h>

#include <boost/scope_exit.hpp>

sigma_settings defaultSigmaSettings;

inline void sigmaRandomFastHash(uint64_t nPseudoRandomAlg, uint8_t* data1, uint64_t data1Size, uint8_t* data2, uint64_t data2Size, uint8_t* data3, uint64_t data3Size, uint256& outHash)
{
    switch (nPseudoRandomAlg)
    {
        case 0:
        {
            //fixme: (SIGMA) - We can obtain a slight speedup by removing this if statement and normalising to always using a pointer for everything
            if (selected_echo256_opt_Init)
            {
                echo256_opt_hashState ctx_echo;
                selected_echo256_opt_Init(&ctx_echo);
                selected_echo256_opt_Update(&ctx_echo, data1, data1Size);
                selected_echo256_opt_Update(&ctx_echo, data2, data2Size);
                selected_echo256_opt_Update(&ctx_echo, data3, data3Size);
                selected_echo256_opt_Final(&ctx_echo, outHash.begin());
            }
            else
            {
                sph_echo256_context ctx_echo;
                sph_echo256_init(&ctx_echo);
                sph_echo256(&ctx_echo, data1, data1Size);
                sph_echo256(&ctx_echo, data2, data2Size);
                sph_echo256(&ctx_echo, data3, data3Size);
                sph_echo256_close(&ctx_echo, outHash.begin());
            }
            break;
        }
        case 1:
        {
            //fixme: (SIGMA) - We can obtain a slight speedup by removing this if statement and normalising to always using a pointer for everything
            if (selected_shavite3_256_opt_Init)
            {   
                shavite3_256_opt_hashState ctx_shavite;
                selected_shavite3_256_opt_Init (&ctx_shavite);
                selected_shavite3_256_opt_Update (&ctx_shavite, data1, data1Size);
                selected_shavite3_256_opt_Update (&ctx_shavite, data2, data2Size);
                selected_shavite3_256_opt_Update (&ctx_shavite, data3, data3Size);
                selected_shavite3_256_opt_Final (&ctx_shavite, outHash.begin());
            }
            else
            {
                shavite3_ref_hashState ctx_shavite;
                shavite3_ref_Init(&ctx_shavite);
                shavite3_ref_Update(&ctx_shavite, data1, data1Size);
                shavite3_ref_Update(&ctx_shavite, data2, data2Size);
                shavite3_ref_Update(&ctx_shavite, data3, data3Size);
                shavite3_ref_Final(&ctx_shavite, (uint8_t*)outHash.begin());
            }
            break;
        }
        default:
            assert(0);
    }
}

inline void sigmaRandomFastHashRef(uint64_t nPseudoRandomAlg, uint8_t* data1, uint64_t data1Size, uint8_t* data2, uint64_t data2Size, uint8_t* data3, uint64_t data3Size, uint256& outHash)
{
    {
        switch (nPseudoRandomAlg)
        {
            case 0:
            {
                sph_echo256_context ctx_echo;
                sph_echo256_init(&ctx_echo);
                sph_echo256(&ctx_echo, data1, data1Size);
                sph_echo256(&ctx_echo, data2, data2Size);
                sph_echo256(&ctx_echo, data3, data3Size);
                sph_echo256_close(&ctx_echo, outHash.begin());
                break;
            }
            case 1:
            {
                shavite3_ref_hashState ctx_shavite;
                shavite3_ref_Init(&ctx_shavite);
                shavite3_ref_Update(&ctx_shavite, data1, data1Size);
                shavite3_ref_Update(&ctx_shavite, data2, data2Size);
                shavite3_ref_Update(&ctx_shavite, data3, data3Size);
                shavite3_ref_Final(&ctx_shavite, (uint8_t*)outHash.begin());
                break;
            }
            default:
                assert(0);
        }
    }
}

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

sigma_settings::sigma_settings()
{
    verify();
}

void sigma_settings::verify()
{
    assert(arenaSizeKb%argonMemoryCostKb==0);
    assert(numHashesPre <= std::numeric_limits<uint16_t>::max()+1);
    assert(numHashesPost <= std::numeric_limits<uint16_t>::max()+1);
    arenaChunkSizeBytes = (arenaSizeKb*1024)/numHashesPost;
    assert(fastHashSizeBytes<=arenaChunkSizeBytes);
}


sigma_context::sigma_context(sigma_settings settings_, uint64_t allocateArenaSizeKb_, uint64_t numThreads_)
: numThreads(numThreads_)
, allocatedArenaSizeKb(allocateArenaSizeKb_)
, settings(settings_)
{           
    assert(allocatedArenaSizeKb <= settings.arenaSizeKb);
    assert(allocatedArenaSizeKb%settings.argonMemoryCostKb==0);
    
    arena = (uint8_t*)malloc(allocatedArenaSizeKb*1024);
    numHashesPossibleWithAvailableMemory = (allocatedArenaSizeKb*1024)/settings.arenaChunkSizeBytes;
}

bool sigma_context::arenaIsValid()
{
    return (arena != nullptr);
}

void sigma_context::prepareArenas(CBlockHeader& headerData)
{
    workerThreads = new boost::asio::thread_pool(numThreads);
    int numHashes = allocatedArenaSizeKb/settings.argonMemoryCostKb;
    
    // Set the nonce to something quasi random, thats difficult for an attacker to control.
    // It doesn't matter exactly what the value is they key thing is that it makes it harder for an attacker to manipulate the arena values, or re-use them across blocks, or to compute the argon hash faster.
    // As it takes away a part of the hashed data that would otherwise be constant.
    // Even if he should find a way to otherwise manipulate things, this measure makes it a bit trickier.
    // This is a bit of a paranoid measure as realistically the argon hashes are of the block header which should always be different anyway.
    uint32_t nBaseNonce = headerData.nBits ^ (uint32_t)(headerData.hashPrevBlock.GetCheapHash());
    for (int i=0;i<numHashes;++i)
    {
        boost::asio::post(*workerThreads, [=]() mutable
        {
            headerData.nNonce = nBaseNonce+i;
            argon2_echo_context context;
            context.t_cost = settings.argonArenaRoundCost;
            context.m_cost = settings.argonMemoryCostKb;
            context.allocated_memory = &arena[(settings.argonMemoryCostKb*1024)*i];                
            context.pwd = (uint8_t*)&headerData.nVersion;
            context.pwdlen = 80;
            
            context.lanes = settings.numVerifyThreads;
            context.threads = 1;
            
            if (selected_argon2_echo_hash(&context, false) != ARGON2_OK)
                assert(0);
        });
    }
    
    workerThreads->join();
}

void sigma_context::benchmarkSlowHashes(uint8_t* hashData, uint64_t numSlowHashes)
{
    uint8_t* hashMem = new uint8_t[settings.argonMemoryCostKb*1024];
    BOOST_SCOPE_EXIT(&hashMem) { delete[] hashMem; } BOOST_SCOPE_EXIT_END

    argon2_echo_context argonContext;
    argonContext.t_cost = settings.argonSlowHashRoundCost;
    argonContext.m_cost = settings.argonMemoryCostKb;
    argonContext.allocated_memory = hashMem;
    argonContext.pwd = (uint8_t*)&hashData[0];
    argonContext.pwdlen = 80;
    
    argonContext.lanes = settings.numVerifyThreads;
    argonContext.threads = 1;

    for (uint64_t i=0;i<numSlowHashes;++i)
    {
        hashData[rand()%80] = rand();
        hashData[rand()%80] = rand();
        hashData[rand()%80] = rand();
        hashData[rand()%80] = rand();
        
        if (selected_argon2_echo_hash(&argonContext, true) != ARGON2_OK)
            assert(0);
    }
}

void sigma_context::benchmarkFastHashes(uint8_t* hashData1, uint8_t* hashData2, uint8_t* hashData3, uint64_t numFastHashes)
{   
    CryptoPP::ECB_Mode<CryptoPP::AES>::Encryption prng;
    prng.SetKey((const unsigned char*)&hashData2[0], 32);
    unsigned char ciphered[32];

    uint256 outHash;
    for (uint64_t i=0;i<numFastHashes;++i)
    {
        prng.ProcessData((unsigned char*)&ciphered[0], (const unsigned char*)&hashData2[0], 32);
        memcpy(&hashData2[0], &ciphered[0], (size_t)32);
                
        hashData1[rand()%80] = rand();
        hashData1[rand()%80] = i;
        hashData2[rand()%32] = i;
        hashData2[rand()%32] = i;
        sigmaRandomFastHash(i%2, (uint8_t*)&hashData1[0], 80, (uint8_t*)&hashData2, 32,  (uint8_t*)&hashData3[0], settings.fastHashSizeBytes, outHash);
    }
}

void sigma_context::benchmarkFastHashesRef(uint8_t* hashData1, uint8_t* hashData2, uint8_t* hashData3, uint64_t numFastHashes)
{   
    CryptoPP::ECB_Mode<CryptoPP::AES>::Encryption prng;
    prng.SetKey((const unsigned char*)&hashData2[0], 32);
    unsigned char ciphered[32];

    uint256 outHash;
    for (uint64_t i=0;i<numFastHashes;++i)
    {
        prng.ProcessData((unsigned char*)&ciphered[0], (const unsigned char*)&hashData2[0], 32);
        memcpy(&hashData2[0], &ciphered[0], (size_t)32);
                
        hashData1[rand()%80] = rand();
        hashData1[rand()%80] = i;
        hashData2[rand()%32] = i;
        hashData2[rand()%32] = i;
        sigmaRandomFastHashRef(i%2, (uint8_t*)&hashData1[0], 80, (uint8_t*)&hashData2, 32,  (uint8_t*)&hashData3[0], settings.fastHashSizeBytes, outHash);
    }
}


//fixme: (SIGMA) - dedup with benchmarkMining
void sigma_context::mineBlock(CBlock* pBlock, std::atomic<uint64_t>& halfHashCounter, uint256& foundBlockHash, bool& interrupt)
{
    CBlockHeader headerData = pBlock->GetBlockHeader();
    
    arith_uint256 hashTarget = arith_uint256().SetCompact(headerData.nBits);
    std::atomic<uint16_t> nGlobalPreNonce=0;
    
    workerThreads = new boost::asio::thread_pool(numThreads);
    BOOST_SCOPE_EXIT(&workerThreads) { workerThreads->join(); delete workerThreads; } BOOST_SCOPE_EXIT_END
    
    for (uint64_t nIndex = 0; nIndex <= settings.numHashesPre;++nIndex)
    {
        boost::asio::post(*workerThreads, [&,headerData]() mutable
        {
            if (UNLIKELY(interrupt))
                return;

            uint16_t nPreNonce = nGlobalPreNonce++;
            uint32_t nBaseNonce = headerData.nBits ^ (uint32_t)(headerData.hashPrevBlock.GetCheapHash());

            // 1. Reset nonce to base nonce and select the pre nonce.
            // This leaves the post nonce in a 'default' but 'psuedo random' state.
            // We do this instead of zeroing out the post-nonce to reduce the predictability of the data to be hashed and therefore make it harder to attack the hash functions.
            uint8_t* hashMem = new uint8_t[settings.argonMemoryCostKb*1024];
            BOOST_SCOPE_EXIT(&hashMem) { delete[] hashMem; } BOOST_SCOPE_EXIT_END
            headerData.nNonce = nBaseNonce;
            headerData.nPreNonce = nPreNonce;
            
            // 2. Perform the "slow" (argon) hash of header with pre-nonce
            argon2_echo_context argonContext;
            argonContext.t_cost = settings.argonSlowHashRoundCost;
            argonContext.m_cost = settings.argonMemoryCostKb;
            argonContext.allocated_memory = hashMem;
            argonContext.pwd = (uint8_t*)&headerData.nVersion;
            argonContext.pwdlen = 80;
            
            argonContext.lanes = settings.numVerifyThreads;
            argonContext.threads = 1;
                       
            if (selected_argon2_echo_hash(&argonContext, true) != ARGON2_OK)
                assert(0);
            
            // 3. Set the initial state of the seed for the 'pseudo random' nonces.
            // PRNG notes:
            // We specifically want a PRNG that does not allow jumping
            // This forces linear instead of parallel PRNG generation which helps to penalise GPU implementations (which thrive on parallelism)
            // Ideally we want something for which CPUs have special instructions so as to be comepetitive with ASICs and faster than GPUs in the PRNG phase.
            // An AES based PRNG is therefore a good fit for this.
            // We do not specifically care about the distribution being 100% perfectly random.
            // Reasonable distribution and randomness are desirable so that certain portions of the memory buffer are not over/under utilised.
            // Non predictability is also desirable to prevent any pre-fetch/order optimisations .
            //
            // We construct a simple PRNG by passing our seed data through AES in ECB mode and then repeatedly feeding it back in.
            // This is possibly not the most high quality RNG ever; however should be perfect for our specific needs.
            CryptoPP::ECB_Mode<CryptoPP::AES>::Encryption prng;
            prng.SetKey((const unsigned char*)&argonContext.outHash[0], 32);
            unsigned char ciphered[32];
                            
            // 4. Iterate through all 'post' nonce combinations, calculating 2 hashes from the global memory.
            // The input of one is determined by the 'post' nonce, while the second is determined by the 'pseudo random' nonce, forcing random memory access.
            headerData.nPostNonce = 0;
            while(true)
            {
                if (UNLIKELY(interrupt))
                    return;

                // 4.1. For each iteration advance the state of the pseudo random nonce
                prng.ProcessData((unsigned char*)&ciphered[0], (const unsigned char*)&argonContext.outHash[0], 32);
                memcpy(&argonContext.outHash[0], &ciphered[0], (size_t)32);

                uint64_t nPseudoRandomNonce1 = (argonContext.outHash[0] ^ argonContext.outHash[1]) % settings.numHashesPost;
                uint64_t nPseudoRandomNonce2 = (argonContext.outHash[2] ^ argonContext.outHash[3]) % settings.numHashesPost;
                
                if (bool skip = nPseudoRandomNonce1 >= numHashesPossibleWithAvailableMemory || nPseudoRandomNonce2 >= numHashesPossibleWithAvailableMemory; LIKELY(!skip))
                {
                    uint64_t nPseudoRandomAlg1 = (argonContext.outHash[0] ^ argonContext.outHash[3]) % 2;
                    uint64_t nPseudoRandomAlg2 = (argonContext.outHash[1] ^ argonContext.outHash[2]) % 2;
                    
                    uint64_t nFastHashOffset1 = (argonContext.outHash[0] ^ argonContext.outHash[2])%(settings.arenaChunkSizeBytes-settings.fastHashSizeBytes);
                    uint64_t nFastHashOffset2 = (argonContext.outHash[1] ^ argonContext.outHash[3])%(settings.arenaChunkSizeBytes-settings.fastHashSizeBytes);

                    // 4.2 Calculate first hash
                    uint256 fastHash;
                    sigmaRandomFastHash(nPseudoRandomAlg1, (uint8_t*)&headerData.nVersion, 80, (uint8_t*)&argonContext.outHash, 32,  &arena[(nPseudoRandomNonce1*settings.arenaChunkSizeBytes)+nFastHashOffset1], settings.fastHashSizeBytes, fastHash);
                    ++halfHashCounter;
                    
                    // 4.3 Evaluate first hash (short circuit evaluation)
                    if (UNLIKELY(UintToArith256(fastHash) <= hashTarget))
                    {
                        // 4.4 Calculate second hash
                        sigmaRandomFastHash(nPseudoRandomAlg2, (uint8_t*)&headerData.nVersion, 80, (uint8_t*)&argonContext.outHash, 32,  &arena[(nPseudoRandomNonce2*settings.arenaChunkSizeBytes)+nFastHashOffset2], settings.fastHashSizeBytes, fastHash);

                        // 4.5 See if we have a valid block
                        if (UNLIKELY(UintToArith256(fastHash) <= hashTarget))
                        {
                            // Found a block, set it and exit.
                            pBlock->nNonce = headerData.nNonce;
                            foundBlockHash = fastHash;
                            interrupt=true;
                            workerThreads->stop();
                            return;
                        }
                    }
                }
                if (UNLIKELY(headerData.nPostNonce == settings.numHashesPost-1))
                    break;
                ++headerData.nPostNonce;
            }
        });
    }
}

void sigma_context::benchmarkMining(CBlockHeader& headerData, std::atomic<uint64_t>& slowHashCounter, std::atomic<uint64_t>& halfHashCounter, std::atomic<uint64_t>& skippedHashCounter, std::atomic<uint64_t>&hashCounter, std::atomic<uint64_t>&blockCounter, uint64_t nRoundsTarget)
{
    arith_uint256 hashTarget = arith_uint256().SetCompact(headerData.nBits);
    std::atomic<uint16_t> nGlobalPreNonce=0;
    
    workerThreads = new boost::asio::thread_pool(numThreads);
    BOOST_SCOPE_EXIT(&workerThreads) { delete workerThreads; } BOOST_SCOPE_EXIT_END
    
    for (uint64_t nIndex = 0; nIndex <= settings.numHashesPre;++nIndex)
    {
        boost::asio::post(*workerThreads, [&,headerData]() mutable
        {
            uint16_t nPreNonce = nGlobalPreNonce++;
            uint32_t nBaseNonce = headerData.nBits ^ (uint32_t)(headerData.hashPrevBlock.GetCheapHash());

            if (hashCounter >= nRoundsTarget)
                return;

            // 1. Reset nonce to base nonce and select the pre nonce.
            // This leaves the post nonce in a 'default' but 'psuedo random' state.
            // We do this instead of zeroing out the post-nonce to reduce the predictability of the data to be hashed and therefore make it harder to attack the hash functions.
            uint8_t* hashMem = new uint8_t[settings.argonMemoryCostKb*1024];
            BOOST_SCOPE_EXIT(&hashMem) { delete[] hashMem; } BOOST_SCOPE_EXIT_END
            headerData.nNonce = nBaseNonce;
            headerData.nPreNonce = nPreNonce;
            
            // 2. Perform the "slow" (argon) hash of header with pre-nonce
            argon2_echo_context argonContext;
            argonContext.t_cost = settings.argonSlowHashRoundCost;
            argonContext.m_cost = settings.argonMemoryCostKb;
            argonContext.allocated_memory = hashMem;
            argonContext.pwd = (uint8_t*)&headerData.nVersion;
            argonContext.pwdlen = 80;
            
            argonContext.lanes = settings.numVerifyThreads;
            argonContext.threads = 1;
            
            ++slowHashCounter;
            
            if (selected_argon2_echo_hash(&argonContext, true) != ARGON2_OK)
                assert(0);
            
            // 3. Set the initial state of the seed for the 'pseudo random' nonces.
            // PRNG notes:
            // We specifically want a PRNG that does not allow jumping
            // This forces linear instead of parallel PRNG generation which helps to penalise GPU implementations (which thrive on parallelism)
            // Ideally we want something for which CPUs have special instructions so as to be comepetitive with ASICs and faster than GPUs in the PRNG phase.
            // An AES based PRNG is therefore a good fit for this.
            // We do not specifically care about the distribution being 100% perfectly random.
            // Reasonable distribution and randomness are desirable so that certain portions of the memory buffer are not over/under utilised.
            // Non predictability is also desirable to prevent any pre-fetch/order optimisations .
            //
            // We construct a simple PRNG by passing our seed data through AES in ECB mode and then repeatedly feeding it back in.
            // This is possibly not the most high quality RNG ever; however should be perfect for our specific needs.
            CryptoPP::ECB_Mode<CryptoPP::AES>::Encryption prng;
            prng.SetKey((const unsigned char*)&argonContext.outHash[0], 32);
            unsigned char ciphered[32];
                            
            // 4. Iterate through all 'post' nonce combinations, calculating 2 hashes from the global memory.
            // The input of one is determined by the 'post' nonce, while the second is determined by the 'pseudo random' nonce, forcing random memory access.
            headerData.nPostNonce = 0;
            while(true)
            {
                if (UNLIKELY(hashCounter >= nRoundsTarget))
                    break;
                
                // 4.1. For each iteration advance the state of the pseudo random nonce
                prng.ProcessData((unsigned char*)&ciphered[0], (const unsigned char*)&argonContext.outHash[0], 32);
                memcpy(&argonContext.outHash[0], &ciphered[0], (size_t)32);

                uint64_t nPseudoRandomNonce1 = (argonContext.outHash[0] ^ argonContext.outHash[1]) % settings.numHashesPost;
                uint64_t nPseudoRandomNonce2 = (argonContext.outHash[2] ^ argonContext.outHash[3]) % settings.numHashesPost;
                if (bool skip = nPseudoRandomNonce1 >= numHashesPossibleWithAvailableMemory || nPseudoRandomNonce2 >= numHashesPossibleWithAvailableMemory; UNLIKELY(skip))
                {
                    ++skippedHashCounter;
                }
                else
                {
                    uint64_t nPseudoRandomAlg1 = (argonContext.outHash[0] ^ argonContext.outHash[3]) % 2;
                    uint64_t nPseudoRandomAlg2 = (argonContext.outHash[1] ^ argonContext.outHash[2]) % 2;
                    uint64_t nFastHashOffset1 = (argonContext.outHash[0] ^ argonContext.outHash[2])%(settings.arenaChunkSizeBytes-settings.fastHashSizeBytes);
                    uint64_t nFastHashOffset2 = (argonContext.outHash[1] ^ argonContext.outHash[3])%(settings.arenaChunkSizeBytes-settings.fastHashSizeBytes);
                    

                    // 4.2 Calculate first hash
                    uint256 fastHash;
                    sigmaRandomFastHash(nPseudoRandomAlg1, (uint8_t*)&headerData.nVersion, 80, (uint8_t*)&argonContext.outHash, 32,  &arena[(nPseudoRandomNonce1*settings.arenaChunkSizeBytes)+nFastHashOffset1], settings.fastHashSizeBytes, fastHash);
                    ++halfHashCounter;
                    
                    
                    // 4.3 Evaluate first hash (short circuit evaluation)
                    if (UNLIKELY(UintToArith256(fastHash) <= hashTarget))
                    {
                        // 4.4 Calculate second hash
                        sigmaRandomFastHash(nPseudoRandomAlg2, (uint8_t*)&headerData.nVersion, 80, (uint8_t*)&argonContext.outHash, 32,  &arena[(nPseudoRandomNonce2*settings.arenaChunkSizeBytes)+nFastHashOffset2], settings.fastHashSizeBytes, fastHash);
                        ++hashCounter;

                        // 4.5 See if we have a valid block
                        if (UNLIKELY(UintToArith256(fastHash) <= hashTarget))
                        {
                            #define LOG_VALID_BLOCK
                            #ifdef LOG_VALID_BLOCK
                            LogPrintf("Found block [%s]\n", HexStr((uint8_t*)&headerData.nVersion, (uint8_t*)&headerData.nVersion+80).c_str());
                            LogPrintf("pseudorandomnonce1[%d] pseudorandomalg1[%d] fasthashoffset1[%d] arenaoffset1[%d]\n", nPseudoRandomNonce1, nPseudoRandomAlg1, nFastHashOffset1, (nPseudoRandomNonce1*settings.arenaChunkSizeBytes)+nFastHashOffset1);
                            LogPrintf("pseudorandomnonce2[%d] pseudorandomalg2[%d] fasthashoffset2[%d] arenaoffset2[%d]\n", nPseudoRandomNonce2, nPseudoRandomAlg2, nFastHashOffset2, (nPseudoRandomNonce2*settings.arenaChunkSizeBytes)+nFastHashOffset2);
                            LogPrintf("base [%d] pre [%d] post [%d] nversion [%d] nbits [%d] ntime [%d]\n", nBaseNonce, headerData.nPreNonce, headerData.nPostNonce, headerData.nVersion, headerData.nBits, headerData.nTime);
                            #endif
                            ++blockCounter;
                        }
                        if (UNLIKELY(hashCounter >= nRoundsTarget))
                            break;
                        
                    }
                }
                if (UNLIKELY(headerData.nPostNonce == settings.numHashesPost-1))
                    break;
                ++headerData.nPostNonce;
            }
        });
    }
    workerThreads->join();
}

sigma_context::~sigma_context()
{
    free(arena);
}


sigma_verify_context::sigma_verify_context(sigma_settings settings_, uint64_t numUserVerifyThreads_)
: settings(settings_)
, numUserVerifyThreads(numUserVerifyThreads_)
{           
    assert(numUserVerifyThreads <= settings.numVerifyThreads);
    hashMem = new uint8_t[settings.argonMemoryCostKb*1024];
    
    //fixme: (SIGMA) - Instead of argon allocating and destroying threads internally we should create threads once per verify context and keep them for the life of the context.
    //This should provide speed benefits when verifying multiple headers in a row.
}

bool sigma_verify_context::verifyHeader(CBlockHeader headerData)
{
    arith_uint256 hashTarget = arith_uint256().SetCompact(headerData.nBits);
    
    uint32_t nBaseNonce = headerData.nBits ^ (uint32_t)(headerData.hashPrevBlock.GetCheapHash());
    uint64_t nPostNonce = headerData.nPostNonce;
    uint64_t nPreNonce = headerData.nPreNonce;
    
    // 1. Reset nonce to base nonce and select the pre nonce.
    // This leaves the post nonce in a 'default' but 'pseudo random' state.
    // We do this instead of zeroing out the post-nonce to reduce the predictability of the data to be hashed and therefore make it harder to attack the hash functions.
    headerData.nNonce = nBaseNonce;
    headerData.nPreNonce = nPreNonce;

    // 2. Perform the "slow" (argon) hash of header with pre-nonce
    argon2_echo_context argonContext;
    argonContext.t_cost = settings.argonSlowHashRoundCost;
    argonContext.m_cost = settings.argonMemoryCostKb;
    argonContext.allocated_memory = hashMem;
    argonContext.pwd = (uint8_t*)&headerData.nVersion;
    argonContext.pwdlen = 80;
    
    argonContext.lanes = settings.numVerifyThreads;
    argonContext.threads = numUserVerifyThreads;

    if (selected_argon2_echo_hash(&argonContext, true) != ARGON2_OK)
        assert(0);

    // 3. Set the initial state of the seed for the 'pseudo random' nonces.
    CryptoPP::ECB_Mode<CryptoPP::AES>::Encryption prng;
    prng.SetKey((const unsigned char*)&argonContext.outHash[0], 32);
    unsigned char ciphered[32];
                            
    // 4. Advance PRNG to 'post' nonce
    headerData.nPostNonce=0;
    for (uint64_t i=0; i<=nPostNonce; ++i)
    {               
        prng.ProcessData((unsigned char*)&ciphered[0], (const unsigned char*)&argonContext.outHash[0], 32);
        memcpy(&argonContext.outHash[0], &ciphered[0], (size_t)32);
        ++headerData.nPostNonce;
    }
    
    uint64_t nPseudoRandomNonce1 = (argonContext.outHash[0] ^ argonContext.outHash[1]) % settings.numHashesPost;
    uint64_t nPseudoRandomNonce2 = (argonContext.outHash[2] ^ argonContext.outHash[3]) % settings.numHashesPost;
    uint64_t nPseudoRandomAlg1   = (argonContext.outHash[0] ^ argonContext.outHash[3]) % 2;
    uint64_t nPseudoRandomAlg2   = (argonContext.outHash[1] ^ argonContext.outHash[2]) % 2;
    uint64_t nFastHashOffset1    = (argonContext.outHash[0] ^ argonContext.outHash[2]) % (settings.arenaChunkSizeBytes-settings.fastHashSizeBytes);
    uint64_t nFastHashOffset2    = (argonContext.outHash[1] ^ argonContext.outHash[3]) % (settings.arenaChunkSizeBytes-settings.fastHashSizeBytes);
    std::array<uint64_t, 4> slowHash = std::move(argonContext.outHash);

    uint64_t nArenaMemoryIndex1  = (settings.arenaChunkSizeBytes*nPseudoRandomNonce1) / (settings.argonMemoryCostKb*1024);
    uint64_t nArenaMemoryOffset1 = (settings.arenaChunkSizeBytes*nPseudoRandomNonce1) % (settings.argonMemoryCostKb*1024);
    uint64_t nArenaMemoryIndex2  = (settings.arenaChunkSizeBytes*nPseudoRandomNonce2) / (settings.argonMemoryCostKb*1024);
    uint64_t nArenaMemoryOffset2 = (settings.arenaChunkSizeBytes*nPseudoRandomNonce2) % (settings.argonMemoryCostKb*1024);
    
    // 5. Generate the part(s) of the arena we need as we don't have the whole arena like a miner would.
    {
        headerData.nNonce = nBaseNonce+nArenaMemoryIndex1;
        argonContext.t_cost = settings.argonArenaRoundCost;    
        
        if (selected_argon2_echo_hash(&argonContext, false) != ARGON2_OK)
            assert(0);
        
        uint256 fastHash;
        // 6. For each fast hash, set the pre and post nonce to the final values the miner claims he was using.
        headerData.nPreNonce = nPreNonce;
        headerData.nPostNonce = nPostNonce;
        sigmaRandomFastHash(nPseudoRandomAlg1, (uint8_t*)&headerData.nVersion, 80, (uint8_t*)slowHash.begin(), 32,  &argonContext.allocated_memory[nArenaMemoryOffset1+nFastHashOffset1], settings.fastHashSizeBytes, fastHash);

        if (UintToArith256(fastHash) <= hashTarget)
        {
            // 7. First fast nonce checks out, repeat process for second fast hash
            headerData.nNonce = nBaseNonce+nArenaMemoryIndex2;
            if (selected_argon2_echo_hash(&argonContext, false) != ARGON2_OK)
                assert(0);
            headerData.nPreNonce = nPreNonce;
            headerData.nPostNonce = nPostNonce;
            sigmaRandomFastHash(nPseudoRandomAlg2, (uint8_t*)&headerData.nVersion, 80, (uint8_t*)slowHash.begin(), 32,  &argonContext.allocated_memory[nArenaMemoryOffset2+nFastHashOffset2], settings.fastHashSizeBytes, fastHash);
            
            if (UintToArith256(fastHash) <= hashTarget)
            {
                // 8. Hooray! the block is valid.
                return true;
            }
        }
    }
    return false;
}

sigma_verify_context::~sigma_verify_context()
{
    delete[] hashMem;
}
