packages:=boost openssl qrencode protobuf

ifneq ($(host_os),ios)
ifneq ($(host_flavor),android)
packages += libevent zeromq libcryptopp
endif
endif

ios_packages = qrencode djinni libcryptoppunity libevent
ifeq ($(host_flavor),android)
packages += libcryptoppunity
endif

rapidcheck_packages = rapidcheck

wallet_packages=bdb

upnp_packages=miniupnpc

darwin_native_packages = native_biplist native_ds_store native_mac_alias

ifneq ($(build_os),darwin)
darwin_native_packages += native_cctools native_cdrkit native_libdmg-hfsplus
endif
