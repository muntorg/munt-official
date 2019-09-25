// Copyright (c) 2019 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the GULDEN software license, see the accompanying file COPYING

// This file is a thin wrapper around the actual 'argon2_echo_opt', along with various other similarly named files.
// The build system compiles each file with slightly different optimisation flags so that we have optimised implementations for a wide spread of processors.

#ifndef HASH_ARGON2_ECHO_SSE3_H
#define HASH_ARGON2_ECHO_SSE3_H
    #define argon2_echo_ctx argon2_echo_ctx_sse3

    #define ARGON2_CORE_OPT_IMPL
    #include "../argon_echo.h"
    #undef ARGON2_CORE_OPT_IMPL

    #undef argon2_echo_ctx
#endif
