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

#ifndef SHAVITE_3_256_AESNI_H
#define SHAVITE_3_256_AESNI_H

typedef struct {
   uint64_t bitcount;                // The number of bits compressed so far
   unsigned char chaining_value[64]; // An array containing the chaining value
   unsigned char buffer[128];        // A buffer storing bytes until they are compressed
   unsigned char partial_byte;       // A byte to store a fraction of a byte in case the input is not fully byte alligned
   unsigned char salt[64];           // The salt used in the hash function
   int DigestSize;                   // The requested digest size
   int BlockSize;                    // The message block size
} shavite3_256_aesni_hashState;

// Initialization of the internal state of the hash function
bool shavite3_256_aesni_Init(shavite3_256_aesni_hashState *state);

// Compressing the input data, and updating the internal state
bool shavite3_256_aesni_Update(shavite3_256_aesni_hashState *state, const unsigned char *data, uint64_t databitlen);

// Performing the padding scheme, and dealing with any remaining bits
bool shavite3_256_aesni_Final(shavite3_256_aesni_hashState *state, unsigned char *hashval);

#endif
