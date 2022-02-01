packages:=boost openssl qrencode protobuf electron_node_headers node_addon_api

qrencode_linux_packages = qrencode
qrencode_android_packages = qrencode
qrencode_darwin_packages = qrencode
qrencode_mingw32_packages = qrencode

ifneq ($(host_os),ios)
ifneq ($(host_flavor),android)
packages += libevent zeromq libcryptopp
endif
endif

ios_packages = qrencode djinni libcryptoppunity libevent
ifeq ($(host_flavor),android)
packages += libcryptoppunity
endif


bdb_packages=bdb

zmq_packages=zeromq

upnp_packages=miniupnpc
natpmp_packages=libnatpmp

multiprocess_packages = libmultiprocess capnp
multiprocess_native_packages = native_libmultiprocess native_capnp

usdt_linux_packages=systemtap

darwin_native_packages = native_biplist native_ds_store native_mac_alias

ifneq ($(build_os),darwin)
darwin_native_packages += native_cctools native_libtapi native_cdrkit native_libdmg-hfsplus

ifeq ($(strip $(FORCE_USE_SYSTEM_CLANG)),)
darwin_native_packages+= native_clang
endif

endif
