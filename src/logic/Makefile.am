
SUBDIRS = manager server gateway

noinst_LIBRARIES = libkumo_logic.a
bin_PROGRAMS = kumo-manager kumo-server kumo-gateway

AM_CPPFLAGS   = -I.. -DREVISION='"$(REVISION)"'
AM_C_CPPFLAGS = -I.. -DREVISION='"$(REVISION)"'


libkumo_logic_a_SOURCES = \
		boot.cc \
		hash.cc \
		wavy_server.cc


kumo_manager_SOURCES = \
		manager/framework.cc \
		manager/main.cc \
		manager/mod_control.cc \
		manager/mod_network.cc \
		manager/mod_replace.cc

kumo_manager_LDADD  = \
		libkumo_logic.a \
		../kazuhiki/libkazuhiki.a \
		../rpc/libkumo_cluster.a \
		../log/libkumo_log.a \
		../mpsrc/libmpio.a


kumo_server_SOURCES = \
		server/framework.cc \
		server/main.cc \
		server/zmmap_stream.cc \
		server/mod_control.cc \
		server/mod_network.cc \
		server/mod_replace.cc \
		server/mod_replace_stream.cc \
		server/mod_store.cc

kumo_server_LDADD  = \
		libkumo_logic.a \
		../storage/libkumo_storage.a \
		../kazuhiki/libkazuhiki.a \
		../rpc/libkumo_cluster.a \
		../log/libkumo_log.a \
		../mpsrc/libmpio.a


kumo_gateway_SOURCES = \
		gateway/framework.cc \
		gateway/gate.cc \
		gateway/main.cc \
		gateway/mod_network.cc \
		gateway/mod_cache.cc \
		gateway/mod_store.cc

kumo_gateway_LDADD  = \
		libkumo_logic.a \
		../gate/libkumo_gate.a \
		../kazuhiki/libkazuhiki.a \
		../log/libkumo_log.a \
		../rpc/libkumo_rpc.a \
		../mpsrc/libmpio.a


noinst_HEADERS = \
		server/proto.h \
		gateway/proto.h \
		manager/proto.h \
		boot.h \
		client_logic.h \
		clock.h \
		clock_logic.h \
		cluster_logic.h \
		global.h \
		hash.h \
		msgtype.h \
		role.h \
		rpc_server.h \
		wavy_server.h \
		manager/framework.h \
		manager/init.h \
		server/framework.h \
		server/init.h \
		server/zmmap_stream.h \
		server/zconnection.h \
		gateway/framework.h \
		gateway/init.h \
		gateway/mod_cache.h \
		gateway/mod_store.h

EXTRA_DIST = \
		protogen \
		gateway.proto.h \
		manager.proto.h \
		server.proto.h


# work around for duplicated file name
kumo_manager_CFLAGS = $(AM_CFLAGS)
kumo_manager_CXXFLAGS = $(AM_CXXFLAGS)
kumo_server_CFLAGS = $(AM_CFLAGS)
kumo_server_CXXFLAGS = $(AM_CXXFLAGS)
kumo_gateway_CFLAGS = $(AM_CFLAGS)
kumo_gateway_CXXFLAGS = $(AM_CXXFLAGS)


