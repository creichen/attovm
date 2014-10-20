CFLAGS=-O0 -g -Wall -Wno-unused-function -Wno-unused-label -D_POSIX_C_SOURCE=200809 -std=c11
#CFLAGS=-O3 -mcpu=native -Wall -Wno-unused-function -Wno-unused-label -D_POSIX_C_SOURCE=200809 -std=c11
PYTHON=python
FLEX=flex

FRONTEND_HEADERS = chash.h symbol-table.h analysis.h lexer-support.h
FRONTEND_GENSRC = parser.c lexer.l lexer.c unparser.c ast.h parser.h
FRONTEND_OBJS = parser.o lexer.o lexer-support.o ast.o unparser.o chash.o symbol-table.o \
		name-analysis.o type-analysis.o storage-allocation.o
FRONTEND = $(FRONTEND_HEADERS) $(FRONTEND_OBJS) 

BACKEND_HEADERS = assembler-buffer.h baseline-backend.h object.h class.h registers.h runtime.h address-store.h \
		dynamic-compiler.h
BACKEND_GENSRC = assembler.c assembler.h
BACKEND_OBJS = assembler.o assembler-buffer.o baseline-backend.o object.o class.o registers.o \
		 builtins.o runtime.o address-store.o dynamic-compiler.o
FINAL_OBJS = atl.o atl backend-test.o backend-test assembler-buffer-test
BACKEND = $(BACKEND_HEADERS) $(BACKEND_OBJS)

ASM_OBJS = 2opm/assembler.o 2opm/asm.o 2opm/asm-lexer.o chash.o address-store.o \
		registers.o assembler.o assembler-buffer.o

.PHONY: clean tidy all

# ========================================

all: atl backend-test assembler-buffer-test

clean:
	rm -f $(FRONTEND_OBJS) $(BACKEND_OBJS) ${FINAL_OBJS}

tidy: clean
	rm -f $(FRONTEND_GENSRC) $(BACKEND_GENSRC)

atl: ${FRONTEND_GENSRC} $(BACKEND_GENSRC) $(BACKEND) $(FRONTEND) atl.o
	$(CC) $(CFLAGS) $(FRONTEND_OBJS) ${BACKEND_OBJS} atl.o -o atl

test-frontend: all
	(cd ../test ; ./test.sh)

backend-test: $(FRONTEND_GENSRC) $(BACKEND_GENSRC) $(BACKEND) $(FRONTEND) backend-test.o
	$(CC) $(CFLAGS) $(FRONTEND_OBJS) $(BACKEND_OBJS) backend-test.o -o backend-test

assembler-buffer-test: $(FRONTEND_GENSRC) $(BACKEND_GENSRC) $(BACKEND) $(FRONTEND) assembler-buffer-test.o
	$(CC) $(CFLAGS) $(FRONTEND_OBJS) $(BACKEND_OBJS) assembler-buffer-test.o -o assembler-buffer-test

test-backend: backend-test
	./backend-test

test-assembler-buffer: assembler-buffer-test
	./assembler-buffer-test

test: test-frontend test-backend

asm: ${ASM_OBJS}
	$(CC) $(CFLAGS) $(ASM_OBJS) -o asm

TAGS:
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

