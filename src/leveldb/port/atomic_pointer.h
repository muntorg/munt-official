// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

// AtomicPointer provides storage for a lock-free pointer.
// Platform-dependent implementation of AtomicPointer:
// - If the platform provides a cheap barrier, we use it with raw pointers
// - If <atomic> is present (on newer versions of gcc, it is), we use
//   a <atomic>-based AtomicPointer.  However we prefer the memory
//   barrier based version, because at least on a gcc 4.4 32-bit build
//   on linux, we have encountered a buggy <atomic> implementation.
//   Also, some <atomic> implementations are much slower than a memory-barrier
//   based implementation (~16ns for <atomic> based acquire-load vs. ~1ns for
//   a barrier based acquire-load).
// This code is based on atomicops-internals-* in Google's perftools:
// http://code.google.com/p/google-perftools/source/browse/#svn%2Ftrunk%2Fsrc%2Fbase

#ifndef PORT_ATOMIC_POINTER_H_
#define PORT_ATOMIC_POINTER_H_

#include <stdint.h>
#ifdef LEVELDB_ATOMIC_PRESENT
#include <atomic>
#endif
#ifdef OS_WIN
#include <windows.h>
#endif
#ifdef OS_MACOSX
#include <libkern/OSAtomic.h>
#endif

#if defined(_M_X64) || defined(__x86_64__)
#define ARCH_CPU_X86_FAMILY 1
#elif defined(_M_IX86) || defined(__i386__) || defined(__i386)
#define ARCH_CPU_X86_FAMILY 1
#elif defined(__ARMEL__)
#define ARCH_CPU_ARM_FAMILY 1
#elif defined(__ppc__) || defined(__powerpc__) || defined(__powerpc64__)
#define ARCH_CPU_PPC_FAMILY 1
#endif

namespace leveldb {
namespace port {

#if defined(OS_WIN) && defined(COMPILER_MSVC) && defined(ARCH_CPU_X86_FAMILY)

#define LEVELDB_HAVE_MEMORY_BARRIER

#elif defined(OS_MACOSX)
    inline void MemoryBarrier()
    {
        OSMemoryBarrier();
    }
#define LEVELDB_HAVE_MEMORY_BARRIER

#elif defined(ARCH_CPU_X86_FAMILY) && defined(__GNUC__)
    inline void MemoryBarrier()
    {

        __asm__ __volatile__(""
                             :
                             :
                             : "memory");
    }
#define LEVELDB_HAVE_MEMORY_BARRIER

#elif defined(ARCH_CPU_X86_FAMILY) && defined(__SUNPRO_CC)
    inline void MemoryBarrier()
    {

        asm volatile(""
                     :
                     :
                     : "memory");
    }
#define LEVELDB_HAVE_MEMORY_BARRIER

#elif defined(ARCH_CPU_ARM_FAMILY) && defined(__linux__)
    typedef void (*LinuxKernelMemoryBarrierFunc)(void);

    inline void MemoryBarrier()
    {
        (*(LinuxKernelMemoryBarrierFunc)0xffff0fa0)();
    }
#define LEVELDB_HAVE_MEMORY_BARRIER

#elif defined(ARCH_CPU_PPC_FAMILY) && defined(__GNUC__)
    inline void MemoryBarrier()
    {

        asm volatile("sync"
                     :
                     :
                     : "memory");
    }
#define LEVELDB_HAVE_MEMORY_BARRIER

#endif

#if defined(LEVELDB_HAVE_MEMORY_BARRIER)
    class AtomicPointer {
    private:
        void* rep_;

    public:
        AtomicPointer() {}
        explicit AtomicPointer(void* p)
            : rep_(p)
        {
        }
        inline void* NoBarrier_Load() const { return rep_; }
        inline void NoBarrier_Store(void* v) { rep_ = v; }
        inline void* Acquire_Load() const
        {
            void* result = rep_;
            MemoryBarrier();
            return result;
        }
        inline void Release_Store(void* v)
        {
            MemoryBarrier();
            rep_ = v;
        }
    };

#elif defined(LEVELDB_ATOMIC_PRESENT)
    class AtomicPointer {
    private:
        std::atomic<void*> rep_;

    public:
        AtomicPointer() {}
        explicit AtomicPointer(void* v)
            : rep_(v)
        {
        }
        inline void* Acquire_Load() const
        {
            return rep_.load(std::memory_order_acquire);
        }
        inline void Release_Store(void* v)
        {
            rep_.store(v, std::memory_order_release);
        }
        inline void* NoBarrier_Load() const
        {
            return rep_.load(std::memory_order_relaxed);
        }
        inline void NoBarrier_Store(void* v)
        {
            rep_.store(v, std::memory_order_relaxed);
        }
    };

#elif defined(__sparcv9) && defined(__GNUC__)
    class AtomicPointer {
    private:
        void* rep_;

    public:
        AtomicPointer() {}
        explicit AtomicPointer(void* v)
            : rep_(v)
        {
        }
        inline void* Acquire_Load() const
        {
            void* val;
            __asm__ __volatile__(
                "ldx [%[rep_]], %[val] \n\t"
                "membar #LoadLoad|#LoadStore \n\t"
                : [val] "=r"(val)
                : [rep_] "r"(&rep_)
                : "memory");
            return val;
        }
        inline void Release_Store(void* v)
        {
            __asm__ __volatile__(
                "membar #LoadStore|#StoreStore \n\t"
                "stx %[v], [%[rep_]] \n\t"
                :
                : [rep_] "r"(&rep_), [v] "r"(v)
                : "memory");
        }
        inline void* NoBarrier_Load() const { return rep_; }
        inline void NoBarrier_Store(void* v) { rep_ = v; }
    };

#elif defined(__ia64) && defined(__GNUC__)
    class AtomicPointer {
    private:
        void* rep_;

    public:
        AtomicPointer() {}
        explicit AtomicPointer(void* v)
            : rep_(v)
        {
        }
        inline void* Acquire_Load() const
        {
            void* val;
            __asm__ __volatile__(
                "ld8.acq %[val] = [%[rep_]] \n\t"
                : [val] "=r"(val)
                : [rep_] "r"(&rep_)
                : "memory");
            return val;
        }
        inline void Release_Store(void* v)
        {
            __asm__ __volatile__(
                "st8.rel [%[rep_]] = %[v]  \n\t"
                :
                : [rep_] "r"(&rep_), [v] "r"(v)
                : "memory");
        }
        inline void* NoBarrier_Load() const { return rep_; }
        inline void NoBarrier_Store(void* v) { rep_ = v; }
    };

#else
#error Please implement AtomicPointer for this platform.

#endif

#undef LEVELDB_HAVE_MEMORY_BARRIER
#undef ARCH_CPU_X86_FAMILY
#undef ARCH_CPU_ARM_FAMILY
#undef ARCH_CPU_PPC_FAMILY

} // namespace port
} // namespace leveldb

#endif // PORT_ATOMIC_POINTER_H_
