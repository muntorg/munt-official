package=node_addon_api
$(package)_version=3.1.0
$(package)_download_path=https://registry.npmjs.org/node-addon-api/-/
$(package)_file_name=node-addon-api-$($(package)_version).tgz
$(package)_sha256_hash=a97a7b2170e0257fe805078e686c6494e2218d08f222bf944fb3eb8558178548

define $(package)_stage_cmds
  mkdir -p $($(package)_staging_prefix_dir)/include && \
  mkdir -p $($(package)_staging_prefix_dir)/include/node-addon-api && \
  cp -r * $($(package)_staging_prefix_dir)/include/node-addon-api/
endef
