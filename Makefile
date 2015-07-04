VERSION=`cat src/version.h | grep define | awk '{print $$4}' | tr -d '"'`
DISTFILES=`echo Makefile; (cd src; make echo-distfiles) | tr ' ' '\n' | awk '/DIST-END/ {exit 0;} {if (started) {print "src/"$$1;}} /DIST-START/ {started = 1}'`
DISTFILES_EXTRA=doc/asm-docs.tex doc/asm-docs.de.tex doc/overview.tex doc/overview.de.tex

.PHONY : all test clean

atl: src/*.c src/*.h src/*.py src/*.l
	(cd src; make atl && cp atl ..)

all: atl docs

docs: doc/asm-docs.pdf doc/overview.pdf

doc/asm-docs.pdf: doc/asm-docs.tex doc/2opm.sty
	(cd doc; pdflatex asm-docs.tex; pdflatex asm-docs.tex)

doc/overview.pdf: doc/overview.tex
	(cd doc; pdflatex overview.tex; pdflatex overview.tex)

clean:
	rm -f atl
	(cd src; make clean)

test:
	(cd src; make test)

dist:
	sh localizescript.sh ${VERSION} ${DISTFILES} ${DISTFILES_EXTRA}

%.pdf : %.tex
	cp doc/2opm.sty . # hack
	pdflatex $<
