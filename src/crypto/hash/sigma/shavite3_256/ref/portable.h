// File originates from the supercop project
//
// File contains modifications by: The Centure developers
// All modifications:
// Copyright (c) 2019-2022 The Centure developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the GNU Lesser General Public License v3, see the accompanying
// file COPYING

/*
 * IT IS ADVISABLE TO INCLUDE THIS FILE AS THE FIRST INCLUDE in the
 * source, as it makes some optimizations over some other include
 * files, which are not possible if the other include files are
 * included first (this is now applicable to string.h)
 */

#ifndef SHAVITE_PORTABLE_C__
#define SHAVITE_PORTABLE_C__

/* __USE_STRING_INLINES tells GCC to optimize memcpy when the number */
/* of bytes is constant (see /usr/include/string.h) */
#if defined(__GNUC__) && !defined(__USE_STRING_INLINES)
#define __USE_STRING_INLINES
#endif
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <stdio.h>


#include <limits.h>
#include <stdint.h>
#include <compat/arch.h>

#if UINT_MAX >= 4294967295UL
#define ONE32   0xFFFFFFFFU
#else
#define ONE32   0xFFFFFFFFUL
#endif
#define ONE8    0xFFU
#define ONE16   0xFFFFU
#define T8(x)   ((x) & ONE8)
#define T16(x)  ((x) & ONE16)
#define T32(x)  ((x) & ONE32)

#define ONE64   LL(0xFFFFFFFFFFFFFFFF)
#define T64(x)  ((x) & ONE64)

// U16TO8_LITTLE(c, v) stores the 16-bit-value v in little-endian convention into the unsigned char array pointed to by c.
#ifndef WORDS_BIGENDIAN
#define U16TO8_LITTLE(c, v) memcpy(c, &v, 2)
#else
// Make sure that the local variable names do not collide with variables of the calling code (i.e., those used in c, v)
#define U16TO8_LITTLE(c, v)    do { \
    uint16_t tmp_portable_h_x = (v); \
    uint8_t* tmp_portable_h_d = (c); \
    tmp_portable_h_d[0] = T8(tmp_portable_h_x); \
    tmp_portable_h_d[1] = T8(tmp_portable_h_x >> 8); \
} while (0)
#endif

// U8TO32_LITTLE(c) returns the 32-bit value stored in little-endian convention in the unsigned char array pointed to by c.
#ifndef WORDS_BIGENDIAN
#define U8TO32_LITTLE(c)  (((uint32_t*)(c))[0])
#else
#define U8TO32_LITTLE(c)  (((uint32_t)T8(*((uint8_t*)(c)))) | ((uint32_t)T8(*(((uint8_t*)(c)) + 1)) << 8) | ((uint32_t)T8(*(((uint8_t*)(c)) + 2)) << 16) | ((uint32_t)T8(*(((uint8_t*)(c)) + 3)) << 24))
#endif

// U32TO8_LITTLE(c, v) stores the 32-bit-value v in little-endian convention into the unsigned char array pointed to by c.
#ifndef WORDS_BIGENDIAN
#define U32TO8_LITTLE(c, v) memcpy(c, &v, 4)
#else
// Make sure that the local variable names do not collide with variables of the calling code (i.e., those used in c, v)
#define U32TO8_LITTLE(c, v)    do { \
    uint32_t tmp_portable_h_x = (v); \
    uint8_t* tmp_portable_h_d = (c); \
    tmp_portable_h_d[0] = T8(tmp_portable_h_x); \
    tmp_portable_h_d[1] = T8(tmp_portable_h_x >> 8); \
    tmp_portable_h_d[2] = T8(tmp_portable_h_x >> 16); \
    tmp_portable_h_d[3] = T8(tmp_portable_h_x >> 24); \
} while (0)
#endif

// U64TO8_LITTLE(c, v) stores the 64-bit-value v in little-endian convention into the unsigned char array pointed to by c.
#ifndef WORDS_BIGENDIAN
#define U64TO8_LITTLE(c, v) memcpy(c, &v, 8)
#else
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
#endif

#endif
