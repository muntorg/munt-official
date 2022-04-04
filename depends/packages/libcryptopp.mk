package=libcryptopp
$(package)_version=8.2.0
$(package)_download_path=https://github.com/weidai11/cryptopp/archive/

$(package)_file_name=CRYPTOPP_8_2_0.tar.gz
$(package)_sha256_hash=e3bcd48a62739ad179ad8064b523346abb53767bcbefc01fe37303412292343e

$(package)_makefile_darwin=GNUmakefile
$(package)_makefile_linux=GNUmakefile
$(package)_makefile_mingw32=GNUmakefile
$(package)_makefile_mingw64=GNUmakefile
$(package)_makefile=$($(package)_makefile_$(host_os))

$(package)_cxxflags_debug += 
$(package)_cxxflags_release += -DNDEBUG -O3 -fPIC
$(package)_cxxflags += -std=c++17

define $(package)_preprocess_cmds
endef

define $(package)_set_vars
endef

define $(package)_config_cmds
    sed -Ei.old "s|[^A-Z][(]CXX[)]|$($(package)_cxx)|" GNUmakefile && \
    sed -Ei.old "s|AR = libtool|AR = $($(package)_libtool)|" GNUmakefile && \
    sed -Ei.old "s|CC=gcc|CC=$($(package)_cxx)|" GNUmakefile && \
    sed -Ei.old "s|CC=clang|CC=$($(package)_cxx)|" GNUmakefile && \
    sed -i.old 's|ifeq ($$$$(IS_ARM32),1)|ifeq ($$$$(IS_ARM32)$$$$(IS_ARMV8),10)|' GNUmakefile &&\
    sed -i.old 's|CRYPTOGAMS_AES_FLAG = -march=armv7-a|CRYPTOGAMS_AES_FLAG = -march=armv7-a+fp|g' GNUmakefile &&\
    sed -Ei.old "s|// [#]define CRYPTOPP_NO_CXX17|#define CRYPTOPP_NO_CXX17|" config.h && \
    sed -Ei.old "s|[#] define CRYPTOPP_UNCAUGHT_EXCEPTION_AVAILABLE|//# define CRYPTOPP_UNCAUGHT_EXCEPTION_AVAILABLE|" config.h
endef

define $(package)_build_cmds
  PREFIX=$($(package)_staging_prefix_dir) CXXFLAGS="$($(package)_cxxflags)" $(MAKE) -f GNUmakefile libcryptopp.pc && \
  PREFIX="$($(package)_staging_prefix_dir)" RANLIB="$($(package)_ranlib)" CXXFLAGS="$($(package)_cxxflags)" $(MAKE) -f $($(package)_makefile) static
endef

define $(package)_stage_cmds
  PREFIX=$($(package)_staging_prefix_dir) RANLIB=$($(package)_ranlib) CXXFLAGS="$($(package)_cxxflags)" $(MAKE) -f $($(package)_makefile) install-lib
endef

define $(package)_postprocess_cmds
endef

