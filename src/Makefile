CFLAGS=-O0 -g -Wall -Wno-unused-function -D_POSIX_C_SOURCE=200809 -std=c11
PYTHON=python
LEX=flex

FRONTEND_HEADERS = parser.h ast.h chash.h
FRONTEND_GENSRC = parser.c lexer.l lexer.c unparser.c
FRONTEND_OBJS = parser.o lexer.o ast.o unparser.o chash.o
FRONTEND = $(FRONTEND_HEADERS) $(FRONTEND_OBJS) 

BACKEND_HEADERS = assembler.h
BACKEND_GENSRC = assembler.c
BACKEND_OBJS = assembler.o
BACKEND = $(BACKEND_HEADERS) $(BACKEND_OBJS) 

.PHONY: clean

# ========================================

all: parse-test

clean:
	rm -f $(FRONTEND_OBJS) $(BACKEND_OBJS) $(FRONTEND_GENSRC) $(BACKEND_GENSRC)

parse-test: $(FRONTEND) parse-test.o
	$(CC) $(CFLAGS) $(FRONTEND_OBJS) parse-test.o -o parse-test

# ========================================
# Generated code

assembler.c: mk-codegen.py
	${PYTHON} mk-codegen.py code > assembler.c

assembler.h: mk-codegen.py
	${PYTHON} mk-codegen.py headers > assembler.h

lexer.l: mk-parser.py lexer.template.l
	${PYTHON} mk-parser.py lexer.l > lexer.l

lexer.c: lexer.l
	$(LEX) $(LFLAGS) -o $@ $^

ast.h: mk-parser.py ast.template.h
	${PYTHON} mk-parser.py ast.h > ast.h

parser.c: mk-parser.py parser.template.c
	${PYTHON} mk-parser.py parser.c > parser.c

parser.h: mk-parser.py parser.template.h
	${PYTHON} mk-parser.py parser.h > parser.h

unparser.c: mk-parser.py unparser.template.c
	${PYTHON} mk-parser.py unparser.c > unparser.c


