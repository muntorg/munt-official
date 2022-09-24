/* $Id: sph_echo.h 216 2010-06-08 09:46:57Z tp $ */
/**
 * ECHO interface. ECHO is a family of functions which differ by
 * their output size; this implementation defines ECHO for output
 * sizes 224, 256, 384 and 512 bits.
 *
 * ==========================(LICENSE BEGIN)============================
 *
 * Copyright (c) 2007-2010  Projet RNRT SAPHIR
 * 
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 * 
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * ===========================(LICENSE END)=============================
 *
 * @file     sph_echo.h
 * @author   Thomas Pornin <thomas.pornin@cryptolog.com>
 */
//
// File contains modifications by: The Centure developers
// All modifications:
// Copyright (c) 2019-2022 The Centure developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the Libre Chain License, see the accompanying
// file COPYING

#ifndef SPH_ECHO_H__
#define SPH_ECHO_H__

#include <stddef.h>
#include "sph_types.h"

/**
 * This structure is a context for ECHO computations: it contains the
 * intermediate values and some data from the last entered block. Once
 * an ECHO computation has been performed, the context can be reused for
 * another computation. This specific structure is used for ECHO-224
 * and ECHO-256.
 *
 * The contents of this structure are private. A running ECHO computation
 * can be cloned by copying the context (e.g. with a simple
 * <code>memcpy()</code>).
 */
typedef struct {
    unsigned char buf[192];    /* first field, for alignment */
    size_t ptr;
    union {
        sph_u32 Vs[4][4];
#if SPH_64
        sph_u64 Vb[4][2];
#endif
    } u;
    sph_u32 C0, C1, C2, C3;
} sph_echo256_context;

/**
 * Initialize an ECHO-256 context. This process performs no memory allocation.
 *
 * @param cc   the ECHO-256 context (pointer to a
 *             <code>sph_echo256_context</code>)
 */
void sph_echo256_init(void *cc);

/**
 * Process some data bytes. It is acceptable that <code>len</code> is zero
 * (in which case this function does nothing).
 *
 * @param cc     the ECHO-256 context
 * @param data   the input data
 * @param len    the input data length (in bytes)
 */
void sph_echo256(void *cc, const void *data, size_t len);

/**
 * Terminate the current ECHO-256 computation and output the result into
 * the provided buffer. The destination buffer must be wide enough to
 * accomodate the result (32 bytes). The context is automatically
 * reinitialized.
 *
 * @param cc    the ECHO-256 context
 * @param dst   the destination buffer
 */
void sph_echo256_close(void *cc, void *dst);

#endif

