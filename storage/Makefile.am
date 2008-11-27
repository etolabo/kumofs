
noinst_LIBRARIES = libkumo_storage.a

if USE_TOKYOCABINET
libkumo_storage_a_SOURCES = tokyocabinet.cc
else
libkumo_storage_a_SOURCES = luxio.cc
endif

noinst_HEADERS = \
	luxio.h \
	tokyocabinet.h

