/*
 * Argon2 reference source code package - reference C implementations
 *
 * Copyright 2015
 * Daniel Dinu, Dmitry Khovratovich, Jean-Philippe Aumasson, and Samuel Neves
 *
 * You may use this work under the terms of a Creative Commons CC0 1.0
 * License/Waiver or the Apache Public License 2.0, at your option. The terms of
 * these licenses can be found at:
 *
 * - CC0 1.0 Universal : http://creativecommons.org/publicdomain/zero/1.0
 * - Apache 2.0        : http://www.apache.org/licenses/LICENSE-2.0
 *
 * You should have received a copy of both of these licenses along with this
 * software. If not, they may be obtained at the above URLs.
 */
// File contains modifications by: The Gulden developers
// All modifications:
// Copyright (c) 2019 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#include <compat/arch.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core.h"
#include "blake2/blake2.h"
#include "blake2/blake2-impl.h"
#include <pthread.h>
 
#include <crypto/hash/echo256/echo256_aesni.h>
#include <crypto/hash/sphlib/sph_echo.h>

#ifdef ARCH_CPU_X86_FAMILY
    #define FILL_SEGMENT_OPTIMISED(I, P)                                            \
    if (__builtin_cpu_supports("avx512f"))                                          \
    {                                                                               \
        fill_segment_avx512f(I, P);                                                 \
    }                                                                               \
    else if (__builtin_cpu_supports("avx2"))                                        \
    {                                                                               \
        fill_segment_avx2(I, P);                                                    \
    }                                                                               \
    else if (__builtin_cpu_supports("sse3"))                                        \
    {                                                                               \
        fill_segment_sse3(I, P);                                                    \
    }                                                                               \
    else if (__builtin_cpu_supports("sse2"))                                        \
    {                                                                               \
        fill_segment_sse2(I, P);                                                    \
    }                                                                               \
    else                                                                            \
    {                                                                               \
        fill_segment_ref(I, P);                                                     \
    }                                                                               \

    #define ECHO_HASH_256(DATA, DATABYTELEN, HASH)                                  \
    if (__builtin_cpu_supports("aes"))                                              \
    {                                                                               \
        echo256_aesni_hashState ctx_echo;                                           \
        echo256_aesni_Init(&ctx_echo);                                              \
        echo256_aesni_Update(&ctx_echo, (const unsigned char*)(DATA), DATABYTELEN); \
        echo256_aesni_Final(&ctx_echo, HASH);                                       \
    }                                                                               \
    else                                                                            \
    {                                                                               \
        sph_echo256_context ctx_echo;                                               \
        sph_echo256_init(&ctx_echo);                                                \
        sph_echo256(&ctx_echo, (const unsigned char*)(DATA), DATABYTELEN);          \
        sph_echo256_close(&ctx_echo, HASH);                                         \
    }                                                                               
#else
    #define FILL_SEGMENT_OPTIMISED(I, P)                                            \
    {fill_segment_ref(I, P);}                                                       \

    #define ECHO_HASH_256(DATA, DATABYTELEN, HASH)                                  \
    {                                                                               \
        sph_echo256_context ctx_echo;                                               \
        sph_echo256_init(&ctx_echo);                                                \
        sph_echo256(&ctx_echo, (const unsigned char*)(DATA), DATABYTELEN);          \
        sph_echo256_close(&ctx_echo, HASH);                                         \
    }  
#endif

/***************Instance and Position constructors**********/
void init_block_value(argon2_echo_block* b, uint8_t in)
{
    memset(b->v, in, sizeof(b->v));
}

void copy_block(argon2_echo_block* dst, const argon2_echo_block* src)
{
    memcpy(dst->v, src->v, sizeof(uint64_t)* ARGON2_QWORDS_IN_BLOCK);
}

void xor_block(argon2_echo_block* dst, const argon2_echo_block* src)
{
    int i;
    for (i = 0; i < ARGON2_QWORDS_IN_BLOCK; ++i)
    {
        dst->v[i] ^= src->v[i];
    }
}

