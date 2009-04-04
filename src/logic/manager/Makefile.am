
PROTOGEN_AUTOGEN_FILES = \
		proto.h \
		mod_control.h \
		mod_network.h \
		mod_replace.h

proto.generated: ../protogen ../manager.proto.h
	$(RUBY) ../protogen ../manager.proto.h
	touch $@

$(PROTOGEN_AUTOGEN_FILES): proto.generated

nodist_noinst_HEADERS = \
		$(PROTOGEN_AUTOGEN_FILES)

MOSTLYCLEANFILES = \
		$(PROTOGEN_AUTOGEN_FILES) \
		proto.generated

