// File originates from the supercop project
// Authors: Eli Biham and Orr Dunkelman
//
// File contains modifications by: The Gulden developers
// All modifications:
// Copyright (c) 2019 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#ifndef SHAVITE_3_256_AESNI_H
#define SHAVITE_3_256_AESNI_H

#include <compat/arch.h>
// Only x86 family CPUs have AES-NI
#ifdef ARCH_CPU_X86_FAMILY

struct shavite3_256_aesni_hashState
{
   uint64_t bitcount;                // The number of bits compressed so far
   uint8_t chaining_value[64]; // An array containing the chaining value
   uint8_t buffer[128];        // A buffer storing bytes until they are compressed
   uint8_t partial_byte=0;       // A byte to store a fraction of a byte in case the input is not fully byte alligned
   int DigestSize;                   // The requested digest size
   int BlockSize;                    // The message block size
    __attribute__ ((aligned (16))) unsigned int SHAVITE_MESS[16];
    __attribute__ ((aligned (16))) unsigned char SHAVITE_PTXT[8*4];
    __attribute__ ((aligned (16))) unsigned int SHAVITE_CNTS[4] = {0,0,0,0}; 
};

// Initialization of the internal state of the hash function
bool shavite3_256_aesni_Init(shavite3_256_aesni_hashState* state);

// Compressing the input data, and updating the internal state
bool shavite3_256_aesni_Update(shavite3_256_aesni_hashState* state, const unsigned char* data, uint64_t dataLenBytes);

// Performing the padding scheme, and dealing with any remaining bits
bool shavite3_256_aesni_Final(shavite3_256_aesni_hashState* state, unsigned char* hashval);

#endif
#endif
