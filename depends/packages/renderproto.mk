package=renderproto
$(package)_version=0.11
$(package)_download_path=http://xorg.freedesktop.org/releases/individual/proto
$(package)_file_name=$(package)-$($(package)_version).tar.bz2
$(package)_sha256_hash=c4d1d6d9b0b6ed9a328a94890c171d534f62708f0982d071ccd443322bedffc2

define $(package)_set_vars
$(package)_config_opts=--disable-shared
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
