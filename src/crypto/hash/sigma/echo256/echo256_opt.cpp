/*
 * file        : echo_vperm.c
 * version     : 1.0.208
 * date        : 14.12.2010
 * 
 * - vperm and aes_ni implementations of hash function ECHO
 * - implements NIST hash api
 * - assumes that message length is multiple of 8-bits
 * - _ECHO_VPERM_ must be defined if compiling with ../main.c
 * -  define NO_AES_NI for aes_ni version
 *
 * Cagdas Calik
 * ccalik@metu.edu.tr
 * Institute of Applied Mathematics, Middle East Technical University, Turkey.
 *
 */
// File contains modifications by: The Centure developers
// All modifications:
// Copyright (c) 2019-2022 The Centure developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the GNU Lesser General Public License v3, see the accompanying
// file COPYING

#include <stdint.h>
#include "echo256_opt.h"

#ifndef USE_HARDWARE_AES
#define _mm_aesenc_si128 _mm_aesenc_si128_sw
#endif

#ifdef ECHO256_OPT_IMPL

#include <memory.h>
#define M128(x) *((__m128i*)x)

__attribute__((aligned(16))) const unsigned int const1[]        = {0x00000001, 0x00000000, 0x00000000, 0x00000000};
__attribute__((aligned(16))) const unsigned int mul2mask[]      = {0x00001b00, 0x00000000, 0x00000000, 0x00000000};
__attribute__((aligned(16))) const unsigned int lsbmask[]       = {0x01010101, 0x01010101, 0x01010101, 0x01010101};

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#define ECHO_SUBBYTES(state, i, j) \
    state[i][j] = _mm_aesenc_si128(state[i][j], k1);\
    state[i][j] = _mm_aesenc_si128(state[i][j], _mm_setzero_si128());\
    k1 = _mm_add_epi32(k1, M128(const1))

#define ECHO_MIXBYTES(state1, state2, j, t1, t2, s2) \
    s2 = _mm_add_epi8(state1[0][j], state1[0][j]);\
    t1 = _mm_srli_epi16(state1[0][j], 7);\
    t1 = _mm_and_si128(t1, M128(lsbmask));\
    t2 = _mm_shuffle_epi8(M128(mul2mask), t1);\
    s2 = _mm_xor_si128(s2, t2);\
    state2[0][j] = s2;\
    state2[1][j] = state1[0][j];\
    state2[2][j] = state1[0][j];\
    state2[3][j] = _mm_xor_si128(s2, state1[0][j]);\
    s2 = _mm_add_epi8(state1[1][(j + 1) & 3], state1[1][(j + 1) & 3]);\
    t1 = _mm_srli_epi16(state1[1][(j + 1) & 3], 7);\
    t1 = _mm_and_si128(t1, M128(lsbmask));\
    t2 = _mm_shuffle_epi8(M128(mul2mask), t1);\
    s2 = _mm_xor_si128(s2, t2);\
    state2[0][j] = _mm_xor_si128(state2[0][j], _mm_xor_si128(s2, state1[1][(j + 1) & 3]));\
    state2[1][j] = _mm_xor_si128(state2[1][j], s2);\
    state2[2][j] = _mm_xor_si128(state2[2][j], state1[1][(j + 1) & 3]);\
    state2[3][j] = _mm_xor_si128(state2[3][j], state1[1][(j + 1) & 3]);\
    s2 = _mm_add_epi8(state1[2][(j + 2) & 3], state1[2][(j + 2) & 3]);\
    t1 = _mm_srli_epi16(state1[2][(j + 2) & 3], 7);\
    t1 = _mm_and_si128(t1, M128(lsbmask));\
    t2 = _mm_shuffle_epi8(M128(mul2mask), t1);\
    s2 = _mm_xor_si128(s2, t2);\
    state2[0][j] = _mm_xor_si128(state2[0][j], state1[2][(j + 2) & 3]);\
    state2[1][j] = _mm_xor_si128(state2[1][j], _mm_xor_si128(s2, state1[2][(j + 2) & 3]));\
    state2[2][j] = _mm_xor_si128(state2[2][j], s2);\
    state2[3][j] = _mm_xor_si128(state2[3][j], state1[2][(j + 2) & 3]);\
    s2 = _mm_add_epi8(state1[3][(j + 3) & 3], state1[3][(j + 3) & 3]);\
    t1 = _mm_srli_epi16(state1[3][(j + 3) & 3], 7);\
    t1 = _mm_and_si128(t1, M128(lsbmask));\
    t2 = _mm_shuffle_epi8(M128(mul2mask), t1);\
    s2 = _mm_xor_si128(s2, t2);\
    state2[0][j] = _mm_xor_si128(state2[0][j], state1[3][(j + 3) & 3]);\
    state2[1][j] = _mm_xor_si128(state2[1][j], state1[3][(j + 3) & 3]);\
    state2[2][j] = _mm_xor_si128(state2[2][j], _mm_xor_si128(s2, state1[3][(j + 3) & 3]));\
    state2[3][j] = _mm_xor_si128(state2[3][j], s2)


