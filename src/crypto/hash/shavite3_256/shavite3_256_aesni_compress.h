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

#define tos(a)    #a
#define tostr(a)  tos(a)

#define T8(x) ((x) & 0xff)

#define rev_reg_0321(j){\
        asm ("pshufb xmm" tostr(j) ", SHAVITE_REVERSE[rip]");\
}

#define replace_aes(i, j){\
        asm ("aesenc xmm" tostr(i) ", xmm" tostr(j) "");\
}

/* Encrypts the plaintext pt[] using the key message[], salt[],      */
/* and counter[], to produce the ciphertext ct[]                     */

__attribute__ ((aligned (16))) unsigned int SHAVITE_MESS[16];
__attribute__ ((aligned (16))) unsigned char SHAVITE_PTXT[8*4];
__attribute__ ((aligned (16))) unsigned int SHAVITE_CNTS[4] = {0,0,0,0}; 
__attribute__ ((aligned (16))) unsigned int SHAVITE_REVERSE[4] = {0x07060504, 0x0b0a0908, 0x0f0e0d0c, 0x03020100 };
__attribute__ ((aligned (16))) unsigned int SHAVITE256_XOR2[4] = {0x0, 0xFFFFFFFF, 0x0, 0x0};
__attribute__ ((aligned (16))) unsigned int SHAVITE256_XOR3[4] = {0x0, 0x0, 0xFFFFFFFF, 0x0};
__attribute__ ((aligned (16))) unsigned int SHAVITE256_XOR4[4] = {0x0, 0x0, 0x0, 0xFFFFFFFF};


#define mixing() do {\
   asm("movaps  xmm11, xmm15");\
   asm("movaps  xmm10, xmm14");\
   asm("movaps  xmm9,  xmm13");\
   asm("movaps  xmm8,  xmm12");\
\
   asm("movaps  xmm6,  xmm11");\
   asm("psrldq  xmm6,  4");\
   asm("pxor    xmm8,  xmm6");\
   asm("movaps  xmm6,  xmm8");\
   asm("pslldq  xmm6,  12");\
   asm("pxor    xmm8,  xmm6");\
\
   asm("movaps  xmm7,  xmm8");\
   asm("psrldq  xmm7,  4");\
   asm("pxor    xmm9,  xmm7");\
   asm("movaps  xmm7,  xmm9");\
   asm("pslldq  xmm7,  12");\
   asm("pxor    xmm9,  xmm7");\
\
   asm("movaps  xmm6,  xmm9");\
   asm("psrldq  xmm6,  4");\
   asm("pxor    xmm10, xmm6");\
   asm("movaps  xmm6,  xmm10");\
   asm("pslldq  xmm6,  12");\
   asm("pxor    xmm10, xmm6");\
\
   asm("movaps  xmm7,  xmm10");\
   asm("psrldq  xmm7,  4");\
   asm("pxor    xmm11, xmm7");\
   asm("movaps  xmm7,  xmm11");\
   asm("pslldq  xmm7,  12");\
   asm("pxor    xmm11, xmm7");\
} while(0);

