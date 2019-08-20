/* Modified (July 2010) by Eli Biham and Orr Dunkelman (applying the
SHAvite-3 tweak) from:                                       */

/* Modified (June 2009) from: */

/*********************************************************************/
/*                                                                   */
/*                           SHAvite-3                               */
/*                                                                   */
/* Candidate submission to NIST SHA-3 competition                    */
/*                                                                   */
/* Written by Eli Biham and Orr Dunkelman                            */
/*                                                                   */
/*********************************************************************/
// File contains modifications by: The Gulden developers
// All modifications:
// Copyright (c) 2019 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#include "compat.h"
#include "assert.h"
#include <memory.h>

#include "shavite3_256_aesni.h"
#include "shavite3_256_aesni_compress.h"

#define U8TO16_LITTLE(c)  (((uint16_t)T8(*((uint8_t*)(c)))) | ((uint16_t)T8(*(((uint8_t*)(c)) + 1)) << 8))

// Make sure that the local variable names do not collide with variables of the calling code (i.e., those used in c, v) */
#define U16TO8_LITTLE(c, v) do { \
    uint16_t tmp_portable_h_x = (v); \
    uint8_t *tmp_portable_h_d = (c); \
    tmp_portable_h_d[0] = T8(tmp_portable_h_x); \
    tmp_portable_h_d[1] = T8(tmp_portable_h_x >> 8); \
} while (0)

#define U8TO32_LITTLE(c)  (((uint32_t)T8(*((uint8_t*)(c)))) | \
    ((uint32_t)T8(*(((uint8_t*)(c)) + 1)) << 8) | \
    ((uint32_t)T8(*(((uint8_t*)(c)) + 2)) << 16) | \
    ((uint32_t)T8(*(((uint8_t*)(c)) + 3)) << 24))

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

/* Initialization of the internal state of the hash function         */
bool shavite3_256_aesni_Init(shavite3_256_aesni_hashState *state)
{
    /* Setting the salt to zero. Applications which wish to use          */
    /* different salt should initialize it after (!) the derivation of   */
    /* the IVs                                                           */
    memset(state->salt,0,32);

    /* Initialization of the counter of number of bits that were hashed  */
    /* so far                                                            */ 
    state->bitcount = 0;

    /* Store the requested digest size                                   */
    state->DigestSize = 256;

    /* Initialize the message block to empty                             */
    memset(state->buffer,0,64);

    /* Set the input to the compression function to all zero             */
    memset(state->chaining_value,0,32); 

    /* Compute MIV_{256}                                                 */
    shavite3_256_aesni_Compress256(state->buffer,state->chaining_value,0x0ULL,state->salt);

    /* Set the message block to the size of the requested digest size    */   
    U16TO8_LITTLE(state->buffer,256);

    /* Compute IV_m                                                      */
    shavite3_256_aesni_Compress256(state->buffer,state->chaining_value,0x0ULL,state->salt);

    /* Set the block size to be 512 bits (as required for C_{256})       */
    state->BlockSize=512; 

    /* Set the message block to zero				     */
    memset(state->buffer,0,64);
    return true;
}



