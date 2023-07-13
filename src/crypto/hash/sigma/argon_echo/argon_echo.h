/*
 * Argon2 reference source code package - reference C implementations
 *
 * Copyright 2015
 * Daniel Dinu, Dmitry Khovratovich, Jean-Philippe Aumasson, and Samuel Neves
 *
 * You may use this work under the terms of a Creative Commons CC0 1.0
 * License/Waiver or the Apache Public License 2.0, at your option. The terms of
 * these licenses can be found at:
 *
 * - CC0 1.0 Universal : http://creativecommons.org/publicdomain/zero/1.0
 * - Apache 2.0        : http://www.apache.org/licenses/LICENSE-2.0
 *
 * You should have received a copy of both of these licenses along with this
 * software. If not, they may be obtained at the above URLs.
 */
// File contains modifications by: The Centure developers
// All modifications:
// Copyright (c) 2019-2022 The Centure developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the GNU Lesser General Public License v3, see the accompanying
// file COPYING




#ifndef ARGON2_CORE_OPT_IMPL
#ifndef ARGON2_ECHO_H
#define ARGON2_ECHO_H

#include <stdint.h>
#include <stddef.h>
#include <limits.h>
#include <stdlib.h>
#include <array>
#include <compat/arch.h>

/*
 * Argon2 input parameter restrictions
 */
/* Minimum and maximum number of lanes (degree of parallelism) */
#define ARGON2_MIN_LANES UINT32_C(1)
#define ARGON2_MAX_LANES UINT32_C(0xFFFFFF)

/* Minimum and maximum number of threads */
#define ARGON2_MIN_THREADS UINT32_C(1)
#define ARGON2_MAX_THREADS UINT32_C(0xFFFFFF)

/* Number of synchronization points between lanes per pass */
#define ARGON2_SYNC_POINTS UINT32_C(4)

/* Minimum and maximum number of memory blocks (each of BLOCK_SIZE bytes) */
#define ARGON2_MIN_MEMORY (2 * ARGON2_SYNC_POINTS) /* 2 blocks per slice */

#define ARGON2_MIN(a, b) ((a) < (b) ? (a) : (b))
/* Max memory size is addressing-space/2, topping at 2^32 blocks (4 TB) */
#define ARGON2_MAX_MEMORY_BITS ARGON2_MIN(UINT32_C(32), (sizeof(void *) * CHAR_BIT - 10 - 1))
#define ARGON2_MAX_MEMORY ARGON2_MIN(UINT32_C(0xFFFFFFFF), UINT64_C(1) << ARGON2_MAX_MEMORY_BITS)

/* Minimum and maximum number of passes */
#define ARGON2_MIN_TIME UINT32_C(1)
#define ARGON2_MAX_TIME UINT32_C(0xFFFFFFFF)

/* Minimum and maximum password length in bytes */
#define ARGON2_MIN_PWD_LENGTH UINT32_C(1)
#define ARGON2_MAX_PWD_LENGTH UINT32_C(0xFFFFFFFF)

/* Flags to determine which fields are securely wiped (default = no wipe). */
#define ARGON2_DEFAULT_FLAGS UINT32_C(0)
#define ARGON2_FLAG_CLEAR_PASSWORD (UINT32_C(1) << 0)
#define ARGON2_FLAG_CLEAR_SECRET (UINT32_C(1) << 1)

/* Error codes */
enum argon2_error_codes
{
    ARGON2_OK = 0,
    ARGON2_OUTPUT_PTR_NULL = -1,
    ARGON2_OUTPUT_TOO_SHORT = -2,
    ARGON2_OUTPUT_TOO_LONG = -3,
    ARGON2_PWD_TOO_SHORT = -4,
    ARGON2_PWD_TOO_LONG = -5,
    ARGON2_TIME_TOO_SMALL = -12,
    ARGON2_TIME_TOO_LARGE = -13,
    ARGON2_MEMORY_TOO_LITTLE = -14,
    ARGON2_MEMORY_TOO_MUCH = -15,
    ARGON2_LANES_TOO_FEW = -16,
    ARGON2_LANES_TOO_MANY = -17,
    ARGON2_PWD_PTR_MISMATCH = -18,    /* NULL ptr with non-zero length */
    ARGON2_MEMORY_ALLOCATION_ERROR = -22,
    ARGON2_INCORRECT_PARAMETER = -25,
    ARGON2_THREADS_TOO_FEW = -28,
    ARGON2_THREADS_TOO_MANY = -29,
    ARGON2_MISSING_ARGS = -30,
    ARGON2_THREAD_FAIL = -33
};

/* Argon2 external data structures */
/*
 * Context: structure to hold Argon2 inputs:
 *  output array and its length,
 *  password and its length,
 *  number of passes, amount of used memory (in KBytes, can be rounded up a bit)
 *  number of parallel threads that will be run.
 */
struct argon2_echo_context
{
    uint8_t* pwd;    /* password array */
    uint32_t pwdlen; /* password length */

    std::array<uint64_t, 4> outHash;

    uint32_t t_cost;  /* number of passes */
    uint32_t m_cost;  /* amount of memory requested (KB) */
    uint32_t lanes;   /* number of lanes */
    uint32_t threads; /* maximum number of threads */

    uint32_t version; /* version number */

    uint8_t* allocated_memory; /* pointer to pre-allocated memory */
};

extern int (*selected_argon2_echo_hash)(argon2_echo_context* context, bool doHash);

/**
 * Get the associated error message for given error code
 * @return  The error message associated with the given error code
 */
const char* argon2_echo_error_message(int error_code);

#include "opt/core_opt_hybrid.h"
//fixme: (SIGMA) - Implement sse2 (we have argon version but not echo etc.)
//#include "opt/core_opt_sse2.h"
//#include "opt/core_opt_sse2_aes.h"
#ifdef ARCH_CPU_X86_FAMILY
#include "opt/core_opt_sse3.h"
#include "opt/core_opt_sse3_aes.h"
#include "opt/core_opt_sse4.h"
#include "opt/core_opt_sse4_aes.h"
#include "opt/core_opt_avx.h"
#include "opt/core_opt_avx_aes.h"
#include "opt/core_opt_avx2.h"
#include "opt/core_opt_avx2_aes.h"
#include "opt/core_opt_avx512f.h"
#include "opt/core_opt_avx512f_aes.h"
#endif

#ifdef ARCH_CPU_ARM_FAMILY
#include "opt/core_opt_arm_cortex_a53.h"
#include "opt/core_opt_arm_cortex_a53_aes.h"
#include "opt/core_opt_arm_cortex_a57.h"
#include "opt/core_opt_arm_cortex_a57_aes.h"
#include "opt/core_opt_arm_cortex_a72.h"
#include "opt/core_opt_arm_cortex_a72_aes.h"
#include "opt/core_opt_arm_thunderx_aes.h"
#endif

int argon2_echo_ctx_ref(argon2_echo_context* context, bool doHash);
#endif
#else
/*
 * Function that performs memory-hard hashing with certain degree of parallelism
 * @param  context  Pointer to the Argon2 internal structure
 * @return Error code if smth is wrong, ARGON2_OK otherwise
 */
int argon2_echo_ctx(argon2_echo_context* context, bool doHash);

#endif
