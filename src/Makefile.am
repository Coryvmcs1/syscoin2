DIST_SUBDIRS = secp256k1 univalue cryptopp

AM_LDFLAGS = $(PTHREAD_CFLAGS) $(LIBTOOL_LDFLAGS) $(HARDENED_LDFLAGS)
AM_CXXFLAGS = $(HARDENED_CXXFLAGS)
AM_CPPFLAGS = $(HARDENED_CPPFLAGS)

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
          OPT="$(AM_CXXFLAGS) $(PIE_FLAGS) $(CXXFLAGS) $(AM_CPPFLAGS) $(CPPFLAGS) -D__STDC_LIMIT_MACROS"
endif

SYSCOIN_CONFIG_INCLUDES=-I$(builddir)/config
SYSCOIN_INCLUDES=-I$(builddir) -I$(builddir)/obj $(BOOST_CPPFLAGS) $(LEVELDB_CPPFLAGS) $(CRYPTO_CFLAGS) $(SSL_CFLAGS)

SYSCOIN_INCLUDES += -I$(srcdir)/secp256k1/include
SYSCOIN_INCLUDES += -I$(srcdir)/univalue/include

LIBSYSCOIN_SERVER=libsyscoin_server.a
LIBSYSCOIN_WALLET=libsyscoin_wallet.a
LIBSYSCOIN_COMMON=libsyscoin_common.a
LIBSYSCOIN_CLI=libsyscoin_cli.a
LIBSYSCOIN_UTIL=libsyscoin_util.a
LIBSYSCOIN_CRYPTO=crypto/libsyscoin_crypto.a
LIBSYSCOINQT=qt/libsyscoinqt.a
LIBSECP256K1=secp256k1/libsecp256k1.la
LIBUNIVALUE=univalue/libunivalue.la
LIBCRYPTOPP=cryptopp/libcryptopp.a

