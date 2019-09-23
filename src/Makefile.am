# Copyright (c) 2013-2016 The Bitcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
#
# File contains modifications by: The Gulden developers
# All modifications:
# Copyright (c) 2016-2018 The Gulden developers
# Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
# Distributed under the GULDEN software license, see the accompanying
# file COPYING

DIST_SUBDIRS = secp256k1 univalue

AM_LDFLAGS = $(PTHREAD_CFLAGS) $(LIBTOOL_LDFLAGS) $(HARDENED_LDFLAGS)
AM_CXXFLAGS = $(HARDENED_CXXFLAGS) $(ERROR_CXXFLAGS)
AM_CPPFLAGS = $(HARDENED_CPPFLAGS)
EXTRA_LIBRARIES =

if EMBEDDED_UNIVALUE
LIBUNIVALUE = univalue/libunivalue.la

$(LIBUNIVALUE): $(wildcard univalue/lib/*) $(wildcard univalue/include/*)
	$(AM_V_at)$(MAKE) $(AM_MAKEFLAGS) -C $(@D) $(@F)
else
LIBUNIVALUE = $(UNIVALUE_LIBS)
endif

GULDEN_CONFIG_INCLUDES=-I$(builddir)/config
GULDEN_INCLUDES=-I$(builddir) -I$(builddir)/obj $(BDB_CPPFLAGS) $(BOOST_CPPFLAGS) $(LEVELDB_CPPFLAGS) $(CRYPTO_CFLAGS) $(SSL_CFLAGS) $(CRYPTOPP_CFLAGS)

GULDEN_INCLUDES += -I$(srcdir)/secp256k1/include
GULDEN_INCLUDES += $(UNIVALUE_CFLAGS)

LIBGULDEN_SERVER=libgulden_server.a
LIBGULDEN_COMMON=libgulden_common.a
LIBGULDEN_CONSENSUS=libgulden_consensus.a
LIBGULDEN_CLI=libgulden_cli.a
LIBGULDEN_UTIL=libgulden_util.a
LIBGULDEN_CRYPTO_SSE3=crypto/libgulden_crypto_sse3.a
LIBGULDEN_CRYPTO_SSE3_AES=crypto/libgulden_crypto_sse3_aes.a
LIBGULDEN_CRYPTO_SSE4=crypto/libgulden_crypto_sse4.a
LIBGULDEN_CRYPTO_SSE4_AES=crypto/libgulden_crypto_sse4_aes.a
LIBGULDEN_CRYPTO_AVX=crypto/libgulden_crypto_avx.a
LIBGULDEN_CRYPTO_AVX_AES=crypto/libgulden_crypto_avx_aes.a
LIBGULDEN_CRYPTO_AVX2=crypto/libgulden_crypto_avx2.a
LIBGULDEN_CRYPTO_AVX2_AES=crypto/libgulden_crypto_avx2_aes.a
LIBGULDEN_CRYPTO_AVX512F=crypto/libgulden_crypto_avx512f.a
LIBGULDEN_CRYPTO_AVX512F_AES=crypto/libgulden_crypto_avx512f_aes.a
LIBGULDEN_CRYPTO_ARM_CORTEX_A53=crypto/libgulden_crypto_arm_cortex_a53.a
LIBGULDEN_CRYPTO_ARM_CORTEX_A53_AES=crypto/libgulden_crypto_arm_cortex_a53_aes.a
LIBGULDEN_CRYPTO_ARM_CORTEX_A57=crypto/libgulden_crypto_arm_cortex_a57.a
LIBGULDEN_CRYPTO_ARM_CORTEX_A57_AES=crypto/libgulden_crypto_arm_cortex_a57_aes.a
LIBGULDEN_CRYPTO_ARM_CORTEX_A72=crypto/libgulden_crypto_arm_cortex_a72.a
LIBGULDEN_CRYPTO_ARM_CORTEX_A72_AES=crypto/libgulden_crypto_arm_cortex_a72_aes.a
LIBGULDEN_CRYPTO_ARM_THUNDERX_AES=crypto/libgulden_crypto_arm_thunderx_aes.a
LIBGULDEN_CRYPTO=crypto/libgulden_crypto.a
LIBGULDENQT=qt/libguldenqt.a
LIBSECP256K1=secp256k1/libsecp256k1.la

if ENABLE_ZMQ
LIBGULDEN_ZMQ=libgulden_zmq.a
endif
if BUILD_GULDEN_LIBS
LIBGULDENCONSENSUS=libguldenconsensus.la
endif
if ENABLE_WALLET
LIBGULDEN_WALLET=libgulden_wallet.a
endif

$(LIBSECP256K1): $(wildcard secp256k1/src/*) $(wildcard secp256k1/include/*)
	$(AM_V_at)$(MAKE) $(AM_MAKEFLAGS) -C $(@D) $(@F)

LIBGULDEN_CRYPTO_ALL = $(LIBGULDEN_CRYPTO) $(LIBGULDEN_CRYPTO_SSE3) $(LIBGULDEN_CRYPTO_SSE3_AES) $(LIBGULDEN_CRYPTO_SSE4) $(LIBGULDEN_CRYPTO_SSE4_AES) $(LIBGULDEN_CRYPTO_AVX) $(LIBGULDEN_CRYPTO_AVX_AES) $(LIBGULDEN_CRYPTO_AVX2) $(LIBGULDEN_CRYPTO_AVX2_AES) $(LIBGULDEN_CRYPTO_AVX512F) $(LIBGULDEN_CRYPTO_AVX512F_AES) \
                       $(LIBGULDEN_CRYPTO_ARM_CORTEX_A53) $(LIBGULDEN_CRYPTO_ARM_CORTEX_A53_AES) $(LIBGULDEN_CRYPTO_ARM_CORTEX_A57) $(LIBGULDEN_CRYPTO_ARM_CORTEX_A57_AES) $(LIBGULDEN_CRYPTO_ARM_CORTEX_A72) $(LIBGULDEN_CRYPTO_ARM_CORTEX_A72_AES) $(LIBGULDEN_CRYPTO_ARM_THUNDERX_AES) \
                       $(LIBGULDEN_CRYPTO) $(LIBGULDEN_CRYPTO_SSE3) $(LIBGULDEN_CRYPTO_SSE3_AES) $(LIBGULDEN_CRYPTO_SSE4) $(LIBGULDEN_CRYPTO_SSE4_AES) $(LIBGULDEN_CRYPTO_AVX) $(LIBGULDEN_CRYPTO_AVX_AES) $(LIBGULDEN_CRYPTO_AVX2) $(LIBGULDEN_CRYPTO_AVX2_AES) $(LIBGULDEN_CRYPTO_AVX512F) $(LIBGULDEN_CRYPTO_AVX512F_AES) \
                       $(LIBGULDEN_CRYPTO_ARM_CORTEX_A53) $(LIBGULDEN_CRYPTO_ARM_CORTEX_A53_AES) $(LIBGULDEN_CRYPTO_ARM_CORTEX_A57) $(LIBGULDEN_CRYPTO_ARM_CORTEX_A57_AES) $(LIBGULDEN_CRYPTO_ARM_CORTEX_A72) $(LIBGULDEN_CRYPTO_ARM_CORTEX_A72_AES) $(LIBGULDEN_CRYPTO_ARM_THUNDERX_AES)

# Make is not made aware of per-object dependencies to avoid limiting building parallelization
# But to build the less dependent modules first, we manually select their order here:
EXTRA_LIBRARIES += \
  $(LIBGULDEN_CRYPTO_ALL)\
  $(LIBGULDEN_UTIL) \
  $(LIBGULDEN_COMMON) \
  $(LIBGULDEN_CONSENSUS) \
  $(LIBGULDEN_SERVER) \
  $(LIBGULDEN_CLI) \
  $(LIBGULDEN_WALLET) \
  $(LIBGULDEN_ZMQ)

lib_LTLIBRARIES = $(LIBGULDENCONSENSUS)

bin_PROGRAMS =
noinst_PROGRAMS =
TESTS =
BENCHMARKS =

if BUILD_GULDEND
  bin_PROGRAMS += GuldenD
endif

if BUILD_GULDEN_UTILS
  bin_PROGRAMS += Gulden-cli Gulden-tx
endif

.PHONY: FORCE check-symbols check-security

# core #
GULDEN_CORE_H = Gulden/rpcgulden.h \
  Gulden/Common/diff.h \
  Gulden/Common/diff_delta.h \
  Gulden/Common/diff_old.h \
  Gulden/Common/diff_common.h \
  Gulden/Common/hash/hash.h \
  Gulden/Common/hash/city.h \
  Gulden/Common/hash/cityconfig.h \
  Gulden/Common/scrypt.h \
  Gulden/mnemonic.h \
  Gulden/util.h \
  Gulden/auto_checkpoints.h \
  wallet/Gulden/guldenwallet.h \
  LRUCache/LRUCache11.hpp \
  alert.h \
  account.h \
  addrdb.h \
  addrman.h \
  base58.h \
  bloom.h \
  blockencodings.h \
  blockstore.h \
  chain.h \
  chainparams.h \
  chainparamsbase.h \
  chainparamsseeds.h \
  checkpoints.h \
  checkqueue.h \
  clientversion.h \
  coins.h \
  compat.h \
  compat/arch.h \
  compat/sys.h \
  compat/sse.h \
  compat/sse2neon.h \
  compat/byteswap.h \
  compat/endian.h \
  compat/sanity.h \
  compressor.h \
  consensus/consensus.h \
  consensus/tx_verify.h \
  core_io.h \
  core_memusage.h \
  cuckoocache.h \
  fs.h \
  httprpc.h \
  httpserver.h \
  indirectmap.h \
  init.h \
  unity/appmanager.h \
  unity/signals.h \
  key.h \
  keystore.h \
  dbwrapper.h \
  limitedmap.h \
  memusage.h \
  merkleblock.h \
  generation/miner.h \
  generation/witness.h \
  generation/generation.h \
  net.h \
  net_processing.h \
  netaddress.h \
  netbase.h \
  netmessagemaker.h \
  noui.h \
  policy/feerate.h \
  policy/fees.h \
  policy/policy.h \
  policy/rbf.h \
  pow.h \
  protocol.h \
  random.h \
  reverselock.h \
  rpc/blockchain.h \
  rpc/client.h \
  rpc/protocol.h \
  rpc/server.h \
  rpc/register.h \
  scheduler.h \
  script/sigcache.h \
  script/sign.h \
  script/standard.h \
  script/ismine.h \
  streams.h \
  support/allocators/secure.h \
  support/allocators/zeroafterfree.h \
  support/cleanse.h \
  support/events.h \
  support/lockedpool.h \
  sync.h \
  threadsafety.h \
  threadinterrupt.h \
  timedata.h \
  torcontrol.h \
  txdb.h \
  txmempool.h \
  ui_interface.h \
  undo.h \
  util.h \
  utilmoneystr.h \
  utiltime.h \
  validation/validation.h \
  validation/witnessvalidation.h \
  validation/versionbitsvalidation.h \
  validation/validationinterface.h \
  versionbits.h \
  wallet/coincontrol.h \
  wallet/crypter.h \
  wallet/db.h \
  wallet/feebumper.h \
  wallet/rpcwallet.h \
  wallet/wallet.h \
  wallet/merkletx.h \
  wallet/wallettx.h \
  wallet/walletdb.h \
  wallet/walletdberrors.h \
  warnings.h \
  zmq/zmqabstractnotifier.h \
  zmq/zmqconfig.h\
  zmq/zmqnotificationinterface.h \
  zmq/zmqpublishnotifier.h


obj/build.h: FORCE
	@$(MKDIR_P) $(builddir)/obj
	@$(top_srcdir)/share/genbuild.sh "$(abs_top_builddir)/src/obj/build.h" \
	  "$(abs_top_srcdir)"
libgulden_util_a-clientversion.$(OBJEXT): obj/build.h

# server: shared between GuldenD and Gulden
libgulden_server_a_CPPFLAGS = $(AM_CPPFLAGS) $(GULDEN_INCLUDES) $(MINIUPNPC_CPPFLAGS) $(EVENT_CFLAGS) $(EVENT_PTHREADS_CFLAGS)
libgulden_server_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
libgulden_server_a_SOURCES = \
  Gulden/rpcgulden.cpp \
  addrdb.cpp \
  addrman.cpp \
  alert.cpp \
  Gulden/auto_checkpoints.cpp \
  bloom.cpp \
  blockencodings.cpp \
  blockstore.cpp \
  chain.cpp \
  checkpoints.cpp \
  consensus/tx_verify.cpp \
  httprpc.cpp \
  httpserver.cpp \
  init.cpp \
  dbwrapper.cpp \
  merkleblock.cpp \
  generation/miner.cpp \
  generation/witness.cpp \
  net.cpp \
  net_processing.cpp \
  noui.cpp \
  policy/fees.cpp \
  policy/policy.cpp \
  policy/rbf.cpp \
  pow.cpp \
  rest.cpp \
  rpc/blockchain.cpp \
  rpc/mining.cpp \
  rpc/misc.cpp \
  rpc/netrpc.cpp \
  rpc/rawtransaction.cpp \
  rpc/server.cpp \
  script/sigcache.cpp \
  script/ismine.cpp \
  timedata.cpp \
  torcontrol.cpp \
  txdb.cpp \
  txmempool.cpp \
  Gulden/util.cpp \
  ui_interface.cpp \
  validation/validation.cpp \
  validation/validation_mempool.cpp \
  validation/validation_misc.cpp \
  validation/witnessvalidation.cpp \
  validation/versionbitsvalidation.cpp \
  validation/validationinterface.cpp \
  versionbits.cpp \
  warnings.cpp \
  $(GULDEN_CORE_H)

if ENABLE_ZMQ
libgulden_zmq_a_CPPFLAGS = $(GULDEN_INCLUDES) $(ZMQ_CFLAGS)
libgulden_zmq_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
libgulden_zmq_a_SOURCES = \
  zmq/zmqabstractnotifier.cpp \
  zmq/zmqnotificationinterface.cpp \
  zmq/zmqpublishnotifier.cpp
endif


# wallet: shared between GuldenD and Gulden, but only linked
# when wallet enabled
libgulden_wallet_a_CPPFLAGS = $(AM_CPPFLAGS) $(GULDEN_INCLUDES)
libgulden_wallet_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
libgulden_wallet_a_SOURCES = \
  account.cpp \
  Gulden/mnemonic.cpp \
  wallet/Gulden/guldenwallet.cpp \
  wallet/crypter.cpp \
  wallet/db.cpp \
  wallet/feebumper.cpp \
  wallet/rpcdump.cpp \
  wallet/rpcwallet.cpp \
  wallet/wallet.cpp \
  wallet/wallet_ismine.cpp \
  wallet/wallet_init.cpp \
  wallet/wallet_transaction.cpp \
  wallet/wallet_keypool.cpp \
  wallet/walletbalance.cpp \
  wallet/merkletx.cpp \
  wallet/wallettx.cpp \
  wallet/walletdb.cpp \
  $(GULDEN_CORE_H)

# crypto primitives library
crypto_libgulden_crypto_sse3_a_CPPFLAGS = $(AM_CPPFLAGS) $(GULDEN_CONFIG_INCLUDES) $(PLATFORM_INTRINSICS_SSE3_FLAGS)
crypto_libgulden_crypto_sse3_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS) $(PLATFORM_INTRINSICS_SSE3_FLAGS)
crypto_libgulden_crypto_sse3_a_SOURCES = \
  crypto/hash/sigma/echo256/opt/echo256_opt_sse3.h \
  crypto/hash/sigma/echo256/opt/echo256_opt_sse3.cpp \
  crypto/hash/sigma/shavite3_256/opt/shavite3_256_opt_sse3.h \
  crypto/hash/sigma/shavite3_256/opt/shavite3_256_opt_sse3.cpp \
  crypto/hash/sigma/argon_echo/opt/core_opt_sse3.h \
  crypto/hash/sigma/argon_echo/opt/core_opt_sse3.cpp

crypto_libgulden_crypto_sse3_aes_a_CPPFLAGS = $(AM_CPPFLAGS) $(GULDEN_CONFIG_INCLUDES) $(PLATFORM_INTRINSICS_SSE3_FLAGS) $(PLATFORM_INTRINSICS_AES_FLAGS)
crypto_libgulden_crypto_sse3_aes_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS) $(PLATFORM_INTRINSICS_SSE3_FLAGS) $(PLATFORM_INTRINSICS_AES_FLAGS)
crypto_libgulden_crypto_sse3_aes_a_SOURCES = \
  crypto/hash/sigma/echo256/opt/echo256_opt_sse3_aes.h \
  crypto/hash/sigma/echo256/opt/echo256_opt_sse3_aes.cpp \
  crypto/hash/sigma/shavite3_256/opt/shavite3_256_opt_sse3_aes.h \
  crypto/hash/sigma/shavite3_256/opt/shavite3_256_opt_sse3_aes.cpp \
  crypto/hash/sigma/argon_echo/opt/core_opt_sse3_aes.h \
  crypto/hash/sigma/argon_echo/opt/core_opt_sse3_aes.cpp

crypto_libgulden_crypto_sse4_a_CPPFLAGS = $(AM_CPPFLAGS) $(GULDEN_CONFIG_INCLUDES) $(PLATFORM_INTRINSICS_SSE4_FLAGS)
crypto_libgulden_crypto_sse4_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS) $(PLATFORM_INTRINSICS_SSE4_FLAGS)
crypto_libgulden_crypto_sse4_a_SOURCES = \
  crypto/hash/sigma/echo256/opt/echo256_opt_sse4.h \
  crypto/hash/sigma/echo256/opt/echo256_opt_sse4.cpp \
  crypto/hash/sigma/shavite3_256/opt/shavite3_256_opt_sse4.h \
  crypto/hash/sigma/shavite3_256/opt/shavite3_256_opt_sse4.cpp \
  crypto/hash/sigma/argon_echo/opt/core_opt_sse4.h \
  crypto/hash/sigma/argon_echo/opt/core_opt_sse4.cpp

crypto_libgulden_crypto_sse4_aes_a_CPPFLAGS = $(AM_CPPFLAGS) $(GULDEN_CONFIG_INCLUDES) $(PLATFORM_INTRINSICS_SSE4_FLAGS) $(PLATFORM_INTRINSICS_AES_FLAGS)
crypto_libgulden_crypto_sse4_aes_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS) $(PLATFORM_INTRINSICS_SSE4_FLAGS) $(PLATFORM_INTRINSICS_AES_FLAGS)
crypto_libgulden_crypto_sse4_aes_a_SOURCES = \
  crypto/hash/sigma/echo256/opt/echo256_opt_sse4_aes.h \
  crypto/hash/sigma/echo256/opt/echo256_opt_sse4_aes.cpp \
  crypto/hash/sigma/shavite3_256/opt/shavite3_256_opt_sse4_aes.h \
  crypto/hash/sigma/shavite3_256/opt/shavite3_256_opt_sse4_aes.cpp \
  crypto/hash/sigma/argon_echo/opt/core_opt_sse4_aes.h \
  crypto/hash/sigma/argon_echo/opt/core_opt_sse4_aes.cpp

crypto_libgulden_crypto_avx_a_CPPFLAGS = $(AM_CPPFLAGS) $(GULDEN_CONFIG_INCLUDES) $(PLATFORM_INTRINSICS_AVX_FLAGS)
crypto_libgulden_crypto_avx_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS) $(PLATFORM_INTRINSICS_AVX_FLAGS)
crypto_libgulden_crypto_avx_a_SOURCES = \
  crypto/hash/sigma/echo256/opt/echo256_opt_avx.h \
  crypto/hash/sigma/echo256/opt/echo256_opt_avx.cpp \
  crypto/hash/sigma/shavite3_256/opt/shavite3_256_opt_avx.h \
  crypto/hash/sigma/shavite3_256/opt/shavite3_256_opt_avx.cpp \
  crypto/hash/sigma/argon_echo/opt/core_opt_avx.h \
  crypto/hash/sigma/argon_echo/opt/core_opt_avx.cpp


crypto_libgulden_crypto_avx_aes_a_CPPFLAGS = $(AM_CPPFLAGS) $(GULDEN_CONFIG_INCLUDES) $(PLATFORM_INTRINSICS_AVX_FLAGS) $(PLATFORM_INTRINSICS_AES_FLAGS)
crypto_libgulden_crypto_avx_aes_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS) $(PLATFORM_INTRINSICS_AVX_FLAGS) $(PLATFORM_INTRINSICS_AES_FLAGS)
crypto_libgulden_crypto_avx_aes_a_SOURCES = \
  crypto/hash/sigma/echo256/opt/echo256_opt_avx_aes.h \
  crypto/hash/sigma/echo256/opt/echo256_opt_avx_aes.cpp \
  crypto/hash/sigma/shavite3_256/opt/shavite3_256_opt_avx_aes.h \
  crypto/hash/sigma/shavite3_256/opt/shavite3_256_opt_avx_aes.cpp \
  crypto/hash/sigma/argon_echo/opt/core_opt_avx_aes.h \
  crypto/hash/sigma/argon_echo/opt/core_opt_avx_aes.cpp


crypto_libgulden_crypto_avx2_a_CPPFLAGS = $(AM_CPPFLAGS) $(GULDEN_CONFIG_INCLUDES) $(PLATFORM_INTRINSICS_AVX2_FLAGS)
crypto_libgulden_crypto_avx2_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS) $(PLATFORM_INTRINSICS_AVX2_FLAGS)
crypto_libgulden_crypto_avx2_a_SOURCES = \
  crypto/hash/sigma/echo256/opt/echo256_opt_avx2.h \
  crypto/hash/sigma/echo256/opt/echo256_opt_avx2.cpp \
  crypto/hash/sigma/shavite3_256/opt/shavite3_256_opt_avx2.h \
  crypto/hash/sigma/shavite3_256/opt/shavite3_256_opt_avx2.cpp \
  crypto/hash/sigma/argon_echo/opt/core_opt_avx2.h \
  crypto/hash/sigma/argon_echo/opt/core_opt_avx2.cpp

crypto_libgulden_crypto_avx2_aes_a_CPPFLAGS = $(AM_CPPFLAGS) $(GULDEN_CONFIG_INCLUDES) $(PLATFORM_INTRINSICS_AVX2_FLAGS) $(PLATFORM_INTRINSICS_AES_FLAGS)
crypto_libgulden_crypto_avx2_aes_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS) $(PLATFORM_INTRINSICS_AVX2_FLAGS) $(PLATFORM_INTRINSICS_AES_FLAGS)
crypto_libgulden_crypto_avx2_aes_a_SOURCES = \
  crypto/hash/sigma/echo256/opt/echo256_opt_avx2_aes.h \
  crypto/hash/sigma/echo256/opt/echo256_opt_avx2_aes.cpp \
  crypto/hash/sigma/shavite3_256/opt/shavite3_256_opt_avx2_aes.h \
  crypto/hash/sigma/shavite3_256/opt/shavite3_256_opt_avx2_aes.cpp \
  crypto/hash/sigma/argon_echo/opt/core_opt_avx2_aes.h \
  crypto/hash/sigma/argon_echo/opt/core_opt_avx2_aes.cpp

crypto_libgulden_crypto_avx512f_a_CPPFLAGS = $(AM_CPPFLAGS) $(GULDEN_CONFIG_INCLUDES) $(PLATFORM_INTRINSICS_AVX512F_FLAGS)
crypto_libgulden_crypto_avx512f_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS) $(PLATFORM_INTRINSICS_AVX512F_FLAGS)
crypto_libgulden_crypto_avx512f_a_SOURCES = \
  crypto/hash/sigma/echo256/opt/echo256_opt_avx512f.h \
  crypto/hash/sigma/echo256/opt/echo256_opt_avx512f.cpp \
  crypto/hash/sigma/shavite3_256/opt/shavite3_256_opt_avx512f.h \
  crypto/hash/sigma/shavite3_256/opt/shavite3_256_opt_avx512f.cpp \
  crypto/hash/sigma/argon_echo/opt/core_opt_avx512f.h \
  crypto/hash/sigma/argon_echo/opt/core_opt_avx512f.cpp

crypto_libgulden_crypto_avx512f_aes_a_CPPFLAGS = $(AM_CPPFLAGS) $(GULDEN_CONFIG_INCLUDES) $(PLATFORM_INTRINSICS_AVX512F_FLAGS) $(PLATFORM_INTRINSICS_AES_FLAGS)
crypto_libgulden_crypto_avx512f_aes_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS) $(PLATFORM_INTRINSICS_AVX512F_FLAGS) $(PLATFORM_INTRINSICS_AES_FLAGS)
crypto_libgulden_crypto_avx512f_aes_a_SOURCES = \
  crypto/hash/sigma/echo256/opt/echo256_opt_avx512f_aes.h \
  crypto/hash/sigma/echo256/opt/echo256_opt_avx512f_aes.cpp \
  crypto/hash/sigma/shavite3_256/opt/shavite3_256_opt_avx512f_aes.h \
  crypto/hash/sigma/shavite3_256/opt/shavite3_256_opt_avx512f_aes.cpp \
  crypto/hash/sigma/argon_echo/opt/core_opt_avx512f_aes.h \
  crypto/hash/sigma/argon_echo/opt/core_opt_avx512f_aes.cpp

crypto_libgulden_crypto_arm_cortex_a53_a_CPPFLAGS = $(AM_CPPFLAGS) $(GULDEN_CONFIG_INCLUDES) $(PLATFORM_INTRINSICS_CORTEX53_FLAGS)
crypto_libgulden_crypto_arm_cortex_a53_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS) $(PLATFORM_INTRINSICS_CORTEX53_FLAGS)
crypto_libgulden_crypto_arm_cortex_a53_a_SOURCES = \
  crypto/hash/sigma/echo256/opt/echo256_opt_arm_cortex_a53.h \
  crypto/hash/sigma/echo256/opt/echo256_opt_arm_cortex_a53.cpp \
  crypto/hash/sigma/shavite3_256/opt/shavite3_256_opt_arm_cortex_a53.h \
  crypto/hash/sigma/shavite3_256/opt/shavite3_256_opt_arm_cortex_a53.cpp \
  crypto/hash/sigma/argon_echo/opt/core_opt_arm_cortex_a53.h \
  crypto/hash/sigma/argon_echo/opt/core_opt_arm_cortex_a53.cpp

crypto_libgulden_crypto_arm_cortex_a53_aes_a_CPPFLAGS = $(AM_CPPFLAGS) $(GULDEN_CONFIG_INCLUDES) $(PLATFORM_INTRINSICS_CORTEX53_AES_FLAGS)
crypto_libgulden_crypto_arm_cortex_a53_aes_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS) $(PLATFORM_INTRINSICS_CORTEX53_AES_FLAGS)
crypto_libgulden_crypto_arm_cortex_a53_aes_a_SOURCES = \
  crypto/hash/sigma/echo256/opt/echo256_opt_arm_cortex_a53_aes.h \
  crypto/hash/sigma/echo256/opt/echo256_opt_arm_cortex_a53_aes.cpp \
  crypto/hash/sigma/shavite3_256/opt/shavite3_256_opt_arm_cortex_a53_aes.h \
  crypto/hash/sigma/shavite3_256/opt/shavite3_256_opt_arm_cortex_a53_aes.cpp \
  crypto/hash/sigma/argon_echo/opt/core_opt_arm_cortex_a53_aes.h \
  crypto/hash/sigma/argon_echo/opt/core_opt_arm_cortex_a53_aes.cpp
  
crypto_libgulden_crypto_arm_cortex_a57_a_CPPFLAGS = $(AM_CPPFLAGS) $(GULDEN_CONFIG_INCLUDES) $(PLATFORM_INTRINSICS_CORTEX57_FLAGS)
crypto_libgulden_crypto_arm_cortex_a57_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS) $(PLATFORM_INTRINSICS_CORTEX57_FLAGS)
crypto_libgulden_crypto_arm_cortex_a57_a_SOURCES = \
  crypto/hash/sigma/echo256/opt/echo256_opt_arm_cortex_a57.h \
  crypto/hash/sigma/echo256/opt/echo256_opt_arm_cortex_a57.cpp \
  crypto/hash/sigma/shavite3_256/opt/shavite3_256_opt_arm_cortex_a57.h \
  crypto/hash/sigma/shavite3_256/opt/shavite3_256_opt_arm_cortex_a57.cpp \
  crypto/hash/sigma/argon_echo/opt/core_opt_arm_cortex_a57.h \
  crypto/hash/sigma/argon_echo/opt/core_opt_arm_cortex_a57.cpp

crypto_libgulden_crypto_arm_cortex_a57_aes_a_CPPFLAGS = $(AM_CPPFLAGS) $(GULDEN_CONFIG_INCLUDES) $(PLATFORM_INTRINSICS_CORTEX57_AES_FLAGS)
crypto_libgulden_crypto_arm_cortex_a57_aes_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS) $(PLATFORM_INTRINSICS_CORTEX57_AES_FLAGS)
crypto_libgulden_crypto_arm_cortex_a57_aes_a_SOURCES = \
  crypto/hash/sigma/echo256/opt/echo256_opt_arm_cortex_a57_aes.h \
  crypto/hash/sigma/echo256/opt/echo256_opt_arm_cortex_a57_aes.cpp \
  crypto/hash/sigma/shavite3_256/opt/shavite3_256_opt_arm_cortex_a57_aes.h \
  crypto/hash/sigma/shavite3_256/opt/shavite3_256_opt_arm_cortex_a57_aes.cpp \
  crypto/hash/sigma/argon_echo/opt/core_opt_arm_cortex_a57_aes.h \
  crypto/hash/sigma/argon_echo/opt/core_opt_arm_cortex_a57_aes.cpp

crypto_libgulden_crypto_arm_cortex_a72_a_CPPFLAGS = $(AM_CPPFLAGS) $(GULDEN_CONFIG_INCLUDES) $(PLATFORM_INTRINSICS_CORTEX72_FLAGS)
crypto_libgulden_crypto_arm_cortex_a72_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS) $(PLATFORM_INTRINSICS_CORTEX72_FLAGS)
crypto_libgulden_crypto_arm_cortex_a72_a_SOURCES = \
  crypto/hash/sigma/echo256/opt/echo256_opt_arm_cortex_a72.h \
  crypto/hash/sigma/echo256/opt/echo256_opt_arm_cortex_a72.cpp \
  crypto/hash/sigma/shavite3_256/opt/shavite3_256_opt_arm_cortex_a72.h \
  crypto/hash/sigma/shavite3_256/opt/shavite3_256_opt_arm_cortex_a72.cpp \
  crypto/hash/sigma/argon_echo/opt/core_opt_arm_cortex_a72.h \
  crypto/hash/sigma/argon_echo/opt/core_opt_arm_cortex_a72.cpp

crypto_libgulden_crypto_arm_cortex_a72_aes_a_CPPFLAGS = $(AM_CPPFLAGS) $(GULDEN_CONFIG_INCLUDES) $(PLATFORM_INTRINSICS_CORTEX72_AES_FLAGS)
crypto_libgulden_crypto_arm_cortex_a72_aes_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS) $(PLATFORM_INTRINSICS_CORTEX72_AES_FLAGS)
crypto_libgulden_crypto_arm_cortex_a72_aes_a_SOURCES = \
  crypto/hash/sigma/echo256/opt/echo256_opt_arm_cortex_a72_aes.h \
  crypto/hash/sigma/echo256/opt/echo256_opt_arm_cortex_a72_aes.cpp \
  crypto/hash/sigma/shavite3_256/opt/shavite3_256_opt_arm_cortex_a72_aes.h \
  crypto/hash/sigma/shavite3_256/opt/shavite3_256_opt_arm_cortex_a72_aes.cpp \
  crypto/hash/sigma/argon_echo/opt/core_opt_arm_cortex_a72_aes.h \
  crypto/hash/sigma/argon_echo/opt/core_opt_arm_cortex_a72_aes.cpp

crypto_libgulden_crypto_arm_thunderx_aes_a_CPPFLAGS = $(AM_CPPFLAGS) $(GULDEN_CONFIG_INCLUDES) $(PLATFORM_INTRINSICS_THUNDERX_AES_FLAGS)
crypto_libgulden_crypto_arm_thunderx_aes_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS) $(PLATFORM_INTRINSICS_THUNDERX_AES_FLAGS)
crypto_libgulden_crypto_arm_thunderx_aes_a_SOURCES = \
  crypto/hash/sigma/echo256/opt/echo256_opt_arm_thunderx_aes.h \
  crypto/hash/sigma/echo256/opt/echo256_opt_arm_thunderx_aes.cpp \
  crypto/hash/sigma/shavite3_256/opt/shavite3_256_opt_arm_thunderx_aes.h \
  crypto/hash/sigma/shavite3_256/opt/shavite3_256_opt_arm_thunderx_aes.cpp \
  crypto/hash/sigma/argon_echo/opt/core_opt_arm_thunderx_aes.h \
  crypto/hash/sigma/argon_echo/opt/core_opt_arm_thunderx_aes.cpp

crypto_libgulden_crypto_a_CPPFLAGS = $(AM_CPPFLAGS) $(GULDEN_CONFIG_INCLUDES)
crypto_libgulden_crypto_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
crypto_libgulden_crypto_a_SOURCES = \
  crypto/chacha20.h \
  crypto/chacha20.cpp \
  crypto/hash/sigma/echo256/sphlib/echo.cpp \
  crypto/hash/sigma/echo256/sphlib/sph_types.h \
  crypto/hash/sigma/echo256/sphlib/sph_echo.h \
  crypto/hash/sigma/echo256/sphlib/aes_helper.c \
  crypto/hash/sigma/shavite3_256/shavite3_256_opt.cpp \
  crypto/hash/sigma/shavite3_256/shavite3_256_opt.h \
  crypto/hash/sigma/shavite3_256/ref/AESround.h \
  crypto/hash/sigma/shavite3_256/ref/portable.h \
  crypto/hash/sigma/shavite3_256/ref/shavite3_256_ref_compress.h \
  crypto/hash/sigma/shavite3_256/ref/shavite3_ref.h \
  crypto/hash/sigma/shavite3_256/ref/shavite3_ref.cpp \
  crypto/hash/sigma/argon_echo/argon2.cpp \
  crypto/hash/sigma/argon_echo/core.cpp \
  crypto/hash/sigma/argon_echo/core.h \
  crypto/hash/sigma/argon_echo/argon_echo.h \
  crypto/hash/sigma/argon_echo/ref.cpp \
  crypto/hash/sigma/argon_echo/opt/core_opt_hybrid.cpp \
  crypto/hash/sigma/argon_echo/opt/core_opt_hybrid.h \
  crypto/hash/sigma/argon_echo/blake2/blake2.h \
  crypto/hash/sigma/argon_echo/blake2/blake2b.cpp \
  crypto/hash/sigma/argon_echo/blake2/blake2-impl.h \
  crypto/hash/sigma/argon_echo/blake2/blamka-round-ref.h \
  crypto/hash/sigma/argon_echo/blake2/blamka-round-opt_sse2.h \
  crypto/hash/sigma/argon_echo/blake2/blamka-round-opt_sse3.h \
  crypto/hash/sigma/argon_echo/blake2/blamka-round-opt_avx2.h \
  crypto/hash/sigma/argon_echo/blake2/blamka-round-opt_avx512f.h \
  crypto/hash/sigma/sigma.h \
  crypto/hash/sigma/sigma.cpp \
  crypto/common.h \
  crypto/hmac_sha256.cpp \
  crypto/hmac_sha256.h \
  crypto/hmac_sha512.cpp \
  crypto/hmac_sha512.h \
  crypto/ripemd160.cpp \
  crypto/ripemd160.h \
  crypto/sha1.cpp \
  crypto/sha1.h \
  crypto/sha256.cpp \
  crypto/sha256.h \
  crypto/sha512.cpp \
  crypto/sha512.h

crypto_libgulden_crypto_a_DIST = \
  crypto/hash/sigma/echo256/echo256_opt.h \
  crypto/hash/sigma/echo256/echo256_opt.cpp

# consensus: shared between all executables that validate any consensus rules.
libgulden_consensus_a_CPPFLAGS = $(AM_CPPFLAGS) $(GULDEN_INCLUDES)
libgulden_consensus_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
libgulden_consensus_a_SOURCES = \
  Gulden/Common/scrypt.cpp \
  Gulden/Common/diff_delta.cpp \
  Gulden/Common/diff_old.cpp \
  Gulden/Common/diff_common.cpp \
  Gulden/Common/hash/city.cpp \
  amount.h \
  arith_uint256.cpp \
  arith_uint256.h \
  consensus/merkle.cpp \
  consensus/merkle.h \
  consensus/params.h \
  consensus/validation.h \
  hash.cpp \
  hash.h \
  prevector.h \
  primitives/block.cpp \
  primitives/block.h \
  primitives/transaction.cpp \
  primitives/transaction.h \
  pubkey.cpp \
  pubkey.h \
  script/guldenconsensus.cpp \
  script/guldenconsensus.h \
  script/interpreter.cpp \
  script/interpreter.h \
  script/script.cpp \
  script/script.h \
  script/script_error.cpp \
  script/script_error.h \
  serialize.h \
  tinyformat.h \
  uint256.cpp \
  uint256.h \
  utilstrencodings.cpp \
  utilstrencodings.h \
  version.h

# common: shared between GuldenD, and Gulden and non-server tools
libgulden_common_a_CPPFLAGS = $(AM_CPPFLAGS) $(GULDEN_INCLUDES)
libgulden_common_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
libgulden_common_a_SOURCES = \
  unity/appmanager.cpp \
  base58.cpp \
  chainparams.cpp \
  coins.cpp \
  compressor.cpp \
  core_read.cpp \
  core_write.cpp \
  key.cpp \
  keystore.cpp \
  netaddress.cpp \
  netbase.cpp \
  policy/feerate.cpp \
  protocol.cpp \
  scheduler.cpp \
  script/sign.cpp \
  script/standard.cpp \
  $(GULDEN_CORE_H)

# util: shared between all executables.
# This library *must* be included to make sure that the glibc
# backward-compatibility objects and their sanity checks are linked.
libgulden_util_a_CPPFLAGS = $(AM_CPPFLAGS) $(GULDEN_INCLUDES)
libgulden_util_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
libgulden_util_a_SOURCES = \
  support/lockedpool.cpp \
  chainparamsbase.cpp \
  clientversion.cpp \
  compat/glibc_sanity.cpp \
  compat/glibcxx_sanity.cpp \
  compat/strnlen.cpp \
  fs.cpp \
  random.cpp \
  rpc/protocol.cpp \
  support/cleanse.cpp \
  sync.cpp \
  threadinterrupt.cpp \
  util.cpp \
  utilmoneystr.cpp \
  utilstrencodings.cpp \
  utiltime.cpp \
  $(GULDEN_CORE_H)

if GLIBC_BACK_COMPAT
libgulden_util_a_SOURCES += compat/glibc_compat.cpp
AM_LDFLAGS += $(COMPAT_LDFLAGS)
endif

# cli: shared between Gulden-cli and Gulden
libgulden_cli_a_CPPFLAGS = $(AM_CPPFLAGS) $(GULDEN_INCLUDES)
libgulden_cli_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
libgulden_cli_a_SOURCES = \
  rpc/client.cpp \
  $(GULDEN_CORE_H)

nodist_libgulden_util_a_SOURCES = $(srcdir)/obj/build.h
#

# GuldenD binary #
GuldenD_SOURCES = GuldenD.cpp
GuldenD_CPPFLAGS = $(AM_CPPFLAGS) $(GULDEN_INCLUDES)
GuldenD_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
GuldenD_LDFLAGS = $(RELDFLAGS) $(AM_LDFLAGS) $(LIBTOOL_APP_LDFLAGS)

if TARGET_WINDOWS
GuldenD_SOURCES += GuldenD-res.rc
endif

GuldenD_LDADD = \
  $(LIBGULDEN_SERVER) \
  $(LIBGULDEN_WALLET) \
  $(LIBGULDEN_COMMON) \
  $(LIBUNIVALUE) \
  $(LIBGULDEN_UTIL) \
  $(LIBGULDEN_ZMQ) \
  $(LIBGULDEN_CONSENSUS) \
  $(LIBGULDEN_CRYPTO_ALL) \
  $(LIBLEVELDB) \
  $(LIBMEMENV) \
  $(LIBSECP256K1)

GuldenD_LDADD += $(BOOST_LIBS) $(BDB_LIBS) $(CRYPTOPP_LIBS) $(CRYPTO_LIBS) $(MINIUPNPC_LIBS) $(EVENT_PTHREADS_LIBS) $(EVENT_LIBS) $(ZMQ_LIBS) $(SSL_LIBS)

# Gulden-cli binary #
Gulden_cli_SOURCES = Gulden-cli.cpp
Gulden_cli_CPPFLAGS = $(AM_CPPFLAGS) $(GULDEN_INCLUDES) $(EVENT_CFLAGS)
Gulden_cli_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
Gulden_cli_LDFLAGS = $(RELDFLAGS) $(AM_LDFLAGS) $(LIBTOOL_APP_LDFLAGS)

if TARGET_WINDOWS
Gulden_cli_SOURCES += Gulden-cli-res.rc
endif

Gulden_cli_LDADD = \
  $(LIBGULDEN_CLI) \
  $(LIBUNIVALUE) \
  $(LIBGULDEN_UTIL) \
  $(LIBGULDEN_CRYPTO_ALL)

Gulden_cli_LDADD += $(BOOST_LIBS) $(SSL_LIBS) $(CRYPTOPP_LIBS) $(CRYPTO_LIBS) $(EVENT_LIBS)
#

# Gulden-tx binary #
Gulden_tx_SOURCES = Gulden-tx.cpp
Gulden_tx_CPPFLAGS = $(AM_CPPFLAGS) $(GULDEN_INCLUDES)
Gulden_tx_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
Gulden_tx_LDFLAGS = $(RELDFLAGS) $(AM_LDFLAGS) $(LIBTOOL_APP_LDFLAGS)

if TARGET_WINDOWS
Gulden_tx_SOURCES += Gulden-tx-res.rc
endif

Gulden_tx_LDADD = \
  $(LIBUNIVALUE) \
  $(LIBGULDEN_COMMON) \
  $(LIBGULDEN_UTIL) \
  $(LIBGULDEN_CONSENSUS) \
  $(LIBGULDEN_CRYPTO_ALL) \
  $(LIBSECP256K1)

Gulden_tx_LDADD += $(BOOST_LIBS) $(CRYPTOPP_LIBS) $(CRYPTO_LIBS)
#

# guldenconsensus library #
if BUILD_GULDEN_LIBS
include_HEADERS = script/guldenconsensus.h
libguldenconsensus_la_SOURCES = $(crypto_libgulden_crypto_a_SOURCES) $(libgulden_consensus_a_SOURCES)

if GLIBC_BACK_COMPAT
  libguldenconsensus_la_SOURCES += compat/glibc_compat.cpp
endif

libguldenconsensus_la_LDFLAGS = $(AM_LDFLAGS)  $(RELDFLAGS)
libguldenconsensus_la_LIBADD = $(LIBSECP256K1) $(SSL_LIBS) $(BOOST_LIBS)
libguldenconsensus_la_CPPFLAGS = $(AM_CPPFLAGS) -I$(builddir)/obj -I$(srcdir)/secp256k1/include -DBUILD_GULDEN_INTERNAL
libguldenconsensus_la_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)

endif
#

CLEANFILES = $(EXTRA_LIBRARIES)

CLEANFILES += *.gcda *.gcno
CLEANFILES += compat/*.gcda compat/*.gcno
CLEANFILES += consensus/*.gcda consensus/*.gcno
CLEANFILES += crypto/*.gcda crypto/*.gcno
CLEANFILES += policy/*.gcda policy/*.gcno
CLEANFILES += primitives/*.gcda primitives/*.gcno
CLEANFILES += script/*.gcda script/*.gcno
CLEANFILES += support/*.gcda support/*.gcno
CLEANFILES += univalue/*.gcda univalue/*.gcno
CLEANFILES += wallet/*.gcda wallet/*.gcno
CLEANFILES += wallet/test/*.gcda wallet/test/*.gcno
CLEANFILES += zmq/*.gcda zmq/*.gcno

DISTCLEANFILES = obj/build.h


config/gulden-config.h: config/stamp-h1
	@$(MAKE) -C $(top_builddir) $(subdir)/$(@)
config/stamp-h1: $(top_srcdir)/$(subdir)/config/gulden-config.h.in $(top_builddir)/config.status
	$(AM_V_at)$(MAKE) -C $(top_builddir) $(subdir)/$(@)
$(top_srcdir)/$(subdir)/config/gulden-config.h.in:  $(am__configure_deps)
	$(AM_V_at)$(MAKE) -C $(top_srcdir) $(subdir)/config/gulden-config.h.in

clean-local:
	-$(MAKE) -C secp256k1 clean
	-$(MAKE) -C univalue clean
	-rm -f leveldb/*/*.gcda leveldb/*/*.gcno leveldb/helpers/memenv/*.gcda leveldb/helpers/memenv/*.gcno
	-rm -f config.h
	-rm -rf test/__pycache__

.rc.o:
	@test -f $(WINDRES)
	## FIXME: How to get the appropriate modulename_CPPFLAGS in here?
	$(AM_V_GEN) $(WINDRES) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(CPPFLAGS) -DWINDRES_PREPROC -i $< -o $@

.mm.o:
	$(AM_V_CXX) $(OBJCXX) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(AM_CPPFLAGS) \
	  $(CPPFLAGS) $(AM_CXXFLAGS) $(QT_INCLUDES) $(AM_CXXFLAGS) $(PIE_FLAGS) $(CXXFLAGS) -c -o $@ $<

%.pb.cc %.pb.h: %.proto
	@test -f $(PROTOC)
	$(AM_V_GEN) $(PROTOC) --cpp_out=$(@D) --proto_path=$(<D) $<

if EMBEDDED_LEVELDB
include Makefile.leveldb.include
endif

if ENABLE_TESTS
include Makefile.test.include
endif

if ENABLE_BENCH
#include Makefile.bench.include
endif

if ENABLE_BENCH
include Makefile.sigmabench.include
endif

if ENABLE_QT
include Makefile.qt.include
endif

if ENABLE_QT_TESTS
include Makefile.qttest.include
endif
