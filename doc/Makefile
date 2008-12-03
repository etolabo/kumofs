SRCS=$(wildcard *txt)
default: $(SRCS:.txt=.pdf)
clean: $(SRCS:.txt=.clean)
distclean: $(SRCS:.txt=.distclean)

%.sdoc : %.txt
	wfdoc -i $< -o $@

%.tex : %.sdoc
	sdoc -toc -format:latex2e $<

%.etex : %.tex
	wftex $< $@

%.toc : %.etex
	platex $<

%.dvi : %.etex %.toc
	platex $<

%.pdf : %.dvi
	dvipdfmx $<
	rm -f $*.aux $*.log

%.clean : %.txt
	rm -f $*.sdoc $*.tex $*.etex $*.dvi $*.toc $*.aux $*.log $*.pdf

%.distclean : %.txt
	rm -f $*.sdoc $*.tex $*.etex $*.dvi $*.toc $*.aux $*.log $*.pdf $*.mk

