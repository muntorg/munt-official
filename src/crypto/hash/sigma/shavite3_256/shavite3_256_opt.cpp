// File originates from the supercop project
// Authors: Eli Biham and Orr Dunkelman
//
// File contains modifications by: The Centure developers
// All modifications:
// Copyright (c) 2019-2022 The Centure developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the Libre Chain License, see the accompanying
// file COPYING


#include "shavite3_256_opt.h"

#ifndef USE_HARDWARE_AES
#define _mm_aesenc_si128 _mm_aesenc_si128_sw
#endif

#ifdef SHAVITE3_256_OPT_IMPL

#include "compat.h"
#include "assert.h"
#include <memory.h>

#define T8(x) ((x) & 0xff)

/// Encrypts the plaintext pt[] using the key message[], and counter[], to produce the ciphertext ct[]


#define SHAVITE_MIXING_256_OPT   \
    x11 = x15;                   \
    x10 = x14;                   \
    x9 =  x13;                   \
    x8 = x12;                    \
                                 \
    x6 = x11;                    \
    x6 = _mm_srli_si128(x6, 4);  \
    x8 = _mm_xor_si128(x8,  x6); \
    x6 = x8;                     \
    x6 = _mm_slli_si128(x6,  12);\
    x8 = _mm_xor_si128(x8, x6);  \
                                 \
    x7 = x8;                     \
    x7 =  _mm_srli_si128(x7,  4);\
    x9 = _mm_xor_si128(x9,  x7); \
    x7 = x9;                     \
    x7 = _mm_slli_si128(x7, 12); \
    x9 = _mm_xor_si128(x9, x7);  \
                                 \
    x6 = x9;                     \
    x6 =  _mm_srli_si128(x6, 4); \
    x10 = _mm_xor_si128(x10, x6);\
    x6 = x10;                    \
    x6 = _mm_slli_si128(x6,  12);\
    x10 = _mm_xor_si128(x10, x6);\
                                 \
    x7 = x10;                    \
    x7 = _mm_srli_si128(x7,  4); \
    x11 = _mm_xor_si128(x11, x7);\
    x7 = x11;                    \
    x7 = _mm_slli_si128(x7,  12);\
    x11 = _mm_xor_si128(x11, x7);