static void load_block(argon2_echo_block* dst, const void* input)
{
    unsigned i;
    for (i = 0; i < ARGON2_QWORDS_IN_BLOCK; ++i)
    {
        dst->v[i] = load64((const uint8_t*)input + i * sizeof(dst->v[i]));
    }
}

void finalize(const argon2_echo_context* context, argon2_echo_instance_t* instance)
{
    if (context != NULL && instance != NULL)
    {
        /* Hash the entire memory to produce our final output hash */
        ECHO_HASH_256(instance->memory, instance->memory_blocks * ARGON2_BLOCK_SIZE, (unsigned char*)context->outHash);
    }
}

/*
* Pass 0:
*      This lane : all already finished segments plus already constructed
* blocks in this segment
*      Other lanes : all already finished segments
* Pass 1+:
*      This lane : (SYNC_POINTS - 1) last segments plus already constructed
* blocks in this segment
*      Other lanes : (SYNC_POINTS - 1) last segments
*/
uint32_t index_alpha(const argon2_echo_instance_t* instance, const argon2_echo_position_t* position, uint32_t pseudo_rand, int same_lane)
{   
    uint32_t reference_area_size;
    uint64_t relative_position;
    uint32_t start_position, absolute_position;

    if (0 == position->pass) /* First pass */
    {
        if (0 == position->slice) /* First slice */
        {
            reference_area_size = position->index - 1; /* all but the previous */
        }
        else
        {
            if (same_lane) /* The same lane => add current segment */
            {
                reference_area_size = position->slice * instance->segment_length + position->index - 1;
            }
            else
            {
                reference_area_size = position->slice * instance->segment_length + ((position->index == 0) ? (-1) : 0);
            }
        }
    }
    else /* Second pass */
    {   
        if (same_lane)
        {
            reference_area_size = instance->lane_length - instance->segment_length + position->index - 1;
        }
        else
        {
            reference_area_size = instance->lane_length - instance->segment_length + ((position->index == 0) ? (-1) : 0);
        }
    }

    /* 1.2.4. Mapping pseudo_rand to 0..<reference_area_size-1> and produce relative position */
    relative_position = pseudo_rand;
    relative_position = relative_position * relative_position >> 32;
    relative_position = reference_area_size - 1 - (reference_area_size * relative_position >> 32);

    /* 1.2.5 Computing starting position */
    start_position = 0;

    if (0 != position->pass)
    {
        start_position = (position->slice == ARGON2_SYNC_POINTS - 1) ? 0 : (position->slice + 1) * instance->segment_length;
    }

    /* 1.2.6. Computing absolute position */
    absolute_position = (start_position + relative_position) % instance->lane_length; /* absolute position */
    return absolute_position;
}

/* Single-threaded version for p=1 case */
static int fill_memory_blocks_st(argon2_echo_instance_t* instance)
{
    uint32_t r, s, l;

    for (r = 0; r < instance->passes; ++r)
    {
        for (s = 0; s < ARGON2_SYNC_POINTS; ++s)
        {
            for (l = 0; l < instance->lanes; ++l)
            {
                argon2_echo_position_t position = {r, l, (uint8_t)s, 0};
                FILL_SEGMENT_OPTIMISED(instance, position);
            }
        }
    }
    return ARGON2_OK;
}

static void* fill_segment_thr(void* thread_data)
{
    argon2_echo_thread_data* my_data = static_cast<argon2_echo_thread_data*>(thread_data);
    FILL_SEGMENT_OPTIMISED(my_data->instance_ptr, my_data->pos);
    pthread_exit(nullptr);
    return 0;
}

