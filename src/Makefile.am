DIST_SUBDIRS = secp256k1
AM_LDFLAGS = $(PTHREAD_CFLAGS) $(LIBTOOL_LDFLAGS)


if EMBEDDED_LEVELDB
LEVELDB_CPPFLAGS += -I$(srcdir)/leveldb/include
LEVELDB_CPPFLAGS += -I$(srcdir)/leveldb/helpers/memenv
LIBLEVELDB += $(builddir)/leveldb/libleveldb.a
LIBMEMENV  += $(builddir)/leveldb/libmemenv.a

# NOTE: This dependency is not strictly necessary, but without it make may try to build both in parallel, which breaks the LevelDB build system in a race
$(LIBLEVELDB): $(LIBMEMENV)

$(LIBLEVELDB) $(LIBMEMENV):
	@echo "Building LevelDB ..." && $(MAKE) -C $(@D) $(@F) CXX="$(CXX)" \
	  CC="$(CC)" PLATFORM=$(TARGET_OS) AR="$(AR)" $(LEVELDB_TARGET_FLAGS) \
          OPT="$(CXXFLAGS) $(CPPFLAGS)"
endif

GULDEN_CONFIG_INCLUDES=-I$(builddir)/config
GULDEN_INCLUDES=-I$(builddir) -I$(builddir)/obj $(BOOST_CPPFLAGS) $(LEVELDB_CPPFLAGS) $(CRYPTO_CFLAGS) $(SSL_CFLAGS)

GULDEN_INCLUDES += -I$(srcdir)/secp256k1/include

LIBGULDEN_SERVER=libgulden_server.a
LIBGULDEN_WALLET=libgulden_wallet.a
LIBGULDEN_COMMON=libgulden_common.a
LIBGULDEN_CLI=libgulden_cli.a
LIBGULDEN_UTIL=libgulden_util.a
LIBGULDEN_CRYPTO=crypto/libgulden_crypto.a
LIBGULDEN_UNIVALUE=univalue/libgulden_univalue.a
LIBBITCOINQT=qt/libguldenqt.a
LIBSECP256K1=secp256k1/libsecp256k1.la

