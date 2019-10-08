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
HashReturn (*selected_echo256_opt_Init)(echo256_opt_hashState* state) = nullptr;
HashReturn (*selected_echo256_opt_Update)(echo256_opt_hashState* state, const unsigned char* data, uint64_t databitlen) = nullptr;
HashReturn (*selected_echo256_opt_Final)(echo256_opt_hashState* state, unsigned char* hashval) = nullptr;
HashReturn (*selected_echo256_opt_UpdateFinal)(echo256_opt_hashState* state, unsigned char* hashval, const unsigned char* data, uint64_t databitlen) = nullptr;
bool (*selected_shavite3_256_opt_Init)(shavite3_256_opt_hashState* state) = nullptr;
bool (*selected_shavite3_256_opt_Update)(shavite3_256_opt_hashState* state, const unsigned char* data, uint64_t dataLenBytes) = nullptr;
bool (*selected_shavite3_256_opt_Final)(shavite3_256_opt_hashState* state, unsigned char* hashval) = nullptr;
int (*selected_argon2_echo_hash)(argon2_echo_context* context, bool doHash) = nullptr;

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
#define FORCE_SELECT_OPTIMISED_SHAVITE(CPU, IDX) \
{\
    selected_shavite3_256_opt_Init   = shavite3_256_opt_##CPU##_Init;\
    selected_shavite3_256_opt_Update = shavite3_256_opt_##CPU##_Update;\
    selected_shavite3_256_opt_Final  = shavite3_256_opt_##CPU##_Final;\
    nSelShavite=IDX;\
}
#define FORCE_SELECT_OPTIMISED_ECHO(CPU, IDX) \
{\
    selected_echo256_opt_Init        = echo256_opt_##CPU##_Init;\
    selected_echo256_opt_Update      = echo256_opt_##CPU##_Update;\
    selected_echo256_opt_Final       = echo256_opt_##CPU##_Final;\
    selected_echo256_opt_UpdateFinal = echo256_opt_##CPU##_UpdateFinal;\
    nSelEcho=IDX;\
}      
#define FORCE_SELECT_OPTIMISED_ARGON(CPU, IDX) \
{\
    selected_argon2_echo_hash = argon2_echo_ctx_##CPU;\
    nSelArgon=IDX;\
}

#ifdef ARCH_CPU_X86_FAMILY
void LogSelection(uint64_t nSel, std::string sAlgoName)
{
    switch (nSel)
    {
        case 0:
            LogPrintf("[%d] Selected reference implementation \n", sAlgoName); break;
        case 1:
            LogPrintf("[%d] Selected avx512f-aes\n", sAlgoName); break;
        case 2:
            LogPrintf("[%d] Selected avx2-aes\n", sAlgoName); break;
        case 3:
            LogPrintf("[%d] Selected avx-aes\n", sAlgoName); break;
        case 4:
            LogPrintf("[%d] Selected sse4-aes\n", sAlgoName); break;
        case 5:
            LogPrintf("[%d] Selected sse3-aes\n", sAlgoName); break;
        case 6:
            LogPrintf("[%d] Selected sse2-aes\n", sAlgoName); break;
        case 7:
            LogPrintf("[%d] Selected avx512f\n", sAlgoName); break;
        case 8:
            LogPrintf("[%d] Selected avx2\n", sAlgoName); break;
        case 9:
            LogPrintf("[%d] Selected avx\n", sAlgoName); break;
        case 10:
            LogPrintf("[%d] Selected sse4\n", sAlgoName); break;
        case 11:
            LogPrintf("[%d] Selected sse3\n", sAlgoName); break;
        case 12:
            LogPrintf("[%d] Selected sse2\n", sAlgoName); break;
        case 9999:
            LogPrintf("[%d] Selected hybrid implementation\n", sAlgoName); break;
    }
}
#elif defined(ARCH_CPU_ARM_FAMILY)
void LogSelection(uint64_t nSel, std::string sAlgoName)
{
    switch (nSel)
    {
        case 0:
            LogPrintf("[%d] Selected reference implementation (no NEON support)\n", sAlgoName); break;
        case 1:
            LogPrintf("[%d] Selected Cortex-A53 optimised NEON support (no hardware AES)\n", sAlgoName); break;
        case 2:
            LogPrintf("[%d] Selected Cortex-A57 optimised NEON support (no hardware AES)\n", sAlgoName); break;
        case 3:
            LogPrintf("[%d] Selected Cortex-A72 optimised NEON support (no hardware AES)\n", sAlgoName); break;
        case 4:
            LogPrintf("[%d] Selected Cortex-A53 optimised NEON+AES support\n", sAlgoName); break;
        case 5:
            LogPrintf("[%d] Selected Cortex-A57 optimised NEON+AES support\n", sAlgoName); break;
        case 6:
            LogPrintf("[%d] Selected Cortex-A72 optimised NEON+AES support\n", sAlgoName); break;
        case 7:
            LogPrintf("[%d] Selected Thunderx optimised NEON+AES support\n", sAlgoName); break;
        case 9999:
            LogPrintf("[%d] Selected hybrid implementation\n", sAlgoName); break;
    }
}
#include <sys/auxv.h>
#else
void LogSelection(uint64_t nSel, std::string sAlgoName)
{
    //fixme: (SIGMA) Implement for riscv
}
#endif

