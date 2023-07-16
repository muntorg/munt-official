package=node_addon_api
$(package)_version=7.0.0
$(package)_download_path=https://registry.npmjs.org/node-addon-api/-/
$(package)_file_name=node-addon-api-$($(package)_version).tgz
$(package)_sha256_hash=0535f2f93f0d3f0d94b6724884bae940f1b45b08ee4dd5b0b5b85525c1f7748d

define $(package)_stage_cmds
  mkdir -p $($(package)_staging_prefix_dir)/include && \
  mkdir -p $($(package)_staging_prefix_dir)/include/node-addon-api && \
  cp -r * $($(package)_staging_prefix_dir)/include/node-addon-api/
endef
