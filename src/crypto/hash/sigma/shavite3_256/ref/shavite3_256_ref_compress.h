// File originates from the supercop project
// Authors: Eli Biham and Orr Dunkelman
//
// File contains modifications by: The Centure developers
// All modifications:
// Copyright (c) 2019-2022 The Centure developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the Libre Chain License, see the accompanying
// file COPYING

#ifndef SHAVITE_REF_256_H
#define SHAVITE_REF_256_H

#include "portable.h"
#include "AESround.h"
#include "shavite3_ref.h"


#define InternalRounds 3
#define ExternalRounds 12
#define ExpandedMessageSize (ExternalRounds*4*(InternalRounds))

// The message expansion takes a 16 words (stored in rk[0,.,15]) and the counter values to produce the full expanded message
// Generates 16 more words in rk[] using the nonlinear expansion step new words: rk[16..31]
#define NonLinExpansion16(rk,counter,x0,x1,x2,x3,y0,y1,y2,y3)\
{\
	x0 = rk[1];\
        x1 = rk[2];\
        x2 = rk[3];\
        x3 = rk[0];\
        roundAESnokey(x0,x1,x2,x3,y0,y1,y2,y3);\
        rk[16] = y0^rk[12]^counter[0];\
        rk[17] = y1^rk[13]^~counter[1];\
        rk[18] = y2^rk[14];\
        rk[19] = y3^rk[15];\
        x0 = rk[5];\
        x1 = rk[6];\
        x2 = rk[7];\
        x3 = rk[4];\
        roundAESnokey(x0,x1,x2,x3,y0,y1,y2,y3);\
        rk[20] = y0^rk[16];\
        rk[21] = y1^rk[17];\
        rk[22] = y2^rk[18];\
        rk[23] = y3^rk[19];\
	x0 = rk[9];\
        x1 = rk[10];\
        x2 = rk[11];\
        x3 = rk[8];\
        roundAESnokey(x0,x1,x2,x3,y0,y1,y2,y3);\
        rk[24] = y0^rk[20];\
        rk[25] = y1^rk[21];\
        rk[26] = y2^rk[22];\
        rk[27] = y3^rk[23];\
        x0 = rk[13];\
        x1 = rk[14];\
        x2 = rk[15];\
        x3 = rk[12];\
        roundAESnokey(x0,x1,x2,x3,y0,y1,y2,y3);\
        rk[28] = y0^rk[24];\
        rk[29] = y1^rk[25];\
        rk[30] = y2^rk[26];\
        rk[31] = y3^rk[27];\
}

// Generates 16 more words in rk[] using the nonlinear expansion step new words: rk[48..63]
#define NonLinExpansion48(rk,counter,x0,x1,x2,x3,y0,y1,y2,y3)\
{\
    x0 = rk[33];\
    x1 = rk[34];\
    x2 = rk[35];\
    x3 = rk[32];\
    roundAESnokey(x0,x1,x2,x3,y0,y1,y2,y3);\
    rk[48] = y0^rk[44];\
    rk[49] = y1^rk[45];\
    rk[50] = y2^rk[46];\
    rk[51] = y3^rk[47];\
    x0 = rk[37];\
    x1 = rk[38];\
    x2 = rk[39];\
    x3 = rk[36];\
    roundAESnokey(x0,x1,x2,x3,y0,y1,y2,y3);\
    rk[52] = y0^rk[48];\
    rk[53] = y1^rk[49];\
    rk[54] = y2^rk[50];\
    rk[55] = y3^rk[51];\
    x0 = rk[41];\
    x1 = rk[42];\
    x2 = rk[43];\
    x3 = rk[40];\
    roundAESnokey(x0,x1,x2,x3,y0,y1,y2,y3);\
    rk[56] = y0^rk[52];\
    rk[57] = y1^rk[53]^counter[1];\
    rk[58] = y2^rk[54]^~counter[0];\
    rk[59] = y3^rk[55];\
    x0 = rk[45];\
    x1 = rk[46];\
    x2 = rk[47];\
    x3 = rk[44];\
    roundAESnokey(x0,x1,x2,x3,y0,y1,y2,y3);\
    rk[60] = y0^rk[56];\
    rk[61] = y1^rk[57];\
    rk[62] = y2^rk[58];\
    rk[63] = y3^rk[59];\
}