/* Multi-threaded version for p > 1 case */
static int fill_memory_blocks_mt(argon2_echo_instance_t* instance)
{
    uint32_t r, s;
    pthread_t* thread = nullptr;
    argon2_echo_thread_data* thr_data = nullptr;
    int rc = ARGON2_OK;

    /* 1. Allocating space for threads */
    thread = static_cast<pthread_t*>(calloc(instance->lanes, sizeof(pthread_t)));
    if (thread == NULL)
    {
        rc = ARGON2_MEMORY_ALLOCATION_ERROR;
        goto fail;
    }

    thr_data = static_cast<argon2_echo_thread_data*>(calloc(instance->lanes, sizeof(argon2_echo_thread_data)));
    if (thr_data == NULL)
    {
        rc = ARGON2_MEMORY_ALLOCATION_ERROR;
        goto fail;
    }

    for (r = 0; r < instance->passes; ++r)
    {
        for (s = 0; s < ARGON2_SYNC_POINTS; ++s)
        {
            uint32_t l, ll;

            /* 2. Calling threads */
            for (l = 0; l < instance->lanes; ++l)
            {
                argon2_echo_position_t position;

                /* 2.1 Join a thread if limit is exceeded */
                if (l >= instance->threads)
                {
                    if (pthread_join(thread[l - instance->threads], nullptr))
                    {
                        rc = ARGON2_THREAD_FAIL;
                        goto fail;
                    }
                }

                /* 2.2 Create thread */
                position.pass = r;
                position.lane = l;
                position.slice = (uint8_t)s;
                position.index = 0;
                thr_data[l].instance_ptr = instance; /* preparing the thread input */
                memcpy(&(thr_data[l].pos), &position, sizeof(argon2_echo_position_t));
                if (pthread_create(&thread[l], nullptr, &fill_segment_thr, (void*)&thr_data[l]))
                {
                    /* Wait for already running threads */
                    for (ll = 0; ll < l; ++ll)
                    {
                        pthread_join(thread[ll], nullptr);
                    }
                    rc = ARGON2_THREAD_FAIL;
                    goto fail;
                }

                /* FILL_SEGMENT_OPTIMISED(instance, position); */
                /*Non-thread equivalent of the lines above */
            }

            /* 3. Joining remaining threads */
            for (l = instance->lanes - instance->threads; l < instance->lanes; ++l)
            {
                if (pthread_join(thread[l], nullptr))
                {
                    rc = ARGON2_THREAD_FAIL;
                    goto fail;
                }
            }
        }
    }

fail:
    if (thread != NULL)
    {
        free(thread);
    }
    if (thr_data != NULL)
    {
        free(thr_data);
    }
    return rc;
}

int fill_memory_blocks(argon2_echo_instance_t* instance)
{
    if (instance == NULL || instance->lanes == 0)
    {
        return ARGON2_INCORRECT_PARAMETER;
    }
    return instance->threads == 1 ? fill_memory_blocks_st(instance) : fill_memory_blocks_mt(instance);
}

int validate_inputs(const argon2_echo_context* context)
{
    if (NULL == context)
    {
        return ARGON2_INCORRECT_PARAMETER;
    }

    /* Validate password (required param) */
    if (NULL == context->pwd)
    {
        if (0 != context->pwdlen)
        {
            return ARGON2_PWD_PTR_MISMATCH;
        }
    }

    if (ARGON2_MIN_PWD_LENGTH > context->pwdlen)
    {
        return ARGON2_PWD_TOO_SHORT;
    }

    if (ARGON2_MAX_PWD_LENGTH < context->pwdlen)
    {
        return ARGON2_PWD_TOO_LONG;
    }

    /* Validate memory cost */
    if (ARGON2_MIN_MEMORY > context->m_cost)
    {
        return ARGON2_MEMORY_TOO_LITTLE;
    }

    if (ARGON2_MAX_MEMORY < context->m_cost)
    {
        return ARGON2_MEMORY_TOO_MUCH;
    }

    if (context->m_cost < 8 * context->lanes)
    {
        return ARGON2_MEMORY_TOO_LITTLE;
    }

    /* Validate time cost */
    if (ARGON2_MIN_TIME > context->t_cost)
    {
        return ARGON2_TIME_TOO_SMALL;
    }

    if (ARGON2_MAX_TIME < context->t_cost)
    {
        return ARGON2_TIME_TOO_LARGE;
    }

    /* Validate lanes */
    if (ARGON2_MIN_LANES > context->lanes)
    {
        return ARGON2_LANES_TOO_FEW;
    }

    if (ARGON2_MAX_LANES < context->lanes)
    {
        return ARGON2_LANES_TOO_MANY;
    }

    /* Validate threads */
    if (ARGON2_MIN_THREADS > context->threads)
    {
        return ARGON2_THREADS_TOO_FEW;
    }

    if (ARGON2_MAX_THREADS < context->threads)
    {
        return ARGON2_THREADS_TOO_MANY;
    }

    return ARGON2_OK;
}

