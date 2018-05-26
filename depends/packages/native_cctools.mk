package=native_cctools
$(package)_version=877.8-ld64-253.9-1
$(package)_download_path=https://github.com/tpoechtrager/cctools-port/archive
$(package)_file_name=cctools-$($(package)_version).tar.gz
$(package)_sha256_hash=c88b0631b1d7bb5186dd6466a62f5220dc6191f2b2d9c7c122b327385e734aaf
$(package)_build_subdir=cctools
$(package)_clang_version=6.0.0
$(package)_clang_download_path=http://releases.llvm.org/$($(package)_clang_version)
$(package)_clang_download_file=clang+llvm-$($(package)_clang_version)-x86_64-linux-gnu-ubuntu-16.04.tar.xz
$(package)_clang_file_name=clang-llvm-$($(package)_clang_version)-x86_64-linux-gnu-ubuntu-16.04.tar.xz
$(package)_clang_sha256_hash=cc99fda45b4c740f35d0a367985a2bf55491065a501e2dd5d1ad3f97dcac89da
$(package)_extra_sources=$($(package)_clang_file_name)

define $(package)_fetch_cmds
$(call fetch_file,$(package),$($(package)_download_path),$($(package)_download_file),$($(package)_file_name),$($(package)_sha256_hash)) && \
$(call fetch_file,$(package),$($(package)_clang_download_path),$($(package)_clang_download_file),$($(package)_clang_file_name),$($(package)_clang_sha256_hash))
endef

define $(package)_extract_cmds
  mkdir -p $($(package)_extract_dir) && \
  echo "$($(package)_sha256_hash)  $($(package)_source)" > $($(package)_extract_dir)/.$($(package)_file_name).hash && \
  echo "$($(package)_clang_sha256_hash)  $($(package)_source_dir)/$($(package)_clang_file_name)" >> $($(package)_extract_dir)/.$($(package)_file_name).hash && \
  $(build_SHA256SUM) -c $($(package)_extract_dir)/.$($(package)_file_name).hash && \
  mkdir -p toolchain/bin toolchain/lib/clang/3.5/include && \
  tar --strip-components=1 -C toolchain -xf $($(package)_source_dir)/$($(package)_clang_file_name) && \
  rm -f toolchain/lib/libc++abi.so* && \
  echo "#!/bin/sh" > toolchain/bin/$(host)-dsymutil && \
  echo "exit 0" >> toolchain/bin/$(host)-dsymutil && \
  chmod +x toolchain/bin/$(host)-dsymutil && \
  tar --strip-components=1 -xf $($(package)_source)
endef

define $(package)_set_vars
$(package)_config_opts=--target=$(host) --disable-lto-support
$(package)_ldflags+=-Wl,-rpath=\\$$$$$$$$\$$$$$$$$ORIGIN/../lib
$(package)_cc=$($(package)_extract_dir)/toolchain/bin/clang
$(package)_cxx=$($(package)_extract_dir)/toolchain/bin/clang++
endef

define $(package)_preprocess_cmds
  cd $($(package)_build_subdir); ./autogen.sh && \
  sed -i.old "/define HAVE_PTHREADS/d" ld64/src/ld/InputFiles.h
endef

define $(package)_config_cmds
  $($(package)_autoconf)
endef

define $(package)_build_cmds
  $(MAKE)
endef

define $(package)_stage_cmds
  $(MAKE) DESTDIR=$($(package)_staging_dir) install && \
  cd $($(package)_extract_dir)/toolchain && \
  mkdir -p $($(package)_staging_prefix_dir)/lib/clang/$($(package)_clang_version)/include && \
  mkdir -p $($(package)_staging_prefix_dir)/bin $($(package)_staging_prefix_dir)/include && \
  cp bin/clang $($(package)_staging_prefix_dir)/bin/ &&\
  cp -P bin/clang++ $($(package)_staging_prefix_dir)/bin/ &&\
  cp lib/libLTO.so $($(package)_staging_prefix_dir)/lib/ && \
  cp -rf lib/clang/$($(package)_clang_version)/include/* $($(package)_staging_prefix_dir)/lib/clang/$($(package)_clang_version)/include/ && \
  cp bin/llvm-dsymutil $($(package)_staging_prefix_dir)/bin/$(host)-dsymutil && \
  if `test -d include/c++/`; then cp -rf include/c++/ $($(package)_staging_prefix_dir)/include/; fi && \
  if `test -d lib/c++/`; then cp -rf lib/c++/ $($(package)_staging_prefix_dir)/lib/; fi
endef
