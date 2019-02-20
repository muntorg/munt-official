package=protobuf
$(package)_version=2.6.1
$(package)_download_path=https://github.com/google/protobuf/releases/download/v$($(package)_version)
$(package)_file_name=protobuf-$($(package)_version).tar.bz2
$(package)_sha256_hash=ee445612d544d885ae240ffbcbf9267faa9f593b7b101f21d58beceb92661910
$(package)_cxxflags=-std=c++11

define $(package)_set_vars
  $(package)_config_opts=--disable-shared --with-protoc=$(build_prefix)/bin/protoc
  $(package)_config_opts_linux=--with-pic
endef

define $(package)_config_cmds
  $($(package)_autoconf)
endef

define $(package)_build_cmds
  $(MAKE) -C src libprotobuf.la
endef

define $(package)_stage_cmds
  $(MAKE) DESTDIR=$($(package)_staging_dir) -C src install-libLTLIBRARIES install-nobase_includeHEADERS &&\
  $(MAKE) DESTDIR=$($(package)_staging_dir) install-pkgconfigDATA
endef

define $(package)_postprocess_cmds
  rm lib/libprotoc.a
endef