// Generates 16 more words in rk[] using the nonlinear expansion step new words: rk[80..95]
#define NonLinExpansion80(rk,counter,x0,x1,x2,x3,y0,y1,y2,y3)\
{\
    x0 = rk[65];\
    x1 = rk[66];\
    x2 = rk[67];\
    x3 = rk[64];\
    roundAESnokey(x0,x1,x2,x3,y0,y1,y2,y3);\
    rk[80] = y0^rk[76];\
    rk[81] = y1^rk[77];\
    rk[82] = y2^rk[78];\
    rk[83] = y3^rk[79];\
    x0 = rk[69];\
    x1 = rk[70];\
    x2 = rk[71];\
    x3 = rk[68];\
    roundAESnokey(x0,x1,x2,x3,y0,y1,y2,y3);\
    rk[84] = y0^rk[80];\
    rk[85] = y1^rk[81];\
    rk[86] = y2^rk[82]^counter[1];\
    rk[87] = y3^rk[83]^~counter[0];\
    x0 = rk[73];\
    x1 = rk[74];\
    x2 = rk[75];\
    x3 = rk[72];\
    roundAESnokey(x0,x1,x2,x3,y0,y1,y2,y3);\
    rk[88] = y0^rk[84];\
    rk[89] = y1^rk[85];\
    rk[90] = y2^rk[86];\
    rk[91] = y3^rk[87];\
    x0 = rk[77];\
    x1 = rk[78];\
    x2 = rk[79];\
    x3 = rk[76];\
    roundAESnokey(x0,x1,x2,x3,y0,y1,y2,y3);\
    rk[92] = y0^rk[88];\
    rk[93] = y1^rk[89];\
    rk[94] = y2^rk[90];\
    rk[95] = y3^rk[91];\
}

// Generates 16 more words in rk[] using the nonlinear expansion step new words: rk[112..127]
#define NonLinExpansion112(rk,counter,x0,x1,x2,x3,y0,y1,y2,y3)\
{\
    x0 = rk[97];\
    x1 = rk[98];\
    x2 = rk[99];\
    x3 = rk[96];\
    roundAESnokey(x0,x1,x2,x3,y0,y1,y2,y3);\
    rk[112] = y0^rk[108];\
    rk[113] = y1^rk[109];\
    rk[114] = y2^rk[110];\
    rk[115] = y3^rk[111];\
    x0 = rk[101];\
    x1 = rk[102];\
    x2 = rk[103];\
    x3 = rk[100];\
    roundAESnokey(x0,x1,x2,x3,y0,y1,y2,y3);\
    rk[116] = y0^rk[112];\
    rk[117] = y1^rk[113];\
    rk[118] = y2^rk[114];\
    rk[119] = y3^rk[115];\
    x0 = rk[105];\
    x1 = rk[106];\
    x2 = rk[107];\
    x3 = rk[104];\
    roundAESnokey(x0,x1,x2,x3,y0,y1,y2,y3);\
    rk[120] = y0^rk[116];\
    rk[121] = y1^rk[117];\
    rk[122] = y2^rk[118];\
    rk[123] = y3^rk[119];\
    x0 = rk[109];\
    x1 = rk[110];\
    x2 = rk[111];\
    x3 = rk[108];\
    roundAESnokey(x0,x1,x2,x3,y0,y1,y2,y3);\
    rk[124] = y0^rk[120]^counter[0];\
    rk[125] = y1^rk[121];\
    rk[126] = y2^rk[122];\
    rk[127] = y3^rk[123]^~counter[1];\
}

#define LinExpansion(rk,position,temp0,temp1,temp2)\
{\
   temp0=position; temp1=position-16; temp2=position-3;\
   for (;temp1<position;temp0++,temp1++,temp2++)\
     rk[temp0] = rk[temp1]^rk[temp2];\
}


