
nodist_noinst_DATA = memtext.c
dist_noinst_DATA = memtext.rl

noinst_LIBRARIES = libkumo_memproto.a

libkumo_memproto_a_SOURCES = \
		memproto.c \
		memtext.c

# FIXME GNU make extension
%.c: %.rl
	$(RAGEL) -C $< -o $@.tmp
	mv $@.tmp $@

MOSTLYCLEANFILES = memtext.c

noinst_HEADERS = \
		memproto.h \
		memtext.h

