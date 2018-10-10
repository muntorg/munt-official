package=djinni
$(package)_version=d5475448
$(package)_download_path=https://github.com/dropbox/djinni/archive/
$(package)_file_name=d54754845de08203c9882f1c3d3526a23c9bd5f8.tar.gz
$(package)_sha256_hash=0dfea6737a703991daa549742fb08da120768915b8409c647a38d62701c322c0

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
  mkdir -p $($(package)_staging_prefix_dir)/include && \
  cp support-lib/objc/*.h $($(package)_staging_prefix_dir)/include/
endef
