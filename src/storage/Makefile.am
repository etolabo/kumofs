AM_CPPFLAGS   = -I..
AM_C_CPPFLAGS = -I..

noinst_LIBRARIES = libkumo_storage.a

if STORAGE_TCHDB
libkumo_storage_a_SOURCES = storage.cc tchdb.cc
endif

if STORAGE_TCADB
libkumo_storage_a_SOURCES = storage.cc tcadb.cc
endif

if STORAGE_TCBDB
libkumo_storage_a_SOURCES = storage.cc tcbdb.cc
endif

if STORAGE_LUXIO
libkumo_storage_a_SOURCES = storage.cc luxio.cc
endif

noinst_HEADERS = \
		buffer_queue.h \
		storage.h \
		interface.h

