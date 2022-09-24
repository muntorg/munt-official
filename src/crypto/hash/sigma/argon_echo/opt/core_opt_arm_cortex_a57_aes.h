// Copyright (c) 2019-2022 The Centure developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the Libre Chain License, see the accompanying file COPYING

// This file is a thin wrapper around the actual 'argon2_echo_opt', along with various other similarly named files.
// The build system compiles each file with slightly different optimisation flags so that we have optimised implementations for a wide spread of processors.

#ifndef HASH_ARGON2_ECHO_ARM_CORTEX_A57_AES_H
#define HASH_ARGON2_ECHO_ARM_CORTEX_A57_AES_H
    #define argon2_echo_ctx argon2_echo_ctx_arm_cortex_a57_aes

    #define ARGON2_CORE_OPT_IMPL
    #include "../argon_echo.h"
    #undef ARGON2_CORE_OPT_IMPL

    #undef argon2_echo_ctx
#endif
