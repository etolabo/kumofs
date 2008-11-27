export ERB
export RAGEL
SUBDIRS = log kazuhiki rpc memproto storage

noinst_LIBRARIES = libkumo_logic.a

libkumo_logic_a_SOURCES = \
		logic/hash.cc \
		logic/base.cc

bin_PROGRAMS = kumo-manager kumo-server kumo-gateway

kumo_manager_SOURCES = \
		control/control.cc \
		logic/mgr_replace.cc \
		logic/mgr.cc \
		logic/mgr_rpc.cc \
		logic/mgr_control.cc \
		logic/mgr_cmd.cc

kumo_server_SOURCES = \
		logic/srv_replace.cc \
		logic/srv_store.cc \
		logic/srv_control.cc \
		logic/srv.cc \
		logic/srv_rpc.cc \
		logic/srv_cmd.cc

kumo_gateway_SOURCES = \
		gateway/memproto_text.cc \
		logic/gw_store.cc \
		logic/gw.cc \
		logic/gw_rpc.cc \
		logic/gw_cmd.cc

#		gateway/memproto.cc

kumo_manager_LDADD  = \
		libkumo_logic.a \
		kazuhiki/libkazuhiki.a \
		log/libkumo_log.a \
		rpc/libkumo_cluster.a

kumo_server_LDADD  = \
		libkumo_logic.a \
		kazuhiki/libkazuhiki.a \
		storage/libkumo_storage.a \
		log/libkumo_log.a \
		rpc/libkumo_cluster.a

kumo_gateway_LDADD  = \
		libkumo_logic.a \
		kazuhiki/libkazuhiki.a \
		memproto/libkumo_memproto.a \
		log/libkumo_log.a \
		rpc/libkumo_rpc.a

if WITH_LIBMEMCACHED_HOOK

lib_LTLIBRARIES = libkumo_libmemcached.la

libkumo_libmemcached_la_SOURCES = \
		$(libkumo_a_SOURCES) \
		user/libmemcached.cc \
		memproto/memproto.c \
		logic/cli_dispatch.cc \
		logic/cli_rpc.cc

# work around for "created both with libtool and without"
libkumo_libmemcached_la_CFLAGS = $(AM_CFLAGS)
libkumo_libmemcached_la_CXXFLAGS = $(AM_CXXFLAGS)

endif


noinst_HEADERS = \
		logic/base.h \
		logic/clock.h \
		logic/hash.h \
		logic/rpc.h \
		logic/cluster.h \
		logic/srv.h \
		logic/srv_impl.h \
		logic/mgr_impl.h \
		logic/gw.h \
		logic/gw_impl.h \
		control/control.h \
		protocol.h \
		gateway/memproto.h \
		gateway/memproto_text.h \
		gateway/gateway.h \
		storage.h

