package=electron_node_headers
$(package)_version=12.0.2
$(package)_download_path=https://atom.io/download/electron/v$($(package)_version)/
$(package)_file_name=iojs-v$($(package)_version).tar.gz
$(package)_sha256_hash=a2fdef516c393eba4e7db2c846bf7af612e64cadca6bd07c5d34529fcb2a091c

$(package)_patches=node.def

$(package)_makelibnode_mingw32=cp -f $($(package)_patch_dir)/node.def . && x86_64-w64-mingw32-dlltool -d node.def -y libnode.a
$(package)_instlibnode_mingw32=&& mkdir $($(package)_staging_prefix_dir)/lib/ &&  cp libnode.a $($(package)_staging_prefix_dir)/lib

define $(package)_preprocess_cmds
  $($(package)_makelibnode_$(host_os))
endef


define $(package)_stage_cmds
  cp -r include $($(package)_staging_prefix_dir) $($(package)_instlibnode_$(host_os))
endef
