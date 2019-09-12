// Copyright (c) 2019 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

// This file is a thin wrapper around the actual 'echo256_aesni_opt' implementation, along with various other similarly named files.
// The build system compiles each file with slightly different optimisation flags so that we have optimised implementations for a wide spread of processors.

#include <compat/arch.h>

#if defined(ARCH_CPU_X86_FAMILY)
    #define echo256_opt_hashState   echo256_opt_avx2_aes_hashState
    #define echo256_opt_Init        echo256_opt_avx2_aes_Init
    #define echo256_opt_Update      echo256_opt_avx2_aes_Update
    #define echo256_opt_Final       echo256_opt_avx2_aes_Final
    #define echo256_opt_UpdateFinal echo256_opt_avx2_aes_UpdateFinal

    #define USE_HARDWARE_AES
    #define ECHO256_OPT_IMPL
    #include "../echo256_opt.cpp"
#endif
