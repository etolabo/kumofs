
AM_CPPFLAGS   = -I.. -I../logic
AM_C_CPPFLAGS = -I.. -I../logic

bin_PROGRAMS = kumomergedb
bin_SCRIPTS = kumoctl kumostat kumotop kumolog kumohash

EXTRA_DIST = $(bin_SCRIPTS)

kumomergedb_SOURCES = \
		mergedb.cc

kumomergedb_LDADD = \
		../storage/libkumo_storage.a \
		../log/libkumo_log.a

