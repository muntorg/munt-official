// Copyright (c) 2019 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#include "sigma.h"

#include <compat/arch.h>

#include <cryptopp/config.h>
#include <cryptopp/aes.h>
#include <cryptopp/modes.h>

#include <stddef.h>
#include <limits.h>
#include <stdlib.h>

#include <boost/scope_exit.hpp>

#include <crypto/hash/sigma/argon_echo/argon_echo.h>
#include <crypto/hash/echo256/sphlib/sph_echo.h>

#define ECHO_DP
#define LENGTH 256
#include <crypto/hash/echo256/echo256_aesni.h>
#include <crypto/hash/shavite3_256/shavite3_256_aesni.h>
#include <crypto/hash/shavite3_256/ref/shavite3_ref.h>

inline void sigmaRandomFastHash(uint64_t nPseudoRandomAlg, uint8_t* data1, uint64_t data1Size, uint8_t* data2, uint64_t data2Size, uint8_t* data3, uint64_t data3Size, uint256& outHash)
{
    #if defined(ARCH_CPU_X86_FAMILY) || defined(ARCH_CPU_ARM_FAMILY)
    #if defined(ARCH_CPU_X86_FAMILY)
    if (__builtin_cpu_supports("aes"))
    #else
    // Always use the 'optimised' version on arm for now.
    if (true)
    #endif
    {
        switch (nPseudoRandomAlg)
        {
            case 0:
            {
                echo256_aesni_hashState ctx_echo;
                echo256_aesni_Init(&ctx_echo);
                echo256_aesni_Update(&ctx_echo, data1, data1Size);
                echo256_aesni_Update(&ctx_echo, data2, data2Size);
                echo256_aesni_Update(&ctx_echo, data3, data3Size);
                echo256_aesni_Final(&ctx_echo, outHash.begin());
                break;
            }
            case 1:
            {
                shavite3_256_aesni_hashState ctx_shavite;
                shavite3_256_aesni_Init(&ctx_shavite);
                shavite3_256_aesni_Update(&ctx_shavite, data1, data1Size);
                shavite3_256_aesni_Update(&ctx_shavite, data2, data2Size);
                shavite3_256_aesni_Update(&ctx_shavite, data3, data3Size);
                shavite3_256_aesni_Final(&ctx_shavite, outHash.begin());
                break;
            }
            default:
                assert(0);
        }
    }
    else
    #endif
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

sigma_context::sigma_context(uint32_t argonArenaRoundCost_, uint32_t argonSlowHashRoundCost_, uint64_t argonMemoryCostKb_, uint64_t arena_size_kb, uint64_t allocateArenaSizeKb_, uint64_t numHashesPre_, uint64_t numHashesPost_, uint64_t numThreads_, uint64_t numVerifyThreads_, uint64_t numUserVerifyThreads_, uint64_t fastHashSizeBytes_)
: numThreads(numThreads_)
, numVerifyThreads(numVerifyThreads_)
, numUserVerifyThreads(numUserVerifyThreads_)
, arenaSizeKb(arena_size_kb)
, allocatedArenaSizeKb(allocateArenaSizeKb_)
, argonMemoryCostKb(argonMemoryCostKb_)
, argonArenaRoundCost(argonArenaRoundCost_)
, argonSlowHashRoundCost(argonSlowHashRoundCost_)
, numHashesPre(numHashesPre_)
, numHashesPost(numHashesPost_)
, fastHashSizeBytes(fastHashSizeBytes_)
{           
    assert(allocatedArenaSizeKb <= arenaSizeKb);
    assert(allocatedArenaSizeKb%argonMemoryCostKb==0);
    assert(arenaSizeKb%argonMemoryCostKb==0);
    assert(numHashesPre <= std::numeric_limits<uint16_t>::max()+1);
    assert(numHashesPost <= std::numeric_limits<uint16_t>::max()+1);
    assert(numUserVerifyThreads <= numVerifyThreads);
    
    arena = (uint8_t*)malloc(allocatedArenaSizeKb*1024);
    
    arenaChunkSizeBytes = (arenaSizeKb*1024)/numHashesPost;
    numHashesPossibleWithAvailableMemory = (allocatedArenaSizeKb*1024)/arenaChunkSizeBytes;
    assert(fastHashSizeBytes<=arenaChunkSizeBytes);
}

bool sigma_context::arenaIsValid()
{
    return (arena != nullptr);
}

void sigma_context::prepareArenas(CBlockHeader& headerData, uint64_t nBlockHeight)
{
    headerBlockHeight = nBlockHeight;
    workerThreads = new boost::asio::thread_pool(numThreads);

    int numHashes = allocatedArenaSizeKb/argonMemoryCostKb;
    
    for (int i=0;i<numHashes;++i)
    {
        boost::asio::post(*workerThreads, [=]() mutable
        {
            headerData.nNonce = headerBlockHeight+i;
            argon2_echo_context context;
            context.t_cost = argonArenaRoundCost;
            context.m_cost = argonMemoryCostKb;
            context.allocated_memory = &arena[(argonMemoryCostKb*1024)*i];                
            context.pwd = (uint8_t*)&headerData.nVersion;
            context.pwdlen = 80;
            
            context.lanes = numVerifyThreads;
            context.threads = 1;
            
            if (argon2_echo_ctx(&context, false) != ARGON2_OK)
                assert(0);
        });
    }
    
    workerThreads->join();
}

void sigma_context::benchmarkSlowHashes(uint8_t* hashData, uint64_t numSlowHashes)
{
    uint8_t* hashMem = new uint8_t[argonMemoryCostKb*1024];
    BOOST_SCOPE_EXIT(&hashMem) { delete[] hashMem; } BOOST_SCOPE_EXIT_END

    argon2_echo_context argonContext;
    argonContext.t_cost = argonSlowHashRoundCost;
    argonContext.m_cost = argonMemoryCostKb;
    argonContext.allocated_memory = hashMem;
    argonContext.pwd = (uint8_t*)&hashData[0];
    argonContext.pwdlen = 80;
    
    argonContext.lanes = numVerifyThreads;
    argonContext.threads = 1;

    for (uint64_t i=0;i<numSlowHashes;++i)
    {
        hashData[rand()%80] = rand();
        hashData[rand()%80] = rand();
        hashData[rand()%80] = rand();
        hashData[rand()%80] = rand();
        
        if (argon2_echo_ctx(&argonContext, true) != ARGON2_OK)
            assert(0);
    }
}

void sigma_context::benchmarkFastHashes(uint8_t* hashData1, uint8_t* hashData2, uint8_t* hashData3, uint64_t numFastHashes)
{   
    CryptoPP::ECB_Mode<CryptoPP::AES>::Encryption prng;
    prng.SetKey((const unsigned char*)&hashData2[0], 32);
    unsigned char ciphered[32];
    prng.ProcessData((unsigned char*)&ciphered[0], (const unsigned char*)&hashData2[0], 32);
    memcpy(&hashData2[0], &ciphered[0], (size_t)32);

    uint256 outHash;
    for (uint64_t i=0;i<numFastHashes;++i)
    {
        prng.ProcessData((unsigned char*)&ciphered[0], (const unsigned char*)&hashData2[0], 32);
        memcpy(&hashData2[0], &ciphered[0], (size_t)32);
                
        hashData1[rand()%80] = rand();
        hashData1[rand()%80] = i;
        hashData2[rand()%32] = i;
        hashData2[rand()%32] = i;
        sigmaRandomFastHash(i%2, (uint8_t*)&hashData1[0], 80, (uint8_t*)&hashData2, 32,  (uint8_t*)&hashData3[0], fastHashSizeBytes, outHash);
    }
}

void sigma_context::benchmarkFastHashesRef(uint8_t* hashData1, uint8_t* hashData2, uint8_t* hashData3, uint64_t numFastHashes)
{   
    CryptoPP::ECB_Mode<CryptoPP::AES>::Encryption prng;
    prng.SetKey((const unsigned char*)&hashData2[0], 32);
    unsigned char ciphered[32];
    prng.ProcessData((unsigned char*)&ciphered[0], (const unsigned char*)&hashData2[0], 32);
    memcpy(&hashData2[0], &ciphered[0], (size_t)32);

    uint256 outHash;
    for (uint64_t i=0;i<numFastHashes;++i)
    {
        prng.ProcessData((unsigned char*)&ciphered[0], (const unsigned char*)&hashData2[0], 32);
        memcpy(&hashData2[0], &ciphered[0], (size_t)32);
                
        hashData1[rand()%80] = rand();
        hashData1[rand()%80] = i;
        hashData2[rand()%32] = i;
        hashData2[rand()%32] = i;
        sigmaRandomFastHashRef(i%2, (uint8_t*)&hashData1[0], 80, (uint8_t*)&hashData2, 32,  (uint8_t*)&hashData3[0], fastHashSizeBytes, outHash);
    }
}

void sigma_context::benchmarkMining(CBlockHeader& headerData, std::atomic<uint64_t>& slowHashCounter, std::atomic<uint64_t>& halfHashCounter, std::atomic<uint64_t>& skippedHashCounter, std::atomic<uint64_t>&hashCounter, std::atomic<uint64_t>&blockCounter, uint64_t nRoundsTarget)
{
    arith_uint256 hashTarget = arith_uint256().SetCompact(headerData.nBits);
    
    std::atomic<uint16_t> nPreNonce=0;
    
    workerThreads = new boost::asio::thread_pool(numThreads);
    BOOST_SCOPE_EXIT(&workerThreads) { delete workerThreads; } BOOST_SCOPE_EXIT_END
    
    for (uint64_t nIndex = 0; nIndex <= numHashesPre;++nIndex)
    {
        boost::asio::post(*workerThreads, [&,headerData]() mutable
        {
            if (hashCounter >= nRoundsTarget)
                return;

            //1. Select pre nonce, reset post nonce to zero and perform argon hash of header.
            uint8_t* hashMem = new uint8_t[argonMemoryCostKb*1024];
            BOOST_SCOPE_EXIT(&hashMem) { delete[] hashMem; } BOOST_SCOPE_EXIT_END
            headerData.nNonce = headerBlockHeight;
            headerData.nPreNonce = nPreNonce++;
            headerData.nPostNonce = 0;
            
            argon2_echo_context argonContext;
            argonContext.t_cost = argonSlowHashRoundCost;
            argonContext.m_cost = argonMemoryCostKb;
            argonContext.allocated_memory = hashMem;
            argonContext.pwd = (uint8_t*)&headerData.nVersion;
            argonContext.pwdlen = 80;
            
            argonContext.lanes = numVerifyThreads;
            argonContext.threads = 1;
            
            ++slowHashCounter;
            
            if (argon2_echo_ctx(&argonContext, true) != ARGON2_OK)
                assert(0);
            
            //2. Set the initial state of the seed for the 'pseudo random' nonces.
            //PRNG notes:
            //We specifically want a PRNG that does not allow jumping
            //This forces linear instead of parallel PRNG generation which helps to penalise GPU implementations (which thrive on parallelism)
            //Ideally we want something for which CPUs have special instructions so as to be comepetitive with ASICs and faster than GPUs in the PRNG phase.
            //An AES based PRNG is therefore a good fit for this.
            //We do not specifically care about the distribution being 100% perfectly random.
            //Reasonable distribution and randomness are desirable so that certain portions of the memory buffer are not over/under utilised.
            //Non predictability is also desirable to prevent any pre-fetch/order optimisations .
            //
            //We construct a simple PRNG by passing our seed data through AES in ECB mode and then repeatedly feeding it back in.
            //This is possibly not the most high quality RNG ever; however should be perfect for our specific needs.
            CryptoPP::ECB_Mode<CryptoPP::AES>::Encryption prng;
            prng.SetKey((const unsigned char*)&argonContext.outHash[0], 32);
            unsigned char ciphered[32];
            prng.ProcessData((unsigned char*)&ciphered[0], (const unsigned char*)&argonContext.outHash[0], 32);
            memcpy(&argonContext.outHash[0], &ciphered[0], (size_t)32);
                            
            //3. Iterate through all 'post' nonce combinations, calculating 2 hashes from the global memory.
            //The input of one is determined by the 'post' nonce, while the second is determined by the 'pseudo random' nonce, forcing random memory access.
            while(true)
            {
                if (hashCounter >= nRoundsTarget)
                    break;
                
                //3.1. For each iteration advance the state of the pseudo random nonce
                prng.ProcessData((unsigned char*)&ciphered[0], (const unsigned char*)&argonContext.outHash[0], 32);
                memcpy(&argonContext.outHash[0], &ciphered[0], (size_t)32);
                uint64_t nPseudoRandomNonce1 = (argonContext.outHash[0] ^ argonContext.outHash[1]) % numHashesPost;
                uint64_t nPseudoRandomNonce2 = (argonContext.outHash[2] ^ argonContext.outHash[3]) % numHashesPost;
                
                if (nPseudoRandomNonce1 >= numHashesPossibleWithAvailableMemory || nPseudoRandomNonce2 >= numHashesPossibleWithAvailableMemory)
                {
                    ++skippedHashCounter;
                }
                else
                {
                    uint64_t nPseudoRandomAlg1 = (argonContext.outHash[0] ^ argonContext.outHash[3]) % 2;
                    uint64_t nPseudoRandomAlg2 = (argonContext.outHash[1] ^ argonContext.outHash[2]) % 2;
                    
                    uint64_t nFastHashOffset1 = (argonContext.outHash[0] ^ argonContext.outHash[2])%(arenaChunkSizeBytes-fastHashSizeBytes);
                    uint64_t nFastHashOffset2 = (argonContext.outHash[1] ^ argonContext.outHash[3])%(arenaChunkSizeBytes-fastHashSizeBytes);

                    //3.2 Calculate first hash
                    uint256 outHash;
                    sigmaRandomFastHash(nPseudoRandomAlg1, (uint8_t*)&headerData.nVersion, 80, (uint8_t*)&argonContext.outHash, 32,  &arena[(nPseudoRandomNonce1*arenaChunkSizeBytes)+nFastHashOffset1], fastHashSizeBytes, outHash);
                    ++halfHashCounter;
                    
                    //3.3 Evaluate first hash (short circuit evaluation)
                    if (UintToArith256(outHash) <= hashTarget)
                    {
                        //3.4 Calculate second hash
                        sigmaRandomFastHash(nPseudoRandomAlg2, (uint8_t*)&headerData.nVersion, 80, (uint8_t*)&argonContext.outHash, 32,  &arena[(nPseudoRandomNonce2*arenaChunkSizeBytes)+nFastHashOffset2], fastHashSizeBytes, outHash);
                        ++hashCounter;

                        //3.5 See if we have a valid block
                        if (UintToArith256(outHash) <= hashTarget)
                        {
                            //#define LOG_VALID_BLOCK
                            #ifdef LOG_VALID_BLOCK
                            LogPrintf("Found block [0x%s] [%d]\n", HexStr((uint8_t*)&headerData.nVersion, (uint8_t*)&headerData.nVersion+80).c_str(), headerBlockHeight);
                            LogPrintf("pseudorandomnonce1[%d] pseudorandomalg1[%d] fasthashoffset1[%d] arenaoffset1[%d]\n", nPseudoRandomNonce1, nPseudoRandomAlg1, nFastHashOffset1, (nPseudoRandomNonce1*arenaChunkSizeBytes)+nFastHashOffset1);
                            LogPrintf("pseudorandomnonce2[%d] pseudorandomalg2[%d] fasthashoffset2[%d] arenaoffset2[%d]\n", nPseudoRandomNonce2, nPseudoRandomAlg2, nFastHashOffset2, (nPseudoRandomNonce2*arenaChunkSizeBytes)+nFastHashOffset2);
                            LogPrintf("pre [%d] post [%d] nversion [%d] nbits [%d] ntime [%d]\n", headerData.nPreNonce, headerData.nPostNonce, headerData.nVersion, headerData.nBits, headerData.nTime);
                            #endif
                            ++blockCounter;
                        }
                        if (hashCounter >= nRoundsTarget)
                            break;
                        
                    }
                }
                if (headerData.nPostNonce == numHashesPost-1)
                    break;
                ++headerData.nPostNonce;
            }
        });
    }
    workerThreads->join();
}

bool sigma_context::verifyHeader(CBlockHeader headerData, uint64_t nBlockHeight)
{
    uint8_t* hashMem = new uint8_t[argonMemoryCostKb*1024];
    BOOST_SCOPE_EXIT(&hashMem) { delete[] hashMem; } BOOST_SCOPE_EXIT_END
    
    arith_uint256 hashTarget = arith_uint256().SetCompact(headerData.nBits);
    
    uint64_t nPostNonce = headerData.nPostNonce;
    uint64_t nPreNonce = headerData.nPreNonce;
    
    //1. Reset post nonce to zero and perform argon hash of header.
    headerData.nPostNonce = 0;
    
    argon2_echo_context argonContext;
    argonContext.t_cost = argonSlowHashRoundCost;
    argonContext.m_cost = argonMemoryCostKb;
    argonContext.allocated_memory = hashMem;
    argonContext.pwd = (uint8_t*)&headerData.nVersion;
    argonContext.pwdlen = 80;
    
    argonContext.lanes = numVerifyThreads;
    argonContext.threads = numUserVerifyThreads;

    if (argon2_echo_ctx(&argonContext, true) != ARGON2_OK)
        assert(0);
            
    //2. Set the initial state of the seed for the 'pseudo random' nonces.
    CryptoPP::ECB_Mode<CryptoPP::AES>::Encryption prng;
    prng.SetKey((const unsigned char*)&argonContext.outHash[0], 32);
    unsigned char ciphered[32];
                            
    //3. Advance PRNG to 'post' nonce (Mining calls PRNG once to prime - i.e. at index 0 we already call the PRNG twice - so add an extra call as well)
    for (uint64_t i=0; i<nPostNonce+2; ++i)
    {               
        prng.ProcessData((unsigned char*)&ciphered[0], (const unsigned char*)&argonContext.outHash[0], 32);
        memcpy(&argonContext.outHash[0], &ciphered[0], (size_t)32);
    }
    
    uint64_t nPseudoRandomNonce1 = (argonContext.outHash[0] ^ argonContext.outHash[1]) % numHashesPost;
    uint64_t nPseudoRandomNonce2 = (argonContext.outHash[2] ^ argonContext.outHash[3]) % numHashesPost;
    uint64_t nPseudoRandomAlg1 = (argonContext.outHash[0] ^ argonContext.outHash[3]) % 2;
    uint64_t nPseudoRandomAlg2 = (argonContext.outHash[1] ^ argonContext.outHash[2]) % 2;
    uint64_t nFastHashOffset1 = (argonContext.outHash[0] ^ argonContext.outHash[2])%(arenaChunkSizeBytes-fastHashSizeBytes);
    uint64_t nFastHashOffset2 = (argonContext.outHash[1] ^ argonContext.outHash[3])%(arenaChunkSizeBytes-fastHashSizeBytes);

    uint64_t nArenaMemoryIndex1 = (arenaChunkSizeBytes*nPseudoRandomNonce1) / (argonMemoryCostKb*1024);
    uint64_t nArenaMemoryOffset1 = (arenaChunkSizeBytes*nPseudoRandomNonce1) % (argonMemoryCostKb*1024);
    uint64_t nArenaMemoryIndex2 = (arenaChunkSizeBytes*nPseudoRandomNonce2) / (argonMemoryCostKb*1024);
    uint64_t nArenaMemoryOffset2 = (arenaChunkSizeBytes*nPseudoRandomNonce2) % (argonMemoryCostKb*1024);
    
    // Generate the part of the arena we need
    {
        headerData.nNonce = nBlockHeight+nArenaMemoryIndex1;
        argon2_echo_context context;
        context.t_cost = argonArenaRoundCost;
        context.m_cost = argonMemoryCostKb;
        context.allocated_memory = hashMem;
        context.pwd = (uint8_t*)&headerData.nVersion;
        context.pwdlen = 80;
               
        context.lanes = numVerifyThreads;
        context.threads = numUserVerifyThreads;            
        
        if (argon2_echo_ctx(&context, false) != ARGON2_OK)
            assert(0);
        
        uint256 outHash;
        headerData.nPreNonce = nPreNonce;
        headerData.nPostNonce = nPostNonce;
        sigmaRandomFastHash(nPseudoRandomAlg1, (uint8_t*)&headerData.nVersion, 80, (uint8_t*)&argonContext.outHash, 32,  &context.allocated_memory[nArenaMemoryOffset1+nFastHashOffset1], fastHashSizeBytes, outHash);
        
        if (UintToArith256(outHash) <= hashTarget)
        {
            headerData.nNonce = nBlockHeight+nArenaMemoryIndex2;
            if (argon2_echo_ctx(&context, false) != ARGON2_OK)
                assert(0);
            headerData.nPreNonce = nPreNonce;
            headerData.nPostNonce = nPostNonce;
            sigmaRandomFastHash(nPseudoRandomAlg2, (uint8_t*)&headerData.nVersion, 80, (uint8_t*)&argonContext.outHash, 32,  &context.allocated_memory[nArenaMemoryOffset2+nFastHashOffset2], fastHashSizeBytes, outHash);
            
            if (UintToArith256(outHash) <= hashTarget)
            {
                return true;
            }
        }
    }
    return false;
}


sigma_context::~sigma_context()
{
    free(arena);
}
