package=icu
$(package)_version=58.1
$(package)_download_path=http://download.icu-project.org/files/icu4c/58.1
$(package)_file_name=icu4c-58_1-src.tgz
$(package)_sha256_hash=0eb46ba3746a9c2092c8ad347a29b1a1b4941144772d13a88667a7b11ea30309
$(package)_build_subdir=source


define $(package)_set_vars
$(package)_config_opts=
endef

define $(package)_config_cmds
  mkdir native && cd native && ../configure && make -j 10 && cd .. && \
  $($(package)_autoconf) --with-cross-build=$($(package)_build_dir)/native --enable-extras=no \
  --enable-strict=no -enable-shared=yes --enable-tests=no \
  --enable-samples=no --enable-dyload=no --enable-tools=no
endef

define $(package)_build_cmds
  sed -i.old 's|gCLocale = _create_locale(LC_ALL, "C");|gCLocale = NULL;|' i18n/digitlst.cpp && \
  sed -i.old "s|freelocale(gCLocale);||" i18n/digitlst.cpp && \
  $(MAKE)
endef

define $(package)_stage_cmds
  $(MAKE) DESTDIR=$($(package)_staging_dir) install
endef

