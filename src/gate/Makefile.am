AM_CPPFLAGS   = -I..
AM_C_CPPFLAGS = -I..

noinst_LIBRARIES = libkumo_gate.a

libkumo_gate_a_SOURCES = \
		memproto/memproto.c \
		memproto/memtext.c \
		memcache_binary.cc \
		memcache_text.cc \
		cloudy.cc

noinst_HEADERS = \
		memproto/memproto.h \
		memproto/memtext.h \
		interface.h \
		memcache_binary.h \
		memcache_text.h \
		cloudy.h

EXTRA_DIST = \
		memproto/memtext.rl

