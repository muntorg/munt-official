/* portable.h version 2.0 */
/* March 2002 */

/*
 * IT IS ADVISABLE TO INCLUDE THIS FILE AS THE FIRST INCLUDE in the
 * source, as it makes some optimizations over some other include
 * files, which are not possible if the other include files are
 * included first (this is now applicable to string.h)
 */


#ifndef PORTABLE_C__
#define PORTABLE_C__

#include <limits.h>

/* First, find what we know about the processor:
 * - Little or big endian:
 *	#define NESSIE_BIG_ENDIAN
 *   or
 *	#define NESSIE_LITTLE_ENDIAN
 *   Code below use these definitions to optimize code.
 *   If neither NESSIE_BIG_ENDIAN nor NESSIE_LITTLE_ENDIAN is defined,
 *   a (potenitally slower) portable code that can be used on both
 *   type of processors is used.
 * - native 64-bit processor:
 *   if native 64-bit:
 *	#define NATIVE64
 * - if processor unknown:
 *	#define PROCESSOR_UNKNOWN
 *   to use code that can run on all processors
 */

/* The BIG endian machines: */
#if defined( sun )		/* Newer Sparc's */
#define NESSIE_BIG_ENDIAN
#elif defined ( __ppc__ )     /* PowerPC */
#define NESSIE_BIG_ENDIAN

/* The LITTLE endian machines: */
#elif defined( __ultrix )	/* Older MIPS */
#define NESSIE_LITTLE_ENDIAN
#elif defined( __alpha )	/* Alpha */
#define NESSIE_LITTLE_ENDIAN
#define NATIVE64
#elif defined( i386 )		/* x86 (gcc, or also MSC?) */
#define NESSIE_LITTLE_ENDIAN
#elif defined( __i386 )		/* x86 (gcc, or also MSC?) */
#define NESSIE_LITTLE_ENDIAN
#elif defined( _MSC_VER )	/* x86 (surely MSC) */
#define NESSIE_LITTLE_ENDIAN
#elif defined( __INTEL_COMPILER )  /* x86 (surely Intel compiler icl.exe) */
#define NESSIE_LITTLE_ENDIAN

/* Finally machines with UNKNOWN endianess: */
#elif defined ( _AIX )		/* RS6000 */
#define PROCESSOR_UNKNOWN
#elif defined( __hpux )		/* HP-PA */
#define PROCESSOR_UNKNOWN
#elif defined( __aux )		/* 68K */
#define PROCESSOR_UNKNOWN
#elif defined( __dgux )		/* 88K (but P6 in latest boxes) */
#define PROCESSOR_UNKNOWN
#elif defined( __sgi )		/* Newer MIPS */
#define PROCESSOR_UNKNOWN
#else				/* Any other processor */
#define PROCESSOR_UNKNOWN
#endif

/* If PROCESSOR_UNKNOWN, try to figure out if it is a native 64-bit processor */
#ifdef PROCESSOR_UNKNOWN
/*
 * Various compilers fail in various of the following tests.  If the
 * processor is NATIVE 64-bit, all the tests should pass. If any test
 * fails, then the processor is not native 64-bit.
 * The final result after all the four tests below is correct
 * on all compilers that we checked (on Windows gcc and cl, Mac gcc,
 * Linux gcc, Sun gcc and cc, Alpha gcc and cc).
 * Eli Biham, Dec 2001--March 2002
 */

#ifdef _LONGLONG
#define NATIVE64
#elif !((4294967290UL+7UL) == ((4294967290UL+7UL)&ULONG_MAX))
#undef NATIVE64
#elif !(((1UL << 31) * 2UL) != 0UL)
#undef NATIVE64
#elif !((((1UL << 31) * 2UL)&ULONG_MAX) != 0UL)
#undef NATIVE64
#elif !(ULONG_MAX > 4294967295UL)
#undef NATIVE64
#else
#define NATIVE64
#endif

#endif /* PROCESSOR_UNKNOWN */

#ifdef NATIVE64
#define NATIVE_WORD_SIZE_B 8
#else
#define NATIVE_WORD_SIZE_B 4
#endif

