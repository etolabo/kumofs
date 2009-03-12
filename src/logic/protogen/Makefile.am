
EXTRA_DIST = protogen

.PHONY: all
all:
	$(RUBY) protogen ../manager.proto.h
	$(RUBY) protogen ../server.proto.h
	$(RUBY) protogen ../gateway.proto.h

