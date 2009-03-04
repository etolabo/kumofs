export ERB
export RUBY
export RAGEL
SUBDIRS = mp mpsrc log kazuhiki rpc memproto storage

noinst_LIBRARIES = libkumo_logic.a

libkumo_logic_a_SOURCES = \
		logic/hash.cc \
		logic/base.cc

bin_PROGRAMS = kumo-manager kumo-server kumo-gateway kumomergedb
bin_SCRIPTS = kumoctl kumostat kumolog

EXTRA_DIST = $(bin_SCRIPTS) doc/memo.txt doc/memo.pdf

kumo_manager_SOURCES = \
		logic/mgr_replace.cc \
		logic/mgr.cc \
		logic/mgr_rpc.cc \
		logic/mgr_control.cc \
		logic/mgr_cmd.cc

kumo_server_SOURCES = \
		logic/srv_stat.cc \
		logic/srv_offer.cc \
		logic/srv_replace.cc \
		logic/srv_store.cc \
		logic/srv_control.cc \
		logic/srv.cc \
		logic/srv_rpc.cc \
		logic/srv_cmd.cc

kumo_gateway_SOURCES = \
		gateway/memtext.cc \
		gateway/cloudy.cc \
		logic/gw_store.cc \
		logic/gw.cc \
		logic/gw_rpc.cc \
		logic/gw_cmd.cc

kumomergedb_SOURCES = \
		logic/mergedb.cc

kumo_manager_LDADD  = \
		libkumo_logic.a \
		kazuhiki/libkazuhiki.a \
		rpc/libkumo_cluster.a \
		log/libkumo_log.a \
		mpsrc/libmpio.a

kumo_server_LDADD  = \
		libkumo_logic.a \
		kazuhiki/libkazuhiki.a \
		storage/libkumo_storage.a \
		rpc/libkumo_cluster.a \
		log/libkumo_log.a \
		mpsrc/libmpio.a

kumo_gateway_LDADD  = \
		libkumo_logic.a \
		kazuhiki/libkazuhiki.a \
		memproto/libkumo_memproto.a \
		log/libkumo_log.a \
		rpc/libkumo_rpc.a \
		mpsrc/libmpio.a

kumomergedb_LDADD = \
		storage/libkumo_storage.a \
		log/libkumo_log.a


noinst_HEADERS = \
		logic/base.h \
		logic/clock.h \
		logic/global.h \
		logic/hash.h \
		logic/rpc.h \
		logic/cluster.h \
		logic/srv.h \
		logic/srv_impl.h \
		logic/mgr.h \
		logic/mgr_impl.h \
		logic/mgr_control.h \
		logic/gw.h \
		logic/gw_impl.h \
		protocol.h \
		gateway/cloudy.h \
		gateway/memproto.h \
		gateway/memtext.h \
		gateway/gateway.h \
		storage.h

man_MANS = \
		doc/kumoctl.1 \
		doc/kumolog.1 \
		doc/kumostat.1

