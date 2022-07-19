# Copyright (c) 2013-2016 The Bitcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
#
# File contains modifications by: The Gulden developers
# All modifications:
# Copyright (c) 2016-2018 The Gulden developers
# Authored by: Malcolm MacLeod (mmacleod@gmx.com)
# Distributed under the GULDEN software license, see the accompanying
# file COPYING

DIST_SUBDIRS = secp256k1

AM_LDFLAGS_NO_THREADS = $(LIBTOOL_LDFLAGS) $(HARDENED_LDFLAGS)
AM_LDFLAGS = $(PTHREAD_CFLAGS) $(AM_LDFLAGS_NO_THREADS)
AM_CXXFLAGS = $(HARDENED_CXXFLAGS) $(ERROR_CXXFLAGS)
AM_CPPFLAGS = $(HARDENED_CPPFLAGS)
EXTRA_LIBRARIES =

CONFIG_INCLUDES=-I$(builddir)/config
COMMON_INCLUDES=-I$(builddir) -I$(builddir)/obj $(BDB_CPPFLAGS) $(BOOST_CPPFLAGS) $(LEVELDB_CPPFLAGS) $(CRYPTO_CFLAGS) $(SSL_CFLAGS) $(CRYPTOPP_CFLAGS) $(LEVELDB_CPPFLAGS)

COMMON_INCLUDES += -I$(srcdir)/secp256k1/include
COMMON_INCLUDES += -I$(srcdir)/$(UNIVALUE_INCLUDE_DIR_INT)

LIB_SERVER=lib_server.a
LIB_RPC=lib_rpc.a
LIB_NODE=lib_node.a
LIB_COMMON=lib_common.a
LIB_CONSENSUS=lib_consensus.a
LIB_CLI=lib_cli.a
LIB_UTIL=lib_util.a
LIB_GENERIC=lib_generic.a
LIB_GENERIC_INIT=lib_generic_init.a
LIB_GENERIC_INIT_NODE=lib_generic_init_node.a
LIB_CRYPTO_SSE3=crypto/lib_crypto_sse3.a
LIB_CRYPTO_SSE3_AES=crypto/lib_crypto_sse3_aes.a
LIB_CRYPTO_SSE4=crypto/lib_crypto_sse4.a
LIB_CRYPTO_SSE4_SHANI=crypto/lib_crypto_sse4_shani.a
LIB_CRYPTO_SSE4_AES=crypto/lib_crypto_sse4_aes.a
LIB_CRYPTO_AVX=crypto/lib_crypto_avx.a
LIB_CRYPTO_AVX_AES=crypto/lib_crypto_avx_aes.a
LIB_CRYPTO_AVX2=crypto/lib_crypto_avx2.a
LIB_CRYPTO_AVX2_AES=crypto/lib_crypto_avx2_aes.a
LIB_CRYPTO_AVX512F=crypto/lib_crypto_avx512f.a
LIB_CRYPTO_AVX512F_AES=crypto/lib_crypto_avx512f_aes.a
LIB_CRYPTO_ARM_CORTEX_A53=crypto/lib_crypto_arm_cortex_a53.a
LIB_CRYPTO_ARM_CORTEX_A53_AES=crypto/lib_crypto_arm_cortex_a53_aes.a
LIB_CRYPTO_ARM_CORTEX_A57=crypto/lib_crypto_arm_cortex_a57.a
LIB_CRYPTO_ARM_CORTEX_A57_AES=crypto/lib_crypto_arm_cortex_a57_aes.a
LIB_CRYPTO_ARM_CORTEX_A72=crypto/lib_crypto_arm_cortex_a72.a
LIB_CRYPTO_ARM_CORTEX_A72_AES=crypto/lib_crypto_arm_cortex_a72_aes.a
LIB_CRYPTO_ARM_THUNDERX_AES=crypto/lib_crypto_arm_thunderx_aes.a
LIB_CRYPTO_ARM_V8_CRYPTO=crypto/lib_crypto_arm_v8_crypto.a
LIB_CRYPTO=crypto/lib_crypto.a
LIB_ANDROID_COMPAT=lib_android_compat.a
LIBGULDENQT=qt/libguldenqt.a
LIBSECP256K1=secp256k1/libsecp256k1.la

if ENABLE_ZMQ
LIB_ZMQ=lib_zmq.a
endif
if BUILD_LIBS_NODE_JS
LIB_UNITY_NODE_JS=lib_unity_node_js.la
endif
if BUILD_LIBS_JNI
LIB_UNITY_JNI=lib_unity_jni.la
endif
if BUILD_LIBS_OBJC
LIB_UNITY_OBJC=lib_unity_objc.a
endif
if ENABLE_WALLET
LIB_WALLET=lib_wallet.a
endif