#if defined(NESSIE_LITTLE_ENDIAN) && defined(NESSIE_BIG_ENDIAN)
#error YOU SHOULD NOT HAVE BOTH NESSIE_LITTLE_ENDIAN AND NESSIE_BIG_ENDIAN DEFINED!!!!!
#endif

#if !defined(NESSIE_LITTLE_ENDIAN) && !defined(NESSIE_BIG_ENDIAN)
#warning NEITHER NESSIE_LITTLE_ENDIAN NOR NESSIE_BIG_ENDIAN ARE DEFINED!!!!!
#endif

/* Some code may use the BYTE_ORDER checks available in some compilers */
/* So we define it if it is not yet defined */
/* string.h/stdlib.h includes endian.h that defines BYTE_ORDER indirectly */
/* on Linux */
/* This ensures that we do not define it before endian.h, neither include */
/* a non-existant endian.h on other systems */
/* On some old gcc's, endian.h is not called from the above, */
/* but indirectly from math.h */

/* __USE_STRING_INLINES tells GCC to optimize memcpy when the number */
/* of bytes is constant (see /usr/include/string.h) */

#if defined(__GNUC__) && !defined(__USE_STRING_INLINES)
#define __USE_STRING_INLINES
#endif
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <stdio.h>

#ifndef BYTE_ORDER
#ifdef NESSIE_LITTLE_ENDIAN
#define BIG_ENDIAN 4321
#define LITTLE_ENDIAN 1234
#define BYTE_ORDER 1234
#endif
#ifdef NESSIE_BIG_ENDIAN
#define BIG_ENDIAN 4321
#define LITTLE_ENDIAN 1234
#define BYTE_ORDER 4321
#endif
#endif

/* Definition of minimum-width integer types
 * 
 * u8   -> unsigned integer type, at least 8 bits, equivalent to unsigned char
 * u16  -> unsigned integer type, at least 16 bits
 * u32  -> unsigned integer type, at least 32 bits
 *
 * s8, s16, s32  -> signed counterparts of u8, u16, u32
 * i8_t, i16_t, i32_t  -> macro (non signed/unsigned prefix) counterparts
 * of u8, u16, u32
 *
 * Always use macro's T8(), T16() or T32() to obtain exact-width results,
 * i.e., to specify the size of the result of each expression.
 */

#define i8_t char
typedef signed char s8;
typedef unsigned char u8;

#if UINT_MAX >= 4294967295UL

#define i16_t short
#define i32_t int
typedef signed short s16;
typedef signed int s32;
typedef unsigned short u16;
typedef unsigned int u32;

#define ONE32   0xFFFFFFFFU

#else

#define i16_t int
#define i32_t long
typedef signed int s16;
typedef signed long s32;
typedef unsigned int u16;
typedef unsigned long u32;

#define ONE32   0xFFFFFFFFUL

#endif

#define ONE8    0xFFU
#define ONE16   0xFFFFU

#define T8(x)   ((x) & ONE8)
#define T16(x)  ((x) & ONE16)
#define T32(x)  ((x) & ONE32)

/*
 * If you have problems with the 64-bit definitions, and you do not need them,
 * then comment the following lines.
 *
 * Note: the test is used to detect native 64-bit architectures;
 * if the unsigned long is strictly greater than 32-bit, it is
 * assumed to be at least 64-bit. This will not work correctly
 * on (old) 36-bit architectures (PDP-11 for instance).
 *
 * On non-64-bit architectures, "long long" (or "__int64") is used.
 *
 * (The __int64 code for __MSC_VER is thanks to the authors of Khazad).
 *
 */

#ifdef _MSC_VER

#define i64_t __int64
typedef unsigned __int64 u64;
typedef signed __int64 s64;
#define LL(v)   (v##i64)

#elif defined( NATIVE64 )

#define i64_t long
typedef unsigned long u64;
typedef signed long s64;
#define LL(v)   (v##UL)

#else  /* not NATIVE64 */

#define i64_t long long
typedef unsigned long long u64;
typedef signed long long s64;
#define LL(v)   (v##ULL)

#endif

#define ONE64   LL(0xFFFFFFFFFFFFFFFF)
#define T64(x)  ((x) & ONE64)

#if defined( NATIVE64 )
typedef u64 unative;
#else
typedef u32 unative;
#endif


