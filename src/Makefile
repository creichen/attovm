#CFLAGS=-O0 -g -Wall -Wno-unused-function -Wno-unused-label -D_POSIX_C_SOURCE=200809 -std=c11
#CFLAGS=-O0 -g -Wall -Wno-unused-function -Wno-unused-label -D_POSIX_C_SOURCE=200809 -std=gnu11
#CFLAGS=-O3 -mtune=native -Wall -Wno-unused-function -Wno-unused-label -D_POSIX_C_SOURCE=200809 -std=c11
CFLAGS=-O3 -mtune=native -Wall -Wno-unused-function -Wno-unused-label -D_POSIX_C_SOURCE=200809 -std=c11
#CC=clang-3.9
PYTHON=python
FLEX=flex

# --------------------
# 2OPM stand-alone assembler
ASM_OBJS = 2opm/asm-lexer.o 2opm/assembler.o 2opm/asm.o chash.o address-store.o \
		registers.o assembler.o assembler-buffer.o lexer-support.o debugger.o \
		2opm/asm-builtins.o 2opm/asm-errors.o  2opm/asm-labels.o \
		2opm/asm-memory.o  2opm/asm-parse.o

# --------------------
# ATL frontend
FRONTEND_HEADERS = chash.h symbol-table.h analysis.h lexer-support.h errors.h compiler-options.h version.h \
		cstack.h bitvector.h control-flow-graph.h data-flow.h symint.h timer.h
FRONTEND_GENSRC = parser.c lexer.l lexer.c unparser.c ast.h parser.h
FRONTEND_SRC = parser.template.c parser.template.h ast.template.h lexer.template.l unparser.template.c \
		mk-codegen.py mk-parser.py lexer-support.c ast.c chash.c cstack.c bitvector.c symbol-table.c \
		name-analysis.c type-analysis.c atl.c backend-test.c data-flow.c control-flow-graph.c \
		data-flow-reaching-definitions.c data-flow-definite-assignments.c data-flow-precise-types.c \
		data-flow-out-of-bounds-elimination.c symint.c timer.c
FRONTEND_OBJS = parser.o lexer.o lexer-support.o ast.o unparser.o chash.o cstack.o bitvector.o symbol-table.o \
		name-analysis.o type-analysis.o data-flow.o control-flow-graph.o \
		data-flow-reaching-definitions.o data-flow-definite-assignments.o data-flow-precise-types.o \
		data-flow-out-of-bounds-elimination.o symint.o timer.o
FRONTEND = $(FRONTEND_HEADERS) $(FRONTEND_OBJS) 

# --------------------
# ATL backend
BACKEND_HEADERS = assembler-buffer.h baseline-backend.h object.h class.h registers.h runtime.h address-store.h \
		dynamic-compiler.h heap.h debugger.h stackmap.h
BACKEND_GENSRC = assembler.c assembler.h
BACKEND_SRC = assembler-buffer.c baseline-backend.c object.c class.c registers.c \
		builtins.c runtime.c address-store.c dynamic-compiler.c heap.c debugger.c stackmap.c
BACKEND_OBJS = assembler.o assembler-buffer.o baseline-backend.o object.o class.o registers.o \
		builtins.o runtime.o address-store.o dynamic-compiler.o heap.o debugger.o stackmap.o
BACKEND = $(BACKEND_HEADERS) $(BACKEND_OBJS)

# --------------------
# ATL main objects
FINAL_SRC = atl.c containers-test.c backend-test.c assembler-buffer-test.c
FINAL_OBJS = atl.o atl containers-test.o containers-test backend-test.o backend-test assembler-buffer-test


.PHONY: clean tidy all echo-distfiles test-backend-valgrind test-backend test test-containers

# ========================================

all: atl backend-test assembler-buffer-test asm

echo-distfiles:
	echo DIST-START Makefile ${BACKEND_HEADERS} ${FRONTEND_HEADERS} ${FRONTEND_GENSRC} ${BACKEND_GENSRC} ${FRONTEND_SRC} ${BACKEND_SRC} ${FINAL_SRC} DIST-END

clean:
	rm -f $(FRONTEND_OBJS) $(BACKEND_OBJS) ${FINAL_OBJS} ${ASM_OBJS}

tidy: clean
	rm -f $(FRONTEND_GENSRC) $(BACKEND_GENSRC)

atl: ${FRONTEND_GENSRC} $(BACKEND_GENSRC) $(BACKEND) $(FRONTEND) atl.o
	$(CC) $(CFLAGS) $(FRONTEND_OBJS) ${BACKEND_OBJS} atl.o -o atl -lrt

test-frontend: all
	(cd ../test ; ./test.sh)

test-containers: containers-test
	./containers-test

backend-test: $(FRONTEND_GENSRC) $(BACKEND_GENSRC) $(BACKEND) $(FRONTEND) backend-test.o
	$(CC) $(CFLAGS) $(FRONTEND_OBJS) $(BACKEND_OBJS) backend-test.o -o backend-test -lrt

containers-test: $(FRONTEND_GENSRC) $(BACKEND_GENSRC) $(BACKEND) $(FRONTEND) containers-test.o
	$(CC) $(CFLAGS) $(FRONTEND_OBJS) $(BACKEND_OBJS) containers-test.o -o containers-test -lrt

assembler-buffer-test: $(FRONTEND_GENSRC) $(BACKEND_GENSRC) $(BACKEND) $(FRONTEND) assembler-buffer-test.o
	$(CC) $(CFLAGS) $(FRONTEND_OBJS) $(BACKEND_OBJS) assembler-buffer-test.o -o assembler-buffer-test -lrt

test-backend: backend-test
	./backend-test

test-backend-valgrind: backend-test
	valgrind --smc-check=all ./backend-test

test-assembler-buffer: assembler-buffer-test
	./assembler-buffer-test

test: test-containers test-backend

asm: ${ASM_OBJS}
	$(CC) $(CFLAGS) $(ASM_OBJS) -o asm

TAGS: ${FRONTEND_SRC} ${BACKEND_SRC} ${FINAL_SRC}
	etags -R -f TAGS *.c *.h

# ========================================
# Generated code

assembler.c: mk-codegen.py assembler.h
	${PYTHON} mk-codegen.py code > assembler.c

assembler.h: mk-codegen.py
	${PYTHON} mk-codegen.py headers > assembler.h

lexer.l: mk-parser.py lexer.template.l
	${PYTHON} mk-parser.py lexer.l > lexer.l

lexer.c: lexer.l
	if [ x`which ${FLEX}` != x ]; then $(LEX) $(LFLAGS) -o $@ $^; fi

ast.h: mk-parser.py ast.template.h
	${PYTHON} mk-parser.py ast.h > ast.h

parser.c: mk-parser.py parser.template.c parser.h ast.h
	${PYTHON} mk-parser.py parser.c > parser.c

parser.h: mk-parser.py parser.template.h
	${PYTHON} mk-parser.py parser.h > parser.h

unparser.c: mk-parser.py unparser.template.c parser.h ast.h
	${PYTHON} mk-parser.py unparser.c > unparser.c

2opm/assembler-instructions.h: mk-codegen.py
	${PYTHON} mk-codegen.py assembler-header > 2opm/assembler-instructions.h

2opm/assembler.c: mk-codegen.py 2opm/assembler-instructions.h
	${PYTHON} mk-codegen.py assembler > 2opm/assembler.c

2opm/asm-lexer.c: 2opm/asm-lexer.l
	if [ x`which ${FLEX}` != x ]; then $(LEX) $(LFLAGS) -o $@ $^; fi

