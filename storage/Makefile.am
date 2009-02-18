
noinst_LIBRARIES = libkumo_storage.a

if USE_TOKYOCABINET
libkumo_storage_a_SOURCES = storage.cc tokyocabinet.cc
else
libkumo_storage_a_SOURCES = storage.cc luxio.cc
endif

noinst_HEADERS = \
	storage.h \
	luxio.h \
	tokyocabinet.h

