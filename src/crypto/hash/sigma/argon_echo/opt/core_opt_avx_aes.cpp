// Copyright (c) 2019-2022 The Centure developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the GNU Lesser General Public License v3, see the accompanying
// file COPYING

// This file is a thin wrapper around the actual 'argon2_echo_opt' implementation, along with various other similarly named files.
// The build system compiles each file with slightly different optimisation flags so that we have optimised implementations for a wide spread of processors.

#if defined(COMPILER_HAS_AVX) && defined(COMPILER_HAS_AES)

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "../blake2/blake2.h"
#include "../blake2/blamka-round-opt_sse3.h"
#include "../../../sigma/echo256/echo256_opt.h"
#include "../argon_echo.h"
#include "../core.h"

#define ARGON2_BLOCK_WORD_SIZE __m128i
#define ARGON2_BLOCK_WORD_COUNT ARGON2_OWORDS_IN_BLOCK
#define next_addresses     avx_aes_next_addresses
#define fill_segment       avx_aes_fill_segment
#define fill_block         avx_aes_fill_block
#define initialize         avx_aes_initialize
#define fill_memory_blocks avx_aes_fill_memory_blocks
#define Compress           avx_aes_argon2_echo_compress
#define init_block_value   avx_aes_argon2_echo_init_block_value
#define copy_block         avx_aes_argon2_echo_copy_block_value
#define xor_block          avx_aes_argon2_echo_xor_block_value
#define finalize           avx_aes_argon2_echo_finalize
#define index_alpha        avx_aes_argon2_echo_index_alpha
#define initial_hash       avx_aes_argon2_echo_initial_hash
#define fill_first_blocks  avx_aes_argon2_echo_fill_first_blocks

#define argon2_echo_ctx argon2_echo_ctx_avx_aes

#define ECHO_HASH_256(DATA, DATABYTELEN, HASH)                                        \
{                                                                                     \
    echo256_opt_hashState ctx_echo;                                                   \
    echo256_opt_avx_aes_Init(&ctx_echo);                                              \
    echo256_opt_avx_aes_Update(&ctx_echo, (const unsigned char*)(DATA), DATABYTELEN); \
    echo256_opt_avx_aes_Final(&ctx_echo, HASH);                                       \
}

//fixme: (SIGMA) (CBSU) - This is just a direct copy of the SSE3 version that we compile with AVX flags; it might theoretically be possible to speed this up with explicit AVX intrinsics.
static void avx_aes_fill_block(__m128i *state, const argon2_echo_block *ref_block, argon2_echo_block *next_block, int with_xor)
{
    __m128i block_XY[ARGON2_OWORDS_IN_BLOCK];
    unsigned int i;

    if (with_xor)
    {
        for (i = 0; i < ARGON2_OWORDS_IN_BLOCK; i++)
        {
            state[i] = _mm_xor_si128(state[i], _mm_loadu_si128((const __m128i *)ref_block->v + i));
            block_XY[i] = _mm_xor_si128(state[i], _mm_loadu_si128((const __m128i *)next_block->v + i));
        }
    }
    else
    {
        for (i = 0; i < ARGON2_OWORDS_IN_BLOCK; i++)
        {
            block_XY[i] = state[i] = _mm_xor_si128(state[i], _mm_loadu_si128((const __m128i *)ref_block->v + i));
        }
    }

    for (i = 0; i < 8; ++i)
    {
        BLAKE2_ROUND_SSE3(state[8 * i + 0], state[8 * i + 1], state[8 * i + 2], state[8 * i + 3], state[8 * i + 4], state[8 * i + 5], state[8 * i + 6], state[8 * i + 7]);
    }

    for (i = 0; i < 8; ++i)
    {
        BLAKE2_ROUND_SSE3(state[8 * 0 + i], state[8 * 1 + i], state[8 * 2 + i], state[8 * 3 + i], state[8 * 4 + i], state[8 * 5 + i], state[8 * 6 + i], state[8 * 7 + i]);
    }

    for (i = 0; i < ARGON2_OWORDS_IN_BLOCK; i++)
    {
        state[i] = _mm_xor_si128(state[i], block_XY[i]);
        _mm_storeu_si128((__m128i *)next_block->v + i, state[i]);
    }
}

#define USE_HARDWARE_AES
#define ARGON2_CORE_OPT_IMPL
#include "../core.cpp"
#include "../argon2.cpp"
#endif