$(LIBSECP256K1): $(wildcard secp256k1/src/*) $(wildcard secp256k1/include/*)
	$(AM_V_at)$(MAKE) $(AM_MAKEFLAGS) -C $(@D) $(@F)

# Make is not made aware of per-object dependencies to avoid limiting building parallelization
# But to build the less dependent modules first, we manually select their order here:
EXTRA_LIBRARIES = \
  crypto/libgulden_crypto.a \
  libgulden_util.a \
  libgulden_common.a \
  univalue/libgulden_univalue.a \
  libgulden_server.a \
  libgulden_cli.a
if ENABLE_WALLET
GULDEN_INCLUDES += $(BDB_CPPFLAGS)
EXTRA_LIBRARIES += libgulden_wallet.a
endif

if BUILD_GULDEN_LIBS
lib_LTLIBRARIES = libguldenconsensus.la
LIBGULDEN_CONSENSUS=libguldenconsensus.la
else
LIBGULDEN_CONSENSUS=
endif

bin_PROGRAMS =
TESTS =

if BUILD_GULDEND
  bin_PROGRAMS += GuldenD
endif

if BUILD_GULDEN_UTILS
  bin_PROGRAMS += Gulden-cli Gulden-tx
endif

GDN_INCLUDES = Gulden/rpcgulden.h \
  Gulden/diff.h \
  Gulden/diff_delta.h \
  Gulden/diff_old.h \
  Gulden/diff_common.h \
  Gulden/hash/hash.h \
  Gulden/hash/city.h \
  Gulden/hash/cityconfig.h \
  Gulden/scrypt.h \
  Gulden/translate.h \
  Gulden/auto_checkpoints.h

GDN_SERVER_SRCS = Gulden/rpcgulden.cpp

GDN_COMMON_SRCS = Gulden/scrypt.cpp \
  Gulden/hash/city.cpp \
  Gulden/diff_delta.cpp \
  Gulden/diff_old.cpp \
  Gulden/diff_common.cpp \
  Gulden/auto_checkpoints.cpp


.PHONY: FORCE
# bitcoin core #
GULDEN_CORE_H = \
  $(GDN_INCLUDES) \
  addrman.h \
  alert.h \
  amount.h \
  arith_uint256.h \
  base58.h \
  bloom.h \
  chain.h \
  chainparamsbase.h \
  chainparams.h \
  chainparamsseeds.h \
  checkpoints.h \
  checkqueue.h \
  clientversion.h \
  coincontrol.h \
  coins.h \
  compat.h \
  compressor.h \
  consensus/params.h \
  core_io.h \
  wallet/db.h \
  eccryptoverify.h \
  ecwrapper.h \
  hash.h \
  init.h \
  key.h \
  keystore.h \
  leveldbwrapper.h \
  limitedmap.h \
  main.h \
  merkleblock.h \
  miner.h \
  mruset.h \
  netbase.h \
  net.h \
  noui.h \
  pow.h \
  primitives/block.h \
  primitives/transaction.h \
  protocol.h \
  pubkey.h \
  random.h \
  rpcclient.h \
  rpcprotocol.h \
  rpcserver.h \
  script/interpreter.h \
  script/script_error.h \
  script/script.h \
  script/sigcache.h \
  script/sign.h \
  script/standard.h \
  serialize.h \
  streams.h \
  support/allocators/secure.h \
  support/allocators/zeroafterfree.h \
  support/cleanse.h \
  support/pagelocker.h \
  sync.h \
  threadsafety.h \
  timedata.h \
  tinyformat.h \
  txdb.h \
  txmempool.h \
  ui_interface.h \
  uint256.h \
  undo.h \
  util.h \
  utilmoneystr.h \
  utilstrencodings.h \
  utiltime.h \
  validationinterface.h \
  version.h \
  wallet/crypter.h \
  wallet/walletdb.h \
  wallet/wallet.h \
  wallet/wallet_ismine.h \
  compat/byteswap.h \
  compat/endian.h \
  compat/sanity.h


JSON_H = \
  json/json_spirit.h \
  json/json_spirit_error_position.h \
  json/json_spirit_reader.h \
  json/json_spirit_reader_template.h \
  json/json_spirit_stream_reader.h \
  json/json_spirit_utils.h \
  json/json_spirit_value.h \
  json/json_spirit_writer.h \
  json/json_spirit_writer_template.h

obj/build.h: FORCE
	@$(MKDIR_P) $(builddir)/obj
	@$(top_srcdir)/share/genbuild.sh $(abs_top_builddir)/src/obj/build.h \
	  $(abs_top_srcdir)
libgulden_util_a-clientversion.$(OBJEXT): obj/build.h

# server: shared between GuldenD and Gulden
libgulden_server_a_CPPFLAGS = $(GULDEN_INCLUDES) $(MINIUPNPC_CPPFLAGS)
libgulden_server_a_SOURCES = \
  $(GDN_SERVER_SRCS) \
  addrman.cpp \
  alert.cpp \
  bloom.cpp \
  chain.cpp \
  checkpoints.cpp \
  init.cpp \
  leveldbwrapper.cpp \
  main.cpp \
  merkleblock.cpp \
  miner.cpp \
  net.cpp \
  noui.cpp \
  pow.cpp \
  rest.cpp \
  rpcblockchain.cpp \
  rpcmining.cpp \
  rpcmisc.cpp \
  rpcnet.cpp \
  rpcrawtransaction.cpp \
  rpcserver.cpp \
  script/sigcache.cpp \
  timedata.cpp \
  txdb.cpp \
  txmempool.cpp \
  validationinterface.cpp \
  $(JSON_H) \
  $(GULDEN_CORE_H)


# wallet: shared between GuldenD and Gulden, but only linked
# when wallet enabled
libgulden_wallet_a_CPPFLAGS = $(GULDEN_INCLUDES)
libgulden_wallet_a_SOURCES = \
  wallet/crypter.cpp \
  wallet/db.cpp \
  wallet/rpcdump.cpp \
  wallet/rpcwallet.cpp \
  wallet/wallet.cpp \
  wallet/wallet_ismine.cpp \
  wallet/walletdb.cpp \
  $(GULDEN_CORE_H)

# crypto primitives library
crypto_libgulden_crypto_a_CPPFLAGS = $(GULDEN_CONFIG_INCLUDES)
crypto_libgulden_crypto_a_SOURCES = \
  crypto/sha1.cpp \
  crypto/sha256.cpp \
  crypto/sha512.cpp \
  crypto/hmac_sha256.cpp \
  crypto/hmac_sha512.cpp \
  crypto/ripemd160.cpp \
  crypto/common.h \
  crypto/sha256.h \
  crypto/sha512.h \
  crypto/hmac_sha256.h \
  crypto/hmac_sha512.h \
  crypto/sha1.h \
  crypto/ripemd160.h

# univalue JSON library
univalue_libgulden_univalue_a_SOURCES = \
  univalue/univalue.cpp \
  univalue/univalue_read.cpp \
  univalue/univalue_write.cpp \
  univalue/univalue_escapes.h \
  univalue/univalue.h

# common: shared between GuldenD, and Gulden and non-server tools
libgulden_common_a_CPPFLAGS = $(GULDEN_INCLUDES)
libgulden_common_a_SOURCES = \
  $(GDN_COMMON_SRCS) \
  arith_uint256.cpp \
  amount.cpp \
  base58.cpp \
  chainparams.cpp \
  coins.cpp \
  compressor.cpp \
  primitives/block.cpp \
  primitives/transaction.cpp \
  core_read.cpp \
  core_write.cpp \
  eccryptoverify.cpp \
  ecwrapper.cpp \
  hash.cpp \
  key.cpp \
  keystore.cpp \
  netbase.cpp \
  protocol.cpp \
  pubkey.cpp \
  script/interpreter.cpp \
  script/script.cpp \
  script/sign.cpp \
  script/standard.cpp \
  script/script_error.cpp \
  $(GULDEN_CORE_H)


# util: shared between all executables.
# This library *must* be included to make sure that the glibc
# backward-compatibility objects and their sanity checks are linked.
libgulden_util_a_CPPFLAGS = $(GULDEN_INCLUDES)
libgulden_util_a_SOURCES = \
  support/pagelocker.cpp \
  chainparamsbase.cpp \
  clientversion.cpp \
  compat/glibc_sanity.cpp \
  compat/glibcxx_sanity.cpp \
  compat/strnlen.cpp \
  random.cpp \
  rpcprotocol.cpp \
  support/cleanse.cpp \
  sync.cpp \
  uint256.cpp \
  util.cpp \
  utilmoneystr.cpp \
  utilstrencodings.cpp \
  utiltime.cpp \
  $(GULDEN_CORE_H)

if GLIBC_BACK_COMPAT
libgulden_util_a_SOURCES += compat/glibc_compat.cpp
endif

# cli: shared between Gulden-cli and Gulden
libgulden_cli_a_CPPFLAGS = $(GULDEN_INCLUDES)
libgulden_cli_a_SOURCES = \
  rpcclient.cpp \
  $(GULDEN_CORE_H)

nodist_libgulden_util_a_SOURCES = $(srcdir)/obj/build.h
#

# GuldenD binary #
GuldenD_SOURCES = GuldenD.cpp
GuldenD_CPPFLAGS = $(GULDEN_INCLUDES)
GuldenD_LDFLAGS = $(RELDFLAGS) $(AM_LDFLAGS) $(LIBTOOL_APP_LDFLAGS)

if TARGET_WINDOWS
GuldenD_SOURCES += GuldenD-res.rc
endif

GuldenD_LDADD = \
  $(LIBGULDEN_SERVER) \
  $(LIBGULDEN_COMMON) \
  $(LIBGULDEN_UNIVALUE) \
  $(LIBGULDEN_UTIL) \
  $(LIBGULDEN_CRYPTO) \
  $(LIBLEVELDB) \
  $(LIBMEMENV) \
  $(LIBSECP256K1)

if ENABLE_WALLET
GuldenD_LDADD += libgulden_wallet.a
endif

GuldenD_LDADD += $(BOOST_LIBS) $(BDB_LIBS) $(SSL_LIBS) $(CRYPTO_LIBS) $(MINIUPNPC_LIBS)
#

# Gulden-cli binary #
Gulden_cli_SOURCES = Gulden-cli.cpp
Gulden_cli_CPPFLAGS = $(GULDEN_INCLUDES)
Gulden_cli_LDFLAGS = $(RELDFLAGS) $(AM_LDFLAGS) $(LIBTOOL_APP_LDFLAGS)

if TARGET_WINDOWS
Gulden_cli_SOURCES += Gulden-cli-res.rc
endif

Gulden_cli_LDADD = \
  $(LIBGULDEN_CLI) \
  $(LIBGULDEN_UTIL) \
  $(LIBSECP256K1)

Gulden_cli_LDADD += $(BOOST_LIBS) $(SSL_LIBS) $(CRYPTO_LIBS)
#

# Gulden-tx binary #
Gulden_tx_SOURCES = Gulden-tx.cpp
Gulden_tx_CPPFLAGS = $(GULDEN_INCLUDES)
Gulden_tx_LDFLAGS = $(RELDFLAGS) $(AM_LDFLAGS) $(LIBTOOL_APP_LDFLAGS)

Gulden_tx_LDADD = \
  $(LIBGULDEN_UNIVALUE) \
  $(LIBGULDEN_COMMON) \
  $(LIBGULDEN_UTIL) \
  $(LIBGULDEN_CRYPTO) \
  $(LIBSECP256K1)

Gulden_tx_LDADD += $(BOOST_LIBS) $(CRYPTO_LIBS)
#

# bitcoinconsensus library #
if BUILD_GULDEN_LIBS
include_HEADERS = script/bitcoinconsensus.h
libguldenconsensus_la_SOURCES = \
  crypto/hmac_sha512.cpp \
  crypto/ripemd160.cpp \
  crypto/sha1.cpp \
  crypto/sha256.cpp \
  crypto/sha512.cpp \
  eccryptoverify.cpp \
  ecwrapper.cpp \
  hash.cpp \
  primitives/transaction.cpp \
  pubkey.cpp \
  script/bitcoinconsensus.cpp \
  script/interpreter.cpp \
  script/script.cpp \
  uint256.cpp \
  utilstrencodings.cpp

if GLIBC_BACK_COMPAT
  libguldenconsensus_la_SOURCES += compat/glibc_compat.cpp
endif

libguldenconsensus_la_LDFLAGS = -no-undefined $(RELDFLAGS)
libguldenconsensus_la_LIBADD = $(CRYPTO_LIBS)
libguldenconsensus_la_CPPFLAGS = $(CRYPTO_CFLAGS) -I$(builddir)/obj -DBUILD_GULDEN_INTERNAL

endif
#

CLEANFILES = leveldb/libleveldb.a leveldb/libmemenv.a *.gcda *.gcno

DISTCLEANFILES = obj/build.h

EXTRA_DIST = leveldb

clean-local:
	-$(MAKE) -C leveldb clean
	-$(MAKE) -C secp256k1 clean
	rm -f leveldb/*/*.gcno leveldb/helpers/memenv/*.gcno
	-rm -f config.h

.rc.o:
	@test -f $(WINDRES)
	$(AM_V_GEN) $(WINDRES) -DWINDRES_PREPROC -i $< -o $@

.mm.o:
	$(AM_V_CXX) $(OBJCXX) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(AM_CPPFLAGS) \
	  $(CPPFLAGS) $(AM_CXXFLAGS) $(QT_INCLUDES) $(CXXFLAGS)  -c -o $@ $<

%.pb.cc %.pb.h: %.proto
	@test -f $(PROTOC)
	$(AM_V_GEN) $(PROTOC) --cpp_out=$(@D) --proto_path=$(abspath $(<D) $<)

if ENABLE_TESTS
include Makefile.test.include
endif

if ENABLE_QT
include Makefile.qt.include
endif

if ENABLE_QT_TESTS
include Makefile.qttest.include
endif