// encryption + Davies-Meyer transform
void shavite3_256_opt_Compress256(const unsigned char* message_block, unsigned char* chaining_value, uint64_t counter)
{
    __attribute__ ((aligned (16))) static const unsigned int SHAVITE_REVERSE[4] = {0x07060504, 0x0b0a0908, 0x0f0e0d0c, 0x03020100 };
    __attribute__ ((aligned (16))) static const unsigned int SHAVITE256_XOR2[4] = {0x0, 0xFFFFFFFF, 0x0, 0x0};
    __attribute__ ((aligned (16))) static const unsigned int SHAVITE256_XOR3[4] = {0x0, 0x0, 0xFFFFFFFF, 0x0};
    __attribute__ ((aligned (16))) static const unsigned int SHAVITE256_XOR4[4] = {0x0, 0x0, 0x0, 0xFFFFFFFF};
    __attribute__ ((aligned (16))) const unsigned int SHAVITE_CNTS[4] = {(unsigned int)(counter & 0xFFFFFFFFULL),(unsigned int)(counter>>32),0,0}; 

    __m128i x0;
    __m128i x1;
    __m128i x2;
    __m128i x3;
    __m128i x4;
    __m128i x6;
    __m128i x7;
    __m128i x8;
    __m128i x9;
    __m128i x10;
    __m128i x11;
    __m128i x12;
    __m128i x13;
    __m128i x14;
    __m128i x15;

    // (L,R) = (xmm0,xmm1)
    const __m128i ptxt1 = _mm_loadu_si128((const __m128i*)chaining_value);
    const __m128i ptxt2 = _mm_loadu_si128((const __m128i*)(chaining_value+16));

    x0 = ptxt1;
    x1 = ptxt2;

    x3 = _mm_loadu_si128((__m128i*)SHAVITE_CNTS);
    x4 = _mm_loadu_si128((__m128i*)SHAVITE256_XOR2);
    x2 = _mm_setzero_si128();

    // init key schedule
    x8 = _mm_loadu_si128((__m128i*)message_block);
    x9 = _mm_loadu_si128((__m128i*)(((unsigned int*)message_block)+4));
    x10 = _mm_loadu_si128((__m128i*)(((unsigned int*)message_block)+8));
    x11 = _mm_loadu_si128((__m128i*)(((unsigned int*)message_block)+12));

    // xmm8..xmm11 = rk[0..15]
    // start key schedule
    x12 = x8;
    x13 = x9;
    x14 = x10;
    x15 = x11;

    const __m128i xtemp = _mm_loadu_si128((__m128i*)SHAVITE_REVERSE);
    x12 = _mm_shuffle_epi8(x12, xtemp);
    x13 = _mm_shuffle_epi8(x13, xtemp);
    x14 = _mm_shuffle_epi8(x14, xtemp);
    x15 = _mm_shuffle_epi8(x15, xtemp);

    x12 = _mm_aesenc_si128(x12, x2);
    x13 = _mm_aesenc_si128(x13, x2);
    x14 = _mm_aesenc_si128(x14, x2);
    x15 = _mm_aesenc_si128(x15, x2);

    x12 = _mm_xor_si128(x12, x3);
    x12 = _mm_xor_si128(x12, x4);
    x4 =  _mm_loadu_si128((__m128i*)SHAVITE256_XOR3);
    x12 = _mm_xor_si128(x12, x11);
    x13 = _mm_xor_si128(x13, x12);
    x14 = _mm_xor_si128(x14, x13);
    x15 = _mm_xor_si128(x15, x14);
   
    // xmm12..xmm15 = rk[16..31]
    // F3 - first round 
    x6 = x8;
    x8 = _mm_xor_si128(x8, x1);
    x8 = _mm_aesenc_si128(x8, x9);
    x8 = _mm_aesenc_si128(x8, x10);
    x8 = _mm_aesenc_si128(x8, x2);
    x0 = _mm_xor_si128(x0, x8);
    x8 = x6;

    // F3 - second round
    x6 = x11;
    x11 = _mm_xor_si128(x11, x0);
    x11 = _mm_aesenc_si128(x11, x12);
    x11 = _mm_aesenc_si128(x11, x13);
    x11 = _mm_aesenc_si128(x11, x2);
    x1 = _mm_xor_si128(x1, x11);
    x11 = x6;

    // key schedule
    SHAVITE_MIXING_256_OPT

    // xmm8..xmm11 - rk[32..47]
    // F3 - third round
    x6 = x14;
    x14 = _mm_xor_si128(x14, x1);
    x14 = _mm_aesenc_si128(x14, x15);
    x14 = _mm_aesenc_si128(x14, x8);
    x14 = _mm_aesenc_si128(x14, x2);
    x0 = _mm_xor_si128(x0, x14);
    x14 = x6;

    // key schedule
    x3 = _mm_shuffle_epi32(x3, 135);

    x12 = x8;
    x13 = x9;
    x14 = x10;
    x15 = x11;
    x12 = _mm_shuffle_epi8(x12, xtemp);
    x13 = _mm_shuffle_epi8(x13, xtemp);
    x14 = _mm_shuffle_epi8(x14, xtemp);
    x15 = _mm_shuffle_epi8(x15, xtemp);
    x12 = _mm_aesenc_si128(x12, x2);
    x13 = _mm_aesenc_si128(x13, x2);
    x14 = _mm_aesenc_si128(x14, x2);
    x15 = _mm_aesenc_si128(x15, x2);

    x12 = _mm_xor_si128(x12, x11);
    x14 = _mm_xor_si128(x14, x3);
    x14 = _mm_xor_si128(x14, x4);
    x4 = _mm_loadu_si128((__m128i*)SHAVITE256_XOR4);
    x13 = _mm_xor_si128(x13, x12);
    x14 = _mm_xor_si128(x14, x13);
    x15 = _mm_xor_si128(x15, x14);

    // xmm12..xmm15 - rk[48..63]

    // F3 - fourth round
    x6 = x9;
    x9 = _mm_xor_si128(x9, x0);
    x9 = _mm_aesenc_si128(x9, x10);
    x9 = _mm_aesenc_si128(x9, x11);
    x9 = _mm_aesenc_si128(x9, x2);
    x1 = _mm_xor_si128(x1, x9);
    x9 = x6;

    // key schedule
    SHAVITE_MIXING_256_OPT
    // xmm8..xmm11 = rk[64..79]
    // F3  - fifth round
    x6 = x12;
    x12 = _mm_xor_si128(x12, x1);
    x12 = _mm_aesenc_si128(x12, x13);
    x12 = _mm_aesenc_si128(x12, x14);
    x12 = _mm_aesenc_si128(x12, x2);
    x0 = _mm_xor_si128(x0, x12);
    x12 = x6;

    // F3 - sixth round
    x6 = x15;
    x15 = _mm_xor_si128(x15, x0);
    x15 = _mm_aesenc_si128(x15, x8);
    x15 = _mm_aesenc_si128(x15, x9);
    x15 = _mm_aesenc_si128(x15, x2);
    x1 = _mm_xor_si128(x1, x15);
    x15 = x6;

    // key schedule
    x3 = _mm_shuffle_epi32(x3, 147);

    x12 = x8;
    x13 = x9;
    x14 = x10;
    x15 = x11;
    x12 = _mm_shuffle_epi8(x12, xtemp);
    x13 = _mm_shuffle_epi8(x13, xtemp);
    x14 = _mm_shuffle_epi8(x14, xtemp);
    x15 = _mm_shuffle_epi8(x15, xtemp);
    x12 = _mm_aesenc_si128(x12, x2);
    x13 = _mm_aesenc_si128(x13, x2);
    x14 = _mm_aesenc_si128(x14, x2);
    x15 = _mm_aesenc_si128(x15, x2);
    x12 = _mm_xor_si128(x12, x11);
    x13 = _mm_xor_si128(x13, x3);
    x13 = _mm_xor_si128(x13, x4);
    x13 = _mm_xor_si128(x13, x12);
    x14 = _mm_xor_si128(x14, x13);
    x15 = _mm_xor_si128(x15, x14);

    // xmm12..xmm15 = rk[80..95]
    // F3 - seventh round
    x6 = x10;
    x10 = _mm_xor_si128(x10, x1);
    x10 = _mm_aesenc_si128(x10, x11);
    x10 = _mm_aesenc_si128(x10, x12);
    x10 = _mm_aesenc_si128(x10, x2);
    x0 = _mm_xor_si128(x0, x10);
    x10 = x6;

    // key schedule
    SHAVITE_MIXING_256_OPT

    // xmm8..xmm11 = rk[96..111]
    // F3 - eigth round
    x6 = x13;
    x13 = _mm_xor_si128(x13, x0);
    x13 = _mm_aesenc_si128(x13, x14);
    x13 = _mm_aesenc_si128(x13, x15);
    x13 = _mm_aesenc_si128(x13, x2);
    x1 = _mm_xor_si128(x1, x13);
    x13 = x6;


    // key schedule
    x3 = _mm_shuffle_epi32(x3, 135);

    x12 = x8;
    x13 = x9;
    x14 = x10;
    x15 = x11;
    x12 = _mm_shuffle_epi8(x12, xtemp);
    x13 = _mm_shuffle_epi8(x13, xtemp);
    x14 = _mm_shuffle_epi8(x14, xtemp);
    x15 = _mm_shuffle_epi8(x15, xtemp);
    x12 = _mm_aesenc_si128(x12, x2);
    x13 = _mm_aesenc_si128(x13, x2);
    x14 = _mm_aesenc_si128(x14, x2);
    x15 = _mm_aesenc_si128(x15, x2);
    x12 = _mm_xor_si128(x12, x11);
    x15 = _mm_xor_si128(x15, x3);
    x15 = _mm_xor_si128(x15, x4);
    x13 = _mm_xor_si128(x13, x12);
    x14 = _mm_xor_si128(x14, x13);
    x15 = _mm_xor_si128(x15, x14);

    // xmm12..xmm15 = rk[112..127]
    // F3 - ninth round
    x6 = x8;
    x8 = _mm_xor_si128(x8, x1);
    x8 = _mm_aesenc_si128(x8, x9);
    x8 = _mm_aesenc_si128(x8, x10);
    x8 = _mm_aesenc_si128(x8, x2);
    x0 = _mm_xor_si128(x0, x8);
    x8 = x6;
    // F3 - tenth round
    x6 = x11;
    x11 = _mm_xor_si128(x11, x0);
    x11 = _mm_aesenc_si128(x11, x12);
    x11 = _mm_aesenc_si128(x11, x13);
    x11 = _mm_aesenc_si128(x11, x2);
    x1 = _mm_xor_si128(x1, x11);
    x11 = x6;

    // key schedule
    SHAVITE_MIXING_256_OPT

    // xmm8..xmm11 = rk[128..143]
    // F3 - eleventh round
    x6 = x14;
    x14 = _mm_xor_si128(x14, x1);
    x14 = _mm_aesenc_si128(x14, x15);
    x14 = _mm_aesenc_si128(x14, x8);
    x14 = _mm_aesenc_si128(x14, x2);
    x0 = _mm_xor_si128(x0, x14);
    x14 = x6;

    // F3 - twelfth round
    x6 = x9;
    x9 = _mm_xor_si128(x9, x0);
    x9 = _mm_aesenc_si128(x9, x10);
    x9 = _mm_aesenc_si128(x9, x11);
    x9 = _mm_aesenc_si128(x9, x2);
    x1 = _mm_xor_si128(x1, x9);
    x9 = x6;


    // feedforward
    x0 = _mm_xor_si128(x0, ptxt1);
    x1 = _mm_xor_si128(x1, ptxt2);
    _mm_storeu_si128((__m128i *)chaining_value, x0);
    _mm_storeu_si128((__m128i *)(chaining_value + 16), x1);

    return;
}