$(LIBSECP256K1): $(wildcard secp256k1/src/*) $(wildcard secp256k1/include/*)
	$(AM_V_at)$(MAKE) $(AM_MAKEFLAGS) -C $(@D) $(@F)
  
$(LIBUNIVALUE): $(wildcard univalue/lib/*) $(wildcard univalue/include/*)
	$(AM_V_at)$(MAKE) $(AM_MAKEFLAGS) -C $(@D) $(@F)
	
$(LIBCRYPTOPP): 
	@echo "Building LibCryptoPP ..." && $(MAKE) -C $(@D) $(@F) CXX="$(CXX)" \
	  CC="$(CC)" \
          OPT="$(AM_CXXFLAGS) $(PIE_FLAGS) $(CXXFLAGS) $(AM_CPPFLAGS) $(CPPFLAGS)"
  
# Make is not made aware of per-object dependencies to avoid limiting building parallelization
# But to build the less dependent modules first, we manually select their order here:
EXTRA_LIBRARIES = \
  crypto/libsyscoin_crypto.a \
  libsyscoin_util.a \
  libsyscoin_common.a \
  libsyscoin_server.a \
  libsyscoin_cli.a
if ENABLE_WALLET
SYSCOIN_INCLUDES += $(BDB_CPPFLAGS)
EXTRA_LIBRARIES += libsyscoin_wallet.a
endif
if ENABLE_ZMQ
EXTRA_LIBRARIES += libsyscoin_zmq.a
endif

if BUILD_SYSCOIN_LIBS
lib_LTLIBRARIES = libsyscoinconsensus.la
LIBSYSCOIN_CONSENSUS=libsyscoinconsensus.la
else
LIBSYSCOIN_CONSENSUS=
endif

bin_PROGRAMS =
TESTS =
BENCHMARKS =

if BUILD_SYSCOIND
  bin_PROGRAMS += syscoind
endif

if BUILD_SYSCOIN_UTILS
  bin_PROGRAMS += syscoin-cli
endif

.PHONY: FORCE
# syscoin core #
SYSCOIN_CORE_H = \
  alias.h \
  offer.h \
  message.h \
  cert.h \
  escrow.h \
  messagecrypter.h \
  addrman.h \
  alert.h \
  auxpow.h \
  amount.h \
  arith_uint256.h \
  base58.h \
  bloom.h \
  chain.h \
  chainparams.h \
  chainparamsbase.h \
  chainparamsseeds.h \
  checkpoints.h \
  checkqueue.h \
  clientversion.h \
  coincontrol.h \
  coins.h \
  compat.h \
  compat/byteswap.h \
  compat/endian.h \
  compat/sanity.h \
  compressor.h \
  consensus/consensus.h \
  consensus/merkle.h \
  consensus/params.h \
  consensus/validation.h \
  core_io.h \
  core_memusage.h \
  hash.h \
  httprpc.h \
  httpserver.h \
  init.h \
  key.h \
  keystore.h \
  dbwrapper.h \
  limitedmap.h \
  main.h \
  memusage.h \
  merkleblock.h \
  miner.h \
  net.h \
  netbase.h \
  noui.h \
  policy/fees.h \
  policy/policy.h \
  pow.h \
  prevector.h \
  primitives/block.h \
  primitives/pureheader.h \
  primitives/transaction.h \
  protocol.h \
  pubkey.h \
  random.h \
  reverselock.h \
  rpcclient.h \
  rpcprotocol.h \
  rpcserver.h \
  scheduler.h \
  script/interpreter.h \
  script/script.h \
  script/script_error.h \
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
  torcontrol.h \
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
  wallet/db.h \
  wallet/wallet.h \
  wallet/wallet_ismine.h \
  wallet/walletdb.h \
  zmq/zmqabstractnotifier.h \
  zmq/zmqconfig.h\
  zmq/zmqnotificationinterface.h \
  zmq/zmqpublishnotifier.h


obj/build.h: FORCE
	@$(MKDIR_P) $(builddir)/obj
	@$(top_srcdir)/share/genbuild.sh $(abs_top_builddir)/src/obj/build.h \
	  $(abs_top_srcdir)
libsyscoin_util_a-clientversion.$(OBJEXT): obj/build.h

# server: shared between syscoind and syscoin-qt
libsyscoin_server_a_CPPFLAGS = $(AM_CPPFLAGS) $(SYSCOIN_INCLUDES) $(MINIUPNPC_CPPFLAGS) $(EVENT_CFLAGS) $(EVENT_PTHREADS_CFLAGS)
libsyscoin_server_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
libsyscoin_server_a_SOURCES = \
  alias.cpp \
  offer.cpp \
  message.cpp \
  cert.cpp \
  escrow.cpp \
  messagecrypter.cpp \
  addrman.cpp \
  auxpow.cpp \
  alert.cpp \
  bloom.cpp \
  chain.cpp \
  checkpoints.cpp \
  httprpc.cpp \
  httpserver.cpp \
  init.cpp \
  dbwrapper.cpp \
  main.cpp \
  merkleblock.cpp \
  miner.cpp \
  net.cpp \
  noui.cpp \
  policy/fees.cpp \
  policy/policy.cpp \
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
  torcontrol.cpp \
  txdb.cpp \
  txmempool.cpp \
  validationinterface.cpp \
  $(SYSCOIN_CORE_H)

if ENABLE_ZMQ
LIBSYSCOIN_ZMQ=libsyscoin_zmq.a

libsyscoin_zmq_a_CPPFLAGS = $(SYSCOIN_INCLUDES) $(ZMQ_CFLAGS)
libsyscoin_zmq_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
libsyscoin_zmq_a_SOURCES = \
  zmq/zmqabstractnotifier.cpp \
  zmq/zmqnotificationinterface.cpp \
  zmq/zmqpublishnotifier.cpp
endif


# wallet: shared between syscoind and syscoin-qt, but only linked
# when wallet enabled
libsyscoin_wallet_a_CPPFLAGS = $(AM_CPPFLAGS) $(SYSCOIN_INCLUDES)
libsyscoin_wallet_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
libsyscoin_wallet_a_SOURCES = \
  wallet/crypter.cpp \
  wallet/db.cpp \
  wallet/rpcdump.cpp \
  wallet/rpcwallet.cpp \
  wallet/wallet.cpp \
  wallet/wallet_ismine.cpp \
  wallet/walletdb.cpp \
  $(SYSCOIN_CORE_H)

# crypto primitives library
crypto_libsyscoin_crypto_a_CPPFLAGS = $(AM_CPPFLAGS) $(SYSCOIN_CONFIG_INCLUDES)
crypto_libsyscoin_crypto_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
crypto_libsyscoin_crypto_a_SOURCES = \
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

# common: shared between syscoind, and syscoin-qt and non-server tools
libsyscoin_common_a_CPPFLAGS = $(AM_CPPFLAGS) $(SYSCOIN_INCLUDES)
libsyscoin_common_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
libsyscoin_common_a_SOURCES = \
  amount.cpp \
  arith_uint256.cpp \
  base58.cpp \
  chainparams.cpp \
  coins.cpp \
  compressor.cpp \
  consensus/merkle.cpp \
  core_read.cpp \
  core_write.cpp \
  hash.cpp \
  key.cpp \
  keystore.cpp \
  netbase.cpp \
  primitives/block.cpp \
  primitives/pureheader.cpp \
  primitives/transaction.cpp \
  protocol.cpp \
  pubkey.cpp \
  scheduler.cpp \
  script/interpreter.cpp \
  script/script.cpp \
  script/script_error.cpp \
  script/sign.cpp \
  script/standard.cpp \
  $(SYSCOIN_CORE_H)

# util: shared between all executables.
# This library *must* be included to make sure that the glibc
# backward-compatibility objects and their sanity checks are linked.
libsyscoin_util_a_CPPFLAGS = $(AM_CPPFLAGS) $(SYSCOIN_INCLUDES)
libsyscoin_util_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
libsyscoin_util_a_SOURCES = \
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
  $(SYSCOIN_CORE_H)

if GLIBC_BACK_COMPAT
libsyscoin_util_a_SOURCES += compat/glibc_compat.cpp
endif

# cli: shared between syscoin-cli and syscoin-qt
libsyscoin_cli_a_CPPFLAGS = $(AM_CPPFLAGS) $(SYSCOIN_INCLUDES)
libsyscoin_cli_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
libsyscoin_cli_a_SOURCES = \
  rpcclient.cpp \
  $(SYSCOIN_CORE_H)

nodist_libsyscoin_util_a_SOURCES = $(srcdir)/obj/build.h
#

# syscoind binary #
syscoind_SOURCES = syscoind.cpp
syscoind_CPPFLAGS = $(AM_CPPFLAGS) $(SYSCOIN_INCLUDES)
syscoind_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
syscoind_LDFLAGS = $(RELDFLAGS) $(AM_LDFLAGS) $(LIBTOOL_APP_LDFLAGS)

if TARGET_WINDOWS
syscoind_SOURCES += syscoind-res.rc
endif

syscoind_LDADD = \
  $(LIBSYSCOIN_SERVER) \
  $(LIBSYSCOIN_COMMON) \
  $(LIBUNIVALUE) \
  $(LIBSYSCOIN_UTIL) \
  $(LIBSYSCOIN_CRYPTO) \
  $(LIBLEVELDB) \
  $(LIBMEMENV) \
  $(LIBSECP256K1) \
  $(LIBCRYPTOPP)

if ENABLE_ZMQ
syscoind_LDADD += $(LIBSYSCOIN_ZMQ) $(ZMQ_LIBS)
endif

if ENABLE_WALLET
syscoind_LDADD += libsyscoin_wallet.a
endif

syscoind_LDADD += $(BOOST_LIBS) $(BDB_LIBS) $(SSL_LIBS) $(CRYPTO_LIBS) $(MINIUPNPC_LIBS) $(EVENT_PTHREADS_LIBS) $(EVENT_LIBS)

# syscoin-cli binary #
syscoin_cli_SOURCES = syscoin-cli.cpp
syscoin_cli_CPPFLAGS = $(AM_CPPFLAGS) $(SYSCOIN_INCLUDES) $(EVENT_CFLAGS)
syscoin_cli_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
syscoin_cli_LDFLAGS = $(RELDFLAGS) $(AM_LDFLAGS) $(LIBTOOL_APP_LDFLAGS)

if TARGET_WINDOWS
syscoin_cli_SOURCES += syscoin-cli-res.rc
endif

syscoin_cli_LDADD = \
  $(LIBSYSCOIN_CLI) \
  $(LIBUNIVALUE) \
  $(LIBSYSCOIN_UTIL)

syscoin_cli_LDADD += $(BOOST_LIBS) $(SSL_LIBS) $(CRYPTO_LIBS) $(EVENT_LIBS)
#

# syscoin-tx binary #
syscoin_tx_SOURCES = syscoin-tx.cpp
syscoin_tx_CPPFLAGS = $(AM_CPPFLAGS) $(SYSCOIN_INCLUDES)
syscoin_tx_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
syscoin_tx_LDFLAGS = $(RELDFLAGS) $(AM_LDFLAGS) $(LIBTOOL_APP_LDFLAGS)

if TARGET_WINDOWS
syscoin_tx_SOURCES += syscoin-tx-res.rc
endif

syscoin_tx_LDADD = \
  $(LIBUNIVALUE) \
  $(LIBSYSCOIN_COMMON) \
  $(LIBSYSCOIN_UTIL) \
  $(LIBSYSCOIN_CRYPTO) \
  $(LIBSECP256K1)

syscoin_tx_LDADD += $(BOOST_LIBS) $(CRYPTO_LIBS)
#

# syscoinconsensus library #
if BUILD_SYSCOIN_LIBS
include_HEADERS = script/syscoinconsensus.h
libsyscoinconsensus_la_SOURCES = \
  crypto/hmac_sha512.cpp \
  crypto/ripemd160.cpp \
  crypto/sha1.cpp \
  crypto/sha256.cpp \
  crypto/sha512.cpp \
  hash.cpp \
  primitives/transaction.cpp \
  pubkey.cpp \
  script/syscoinconsensus.cpp \
  script/interpreter.cpp \
  script/script.cpp \
  uint256.cpp \
  utilstrencodings.cpp

if GLIBC_BACK_COMPAT
  libsyscoinconsensus_la_SOURCES += compat/glibc_compat.cpp
endif

libsyscoinconsensus_la_LDFLAGS = $(AM_LDFLAGS) -no-undefined $(RELDFLAGS)
libsyscoinconsensus_la_LIBADD = $(LIBSECP256K1)
libsyscoinconsensus_la_CPPFLAGS = $(AM_CPPFLAGS) -I$(builddir)/obj -I$(srcdir)/secp256k1/include -DBUILD_SYSCOIN_INTERNAL
libsyscoinconsensus_la_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)

endif
#

CLEANFILES = leveldb/libleveldb.a leveldb/libmemenv.a
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

EXTRA_DIST = leveldb

clean-local:
	-$(MAKE) -C leveldb clean
	-$(MAKE) -C secp256k1 clean
	-$(MAKE) -C univalue clean
	-$(MAKE) -C cryptopp clean
	-rm -f leveldb/*/*.gcda leveldb/*/*.gcno leveldb/helpers/memenv/*.gcda leveldb/helpers/memenv/*.gcno
	-rm -f config.h

.rc.o:
	@test -f $(WINDRES)
	$(AM_V_GEN) $(WINDRES) -DWINDRES_PREPROC -i $< -o $@

.mm.o:
	$(AM_V_CXX) $(OBJCXX) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(AM_CPPFLAGS) \
	  $(CPPFLAGS) $(AM_CXXFLAGS) $(QT_INCLUDES) $(AM_CXXFLAGS) $(PIE_FLAGS) $(CXXFLAGS) -c -o $@ $<

%.pb.cc %.pb.h: %.proto
	@test -f $(PROTOC)
	$(AM_V_GEN) $(PROTOC) --cpp_out=$(@D) --proto_path=$(abspath $(<D) $<)

if ENABLE_TESTS
include Makefile.test.include
endif

if ENABLE_BENCH
include Makefile.bench.include
endif

if ENABLE_QT
include Makefile.qt.include
endif

if ENABLE_QT_TESTS
include Makefile.qttest.include
endif
