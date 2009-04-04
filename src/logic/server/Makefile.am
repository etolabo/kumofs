
PROTOGEN_AUTOGEN_FILES = \
		proto.h \
		mod_control.h \
		mod_network.h \
		mod_replace.h \
		mod_replace_stream.h \
		mod_store.h

proto.generated: ../protogen ../server.proto.h
	$(RUBY) ../protogen ../server.proto.h
	touch $@

$(PROTOGEN_AUTOGEN_FILES): proto.generated

nodist_noinst_HEADERS = \
		$(PROTOGEN_AUTOGEN_FILES)

MOSTLYCLEANFILES = \
		$(PROTOGEN_AUTOGEN_FILES) \
		proto.generated

