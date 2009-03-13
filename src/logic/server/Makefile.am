
PROTOGEN_AUTOGEN_FILES = \
		server/proto.h \
		server/proto_control.h \
		server/proto_network.h \
		server/proto_replace.h \
		server/proto_replace_stream.h \
		server/proto_store.h

proto.generated: ../protogen ../server.proto.h
	$(RUBY) ../protogen ../server.proto.h
	touch $@

$(PROTOGEN_AUTOGEN_FILES): proto.generated

nodist_noinst_HEADERS = \
		$(PROTOGEN_AUTOGEN_FILES)

MOSTLYCLEANFILES = \
		$(PROTOGEN_AUTOGEN_FILES) \
		proto.generated