#define ECHO_ROUND_UNROLL2 \
    ECHO_SUBBYTES(_state, 0, 0);\
    ECHO_SUBBYTES(_state, 1, 0);\
    ECHO_SUBBYTES(_state, 2, 0);\
    ECHO_SUBBYTES(_state, 3, 0);\
    ECHO_SUBBYTES(_state, 0, 1);\
    ECHO_SUBBYTES(_state, 1, 1);\
    ECHO_SUBBYTES(_state, 2, 1);\
    ECHO_SUBBYTES(_state, 3, 1);\
    ECHO_SUBBYTES(_state, 0, 2);\
    ECHO_SUBBYTES(_state, 1, 2);\
    ECHO_SUBBYTES(_state, 2, 2);\
    ECHO_SUBBYTES(_state, 3, 2);\
    ECHO_SUBBYTES(_state, 0, 3);\
    ECHO_SUBBYTES(_state, 1, 3);\
    ECHO_SUBBYTES(_state, 2, 3);\
    ECHO_SUBBYTES(_state, 3, 3);\
    ECHO_MIXBYTES(_state, _state2, 0, t1, t2, s2);\
    ECHO_MIXBYTES(_state, _state2, 1, t1, t2, s2);\
    ECHO_MIXBYTES(_state, _state2, 2, t1, t2, s2);\
    ECHO_MIXBYTES(_state, _state2, 3, t1, t2, s2);\
    ECHO_SUBBYTES(_state2, 0, 0);\
    ECHO_SUBBYTES(_state2, 1, 0);\
    ECHO_SUBBYTES(_state2, 2, 0);\
    ECHO_SUBBYTES(_state2, 3, 0);\
    ECHO_SUBBYTES(_state2, 0, 1);\
    ECHO_SUBBYTES(_state2, 1, 1);\
    ECHO_SUBBYTES(_state2, 2, 1);\
    ECHO_SUBBYTES(_state2, 3, 1);\
    ECHO_SUBBYTES(_state2, 0, 2);\
    ECHO_SUBBYTES(_state2, 1, 2);\
    ECHO_SUBBYTES(_state2, 2, 2);\
    ECHO_SUBBYTES(_state2, 3, 2);\
    ECHO_SUBBYTES(_state2, 0, 3);\
    ECHO_SUBBYTES(_state2, 1, 3);\
    ECHO_SUBBYTES(_state2, 2, 3);\
    ECHO_SUBBYTES(_state2, 3, 3);\
    ECHO_MIXBYTES(_state2, _state, 0, t1, t2, s2);\
    ECHO_MIXBYTES(_state2, _state, 1, t1, t2, s2);\
    ECHO_MIXBYTES(_state2, _state, 2, t1, t2, s2);\
    ECHO_MIXBYTES(_state2, _state, 3, t1, t2, s2)



