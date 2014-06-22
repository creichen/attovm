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
#include "runtime.h"
#include "symbol-table.h"

extern FILE *builtin_print_redirection; // builtins.c

static int failures = 0;
static int runs = 0;


void
fail(char *msg)
{
	fprintf(stderr, "Critical failure: %s\n", msg);
	exit(1);
}

static runtime_image_t *
compile(char *src, int line)
{
	FILE *memfile = fmemopen(src, strlen(src), "r");
	parser_restart(memfile);
	ast_node_t *root;
	if (!parse_program(&root)) {
		fprintf(stderr, "NOPARSE");
		fclose(memfile);
		return NULL;
	}
	fclose(memfile);
	runtime_image_t *img = runtime_prepare(root, RUNTIME_ACTION_COMPILE);
	ast_node_dump(stderr, img->ast, AST_NODE_DUMP_FORMATTED | AST_NODE_DUMP_ADDRESS | AST_NODE_DUMP_FLAGS);
	return img;
}

void
signal_failure()
{
	 ++failures;
	 printf("\033[1;31mFAILURE\033[0m\n");
}

void
signal_success()
{
	 printf("\033[1;32mOK\033[0m\n");
}

void
start_dynamic()
{
}

void
test_program(char *source, char *expected_result, int line)
{
	++runs;
	builtins_init();
	printf("[L%d] Testing: \t", line);
	runtime_image_t *image = compile(source, line);
	if (!image) {
		signal_failure();
		return;
	}

	const int output_buf_len = 4096 + strlen(expected_result);
	char output_buf[output_buf_len + 1];
	memset(output_buf, 0, output_buf_len + 1);
	FILE *writefile = fmemopen(output_buf, output_buf_len, "w");

	buffer_disassemble(image->code_buffer);	// for completeness for now...

	builtin_print_redirection = writefile;
	runtime_execute(image); // Engage!
	builtin_print_redirection = NULL;

	fflush(NULL); // Outputs might not be written until we flush
	if (strcmp(expected_result, output_buf)) {
		signal_failure();
		fprintf(stderr, "[L%d] Result mismatch:\n----- Expected:\n%s\n----- Actual:\n%s\n-----\n", line, expected_result, output_buf);
		size_t expected_len = strlen(expected_result);
		size_t output_len = strlen(output_buf);
		if (expected_len != output_len) {
			fprintf(stderr, "Output size mismatch: Expected %zu, got %zu\n", expected_len, output_len);
		}
		size_t len = output_len < expected_len? output_len : expected_len;
		for (int i = 0; i < len; i++) {
			if (output_buf[i] != expected_result[i]) {
				fprintf(stderr, "First mismatch at offset %d: expected %02x/`%c', got %02x/`%c'\n",
					i,
					expected_result[i], expected_result[i], 
					output_buf[i], output_buf[i]);
				break;
			}
		}

		buffer_disassemble(image->code_buffer);
		fflush(NULL);
		fprintf(stderr, "[L%d] From program `%s'\n", line, source);
	} else {
		signal_success();
	}

	runtime_free(image);
	fclose(writefile);
}

#define TEST(program, expected) test_program(program, expected, __LINE__);

