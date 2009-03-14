export ERB
export RUBY
export RAGEL

# work around for dependency
SUBDIRS = manager server gateway

noinst_LIBRARIES = libkumo_logic.a
bin_PROGRAMS = kumo-manager kumo-server kumo-gateway

AM_CPPFLAGS   = -I..
AM_C_CPPFLAGS = -I..


libkumo_logic_a_SOURCES = \
		boot.cc \
		hash.cc \
		wavy_server.cc


kumo_manager_SOURCES = \
		manager/framework.cc \
		manager/main.cc \
		manager/proto_control.cc \
		manager/proto_network.cc \
		manager/proto_replace.cc

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
		server/proto_control.cc \
		server/proto_network.cc \
		server/proto_replace.cc \
		server/proto_replace_stream.cc \
		server/proto_store.cc

kumo_server_LDADD  = \
		libkumo_logic.a \
		../storage/libkumo_storage.a \
		../kazuhiki/libkazuhiki.a \
		../rpc/libkumo_cluster.a \
		../log/libkumo_log.a \
		../mpsrc/libmpio.a


kumo_gateway_SOURCES = \
		gateway/framework.cc \
		gateway/interface.cc \
		gateway/main.cc \
		gateway/proto_network.cc \
		gateway/scope_store.cc \
		gateway/gate_cloudy.cc \
		gateway/gate_memproto.cc \
		gateway/gate_memtext.cc \
		gateway/memproto/memproto.c \
		gateway/memproto/memtext.c

kumo_gateway_LDADD  = \
		libkumo_logic.a \
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
		gateway/interface.h \
		gateway/scope_store.h \
		gateway/gate_cloudy.h \
		gateway/gate_memproto.h \
		gateway/gate_memtext.h \
		gateway/memproto/memproto.h \
		gateway/memproto/memtext.h

EXTRA_DIST = \
		protogen \
		gateway.proto.h \
		manager.proto.h \
		server.proto.h \
		gateway/memproto/memtext.rl

gateway/memproto/memtext.c: gateway/memproto/memtext.rl
	$(RAGEL) -C $< -o $@.tmp
	mv $@.tmp $@

MOSTLYCLEANFILES = \
		gateway/memproto/memtext.c


# work around for duplicated file name
kumo_manager_CFLAGS = $(AM_CFLAGS)
kumo_manager_CXXFLAGS = $(AM_CXXFLAGS)
kumo_server_CFLAGS = $(AM_CFLAGS)
kumo_server_CXXFLAGS = $(AM_CXXFLAGS)
kumo_gateway_CFLAGS = $(AM_CFLAGS)
kumo_gateway_CXXFLAGS = $(AM_CXXFLAGS)