#define SAVESTATE(dst, src)\
    dst[0][0] = src[0][0];\
    dst[0][1] = src[0][1];\
    dst[0][2] = src[0][2];\
    dst[0][3] = src[0][3];\
    dst[1][0] = src[1][0];\
    dst[1][1] = src[1][1];\
    dst[1][2] = src[1][2];\
    dst[1][3] = src[1][3];\
    dst[2][0] = src[2][0];\
    dst[2][1] = src[2][1];\
    dst[2][2] = src[2][2];\
    dst[2][3] = src[2][3];\
    dst[3][0] = src[3][0];\
    dst[3][1] = src[3][1];\
    dst[3][2] = src[3][2];\
    dst[3][3] = src[3][3]



void Compress(echo256_opt_hashState* ctx, const unsigned char* pmsg, unsigned int uBlockCount)
{    
    unsigned int r, b, i, j;
    __m128i t1, t2, s2, k1;
    __m128i _state[4][4], _state2[4][4], _statebackup[4][4]; 

    for(i = 0; i < 4; i++)
    {
        _state[i][0] = ctx->state[i][0];
    }

    for(b = 0; b < uBlockCount; b++)
    {
        ctx->k = _mm_add_epi64(ctx->k, ctx->const1536);

        // load message
        for(j = 1; j < 4; j++)
        {
            for(i = 0; i < 4; i++)
            {
                _state[i][j] = _mm_loadu_si128((__m128i*)pmsg + 4 * (j - 1) + i);
            }
        }

        // save state
        SAVESTATE(_statebackup, _state);

        k1 = ctx->k;

        for(r = 0; r < ctx->uRounds / 2; r++)
        {
            ECHO_ROUND_UNROLL2;
        }

        for(i = 0; i < 4; i++)
        {
            _state[i][0] = _mm_xor_si128(_state[i][0], _state[i][1]);
            _state[i][0] = _mm_xor_si128(_state[i][0], _state[i][2]);
            _state[i][0] = _mm_xor_si128(_state[i][0], _state[i][3]);
            _state[i][0] = _mm_xor_si128(_state[i][0], _statebackup[i][0]);
            _state[i][0] = _mm_xor_si128(_state[i][0], _statebackup[i][1]);
            _state[i][0] = _mm_xor_si128(_state[i][0], _statebackup[i][2]);
            _state[i][0] = _mm_xor_si128(_state[i][0], _statebackup[i][3]);
        }
        pmsg += ctx->uBlockLength;
    }
    SAVESTATE(ctx->state, _state);
}
#pragma GCC diagnostic pop


__attribute__((aligned(16))) const unsigned int constinit1[] = {0x00000100, 0x00000000, 0x00000000, 0x00000000};
__attribute__((aligned(16))) const unsigned int constinit2[] = {0x00000600, 0x00000000, 0x00000000, 0x00000000};

HashReturn echo256_opt_Init(echo256_opt_hashState *ctx)
{
    int i, j;

    ctx->k = _mm_setzero_si128(); 
    ctx->processed_bits = 0;
    ctx->uBufferBytes = 0;

    ctx->uHashSize = 256;
    ctx->uBlockLength = 192;
    ctx->uRounds = 8;
    ctx->hashsize = _mm_loadu_si128((__m128i*)constinit1);
    ctx->const1536 = _mm_loadu_si128((__m128i*)constinit2);

    for(i = 0; i < 4; i++)
    {
        ctx->state[i][0] = ctx->hashsize;
    }

    for(i = 0; i < 4; i++)
    {
        for(j = 1; j < 4; j++)
        {
            ctx->state[i][j] = _mm_setzero_si128();
        }
    }
    return SUCCESS;
}

