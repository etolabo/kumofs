
PROTOGEN_AUTOGEN_FILES = \
		manager/proto.h \
		manager/proto_control.h \
		manager/proto_network.h \
		manager/proto_replace.h

proto.generated: ../protogen ../manager.proto.h
	$(RUBY) ../protogen ../manager.proto.h
	touch $@

$(PROTOGEN_AUTOGEN_FILES): proto.generated

nodist_noinst_HEADERS = \
		$(PROTOGEN_AUTOGEN_FILES)

MOSTLYCLEANFILES = \
		$(PROTOGEN_AUTOGEN_FILES) \
		proto.generated