/*
 * U8TO16_BIG(c) returns the 16-bit value stored in big-endian convention
 * in the unsigned char array pointed to by c.
 */

#ifdef NESSIE_BIG_ENDIAN
#define U8TO16_BIG(c)  (((u16*)(c))[0])
#else
#define U8TO16_BIG(c)  (((u16)T8(*((u8*)(c))) << 8) | \
			((u16)T8(*(((u8*)(c)) + 1))))
#endif

/*
 * U8TO16_LITTLE(c) returns the 16-bit value stored in little-endian convention
 * in the unsigned char array pointed to by c.
 */

#ifdef NESSIE_LITTLE_ENDIAN
#define U8TO16_LITTLE(c)  (((u16*)(c))[0])
#else
#define U8TO16_LITTLE(c)  (((u16)T8(*((u8*)(c)))) | \
			   ((u16)T8(*(((u8*)(c)) + 1)) << 8))
#endif

/*
 * U16TO8_BIG(c, v) stores the 16-bit-value v in big-endian convention
 * into the unsigned char array pointed to by c.
 */

#ifdef NESSIE_BIG_ENDIAN
#define U16TO8_BIG(c, v) ((u16*)(c))[0]=v
#else
/* Make sure that the local variable names do not collide with */
/* variables of the calling code (i.e., those used in c, v) */
#define U16TO8_BIG(c, v)    do { \
		u16 tmp_portable_h_x = (v); \
		u8 *tmp_portable_h_d = (c); \
		tmp_portable_h_d[0] = T8(tmp_portable_h_x >> 8); \
		tmp_portable_h_d[1] = T8(tmp_portable_h_x); \
	} while (0)
#endif

/*
 * U16TO8_LITTLE(c, v) stores the 16-bit-value v in little-endian convention
 * into the unsigned char array pointed to by c.
 */

#ifdef NESSIE_LITTLE_ENDIAN
#define U16TO8_LITTLE(c, v) ((u16*)(c))[0]=v
#else
/* Make sure that the local variable names do not collide with */
/* variables of the calling code (i.e., those used in c, v) */
#define U16TO8_LITTLE(c, v)    do { \
		u16 tmp_portable_h_x = (v); \
		u8 *tmp_portable_h_d = (c); \
		tmp_portable_h_d[0] = T8(tmp_portable_h_x); \
		tmp_portable_h_d[1] = T8(tmp_portable_h_x >> 8); \
	} while (0)
#endif


/*
 * U8TO32_BIG(c) returns the 32-bit value stored in big-endian convention
 * in the unsigned char array pointed to by c.
 */

#ifdef NESSIE_BIG_ENDIAN
#define U8TO32_BIG(c)  (((u32*)(c))[0])
#else
#define U8TO32_BIG(c)  (((u32)T8(*((u8*)(c))) << 24) | \
			((u32)T8(*(((u8*)(c)) + 1)) << 16) | \
			((u32)T8(*(((u8*)(c)) + 2)) << 8) | \
			((u32)T8(*(((u8*)(c)) + 3))))
#endif

/*
 * U8TO32_LITTLE(c) returns the 32-bit value stored in little-endian convention
 * in the unsigned char array pointed to by c.
 */

#ifdef NESSIE_LITTLE_ENDIAN
#define U8TO32_LITTLE(c)  (((u32*)(c))[0])
#else
#define U8TO32_LITTLE(c)  (((u32)T8(*((u8*)(c)))) | \
			   ((u32)T8(*(((u8*)(c)) + 1)) << 8) | \
			   ((u32)T8(*(((u8*)(c)) + 2)) << 16) | \
			   ((u32)T8(*(((u8*)(c)) + 3)) << 24))
#endif

/*
 * U32TO8_BIG(c, v) stores the 32-bit-value v in big-endian convention
 * into the unsigned char array pointed to by c.
 */