void fill_first_blocks(uint8_t* blockhash, const argon2_echo_instance_t* instance)
{
    uint32_t l;
    /* Make the first and second block in each lane as G(H0||0||i) or G(H0||1||i) */
    uint8_t blockhash_bytes[ARGON2_BLOCK_SIZE];
    for (l = 0; l < instance->lanes; ++l)
    {
        store32(blockhash + ARGON2_PREHASH_DIGEST_LENGTH, 0);
        store32(blockhash + ARGON2_PREHASH_DIGEST_LENGTH + 4, l);
        blake2b_long(blockhash_bytes, ARGON2_BLOCK_SIZE, blockhash, ARGON2_PREHASH_SEED_LENGTH);
        load_block(&instance->memory[l * instance->lane_length + 0], blockhash_bytes);

        store32(blockhash + ARGON2_PREHASH_DIGEST_LENGTH, 1);
        blake2b_long(blockhash_bytes, ARGON2_BLOCK_SIZE, blockhash, ARGON2_PREHASH_SEED_LENGTH);
        load_block(&instance->memory[l * instance->lane_length + 1], blockhash_bytes);
    }
    // NB! Ordinarily argon would erase the memory here to keep sensitive data out of memory.
    // For our use case (PoW) this is not necessary, as we are hashing a block header that is anyway public knowledge.
}

void initial_hash(uint8_t* blockhash, argon2_echo_context* context)
{
    if (NULL == context || NULL == blockhash)
    {
        return;
    }

    // NB! Ordinarily argon2 would produce a 64 bit blake hash of the various input paramaters here.
    // However we have stripped away most the paramaters we aren't using, and the remaining paramaters are all constant (m_cost, t_cost, pwdlen are all constant so no point hashing these)
    // So we perform a double echo-256 hash on the password instead to obtain the full 512 bits argon requires for initalisation.
    ECHO_HASH_256(context->pwd, context->pwdlen, blockhash);
    ECHO_HASH_256(blockhash, 32, blockhash+32);
}

int initialize(argon2_echo_instance_t* instance, argon2_echo_context* context)
{
    uint8_t blockhash[ARGON2_PREHASH_SEED_LENGTH];

    if (instance == NULL || context == NULL)
    {
        return ARGON2_INCORRECT_PARAMETER;
    }
    instance->context_ptr = context;

    /* 1. Memory allocation */
    // Ordinarily argon would allocate memory here, but for our implementation we let the caller pre-allocate instead so we just grab out memory from 'allocated_memory'
    instance->memory = (argon2_echo_block*)context->allocated_memory;

    /* 2. Initial hashing */
    /* H_0 + 8 extra bytes to produce the first blocks */
    /* uint8_t blockhash[ARGON2_PREHASH_SEED_LENGTH]; */
    /* Hashing all inputs */
    initial_hash(blockhash, context);
    /* Zeroing 8 extra bytes */
    memset(blockhash + ARGON2_PREHASH_DIGEST_LENGTH, 0, ARGON2_PREHASH_SEED_LENGTH - ARGON2_PREHASH_DIGEST_LENGTH);

    /* 3. Creating first blocks, we always have at least two blocks in a slice */
    fill_first_blocks(blockhash, instance);
    
    /* Clearing the hash */
    // NB! Ordinarily argon would erase the memory here to keep sensitive data out of memory.
    // For our use case (PoW) this is not necessary, as we are hashing a block header that is anyway public knowledge.
    return ARGON2_OK;
}