HashReturn echo256_opt_Update(echo256_opt_hashState* state, const unsigned char* data, uint64_t dataByteLength)
{
    unsigned int uBlockCount, uRemainingBytes;

    if((state->uBufferBytes + dataByteLength) >= state->uBlockLength)
    {
        if(state->uBufferBytes != 0)
        {
            // Fill the buffer
            memcpy(state->buffer + state->uBufferBytes, (void*)data, state->uBlockLength - state->uBufferBytes);

            // Process buffer
            Compress(state, state->buffer, 1);
            state->processed_bits += state->uBlockLength * 8;

            data += state->uBlockLength - state->uBufferBytes;
            dataByteLength -= state->uBlockLength - state->uBufferBytes;
        }

        // buffer now does not contain any unprocessed bytes

        uBlockCount = dataByteLength / state->uBlockLength;
        uRemainingBytes = dataByteLength % state->uBlockLength;

        if(uBlockCount > 0)
        {
            Compress(state, data, uBlockCount);

            state->processed_bits += uBlockCount * state->uBlockLength * 8;
            data += uBlockCount * state->uBlockLength;
        }

        if(uRemainingBytes > 0)
        {
            memcpy(state->buffer, (void*)data, uRemainingBytes);
        }

        state->uBufferBytes = uRemainingBytes;
    }
    else
    {
        memcpy(state->buffer + state->uBufferBytes, (void*)data, dataByteLength);
        state->uBufferBytes += dataByteLength;
    }

    return SUCCESS;
}

HashReturn echo256_opt_Final(echo256_opt_hashState* state, unsigned char* hashval)
{
    __m128i remainingbits;

    // Add remaining bytes in the buffer
    state->processed_bits += state->uBufferBytes * 8;

    __attribute__((aligned(16))) const unsigned int load_buffer_bytes[] = {state->uBufferBytes * 8, 0x00000000, 0x00000000, 0x00000000};
    remainingbits = _mm_loadu_si128((__m128i*)load_buffer_bytes);

    // Pad with 0x80
    state->buffer[state->uBufferBytes++] = 0x80;

    // Enough buffer space for padding in this block?
    if((state->uBlockLength - state->uBufferBytes) >= 18)
    {
        // Pad with zeros
        memset(state->buffer + state->uBufferBytes, 0, state->uBlockLength - (state->uBufferBytes + 18));

        // Hash size
        *((unsigned short*)(state->buffer + state->uBlockLength - 18)) = state->uHashSize;

        // Processed bits
        *((uint64_t*)(state->buffer + state->uBlockLength - 16)) = state->processed_bits;
        *((uint64_t*)(state->buffer + state->uBlockLength - 8)) = 0;

        // Last block contains message bits?
        if(state->uBufferBytes == 1)
        {
            state->k = _mm_xor_si128(state->k, state->k);
            state->k = _mm_sub_epi64(state->k, state->const1536);
        }
        else
        {
            state->k = _mm_add_epi64(state->k, remainingbits);
            state->k = _mm_sub_epi64(state->k, state->const1536);
        }

        // Compress
        Compress(state, state->buffer, 1);
    }
    else
    {
        // Fill with zero and compress
        memset(state->buffer + state->uBufferBytes, 0, state->uBlockLength - state->uBufferBytes);
        state->k = _mm_add_epi64(state->k, remainingbits);
        state->k = _mm_sub_epi64(state->k, state->const1536);
        Compress(state, state->buffer, 1);

        // Last block
        memset(state->buffer, 0, state->uBlockLength - 18);

        // Hash size
        *((unsigned short*)(state->buffer + state->uBlockLength - 18)) = state->uHashSize;

        // Processed bits
        *((uint64_t*)(state->buffer + state->uBlockLength - 16)) = state->processed_bits;
        *((uint64_t*)(state->buffer + state->uBlockLength - 8)) = 0;

        // Compress the last block
        state->k = _mm_xor_si128(state->k, state->k);
        state->k = _mm_sub_epi64(state->k, state->const1536);
        Compress(state, state->buffer, 1);
    }

    // Store the hash value
    _mm_storeu_si128((__m128i*)hashval + 0, state->state[0][0]);
    _mm_storeu_si128((__m128i*)hashval + 1, state->state[1][0]);

    return SUCCESS;
}

