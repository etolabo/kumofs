#SRCS=$(wildcard *txt)
#default: $(SRCS:.txt=.pdf) $(SRCS:.txt=.html)
#clean: $(SRCS:.txt=.clean)
#distclean: $(SRCS:.txt=.distclean)

default: memo.pdf memo.html kumoctl.1 kumostat.1 kumotop.1 kumolog.1
clean: memo.clean
distclean: memo.distclean


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

%.1 : %.1.txt
	wfdoc -f man $< -o $@

