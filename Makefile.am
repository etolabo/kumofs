export ERB
export RUBY
export RAGEL

REVISION=$(shell cut -d ' ' -f 2 revision)
export REVISION

SUBDIRS = src

DOC_WFDOC = \
		doc/memo.txt \
		doc/kumoctl.1.txt \
		doc/kumolog.1.txt \
		doc/kumotop.1.txt \
		doc/kumostat.1.txt \
		doc/kumomergedb.1.txt

DOC_FILES = \
		doc/memo.pdf \
		doc/memo.html \
		doc/kumoctl.1 \
		doc/kumolog.1 \
		doc/kumotop.1 \
		doc/kumostat.1 \
		doc/kumomergedb.1

EXTRA_DIST = $(DOC_WFDOC) $(DOC_FILES)

man_MANS = \
		doc/kumoctl.1 \
		doc/kumolog.1 \
		doc/kumotop.1 \
		doc/kumostat.1 \
		doc/kumomergedb.1