#define U8TO16_LITTLE(c)  (((uint16_t)T8(*((uint8_t*)(c)))) | ((uint16_t)T8(*(((uint8_t*)(c)) + 1)) << 8))

// Make sure that the local variable names do not collide with variables of the calling code (i.e., those used in c, v)
#define U16TO8_LITTLE(c, v) do { \
    uint16_t tmp_portable_h_x = (v); \
    uint8_t *tmp_portable_h_d = (c); \
    tmp_portable_h_d[0] = T8(tmp_portable_h_x); \
    tmp_portable_h_d[1] = T8(tmp_portable_h_x >> 8); \
} while (0)

#define U8TO32_LITTLE(c)  (((uint32_t)T8(*((uint8_t*)(c)))) | ((uint32_t)T8(*(((uint8_t*)(c)) + 1)) << 8) | ((uint32_t)T8(*(((uint8_t*)(c)) + 2)) << 16) | ((uint32_t)T8(*(((uint8_t*)(c)) + 3)) << 24))

// Make sure that the local variable names do not collide with variables of the calling code (i.e., those used in c, v)
#define U32TO8_LITTLE(c, v) do { \
    uint32_t tmp_portable_h_x = (v); \
    uint8_t *tmp_portable_h_d = (c); \
    tmp_portable_h_d[0] = T8(tmp_portable_h_x); \
    tmp_portable_h_d[1] = T8(tmp_portable_h_x >> 8); \
    tmp_portable_h_d[2] = T8(tmp_portable_h_x >> 16); \
    tmp_portable_h_d[3] = T8(tmp_portable_h_x >> 24); \
} while (0)

