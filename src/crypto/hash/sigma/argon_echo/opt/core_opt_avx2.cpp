// Copyright (c) 2019-2022 The Centure developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the GNU Lesser General Public License v3, see the accompanying
// file COPYING

// This file is a thin wrapper around the actual 'argon2_echo_opt' implementation, along with various other similarly named files.
// The build system compiles each file with slightly different optimisation flags so that we have optimised implementations for a wide spread of processors.

#if defined(COMPILER_HAS_AVX2)

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "../blake2/blake2.h"
#include "../blake2/blamka-round-opt_avx2.h"
#include "../../../sigma/echo256/echo256_opt.h"
#include "../argon_echo.h"
#include "../core.h"

#define ARGON2_BLOCK_WORD_SIZE __m256i
#define ARGON2_BLOCK_WORD_COUNT ARGON2_HWORDS_IN_BLOCK
#define next_addresses     avx2_next_addresses
#define fill_segment       avx2_fill_segment
#define fill_block         avx2_fill_block
#define initialize         avx2_initialize
#define fill_memory_blocks avx2_fill_memory_blocks
#define Compress           avx2_argon2_echo_compress
#define init_block_value   avx2_argon2_echo_init_block_value
#define copy_block         avx2_argon2_echo_copy_block_value
#define xor_block          avx2_argon2_echo_xor_block_value
#define finalize           avx2_argon2_echo_finalize
#define index_alpha        avx2_argon2_echo_index_alpha
#define initial_hash       avx2_argon2_echo_initial_hash
#define fill_first_blocks  avx2_argon2_echo_fill_first_blocks

#define argon2_echo_ctx argon2_echo_ctx_avx2

#define ECHO_HASH_256(DATA, DATABYTELEN, HASH)                                     \
{                                                                                  \
    echo256_opt_hashState ctx_echo;                                                \
    echo256_opt_avx2_Init(&ctx_echo);                                              \
    echo256_opt_avx2_Update(&ctx_echo, (const unsigned char*)(DATA), DATABYTELEN); \
    echo256_opt_avx2_Final(&ctx_echo, HASH);                                       \
}

static void avx2_fill_block(__m256i* state, const argon2_echo_block* ref_block, argon2_echo_block* next_block, int with_xor)
{
    __m256i block_XY[ARGON2_HWORDS_IN_BLOCK];
    unsigned int i;

    if (with_xor)
    {
        for (i = 0; i < ARGON2_HWORDS_IN_BLOCK; i++)
        {
            state[i] = _mm256_xor_si256(state[i], _mm256_loadu_si256((const __m256i *)ref_block->v + i));
            block_XY[i] = _mm256_xor_si256(state[i], _mm256_loadu_si256((const __m256i *)next_block->v + i));
        }
    }
    else
    {
        for (i = 0; i < ARGON2_HWORDS_IN_BLOCK; i++)
        {
            block_XY[i] = state[i] = _mm256_xor_si256(state[i], _mm256_loadu_si256((const __m256i *)ref_block->v + i));
        }
    }

    for (i = 0; i < 4; ++i)
    {
        BLAKE2_ROUND_AVX2_1(state[8 * i + 0], state[8 * i + 4], state[8 * i + 1], state[8 * i + 5], state[8 * i + 2], state[8 * i + 6], state[8 * i + 3], state[8 * i + 7]);
    }

    for (i = 0; i < 4; ++i)
    {
        BLAKE2_ROUND_AVX2_2(state[ 0 + i], state[ 4 + i], state[ 8 + i], state[12 + i], state[16 + i], state[20 + i], state[24 + i], state[28 + i]);
    }

    for (i = 0; i < ARGON2_HWORDS_IN_BLOCK; i++)
    {
        state[i] = _mm256_xor_si256(state[i], block_XY[i]);
        _mm256_storeu_si256((__m256i *)next_block->v + i, state[i]);
    }
}

#define ARGON2_CORE_OPT_IMPL
#include "../core.cpp"
#include "../argon2.cpp"
#endif
