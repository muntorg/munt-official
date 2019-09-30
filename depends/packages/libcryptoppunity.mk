package=libcryptopp
$(package)_version=8.2.0
$(package)_download_path=https://github.com/weidai11/cryptopp/archive/

$(package)_file_name=CRYPTOPP_8_2_0.tar.gz
$(package)_sha256_hash=e3bcd48a62739ad179ad8064b523346abb53767bcbefc01fe37303412292343e

$(package)_makefile_$(host_os)=GNUmakefile-cross
$(package)_makefile=$($(package)_makefile_$(host_os))

$(package)_cxxflags_debug += -DDEBUG -DDEBUG_LOCKORDER
$(package)_cxxflags_release += -DNDEBUG -O3 -fPIC
$(package)_cxxflags += -std=c++17

$(package)_cross_target_aarch64_ios=arm64
$(package)_cross_target_x86_64_ios=simulator
$(package)_cross_target=$($(package)_cross_target_$(host_arch)_$(host_os))

define $(package)_preprocess_cmds
endef

define $(package)_set_vars
endef

define $(package)_config_cmds
endef

define $(package)_build_cmds
  PREFIX=$($(package)_staging_prefix_dir) $(MAKE) -f GNUmakefile libcryptopp.pc && \
  source ./setenv-ios.sh $($(package)_cross_target) && \
  PREFIX=$($(package)_staging_prefix_dir) RANLIB=$($(package)_ranlib) $(MAKE) -f $($(package)_makefile) static
endef

define $(package)_stage_cmds
  PREFIX=$($(package)_staging_prefix_dir) RANLIB=$($(package)_ranlib) $(MAKE) -f $($(package)_makefile) install-lib
endef

define $(package)_postprocess_cmds
endef