void selectOptimisedImplementations()
{
    uint64_t nSelShavite=0;
    uint64_t nSelEcho=0;
    uint64_t nSelArgon=0;

    #ifndef ARCH_CPU_X86_FAMILY
    std::string data = "zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz";
    std::vector<unsigned char> outHash(32);
    shavite3_256_opt_hashState ctx_shavite;
    echo256_opt_hashState ctx_echo;

    uint64_t nBestTimeShavite = std::numeric_limits<uint64_t>::max();
    uint64_t nBestTimeEcho = std::numeric_limits<uint64_t>::max();
    uint64_t nBestTimeArgon = std::numeric_limits<uint64_t>::max();
    
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
    #endif
  
    #ifdef ARCH_CPU_X86_FAMILY
    {
        {
            #if defined(COMPILER_HAS_AVX512F)
            if (__builtin_cpu_supports("avx512f"))
            {
                FORCE_SELECT_OPTIMISED_SHAVITE(avx512f, 7);
                FORCE_SELECT_OPTIMISED_ECHO   (avx512f, 7);
                FORCE_SELECT_OPTIMISED_ARGON  (avx512f, 7);
                goto logselection;
            }
            #endif
            #if defined(COMPILER_HAS_AVX2)
            if (__builtin_cpu_supports("avx2"))
            {
                FORCE_SELECT_OPTIMISED_SHAVITE(avx2, 8);
                FORCE_SELECT_OPTIMISED_ECHO   (avx2, 8);
                FORCE_SELECT_OPTIMISED_ARGON  (avx2, 8);
                goto logselection;
            }
            #endif
            #if defined(COMPILER_HAS_AVX)
            if (__builtin_cpu_supports("avx"))
            {
                FORCE_SELECT_OPTIMISED_SHAVITE(avx, 9);
                FORCE_SELECT_OPTIMISED_ECHO   (avx, 9);
                FORCE_SELECT_OPTIMISED_ARGON  (avx, 9);
                goto logselection;
            }
            #endif
            #if defined(COMPILER_HAS_SSE4)
            if (__builtin_cpu_supports("sse4.2"))
            {
                FORCE_SELECT_OPTIMISED_SHAVITE(sse4, 10);
                FORCE_SELECT_OPTIMISED_ECHO   (sse4, 10);
                FORCE_SELECT_OPTIMISED_ARGON  (sse4, 10);
                goto logselection;
            }
            #endif
            #if defined(COMPILER_HAS_SSE3)
            if (__builtin_cpu_supports("sse3"))
            {
                FORCE_SELECT_OPTIMISED_SHAVITE(sse3, 11);
                FORCE_SELECT_OPTIMISED_ECHO   (sse3, 11);
                FORCE_SELECT_OPTIMISED_ARGON  (sse3, 11);
                goto logselection;
            }
            #endif
            #if defined(COMPILER_HAS_SSE2)
            #if 0
            //fixme: (SIGMA)
            else if (__builtin_cpu_supports("sse2"))
            {
                FORCE_SELECT_OPTIMISED_SHAVITE(sse2, 12);
                FORCE_SELECT_OPTIMISED_ECHO   (sse2, 12);
                FORCE_SELECT_OPTIMISED_ARGON  (sse2, 12);
                goto logselection;
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
    
    // Finally (only after we have fastest echo implementation) give the hybrid echo a go
    // Just in case it happens to be faster.
    SELECT_OPTIMISED_ARGON(hybrid, 9999);
    #endif
    
logselection:
    LogSelection(nSelShavite, "shavite");
    LogSelection(nSelEcho, "echo");
    LogSelection(nSelArgon, "argon");
}

void normaliseBufferSize(uint64_t& nBufferSizeBytes)
{
    nBufferSizeBytes = (nBufferSizeBytes / (defaultSigmaSettings.argonMemoryCostKb*1024)) * (defaultSigmaSettings.argonMemoryCostKb*1024);
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


sigma_verify_context::sigma_verify_context(sigma_settings settings_, uint64_t numUserVerifyThreads_)
: settings(settings_)
, numUserVerifyThreads(numUserVerifyThreads_)
{           
    assert(numUserVerifyThreads <= settings.numVerifyThreads);
    hashMem = new uint8_t[settings.argonMemoryCostKb*1024];
    
    //fixme: (SIGMA) - Instead of argon allocating and destroying threads internally we should create threads once per verify context and keep them for the life of the context.
    //This should provide speed benefits when verifying multiple headers in a row.
}

template<int verifyLevel> bool sigma_verify_context::verifyHeader(CBlockHeader headerData)
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
    
    [[maybe_unused]] uint64_t nPseudoRandomNonce1 = (argonContext.outHash[0] ^ argonContext.outHash[1]) % settings.numHashesPost;
    [[maybe_unused]] uint64_t nPseudoRandomNonce2 = (argonContext.outHash[2] ^ argonContext.outHash[3]) % settings.numHashesPost;
    [[maybe_unused]] uint64_t nPseudoRandomAlg1   = (argonContext.outHash[0] ^ argonContext.outHash[3]) % 2;
    [[maybe_unused]] uint64_t nPseudoRandomAlg2   = (argonContext.outHash[1] ^ argonContext.outHash[2]) % 2;
    [[maybe_unused]] uint64_t nFastHashOffset1    = (argonContext.outHash[0] ^ argonContext.outHash[2]) % (settings.arenaChunkSizeBytes-settings.fastHashSizeBytes);
    [[maybe_unused]] uint64_t nFastHashOffset2    = (argonContext.outHash[1] ^ argonContext.outHash[3]) % (settings.arenaChunkSizeBytes-settings.fastHashSizeBytes);
    std::array<uint64_t, 4> slowHash = std::move(argonContext.outHash);

    [[maybe_unused]] uint64_t nArenaMemoryIndex1  = (settings.arenaChunkSizeBytes*nPseudoRandomNonce1) / (settings.argonMemoryCostKb*1024);
    [[maybe_unused]] uint64_t nArenaMemoryOffset1 = (settings.arenaChunkSizeBytes*nPseudoRandomNonce1) % (settings.argonMemoryCostKb*1024);
    [[maybe_unused]] uint64_t nArenaMemoryIndex2  = (settings.arenaChunkSizeBytes*nPseudoRandomNonce2) / (settings.argonMemoryCostKb*1024);
    [[maybe_unused]] uint64_t nArenaMemoryOffset2 = (settings.arenaChunkSizeBytes*nPseudoRandomNonce2) % (settings.argonMemoryCostKb*1024);
    
    // 5. Generate the part(s) of the arena we need as we don't have the whole arena like a miner would.
    {
        uint256 fastHash;
        argonContext.t_cost = settings.argonArenaRoundCost;    
        
        // For each fast hash, set the pre and post nonce to the final values the miner claims he was using
        // Then calculate the fast hash and compare against the hash target to see if it meets it or not
        // NB!!! As a special optimisation we allow the caller to execute discretion and skip the check for one of the two fast hashes
        // This is fine if used sparingly and with other precautions in place but caution should be exercised as if used carelessly it can make it easier to split the chain
        // NB!!! Do not make use of this unless you fully understand the repercussions
        
        // 6. Verify first fast hash
        if (verifyLevel == 0 || verifyLevel == 1)
        {
            headerData.nNonce = nBaseNonce+nArenaMemoryIndex1;
            if (selected_argon2_echo_hash(&argonContext, false) != ARGON2_OK)
                assert(0);        
            headerData.nPreNonce = nPreNonce;
            headerData.nPostNonce = nPostNonce;
            sigmaRandomFastHash(nPseudoRandomAlg1, (uint8_t*)&headerData.nVersion, 80, (uint8_t*)slowHash.begin(), 32,  &argonContext.allocated_memory[nArenaMemoryOffset1+nFastHashOffset1], settings.fastHashSizeBytes, fastHash);
            if (UintToArith256(fastHash) > hashTarget)
            {
                return false;
            }
        }
        
        // 7. First fast hash checks out, repeat process for second fast hash
        if (verifyLevel == 0 || verifyLevel == 2)
        {
            headerData.nNonce = nBaseNonce+nArenaMemoryIndex2;
            if (selected_argon2_echo_hash(&argonContext, false) != ARGON2_OK)
                assert(0);
            headerData.nPreNonce = nPreNonce;
            headerData.nPostNonce = nPostNonce;
            sigmaRandomFastHash(nPseudoRandomAlg2, (uint8_t*)&headerData.nVersion, 80, (uint8_t*)slowHash.begin(), 32,  &argonContext.allocated_memory[nArenaMemoryOffset2+nFastHashOffset2], settings.fastHashSizeBytes, fastHash);
            if (UintToArith256(fastHash) > hashTarget)
            {
                return false;
            }
        }

        // 8. Hooray! the block is valid.
        return true;
    }
    return false;
}
template bool sigma_verify_context::verifyHeader<0>(CBlockHeader);
template bool sigma_verify_context::verifyHeader<1>(CBlockHeader);
template bool sigma_verify_context::verifyHeader<2>(CBlockHeader);

sigma_verify_context::~sigma_verify_context()
{
    delete[] hashMem;
}
