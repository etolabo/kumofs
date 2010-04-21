
REVISION = $(shell git show --pretty=format:'%T %ad' | head -n 1)
export REVISION

SUBDIRS = src

DOC_FILES = \
		README.md \
		NOTICE \
		doc/kumo-gateway.1.txt \
		doc/kumo-manager.1.txt \
		doc/kumo-server.1.txt \
		doc/kumoctl.1.txt \
		doc/kumohash.1.txt \
		doc/kumolog.1.txt \
		doc/kumomergedb.1.txt \
		doc/kumostat.1.txt \
		doc/kumotop.1.txt \
		doc/kumo-gateway.1 \
		doc/kumo-manager.1 \
		doc/kumo-server.1 \
		doc/kumoctl.1 \
		doc/kumohash.1 \
		doc/kumolog.1 \
		doc/kumomergedb.1 \
		doc/kumostat.1 \
		doc/kumotop.1 \
		doc/doc.en.md \
		doc/doc.ja.md


EXTRA_DIST = $(DOC_FILES)

man_MANS = \
		doc/kumo-gateway.1 \
		doc/kumo-manager.1 \
		doc/kumo-server.1 \
		doc/kumoctl.1 \
		doc/kumohash.1 \
		doc/kumolog.1 \
		doc/kumomergedb.1 \
		doc/kumostat.1 \
		doc/kumotop.1

