// File originates from the supercop project
// Authors: Eli Biham and Orr Dunkelman
//
// File contains modifications by: The Centure developers
// All modifications:
// Copyright (c) 2019-2022 The Centure developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the Libre Chain License, see the accompanying
// file COPYING

// definitions imposed by the API
#ifndef SHA3API_H
#define SHA3API_H

#include <stdint.h>

// SHAvite-3 definition
struct shavite3_ref_hashState
{
   uint64_t bitcount;            // The number of bits compressed so far
   uint8_t chaining_value[64];   // An array containing the chaining value
   uint8_t buffer[128];          // A buffer storing bytes until they are compressed
   uint8_t partial_byte=0;         // A byte to store a fraction of a byte in case the input is not fully byte alligned
   int DigestSize;               // The requested digest size
   int BlockSize;                // The message block size
};

// Function calls imposed by the API
bool shavite3_ref_Init(shavite3_ref_hashState* state);
bool shavite3_ref_Update(shavite3_ref_hashState* state, const uint8_t* data, uint64_t dataLenBytes);
bool shavite3_ref_Final(shavite3_ref_hashState* state, uint8_t* hashval);

#endif
