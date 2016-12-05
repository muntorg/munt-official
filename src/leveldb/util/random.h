// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef STORAGE_LEVELDB_UTIL_RANDOM_H_
#define STORAGE_LEVELDB_UTIL_RANDOM_H_

#include <stdint.h>

namespace leveldb {

// A very simple random number generator.  Not especially good at
// generating truly random bits, but good enough for our needs in this
// package.
class Random {
private:
    uint32_t seed_;

public:
    explicit Random(uint32_t s)
        : seed_(s & 0x7fffffffu)
    {
        // Avoid bad seeds.
        if (seed_ == 0 || seed_ == 2147483647L) {
            seed_ = 1;
        }
    }
    uint32_t Next()
    {
        static const uint32_t M = 2147483647L; // 2^31-1
        static const uint64_t A = 16807; // bits 14, 8, 7, 5, 2, 1, 0

        uint64_t product = seed_ * A;

        seed_ = static_cast<uint32_t>((product >> 31) + (product & M));

        if (seed_ > M) {
            seed_ -= M;
        }
        return seed_;
    }

    uint32_t Uniform(int n) { return Next() % n; }

    bool OneIn(int n) { return (Next() % n) == 0; }

    uint32_t Skewed(int max_log)
    {
        return Uniform(1 << Uniform(max_log + 1));
    }
};

} // namespace leveldb

#endif // STORAGE_LEVELDB_UTIL_RANDOM_H_
