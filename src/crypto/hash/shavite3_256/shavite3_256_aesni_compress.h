/* Modified (October 2010) by Eli Biham and Orr Dunkelman    * 
 * (applying the SHAvite-3 tweak) from:                      */

/*                     compress.h                            */

/*************************************************************
 * Source for Intel AES-NI assembly implementation/emulation *
 * of the compression function of SHAvite-3 256              *
 *                                                           *
 * Authors:  Ryad Benadjila -- Orange Labs                   *
 *           Olivier Billet -- Orange Labs                   *
 *                                                           *
 * June, 2009                                                *
 *************************************************************/
// File contains modifications by: The Gulden developers
// All modifications:
// Copyright (c) 2019 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#ifndef SHAVITE_3_256_AESNI_COMPRESS_H
#define SHAVITE_3_256_AESNI_COMPRESS_H

#include <compat/arch.h>
// Only x86 family CPUs have AES-NI
#ifdef ARCH_CPU_X86_FAMILY

#define T8(x) ((x) & 0xff)

/* Encrypts the plaintext pt[] using the key message[], salt[],      */
/* and counter[], to produce the ciphertext ct[]                     */

__attribute__ ((aligned (16))) thread_local unsigned int SHAVITE_MESS[16];
__attribute__ ((aligned (16))) thread_local unsigned char SHAVITE_PTXT[8*4];
__attribute__ ((aligned (16))) thread_local unsigned int SHAVITE_CNTS[4] = {0,0,0,0}; 
__attribute__ ((aligned (16))) thread_local unsigned int SHAVITE_REVERSE[4] = {0x07060504, 0x0b0a0908, 0x0f0e0d0c, 0x03020100 };
__attribute__ ((aligned (16))) thread_local unsigned int SHAVITE256_XOR2[4] = {0x0, 0xFFFFFFFF, 0x0, 0x0};
__attribute__ ((aligned (16))) thread_local unsigned int SHAVITE256_XOR3[4] = {0x0, 0x0, 0xFFFFFFFF, 0x0};
__attribute__ ((aligned (16))) thread_local unsigned int SHAVITE256_XOR4[4] = {0x0, 0x0, 0x0, 0xFFFFFFFF};


#define PERFORM_MIXING        \
   "movaps  xmm11, xmm15\n\t" \
   "movaps  xmm10, xmm14\n\t" \
   "movaps  xmm9,  xmm13\n\t" \
   "movaps  xmm8,  xmm12\n\t" \
                              \
   "movaps  xmm6,  xmm11\n\t" \
   "psrldq  xmm6,  4\n\t"     \
   "pxor    xmm8,  xmm6\n\t"  \
   "movaps  xmm6,  xmm8\n\t"  \
   "pslldq  xmm6,  12\n\t"    \
   "pxor    xmm8,  xmm6\n\t"  \
                              \
   "movaps  xmm7,  xmm8\n\t"  \
   "psrldq  xmm7,  4\n\t"     \
   "pxor    xmm9,  xmm7\n\t"  \
   "movaps  xmm7,  xmm9\n\t"  \
   "pslldq  xmm7,  12\n\t"    \
   "pxor    xmm9,  xmm7\n\t"  \
                              \
   "movaps  xmm6,  xmm9\n\t"  \
   "psrldq  xmm6,  4\n\t"     \
   "pxor    xmm10, xmm6\n\t"  \
   "movaps  xmm6,  xmm10\n\t" \
   "pslldq  xmm6,  12\n\t"    \
   "pxor    xmm10, xmm6\n\t"  \
                              \
   "movaps  xmm7,  xmm10\n\t" \
   "psrldq  xmm7,  4\n\t"     \
   "pxor    xmm11, xmm7\n\t"  \
   "movaps  xmm7,  xmm11\n\t" \
   "pslldq  xmm7,  12\n\t"    \
   "pxor    xmm11, xmm7\n\t"