// Make sure that the local variable names do not collide with variables of the calling code (i.e., those used in c, v)
#define U64TO8_LITTLE(c, v)    do { \
    uint64_t tmp_portable_h_x = (v); \
    uint8_t *tmp_portable_h_d = (c); \
    tmp_portable_h_d[0] = T8(tmp_portable_h_x); \
    tmp_portable_h_d[1] = T8(tmp_portable_h_x >> 8);  \
    tmp_portable_h_d[2] = T8(tmp_portable_h_x >> 16); \
    tmp_portable_h_d[3] = T8(tmp_portable_h_x >> 24); \
    tmp_portable_h_d[4] = T8(tmp_portable_h_x >> 32); \
    tmp_portable_h_d[5] = T8(tmp_portable_h_x >> 40); \
    tmp_portable_h_d[6] = T8(tmp_portable_h_x >> 48); \
    tmp_portable_h_d[7] = T8(tmp_portable_h_x >> 56); \
} while (0)

// Initialization of the internal state of the hash function
bool shavite3_256_opt_Init ( shavite3_256_opt_hashState* state)
{
    // Initialization of the counter of number of bits that were hashed so far
    state->bitcount = 0;

    // Store the requested digest size
    state->DigestSize = 256;

    // Initialize the message block to empty
    memset(state->buffer,0,64);

    // Set the input to the compression function to all zero
    memset(state->chaining_value,0,32); 

    // Compute MIV_{256}
    shavite3_256_opt_Compress256(state->buffer,state->chaining_value,0x0ULL);

    // Set the message block to the size of the requested digest size
    U16TO8_LITTLE(state->buffer,256);

    // Compute IV_m
    shavite3_256_opt_Compress256(state->buffer,state->chaining_value,0x0ULL);

    // Set the block size to be 512 bits (as required for C_{256})
    state->BlockSize=512; 

    // Set the message block to zero
    memset(state->buffer,0,64);
    return true;
}



