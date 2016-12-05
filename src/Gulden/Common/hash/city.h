// Copyright (c) 2011 Google, Inc.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
// CityHash, by Geoff Pike and Jyrki Alakuijala
//
// http://code.google.com/p/cityhash/
//

#ifndef CITY_HASH_H_
#define CITY_HASH_H_

#include <stdlib.h> // for size_t.
#include <stdint.h>
#include <utility>

typedef uint8_t uint8;
typedef uint32_t uint32;
typedef uint64_t uint64;
typedef std::pair<uint64, uint64> uint128;

inline uint64 Uint128Low64(const uint128& x) { return x.first; }
inline uint64 Uint128High64(const uint128& x) { return x.second; }

uint64 CityHash64(const char* buf, size_t len);

uint64 CityHash64WithSeed(const char* buf, size_t len, uint64 seed);

uint64 CityHash64WithSeeds(const char* buf, size_t len, uint64 seed0, uint64 seed1);

uint128 CityHash128(const char* s, size_t len);

uint128 CityHash128WithSeed(const char* s, size_t len, uint128 seed);

uint32 CityHash32(const char* buf, size_t len);

inline uint64 Hash128to64(const uint128& x)
{

    const uint64 kMul = 0x9ddfea08eb382d69ULL;
    uint64 a = (Uint128Low64(x) ^ Uint128High64(x)) * kMul;
    a ^= (a >> 47);
    uint64 b = (Uint128High64(x) ^ a) * kMul;
    b ^= (b >> 47);
    b *= kMul;
    return b;
}

#endif // CITY_HASH_H_
