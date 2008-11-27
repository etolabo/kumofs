
nodist_noinst_DATA = memproto_text.c
dist_noinst_DATA = memproto_text.rl

noinst_LIBRARIES = libkumo_memproto.a

libkumo_memproto_a_SOURCES = \
		memproto.c \
		memproto_text.c

# FIXME GNU make extension
%.c: %.rl
	$(RAGEL) -C $< -o $@.tmp
	mv $@.tmp $@

MOSTLYCLEANFILES = memproto_text.c

noinst_HEADERS = \
		memproto.h \
		memproto_text.h

