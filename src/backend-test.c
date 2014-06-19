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
	ast_node_dump(stderr, root, 0x6); 
	return runtime_compile(root);
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
	TEST("print(1);", "1\n");
	TEST("print(3+4);", "7\n");
	if (!failures) {
		printf("All %d tests succeeded\n", runs);
	} else {
		printf("%d of %d tests failed\n", failures, runs);
	}
	return failures > 0;
}
