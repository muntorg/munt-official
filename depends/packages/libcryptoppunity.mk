package=libcryptopp
$(package)_version=8.2.0
$(package)_download_path=https://github.com/robobit/cryptopp/archive/

$(package)_file_name=04e8eb372d436acb35a70d9aa8453beffbeaad66.tar.gz
$(package)_sha256_hash=93e88e5b001cded565908f6c70cd36a5771b24d2a1c6a2d64401b8217b2dc8c0

$(package)_makefile_$(host_os)=GNUmakefile-cross
$(package)_makefile=$($(package)_makefile_$(host_os))

$(package)_env_script_$(host_os)=./setenv-android-clang.sh
$(package)_env_script_ios=./setenv-ios.sh
$(package)_env_script=$($(package)_env_script_$(host_os))

$(package)_cxxflags_debug += -DDEBUG -DDEBUG_LOCKORDER
$(package)_cxxflags_release += -DNDEBUG -O3 -fPIC
$(package)_cxxflags += -std=c++17

$(package)_cross_target_$(host_arch)_$(host_os)=$(host_arch)
$(package)_cross_target_aarch64_ios=arm64
$(package)_cross_target_x86_64_ios=x86_64
$(package)_cross_target=$($(package)_cross_target_$(host_arch)_$(host_os))

ifeq ($(host_os),android)
$(package)_env_arch_arm=AOSP_FLAGS="-march=armv7-a -mthumb -mfpu=vfpv3-d16 -mfloat-abi=softfp -DCRYPTOPP_DISABLE_ASM -Wl,--fix-cortex-a8 -funwind-tables -fexceptions -frtti"
$(package)_env_arch_arm+=AOSP_STL_LIB="$(NDK_ROOT)/sources/cxx-stl/llvm-libc++/libs/armeabi-v7a/libc++_static.a"

$(package)_env_arch_aarch64=AOSP_FLAGS="-funwind-tables -fexceptions -frtti"
$(package)_env_arch_aarch64+=AOSP_STL_LIB="$(NDK_ROOT)/sources/cxx-stl/llvm-libc++/libs/arm64-v8a/libc++_static.a"

$(package)_env_arch_x86_64=AOSP_FLAGS="-DCRYPTOPP_DISABLE_ASM"
$(package)_env_arch_x86_64+=AOSP_STL_LIB="$(NDK_ROOT)/sources/cxx-stl/llvm-libc++/libs/x86_64/libc++_static.a"

$(package)_env_arch_i686=AOSP_FLAGS="-DCRYPTOPP_DISABLE_ASM"
$(package)_env_arch_i686+=AOSP_STL_LIB="$(NDK_ROOT)/sources/cxx-stl/llvm-libc++/libs/x86/libc++_static.a"

$(package)_env=IS_ANDROID=1
$(package)_env+=$($(package)_env_arch_$(host_arch))
endif

ifeq ($(host_os),ios)
$(package)_env_arch_aarch64=IOS_FLAGS=
$(package)_env_arch_x86_64=IOS_FLAGS=-DCRYPTOPP_DISABLE_ASM

$(package)_env=IS_IOS=1
$(package)_env+=IOS_SYSROOT=$(ios_SDK)
$(package)_env+=IOS_ARCH=$($(package)_cross_target)
$(package)_env+=$($(package)_env_arch_$(host_arch))
endif

define $(package)_preprocess_cmds
    sed -Ei.old "s|// [#]define CRYPTOPP_NO_CXX17|#define CRYPTOPP_NO_CXX17|" config.h && \
    sed -Ei.old "s|[#] define CRYPTOPP_UNCAUGHT_EXCEPTION_AVAILABLE|//# define CRYPTOPP_UNCAUGHT_EXCEPTION_AVAILABLE|" config.h 
endef

define $(package)_set_vars
endef

ifeq ($(host_os),android)
define $(package)_config_cmds
   cp "$(NDK_ROOT)/sources/android/cpufeatures/cpu-features.h" . && \
   cp "$(NDK_ROOT)/sources/android/cpufeatures/cpu-features.c" .
endef
else
define $(package)_config_cmds
endef
endif

define $(package)_build_cmds
  PREFIX=$($(package)_staging_prefix_dir) $(MAKE) -f GNUmakefile libcryptopp.pc && \
  $($(package)_env) PREFIX=$($(package)_staging_prefix_dir) RANLIB=$($(package)_ranlib) $(MAKE) -f $($(package)_makefile) static
endef

define $(package)_stage_cmds
  PREFIX=$($(package)_staging_prefix_dir) RANLIB=$($(package)_ranlib) $(MAKE) -f $($(package)_makefile) install-lib
endef

define $(package)_postprocess_cmds
endef