/* Compressing the input data, and updating the internal state       */
bool shavite3_256_aesni_Update(shavite3_256_aesni_hashState *state, const unsigned char *data, uint64_t databitlen)
{
    /* p is a pointer to the current location inside data that we need   */
    /* to process (i.e., the first byte of the data which was not used   */
    /* as an input to the compression function                           */
    uint8_t *p = (uint8_t*)data;

    /* len is the size of the data that was not process yet in bytes     */
    int len = databitlen>>3;

    /* BlockSizeB is the size of the message block of the compression    */
    /* function                                                          */
    int BlockSizeB = (state->BlockSize/8);

    /* bufcnt stores the number of bytes that are were "sent" to the     */
    /* compression function, but were not yet processed, as a full block */
    /* has not been obtained                                             */
    int bufcnt= (state->bitcount>>3)%BlockSizeB;

    /* local_bitcount contains the number of bits actually hashed so far */
    uint64_t SHAVITE_CNT;

    /* If we had to process a message with partial bytes before, then    */
    /* Update() should not have been called again.                       */
    /* We just discard the extra bits, and inform the user               */
    if (state->bitcount&7ULL)
    {
        assert(0);
        //fprintf(stderr, "We are sorry, you are calling Update one time after\nwhat should have been the last call. We ignore few bits of the input.\n");
        state->bitcount &= ~7ULL;
    }

    /* load the number of bits hashed so far into SHAVITE_CNT         */
    SHAVITE_CNT=state->bitcount;

    /* mark that we processed more bits                                  */
    state->bitcount += databitlen;

    /* if the input contains a partial byte - store it independently     */
    if (databitlen&7)
    {
        state->partial_byte = data[databitlen>>3];
    }

    /* Check if we have enough data to call the compression function     */
    /* If not, just copy the input to the buffer of the message block    */
    if (bufcnt + len < BlockSizeB)
    {
        memcpy(&state->buffer[bufcnt], p, len);
        return true;
    }


    /* There is enough data to start calling the compression function.   */
    /* We first check whether there is data remaining from previous      */
    /* calls                                                             */
    if (bufcnt>0)
    {
        /* Copy from the input the required number of bytes to fill a block  */
        memcpy(&state->buffer[bufcnt], p, BlockSizeB-bufcnt);

        /* Update the location of the first byte that was not processed      */
        p += BlockSizeB-bufcnt;

        /* Update the remaining number of bytes to process                   */
         len -= BlockSizeB-bufcnt;

        /* Update the number of bits hashed so far (locally)                 */
        SHAVITE_CNT+=8*(BlockSizeB-bufcnt);

        /* Call the compression function to process the current block        */
        shavite3_256_aesni_Compress256(state->buffer, state->chaining_value, SHAVITE_CNT, state->salt);
    }


    /* At this point, the only remaining data is from the message block  */
    /* call the compression function as many times as possible, and      */
    /* store the remaining bytes in the buffer                           */

    /* Each step of the loop compresses BlockSizeB bytes                 */
    for( ; len>=BlockSizeB; len-=BlockSizeB, p+=BlockSizeB)
    {
        /* Update the number of bits hashed so far (locally)                 */
        SHAVITE_CNT+=8*BlockSizeB;

        /* Call the compression function to process the current   block      */
        shavite3_256_aesni_Compress256(p, state->chaining_value, SHAVITE_CNT, state->salt);
    }

    /* If there are still unprocessed bytes, store them locally and wait */
    /* for more                                                          */
    if (len>0)
    {
        memcpy(state->buffer, p, len);
    }
    return true;
}


/* Performing the padding scheme, and dealing with any remaining     */
/* bits                                                              */
bool shavite3_256_aesni_Final(shavite3_256_aesni_hashState *state, unsigned char *hashval)
{
   /* Stores inputs (message blocks) to the compression function        */
   uint8_t block[64];

   /* Stores results (chaining value) of the compression function       */
   uint8_t result[32];

    /* BlockSizeB is the size of the message block of the compression    */
    /* function                                                          */
    int BlockSizeB = (state->BlockSize/8);

    /* bufcnt stores the number of bytes that are were "sent" to the     */
    /* compression function, but were not yet processed, as a full block */
    /* has not been obtained                                             */
    int bufcnt= ((uint32_t)state->bitcount>>3)%BlockSizeB;

    int i;
    /* Copy the current chaining value into result (as a temporary step) */
    memcpy(result, state->chaining_value, 32);


    /* Initialize block as the message block to compress with the bytes  */
    /* that were not processed yet                                       */
    memset(block, 0, BlockSizeB);
    memcpy(block, state->buffer, bufcnt);


    /* Pad the buffer with the byte which contains the fraction of bytes */
    /* from and a bit equal to 1					     */
   block[bufcnt] = (state->partial_byte & ~((0x80 >> (state->bitcount&7))-1)) | (0x80 >> (state->bitcount&7));


    /* Compress the last block (according to the digest size)            */
    /* An additional message block is required if there are less than 10 */
    /* more bytes for message length and digest length encoding          */
    if (bufcnt>=BlockSizeB-10)
    {
        /* Compress the current block                                        */
        shavite3_256_aesni_Compress256(block,result,state->bitcount,state->salt);

        /* Generate the full padding block                                   */
        memset(block, 0, BlockSizeB);
        U64TO8_LITTLE(block+BlockSizeB-10, state->bitcount);
        U16TO8_LITTLE(block+BlockSizeB-2, state->DigestSize);

        /* Compress the full padding block                                   */
        shavite3_256_aesni_Compress256(block,result,0x0UL,state->salt);
    }
    else
    {
        /* Pad the number of bits hashed so far and the digest size to the   */
        /* last message block and compress it
        */
        U64TO8_LITTLE(block+BlockSizeB-10, state->bitcount);
        U16TO8_LITTLE(block+BlockSizeB-2, state->DigestSize);
        if ((state->bitcount&(state->BlockSize-1))==0)
        {
            shavite3_256_aesni_Compress256(block,result, 0ULL, state->salt);
        }
        else
        {
            shavite3_256_aesni_Compress256(block,result, state->bitcount, state->salt);
        }
    }
    
    /* Copy the result into the supplied array of bytes.                 */
    for (i=0;i<(state->DigestSize+7)/8;i++)
    {
        hashval[i]=result[i];
    }

    /* Treat cases where the digest size is not a multiple of a byte     */
   if ((state->DigestSize)&7)
   {
       hashval[(state->DigestSize+7)/8] &= (0xFF<<(8-((state->DigestSize)%8)))&0xFF;
   }
   return true;
}


