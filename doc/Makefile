SRCS=$(wildcard *txt)
default: $(SRCS:.txt=.pdf) $(SRCS:.txt=.html)
clean: $(SRCS:.txt=.clean)
distclean: $(SRCS:.txt=.distclean)

%.sdoc : %.txt
	wfdoc -i $< -o $@

%.html : %.txt
	wfdoc -f html -i $< -o $@

%.tex : %.sdoc
	sdoc -toc -format:latex2e -latex2e.driver=dvipdfm -latex2e.ref=HyperRefLaTeX2eRefHandler $<

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
	rm -f $*.sdoc $*.tex $*.etex $*.dvi $*.toc $*.aux $*.out $*.log $*.pdf

%.distclean : %.txt
	rm -f $*.sdoc $*.tex $*.etex $*.dvi $*.toc $*.aux $*.out $*.log $*.pdf $*.mk