void E256()
{
   asm (".intel_syntax noprefix");

   /* (L,R) = (xmm0,xmm1) */
   asm ("movaps xmm0, SHAVITE_PTXT[rip]");
   asm ("movaps xmm1, SHAVITE_PTXT[rip+16]");
   asm ("movaps xmm3, SHAVITE_CNTS[rip]");
   asm ("movaps xmm4, SHAVITE256_XOR2[rip]");
   asm ("pxor   xmm2,  xmm2");

   /* init key schedule */
   asm ("movaps xmm8,  SHAVITE_MESS[rip]");
   asm ("movaps xmm9,  SHAVITE_MESS[rip+16]");
   asm ("movaps xmm10, SHAVITE_MESS[rip+32]");
   asm ("movaps xmm11, SHAVITE_MESS[rip+48]");

   /* xmm8..xmm11 = rk[0..15] */

   /* start key schedule */
   asm ("movaps xmm12, xmm8");
   asm ("movaps xmm13, xmm9");
   asm ("movaps xmm14, xmm10");
   asm ("movaps xmm15, xmm11");

   rev_reg_0321(12);
   rev_reg_0321(13);
   rev_reg_0321(14);
   rev_reg_0321(15);
   replace_aes(12, 2);
   replace_aes(13, 2);
   replace_aes(14, 2);
   replace_aes(15, 2);

   asm ("pxor   xmm12, xmm3");
   asm ("pxor   xmm12, xmm4");
   asm ("movaps xmm4, SHAVITE256_XOR3[rip]");
   asm ("pxor   xmm12, xmm11");
   asm ("pxor   xmm13, xmm12");
   asm ("pxor   xmm14, xmm13");
   asm ("pxor   xmm15, xmm14");
   /* xmm12..xmm15 = rk[16..31] */
   
   /* F3 - first round */

   asm ("movaps xmm6,  xmm8");
   asm ("pxor   xmm8,  xmm1");
   replace_aes(8, 9);
   replace_aes(8, 10);
   replace_aes(8, 2);
   asm ("pxor   xmm0,  xmm8");
   asm ("movaps xmm8,  xmm6");
   
   /* F3 - second round */

   asm ("movaps xmm6,  xmm11");
   asm ("pxor   xmm11, xmm0");
   replace_aes(11, 12);
   replace_aes(11, 13);
   replace_aes(11, 2);
   asm ("pxor   xmm1,  xmm11");
   asm ("movaps xmm11,  xmm6");

   /* key schedule */
   mixing();

   /* xmm8..xmm11 - rk[32..47] */

   /* F3 - third round */
   asm ("movaps xmm6, xmm14");
   asm ("pxor   xmm14, xmm1");
   replace_aes(14, 15);
   replace_aes(14, 8);
   replace_aes(14, 2);
   asm ("pxor   xmm0,  xmm14");
   asm ("movaps xmm14,  xmm6");

   /* key schedule */

   asm ("pshufd xmm3,  xmm3,135");

   asm ("movaps xmm12, xmm8");
   asm ("movaps xmm13, xmm9");
   asm ("movaps xmm14, xmm10");
   asm ("movaps xmm15, xmm11");
   rev_reg_0321(12);
   rev_reg_0321(13);
   rev_reg_0321(14);
   rev_reg_0321(15);
   replace_aes(12, 2);
   replace_aes(13, 2);
   replace_aes(14, 2);
   replace_aes(15, 2);
   
   asm ("pxor   xmm12, xmm11");
   asm ("pxor   xmm14, xmm3");
   asm ("pxor   xmm14, xmm4");
   asm ("movaps xmm4, SHAVITE256_XOR4[rip]");
   asm ("pxor   xmm13, xmm12");
   asm ("pxor   xmm14, xmm13");
   asm ("pxor   xmm15, xmm14");
   
   /* xmm12..xmm15 - rk[48..63] */

   /* F3 - fourth round */
   asm ("movaps xmm6, xmm9");
   asm ("pxor   xmm9, xmm0");
   replace_aes(9, 10);
   replace_aes(9, 11);
   replace_aes(9, 2);
   asm ("pxor   xmm1,  xmm9");
   asm ("movaps xmm9,  xmm6");

   /* key schedule */
   mixing();
   /* xmm8..xmm11 = rk[64..79] */

   /* F3  - fifth round */
   asm ("movaps xmm6,  xmm12");
   asm ("pxor   xmm12,  xmm1");
   replace_aes(12, 13);
   replace_aes(12, 14);
   replace_aes(12, 2);
   asm ("pxor   xmm0,  xmm12");
   asm ("movaps xmm12,  xmm6");
   
   /* F3 - sixth round */
   asm ("movaps xmm6,  xmm15");
   asm ("pxor   xmm15, xmm0");
   replace_aes(15, 8);
   replace_aes(15, 9);
   replace_aes(15, 2);
   asm ("pxor   xmm1,  xmm15");
   asm ("movaps xmm15,  xmm6");

   /* key schedule */
   asm ("pshufd xmm3,  xmm3, 147");

   asm ("movaps xmm12, xmm8");
   asm ("movaps xmm13, xmm9");
   asm ("movaps xmm14, xmm10");
   asm ("movaps xmm15, xmm11");
   rev_reg_0321(12);
   rev_reg_0321(13);
   rev_reg_0321(14);
   rev_reg_0321(15);
   replace_aes(12, 2);
   replace_aes(13, 2);
   replace_aes(14, 2);
   replace_aes(15, 2);
   asm ("pxor   xmm12, xmm11");
   asm ("pxor   xmm13, xmm3");
   asm ("pxor   xmm13, xmm4");
   asm ("pxor   xmm13, xmm12");
   asm ("pxor   xmm14, xmm13");
   asm ("pxor   xmm15, xmm14");

   /* xmm12..xmm15 = rk[80..95] */

   /* F3 - seventh round */
   asm ("movaps xmm6,  xmm10");
   asm ("pxor   xmm10,  xmm1");
   replace_aes(10, 11);
   replace_aes(10, 12);
   replace_aes(10, 2);
   asm ("pxor   xmm0,  xmm10");
   asm ("movaps xmm10,  xmm6");

   /* key schedule */
   mixing();

   /* xmm8..xmm11 = rk[96..111] */

   /* F3 - eigth round */
   asm ("movaps xmm6, xmm13");
   asm ("pxor   xmm13, xmm0");
   replace_aes(13, 14);
   replace_aes(13, 15);
   replace_aes(13, 2);
   asm ("pxor   xmm1,  xmm13");
   asm ("movaps xmm13,  xmm6");


   /* key schedule */
   asm ("pshufd xmm3,  xmm3, 135");

   asm ("movaps xmm12, xmm8");
   asm ("movaps xmm13, xmm9");
   asm ("movaps xmm14, xmm10");
   asm ("movaps xmm15, xmm11");
   rev_reg_0321(12);
   rev_reg_0321(13);
   rev_reg_0321(14);
   rev_reg_0321(15);
   replace_aes(12, 2);
   replace_aes(13, 2);
   replace_aes(14, 2);
   replace_aes(15, 2);
   asm ("pxor   xmm12, xmm11");
   asm ("pxor   xmm15, xmm3");
   asm ("pxor   xmm15, xmm4");
   asm ("pxor   xmm13, xmm12");
   asm ("pxor   xmm14, xmm13");
   asm ("pxor   xmm15, xmm14");

   /* xmm12..xmm15 = rk[112..127] */
   
   /* F3 - ninth round */
   asm ("movaps xmm6,  xmm8");
   asm ("pxor   xmm8,  xmm1");
   replace_aes(8, 9);
   replace_aes(8, 10);
   replace_aes(8, 2);
   asm ("pxor   xmm0,  xmm8");
   asm ("movaps xmm8,  xmm6");
   /* F3 - tenth round */
   asm ("movaps xmm6,  xmm11");
   asm ("pxor   xmm11, xmm0");
   replace_aes(11, 12);
   replace_aes(11, 13);
   replace_aes(11, 2);
   asm ("pxor   xmm1,  xmm11");
   asm ("movaps xmm11,  xmm6");

   /* key schedule */
   mixing();

   /* xmm8..xmm11 = rk[128..143] */

   /* F3 - eleventh round */
   asm ("movaps xmm6,  xmm14");
   asm ("pxor   xmm14,  xmm1");
   replace_aes(14, 15);
   replace_aes(14, 8);
   replace_aes(14, 2);
   asm ("pxor   xmm0,  xmm14");
   asm ("movaps xmm14,  xmm6");

   /* F3 - twelfth round */
   asm ("movaps xmm6,  xmm9");
   asm ("pxor   xmm9, xmm0");
   replace_aes(9, 10);
   replace_aes(9, 11);
   replace_aes(9, 2);
   asm ("pxor   xmm1,  xmm9");
   asm ("movaps xmm9,  xmm6");


   /* feedforward */
   asm ("pxor   xmm0,  SHAVITE_PTXT[rip]");
   asm ("pxor   xmm1,  SHAVITE_PTXT[rip+16]");
   asm ("movaps SHAVITE_PTXT[rip],    xmm0");
   asm ("movaps SHAVITE_PTXT[rip+16], xmm1");
   asm (".att_syntax noprefix");

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