HashReturn echo256_opt_UpdateFinal( echo256_opt_hashState* state, unsigned char* hashval, const unsigned char* data, uint64_t dataByteLength )
{
    unsigned int uBlockCount, uRemainingBytes;

    if( (state->uBufferBytes + dataByteLength) >= state->uBlockLength )
    {
        if( state->uBufferBytes != 0 )
        {
            // Fill the buffer
            memcpy( state->buffer + state->uBufferBytes, (void*)data, state->uBlockLength - state->uBufferBytes );

            // Process buffer
            Compress( state, state->buffer, 1 );
            state->processed_bits += state->uBlockLength * 8;

            data += state->uBlockLength - state->uBufferBytes;
            dataByteLength -= state->uBlockLength - state->uBufferBytes;
        }

        // buffer now does not contain any unprocessed bytes

        uBlockCount = dataByteLength / state->uBlockLength;
        uRemainingBytes = dataByteLength % state->uBlockLength;

        if( uBlockCount > 0 )
        {
            Compress( state, data, uBlockCount );
            state->processed_bits += uBlockCount * state->uBlockLength * 8;
            data += uBlockCount * state->uBlockLength;
        }

        if( uRemainingBytes > 0 )
        memcpy(state->buffer, (void*)data, uRemainingBytes);

        state->uBufferBytes = uRemainingBytes;
    }
    else
    {
        memcpy( state->buffer + state->uBufferBytes, (void*)data, dataByteLength );
        state->uBufferBytes += dataByteLength;
    }

    __m128i remainingbits;

    // Add remaining bytes in the buffer
    state->processed_bits += state->uBufferBytes * 8;

    __attribute__((aligned(16))) const unsigned int load_buffer_bytes[] = {state->uBufferBytes * 8, 0x00000000, 0x00000000, 0x00000000};
    remainingbits = _mm_loadu_si128((__m128i*)load_buffer_bytes);

    // Pad with 0x80
    state->buffer[state->uBufferBytes++] = 0x80;
    // Enough buffer space for padding in this block?
    if( (state->uBlockLength - state->uBufferBytes) >= 18 )
    {
        // Pad with zeros
        memset( state->buffer + state->uBufferBytes, 0, state->uBlockLength - (state->uBufferBytes + 18) );

        // Hash size
        *( (unsigned short*)(state->buffer + state->uBlockLength - 18) ) = state->uHashSize;

        // Processed bits
        *( (uint64_t*)(state->buffer + state->uBlockLength - 16) ) = state->processed_bits;
        *( (uint64_t*)(state->buffer + state->uBlockLength - 8) ) = 0;

        // Last block contains message bits?
        if( state->uBufferBytes == 1 )
        {
            state->k = _mm_xor_si128( state->k, state->k );
            state->k = _mm_sub_epi64( state->k, state->const1536 );
        }
        else
        {
            state->k = _mm_add_epi64( state->k, remainingbits );
            state->k = _mm_sub_epi64( state->k, state->const1536 );
        }

        // Compress
        Compress( state, state->buffer, 1 );
    }
    else
    {
        // Fill with zero and compress
        memset( state->buffer + state->uBufferBytes, 0, state->uBlockLength - state->uBufferBytes );
        state->k = _mm_add_epi64( state->k, remainingbits );
        state->k = _mm_sub_epi64( state->k, state->const1536 );
        Compress( state, state->buffer, 1 );

        // Last block
        memset( state->buffer, 0, state->uBlockLength - 18 );

        // Hash size
        *( (unsigned short*)(state->buffer + state->uBlockLength - 18) ) = state->uHashSize;

        // Processed bits
        *( (uint64_t*)(state->buffer + state->uBlockLength - 16) ) = state->processed_bits;
        *( (uint64_t*)(state->buffer + state->uBlockLength - 8) ) = 0;
        // Compress the last block
        state->k = _mm_xor_si128( state->k, state->k );
        state->k = _mm_sub_epi64( state->k, state->const1536 );
        Compress( state, state->buffer, 1) ;
    }

    // Store the hash value
    _mm_storeu_si128( (__m128i*)hashval + 0, state->state[0][0] );
    _mm_storeu_si128( (__m128i*)hashval + 1, state->state[1][0] );

    return SUCCESS;
}
#endif
