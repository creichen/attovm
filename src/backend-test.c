/***************************************************************************
  Copyright (C) 2014 Christoph Reichenbach


 This program may be modified and copied freely according to the terms of
 the GNU general public license (GPL), as long as the above copyright
 notice and the licensing information contained herein are preserved.

 Please refer to www.gnu.org for licensing details.

 This work is provided AS IS, without warranty of any kind, expressed or
 implied, including but not limited to the warranties of merchantability,
 noninfringement, and fitness for a specific purpose. The author will not
 be held liable for any damage caused by this work or derivatives of it.

 By using this source code, you agree to the licensing terms as stated
 above.


 Please contact the maintainer for bug reports or inquiries.

 Current Maintainer:

    Christoph Reichenbach (CR) <creichen@gmail.com>

***************************************************************************/

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "ast.h"
#include "assembler.h"
#include "baseline-backend.h"
#include "class.h"
#include "analysis.h"
#include "object.h"
#include "parser.h"
#include "registers.h"
#include "symbol-table.h"

extern FILE *builtin_print_redirection; // builtins.c
extern FILE *yyin; // lexer

void *builtin_op_print(object_t *arg);  // builtins.c

static int failures = 0;

void
fail(char *msg)
{
	fprintf(stderr, "Critical failure: %s\n", msg);
	exit(1);
}

void
test_expr(char *source, char *expected_result, int line)
{
	const int output_buf_len = 4096 + strlen(expected_result);
	char output_buf[output_buf_len + 1];
	memset(output_buf, 0, output_buf_len + 1);
	FILE *writefile = fmemopen(output_buf, output_buf_len, "w");
	builtin_print_redirection = writefile;
	FILE *memfile = fmemopen(source, strlen(source), "r");
	yyin = memfile;

	ast_node_t *root;
	if (!parse_expr(&root)) {
		fprintf(stderr, "[%d] Parse error in `%s'\n", line, source);
		++failures;
		return;
	}
	name_analysis(root);
	if (name_analysis_errors()) {
		fprintf(stderr, "[%d] Name analysis failures in `%s'\n", line, source);
		++failures;
		return;
	}

	buffer_t outbuf = buffer_new(1024);
	baseline_compile_expr(&outbuf, registers_argument_nr[0], root, NULL, 0);
	emit_li(&outbuf, REGISTER_V0, (long long int) new_int);
	emit_jalr(&outbuf, REGISTER_V0);
	emit_move(&outbuf, registers_argument_nr[0], REGISTER_V0);
	emit_li(&outbuf, REGISTER_V0, (long long int) builtin_op_print);
	emit_jalr(&outbuf, REGISTER_V0);
	emit_jreturn(&outbuf);
	buffer_terminate(outbuf);
	void (*f)(void);
	f = (void (*)(void)) buffer_entrypoint(outbuf);
	buffer_disassemble(outbuf);	// for completeness for now...
	fprintf(stderr, "START\n");
	f();
	fprintf(stderr, "DONE\n");
	if (strcmp(expected_result, output_buf)) {
		fprintf(stderr, "[%d] Result mismatch:\n\tExpected: `%s'\n\tActual:  `%s'", line, expected_result, output_buf);
		buffer_disassemble(outbuf);
		fflush(NULL);
		fprintf(stderr, "[%d] From expression `%s'\n", line, source);
		++failures;
		return;
	} else {
		printf("OK\n");
	}

	ast_node_free(root, 1);
	fclose(memfile);
	fclose(writefile);
	builtin_print_redirection = NULL;
}

int
main(int argc, char **argv)
{
	builtins_init();
	test_expr("3 + 4", "7\n", __LINE__);
}
