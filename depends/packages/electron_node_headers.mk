package=electron_node_headers
$(package)_version=18.15.0
$(package)_download_path=https://nodejs.org/dist/v$($(package)_version)/
$(package)_file_name=node-v$($(package)_version)-headers.tar.gz
$(package)_sha256_hash=5ada31143933f0443d20769ae5942c4b1c5c80fe9f8a8221dae12ed9bec2ad2d

$(package)_patches=node.def

$(package)_makelibnode_mingw32=cp -f $($(package)_patch_dir)/node.def . && x86_64-w64-mingw32-dlltool -d node.def -y libnode.a
$(package)_instlibnode_mingw32=&& mkdir $($(package)_staging_prefix_dir)/lib/ &&  cp libnode.a $($(package)_staging_prefix_dir)/lib

define $(package)_preprocess_cmds
  $($(package)_makelibnode_$(host_os))
endef


define $(package)_stage_cmds
  cp -r include $($(package)_staging_prefix_dir) $($(package)_instlibnode_$(host_os))
endef
