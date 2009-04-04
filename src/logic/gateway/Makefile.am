
PROTOGEN_AUTOGEN_FILES = \
		proto.h \
		mod_network.h

proto.generated: ../protogen ../gateway.proto.h
	$(RUBY) ../protogen ../gateway.proto.h
	touch $@

$(PROTOGEN_AUTOGEN_FILES): proto.generated

nodist_noinst_HEADERS = \
		$(PROTOGEN_AUTOGEN_FILES)

MOSTLYCLEANFILES = \
		$(PROTOGEN_AUTOGEN_FILES) \
		proto.generated

