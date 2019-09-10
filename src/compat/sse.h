// Copyright (c) 2019 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

// This file defines a basic compatibility layer to compile sse instructions to neon on arm achitectures.
// The result is not likely to be optimal or as good as a complete neon re-write but still better than a naive non-neon implementation.

#ifndef COMPAT_SSE_H
#define COMPAT_SSE_H

#ifndef __clang__
    #define PRAGMA_HELPER0(x) #x
    #define PRAGMA_HELPER1(x) PRAGMA_HELPER0(GCC target(x))
    #ifndef DEBUG
        #define PUSH_COMPILER_OPTIMISATIONS(OPT) \
        _Pragma("GCC push_options")              \
        _Pragma(PRAGMA_HELPER1(OPT))             \
        _Pragma("GCC optimize (\"O3\")")
    #else
        #define PUSH_COMPILER_OPTIMISATIONS(OPT) \
        _Pragma("GCC push_options")              \
        _Pragma(PRAGMA_HELPER1(OPT))
    #endif
    #define POP_COMPILER_OPTIMISATIONS() \
    _Pragma("GCC pop_options")
#else
    #define PRAGMA_HELPER0(x) #x
    #define PRAGMA_HELPER1(x) PRAGMA_HELPER0(clang attribute push (__attribute__((target(x))), apply_to=any(function)))
    #define PRAGMA_HELPER2(y) PRAGMA_HELPER1(y)
    #define PUSH_COMPILER_OPTIMISATIONS(OPT) \
    _Pragma(PRAGMA_HELPER1(OPT))
    #define POP_COMPILER_OPTIMISATIONS() \
    _Pragma("clang attribute pop")
#endif

#if defined(ARCH_CPU_X86_FAMILY)
    #include <emmintrin.h>
    #include <immintrin.h>
