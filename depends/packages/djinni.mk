package=djinni
$(package)_version=d5475448
$(package)_download_path=https://github.com/mjmacleod/djinni/archive/
$(package)_file_name=dadbe52c7064ef37231de48acd76ffd26e54148f.tar.gz
$(package)_sha256_hash=7cb30b4d1d03a821292784c0384ceb9692c9675d4943ea5838cdbb59ef352702

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
