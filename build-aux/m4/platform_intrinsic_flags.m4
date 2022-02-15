dnl Copyright (c) 2019 The Gulden developers
dnl Authored by: Malcolm MacLeod (mmacleod@gmx.com)
dnl Distributed under the GULDEN software license, see the accompanying file COPYING

dnl Figure out which intrinsics our compiler can handle for its architecture at compile time and the flag combinations to obtain them
dnl NB! This is for compile time support, not runtime support, not every cpu will support these flags.
dnl The program must take care of filtering out implementations with unsupported flags at runtime.

AC_DEFUN([INTRINSIC_FLAG_CHECK],
[
  AC_MSG_CHECKING([if $CXX supports $1])
  AC_LANG_PUSH([C++])
  ac_saved_cxxflags="$CXXFLAGS"
  COMPILERINSTRINSICS=""
  
  dnl -------------------------- Start of x86 tests ------------------------------------------
  INTRINSICFLAGS="-O3 -mmmx  -msse  -msse2 -DCOMPILER_HAS_SSE2"
  CXXFLAGS="-Werror $INTRINSICFLAGS"
  PASSED=no
  AC_COMPILE_IFELSE([AC_LANG_PROGRAM([])], [PASSED=yes], [PASSED=no] )
  AS_IF([test "$PASSED" = yes], [AC_SUBST(PLATFORM_INTRINSICS_SSE2_FLAGS, $INTRINSICFLAGS)])
  AS_IF([test "$PASSED" = yes], COMPILERINSTRINSICS+="-DCOMPILER_HAS_SSE2 ")
  
  INTRINSICFLAGS="-O3 -mmmx -msse -msse2 -msse3 -mssse3 -DCOMPILER_HAS_SSE3"
  CXXFLAGS="-Werror $INTRINSICFLAGS"
  AC_COMPILE_IFELSE([AC_LANG_PROGRAM([])], [PASSED=yes], [PASSED=no] )
  AS_IF([test "$PASSED" = yes], [AC_SUBST(PLATFORM_INTRINSICS_SSE3_FLAGS, $INTRINSICFLAGS)])
  AS_IF([test "$PASSED" = yes], COMPILERINSTRINSICS+="-DCOMPILER_HAS_SSE3 ")
  
  dnl fixme: We should handle also -msse4.1  -msse4.2  -msse4 -msse4a all as seperate things; however these are a complicated mess, for now we just assume that SSE4 includes 4.1 and 4.2 and if these aren't present then no sse4 at all
  INTRINSICFLAGS="-O3 -mmmx  -msse  -msse2  -msse3  -mssse3 -msse4.1 -msse4.2 -DCOMPILER_HAS_SSE4"
  CXXFLAGS="-Werror $INTRINSICFLAGS"
  AC_COMPILE_IFELSE([AC_LANG_PROGRAM([])], [PASSED=yes], [PASSED=no] )
  AS_IF([test "$PASSED" = yes], [AC_SUBST(PLATFORM_INTRINSICS_SSE4_FLAGS, $INTRINSICFLAGS)])
  AS_IF([test "$PASSED" = yes], COMPILERINSTRINSICS+="-DCOMPILER_HAS_SSE4 ")
  
  INTRINSICFLAGS="-O3 -msse4 -msha -DCOMPILER_HAS_SSE4_SHANI"
  CXXFLAGS="-Werror $INTRINSICFLAGS"
  AC_COMPILE_IFELSE([AC_LANG_PROGRAM([])], [PASSED=yes], [PASSED=no] )
  AS_IF([test "$PASSED" = yes], [AC_SUBST(PLATFORM_INTRINSICS_SSE4_SHANI_FLAGS, $INTRINSICFLAGS)])
  AS_IF([test "$PASSED" = yes], COMPILERINSTRINSICS+="-DCOMPILER_HAS_SSE4_SHANI ")  
  
  INTRINSICFLAGS="-O3 -mmmx  -msse  -msse2  -msse3  -mssse3 -msse4.1 -msse4.2 -mavx -DCOMPILER_HAS_AVX"
  CXXFLAGS="-Werror $INTRINSICFLAGS"
  AC_COMPILE_IFELSE([AC_LANG_PROGRAM([])], [PASSED=yes], [PASSED=no] )
  AS_IF([test "$PASSED" = yes], [AC_SUBST(PLATFORM_INTRINSICS_AVX_FLAGS, $INTRINSICFLAGS)])
  AS_IF([test "$PASSED" = yes], COMPILERINSTRINSICS+="-DCOMPILER_HAS_AVX ")
  
  INTRINSICFLAGS="-O3 -mmmx  -msse  -msse2  -msse3  -mssse3 -msse4.1 -msse4.2 -mavx -mavx2 -DCOMPILER_HAS_AVX2"
  CXXFLAGS="-Werror $INTRINSICFLAGS"
  AC_COMPILE_IFELSE([AC_LANG_PROGRAM([])], [PASSED=yes], [PASSED=no] )
  AS_IF([test "$PASSED" = yes], [AC_SUBST(PLATFORM_INTRINSICS_AVX2_FLAGS, $INTRINSICFLAGS)])
  AS_IF([test "$PASSED" = yes], COMPILERINSTRINSICS+="-DCOMPILER_HAS_AVX2 ")
  
  INTRINSICFLAGS="-O3 -mmmx  -msse  -msse2  -msse3  -mssse3 -msse4.1 -msse4.2 -mavx -mavx2 -mavx512f -DCOMPILER_HAS_AVX512F"
  CXXFLAGS="-Werror $INTRINSICFLAGS"
  AC_COMPILE_IFELSE([AC_LANG_PROGRAM([])], [PASSED=yes], [PASSED=no] )
  AS_IF([test "$PASSED" = yes], [AC_SUBST(PLATFORM_INTRINSICS_AVX512F_FLAGS, $INTRINSICFLAGS)])
  AS_IF([test "$PASSED" = yes], COMPILERINSTRINSICS+="-DCOMPILER_HAS_AVX512F ")
  
  dnl fixme: We should handle also -mavx512pf  -mavx512er  -mavx512cd  -mavx512vl -mavx512bw  -mavx512dq  -mavx512ifma  -mavx512vbmi - potentially, though its unclear if any of our code would benefit from these
  
  INTRINSICFLAGS="-maes -DCOMPILER_HAS_AES"
  CXXFLAGS="-Werror $INTRINSICFLAGS"
  AC_COMPILE_IFELSE([AC_LANG_PROGRAM([])], [PASSED=yes], [PASSED=no] )
  AS_IF([test "$PASSED" = yes], [AC_SUBST(PLATFORM_INTRINSICS_AES_FLAGS, $INTRINSICFLAGS)])
  AS_IF([test "$PASSED" = yes], COMPILERINSTRINSICS+="-DCOMPILER_HAS_AES")
  
  dnl -------------------------- End of x86 tests ------------------------------------------
  dnl -------------------------- Start of arm tests ------------------------------------------

  INTRINSICFLAGS="-O3 -mcpu=cortex-a53 -mfloat-abi=hard -mfpu=neon-fp-armv8 -mneon-for-64bits -DCOMPILER_HAS_CORTEX53"
  CXXFLAGS="-Werror $INTRINSICFLAGS"
  AC_COMPILE_IFELSE([AC_LANG_PROGRAM([])], [PASSED=yes], [PASSED=no] )
  AS_IF([test "$PASSED" = yes], [AC_SUBST(PLATFORM_INTRINSICS_CORTEX53_FLAGS, $INTRINSICFLAGS)])
  AS_IF([test "$PASSED" = yes], COMPILERINSTRINSICS+="-DCOMPILER_HAS_CORTEX53 ")
  
  INTRINSICFLAGS="-O3 -mcpu=cortex-a53+simd -DCOMPILER_HAS_CORTEX53"
  CXXFLAGS="-Werror $INTRINSICFLAGS"
  AC_COMPILE_IFELSE([AC_LANG_PROGRAM([])], [PASSED=yes], [PASSED=no] )
  AS_IF([test "$PASSED" = yes], [AC_SUBST(PLATFORM_INTRINSICS_CORTEX53_FLAGS, $INTRINSICFLAGS)])
  AS_IF([test "$PASSED" = yes], COMPILERINSTRINSICS+="-DCOMPILER_HAS_CORTEX53 ")
  
  INTRINSICFLAGS="-O3 -mcpu=cortex-a53+crypto -mfloat-abi=hard -mfpu=crypto-neon-fp-armv8 -mneon-for-64bits -DCOMPILER_HAS_CORTEX53_AES"
  CXXFLAGS="-Werror $INTRINSICFLAGS"
  AC_COMPILE_IFELSE([AC_LANG_PROGRAM([])], [PASSED=yes], [PASSED=no] )
  AS_IF([test "$PASSED" = yes], [AC_SUBST(PLATFORM_INTRINSICS_CORTEX53_AES_FLAGS, $INTRINSICFLAGS)])
  AS_IF([test "$PASSED" = yes], COMPILERINSTRINSICS+="-DCOMPILER_HAS_CORTEX53_AES ")
  
  INTRINSICFLAGS="-O3 -mcpu=cortex-a53+simd+crypto -DCOMPILER_HAS_CORTEX53_AES"
  CXXFLAGS="-Werror $INTRINSICFLAGS"
  AC_COMPILE_IFELSE([AC_LANG_PROGRAM([])], [PASSED=yes], [PASSED=no] )
  AS_IF([test "$PASSED" = yes], [AC_SUBST(PLATFORM_INTRINSICS_CORTEX53_AES_FLAGS, $INTRINSICFLAGS)])
  AS_IF([test "$PASSED" = yes], COMPILERINSTRINSICS+="-DCOMPILER_HAS_CORTEX53_AES ")
  
  INTRINSICFLAGS="-O3 -mcpu=cortex-a57 -mfloat-abi=hard -mfpu=neon-fp-armv8 -mneon-for-64bits -DCOMPILER_HAS_CORTEX57"
  CXXFLAGS="-Werror $INTRINSICFLAGS"
  AC_COMPILE_IFELSE([AC_LANG_PROGRAM([])], [PASSED=yes], [PASSED=no] )
  AS_IF([test "$PASSED" = yes], [AC_SUBST(PLATFORM_INTRINSICS_CORTEX57_FLAGS, $INTRINSICFLAGS)])
  AS_IF([test "$PASSED" = yes], COMPILERINSTRINSICS+="-DCOMPILER_HAS_CORTEX57 ")
  
  INTRINSICFLAGS="-O3 -mcpu=cortex-a57+simd -DCOMPILER_HAS_CORTEX57"
  CXXFLAGS="-Werror $INTRINSICFLAGS"
  AC_COMPILE_IFELSE([AC_LANG_PROGRAM([])], [PASSED=yes], [PASSED=no] )
  AS_IF([test "$PASSED" = yes], [AC_SUBST(PLATFORM_INTRINSICS_CORTEX57_FLAGS, $INTRINSICFLAGS)])
  AS_IF([test "$PASSED" = yes], COMPILERINSTRINSICS+="-DCOMPILER_HAS_CORTEX57 ")
  
  INTRINSICFLAGS="-O3 -mcpu=cortex-a57+crypto -mfloat-abi=hard -mfpu=crypto-neon-fp-armv8 -mneon-for-64bits -DCOMPILER_HAS_CORTEX57_AES"
  CXXFLAGS="-Werror $INTRINSICFLAGS"
  AC_COMPILE_IFELSE([AC_LANG_PROGRAM([])], [PASSED=yes], [PASSED=no] )
  AS_IF([test "$PASSED" = yes], [AC_SUBST(PLATFORM_INTRINSICS_CORTEX57_AES_FLAGS, $INTRINSICFLAGS)])
  AS_IF([test "$PASSED" = yes], COMPILERINSTRINSICS+="-DCOMPILER_HAS_CORTEX57_AES ")
  
  INTRINSICFLAGS="-O3 -mcpu=cortex-a57+simd+crypto -DCOMPILER_HAS_CORTEX57_AES"
  CXXFLAGS="-Werror $INTRINSICFLAGS"
  AC_COMPILE_IFELSE([AC_LANG_PROGRAM([])], [PASSED=yes], [PASSED=no] )
  AS_IF([test "$PASSED" = yes], [AC_SUBST(PLATFORM_INTRINSICS_CORTEX57_AES_FLAGS, $INTRINSICFLAGS)])
  AS_IF([test "$PASSED" = yes], COMPILERINSTRINSICS+="-DCOMPILER_HAS_CORTEX57_AES ")
  
  INTRINSICFLAGS="-O3 -mcpu=cortex-a72 -mfloat-abi=hard -mfpu=neon-fp-armv8 -mneon-for-64bits -DCOMPILER_HAS_CORTEX72"
  CXXFLAGS="-Werror $INTRINSICFLAGS"
  AC_COMPILE_IFELSE([AC_LANG_PROGRAM([])], [PASSED=yes], [PASSED=no] )
  AS_IF([test "$PASSED" = yes], [AC_SUBST(PLATFORM_INTRINSICS_CORTEX72_FLAGS, $INTRINSICFLAGS)])
  AS_IF([test "$PASSED" = yes], COMPILERINSTRINSICS+="-DCOMPILER_HAS_CORTEX72 ")
  
  INTRINSICFLAGS="-O3 -mcpu=cortex-a72+simd -DCOMPILER_HAS_CORTEX72"
  CXXFLAGS="-Werror $INTRINSICFLAGS"
  AC_COMPILE_IFELSE([AC_LANG_PROGRAM([])], [PASSED=yes], [PASSED=no] )
  AS_IF([test "$PASSED" = yes], [AC_SUBST(PLATFORM_INTRINSICS_CORTEX72_FLAGS, $INTRINSICFLAGS)])
  AS_IF([test "$PASSED" = yes], COMPILERINSTRINSICS+="-DCOMPILER_HAS_CORTEX72 ")
  
  INTRINSICFLAGS="-O3 -mcpu=cortex-a72+crypto -mfloat-abi=hard -mfpu=crypto-neon-fp-armv8 -mneon-for-64bits -DCOMPILER_HAS_CORTEX72_AES"
  CXXFLAGS="-Werror $INTRINSICFLAGS"
  AC_COMPILE_IFELSE([AC_LANG_PROGRAM([])], [PASSED=yes], [PASSED=no] )
  AS_IF([test "$PASSED" = yes], [AC_SUBST(PLATFORM_INTRINSICS_CORTEX72_AES_FLAGS, $INTRINSICFLAGS)])
  AS_IF([test "$PASSED" = yes], COMPILERINSTRINSICS+="-DCOMPILER_HAS_CORTEX72_AES ")
  
  INTRINSICFLAGS="-O3 -mcpu=cortex-a72+simd+crypto -DCOMPILER_HAS_CORTEX72_AES"
  CXXFLAGS="-Werror $INTRINSICFLAGS"
  AC_COMPILE_IFELSE([AC_LANG_PROGRAM([])], [PASSED=yes], [PASSED=no] )
  AS_IF([test "$PASSED" = yes], [AC_SUBST(PLATFORM_INTRINSICS_CORTEX72_AES_FLAGS, $INTRINSICFLAGS)])
  AS_IF([test "$PASSED" = yes], COMPILERINSTRINSICS+="-DCOMPILER_HAS_CORTEX72_AES ")
  
  INTRINSICFLAGS="-O3 -mcpu=thunderx+simd+crypto -DCOMPILER_HAS_THUNDERX_AES"
  CXXFLAGS="-Werror $INTRINSICFLAGS"
  AC_COMPILE_IFELSE([AC_LANG_PROGRAM([])], [PASSED=yes], [PASSED=no] )
  AS_IF([test "$PASSED" = yes], [AC_SUBST(PLATFORM_INTRINSICS_THUNDERX_AES_FLAGS, $INTRINSICFLAGS)])
  AS_IF([test "$PASSED" = yes], COMPILERINSTRINSICS+="-DCOMPILER_HAS_THUNDERX_AES ")
  
  INTRINSICFLAGS="-O3 -march=armv8-a+crypto -DCOMPILER_HAS_ARMV8_CRYPTO"
  CXXFLAGS="-Werror $INTRINSICFLAGS"
  AC_COMPILE_IFELSE([AC_LANG_PROGRAM([])], [PASSED=yes], [PASSED=no] )
  AS_IF([test "$PASSED" = yes], [AC_SUBST(PLATFORM_INTRINSICS_ARMV8_CRYPTO_FLAGS, $INTRINSICFLAGS)])
  AS_IF([test "$PASSED" = yes], COMPILERINSTRINSICS+="-DCOMPILER_HAS_ARMV8_CRYPTO ")
  
  
  
  dnl -------------------------- End of arm tests ------------------------------------------
  
  CXXFLAGS="$ac_saved_cxxflags ${COMPILERINSTRINSICS} "
  AC_LANG_POP([C++])
])
