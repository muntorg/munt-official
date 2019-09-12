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

#include "compat.h"
#include <compat/arch.h>
#include <compat/sse.h>

#ifndef ECHO256_OPT_IMPL
#error This file should not be included directly, include one or more optimised variants e.g. echo256_aesni_opt_sse2.h instead.
#endif

extern int echo256_opt_selected;
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
} echo256_opt_hashState __attribute__ ((aligned (64)));

typedef enum {SUCCESS = 0, FAIL = 1, BAD_HASHBITLEN = 2} HashReturn;

HashReturn echo256_opt_Init(echo256_opt_hashState *state);

HashReturn echo256_opt_Update(echo256_opt_hashState *state, const unsigned char* data, uint64_t databitlen);

HashReturn echo256_opt_Final(echo256_opt_hashState *state, unsigned char* hashval);

HashReturn echo256_opt_UpdateFinal( echo256_opt_hashState *state, unsigned char* hashval, const unsigned char* data, uint64_t databitlen );