void E256()
{
   asm (".intel_syntax noprefix");

   /* (L,R) = (xmm0,xmm1) */
   asm ("movaps xmm0,  %[PTXT] \n\t"
        "movaps xmm1,  %[PTXT]+16 \n\t"
        "movaps xmm3,  %[CNTS] \n\t"
        "movaps xmm4,  %[XOR2_256] \n\t"
        "pxor   xmm2,  xmm2 \n\t"
   /* init key schedule */
        "movaps xmm8,  %[MESS] \n\t"
        "movaps xmm9,  %[MESS]+16 \n\t"
        "movaps xmm10, %[MESS]+32 \n\t"
        "movaps xmm11, %[MESS]+48\n\t" 
   /* xmm8..xmm11 = rk[0..15] */
   /* start key schedule */
        "movaps xmm12, xmm8\n\t"
        "movaps xmm13, xmm9\n\t"
        "movaps xmm14, xmm10\n\t"
        "movaps xmm15, xmm11\n\t"
        "pshufb xmm12, %[REV]\n\t"
        "pshufb xmm13, %[REV]\n\t"
        "pshufb xmm14, %[REV]\n\t"
        "pshufb xmm15, %[REV]\n\t"
        "aesenc xmm12, xmm2\n\t"
        "aesenc xmm13, xmm2\n\t"
        "aesenc xmm14, xmm2\n\t"
        "aesenc xmm15, xmm2\n\t"
        "pxor   xmm12, xmm3\n\t"
        "pxor   xmm12, xmm4\n\t"
        "movaps xmm4,  %[XOR3_256]\n\t"
        "pxor   xmm12, xmm11\n\t"
        "pxor   xmm13, xmm12\n\t"
        "pxor   xmm14, xmm13\n\t"
        "pxor   xmm15, xmm14\n\t"
   /* xmm12..xmm15 = rk[16..31] */
   /* F3 - first round */
        "movaps xmm6,  xmm8\n\t"
        "pxor   xmm8,  xmm1\n\t"
        "aesenc xmm8,  xmm9\n\t"
        "aesenc xmm8,  xmm10\n\t"
        "aesenc xmm8,  xmm2\n\t"
        "pxor   xmm0,  xmm8\n\t"
        "movaps xmm8,  xmm6\n\t"
    /* F3 - second round */
        "movaps xmm6,  xmm11\n\t"
        "pxor   xmm11, xmm0\n\t"
        "aesenc xmm11, xmm12\n\t"
        "aesenc xmm11, xmm13\n\t"
        "aesenc xmm11, xmm2\n\t"
        "pxor   xmm1,  xmm11\n\t"
        "movaps xmm11, xmm6\n\t"
    /* key schedule */
        PERFORM_MIXING
    /* xmm8..xmm11 - rk[32..47] */
    /* F3 - third round */
        "movaps xmm6,  xmm14\n\t"
        "pxor   xmm14, xmm1\n\t"
        "aesenc xmm14, xmm15\n\t"
        "aesenc xmm14, xmm8\n\t"
        "aesenc xmm14, xmm2\n\t"
        "pxor   xmm0,  xmm14\n\t"
        "movaps xmm14, xmm6\n\t"
    /* key schedule */
        "pshufd xmm3,  xmm3,135\n\t"
        "movaps xmm12, xmm8\n\t"
        "movaps xmm13, xmm9\n\t"
        "movaps xmm14, xmm10\n\t"
        "movaps xmm15, xmm11\n\t"
        "pshufb xmm12, %[REV]\n\t"
        "pshufb xmm13, %[REV]\n\t"
        "pshufb xmm14, %[REV]\n\t"
        "pshufb xmm15, %[REV]\n\t"
        "aesenc xmm12, xmm2\n\t"
        "aesenc xmm13, xmm2\n\t"
        "aesenc xmm14, xmm2\n\t"
        "aesenc xmm15, xmm2\n\t"
        "pxor   xmm12, xmm11\n\t"
        "pxor   xmm14, xmm3\n\t"
        "pxor   xmm14, xmm4\n\t"
        "movaps xmm4,  %[XOR4_256]\n\t"
        "pxor   xmm13, xmm12\n\t"
        "pxor   xmm14, xmm13\n\t"
        "pxor   xmm15, xmm14\n\t"
    /* xmm12..xmm15 - rk[48..63] */
    /* F3 - fourth round */
        "movaps xmm6,  xmm9\n\t"
        "pxor   xmm9,  xmm0\n\t"
        "aesenc xmm9,  xmm10\n\t"
        "aesenc xmm9,  xmm11\n\t"
        "aesenc xmm9,  xmm2\n\t"
        "pxor   xmm1,  xmm9\n\t"
        "movaps xmm9,  xmm6\n\t"
    /* key schedule */
        PERFORM_MIXING
    /* xmm8..xmm11 = rk[64..79] */
    /* F3  - fifth round */
        "movaps xmm6,  xmm12\n\t"
        "pxor   xmm12, xmm1\n\t"
        "aesenc xmm12, xmm13\n\t"
        "aesenc xmm12, xmm14\n\t"
        "aesenc xmm12, xmm2\n\t"
        "pxor   xmm0,  xmm12\n\t"
        "movaps xmm12, xmm6\n\t"
    /* F3 - sixth round */
        "movaps xmm6,  xmm15\n\t"
        "pxor   xmm15, xmm0\n\t"
        "aesenc xmm15, xmm8\n\t"
        "aesenc xmm15, xmm9\n\t"
        "aesenc xmm15, xmm2\n\t"
        "pxor   xmm1,  xmm15\n\t"
        "movaps xmm15, xmm6\n\t"
    /* key schedule */
        "pshufd xmm3,  xmm3, 147\n\t"
        "movaps xmm12, xmm8\n\t"
        "movaps xmm13, xmm9\n\t"
        "movaps xmm14, xmm10\n\t"
        "movaps xmm15, xmm11\n\t"
        "pshufb xmm12, %[REV]\n\t"
        "pshufb xmm13, %[REV]\n\t"
        "pshufb xmm14, %[REV]\n\t"
        "pshufb xmm15, %[REV]\n\t"
        "aesenc xmm12, xmm2\n\t"
        "aesenc xmm13, xmm2\n\t"
        "aesenc xmm14, xmm2\n\t"
        "aesenc xmm15, xmm2\n\t"
        "pxor   xmm12, xmm11\n\t"
        "pxor   xmm13, xmm3\n\t"
        "pxor   xmm13, xmm4\n\t"
        "pxor   xmm13, xmm12\n\t"
        "pxor   xmm14, xmm13\n\t"
        "pxor   xmm15, xmm14\n\t"
    /* xmm12..xmm15 = rk[80..95] */
    /* F3 - seventh round */
        "movaps xmm6,  xmm10\n\t"
        "pxor   xmm10, xmm1\n\t"
        "aesenc xmm10, xmm11\n\t"
        "aesenc xmm10, xmm12\n\t"
        "aesenc xmm10, xmm2\n\t"
        "pxor   xmm0,  xmm10\n\t"
        "movaps xmm10, xmm6\n\t"
    /* key schedule */
        PERFORM_MIXING
    /* xmm8..xmm11 = rk[96..111] */
    /* F3 - eigth round */
        "movaps xmm6,  xmm13\n\t"
        "pxor   xmm13, xmm0\n\t"
        "aesenc xmm13, xmm14\n\t"
        "aesenc xmm13, xmm15\n\t"
        "aesenc xmm13, xmm2\n\t"
        "pxor   xmm1,  xmm13\n\t"
        "movaps xmm13, xmm6\n\t"
    /* key schedule */
        "pshufd xmm3,  xmm3, 135\n\t"
        "movaps xmm12, xmm8\n\t"
        "movaps xmm13, xmm9\n\t"
        "movaps xmm14, xmm10\n\t"
        "movaps xmm15, xmm11\n\t"
        "pshufb xmm12, %[REV]\n\t"
        "pshufb xmm13, %[REV]\n\t"
        "pshufb xmm14, %[REV]\n\t"
        "pshufb xmm15, %[REV]\n\t"
        "aesenc xmm12, xmm2\n\t"
        "aesenc xmm13, xmm2\n\t"
        "aesenc xmm14, xmm2\n\t"
        "aesenc xmm15, xmm2\n\t"
        "pxor   xmm12, xmm11\n\t"
        "pxor   xmm15, xmm3\n\t"
        "pxor   xmm15, xmm4\n\t"
        "pxor   xmm13, xmm12\n\t"
        "pxor   xmm14, xmm13\n\t"
        "pxor   xmm15, xmm14\n\t"
    /* xmm12..xmm15 = rk[112..127] */
    /* F3 - ninth round */
        "movaps xmm6,  xmm8\n\t"
        "pxor   xmm8,  xmm1\n\t"
        "aesenc xmm8,  xmm9\n\t"
        "aesenc xmm8,  xmm10\n\t"
        "aesenc xmm8,  xmm2\n\t"
        "pxor   xmm0,  xmm8\n\t"
        "movaps xmm8,  xmm6\n\t"
    /* F3 - tenth round */
        "movaps xmm6,  xmm11\n\t"
        "pxor   xmm11, xmm0\n\t"
        "aesenc xmm11, xmm12\n\t"
        "aesenc xmm11, xmm13\n\t"
        "aesenc xmm11, xmm2\n\t"
        "pxor   xmm1,  xmm11\n\t"
        "movaps xmm11, xmm6\n\t"
    /* key schedule */
        PERFORM_MIXING
    /* xmm8..xmm11 = rk[128..143] */
    /* F3 - eleventh round */
        "movaps xmm6,  xmm14\n\t"
        "pxor   xmm14, xmm1\n\t"
        "aesenc xmm14, xmm15\n\t"
        "aesenc xmm14, xmm8\n\t"
        "aesenc xmm14, xmm2\n\t"
        "pxor   xmm0,  xmm14\n\t"
        "movaps xmm14, xmm6\n\t"
   /* F3 - twelfth round */
        "movaps xmm6,  xmm9\n\t"
        "pxor   xmm9,  xmm0\n\t"
        "aesenc xmm9,  xmm10\n\t"
        "aesenc xmm9,  xmm11\n\t"
        "aesenc xmm9,  xmm2\n\t"
        "pxor   xmm1,  xmm9\n\t"
        "movaps xmm9,  xmm6\n\t"
   /* feedforward */
        "pxor   xmm0,  %[PTXT]\n\t"
        "pxor   xmm1,  %[PTXT]+16\n\t"
        "movaps %[PTXT], xmm0\n\t"
        "movaps %[PTXT]+16, xmm1\n\t"
        ".att_syntax noprefix"
        : [MESS]     "+m" (SHAVITE_MESS)
        , [CNTS]     "+m" (SHAVITE_CNTS) 
        , [PTXT]     "+m" (SHAVITE_PTXT)
        , [XOR2_256] "+m" (SHAVITE256_XOR2)
        , [XOR3_256] "+m" (SHAVITE256_XOR3)
        , [XOR4_256] "+m" (SHAVITE256_XOR4)
        , [REV]      "+m" (SHAVITE_REVERSE)
   );
   return;
}

void shavite3_256_aesni_Compress256(const unsigned char *message_block, unsigned char *chaining_value, unsigned long long counter, const unsigned char salt[32])
{    
   int i;

   for (i=0;i<8*4;i++)
   {
      SHAVITE_PTXT[i]=chaining_value[i];
   }
   
   for (i=0;i<16;i++)
   {
      SHAVITE_MESS[i] = *((unsigned int*)(message_block+4*i));
   }
   

   SHAVITE_CNTS[0] = (unsigned int)(counter & 0xFFFFFFFFULL);
   SHAVITE_CNTS[1] = (unsigned int)(counter>>32);
   /* encryption + Davies-Meyer transform */
   E256();

   for (i=0; i<4*8; i++)
   {
       chaining_value[i]=SHAVITE_PTXT[i];
   }


   return;
}

#endif
#endif
