package=icu
$(package)_version=58.1
$(package)_download_path=http://download.icu-project.org/files/icu4c/58.1
$(package)_file_name=icu4c-58_1-src.tgz
$(package)_sha256_hash=0eb46ba3746a9c2092c8ad347a29b1a1b4941144772d13a88667a7b11ea30309
$(package)_build_subdir=source


$(package)_patch_xlocale = sed -i 's|xlocale[.]h|locale.h|' i18n/digitlst.cpp
$(package)_reverse_patch_xlocale_linux = true
$(package)_reverse_patch_xlocale_mingw32 = true
$(package)_reverse_patch_xlocale_darwin = sed -i 's|locale[.]h|xlocale.h|' i18n/digitlst.cpp
$(package)_reverse_patch_xlocale =  $($(package)_reverse_patch_xlocale_$(host_os))


define $(package)_set_vars
$(package)_config_opts=
endef

define $(package)_config_cmds
  $($(package)_patch_xlocale) && \
  mkdir native && cd native && \
  PATH=$(or $(HOST_NATIVE_PATH),${PATH}) ../configure && \
  PATH=$(or $(HOST_NATIVE_PATH),${PATH}) make -j 10 && cd .. && \
  $($(package)_reverse_patch_xlocale) && \
  PATH=${PATH} $($(package)_autoconf) --with-cross-build=$($(package)_build_dir)/native --prefix=$(host_prefix) --enable-extras=no \
  --enable-strict=no -enable-shared=yes --enable-tests=no \
  --enable-samples=no --enable-dyload=no --enable-tools=no CXXFLAGS="--std=c++11"
endef

define $(package)_build_cmds
  sed -i.old 's|gCLocale = _create_locale(LC_ALL, "C");|gCLocale = NULL;|' i18n/digitlst.cpp && \
  sed -i.old "s|freelocale(gCLocale);||" i18n/digitlst.cpp && \
  PATH=${PATH} $(MAKE)
endef

define $(package)_stage_cmds
  $(MAKE) DESTDIR=$($(package)_staging_dir) install $($(package)_build_opts)
endef