$(LIBSECP256K1): $(wildcard secp256k1/src/*) $(wildcard secp256k1/include/*)
	$(AM_V_at)$(MAKE) $(AM_MAKEFLAGS) -C $(@D) $(@F)

LIB_CRYPTO_ARM = $(LIB_CRYPTO_ARM_CORTEX_A53) $(LIB_CRYPTO_ARM_CORTEX_A53_AES) $(LIB_CRYPTO_ARM_CORTEX_A57) $(LIB_CRYPTO_ARM_CORTEX_A57_AES) $(LIB_CRYPTO_ARM_CORTEX_A72) $(LIB_CRYPTO_ARM_CORTEX_A72_AES) $(LIB_CRYPTO_ARM_THUNDERX_AES) $(LIB_CRYPTO_ARM_V8_CRYPTO)
LIB_CRYPTO_INTEL = $(LIB_CRYPTO_SSE3) $(LIB_CRYPTO_SSE3_AES) $(LIB_CRYPTO_SSE4) $(LIB_CRYPTO_SSE4_SHANI) $(LIB_CRYPTO_SSE4_AES) $(LIB_CRYPTO_AVX) $(LIB_CRYPTO_AVX_AES) $(LIB_CRYPTO_AVX2) $(LIB_CRYPTO_AVX2_AES) $(LIB_CRYPTO_AVX512F) $(LIB_CRYPTO_AVX512F_AES)
LIB_CRYPTO_ALL = $(LIB_CRYPTO) $(LIB_CRYPTO_INTEL) $(LIB_CRYPTO_ARM)

# Make is not made aware of per-object dependencies to avoid limiting building parallelization
# But to build the less dependent modules first, we manually select their order here:
EXTRA_LIBRARIES += \
  $(LIB_CRYPTO_ALL) \
  $(LIB_UTIL) \
  $(LIB_GENERIC) \
  $(LIB_GENERIC_INIT) \
  $(LIB_GENERIC_INIT_NODE) \
  $(LIB_COMMON) \
  $(LIB_CONSENSUS) \
  $(LIB_SERVER) \
  $(LIB_RPC) \
  $(LIB_NODE) \
  $(LIB_CLI) \
  $(LIB_WALLET) \
  $(LIB_ZMQ)

lib_LTLIBRARIES = $(LIB_UNITY_JNI) $(LIB_UNITY_NODE_JS)
noinst_LTLIBRARIES =
noinst_LIBRARIES =

bin_PROGRAMS =
noinst_PROGRAMS =
TESTS =
BENCHMARKS =

if BUILD_DAEMON
  bin_PROGRAMS += GuldenD
endif

if BUILD_UTILS
  bin_PROGRAMS += Gulden-cli Gulden-tx
endif

.PHONY: FORCE check-symbols check-security

# core #
CORE_H = rpc/accounts.h \
  pow/diff.h \
  pow/diff_delta.h \
  pow/diff_old.h \
  pow/diff_common.h \
  crypto/hash/hash.h \
  crypto/hash/city.h \
  crypto/hash/cityconfig.h \
  wallet/mnemonic.h \
  witnessutil.h \
  wallet/extwallet.h \
  LRUCache/LRUCache11.hpp \
  alert.h \
  wallet/account.h \
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
  appname.h \
  coins.h \
  compat.h \
  compat/arch.h \
  compat/cpuid.h \
  compat/sys.h \
  compat/sse.h \
  compat/sse2neon.h \
  compat/byteswap.h \
  compat/endian.h \
  compat/sanity.h \
  compat/assumptions.h \
  compressor.h \
  blockfilter.h \
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
  node/context.h \
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
  generation/witnessrewardtemplate.h \
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
  pow/pow.h \
  protocol.h \
  random.h \
  randomenv.h \
  reverselock.h \
  reverse_iterator.h \
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
  span.h \
  streams.h \
  support/allocators/secure.h \
  support/allocators/zeroafterfree.h \
  support/cleanse.h \
  support/events.h \
  support/lockedpool.h \
  sync.h \
  util/thread.h \
  threadsafety.h \
  threadinterrupt.h \
  timedata.h \
  torcontrol.h \
  txdb.h \
  txmempool.h \
  ui_interface.h \
  undo.h \
  logging.h \
  util.h \
  util/time.h \
  util/check.h \
  util/macros.h \
  util/overloaded.h \
  util/syscall_sandbox.h \
  util/threadnames.h \
  util/getuniquepath.h \
  attributes.h \
  util/moneystr.h \
  util/strencodings.h \
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
  wallet/spvscanner.h \
  wallet/wallet.h \
  wallet/merkletx.h \
  wallet/wallettx.h \
  wallet/walletdb.h \
  wallet/walletdberrors.h \
  wallet/witness_operations.h \
  warnings.h \
  zmq/zmqabstractnotifier.h \
  zmq/zmqconfig.h\
  zmq/zmqnotificationinterface.h \
  zmq/zmqpublishnotifier.h


obj/build.h: FORCE
	@$(MKDIR_P) $(builddir)/obj
	@$(top_srcdir)/share/genbuild.sh "$(abs_top_builddir)/src/obj/build.h" \
	  "$(abs_top_srcdir)"

clientversion.cpp: obj/build.h

# RPC: Daemon and GUI/libs
lib_rpc_a_CPPFLAGS = $(AM_CPPFLAGS) $(COMMON_INCLUDES) $(MINIUPNPC_CPPFLAGS) $(EVENT_CFLAGS) $(EVENT_PTHREADS_CFLAGS)
lib_rpc_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
lib_rpc_a_SOURCES = \
  rpc/accounts.cpp \
  rpc/blockchain.cpp \
  rpc/mining.cpp \
  rpc/misc.cpp \
  rpc/netrpc.cpp \
  rpc/rawtransaction.cpp \
  rpc/protocol.cpp \
  rpc/server.cpp \
  rpc/client.cpp \
  $(CORE_H)

if ENABLE_WALLET
lib_rpc_a_SOURCES += \
  wallet/rpcdump.cpp \
  wallet/rpcwallet.cpp
endif

# server: shared between Daemon and GUI client
lib_server_a_CPPFLAGS = $(AM_CPPFLAGS) $(COMMON_INCLUDES) $(MINIUPNPC_CPPFLAGS) $(EVENT_CFLAGS) $(EVENT_PTHREADS_CFLAGS)
lib_server_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
lib_server_a_SOURCES = \
  httprpc.cpp \
  httpserver.cpp \
  rest.cpp \
  torcontrol.cpp \
  $(CORE_H)

# node: shared between Daemon, GUI client and unity libraries.
lib_node_a_CPPFLAGS = $(AM_CPPFLAGS) $(COMMON_INCLUDES) $(MINIUPNPC_CPPFLAGS) $(EVENT_CFLAGS) $(EVENT_PTHREADS_CFLAGS)
lib_node_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
lib_node_a_SOURCES = \
  addrdb.cpp \
  addrman.cpp \
  alert.cpp \
  bloom.cpp \
  blockencodings.cpp \
  blockstore.cpp \
  chain.cpp \
  checkpoints.cpp \
  consensus/tx_verify.cpp \
  init.cpp \
  dbwrapper.cpp \
  merkleblock.cpp \
  generation/miner.cpp \
  generation/witness.cpp \
  generation/witnessrewardtemplate.cpp \
  net.cpp \
  net_processing.cpp \
  noui.cpp \
  policy/feerate.cpp \
  policy/fees.cpp \
  policy/policy.cpp \
  policy/rbf.cpp \
  txdb.cpp \
  txmempool.cpp \
  witnessutil.cpp \
  ui_interface.cpp \
  validation/validation.cpp \
  validation/validation_mempool.cpp \
  validation/validation_misc.cpp \
  validation/witnessvalidation.cpp \
  validation/versionbitsvalidation.cpp \
  validation/validationinterface.cpp \
  versionbits.cpp \
  warnings.cpp \
  script/sigcache.cpp \
  script/ismine.cpp \
  timedata.cpp \
  pow/pow.cpp \
  $(CORE_H)

# zmq: shared between Daemon and GUI client, only if zmq enabled
if ENABLE_ZMQ
lib_zmq_a_CPPFLAGS = $(COMMON_INCLUDES) $(ZMQ_CFLAGS)
lib_zmq_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
lib_zmq_a_SOURCES = \
  zmq/zmqabstractnotifier.cpp \
  zmq/zmqnotificationinterface.cpp \
  zmq/zmqpublishnotifier.cpp
endif


# wallet: shared between Daemon, GUI client and unity libraries but only linked when wallet enabled
lib_wallet_a_CPPFLAGS = $(AM_CPPFLAGS) $(COMMON_INCLUDES)
lib_wallet_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
lib_wallet_a_SOURCES = \
  wallet/account.cpp \
  wallet/mnemonic.cpp \
  wallet/extwallet.cpp \
  wallet/crypter.cpp \
  wallet/db.cpp \
  wallet/feebumper.cpp \
  wallet/spvscanner.cpp \
  wallet/wallet.cpp \
  wallet/wallet_ismine.cpp \
  wallet/wallet_init.cpp \
  wallet/wallet_transaction.cpp \
  wallet/wallet_keypool.cpp \
  wallet/walletbalance.cpp \
  wallet/merkletx.cpp \
  wallet/wallettx.cpp \
  wallet/walletdb.cpp \
  wallet/witness_operations.cpp \
  $(CORE_H)

# crypto primitives library: Shared between all binaries and unity libraries
crypto_lib_crypto_sse3_a_CPPFLAGS = $(AM_CPPFLAGS) $(CONFIG_INCLUDES) $(PLATFORM_INTRINSICS_SSE3_FLAGS)
crypto_lib_crypto_sse3_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS) $(PLATFORM_INTRINSICS_SSE3_FLAGS)
crypto_lib_crypto_sse3_a_SOURCES = \
  crypto/hash/sigma/echo256/opt/echo256_opt_sse3.h \
  crypto/hash/sigma/echo256/opt/echo256_opt_sse3.cpp \
  crypto/hash/sigma/shavite3_256/opt/shavite3_256_opt_sse3.h \
  crypto/hash/sigma/shavite3_256/opt/shavite3_256_opt_sse3.cpp \
  crypto/hash/sigma/argon_echo/opt/core_opt_sse3.h \
  crypto/hash/sigma/argon_echo/opt/core_opt_sse3.cpp

crypto_lib_crypto_sse3_aes_a_CPPFLAGS = $(AM_CPPFLAGS) $(CONFIG_INCLUDES) $(PLATFORM_INTRINSICS_SSE3_FLAGS) $(PLATFORM_INTRINSICS_AES_FLAGS)
crypto_lib_crypto_sse3_aes_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS) $(PLATFORM_INTRINSICS_SSE3_FLAGS) $(PLATFORM_INTRINSICS_AES_FLAGS)
crypto_lib_crypto_sse3_aes_a_SOURCES = \
  crypto/hash/sigma/echo256/opt/echo256_opt_sse3_aes.h \
  crypto/hash/sigma/echo256/opt/echo256_opt_sse3_aes.cpp \
  crypto/hash/sigma/shavite3_256/opt/shavite3_256_opt_sse3_aes.h \
  crypto/hash/sigma/shavite3_256/opt/shavite3_256_opt_sse3_aes.cpp \
  crypto/hash/sigma/argon_echo/opt/core_opt_sse3_aes.h \
  crypto/hash/sigma/argon_echo/opt/core_opt_sse3_aes.cpp

crypto_lib_crypto_sse4_a_CPPFLAGS = $(AM_CPPFLAGS) $(CONFIG_INCLUDES) $(PLATFORM_INTRINSICS_SSE4_FLAGS)
crypto_lib_crypto_sse4_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS) $(PLATFORM_INTRINSICS_SSE4_FLAGS)
crypto_lib_crypto_sse4_a_SOURCES = \
  crypto/hash/sigma/echo256/opt/echo256_opt_sse4.h \
  crypto/hash/sigma/echo256/opt/echo256_opt_sse4.cpp \
  crypto/hash/sigma/shavite3_256/opt/shavite3_256_opt_sse4.h \
  crypto/hash/sigma/shavite3_256/opt/shavite3_256_opt_sse4.cpp \
  crypto/hash/sigma/argon_echo/opt/core_opt_sse4.h \
  crypto/hash/sigma/argon_echo/opt/core_opt_sse4.cpp \
  crypto/sha256_sse4.cpp

crypto_lib_crypto_sse4_shani_a_CPPFLAGS = $(AM_CPPFLAGS) $(CONFIG_INCLUDES) $(PLATFORM_INTRINSICS_SSE4_SHANI_FLAGS)
crypto_lib_crypto_sse4_shani_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS) $(PLATFORM_INTRINSICS_SSE4_SHANI_FLAGS)
crypto_lib_crypto_sse4_shani_a_SOURCES = \
  crypto/sha256_x86_shani.cpp

crypto_lib_crypto_sse4_aes_a_CPPFLAGS = $(AM_CPPFLAGS) $(CONFIG_INCLUDES) $(PLATFORM_INTRINSICS_SSE4_FLAGS) $(PLATFORM_INTRINSICS_AES_FLAGS)
crypto_lib_crypto_sse4_aes_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS) $(PLATFORM_INTRINSICS_SSE4_FLAGS) $(PLATFORM_INTRINSICS_AES_FLAGS)
crypto_lib_crypto_sse4_aes_a_SOURCES = \
  crypto/hash/sigma/echo256/opt/echo256_opt_sse4_aes.h \
  crypto/hash/sigma/echo256/opt/echo256_opt_sse4_aes.cpp \
  crypto/hash/sigma/shavite3_256/opt/shavite3_256_opt_sse4_aes.h \
  crypto/hash/sigma/shavite3_256/opt/shavite3_256_opt_sse4_aes.cpp \
  crypto/hash/sigma/argon_echo/opt/core_opt_sse4_aes.h \
  crypto/hash/sigma/argon_echo/opt/core_opt_sse4_aes.cpp

crypto_lib_crypto_avx_a_CPPFLAGS = $(AM_CPPFLAGS) $(CONFIG_INCLUDES) $(PLATFORM_INTRINSICS_AVX_FLAGS)
crypto_lib_crypto_avx_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS) $(PLATFORM_INTRINSICS_AVX_FLAGS)
crypto_lib_crypto_avx_a_SOURCES = \
  crypto/hash/sigma/echo256/opt/echo256_opt_avx.h \
  crypto/hash/sigma/echo256/opt/echo256_opt_avx.cpp \
  crypto/hash/sigma/shavite3_256/opt/shavite3_256_opt_avx.h \
  crypto/hash/sigma/shavite3_256/opt/shavite3_256_opt_avx.cpp \
  crypto/hash/sigma/argon_echo/opt/core_opt_avx.h \
  crypto/hash/sigma/argon_echo/opt/core_opt_avx.cpp


crypto_lib_crypto_avx_aes_a_CPPFLAGS = $(AM_CPPFLAGS) $(CONFIG_INCLUDES) $(PLATFORM_INTRINSICS_AVX_FLAGS) $(PLATFORM_INTRINSICS_AES_FLAGS)
crypto_lib_crypto_avx_aes_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS) $(PLATFORM_INTRINSICS_AVX_FLAGS) $(PLATFORM_INTRINSICS_AES_FLAGS)
crypto_lib_crypto_avx_aes_a_SOURCES = \
  crypto/hash/sigma/echo256/opt/echo256_opt_avx_aes.h \
  crypto/hash/sigma/echo256/opt/echo256_opt_avx_aes.cpp \
  crypto/hash/sigma/shavite3_256/opt/shavite3_256_opt_avx_aes.h \
  crypto/hash/sigma/shavite3_256/opt/shavite3_256_opt_avx_aes.cpp \
  crypto/hash/sigma/argon_echo/opt/core_opt_avx_aes.h \
  crypto/hash/sigma/argon_echo/opt/core_opt_avx_aes.cpp


crypto_lib_crypto_avx2_a_CPPFLAGS = $(AM_CPPFLAGS) $(CONFIG_INCLUDES) $(PLATFORM_INTRINSICS_AVX2_FLAGS)
crypto_lib_crypto_avx2_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS) $(PLATFORM_INTRINSICS_AVX2_FLAGS)
crypto_lib_crypto_avx2_a_SOURCES = \
  crypto/hash/sigma/echo256/opt/echo256_opt_avx2.h \
  crypto/hash/sigma/echo256/opt/echo256_opt_avx2.cpp \
  crypto/hash/sigma/shavite3_256/opt/shavite3_256_opt_avx2.h \
  crypto/hash/sigma/shavite3_256/opt/shavite3_256_opt_avx2.cpp \
  crypto/hash/sigma/argon_echo/opt/core_opt_avx2.h \
  crypto/hash/sigma/argon_echo/opt/core_opt_avx2.cpp

crypto_lib_crypto_avx2_aes_a_CPPFLAGS = $(AM_CPPFLAGS) $(CONFIG_INCLUDES) $(PLATFORM_INTRINSICS_AVX2_FLAGS) $(PLATFORM_INTRINSICS_AES_FLAGS)
crypto_lib_crypto_avx2_aes_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS) $(PLATFORM_INTRINSICS_AVX2_FLAGS) $(PLATFORM_INTRINSICS_AES_FLAGS)
crypto_lib_crypto_avx2_aes_a_SOURCES = \
  crypto/hash/sigma/echo256/opt/echo256_opt_avx2_aes.h \
  crypto/hash/sigma/echo256/opt/echo256_opt_avx2_aes.cpp \
  crypto/hash/sigma/shavite3_256/opt/shavite3_256_opt_avx2_aes.h \
  crypto/hash/sigma/shavite3_256/opt/shavite3_256_opt_avx2_aes.cpp \
  crypto/hash/sigma/argon_echo/opt/core_opt_avx2_aes.h \
  crypto/hash/sigma/argon_echo/opt/core_opt_avx2_aes.cpp

crypto_lib_crypto_avx512f_a_CPPFLAGS = $(AM_CPPFLAGS) $(CONFIG_INCLUDES) $(PLATFORM_INTRINSICS_AVX512F_FLAGS)
crypto_lib_crypto_avx512f_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS) $(PLATFORM_INTRINSICS_AVX512F_FLAGS)
crypto_lib_crypto_avx512f_a_SOURCES = \
  crypto/hash/sigma/echo256/opt/echo256_opt_avx512f.h \
  crypto/hash/sigma/echo256/opt/echo256_opt_avx512f.cpp \
  crypto/hash/sigma/shavite3_256/opt/shavite3_256_opt_avx512f.h \
  crypto/hash/sigma/shavite3_256/opt/shavite3_256_opt_avx512f.cpp \
  crypto/hash/sigma/argon_echo/opt/core_opt_avx512f.h \
  crypto/hash/sigma/argon_echo/opt/core_opt_avx512f.cpp

crypto_lib_crypto_avx512f_aes_a_CPPFLAGS = $(AM_CPPFLAGS) $(CONFIG_INCLUDES) $(PLATFORM_INTRINSICS_AVX512F_FLAGS) $(PLATFORM_INTRINSICS_AES_FLAGS)
crypto_lib_crypto_avx512f_aes_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS) $(PLATFORM_INTRINSICS_AVX512F_FLAGS) $(PLATFORM_INTRINSICS_AES_FLAGS)
crypto_lib_crypto_avx512f_aes_a_SOURCES = \
  crypto/hash/sigma/echo256/opt/echo256_opt_avx512f_aes.h \
  crypto/hash/sigma/echo256/opt/echo256_opt_avx512f_aes.cpp \
  crypto/hash/sigma/shavite3_256/opt/shavite3_256_opt_avx512f_aes.h \
  crypto/hash/sigma/shavite3_256/opt/shavite3_256_opt_avx512f_aes.cpp \
  crypto/hash/sigma/argon_echo/opt/core_opt_avx512f_aes.h \
  crypto/hash/sigma/argon_echo/opt/core_opt_avx512f_aes.cpp

crypto_lib_crypto_arm_cortex_a53_a_CPPFLAGS = $(AM_CPPFLAGS) $(CONFIG_INCLUDES) $(PLATFORM_INTRINSICS_CORTEX53_FLAGS)
crypto_lib_crypto_arm_cortex_a53_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS) $(PLATFORM_INTRINSICS_CORTEX53_FLAGS)
crypto_lib_crypto_arm_cortex_a53_a_SOURCES = \
  crypto/hash/sigma/echo256/opt/echo256_opt_arm_cortex_a53.h \
  crypto/hash/sigma/echo256/opt/echo256_opt_arm_cortex_a53.cpp \
  crypto/hash/sigma/shavite3_256/opt/shavite3_256_opt_arm_cortex_a53.h \
  crypto/hash/sigma/shavite3_256/opt/shavite3_256_opt_arm_cortex_a53.cpp \
  crypto/hash/sigma/argon_echo/opt/core_opt_arm_cortex_a53.h \
  crypto/hash/sigma/argon_echo/opt/core_opt_arm_cortex_a53.cpp

crypto_lib_crypto_arm_cortex_a53_aes_a_CPPFLAGS = $(AM_CPPFLAGS) $(CONFIG_INCLUDES) $(PLATFORM_INTRINSICS_CORTEX53_AES_FLAGS)
crypto_lib_crypto_arm_cortex_a53_aes_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS) $(PLATFORM_INTRINSICS_CORTEX53_AES_FLAGS)
crypto_lib_crypto_arm_cortex_a53_aes_a_SOURCES = \
  crypto/hash/sigma/echo256/opt/echo256_opt_arm_cortex_a53_aes.h \
  crypto/hash/sigma/echo256/opt/echo256_opt_arm_cortex_a53_aes.cpp \
  crypto/hash/sigma/shavite3_256/opt/shavite3_256_opt_arm_cortex_a53_aes.h \
  crypto/hash/sigma/shavite3_256/opt/shavite3_256_opt_arm_cortex_a53_aes.cpp \
  crypto/hash/sigma/argon_echo/opt/core_opt_arm_cortex_a53_aes.h \
  crypto/hash/sigma/argon_echo/opt/core_opt_arm_cortex_a53_aes.cpp
  
crypto_lib_crypto_arm_cortex_a57_a_CPPFLAGS = $(AM_CPPFLAGS) $(CONFIG_INCLUDES) $(PLATFORM_INTRINSICS_CORTEX57_FLAGS)
crypto_lib_crypto_arm_cortex_a57_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS) $(PLATFORM_INTRINSICS_CORTEX57_FLAGS)
crypto_lib_crypto_arm_cortex_a57_a_SOURCES = \
  crypto/hash/sigma/echo256/opt/echo256_opt_arm_cortex_a57.h \
  crypto/hash/sigma/echo256/opt/echo256_opt_arm_cortex_a57.cpp \
  crypto/hash/sigma/shavite3_256/opt/shavite3_256_opt_arm_cortex_a57.h \
  crypto/hash/sigma/shavite3_256/opt/shavite3_256_opt_arm_cortex_a57.cpp \
  crypto/hash/sigma/argon_echo/opt/core_opt_arm_cortex_a57.h \
  crypto/hash/sigma/argon_echo/opt/core_opt_arm_cortex_a57.cpp

crypto_lib_crypto_arm_cortex_a57_aes_a_CPPFLAGS = $(AM_CPPFLAGS) $(CONFIG_INCLUDES) $(PLATFORM_INTRINSICS_CORTEX57_AES_FLAGS)
crypto_lib_crypto_arm_cortex_a57_aes_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS) $(PLATFORM_INTRINSICS_CORTEX57_AES_FLAGS)
crypto_lib_crypto_arm_cortex_a57_aes_a_SOURCES = \
  crypto/hash/sigma/echo256/opt/echo256_opt_arm_cortex_a57_aes.h \
  crypto/hash/sigma/echo256/opt/echo256_opt_arm_cortex_a57_aes.cpp \
  crypto/hash/sigma/shavite3_256/opt/shavite3_256_opt_arm_cortex_a57_aes.h \
  crypto/hash/sigma/shavite3_256/opt/shavite3_256_opt_arm_cortex_a57_aes.cpp \
  crypto/hash/sigma/argon_echo/opt/core_opt_arm_cortex_a57_aes.h \
  crypto/hash/sigma/argon_echo/opt/core_opt_arm_cortex_a57_aes.cpp

crypto_lib_crypto_arm_cortex_a72_a_CPPFLAGS = $(AM_CPPFLAGS) $(CONFIG_INCLUDES) $(PLATFORM_INTRINSICS_CORTEX72_FLAGS)
crypto_lib_crypto_arm_cortex_a72_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS) $(PLATFORM_INTRINSICS_CORTEX72_FLAGS)
crypto_lib_crypto_arm_cortex_a72_a_SOURCES = \
  crypto/hash/sigma/echo256/opt/echo256_opt_arm_cortex_a72.h \
  crypto/hash/sigma/echo256/opt/echo256_opt_arm_cortex_a72.cpp \
  crypto/hash/sigma/shavite3_256/opt/shavite3_256_opt_arm_cortex_a72.h \
  crypto/hash/sigma/shavite3_256/opt/shavite3_256_opt_arm_cortex_a72.cpp \
  crypto/hash/sigma/argon_echo/opt/core_opt_arm_cortex_a72.h \
  crypto/hash/sigma/argon_echo/opt/core_opt_arm_cortex_a72.cpp

crypto_lib_crypto_arm_cortex_a72_aes_a_CPPFLAGS = $(AM_CPPFLAGS) $(CONFIG_INCLUDES) $(PLATFORM_INTRINSICS_CORTEX72_AES_FLAGS)
crypto_lib_crypto_arm_cortex_a72_aes_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS) $(PLATFORM_INTRINSICS_CORTEX72_AES_FLAGS)
crypto_lib_crypto_arm_cortex_a72_aes_a_SOURCES = \
  crypto/hash/sigma/echo256/opt/echo256_opt_arm_cortex_a72_aes.h \
  crypto/hash/sigma/echo256/opt/echo256_opt_arm_cortex_a72_aes.cpp \
  crypto/hash/sigma/shavite3_256/opt/shavite3_256_opt_arm_cortex_a72_aes.h \
  crypto/hash/sigma/shavite3_256/opt/shavite3_256_opt_arm_cortex_a72_aes.cpp \
  crypto/hash/sigma/argon_echo/opt/core_opt_arm_cortex_a72_aes.h \
  crypto/hash/sigma/argon_echo/opt/core_opt_arm_cortex_a72_aes.cpp

crypto_lib_crypto_arm_thunderx_aes_a_CPPFLAGS = $(AM_CPPFLAGS) $(CONFIG_INCLUDES) $(PLATFORM_INTRINSICS_THUNDERX_AES_FLAGS)
crypto_lib_crypto_arm_thunderx_aes_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS) $(PLATFORM_INTRINSICS_THUNDERX_AES_FLAGS)
crypto_lib_crypto_arm_thunderx_aes_a_SOURCES = \
  crypto/hash/sigma/echo256/opt/echo256_opt_arm_thunderx_aes.h \
  crypto/hash/sigma/echo256/opt/echo256_opt_arm_thunderx_aes.cpp \
  crypto/hash/sigma/shavite3_256/opt/shavite3_256_opt_arm_thunderx_aes.h \
  crypto/hash/sigma/shavite3_256/opt/shavite3_256_opt_arm_thunderx_aes.cpp \
  crypto/hash/sigma/argon_echo/opt/core_opt_arm_thunderx_aes.h \
  crypto/hash/sigma/argon_echo/opt/core_opt_arm_thunderx_aes.cpp
  
crypto_lib_crypto_arm_v8_crypto_a_CPPFLAGS = $(AM_CPPFLAGS) $(CONFIG_INCLUDES) $(PLATFORM_INTRINSICS_ARMV8_CRYPTO_FLAGS)
crypto_lib_crypto_arm_v8_crypto_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS) $(PLATFORM_INTRINSICS_ARMV8_CRYPTO_FLAGS)
crypto_lib_crypto_arm_v8_crypto_a_SOURCES = \
  crypto/sha256_arm_shani.cpp

crypto_lib_crypto_a_CPPFLAGS = $(AM_CPPFLAGS) $(CONFIG_INCLUDES)
crypto_lib_crypto_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
crypto_lib_crypto_a_SOURCES = \
  util/bytevectorhash.h \
  util/bytevectorhash.cpp \
  crypto/chacha20.h \
  crypto/chacha20.cpp \
  crypto/hash/scrypt.h \
  crypto/hash/scrypt.cpp \
  crypto/scrypt/sha256_scrypt.h \
  crypto/scrypt/sha256_scrypt.cpp \
  crypto/scrypt/crypto_scrypt.h \
  crypto/scrypt/crypto_scrypt.cpp \
  crypto/scrypt/crypto_scrypt_smix.h \
  crypto/scrypt/crypto_scrypt_smix.cpp \
  crypto/scrypt/crypto_scrypt_smix_sse2.h \
  crypto/scrypt/crypto_scrypt_smix_sse2.cpp \
  crypto/hash/sigma/echo256/sphlib/echo.cpp \
  crypto/hash/sigma/echo256/sphlib/sph_types.h \
  crypto/hash/sigma/echo256/sphlib/sph_echo.h \
  crypto/hash/sigma/echo256/sphlib/aes_helper.c \
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

EXTRA_crypto_lib_crypto_a_SOURCES = \
  crypto/hash/sigma/echo256/echo256_opt.h \
  crypto/hash/sigma/echo256/echo256_opt.cpp \
  crypto/hash/sigma/shavite3_256/shavite3_256_opt.cpp \
  crypto/hash/sigma/shavite3_256/shavite3_256_opt.h \
  llvm-cpumodel-hack.cpp

# android compat library; shared between unity and testing framework.
lib_android_compat_a_CPPFLAGS = $(AM_CPPFLAGS) $(CONFIG_INCLUDES) $(PROTOBUF_CFLAGS)
lib_android_compat_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
lib_android_compat_a_SOURCES = \
  unity/compat/android_wallet.cpp \
  unity/compat/android_wallet.h

# consensus: Included by all executables except CLI; also all unity libraries
lib_consensus_a_CPPFLAGS = $(AM_CPPFLAGS) $(COMMON_INCLUDES)
lib_consensus_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
lib_consensus_a_SOURCES = \
  pow/diff_delta.cpp \
  pow/diff_old.cpp \
  pow/diff_common.cpp \
  crypto/hash/city.cpp \
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
  script/consensus.cpp \
  script/consensus.h \
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
  version.h

# common: shared between Daemon, GUI client and non-server tools
lib_common_a_CPPFLAGS = $(AM_CPPFLAGS) $(COMMON_INCLUDES)
lib_common_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
lib_common_a_SOURCES = \
  unity/appmanager.cpp \
  base58.cpp \
  chainparams.cpp \
  coins.cpp \
  compressor.cpp \
  node/context.cpp \
  blockfilter.cpp \
  key.cpp \
  keystore.cpp \
  netaddress.cpp \
  netbase.cpp \
  protocol.cpp \
  scheduler.cpp \
  script/sign.cpp \
  script/standard.cpp \
  $(CORE_H)

# util: shared between all executables and unity libraries.
# This library *must* be included to make sure that the glibc
# backward-compatibility objects and their sanity checks are linked.
lib_util_a_CPPFLAGS = $(AM_CPPFLAGS) $(COMMON_INCLUDES)
lib_util_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
lib_util_a_SOURCES = \
  support/lockedpool.cpp \
  chainparamsbase.cpp \
  clientversion.cpp \
  compat/glibc_sanity.cpp \
  compat/glibcxx_sanity.cpp \
  compat/strnlen.cpp \
  fs.cpp \
  random.cpp \
  randomenv.cpp \
  support/cleanse.cpp \
  sync.cpp \
  util/thread.cpp \
  threadinterrupt.cpp \
  logging.cpp \
  util.cpp \
  util/moneystr.cpp \
  util/strencodings.cpp \
  util/time.cpp \
  util/syscall_sandbox.cpp \
  util/threadnames.cpp \
  util/getuniquepath.cpp \
  $(CORE_H)

# generic: shared between everything except the unity libraries (which provide their own specialised implementations)
lib_generic_a_CPPFLAGS = $(AM_CPPFLAGS) $(COMMON_INCLUDES)
lib_generic_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
lib_generic_a_SOURCES = \
  unity/generic/logging.cpp \
  core_read.cpp \
  core_write.cpp \
  $(CORE_H)
  
# generic_init: anything that includes 'generic' library must also include either this or 'generic_init_node' or provide its own init functions depending on if initialisation of http/rpc etc. is required
lib_generic_init_a_CPPFLAGS = $(AM_CPPFLAGS) $(COMMON_INCLUDES)
lib_generic_init_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
lib_generic_init_a_SOURCES = \
  unity/generic/init_generic.cpp \
  $(CORE_H)
  
# generic_init: anything that includes 'generic' library must also include either this or 'generic_init_server' or provide its own init functions depending on if initialisation of http/rpc etc. is required
lib_generic_init_node_a_CPPFLAGS = $(AM_CPPFLAGS) $(COMMON_INCLUDES)
lib_generic_init_node_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
lib_generic_init_node_a_SOURCES = \
  unity/generic/init_generic_node.cpp \
  $(CORE_H)

if GLIBC_BACK_COMPAT
lib_util_a_SOURCES += compat/glibc_compat.cpp
AM_LDFLAGS += $(COMPAT_LDFLAGS)
endif

# cli: cli client only
lib_cli_a_CPPFLAGS = $(AM_CPPFLAGS) $(COMMON_INCLUDES)
lib_cli_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
lib_cli_a_SOURCES = \
  rpc/client.cpp \
  rpc/protocol.cpp \
  $(CORE_H)

nodist_lib_util_a_SOURCES = $(srcdir)/obj/build.h
#

if TARGET_INTEL
LIB_UNITY_CRYPTO_OBJ=$(LIB_CRYPTO_INTEL)
else
LIB_UNITY_CRYPTO_OBJ=$(LIB_CRYPTO_ARM)
endif

# unity library (for node.js)#
if BUILD_LIBS_NODE_JS

DJINNI_CPP_GEN = \
  unity/djinni/cpp/address_record.hpp \
  unity/djinni/cpp/account_record.hpp \
  unity/djinni/cpp/balance_record.hpp \
  unity/djinni/cpp/block_info_record.hpp \
  unity/djinni/cpp/i_library_controller.cpp \
  unity/djinni/cpp/i_library_controller.hpp \
  unity/djinni/cpp/i_library_listener.hpp \
  unity/djinni/cpp/legacy_wallet_result.hpp \
  unity/djinni/cpp/monitor_listener.hpp \
  unity/djinni/cpp/monitor_record.hpp \
  unity/djinni/cpp/mutation_record.hpp \
  unity/djinni/cpp/mnemonic_record.hpp \
  unity/djinni/cpp/peer_record.hpp \
  unity/djinni/cpp/qr_code_record.hpp \
  unity/djinni/cpp/transaction_record.hpp \
  unity/djinni/cpp/transaction_status.hpp \
  unity/djinni/cpp/input_record.hpp \
  unity/djinni/cpp/output_record.hpp \
  unity/djinni/cpp/uri_recipient.hpp \
  unity/djinni/cpp/i_rpc_controller.hpp \
  unity/djinni/cpp/i_rpc_listener.hpp \
  unity/djinni/cpp/i_p2p_network_controller.hpp \
  unity/djinni/cpp/i_p2p_network_listener.hpp \
  unity/djinni/cpp/i_accounts_controller.hpp \
  unity/djinni/cpp/i_witness_controller.hpp \
  unity/djinni/cpp/witness_estimate_info_record.hpp \
  unity/djinni/cpp/witness_funding_result_record.hpp \
  unity/djinni/cpp/witness_account_statistics_record.hpp \
  unity/djinni/cpp/i_accounts_listener.hpp \
  unity/djinni/cpp/i_wallet_controller.hpp \
  unity/djinni/cpp/i_wallet_listener.hpp \
  unity/djinni/cpp/i_generation_controller.hpp \
  unity/djinni/cpp/i_generation_listener.hpp \
  unity/djinni/cpp/uri_record.hpp \
  unity/djinni/cpp/wallet_lock_status.hpp


DJINNI_NODE_JS_GEN = \
  unity/djinni/node_js/unifiedbackend.cpp \
  unity/djinni/node_js/NJSMonitorListener.cpp \
  unity/djinni/node_js/NJSMonitorListener.hpp \
  unity/djinni/node_js/NJSILibraryController.cpp \
  unity/djinni/node_js/NJSILibraryController.hpp \
  unity/djinni/node_js/NJSILibraryListener.cpp \
  unity/djinni/node_js/NJSILibraryListener.hpp \
  unity/djinni/node_js/NJSIRpcController.cpp \
  unity/djinni/node_js/NJSIRpcController.hpp \
  unity/djinni/node_js/NJSIRpcListener.cpp \
  unity/djinni/node_js/NJSIRpcListener.hpp \
  unity/djinni/node_js/NJSIP2pNetworkController.cpp \
  unity/djinni/node_js/NJSIP2pNetworkController.hpp \
  unity/djinni/node_js/NJSIP2pNetworkListener.cpp \
  unity/djinni/node_js/NJSIP2pNetworkListener.hpp \
  unity/djinni/node_js/NJSIWalletController.cpp \
  unity/djinni/node_js/NJSIWalletController.hpp \
  unity/djinni/node_js/NJSIWalletListener.cpp \
  unity/djinni/node_js/NJSIWalletListener.hpp \
  unity/djinni/node_js/NJSIAccountsController.cpp \
  unity/djinni/node_js/NJSIAccountsController.hpp \
  unity/djinni/node_js/NJSIWitnessController.cpp \
  unity/djinni/node_js/NJSIWitnessController.hpp \
  unity/djinni/node_js/NJSIAccountsListener.cpp \
  unity/djinni/node_js/NJSIAccountsListener.hpp \
  unity/djinni/node_js/NJSIGenerationController.cpp \
  unity/djinni/node_js/NJSIGenerationController.hpp \
  unity/djinni/node_js/NJSIGenerationListener.cpp \
  unity/djinni/node_js/NJSIGenerationListener.hpp



lib_unity_node_js_a_CPPFLAGS = $(AM_CPPFLAGS) $(COMMON_INCLUDES)
lib_unity_node_js_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)

lib_unity_node_js_la_SOURCES = \
  controllers/rpccontroller.cpp \
  controllers/rpccontroller.h \
  core_read.cpp \
  core_write.cpp \
  unity/libinit.cpp \
  unity/libinit.h \
  unity/node_js/logging_node_js.cpp \
  unity/node_js/init_node_js.cpp \
  unity/unity_impl.h \
  unity/unity_impl.cpp \
  unity/controllers/irpccontroller.cpp \
  unity/controllers/ip2pnetworkcontroller.cpp \
  unity/controllers/iaccountscontroller.cpp \
  unity/controllers/iwitnesscontroller.cpp \
  unity/controllers/iwalletcontroller.cpp \
  unity/controllers/igenerationcontroller.cpp \
  unity/djinni/support-lib/nodejs/win_delay_load_hook.cpp \
  $(DJINNI_CPP_GEN) \
  $(DJINNI_NODE_JS_GEN)


NJS_DEPS = $(LIB_CONSENSUS) $(LIB_NODE) $(LIB_UTIL) $(LIB_COMMON) $(LIB_SERVER) $(LIB_RPC) $(LIB_WALLET) $(LIB_CRYPTO_ALL) $(LIB_ZMQ)
lib_unity_node_js_la_LDFLAGS = -shared -module -no-undefined -export-dynamic $(AM_LDFLAGS) $(RELDFLAGS) -pthread
lib_unity_node_js_la_LIBADD = $(NJS_DEPS) $(NJS_DEPS) $(LIBSECP256K1) $(SSL_LIBS) $(CRYPTOPP_LIBS) $(BOOST_LIBS) $(BDB_LIBS) $(LIBLEVELDB) $(LIBMEMENV) $(LIBUNIVALUE) $(EVENT_PTHREADS_LIBS) $(EVENT_LIBS) $(SSL_LIBS) $(ZMQ_LIBS) -lqrencode -lpthread -lcryptopp $(SSL_LIBS) -lcrypto
lib_unity_node_js_la_CPPFLAGS = $(AM_CPPFLAGS) $(COMMON_INCLUDES) $(EVENT_CFLAGS) $(EVENT_PTHREADS_CFLAGS) -I$(srcdir)/unity/djinni/node_js -I$(srcdir)/unity/djinni/cpp -I$(builddir)/obj -I$(srcdir)/secp256k1/include -DBUILD_INTERNAL
lib_unity_node_js_la_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS) $(COMMON_INCLUDES) $(EVENT_CFLAGS) $(EVENT_PTHREADS_CFLAGS) -I$(srcdir)/unity/djinni/node_js -I$(srcdir)/unity/djinni/cpp -I$(builddir)/obj -I$(srcdir)/secp256k1/include -DBUILD_INTERNAL
endif

# unity library (for java)#
if BUILD_LIBS_JNI
lib_unity_a_CPPFLAGS = $(AM_CPPFLAGS) $(COMMON_INCLUDES) $(PROTOBUF_CFLAGS)
lib_unity_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)

lib_unity_jni_la_SOURCES = \
  unity/libinit.cpp \
  unity/libinit.h \
  unity/android/logging_android.cpp \
  unity/android/init_android.cpp \
  unity/unity_impl.h \
  unity/unity_impl.cpp \
  unity/controllers/iwalletcontroller.cpp \
  unity/controllers/ip2pnetworkcontroller.cpp \
  unity/djinni/jni/NativeBlockInfoRecord.hpp \
  unity/djinni/jni/NativeBlockInfoRecord.cpp \
  unity/djinni/jni/NativeMonitorListener.cpp \
  unity/djinni/jni/NativeMonitorListener.hpp \
  unity/djinni/jni/NativeILibraryController.hpp \
  unity/djinni/jni/NativeILibraryController.cpp \
  unity/djinni/jni/NativeIWalletController.hpp \
  unity/djinni/jni/NativeIWalletController.cpp \
  unity/djinni/jni/NativeWalletLockStatus.hpp \
  unity/djinni/jni/NativeWalletLockStatus.cpp \
  unity/djinni/jni/NativeIWalletListener.hpp \
  unity/djinni/jni/NativeIWalletListener.cpp \
  unity/djinni/jni/NativeILibraryListener.hpp \
  unity/djinni/jni/NativeILibraryListener.cpp \
  unity/djinni/jni/NativeIP2pNetworkController.hpp \
  unity/djinni/jni/NativeIP2pNetworkController.cpp \
  unity/djinni/jni/NativeIP2pNetworkListener.hpp \
  unity/djinni/jni/NativeIP2pNetworkListener.cpp \
  unity/djinni/jni/NativeLegacyWalletResult.hpp \
  unity/djinni/jni/NativeMonitorRecord.cpp \
  unity/djinni/jni/NativeMonitorRecord.hpp \
  unity/djinni/jni/NativeMutationRecord.cpp \
  unity/djinni/jni/NativeMnemonicRecord.cpp \
  unity/djinni/jni/NativeMutationRecord.hpp \
  unity/djinni/jni/NativePaymentResultStatus.hpp \
  unity/djinni/jni/NativePeerRecord.hpp \
  unity/djinni/jni/NativePeerRecord.cpp \
  unity/djinni/jni/NativeQrCodeRecord.hpp \
  unity/djinni/jni/NativeQrCodeRecord.cpp \
  unity/djinni/jni/NativeAddressRecord.hpp \
  unity/djinni/jni/NativeAddressRecord.cpp \
  unity/djinni/jni/NativeBalanceRecord.hpp \
  unity/djinni/jni/NativeBalanceRecord.cpp \
  unity/djinni/jni/NativeUriRecord.hpp \
  unity/djinni/jni/NativeUriRecord.cpp \
  unity/djinni/jni/NativeUriRecipient.hpp \
  unity/djinni/jni/NativeUriRecipient.cpp \
  unity/djinni/jni/NativeTransactionRecord.hpp \
  unity/djinni/jni/NativeTransactionRecord.cpp \
  unity/djinni/jni/NativeTransactionStatus.hpp \
  unity/djinni/jni/NativeOutputRecord.hpp \
  unity/djinni/jni/NativeOutputRecord.cpp \
  unity/djinni/jni/NativeInputRecord.hpp \
  unity/djinni/jni/NativeInputRecord.cpp \
  unity/djinni/cpp/i_library_controller.hpp \
  unity/djinni/cpp/i_library_controller.cpp \
  unity/djinni/cpp/i_library_listener.hpp \
  unity/djinni/cpp/balance_record.hpp \
  unity/djinni/cpp/mutation_record.hpp \
  $(CORE_H)

nodist_lib_unity_jni_la_SOURCES = \
  unity/djinni/support-lib/jni/djinni_support.cpp \
  unity/djinni/support-lib/jni/djinni_main.cpp

ANDROID_DEPS = $(LIB_COMPAT) $(LIB_CONSENSUS) $(LIB_NODE) $(LIB_UTIL) $(LIB_COMMON) $(LIB_WALLET) $(LIB_CRYPTO_ALL) $(LIB_ANDROID_COMPAT)


lib_unity_jni_la_LDFLAGS = -shared -module -export-dynamic $(AM_LDFLAGS_NO_THREADS) $(RELDFLAGS) -llog
lib_unity_jni_la_LIBADD = $(ANDROID_DEPS) $(ANDROID_DEPS) $(LIBSECP256K1) $(SSL_LIBS) $(BOOST_LIBS) $(BDB_LIBS) $(LIBLEVELDB) $(LIBMEMENV) $(PROTOBUF_LIBS) -lqrencode -lcryptopp
lib_unity_jni_la_CPPFLAGS = $(AM_CPPFLAGS) $(COMMON_INCLUDES) -I$(srcdir)/unity/djinni/jni -I$(srcdir)/unity/djinni/cpp -I$(builddir)/obj -I$(srcdir)/unity/djinni/support-lib/jni -I$(srcdir)/secp256k1/include -DBUILD_INTERNAL
lib_unity_jni_la_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS) $(COMMON_INCLUDES) -I$(srcdir)/unity/djinni/jni -I$(srcdir)/unity/djinni/cpp -I$(builddir)/obj -I$(srcdir)/unity/djinni/support-lib/jni -I$(srcdir)/secp256k1/include -DBUILD_INTERNAL
endif
#

# unity library for objc #
if BUILD_LIBS_OBJC

noinst_LIBRARIES += $(LIB_UNITY_OBJC)

lib_unity_objc_a_CPPFLAGS = $(AM_CPPFLAGS) $(COMMON_INCLUDES) -I$(srcdir)/secp256k1/include -DBUILD_INTERNAL
lib_unity_objc_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS) -I$(srcdir)/unity/djinni/cpp
lib_unity_objc_a_OBJCXXFLAGS = -fobjc-arc  -I$(srcdir)/unity/djinni/cpp $(DJINNI_OBJCXXFLAGS)

DJINNI_CPP_GEN = \
  unity/djinni/cpp/address_record.hpp \
  unity/djinni/cpp/balance_record.hpp \
  unity/djinni/cpp/block_info_record.hpp \
  unity/djinni/cpp/i_library_controller.cpp \
  unity/djinni/cpp/i_library_controller.hpp \
  unity/djinni/cpp/i_library_listener.hpp \
  unity/djinni/cpp/legacy_wallet_result.hpp \
  unity/djinni/cpp/monitor_listener.hpp \
  unity/djinni/cpp/monitor_record.hpp \
  unity/djinni/cpp/mutation_record.hpp \
  unity/djinni/cpp/mnemonic_record.hpp \
  unity/djinni/cpp/payment_result_status.hpp \
  unity/djinni/cpp/peer_record.hpp \
  unity/djinni/cpp/qr_code_record.hpp \
  unity/djinni/cpp/transaction_record.hpp \
  unity/djinni/cpp/transaction_status.hpp \
  unity/djinni/cpp/input_record.hpp \
  unity/djinni/cpp/output_record.hpp \
  unity/djinni/cpp/uri_recipient.hpp \
  unity/djinni/cpp/uri_record.hpp

DJINNI_OBJC_GEN = \
  unity/djinni/objc/DBAddressRecord+Private.h \
  unity/djinni/objc/DBAddressRecord+Private.mm \
  unity/djinni/objc/DBAddressRecord.h \
  unity/djinni/objc/DBAddressRecord.mm \
  unity/djinni/objc/DBBalanceRecord+Private.h \
  unity/djinni/objc/DBBalanceRecord+Private.mm \
  unity/djinni/objc/DBBalanceRecord.h \
  unity/djinni/objc/DBBalanceRecord.mm \
  unity/djinni/objc/DBBlockInfoRecord+Private.h \
  unity/djinni/objc/DBBlockInfoRecord+Private.mm \
  unity/djinni/objc/DBBlockInfoRecord.h \
  unity/djinni/objc/DBBlockInfoRecord.mm \
  unity/djinni/objc/DBILibraryController+Private.h \
  unity/djinni/objc/DBILibraryController+Private.mm \
  unity/djinni/objc/DBILibraryController.h \
  unity/djinni/objc/DBILibraryController.mm \
  unity/djinni/objc/DBILibraryListener+Private.h \
  unity/djinni/objc/DBILibraryListener+Private.mm \
  unity/djinni/objc/DBILibraryController.h \
  unity/djinni/objc/DBIP2pNetworkController+Private.h  \
  unity/djinni/objc/DBIP2pNetworkController+Private.mm \
  unity/djinni/objc/DBIP2pNetworkListener.h \
  unity/djinni/objc/DBIP2pNetworkListener+Private.h \
  unity/djinni/objc/DBIP2pNetworkListener+Private.mm \
  unity/djinni/objc/DBLegacyWalletResult.h \
  unity/djinni/objc/DBLegacyWalletResult+Private.h \
  unity/djinni/objc/DBMonitorListener+Private.h \
  unity/djinni/objc/DBMonitorListener+Private.mm \
  unity/djinni/objc/DBMonitorListener.h \
  unity/djinni/objc/DBMonitorRecord+Private.h \
  unity/djinni/objc/DBMonitorRecord+Private.mm \
  unity/djinni/objc/DBMutationRecord+Private.h \
  unity/djinni/objc/DBMutationRecord+Private.mm \
  unity/djinni/objc/DBMutationRecord.h \
  unity/djinni/objc/DBMutationRecord.mm \
  unity/djinni/objc/DBMnemonicRecord+Private.h \
  unity/djinni/objc/DBMnemonicRecord+Private.mm \
  unity/djinni/objc/DBMnemonicRecord.h \
  unity/djinni/objc/DBMnemonicRecord.mm \
  unity/djinni/objc/DBMonitorRecord.h \
  unity/djinni/objc/DBMonitorRecord.mm \
  unity/djinni/objc/DBInputRecord+Private.h \
  unity/djinni/objc/DBInputRecord+Private.mm \
  unity/djinni/objc/DBInputRecord.h \
  unity/djinni/objc/DBInputRecord.mm \
  unity/djinni/objc/DBOutputRecord+Private.h \
  unity/djinni/objc/DBOutputRecord+Private.mm \
  unity/djinni/objc/DBOutputRecord.h \
  unity/djinni/objc/DBOutputRecord.mm \
  unity/djinni/objc/DBPaymentResultStatus+Private.h \
  unity/djinni/objc/DBPaymentResultStatus.h \
  unity/djinni/objc/DBPeerRecord+Private.h \
  unity/djinni/objc/DBPeerRecord+Private.mm \
  unity/djinni/objc/DBPeerRecord.h \
  unity/djinni/objc/DBPeerRecord.mm \
  unity/djinni/objc/DBQrCodeRecord+Private.h \
  unity/djinni/objc/DBQrCodeRecord+Private.mm \
  unity/djinni/objc/DBQrCodeRecord.h \
  unity/djinni/objc/DBQrCodeRecord.mm \
  unity/djinni/objc/DBTransactionRecord+Private.h \
  unity/djinni/objc/DBTransactionRecord+Private.mm \
  unity/djinni/objc/DBTransactionRecord.h \
  unity/djinni/objc/DBTransactionRecord.mm \
  unity/djinni/objc/DBTransactionStatus+Private.h \
  unity/djinni/objc/DBTransactionStatus.h \
  unity/djinni/objc/DBUriRecipient+Private.h \
  unity/djinni/objc/DBUriRecipient+Private.mm \
  unity/djinni/objc/DBUriRecipient.h \
  unity/djinni/objc/DBUriRecipient.mm \
  unity/djinni/objc/DBUriRecord+Private.h \
  unity/djinni/objc/DBUriRecord+Private.mm \
  unity/djinni/objc/DBUriRecord.h \
  unity/djinni/objc/DBUriRecord.mm

lib_unity_objc_a_SOURCES = \
  unity/libinit.cpp \
  unity/libinit.h \
  unity/ios/logging_ios.cpp \
  unity/ios/init_ios.cpp \
  $(lib_wallet_a_SOURCES) \
  $(lib_common_a_SOURCES) \
  $(lib_util_a_SOURCES) \
  $(lib_node_a_SOURCES) \
  $(lib_consensus_a_SOURCES) \
  $(crypto_lib_crypto_a_SOURCES) \
  unity/unity_impl.h \
  unity/unity_impl.cpp \
  unity/controllers/ip2pnetworkcontroller.cpp \
  $(DJINNI_CPP_GEN) \
  $(DJINNI_OBJC_GEN) \
  $(CORE_H)

LIB_UNITY_CRYPTO=lib_unity_crypto.a

$(LIB_UNITY_CRYPTO): $(LIB_UNITY_CRYPTO_OBJ)
	libtool -static -o $@ $^

$(LIB_UNITY_OBJC): $(LIBSECP256K1) $(LIBLEVELDB) $(LIBMEMENV) $(LIB_UNITY_CRYPTO)

endif

# Daemon binary #
if BUILD_DAEMON
GuldenD_SOURCES = daemon-main.cpp
GuldenD_CPPFLAGS = $(AM_CPPFLAGS) $(COMMON_INCLUDES)
GuldenD_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
GuldenD_LDFLAGS = $(RELDFLAGS) $(AM_LDFLAGS) $(LIBTOOL_APP_LDFLAGS)

if TARGET_WINDOWS
GuldenD_SOURCES += daemon-res.rc
endif

GuldenD_LDADD = \
  $(LIB_SERVER) \
  $(LIB_RPC) \
  $(LIB_NODE) \
  $(LIB_SERVER) \
  $(LIB_RPC) \
  $(LIB_NODE) \
  $(LIB_WALLET) \
  $(LIB_COMMON) \
  $(LIBUNIVALUE) \
  $(LIB_UTIL) \
  $(LIB_GENERIC) \
  $(LIB_GENERIC_INIT_NODE) \
  $(LIB_SERVER) \
  $(LIB_RPC) \
  $(LIB_ZMQ) \
  $(LIB_CONSENSUS) \
  $(LIB_CRYPTO_ALL) \
  $(LIBLEVELDB) \
  $(LIBMEMENV) \
  $(LIBSECP256K1)

GuldenD_LDADD += $(BOOST_LIBS) $(BDB_LIBS) $(CRYPTOPP_LIBS) $(CRYPTO_LIBS) $(MINIUPNPC_LIBS) $(EVENT_PTHREADS_LIBS) $(EVENT_LIBS) $(ZMQ_LIBS) $(SSL_LIBS)
endif
#

# cli binary #
if BUILD_UTILS
Gulden_cli_SOURCES = cli-main.cpp
Gulden_cli_CPPFLAGS = $(AM_CPPFLAGS) $(COMMON_INCLUDES) $(EVENT_CFLAGS)
Gulden_cli_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
Gulden_cli_LDFLAGS = $(RELDFLAGS) $(AM_LDFLAGS) $(LIBTOOL_APP_LDFLAGS)

if TARGET_WINDOWS
Gulden_cli_SOURCES += cli-res.rc
endif

Gulden_cli_LDADD = \
  $(LIB_CLI) \
  $(LIBUNIVALUE) \
  $(LIB_UTIL) \
  $(LIB_GENERIC) \
  $(LIB_GENERIC_INIT) \
  $(LIB_CRYPTO_ALL)

Gulden_cli_LDADD += $(BOOST_LIBS) $(SSL_LIBS) $(CRYPTOPP_LIBS) $(CRYPTO_LIBS) $(EVENT_LIBS)
endif
#

# tx binary #
if BUILD_UTILS
Gulden_tx_SOURCES = tx-main.cpp
Gulden_tx_CPPFLAGS = $(AM_CPPFLAGS) $(COMMON_INCLUDES)
Gulden_tx_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
Gulden_tx_LDFLAGS = $(RELDFLAGS) $(AM_LDFLAGS) $(LIBTOOL_APP_LDFLAGS)

if TARGET_WINDOWS
Gulden_tx_SOURCES += tx-res.rc
endif

Gulden_tx_LDADD = \
  $(LIBUNIVALUE) \
  $(LIB_COMMON) \
  $(LIB_UTIL) \
  $(LIB_GENERIC) \
  $(LIB_GENERIC_INIT) \
  $(LIB_CONSENSUS) \
  $(LIB_CRYPTO_ALL) \
  $(LIB_COMMON) \
  $(LIB_UTIL) \
  $(LIB_GENERIC) \
  $(LIB_CONSENSUS) \
  $(LIB_CRYPTO_ALL) \
  $(LIB_COMMON) \
  $(LIBSECP256K1)

Gulden_tx_LDADD += $(BOOST_LIBS) $(CRYPTOPP_LIBS) $(CRYPTO_LIBS)
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


config/build-config.h: config/stamp-h1
	@$(MAKE) -C $(top_builddir) $(subdir)/$(@)
config/stamp-h1: $(top_srcdir)/$(subdir)/config/build-config.h.in $(top_builddir)/config.status
	$(AM_V_at)$(MAKE) -C $(top_builddir) $(subdir)/$(@)
$(top_srcdir)/$(subdir)/config/build-config.h.in:  $(am__configure_deps)
	$(AM_V_at)$(MAKE) -C $(top_srcdir) $(subdir)/config/build-config.h.in

clean-local:
	-$(MAKE) -C secp256k1 clean
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

include Makefile.crc32c.include
include Makefile.leveldb.include

if ENABLE_TESTS
include Makefile.test.include
endif

if ENABLE_BENCH
#include Makefile.bench.include
include Makefile.sigmabench.include
endif

if ENABLE_QT
include Makefile.qt.include
endif

if ENABLE_QT_TESTS
include Makefile.qttest.include
endif

include Makefile.univalue.include
