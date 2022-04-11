package=electron_node_headers
$(package)_version=12.0.2
$(package)_download_path=https://atom.io/download/electron/v$($(package)_version)/
$(package)_file_name=iojs-v$($(package)_version).tar.gz
$(package)_sha256_hash=a2fdef516c393eba4e7db2c846bf7af612e64cadca6bd07c5d34529fcb2a091c

define $(package)_stage_cmds
  cp -r include $($(package)_staging_prefix_dir)/
endef