// Compressing the input data, and updating the internal state
bool shavite3_256_opt_Update ( shavite3_256_opt_hashState* state, const unsigned char* data, uint64_t dataLenBytes)
{
    // p is a pointer to the current location inside data that we need to process (i.e., the first byte of the data which was not used as an input to the compression function
    uint8_t* p = (uint8_t*)data;

    // len is the size of the data that was not process yet in bytes
    int len = dataLenBytes;

    // BlockSizeB is the size of the message block of the compression function
    int BlockSizeB = (state->BlockSize/8);

    // bufcnt stores the number of bytes that are were "sent" to the compression function, but were not yet processed, as a full block has not been obtained
    int bufcnt= (state->bitcount>>3)%BlockSizeB;

    // local_bitcount contains the number of bits actually hashed so far
    uint64_t SHAVITE_CNT;

    // load the number of bits hashed so far into SHAVITE_CNT
    SHAVITE_CNT=state->bitcount;

    // mark that we processed more bits
    state->bitcount += dataLenBytes*8;

    // Check if we have enough data to call the compression function
    // If not, just copy the input to the buffer of the message block
    if (bufcnt + len < BlockSizeB)
    {
        memcpy(&state->buffer[bufcnt], p, len);
        return true;
    }

    // There is enough data to start calling the compression function.
    // We first check whether there is data remaining from previous calls
    if (bufcnt>0)
    {
        // Copy from the input the required number of bytes to fill a block
        memcpy(&state->buffer[bufcnt], p, BlockSizeB-bufcnt);

        // Update the location of the first byte that was not processed
        p += BlockSizeB-bufcnt;

        // Update the remaining number of bytes to process
        len -= BlockSizeB-bufcnt;

        // Update the number of bits hashed so far (locally)
        SHAVITE_CNT+=8*(BlockSizeB-bufcnt);

        // Call the compression function to process the current block
        shavite3_256_opt_Compress256(state->buffer, state->chaining_value, SHAVITE_CNT);
    }


    // At this point, the only remaining data is from the message block call the compression function as many times as possible, and store the remaining bytes in the buffer
    // Each step of the loop compresses BlockSizeB bytes
    for( ; len>=BlockSizeB; len-=BlockSizeB, p+=BlockSizeB)
    {
        // Update the number of bits hashed so far (locally)
        SHAVITE_CNT+=8*BlockSizeB;

        // Call the compression function to process the current block
        shavite3_256_opt_Compress256(p, state->chaining_value, SHAVITE_CNT);
    }

    // If there are still unprocessed bytes, store them locally and wait for more
    if (len>0)
    {
        memcpy(state->buffer, p, len);
    }
    return true;
}


