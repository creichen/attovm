.PHONY : all test clean

all: atl

atl: src/*.c src/*.h src/*.py src/*.l
	(cd src; make atl && cp atl ..)

clean:
	rm -f atl
	(cd src; make clean)

test:
	(cd src; make backend-test)
