// Copyright (c) 2019-2022 The Centure developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the GNU Lesser General Public License v3, see the accompanying
// file COPYING

// This file defines various architecture flags for both specific architectures and 'families'.
// The rest of the codebase can just use 'ARCH_CPU_X86_FAMILY' for code that is specific to the x86 family (for instance) - e.g. Some code that contains SSE optimisations, note the file would still have to handle turning that on/off appropriately.
// Or ARCH_X86_64 for code that is specific to 64 bit 'x86' processors only etc.

#ifndef COMPAT_ARCH_H
#define COMPAT_ARCH_H

#if defined(HAVE_CONFIG_H)
#include "config/build-config.h"
#endif

#if defined(_M_X64) || defined(__x86_64__) || defined(__amd64__) || defined(_M_AMD64)
    #define ARCH_CPU_X86_FAMILY 1
    #define ARCH_X86_64 1
#elif defined(_M_IX86) || defined(__i386__) || defined(__i386)
    #define ARCH_X86 1
    #define ARCH_CPU_X86_FAMILY 1
#elif defined(__ia64__) || defined(__itanium__) || defined(_M_IA64)
    #define ARCH_ITANIUM 1
#elif defined(__aarch64__)
    #define ARCH_ARM64 1
    #define ARCH_CPU_ARM_FAMILY 1
    #define ARCH_CPU_ARM64_FAMILY 1
    #define __ARM_ARCH 8
#elif defined(__ARMEL__) || defined(__arm__)
    #define ARCH_ARM 1
    #define ARCH_CPU_ARM_FAMILY 1
#elif defined(__ppc__) || defined(__powerpc__) || defined(_M_PPC)
    #define ARCH_PPC 1
    #define ARCH_CPU_PPC_FAMILY 1
#elif defined(__ppc64__) || defined(__powerpc64__)
    #define ARCH_PPC64 1
    #define ARCH_CPU_PPC_FAMILY 1
#elif defined(__mips__)
    #define ARCH_MIPS 1
    #define ARCH_CPU_MIPS_FAMILY 1
#elif defined(__alpha__) || defined(_M_ALPHA)
    #define ARCH_CPU_ALPHA 1
#elif defined(__sparc__)
    #define ARCH_SPARC 1
#elif defined(__riscv)
    #define ARCH_RISC 1
#endif



#endif