// Performing the padding scheme, and dealing with any remaining bits
bool shavite3_256_opt_Final ( shavite3_256_opt_hashState *state, unsigned char *hashval)
{
    // Stores inputs (message blocks) to the compression function
    uint8_t block[64];

    // Stores results (chaining value) of the compression function
    uint8_t result[32];

    // BlockSizeB is the size of the message block of the compression function
    int BlockSizeB = (state->BlockSize/8);

    // bufcnt stores the number of bytes that are were "sent" to the compression function, but were not yet processed, as a full block has not been obtained
    int bufcnt= ((uint32_t)state->bitcount>>3)%BlockSizeB;

    int i;
    // Copy the current chaining value into result (as a temporary step)
    memcpy(result, state->chaining_value, 32);


    // Initialize block as the message block to compress with the bytes that were not processed yet
    memset(block, 0, BlockSizeB);
    memcpy(block, state->buffer, bufcnt);

    // Pad the buffer with the byte which contains the fraction of bytes from and a bit equal to 1
   block[bufcnt] = (state->partial_byte & ~((0x80 >> (state->bitcount&7))-1)) | (0x80 >> (state->bitcount&7));

    // Compress the last block (according to the digest size)
    // An additional message block is required if there are less than 10 more bytes for message length and digest length encoding
    if (bufcnt>=BlockSizeB-10)
    {
        // Compress the current block
        shavite3_256_opt_Compress256(block,result,state->bitcount);

        // Generate the full padding block
        memset(block, 0, BlockSizeB);
        U64TO8_LITTLE(block+BlockSizeB-10, state->bitcount);
        U16TO8_LITTLE(block+BlockSizeB-2, state->DigestSize);

        // Compress the full padding block
        shavite3_256_opt_Compress256(block,result,0x0UL);
    }
    else
    {
        // Pad the number of bits hashed so far and the digest size to the last message block and compress it
        U64TO8_LITTLE(block+BlockSizeB-10, state->bitcount);
        U16TO8_LITTLE(block+BlockSizeB-2, state->DigestSize);
        if ((state->bitcount&(state->BlockSize-1))==0)
        {
            shavite3_256_opt_Compress256(block,result, 0ULL);
        }
        else
        {
            shavite3_256_opt_Compress256(block,result, state->bitcount);
        }
    }
    
    // Copy the result into the supplied array of bytes.
    for (i=0;i<(state->DigestSize+7)/8;i++)
    {
        hashval[i]=result[i];
    }

    // Treat cases where the digest size is not a multiple of a byte
    if ((state->DigestSize)&7)
    {
        hashval[(state->DigestSize+7)/8] &= (0xFF<<(8-((state->DigestSize)%8)))&0xFF;
    }
    return true;
}

#endif
