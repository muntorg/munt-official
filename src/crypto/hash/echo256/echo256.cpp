/*------------------------------------------------------------------------------------ */
/* Implementation of the double pipe ECHO hash function in its 256-bit outputs variant.*/
/* Optimized for ANSI C, 64-bit mode                                                   */
/*                                                                                     */
/* Date:     2010-07-23                                                                */
/*                                                                                     */
/* Authors:  Ryad Benadjila  <ryadbenadjila@gmail.com>                                 */
/*           Olivier Billet  <billet@eurecom.fr>                                       */
/*------------------------------------------------------------------------------------ */
// File contains modifications by: The Gulden developers
// All modifications:
// Copyright (c) 2019 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING
#include "echo256.h"

#define SALT_A 0
#define SALT_B 0

/***************************Endianess definitions********************************/
#define  USINT     0
#define  UINT      1
#define  ULL       2

#define ET_BIG_ENDIAN 		0
#define ET_LITTLE_ENDIAN	1
#define ET_MIDDLE_ENDIAN	2


unsigned char endian;

/***************************Endianess routines***********************************/
unsigned char Endianess(){
/*	Endianess test								*/
	unsigned int T;
	unsigned char T_o[8];

	T = 0xAABBCCDD;
	*(unsigned int*)T_o = T;
	if(T_o[0] == 0xAA){
		return ET_BIG_ENDIAN;
	}
	if(T_o[0] == 0xDD){
		return ET_LITTLE_ENDIAN;
	}
	else{
		return ET_MIDDLE_ENDIAN;
	}
}

void PushString(unsigned long long num, unsigned char* string, unsigned char type){
	unsigned int j;

	if(type == USINT){
		for(j=0; j<2; j++){
	  		string[j] = (num >> (j*8)) & 0xFF;
		}
	}
	if(type == UINT){
		for(j=0; j<4; j++){
	  		string[j] = (num >> (j*8)) & 0xFF;
		}
	}
	if(type == ULL){
		for(j=0; j<8; j++){
	  		string[j] = (num >> (j*8)) & 0xFF;
		}
	}

	return;
}

#ifdef VSALT
/***************************Changing SALT routine  ***********************************/
HashReturn SetSalt(hashState* state, const BitSequence* SALT){
	unsigned long long topush = *((unsigned long long*)SALT);
	PushString(topush, state->SALT, ULL);
	topush = *((unsigned long long*)(SALT+8));
	PushString(topush, state->SALT+8, ULL);

	return SUCCESS;
}
#endif


HashReturn Init(hashState *state, int hashbitlen)
{
  int i;
  
  /* record the requested hash bit length */
  if((hashbitlen >= 0) && (hashbitlen <= 512)){
    state->hashbitlen = hashbitlen;
  } 
  else 
    return BAD_HASHBITLEN;

  /* number of 128 bit blocs used by the IV */
  if(hashbitlen > 256) 
    state->cv_blocks = 8;
  else 
    state->cv_blocks = 4; 
    
  /* initialize the IV part of the internal state.
  ** the 128 bit words are split into two 64 bit words */
  for(i=0; i<state->cv_blocks; i++){
    PushString(hashbitlen, (unsigned char*)(state->state+2*i), ULL);
    state->state[2*i+1]= 0;
  }
  
  /* fill the rest of the state with zeros */
  for(i=state->cv_blocks; i<16; i++){
    state->state[2*i]  = 0;
    state->state[2*i+1]= 0;
  }

  /* set the counter to one */
  state->counter = 0;

  /* length of message curently hold in bits */
  state->message_bitlen = 0;

#ifdef VSALT
  /* set the default salt */
  memset(state->SALT, 0, 16);
#endif

  /*	Endianess testing	   */
  endian = Endianess();

  return SUCCESS;
}


#define lbyte(k)  ((k)        & 0xff)
#define hbyte(k) (((k) >>  8) & 0xff)
#define sbyte(k) (((k) >> 16) & 0xff)
#define mbyte(k) (((k) >> 24) & 0xff)

#define Lbyte(k) (((k) >> 32) & 0xff)
#define Hbyte(k) (((k) >> 40) & 0xff)
#define Sbyte(k) (((k) >> 48) & 0xff)
#define Mbyte(k)  ((k) >> 56)

#define linteger(s, i) (*((unsigned long long*)(s) + i))
#ifdef VSALT
#define AESround(i) do { \
  unsigned int WA, WB, WC, WD; \
