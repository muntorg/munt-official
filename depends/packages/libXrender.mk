package=libXrender
$(package)_version=0.9.10
$(package)_download_path=http://xorg.freedesktop.org/releases/individual/lib/
$(package)_file_name=$(package)-$($(package)_version).tar.bz2
$(package)_sha256_hash=c06d5979f86e64cabbde57c223938db0b939dff49fdb5a793a1d3d0396650949
$(package)_dependencies=libX11 libxcb xtrans xextproto xproto renderproto

define $(package)_set_vars
$(package)_config_opts=--disable-static
endef

define $(package)_preprocess_cmds
  sed "s/pthread-stubs//" -i configure
endef

define $(package)_config_cmds
  $($(package)_autoconf)
endef

define $(package)_build_cmds
  $(MAKE)
endef

define $(package)_stage_cmds
  $(MAKE) DESTDIR=$($(package)_staging_dir) install
endef

define $(package)_postprocess_cmds
  rm -rf share/man share/doc
endef
