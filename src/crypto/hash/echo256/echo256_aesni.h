/*
 * file        : hash_api.h
 * version     : 1.0.208
 * date        : 14.12.2010
 * 
 * ECHO vperm implementation Hash API
 *
 * Cagdas Calik
 * ccalik@metu.edu.tr
 * Institute of Applied Mathematics, Middle East Technical University, Turkey.
 *
 */
// File contains modifications by: The Gulden developers
// All modifications:
// Copyright (c) 2019 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#ifndef HASH_ECHO_256_AESNI_H
#define HASH_ECHO_256_AESNI_H

#include "compat.h"
#include <compat/arch.h>

// We only implement aes-ni/sse equivalent optimisations for x86 and arm processors currently.
#if defined(ARCH_CPU_X86_FAMILY) || defined(ARCH_CPU_ARM_FAMILY)
#include <compat/sse.h>

typedef struct
{
    __m128i        state[4][4];
    unsigned char  buffer[192];
    __m128i        k;
    __m128i        hashsize;
    __m128i        const1536;
    unsigned int   uRounds;
    unsigned int   uHashSize;
    unsigned int   uBlockLength;
    unsigned int   uBufferBytes;
    uint64_t       processed_bits;
} echo256_aesni_hashState __attribute__ ((aligned (64)));

typedef enum {SUCCESS = 0, FAIL = 1, BAD_HASHBITLEN = 2} HashReturn;

HashReturn echo256_aesni_Init(echo256_aesni_hashState *state);

HashReturn echo256_aesni_Update(echo256_aesni_hashState *state, const unsigned char* data, uint64_t databitlen);

HashReturn echo256_aesni_Final(echo256_aesni_hashState *state, unsigned char* hashval);

HashReturn echo256_aesni_UpdateFinal( echo256_aesni_hashState *state, unsigned char* hashval, const unsigned char* data, uint64_t databitlen );

#endif
#endif