#ifdef NESSIE_BIG_ENDIAN
#define U32TO8_BIG(c, v) ((u32*)(c))[0]=v
#else
/* Make sure that the local variable names do not collide with */
/* variables of the calling code (i.e., those used in c, v) */
#define U32TO8_BIG(c, v)    do { \
		u32 tmp_portable_h_x = (v); \
		u8 *tmp_portable_h_d = (c); \
		tmp_portable_h_d[0] = T8(tmp_portable_h_x >> 24); \
		tmp_portable_h_d[1] = T8(tmp_portable_h_x >> 16); \
		tmp_portable_h_d[2] = T8(tmp_portable_h_x >> 8); \
		tmp_portable_h_d[3] = T8(tmp_portable_h_x); \
	} while (0)
#endif

/*
 * U32TO8_LITTLE(c, v) stores the 32-bit-value v in little-endian convention
 * into the unsigned char array pointed to by c.
 */

#ifdef NESSIE_LITTLE_ENDIAN
#define U32TO8_LITTLE(c, v) ((u32*)(c))[0]=v
#else
/* Make sure that the local variable names do not collide with */
/* variables of the calling code (i.e., those used in c, v) */
#define U32TO8_LITTLE(c, v)    do { \
		u32 tmp_portable_h_x = (v); \
		u8 *tmp_portable_h_d = (c); \
		tmp_portable_h_d[0] = T8(tmp_portable_h_x); \
		tmp_portable_h_d[1] = T8(tmp_portable_h_x >> 8); \
		tmp_portable_h_d[2] = T8(tmp_portable_h_x >> 16); \
		tmp_portable_h_d[3] = T8(tmp_portable_h_x >> 24); \
	} while (0)
#endif

/* Now also U8TO64_LITTLE and U64TO8_LITTLE
 * thanks to the submitter of Nimbus
 */
/*
 * U8TO64_BIG(c) returns the 64-bit value stored in big-endian convention
 * in the unsigned char array pointed to by c.
 */

#ifdef NESSIE_BIG_ENDIAN
#define U8TO64_BIG(c)  (((u64*)(c))[0])
#else
#define U8TO64_BIG(c)  (((u64)T8(*(c)) << 56) | ((u64)T8(*((c) + 1)) << 48) | \
                  ((u64)T8(*((c) + 2)) << 40) | ((u64)T8(*((c) + 3)) << 32) | \
                  ((u64)T8(*((c) + 4)) << 24) | ((u64)T8(*((c) + 5)) << 16) | \
                  ((u64)T8(*((c) + 6)) <<  8) | ((u64)T8(*((c) + 7))))
#endif

/*
 * U8TO64_LITTLE(c) returns the 64-bit value stored in little-endian convention
 * in the unsigned char array pointed to by c.
 */

#ifdef NESSIE_LITTLE_ENDIAN
#define U8TO64_LITTLE(c)  (((u64*)(c))[0])
#else
#define U8TO64_LITTLE(c)  (((u64)T8(*(c))) | ((u64)T8(*((c) + 1)) <<  8) | \
               ((u64)T8(*((c) + 2)) << 16) | ((u64)T8(*((c) + 3)) << 24) | \
               ((u64)T8(*((c) + 4)) << 32) | ((u64)T8(*((c) + 5)) << 40) | \
               ((u64)T8(*((c) + 6)) << 48) | ((u64)T8(*((c) + 7)) << 56))
#endif

/*
 * U64TO8_BIG(c, v) stores the 64-bit-value v in big-endian convention
 * into the unsigned char array pointed to by c.
 */

#ifdef NESSIE_BIG_ENDIAN
#define U64TO8_BIG(c, v) ((u64*)(c))[0]=v
#else
/* Make sure that the local variable names do not collide with */
/* variables of the calling code (i.e., those used in c, v) */
#define U64TO8_BIG(c, v)    do { \
		u64 tmp_portable_h_x = (v); \
		u8 *tmp_portable_h_d = (c); \
		tmp_portable_h_d[0] = T8(tmp_portable_h_x >> 56); \
		tmp_portable_h_d[1] = T8(tmp_portable_h_x >> 48); \
		tmp_portable_h_d[2] = T8(tmp_portable_h_x >> 40); \
		tmp_portable_h_d[3] = T8(tmp_portable_h_x >> 32); \
		tmp_portable_h_d[4] = T8(tmp_portable_h_x >> 24); \
		tmp_portable_h_d[5] = T8(tmp_portable_h_x >> 16); \
		tmp_portable_h_d[6] = T8(tmp_portable_h_x >> 8); \
		tmp_portable_h_d[7] = T8(tmp_portable_h_x); \
	} while (0)