\
      WA = T0[ lbyte(WL##i) ] ^ T1[ Hbyte(WL##i) ] ^ T2[ sbyte(WH##i) ] ^ T3[ Mbyte(WH##i) ] ^ ((unsigned int)CNT);\
      WB = T0[ Lbyte(WL##i) ] ^ T1[ hbyte(WH##i) ] ^ T2[ Sbyte(WH##i) ] ^ T3[ mbyte(WL##i) ] ^ ((unsigned long long)CNT >> 32);\
      WC = T0[ lbyte(WH##i) ] ^ T1[ Hbyte(WH##i) ] ^ T2[ sbyte(WL##i) ] ^ T3[ Mbyte(WL##i) ];\
      WD = T0[ Lbyte(WH##i) ] ^ T1[ hbyte(WL##i) ] ^ T2[ Sbyte(WL##i) ] ^ T3[ mbyte(WH##i) ]; \
\
      CNT++;\
\
      WL##i = (unsigned long long)(T0[ lbyte(WA) ] ^ T1[ hbyte(WB) ] ^ T2[ sbyte(WC) ] ^ T3[ mbyte(WD) ]);\
      WL##i^=((unsigned long long)(T0[ lbyte(WB) ] ^ T1[ hbyte(WC) ] ^ T2[ sbyte(WD) ] ^ T3[ mbyte(WA) ]) << 32);\
      WH##i = (unsigned long long)(T0[ lbyte(WC) ] ^ T1[ hbyte(WD) ] ^ T2[ sbyte(WA) ] ^ T3[ mbyte(WB) ]);\
      WH##i^=((unsigned long long)(T0[ lbyte(WD) ] ^ T1[ hbyte(WA) ] ^ T2[ sbyte(WB) ] ^ T3[ mbyte(WC) ]) << 32);\
\
      WL##i^= linteger(state->SALT, 0); \
      WH##i^= linteger(state->SALT, 1); \
} while(0);
#else
#define AESround(i) do { \
  unsigned int WA, WB, WC, WD; \
\
      WA = T0[ lbyte(WL##i) ] ^ T1[ Hbyte(WL##i) ] ^ T2[ sbyte(WH##i) ] ^ T3[ Mbyte(WH##i) ] ^ ((unsigned int)CNT);\
      WB = T0[ Lbyte(WL##i) ] ^ T1[ hbyte(WH##i) ] ^ T2[ Sbyte(WH##i) ] ^ T3[ mbyte(WL##i) ] ^ ((unsigned long long)CNT >> 32);\
      WC = T0[ lbyte(WH##i) ] ^ T1[ Hbyte(WH##i) ] ^ T2[ sbyte(WL##i) ] ^ T3[ Mbyte(WL##i) ] ^ (unsigned int)SALT_A;\
      WD = T0[ Lbyte(WH##i) ] ^ T1[ hbyte(WL##i) ] ^ T2[ Sbyte(WL##i) ] ^ T3[ mbyte(WH##i) ] ^ (unsigned int)((unsigned long long)SALT_A >> 32); \
\
      CNT++;\
\
      WL##i = (unsigned long long)(T0[ lbyte(WA) ] ^ T1[ hbyte(WB) ] ^ T2[ sbyte(WC) ] ^ T3[ mbyte(WD) ]);\
      WL##i^=((unsigned long long)(T0[ lbyte(WB) ] ^ T1[ hbyte(WC) ] ^ T2[ sbyte(WD) ] ^ T3[ mbyte(WA) ]) << 32);\
      WH##i = (unsigned long long)(T0[ lbyte(WC) ] ^ T1[ hbyte(WD) ] ^ T2[ sbyte(WA) ] ^ T3[ mbyte(WB) ]);\
      WH##i^=((unsigned long long)(T0[ lbyte(WD) ] ^ T1[ hbyte(WA) ] ^ T2[ sbyte(WB) ] ^ T3[ mbyte(WC) ]) << 32);\
\
      WH##i^= SALT_B; \
} while(0);
#endif

#define FEEDforward do { \
 if(state->cv_blocks < 8){ \
    WL8  ^= WL12 ^ WL4  ^ WL0 ; \
    WH8  ^= WH12 ^ WH4  ^ WH0 ; \
    WL9  ^= WL13 ^ WL5  ^ WL1 ; \
    WH9  ^= WH13 ^ WH5  ^ WH1 ; \
    WL10 ^= WL14 ^ WL6  ^ WL2 ; \
    WH10 ^= WH14 ^ WH6  ^ WH2 ; \
    WL11 ^= WL15 ^ WL7  ^ WL3 ; \
    WH11 ^= WH15 ^ WH7  ^ WH3 ; \
    state->state[0] =state->CV[0]^ WL8 ; \
    state->state[1] =state->CV[1]^ WH8 ; \
    state->state[2] =state->CV[2]^ WL9 ; \
    state->state[3] =state->CV[3]^ WH9 ; \
    state->state[4] =state->CV[4]^ WL10; \
    state->state[5] =state->CV[5]^ WH10; \
    state->state[6] =state->CV[6]^ WL11; \
    state->state[7] =state->CV[7]^ WH11; \
  } else { \
    state->state[0]  =state->CV[0]   ^ WL0 ^ WL8 ; \
    state->state[1]  =state->CV[1]   ^ WH0 ^ WH8 ; \
    state->state[2]  =state->CV[2]   ^ WL1 ^ WL9 ; \
    state->state[3]  =state->CV[3]   ^ WH1 ^ WH9 ; \
    state->state[4]  =state->CV[4]   ^ WL2 ^ WL10; \
    state->state[5]  =state->CV[5]   ^ WH2 ^ WH10; \
    state->state[6]  =state->CV[6]   ^ WL3 ^ WL11; \
    state->state[7]  =state->CV[7]   ^ WH3 ^ WH11; \
    state->state[8+0]=state->CV[8+0] ^ WL4 ^ WL12; \
    state->state[8+1]=state->CV[8+1] ^ WH4 ^ WH12; \
    state->state[8+2]=state->CV[8+2] ^ WL5 ^ WL13; \
    state->state[8+3]=state->CV[8+3] ^ WH5 ^ WH13; \
    state->state[8+4]=state->CV[8+4] ^ WL6 ^ WL14; \
    state->state[8+5]=state->CV[8+5] ^ WH6 ^ WH14; \
    state->state[8+6]=state->CV[8+6] ^ WL7 ^ WL15; \
    state->state[8+7]=state->CV[8+7] ^ WH7 ^ WH15; \
  } \
} while(0);

#define LOADs(i) do { \
  WL##i = state->state[2*i]; \
  WH##i = state->state[2*i+1]; \
} while(0);

#define LOADstate do { \
  LOADs(0); LOADs(1); LOADs(2); LOADs(3); \
  LOADs(4); LOADs(5); LOADs(6); LOADs(7); \
  LOADs(8); LOADs(9); LOADs(10);LOADs(11);\
  LOADs(12);LOADs(13);LOADs(14);LOADs(15);\
} while(0);

#define MDShelp(WL0, WL1, WL2, WL3) do { \
        WT0  = ((WL0 >> 7) & 0x0101010101010101ULL) *27;\
        WT0 ^= ((WL0 << 1) & 0xfefefefefefefefeULL);\
        WT1  = ((WL1 >> 7) & 0x0101010101010101ULL) *27;\
        WT1 ^= ((WL1 << 1) & 0xfefefefefefefefeULL);\
        WT2  = ((WL2 >> 7) & 0x0101010101010101ULL) *27;\
        WT2 ^= ((WL2 << 1) & 0xfefefefefefefefeULL);\
        WT3  = ((WL3 >> 7) & 0x0101010101010101ULL) *27;\
        WT3 ^= ((WL3 << 1) & 0xfefefefefefefefeULL);\
\
        WT4  =  WL0 ^ WL1 ^ WL2 ^ WL3; \
        WL0 ^= WT0 ^ WT1 ^ WT4;\
        WL1 ^= WT1 ^ WT2 ^ WT4;\
        WL2 ^= WT2 ^ WT3 ^ WT4;\
        WL3 ^= WT0 ^ WT4 ^ WT3;  \
} while(0);

#define SWAP(a,b) do { \
  WT0 = WL##a;  \
  WL##a = WL##b;\
  WL##b = WT0;\
  WT1 = WH##a;  \
  WH##a = WH##b;\
  WH##b = WT1;\
} while(0); 

#define lint_revert(in)  do {\
	unsigned int j;\
	unsigned long long temp = 0;\
	for(j=0; j<8; j++)\
		temp  ^= (((in)>>(8*(7-j)))&0xff) << (8*j);\
	in = temp;\
}while(0);

#define reverse_state(state) do {\
	lint_revert (WL0); lint_revert (WH0);\
	lint_revert (WL1); lint_revert (WH1);\
	lint_revert (WL2); lint_revert (WH2);\
	lint_revert (WL3); lint_revert (WH3);\
	lint_revert (WL4); lint_revert (WH4);\
	lint_revert (WL5); lint_revert (WH5);\
	lint_revert (WL6); lint_revert (WH6);\
	lint_revert (WL7); lint_revert (WH7);\
	lint_revert (WL8); lint_revert (WH8);\
	lint_revert (WL9); lint_revert (WH9);\
	lint_revert (WL10);lint_revert (WH10);\
	lint_revert (WL11);lint_revert (WH11);\
	lint_revert (WL12);lint_revert (WH12);\
	lint_revert (WL13);lint_revert (WH13);\
	lint_revert (WL14);lint_revert (WH14);\
	lint_revert (WL15);lint_revert (WH15);\
} while(0);

void Compress(hashState *state)
{
  unsigned long long
    WL0, WH0, WL1, WH1, WL2, WH2, WL3, WH3,
    WL4, WH4, WL5, WH5, WL6, WH6, WL7, WH7,
    WL8, WH8, WL9, WH9, WL10,WH10,WL11,WH11,
    WL12,WH12,WL13,WH13,WL14,WH14,WL15,WH15,
    WT0, WT1, WT2, WT3, WT4;
  unsigned long long CNT = 0;
  int r; 

  if(state->cv_blocks < 8){
    /* 256 bits */
  	state->CV[0]=state->state[0]; 
  	state->CV[1]=state->state[1];
  	state->CV[2]=state->state[2];
  	state->CV[3]=state->state[3]; 
  	state->CV[4]=state->state[4]; 
  	state->CV[5]=state->state[5]; 
  	state->CV[6]=state->state[6]; 
  	state->CV[7]=state->state[7]; 
  	for(r=1;r<4;r++){
		state->CV[0]^=state->state[8*r+0]; 
		state->CV[1]^=state->state[8*r+1];
		state->CV[2]^=state->state[8*r+2];
		state->CV[3]^=state->state[8*r+3]; 
		state->CV[4]^=state->state[8*r+4]; 
		state->CV[5]^=state->state[8*r+5]; 
		state->CV[6]^=state->state[8*r+6]; 
		state->CV[7]^=state->state[8*r+7]; 
  	}
  }
  else{
    /* 512 bits */
  	state->CV[0] =state->state[0]  ^ state->state[16]; 
  	state->CV[1] =state->state[1]  ^ state->state[17];
  	state->CV[2] =state->state[2]  ^ state->state[18];
  	state->CV[3] =state->state[3]  ^ state->state[19];  
  	state->CV[4] =state->state[4]  ^ state->state[20];  
  	state->CV[5] =state->state[5]  ^ state->state[21];  
  	state->CV[6] =state->state[6]  ^ state->state[22];  
  	state->CV[7] =state->state[7]  ^ state->state[23];  
	state->CV[8] =state->state[8]  ^ state->state[24];  
	state->CV[9] =state->state[9]  ^ state->state[25]; 
	state->CV[10]=state->state[10] ^ state->state[26]; 
	state->CV[11]=state->state[11] ^ state->state[27];  
	state->CV[12]=state->state[12] ^ state->state[28];  
	state->CV[13]=state->state[13] ^ state->state[29];  
	state->CV[14]=state->state[14] ^ state->state[30];  
	state->CV[15]=state->state[15] ^ state->state[31];  
  }

  state->counter += state->message_bitlen;
  if(state->message_bitlen) CNT = state->counter;
  
  LOADstate; 
  r = 2*(3+(state->cv_blocks >> 2));

  if(endian == ET_BIG_ENDIAN){	
	reverse_state(state->state);
  }

do {
  AESround(0); AESround(1); AESround(2); AESround(3); 
  AESround(4); AESround(5); AESround(6); AESround(7); 
  AESround(8); AESround(9); AESround(10);AESround(11); 
  AESround(12);AESround(13);AESround(14);AESround(15); 

  SWAP(2,10); SWAP(1,5);  SWAP(3,15);
  MDShelp(WL0, WL1, WL2, WL3);
  MDShelp(WH0, WH1, WH2, WH3);
  SWAP(13,9); SWAP(11,7); 
  MDShelp(WL8, WL9, WL10,WL11);
  MDShelp(WH8, WH9, WH10,WH11);
  SWAP(6,14); SWAP(13,5); SWAP(15,7);
  MDShelp(WL4, WL5, WL6, WL7);
  MDShelp(WH4, WH5, WH6, WH7);
  MDShelp(WL12,WL13,WL14,WL15);
  MDShelp(WH12,WH13,WH14,WH15);

  r--;
} while(r);

  if(endian == ET_BIG_ENDIAN){	
	reverse_state(state->state);
  }

  FEEDforward;
  state->message_bitlen = 0;
}



HashReturn Update(hashState *state, const BitSequence *data,
                     DataLength databitlen)
{
  int length, message_size, cv_length;
  unsigned char *message;
  long long data_size;
  
  cv_length    = (state->cv_blocks << 4);
  message      = (unsigned char*)(state->state)+cv_length;
  data_size    = (databitlen >> 3);
  message_size = (state->message_bitlen >> 3);
  
  if(databitlen == 0){
	return SUCCESS;
  }  
  if(state->message_bitlen % 8 != 0){
	return UPDATE_WBITS_TWICE;
  }

  while(data_size > 0){
    length = (16*16) - cv_length - message_size;
    if(data_size >= length){
      memcpy(message + message_size, data, length);
      state->message_bitlen += (length << 3);
      Compress(state);
      message_size = 0;
      data += length;
      data_size -= length;
    } 
    else {
      memcpy(message + message_size, data, data_size);
      message_size += data_size;
      data += data_size;
      data_size = 0;
    }
  }
  
  state->message_bitlen = (message_size << 3);
  if(databitlen & 0x07){
    int dbl, i;
    unsigned char a = 0;
    dbl = (databitlen & 0x07);
    state->message_bitlen += dbl;
    for(i=0; i<dbl; i++) a |= (0x01 << (7-i));
    message[message_size] = (a & data[0]);
  }
  
  return SUCCESS;
}


HashReturn Final(hashState *state, BitSequence *hashval)
{
  int filled_space, block_length;
  unsigned char *message;
  unsigned long long cnt;
  unsigned int i;

  /* perform padding */
  block_length = 128*(16-state->cv_blocks);
  filled_space   = state->message_bitlen;

  message  = (unsigned char*)(state->state);
  message += (state->cv_blocks << 4);
  /* insert the mandatory 1
   * (and if we need to go to a next message block, fill with
   * zeros up to the next block and call Compress) */
  if((block_length-filled_space) < 145){
    message[filled_space/8] |= (1 << (7-filled_space%8));
    message[filled_space/8] &= (0xff << (7-filled_space%8));
    filled_space += 7-filled_space%8;
    if(filled_space > 0){
      memset(&(message[filled_space/8])+1, 0, (block_length-filled_space)/8);
      Compress(state);
      filled_space = 0;
    } 
  } else {
    /*  Add the 1 and fill the remaining bits of the byte with zeros  */
    message[filled_space/8] |= (1 << (7-filled_space%8));
    message[filled_space/8] &= (0xff << (7-filled_space%8));
    filled_space += 7-filled_space%8;
  }
  /* now fill with zeros up to the last 144 bits */
  message  = (unsigned char*)(state->state);
  message += (state->cv_blocks << 4);
  cnt = state->counter + state->message_bitlen;
  if(filled_space == 0){
    memset(&(message[filled_space/8])  , 0, (block_length-144)/8);
  }
  else{
    memset(&(message[filled_space/8])+1, 0, (block_length-144-filled_space)/8);
  }
  message += (256-16*state->cv_blocks-18);
  PushString(state->hashbitlen, message, USINT);
  PushString(cnt, message+2, ULL);
  PushString(0,   message+10, ULL);
  Compress(state);

  /* output truncated hash value */
  if(state->hashbitlen%8 == 0){
	memcpy(hashval, state->state, (state->hashbitlen)/8);  	
  }
  else{
	((unsigned char*)state->state)[((state->hashbitlen)/8)] = \
	((unsigned char*)state->state)[((state->hashbitlen)/8)] & (0xff << (8-(state->hashbitlen%8)));	
	memcpy(hashval, state->state, ((state->hashbitlen)/8) + 1);  			
  }
	
  return SUCCESS;
}


HashReturn EchoHash(int hashbitlen, BitSequence *data, DataLength databitlen, BitSequence *hashval) 
{ 
  HashReturn S; 
  hashState state; 
  S = Init(&state, hashbitlen); 
  if(S != SUCCESS) return S; 
  S = Update(&state, data, databitlen); 
  if(S != SUCCESS) return S; 
  return Final(&state, hashval); 
}

int crypto_hash_echo256_generic_opt64(unsigned char *out,const unsigned char *in,unsigned long long inlen)
{
  if (EchoHash(CRYPTO_BYTES * 8, (BitSequence*)in, inlen * 8, out) == SUCCESS) return 0;
  return -1;
}


