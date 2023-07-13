// Copyright (c) 2019-2022 The Centure developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the GNU Lesser General Public License v3, see the accompanying file COPYING

// This file is a thin wrapper around the actual 'shavite3_256_aesni_opt', along with various other similarly named files.
// The build system compiles each file with slightly different optimisation flags so that we have optimised implementations for a wide spread of processors.

#ifndef HASH_SHAVITE3_256_ARM_CORTEX_A53_AES_H
#define HASH_SHAVITE3_256_ARM_CORTEX_A53_AES_H
    #define shavite3_256_opt_Init        shavite3_256_opt_arm_cortex_a53_aes_Init
    #define shavite3_256_opt_Update      shavite3_256_opt_arm_cortex_a53_aes_Update
    #define shavite3_256_opt_Final       shavite3_256_opt_arm_cortex_a53_aes_Final
    #define shavite3_256_opt_UpdateFinal shavite3_256_opt_arm_cortex_a53_aes_UpdateFinal

    #define SHAVITE3_256_OPT_IMPL
    #include "../shavite3_256_opt.h"
    #undef SHAVITE3_256_OPT_IMPL

    #undef shavite3_256_opt_Init
    #undef shavite3_256_opt_Update
    #undef shavite3_256_opt_Final
    #undef shavite3_256_opt_UpdateFinal
#endif