#endif

/*
 * U64TO8_LITTLE(c, v) stores the 64-bit-value v in little-endian convention
 * into the unsigned char array pointed to by c.
 */

#ifdef NESSIE_LITTLE_ENDIAN
#define U64TO8_LITTLE(c, v) ((u64*)(c))[0]=v
#else
/* Make sure that the local variable names do not collide with */
/* variables of the calling code (i.e., those used in c, v) */
#define U64TO8_LITTLE(c, v)    do { \
		u64 tmp_portable_h_x = (v); \
		u8 *tmp_portable_h_d = (c); \
		tmp_portable_h_d[0] = T8(tmp_portable_h_x); \
		tmp_portable_h_d[1] = T8(tmp_portable_h_x >> 8);  \
		tmp_portable_h_d[2] = T8(tmp_portable_h_x >> 16); \
		tmp_portable_h_d[3] = T8(tmp_portable_h_x >> 24); \
		tmp_portable_h_d[4] = T8(tmp_portable_h_x >> 32); \
		tmp_portable_h_d[5] = T8(tmp_portable_h_x >> 40); \
		tmp_portable_h_d[6] = T8(tmp_portable_h_x >> 48); \
		tmp_portable_h_d[7] = T8(tmp_portable_h_x >> 56); \
	} while (0)
#endif

/*
 * ROTL32(v, n) returns the value of the 32-bit unsigned value v after
 * a rotation of n bits to the left. It might be replaced by the appropriate
 * architecture-specific macro.
 *
 * It evaluates v and n twice.
 *
 * The compiler might emit a warning if n is the constant 0. The result
 * is undefined if n is greater than 31.
 */
#if defined(WIN32) && defined(_MSC_VER)

#include <stdlib.h>
#pragma intrinsic(_lrotr,_lrotl)
#define ROTR32(x,n) _lrotr(x,n)
#define ROTL32(x,n) _lrotl(x,n)

#elif defined(__BORLANDC__)

#define ROTR32(v, n) _lrotr(v, n)
#define ROTL32(v, n) _lrotl(v, n)

#else

#define ROTL32(v, n)   (T32((v) << ((n)&0x1F)) | \
			T32((v) >> (32 - ((n)&0x1F))))
#define ROTR32(v, n)   (T32((v) >> ((n)&0x1F)) | \
			T32((v) << (32 - ((n)&0x1F))))

#endif

#define ROTL16(v, n)   (T16((v) << ((n)&0xF)) | \
			T16((v) >> (16 - ((n)&0xF))))
#define ROTR16(v, n)   (T16((v) >> ((n)&0xF)) | \
			T16((v) << (16 - ((n)&0xF))))

#define ROTL64(v, n)   (T64((v) << ((n)&0x3F)) | \
			T64((v) >> (64 - ((n)&0x3F))))
#define ROTR64(v, n)   (T64((v) >> ((n)&0x3F)) | \
			T64((v) << (64 - ((n)&0x3F))))


#if defined(__SUNPRO_C)
#undef  __inline
#define __inline
#undef  inline
#define inline

#else

#define inline __inline
#endif

#if defined( __INTEL_COMPILER ) || defined( _MSC_VER )
  /* This assumes including string.h above */
#define bzero(p,l)    memset((p), 0x00, (l))
#define bcopy(s,d,l) memcpy((d), (s), (l))
#endif

/* This function cannot be a macro, as in some of it's calls,
/*  "x++" is passed as an argument, thus increasing x 4 times instead of one */
static inline u32 swap32(u32 x) 
{ 
  return ((((x)&    0xFF)<<24) | (((x)&    0xFF00)<<8) | \
	  (((x)&0xFF0000)>> 8) | (((x)&0xFF000000)>>24)); 
}

#ifndef linux 
#ifdef NESSIE_LITTLE_ENDIAN
#define ntohl(x) swap32(x)
#define htonl(x) swap32(x)
#else 
#define ntohl(x) (x) 
#define htonl(x) (x) 
#endif 
#endif 

#endif   /* PORTABLE_C__ */
