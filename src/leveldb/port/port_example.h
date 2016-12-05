// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.
//
// This file contains the specification, but not the implementations,
// of the types/operations/etc. that should be defined by a platform
// specific port_<platform>.h file.  Use this file as a reference for
// how to port this package to a new platform.

#ifndef STORAGE_LEVELDB_PORT_PORT_EXAMPLE_H_
#define STORAGE_LEVELDB_PORT_PORT_EXAMPLE_H_

namespace leveldb {
namespace port {

    // TODO(jorlow): Many of these belong more in the environment class rather than
    //               here. We should try moving them and see if it affects perf.

    // The following boolean constant must be true on a little-endian machine
    // and false otherwise.
    static const bool kLittleEndian = true /* or some other expression */;

    // ------------------ Threading -------------------

    class Mutex {
    public:
        Mutex();
        ~Mutex();

        void Lock();

        void Unlock();

        void AssertHeld();
    };

    class CondVar {
    public:
        explicit CondVar(Mutex* mu);
        ~CondVar();

        void Wait();

        void Signal();

        void SignallAll();
    };

    typedef intptr_t OnceType;
#define LEVELDB_ONCE_INIT 0
    extern void InitOnce(port::OnceType*, void (*initializer)());

    class AtomicPointer {
    private:
        intptr_t rep_;

    public:
        AtomicPointer();

        explicit AtomicPointer(void* v)
            : rep_(v)
        {
        }

        void* Acquire_Load() const;

        void Release_Store(void* v);

        void* NoBarrier_Load() const;

        void NoBarrier_Store(void* v);
    };

    extern bool Snappy_Compress(const char* input, size_t input_length,
                                std::string* output);

    extern bool Snappy_GetUncompressedLength(const char* input, size_t length,
                                             size_t* result);

    extern bool Snappy_Uncompress(const char* input_data, size_t input_length,
                                  char* output);

    extern bool GetHeapProfile(void (*func)(void*, const char*, int), void* arg);

} // namespace port
} // namespace leveldb

#endif // STORAGE_LEVELDB_PORT_PORT_EXAMPLE_H_
