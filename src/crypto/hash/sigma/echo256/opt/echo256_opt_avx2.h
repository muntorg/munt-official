// Copyright (c) 2019-2022 The Centure developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the GNU Lesser General Public License v3, see the accompanying file COPYING

// This file is a thin wrapper around the actual 'echo256_aesni_opt', along with various other similarly named files.
// The build system compiles each file with slightly different optimisation flags so that we have optimised implementations for a wide spread of processors.

#ifndef HASH_ECHO_256_AVX2_H
#define HASH_ECHO_256_AVX2_H
    #define echo256_opt_Init        echo256_opt_avx2_Init
    #define echo256_opt_Update      echo256_opt_avx2_Update
    #define echo256_opt_Final       echo256_opt_avx2_Final
    #define echo256_opt_UpdateFinal echo256_opt_avx2_UpdateFinal

    #define ECHO256_OPT_IMPL
    #include "../echo256_opt.h"
    #undef ECHO256_OPT_IMPL

    #undef echo256_opt_Init
    #undef echo256_opt_Update
    #undef echo256_opt_Final
    #undef echo256_opt_UpdateFinal
#endif

