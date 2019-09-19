// Copyright (c) 2019 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#include "sigma.h"

#include <compat/arch.h>
#include "util.h"

#include <cryptopp/config.h>
#include <cryptopp/aes.h>
#include <cryptopp/modes.h>

#include <stddef.h>
#include <limits.h>
#include <stdlib.h>

#include <boost/scope_exit.hpp>

#include <crypto/hash/sigma/argon_echo/argon_echo.h>
#include <crypto/hash/echo256/sphlib/sph_echo.h>

#include <crypto/hash/echo256/echo256_opt.h>
#include <crypto/hash/shavite3_256/shavite3_256_opt.h>
#include <crypto/hash/shavite3_256/ref/shavite3_ref.h>

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

sigma_settings::sigma_settings(uint32_t argonArenaRoundCost_, uint32_t argonSlowHashRoundCost_, uint64_t argonMemoryCostKb_, uint64_t arena_size_kb, uint64_t numHashesPre_, uint64_t numHashesPost_, uint64_t numVerifyThreads_, uint64_t fastHashSizeBytes_)
: numVerifyThreads(numVerifyThreads_)
, arenaSizeKb(arena_size_kb)
, argonMemoryCostKb(argonMemoryCostKb_)
, argonArenaRoundCost(argonArenaRoundCost_)
, argonSlowHashRoundCost(argonSlowHashRoundCost_)
, numHashesPre(numHashesPre_)
, numHashesPost(numHashesPost_)
, fastHashSizeBytes(fastHashSizeBytes_)
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

void sigma_context::prepareArenas(CBlockHeader& headerData, uint64_t nBlockHeight)
{
    headerBlockHeight = nBlockHeight;
    workerThreads = new boost::asio::thread_pool(numThreads);

    int numHashes = allocatedArenaSizeKb/settings.argonMemoryCostKb;
    
    for (int i=0;i<numHashes;++i)
    {
        boost::asio::post(*workerThreads, [=]() mutable
        {
            headerData.nNonce = headerBlockHeight+i;
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

void sigma_context::benchmarkMining(CBlockHeader& headerData, std::atomic<uint64_t>& slowHashCounter, std::atomic<uint64_t>& halfHashCounter, std::atomic<uint64_t>& skippedHashCounter, std::atomic<uint64_t>&hashCounter, std::atomic<uint64_t>&blockCounter, uint64_t nRoundsTarget)
{
    arith_uint256 hashTarget = arith_uint256().SetCompact(headerData.nBits);
    
    std::atomic<uint16_t> nPreNonce=0;
    
    workerThreads = new boost::asio::thread_pool(numThreads);
    BOOST_SCOPE_EXIT(&workerThreads) { delete workerThreads; } BOOST_SCOPE_EXIT_END
    
    for (uint64_t nIndex = 0; nIndex <= settings.numHashesPre;++nIndex)
    {
        boost::asio::post(*workerThreads, [&,headerData]() mutable
        {
            if (hashCounter >= nRoundsTarget)
                return;

            //1. Select pre nonce, reset post nonce to zero and perform argon hash of header.
            uint8_t* hashMem = new uint8_t[settings.argonMemoryCostKb*1024];
            BOOST_SCOPE_EXIT(&hashMem) { delete[] hashMem; } BOOST_SCOPE_EXIT_END
            headerData.nNonce = headerBlockHeight;
            headerData.nPreNonce = nPreNonce++;
            headerData.nPostNonce = 0;
            
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
                            
            //3. Iterate through all 'post' nonce combinations, calculating 2 hashes from the global memory.
            //The input of one is determined by the 'post' nonce, while the second is determined by the 'pseudo random' nonce, forcing random memory access.
            while(true)
            {
                if (UNLIKELY(hashCounter >= nRoundsTarget))
                    break;
                
                //3.1. For each iteration advance the state of the pseudo random nonce
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

                    //3.2 Calculate first hash
                    uint256 fastHash;
                    sigmaRandomFastHash(nPseudoRandomAlg1, (uint8_t*)&headerData.nVersion, 80, (uint8_t*)&argonContext.outHash, 32,  &arena[(nPseudoRandomNonce1*settings.arenaChunkSizeBytes)+nFastHashOffset1], settings.fastHashSizeBytes, fastHash);
                    ++halfHashCounter;
                    
                    //3.3 Evaluate first hash (short circuit evaluation)
                    if (UNLIKELY(UintToArith256(fastHash) <= hashTarget))
                    {
                        //3.4 Calculate second hash
                        sigmaRandomFastHash(nPseudoRandomAlg2, (uint8_t*)&headerData.nVersion, 80, (uint8_t*)&argonContext.outHash, 32,  &arena[(nPseudoRandomNonce2*settings.arenaChunkSizeBytes)+nFastHashOffset2], settings.fastHashSizeBytes, fastHash);
                        ++hashCounter;

                        //3.5 See if we have a valid block
                        if (UNLIKELY(UintToArith256(fastHash) <= hashTarget))
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

bool sigma_verify_context::verifyHeader(CBlockHeader headerData, uint64_t nBlockHeight)
{    
    arith_uint256 hashTarget = arith_uint256().SetCompact(headerData.nBits);
    
    uint64_t nPostNonce = headerData.nPostNonce;
    uint64_t nPreNonce = headerData.nPreNonce;
    
    //1. Reset post nonce to zero and perform argon hash of header.
    headerData.nPostNonce = 0;
    
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

    //2. Set the initial state of the seed for the 'pseudo random' nonces.
    CryptoPP::ECB_Mode<CryptoPP::AES>::Encryption prng;
    prng.SetKey((const unsigned char*)&argonContext.outHash[0], 32);
    unsigned char ciphered[32];
                            
    //3. Advance PRNG to 'post' nonce 
    for (uint64_t i=0; i<=nPostNonce; ++i)
    {               
        prng.ProcessData((unsigned char*)&ciphered[0], (const unsigned char*)&argonContext.outHash[0], 32);
        memcpy(&argonContext.outHash[0], &ciphered[0], (size_t)32);
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
    
    // Generate the part of the arena we need
    {
        headerData.nNonce = nBlockHeight+nArenaMemoryIndex1;
        argonContext.t_cost = settings.argonArenaRoundCost;    
        
        if (selected_argon2_echo_hash(&argonContext, false) != ARGON2_OK)
            assert(0);
        
        uint256 fastHash;
        headerData.nPreNonce = nPreNonce;
        headerData.nPostNonce = nPostNonce;
        sigmaRandomFastHash(nPseudoRandomAlg1, (uint8_t*)&headerData.nVersion, 80, (uint8_t*)slowHash.begin(), 32,  &argonContext.allocated_memory[nArenaMemoryOffset1+nFastHashOffset1], settings.fastHashSizeBytes, fastHash);

        if (UintToArith256(fastHash) <= hashTarget)
        {
            headerData.nNonce = nBlockHeight+nArenaMemoryIndex2;
            if (selected_argon2_echo_hash(&argonContext, false) != ARGON2_OK)
                assert(0);
            headerData.nPreNonce = nPreNonce;
            headerData.nPostNonce = nPostNonce;
            sigmaRandomFastHash(nPseudoRandomAlg2, (uint8_t*)&headerData.nVersion, 80, (uint8_t*)slowHash.begin(), 32,  &argonContext.allocated_memory[nArenaMemoryOffset2+nFastHashOffset2], settings.fastHashSizeBytes, fastHash);
            
            if (UintToArith256(fastHash) <= hashTarget)
            {
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
