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
// Distributed under the Libre Chain License, see the accompanying
// file COPYING

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "argon_echo.h"
#include "core.h"

#if defined(ARGON2_CORE_OPT_IMPL) || defined(ARGON2_CORE_REF_IMPL)
int argon2_echo_ctx(argon2_echo_context *context, bool doHash)
{
    /* 1. Validate all inputs */
    int result = validate_inputs(context);
    uint32_t memory_blocks, segment_length;
    argon2_echo_instance_t instance;

    if (ARGON2_OK != result)
    {
        return result;
    }

    /* 2. Align memory size */
    /* Minimum memory_blocks = 8L blocks, where L is the number of lanes */
    memory_blocks = context->m_cost;

    if (memory_blocks < 2 * ARGON2_SYNC_POINTS * context->lanes)
    {
        memory_blocks = 2 * ARGON2_SYNC_POINTS * context->lanes;
    }

    segment_length = memory_blocks / (context->lanes * ARGON2_SYNC_POINTS);
    /* Ensure that all segments have equal length */
    memory_blocks = segment_length * (context->lanes * ARGON2_SYNC_POINTS);

    instance.memory = NULL;
    instance.passes = context->t_cost;
    instance.memory_blocks = memory_blocks;
    instance.segment_length = segment_length;
    instance.lane_length = segment_length * ARGON2_SYNC_POINTS;
    instance.lanes = context->lanes;
    instance.threads = context->threads;

    if (instance.threads > instance.lanes)
    {
        instance.threads = instance.lanes;
    }

    /* 3. Initialization: Hashing inputs, allocating memory, filling first
     * blocks
     */
    result = initialize(&instance, context);

    if (ARGON2_OK != result)
    {
        return result;
    }

    /* 4. Filling memory */
    result = fill_memory_blocks(&instance);

    if (ARGON2_OK != result)
    {
        return result;
    }

    /* 5. Finalization */
    //NB! We only bother to finalize if we are interested in the actual hash (doHash=true)
    //If we are only using argon to fill memory (doHash=false) then we don't actually care about the resulting hash.
    if (doHash)
    {
        finalize(context, &instance);
    }
    
    return ARGON2_OK;
}

#else

const char *argon2_echo_error_message(int error_code)
{
    switch (error_code)
    {
        case ARGON2_OK:
            return "OK";
        case ARGON2_OUTPUT_PTR_NULL:
            return "Output pointer is NULL";
        case ARGON2_OUTPUT_TOO_SHORT:
            return "Output is too short";
        case ARGON2_OUTPUT_TOO_LONG:
            return "Output is too long";
        case ARGON2_PWD_TOO_SHORT:
            return "Password is too short";
        case ARGON2_PWD_TOO_LONG:
            return "Password is too long";
        case ARGON2_TIME_TOO_SMALL:
            return "Time cost is too small";
        case ARGON2_TIME_TOO_LARGE:
            return "Time cost is too large";
        case ARGON2_MEMORY_TOO_LITTLE:
            return "Memory cost is too small";
        case ARGON2_MEMORY_TOO_MUCH:
            return "Memory cost is too large";
        case ARGON2_LANES_TOO_FEW:
            return "Too few lanes";
        case ARGON2_LANES_TOO_MANY:
            return "Too many lanes";
        case ARGON2_PWD_PTR_MISMATCH:
            return "Password pointer is NULL, but password length is not 0";
        case ARGON2_MEMORY_ALLOCATION_ERROR:
            return "Memory allocation error";
        case ARGON2_INCORRECT_PARAMETER:
            return "Context is NULL";
        case ARGON2_THREADS_TOO_FEW:
            return "Not enough threads";
        case ARGON2_THREADS_TOO_MANY:
            return "Too many threads";
        case ARGON2_MISSING_ARGS:
            return "Missing arguments";
        case ARGON2_THREAD_FAIL:
            return "Threading failure";
        default:
            return "Unknown error code";
    }
}

#endif

