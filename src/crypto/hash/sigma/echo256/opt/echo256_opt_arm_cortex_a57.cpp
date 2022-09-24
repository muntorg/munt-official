// Copyright (c) 2019-2022 The Centure developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the Libre Chain License, see the accompanying
// file COPYING

// This file is a thin wrapper around the actual 'echo256_aesni_opt' implementation, along with various other similarly named files.
// The build system compiles each file with slightly different optimisation flags so that we have optimised implementations for a wide spread of processors.

#if defined(COMPILER_HAS_CORTEX57)
    #define echo256_opt_Init        echo256_opt_arm_cortex_a57_Init
    #define echo256_opt_Update      echo256_opt_arm_cortex_a57_Update
    #define echo256_opt_Final       echo256_opt_arm_cortex_a57_Final
    #define echo256_opt_UpdateFinal echo256_opt_arm_cortex_a57_UpdateFinal
    #define Compress                echo256_opt_arm_cortex_a57_compress

    #define ECHO256_OPT_IMPL
    #include "../echo256_opt.cpp"
#endif
