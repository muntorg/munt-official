package=libcryptopp
$(package)_version=8.2.0
$(package)_download_path=https://github.com/weidai11/cryptopp/archive/

$(package)_file_name=CRYPTOPP_8_2_0.tar.gz
$(package)_sha256_hash=e3bcd48a62739ad179ad8064b523346abb53767bcbefc01fe37303412292343e

define $(package)_preprocess_cmds
endef

define $(package)_set_vars
endef

define $(package)_config_cmds
endef

define $(package)_build_cmds
  PREFIX=$($(package)_staging_dir) $(MAKE) GNUmakefile static libcryptopp.pc
endef

define $(package)_stage_cmds
  PREFIX=$($(package)_staging_dir) $(MAKE) install
endef

define $(package)_postprocess_cmds
endef

