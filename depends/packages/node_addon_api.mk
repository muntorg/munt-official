package=node_addon_api
$(package)_version=6.0.0
$(package)_download_path=https://registry.npmjs.org/node-addon-api/-/
$(package)_file_name=node-addon-api-$($(package)_version).tgz
$(package)_sha256_hash=16c0b0e9fbb10a79250b4dc8a4ab6e712e9cd918d17bc59e1034699ac8336f4a

define $(package)_stage_cmds
  mkdir -p $($(package)_staging_prefix_dir)/include && \
  mkdir -p $($(package)_staging_prefix_dir)/include/node-addon-api && \
  cp -r * $($(package)_staging_prefix_dir)/include/node-addon-api/
endef