#elif defined(ARCH_CPU_ARM_FAMILY)
    // Compat layer for neon intrinsics
    #include <stdint.h>
    #if __ARM_ARCH >= 8
    #define __ARM_FEATURE_CRYPTO
    #endif
    #include <arm_neon.h>
    typedef int32x4_t __m128i;
    
    
    inline __m128i _mm_mul_epu32(__m128i a, __m128i b)
    {
        return vreinterpretq_s32_u32(vmulq_u32(vreinterpretq_u32_s32(a), vreinterpretq_u32_s32(b)));
    }
    inline __m128i _mm_sub_epi64(__m128i a, __m128i b)
    {
        return vreinterpretq_s32_s64(vsubq_s64(vreinterpretq_s64_s32(a), vreinterpretq_s64_s32(b)));
    }
    inline __m128i _mm_add_epi64(__m128i a, __m128i b)
    {
        return vreinterpretq_s32_s64(vaddq_s64(vreinterpretq_s64_s32(a), vreinterpretq_s64_s32(b)));
    }

    #define _mm_srli_epi64(a, imm)                                                      \
    ({                                                                                  \
        __m128i ret;                                                                    \
        if ((imm) <= 0) {                                                               \
            ret = a;                                                                    \
        }                                                                               \
        else if ((imm)> 31) {                                                           \
            ret = _mm_setzero_si128();                                                  \
        }                                                                               \
        else {                                                                          \
            ret = vreinterpretq_s32_u64(vshrq_n_u64(vreinterpretq_u64_s32(a), (imm)));  \
        }                                                                               \
        ret;                                                                            \
    })
    #define _mm_slli_epi64(a, imm)                                                      \
    ({                                                                                  \
        __m128i ret;                                                                    \
        if ((imm) <= 0) {                                                               \
            ret = a;                                                                    \
        }                                                                               \
        else if ((imm) > 31) {                                                          \
            ret = _mm_setzero_si128();                                                  \
        }                                                                               \
        else {                                                                          \
            ret = vreinterpretq_s32_s64(vshlq_n_s64(vreinterpretq_s64_s32(a), (imm)));  \
        }                                                                               \
        ret;                                                                            \
    })
    inline __m128i _mm_unpacklo_epi64(__m128i a, __m128i b)
    {
        int64x1_t a1 = vget_low_s64(vreinterpretq_s64_s32(a));
        int64x1_t b1 = vget_low_s64(vreinterpretq_s64_s32(b));
        return (int32x4_t)vcombine_s64(a1, b1);
    }
    inline __m128i _mm_unpackhi_epi64(__m128i a, __m128i b)
    {
        int64x1_t a1 = vget_high_s64(vreinterpretq_s64_s32(a));
        int64x1_t b1 = vget_high_s64(vreinterpretq_s64_s32(b));
        return (int32x4_t)vcombine_s64(a1, b1);
    }
    
    
    // Older gcc does not define vld1q_u8_x4 type
    #if defined(__GNUC__) && !defined(__clang__)
    #if __GNUC__ < 9 || (__GNUC__ == 9 && (__GNUC_MINOR__ <= 2))
    inline uint8x16x4_t vld1q_u8_x4(const uint8_t *p)
    {
            uint8x16x4_t ret;
            ret.val[0] = vld1q_u8(p +  0);
            ret.val[1] = vld1q_u8(p + 16);
            ret.val[2] = vld1q_u8(p + 32);
            ret.val[3] = vld1q_u8(p + 48);
            return ret;
    }
    #endif
    #endif
    
    // AESENC
    #ifndef __ARM_FEATURE_CRYPTO
        // In the absence of crypto extensions, implement aesenc using regular neon intrinsics instead.
        // See: http://www.workofard.com/2017/01/accelerated-aes-for-the-arm64-linux-kernel/ http://www.workofard.com/2017/07/ghash-for-low-end-cores/ and https://github.com/ColinIanKing/linux-next-mirror/blob/b5f466091e130caaf0735976648f72bd5e09aa84/crypto/aegis128-neon-inner.c#L52 for more information
        // Reproduced with permission of the author.
        inline __m128i _mm_aesenc_si128(__m128i EncBlock, __m128i RoundKey)
        {
            static const uint8_t crypto_aes_sbox[256] = { 0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76, 0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0, 0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15, 0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75, 0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84, 0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf, 0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8, 0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5, 0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2, 0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73, 0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb, 0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c, 0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79, 0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08, 0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a, 0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e, 0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf, 0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16 };
            static const uint8_t shift_rows[] = { 0x0, 0x5, 0xa, 0xf, 0x4, 0x9, 0xe, 0x3, 0x8, 0xd, 0x2, 0x7, 0xc, 0x1, 0x6, 0xb};
            static const uint8_t ror32by8[] = {0x1, 0x2, 0x3, 0x0, 0x5, 0x6, 0x7, 0x4, 0x9, 0xa, 0xb, 0x8, 0xd, 0xe, 0xf, 0xc};

            uint8x16_t v;
            uint8x16_t w = vreinterpretq_u8_s32(EncBlock);

            // shift rows
            w = vqtbl1q_u8(w, vld1q_u8(shift_rows));

            // sub bytes
            v = vqtbl4q_u8(vld1q_u8_x4(crypto_aes_sbox), w);
            v = vqtbx4q_u8(v, vld1q_u8_x4(crypto_aes_sbox + 0x40), w - 0x40);
            v = vqtbx4q_u8(v, vld1q_u8_x4(crypto_aes_sbox + 0x80), w - 0x80);
            v = vqtbx4q_u8(v, vld1q_u8_x4(crypto_aes_sbox + 0xc0), w - 0xc0);

            // mix columns
            w = (v << 1) ^ (uint8x16_t)(((int8x16_t)v >> 7) & 0x1b);
            w ^= (uint8x16_t)vrev32q_u16((uint16x8_t)v);
            w ^= vqtbl1q_u8(v ^ w, vld1q_u8(ror32by8));

            //  add round key
            return vreinterpretq_s32_u8(w) ^ RoundKey;
        }
    #else
        // Implements equivalent of 'aesenc' by combining AESE (with an empty key) and AESMC and then manually applying the real key as an xor operation
        // This unfortunately means an additional xor op; the compiler should be able to optimise this away for repeated calls however
        // See  https://blog.michaelbrase.com/2018/05/08/emulating-x86-aes-intrinsics-on-armv8-a for more details.
        inline __m128i _mm_aesenc_si128(__m128i a, __m128i b)
        {
            return vreinterpretq_s32_u8(vaesmcq_u8(vaeseq_u8(vreinterpretq_u8_s32(a), uint8x16_t{})) ^ vreinterpretq_u8_s32(b));
        }
    #endif
      
    // Most of the conversions after this are heavily inspired by or taken from sse2neon project (https://github.com/jratcliff63367/sse2neon)
    #define _mm_xor_si128 veorq_s32
    #define _mm_and_si128 vandq_s32
    #define _mm_add_epi32 vaddq_s32
    #define _mm_loadu_si128(p) vld1q_s32((int32_t*)(p))
    #define _mm_setzero_si128() vdupq_n_s32(0)
    #define _mm_storeu_si128(p, a) vst1q_s32((int32_t*)(p), (a))
    
    inline __m128i _mm_add_epi8(__m128i a, __m128i b)
    {
        return vreinterpretq_s32_s8(vaddq_s8(vreinterpretq_s8_s32(a), vreinterpretq_s8_s32(b)));
    }
    
    inline __m128i _mm_shuffle_epi8(__m128i ia, __m128i ib)
    {
        uint8_t *a = (uint8_t *) &ia; // input a
        uint8_t *b = (uint8_t *) &ib; // input b
        int32_t r[4];

        r[0] = ((b[3] & 0x80) ? 0 : a[b[3] % 16])<<24;
        r[0] |= ((b[2] & 0x80) ? 0 : a[b[2] % 16])<<16;
        r[0] |= ((b[1] & 0x80) ? 0 : a[b[1] % 16])<<8;
        r[0] |= ((b[0] & 0x80) ? 0 : a[b[0] % 16]);

        r[1] = ((b[7] & 0x80) ? 0 : a[b[7] % 16])<<24;
        r[1] |= ((b[6] & 0x80) ? 0 : a[b[6] % 16])<<16;
        r[1] |= ((b[5] & 0x80) ? 0 : a[b[5] % 16])<<8;
        r[1] |= ((b[4] & 0x80) ? 0 : a[b[4] % 16]);

        r[2] = ((b[11] & 0x80) ? 0 : a[b[11] % 16])<<24;
        r[2] |= ((b[10] & 0x80) ? 0 : a[b[10] % 16])<<16;
        r[2] |= ((b[9] & 0x80) ? 0 : a[b[9] % 16])<<8;
        r[2] |= ((b[8] & 0x80) ? 0 : a[b[8] % 16]);

        r[3] = ((b[15] & 0x80) ? 0 : a[b[15] % 16])<<24;
        r[3] |= ((b[14] & 0x80) ? 0 : a[b[14] % 16])<<16;
        r[3] |= ((b[13] & 0x80) ? 0 : a[b[13] % 16])<<8;
        r[3] |= ((b[12] & 0x80) ? 0 : a[b[12] % 16]);

        return vld1q_s32(r);
    }
    
    inline __m128i _mm_set_epi32(int i3, int i2, int i1, int i0)
    {
        int32_t __attribute__((aligned(16))) data[4] = { i0, i1, i2, i3 };
        return vld1q_s32(data);
    }
    
    #define _mm_srli_epi16(a, imm)                                                                      \
    ({                                                                                                  \
        __m128i ret;                                                                                    \
        if ((imm) <= 0) {                                                                               \
            ret = a;                                                                                    \
        }                                                                                               \
        else if ((imm)> 31) {                                                                           \
            ret = _mm_setzero_si128();                                                                  \
        }                                                                                               \
        else {                                                                                          \
            ret = vreinterpretq_s32_u16(vshrq_n_u16(vreinterpretq_u16_s32(a), (imm)));                  \
        }                                                                                               \
        ret;                                                                                            \
    })
    #define _mm_srli_si128(a, imm)                                                                      \
    ({                                                                                                  \
        __m128i ret;                                                                                    \
        if ((imm) <= 0) {                                                                               \
            ret = a;                                                                                    \
        }                                                                                               \
        else if ((imm) > 15) {                                                                          \
            ret = _mm_setzero_si128();                                                                  \
        }                                                                                               \
        else {                                                                                          \
            ret = vreinterpretq_s32_s8(vextq_s8(vreinterpretq_s8_s32(a), vdupq_n_s8(0), (imm)));        \
        }                                                                                               \
        ret;                                                                                            \
    })
    #define _mm_slli_si128(a, imm)                                                                      \
    ({                                                                                                  \
        __m128i ret;                                                                                    \
        if ((imm) <= 0) {                                                                               \
            ret = a;                                                                                    \
        }                                                                                               \
        else if ((imm) > 15) {                                                                          \
            ret = _mm_setzero_si128();                                                                  \
        }                                                                                               \
        else {                                                                                          \
            ret = vreinterpretq_s32_s8(vextq_s8(vdupq_n_s8(0), vreinterpretq_s8_s32(a), 16 - (imm)));   \
        }                                                                                               \
        ret;                                                                                            \
    })
    

    #if defined(ARCH_ARM64)
        #define _mm_shuffle_epi32_splat(a, imm) vdupq_laneq_s32(a, (imm))
    #else
        #define _mm_shuffle_epi32_splat(a, imm) vdupq_n_s32(vgetq_lane_s32(a, (imm)))
    #endif
    template<typename A, size_t B> class IntrinsicShuffleEpi32Wrapper
    {
        public:
        static __m128i perform_mm_shuffle_epi32(A a)
        {
            int32x4_t ret;
            ret = vmovq_n_s32(vgetq_lane_s32(a, (B) & 0x3));
            ret = vsetq_lane_s32(vgetq_lane_s32(a, ((B) >> 2) & 0x3), ret, 1);
            ret = vsetq_lane_s32(vgetq_lane_s32(a, ((B) >> 4) & 0x3), ret, 2);
            return vsetq_lane_s32(vgetq_lane_s32(a, ((B) >> 6) & 0x3), ret, 3);
        }
    };
    template<typename A> class IntrinsicShuffleEpi32Wrapper<A, 78>
    {
        public:
        static __m128i perform_mm_shuffle_epi32(A a)
        {
            int32x2_t a32 = vget_high_s32(a);
            int32x2_t a10 = vget_low_s32(a);
            return vcombine_s32(a32, a10);
        }
    };
    template<typename A> class IntrinsicShuffleEpi32Wrapper<A, 177>
    {
        public:
        static __m128i perform_mm_shuffle_epi32(A a)
        {
            int32x2_t a01 = vrev64_s32(vget_low_s32(a));
            int32x2_t a23 = vrev64_s32(vget_high_s32(a));
            return vcombine_s32(a01, a23);
        }
    };
    template<typename A> class IntrinsicShuffleEpi32Wrapper<A, 57>
    {
        public:
        static __m128i perform_mm_shuffle_epi32(A a)
        {
            return vextq_s32(a, a, 1);
        }
    };
    template<typename A> class IntrinsicShuffleEpi32Wrapper<A, 147>
    {
        public:
        static __m128i perform_mm_shuffle_epi32(A a)
        {
            return vextq_s32(a, a, 3);
        }
    };
    template<typename A> class IntrinsicShuffleEpi32Wrapper<A, 68>
    {
        public:
        static __m128i perform_mm_shuffle_epi32(A a)
        {
            int32x2_t a10 = vget_low_s32(a);
            return vcombine_s32(a10, a10);
        }
    };
    template<typename A> class IntrinsicShuffleEpi32Wrapper<A, 65>
    {
        public:
        static __m128i perform_mm_shuffle_epi32(A a)
        {
            int32x2_t a01 = vrev64_s32(vget_low_s32(a));
            int32x2_t a10 = vget_low_s32(a);
            return vcombine_s32(a01, a10);
        }
    };
    template<typename A> class IntrinsicShuffleEpi32Wrapper<A, 17>
    {
        public:
        static __m128i perform_mm_shuffle_epi32(A a)
        {
            int32x2_t a01 = vrev64_s32(vget_low_s32(a));
            return vcombine_s32(a01, a01);
        }
    };
    template<typename A> class IntrinsicShuffleEpi32Wrapper<A, 165>
    {
        public:
        static __m128i perform_mm_shuffle_epi32(A a)
        {
            int32x2_t a11 = vdup_lane_s32(vget_low_s32(a), 1);
            int32x2_t a22 = vdup_lane_s32(vget_high_s32(a), 0);
            return vcombine_s32(a11, a22);
        }
    };
    template<typename A> class IntrinsicShuffleEpi32Wrapper<A, 26>
    {
        public:
        static __m128i perform_mm_shuffle_epi32(A a)
        {
            int32x2_t a22 = vdup_lane_s32(vget_high_s32(a), 0);
            int32x2_t a01 = vrev64_s32(vget_low_s32(a));
            return vcombine_s32(a22, a01);
        }
    };
    template<typename A> class IntrinsicShuffleEpi32Wrapper<A, 254>
    {
        public:
        static __m128i perform_mm_shuffle_epi32(A a)
        {
            int32x2_t a32 = vget_high_s32(a);
        int32x2_t a33 = vdup_lane_s32(vget_high_s32(a), 1);
        return vcombine_s32(a32, a33);
        }
    };
    template<typename A> class IntrinsicShuffleEpi32Wrapper<A, 0>
    {
        public:
        static __m128i perform_mm_shuffle_epi32(A a)
        {
            return _mm_shuffle_epi32_splat((a),0);
        }
    };
    template<typename A> class IntrinsicShuffleEpi32Wrapper<A, 85>
    {
        public:
        static __m128i perform_mm_shuffle_epi32(A a)
        {
            return _mm_shuffle_epi32_splat((a),1);
        }
    };
    template<typename A> class IntrinsicShuffleEpi32Wrapper<A, 170>
    {
        public:
        static __m128i perform_mm_shuffle_epi32(A a)
        {
            return _mm_shuffle_epi32_splat((a),2);
        }
    };
    template<typename A> class IntrinsicShuffleEpi32Wrapper<A, 255>
    {
        public:
        static __m128i perform_mm_shuffle_epi32(A a)
        {
            return _mm_shuffle_epi32_splat((a),3);
        }
    };
    #define _mm_shuffle_epi32(A, B) IntrinsicShuffleEpi32Wrapper<decltype(A), B>::perform_mm_shuffle_epi32(A);
#endif

#endif