int
main(int argc, char **argv)
{
//#define ALL
#ifdef ALL
	TEST("print(1);", "1\n");
	TEST("print(3+4);", "7\n");
	TEST("print(3+4+1);", "8\n");
	TEST("print(4-1);", "3\n");
	TEST("print(4*6);", "24\n");
	TEST("print(7/2);", "3\n");
	TEST("print(((2*3)+(1+1))*((3+2)-(2+1)));", "16\n");
	TEST("{print(1);print(2);}", "1\n2\n");
	TEST("{ int x = 17; print(x); }", "17\n");
	TEST("{ int x = 17; print(x); x := 3; print(x*x+1); }", "17\n10\n");
	TEST("{ obj x = 17; print(x); }", "17\n");
	TEST("{ obj x = 17; print(x); x := 3; print(x*x+1); }", "17\n10\n");
	TEST("{ var x = 17; print(x); }", "17\n");
	TEST("{ var x = 17; print(x); x := 3; print(x*x+1); }", "17\n10\n");

	TEST("{ if (1) print(\"1\"); print(2);}", "1\n2\n");
	TEST("{ if (0) {print(\"0\");} print(2);}", "2\n");
	TEST("{ if (0) {print(\"0\");} else {print(\"1\");} print(2);}", "1\n2\n");

	TEST("{ if (1) print(\"0\"); else print(\"1\"); print(2);}", "0\n2\n");
	TEST("{ if (not 0) print(\"0\"); else print(\"1\"); print(2);}", "0\n2\n");
	TEST("{ if (not 1) print(\"0\"); else print(\"1\"); print(2);}", "1\n2\n");

	TEST("{ if (1 > 2) print(\"0\"); else print(\"1\"); print(2);}", "1\n2\n");
	TEST("{ if (1 >= 2) print(\"0\"); else print(\"1\"); print(2);}", "1\n2\n");
	TEST("{ if (2 > 2) print(\"0\"); else print(\"1\"); print(2);}", "1\n2\n");
	TEST("{ if (2 >= 2) print(\"0\"); else print(\"1\"); print(2);}", "0\n2\n");
	TEST("{ if (3 > 2) print(\"0\"); else print(\"1\"); print(2);}", "0\n2\n");
	TEST("{ if (3 >= 2) print(\"0\"); else print(\"1\"); print(2);}", "0\n2\n");

	TEST("{ if (1 < 2) print(\"0\"); else print(\"1\"); print(2);}", "0\n2\n");
	TEST("{ if (1 <= 2) print(\"0\"); else print(\"1\"); print(2);}", "0\n2\n");
	TEST("{ if (2 < 2) print(\"0\"); else print(\"1\"); print(2);}", "1\n2\n");
	TEST("{ if (2 <= 2) print(\"0\"); else print(\"1\"); print(2);}", "0\n2\n");
	TEST("{ if (3 < 2) print(\"0\"); else print(\"1\"); print(2);}", "1\n2\n");
	TEST("{ if (3 <= 2) print(\"0\"); else print(\"1\"); print(2);}", "1\n2\n");

	TEST("{ if (0 == \"foo\") print(\"0\"); print(2);}", "2\n");
	TEST("{ if (\"bar\" == \"foo\") print(\"0\"); print(2);}", "2\n");
	TEST("{ if (\"bar\" == \"bar\") print(\"0\"); print(2);}", "0\n2\n");
	TEST("{ obj v = 0; if (0 == v) print(\"0\"); print(2);}", "0\n2\n");
	TEST("{ obj v = 0; if (v == 0) print(\"0\"); print(2);}", "0\n2\n");
	TEST("{ obj v = 0; if (1 == v) print(\"0\"); print(2);}", "2\n");
	TEST("{ obj v = 0; if (v == 1) print(\"0\"); print(2);}", "2\n");
	TEST("{ obj v = 0; obj v2 = 0; if (v2 == v) print(\"0\"); print(2);}", "0\n2\n");
	TEST("{ obj v = 0; obj v2 = 1; if (v2 == v) print(\"0\"); print(2);}", "2\n");

	TEST("{ int x = 0; while(x < 3) { print(x); x := x + 1; } } ", "0\n1\n2\n");
	//TEST("{ int x = 0; do { print(x); x := x + 1; } while(x < 3);} ", "0\n1\n2\n");
	TEST("{ int x = 0; while (x < 0) { print(x); x := x + 1; } } ", "");
	//TEST("{ int x = 0; do { print(x); x := x + 1; } while(x < 0);} ", "0\n");

	TEST("{ int x = 10; while(x > 4) { x := x - 1; if (x == 7) continue; print(x); } } ", "9\n8\n6\n5\n4\n");
	//TEST("{ int x = 10; do { x := x - 1; if (x == 7) continue; print(x); } while(x > 4);} ", "9\n8\n6\n5\n4\n");
	TEST("{ int x = 10; while(x > 4) { x := x - 1; if (x == 7) break; print(x); } } ", "9\n8\n");
	//TEST("{ int x = 10; do { x := x - 1; if (x == 7) break; print(x); } while(x > 4);} ", "9\n8");

	TEST("{ int x = 0; int y; while(x < 5) { y := x; x := x + 1; while (y < 4) { y := y + 1; if (y == 2) continue; print(y); } if (x == 3) break; } } ", "1\n3\n4\n3\n4\n3\n4\n");
	TEST("{ int x = 0; int y; while(x < 3) { y := x; x := x + 1; while (y < 5) { if (y == 3) break; print(y); y := y + 1; } if (x == 2) continue; print(\"+\");} } ", "0\n1\n2\n+\n1\n2\n2\n+\n");

	TEST("{ obj a = [1]; print(a[0]); }", "1\n");
	TEST("{ obj a = [1,7]; print(a[1]); print(a[0]);}", "7\n1\n");
	TEST("{ obj a = [1,[3,7]]; print(a[1][0]); print(a[0]);}", "3\n1\n");
	TEST("{ obj a = [1,7]; print(a[1]); a[1] := 2; print(a[1]); print(a[0]); }", "7\n2\n1\n");
	TEST("{ obj a = [1,\"foo\", /5]; print(a[1]); a[4] := 2; print(a[0]); print(a[4]); }", "foo\n1\n2\n");
	// next: NULL literal
	TEST("if (NULL == NULL) { print(\"null\"); }", "null\n");
	TEST("if (NULL == \"\") { print(\"null\"); }", "");
	TEST("if (NULL == 1) { print(\"null\"); }", "");

	// next: `is'
	TEST("if (1 is int) { print(\"1\"); }", "1\n");
	TEST("if (\"x\" is int) { print(\"1\"); }", "");
	TEST("if (\"x\" is string) { print(\"1\"); }", "1\n");
	TEST("if (\"x\" is int) { print(\"1\"); }", "");
	TEST("if (NULL is int) { print(\"1\"); }", "");
	TEST("if (NULL is string) { print(\"1\"); }", "");
#endif



	// next: functions
	// next: object instance creation
	// next: field read/write (outside)
	// next: nontrivial constructor (field init)
	// next: method call
	// next: method call within class
	// next: field access within class

	if (!failures) {
		printf("All %d tests succeeded\n", runs);
	} else {
		printf("%d of %d tests failed\n", failures, runs);
	}
	return failures > 0;
}
