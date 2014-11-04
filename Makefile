VERSION=`cat src/version.h | grep define | awk '{print $$4}' | tr -d '"'`
DISTFILES=`echo Makefile; (cd src; make echo-distfiles) | tr ' ' '\n' | awk '/DIST-END/ {exit 0;} {if (started) {print "src/"$$1;}} /DIST-START/ {started = 1}'`

.PHONY : all test clean

all: atl

atl: src/*.c src/*.h src/*.py src/*.l
	(cd src; make atl && cp atl ..)

clean:
	rm -f atl
	(cd src; make clean)

test:
	(cd src; make backend-test; ./backend-test)

dist:
	sh localizescript.sh ${VERSION} ${DISTFILES}
