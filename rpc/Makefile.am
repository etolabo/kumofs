
noinst_LIBRARIES = libkumo_rpc.a libkumo_cluster.a

libkumo_rpc_a_SOURCES = \
		address.cc \
		session.cc

libkumo_cluster_a_SOURCES = \
		$(libkumo_rpc_a_SOURCES) \
		server.cc \
		cluster.cc

noinst_HEADERS = \
		address.h \
		cluster.h \
		connection.h \
		message.h \
		protocol.h \
		sbuffer.h \
		client.h \
		client_tmpl.h \
		server.h \
		session.h \
		session_impl.h \
		transport.h \
		transport_impl.h \
		vrefbuffer.h

