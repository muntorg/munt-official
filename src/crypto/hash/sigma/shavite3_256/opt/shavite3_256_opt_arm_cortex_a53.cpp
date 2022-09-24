// Copyright (c) 2019-2022 The Centure developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the Libre Chain License, see the accompanying
// file COPYING

// This file is a thin wrapper around the actual 'shavite3_256_aesni_opt' implementation, along with various other similarly named files.
// The build system compiles each file with slightly different optimisation flags so that we have optimised implementations for a wide spread of processors.

#if defined(COMPILER_HAS_CORTEX53)
    #define shavite3_256_opt_Init        shavite3_256_opt_arm_cortex_a53_Init
    #define shavite3_256_opt_Update      shavite3_256_opt_arm_cortex_a53_Update
    #define shavite3_256_opt_Final       shavite3_256_opt_arm_cortex_a53_Final
    #define shavite3_256_opt_UpdateFinal shavite3_256_opt_arm_cortex_a53_UpdateFinal
    #define shavite3_256_opt_Compress256 shavite3_256_opt_arm_cortex_a53_Compress256

    #define SHAVITE3_256_OPT_IMPL
    #include "../shavite3_256_opt.cpp"
#endif
