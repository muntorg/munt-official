package=icu
$(package)_version=60.2
$(package)_download_path=http://download.icu-project.org/files/icu4c/$($(package)_version)
$(package)_file_name=icu4c-60_2-src.tgz
$(package)_sha256_hash=f073ea8f35b926d70bb33e6577508aa642a8b316a803f11be20af384811db418
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