#define two_rounds(state0,state1,state2,state3,state4,state5,state6,state7,x0,x1,x2,x3,y0,y1,y2,y3,rk,position)\
{\
   x0 = state4^rk[position];\
   x1 = state5^rk[position+1];\
   x2 = state6^rk[position+2];\
   x3 = state7^rk[position+3];\
   roundAES(x0,x1,x2,x3,y0,y1,y2,y3,rk[position+4],rk[position+5],rk[position+6],rk[position+7]);\
   roundAES(y0,y1,y2,y3,x0,x1,x2,x3,rk[position+8],rk[position+9],rk[position+10],rk[position+11]);\
   roundAESnokey(x0,x1,x2,x3,y0,y1,y2,y3);\
   state0 ^= y0;\
   state1 ^= y1;\
   state2 ^= y2;\
   state3 ^= y3;\
   x0 = state0^rk[position+12];\
   x1 = state1^rk[position+13];\
   x2 = state2^rk[position+14];\
   x3 = state3^rk[position+15];\
   roundAES(x0,x1,x2,x3,y0,y1,y2,y3,rk[position+16],rk[position+17],rk[position+18],rk[position+19]);\
   roundAES(y0,y1,y2,y3,x0,x1,x2,x3,rk[position+20],rk[position+21],rk[position+22],rk[position+23]);\
   roundAESnokey(x0,x1,x2,x3,y0,y1,y2,y3);\
   state4^=y0;\
   state5^=y1;\
   state6^=y2;\
   state7^=y3;\
}

inline void E256_ref(uint32_t pt[8], uint32_t ct[8], uint32_t message[16], uint32_t counter[2])
{
    uint32_t state0,state1,state2,state3,state4,state5,state6,state7,i,j,k;
    uint32_t x0,x1,x2,x3,y0,y1,y2,y3;
    uint32_t rk[ExpandedMessageSize];

    state0=pt[0]; 
    state1=pt[1]; 
    state2=pt[2]; 
    state3=pt[3];
    state4=pt[4];
    state5=pt[5];
    state6=pt[6];
    state7=pt[7];
    for (i=0;i<16;i++) rk[i]=message[i];

    NonLinExpansion16(rk,counter,x0,x1,x2,x3,y0,y1,y2,y3);

    two_rounds(state0,state1,state2,state3,state4,state5,state6,state7,y0,y1,y2,y3,x0,x1,x2,x3,rk,0);
    LinExpansion(rk,32,i,j,k);
    two_rounds(state0,state1,state2,state3,state4,state5,state6,state7,y0,y1,y2,y3,x0,x1,x2,x3,rk,24);
    NonLinExpansion48(rk,counter,x0,x1,x2,x3,y0,y1,y2,y3);
    LinExpansion(rk,64,i,j,k);
    two_rounds(state0,state1,state2,state3,state4,state5,state6,state7,y0,y1,y2,y3,x0,x1,x2,x3,rk,48);
    NonLinExpansion80(rk,counter,x0,x1,x2,x3,y0,y1,y2,y3);
    two_rounds(state0,state1,state2,state3,state4,state5,state6,state7,y0,y1,y2,y3,x0,x1,x2,x3,rk,72);
    LinExpansion(rk,96,i,j,k);
    NonLinExpansion112(rk,counter,x0,x1,x2,x3,y0,y1,y2,y3);
    two_rounds(state0,state1,state2,state3,state4,state5,state6,state7,y0,y1,y2,y3,x0,x1,x2,x3,rk,96);
    LinExpansion(rk,128,i,j,k);
    two_rounds(state0,state1,state2,state3,state4,state5,state6,state7,y0,y1,y2,y3,x0,x1,x2,x3,rk,120);

    ct[0]=state0; 
    ct[1]=state1; 
    ct[2]=state2; 
    ct[3]=state3;
    ct[4]=state4;
    ct[5]=state5;
    ct[6]=state6;
    ct[7]=state7;

    return;
}

// The actual compression function C_{256}
inline void Compress256_ref(const uint8_t* message_block, uint8_t* chaining_value, uint64_t counter)
{    
    uint32_t pt[8],ct[8];
    uint32_t msg_u32[16];
    uint32_t cnt[2];
    int i;

    // Translating all the inputs to 32-bit words
    for (i=0;i<8;i++)
    {
        pt[i]=U8TO32_LITTLE(chaining_value+4*i);
    }

    for (i=0;i<16;i++)
    {
        msg_u32[i]=U8TO32_LITTLE(message_block+4*i);
    }

    cnt[1]=(uint32_t)(counter>>32);
    cnt[0]=(uint32_t)(counter & 0xFFFFFFFFULL);

    // Computing the encryption function
    E256_ref(pt, ct, msg_u32, cnt);

    // Davies-Meyer transformation
    for (i=0; i<8; i++)
    {
        pt[i]^=ct[i];
    }

    // Translating the output to 8-bit words
    for (i=0; i<8; i++)
    {
        U32TO8_LITTLE(chaining_value+i*4, pt[i]);
    }

   return;
}

#endif
