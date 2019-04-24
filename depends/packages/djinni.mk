package=djinni
$(package)_commit=da0f079711e863542b558aa31690605a8688ab55
$(package)_version=$($(package)_commit)
$(package)_download_path=https://github.com/mjmacleod/djinni/archive/
$(package)_file_name=$($(package)_commit).tar.gz
$(package)_sha256_hash=206c5e81695d9863bf07eee6cb5416c923ffe27d7fb83f4f27261bc24ce4937a

define $(package)_build_cmds
  $(MAKE) djinni && \
  ./support-lib/ios-build-support-lib.sh
endef

define $(package)_stage_cmds
  cp -R src $($(package)_staging_prefix_dir)/$(package) && \
  mkdir -p $($(package)_staging_prefix_dir)/bin && \
  ln -s ../djinni/run-assume-built $($(package)_staging_prefix_dir)/bin/djinni-run && \
  mkdir -p $($(package)_staging_prefix_dir)/lib && \
  cp support-lib/out/apple/libdjinni_support_lib_UNIVERSAL.a $($(package)_staging_prefix_dir)/lib/ && \
  mkdir -p $($(package)_staging_prefix_dir)/include/objc && \
	cp support-lib/*.hpp $($(package)_staging_prefix_dir)/include && \
  cp support-lib/objc/*.h $($(package)_staging_prefix_dir)/include/objc
endef
