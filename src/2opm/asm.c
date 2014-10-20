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

#include <stdbool.h>
#include <assert.h>
#include <limits.h>
#include <stdarg.h>

#include "../assembler-buffer.h"
#include "../chash.h"

#include "assembler-instructions.h"
#include "asm.h"

#define MAX_ASM_ARGS 5
#define DATA_SECTION_SIZE (1024 * 1024)

int yylex();
yylval_t yylval;
extern int yy_line_nr;

int errors_nr = 0;
int warnings_nr = 0;

void
error(const char *fmt, ...)
{
	va_list args;
	fprintf(stderr, "[line %d] error: ");
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
	++errors_nr;
}

void
warn(const char *fmt, ...)
{
	va_list args;
	fprintf(stderr, "[line %d] warning: ");
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
	++warnings_nr;
}

void
yyerror(const char *str)
{
	error("%s", str);
}

int data_offset = 0;
buffer_t text_buffer;
char data_section[DATA_SECTION_SIZE];
bool in_text_section = false; // which section are we operating on?
int data_offset = 0;

void *
current_offset()
{
	if (in_text_section) {
		return buffer_target(text_buffer);
	} else {
		return &(data_section[data_offset]);
	}
}


struct {
	void *offset;
	bool text_section;
	hashtable_t *label_references;	// contains mallocd label_t* s
	hashtable_t *gp_references;	// contains sint32* s
};
static hashtable_t *labels_table;

void init()
{
	data_offset = 0;
	text_buffer = buffer_new(1024);
	data_section = malloc(1);
	in_text_section = false;
	labels_table = hashtable_alloc(hashtable_pointer_hash, hashtable_pointer_compare, 4);
}

static void *
alloc_data_in_section(size_t size)
{
	if (in_text_section) {
		return buffer_alloc(text_buffer, size);
	} else {
		void *retval = &(data_section[data_offset]);
		data_offset += size;
		if (data_offset >= DATA_SECTION_SIZE) {
			error("Allocated too much data (maximum data section size is %d)", DATA_SECTION_SIZE);
			exit(1);
		}
		return retval;
	}
}

static void
add_label(char *name, void *offset)
{
	if (hashtable_get(labels_table, name)) {
		error("Multiple definitions of label `%s'", name);
		return;
	}
	hashtable_put(labels_table, name, offset);
}

static label_t *
get_label(char *name)
{
	hashtable_get(labels
}

static void
try_emit_insn(char *insn, int args_nr, int args_types, asm_arg *args)
{
	...
}

/* #define INSN_MODE_EXPECT_END		1 */
/* #define INSN_MODE_EXPECT_ARG		2 */
/* #define INSN_MODE_EXPECT_DISPLACEMENT	3 */
/* #define INSN_MODE_EXPECT_UNDISPLACEMENT	4 */

static char *
describe_insn_mode(int mode)
{
	switch (mode) {
	case INSN_MODE_EXPECT_END:
		return "Expecting end of instruction or comma";
	case INSN_MODE_EXPECT_ARG:
		return "Expecting argument after comma";
	case INSN_MODE_EXPECT_DISPLACEMENT:
		return "Expecting argument after '('";
	case INSN_MODE_EXPECT_UNDISPLACEMENT:
		return "Expecting closing ')'";
	default:
		return "?";
	}
}

static void
check_bounds(int type, signed long long v, bool is_signed)
{
	signed long long lower = 0, upper = 0;
	char *mode = "";

	switch (type) {
	case ASM_ARG_IMM8U:
		mode = "unsigned 8 bit";
		lower = 0;
		upper = UCHAR_MAX;
		break;

	case ASM_ARG_IMM32U:
		mode = "unsigned 32 bit";
		lower = 0;
		upper = UINT_MAX;
		break;

	case ASM_ARG_IMM32S:
		mode = "signed 32 bit";
		lower = INT_MIN;
		upper = INT_MAX;
		break;

	case ASM_ARG_IMM64S:
		if (!is_signed && v < 0) {
			warn("Number %llu (%llx) is too large for signed 64 bit argument",
			     (unsigned long long) v,
			     (unsigned long long) v);
		}
		return;

	case ASM_ARG_IMM64U:
		if (is_signed) {
			warn("Number %lld is negative, can't be represented as unsigned 64 bit argument",
			     v);
		}
		return;

	default:
		return;
	}

	if (v < lower || v > upper) {
		warn("%lld is out of bounds [%lld-%lld] for %s", v, lower, upper, mode);
	}
}

static int
parse_register()
{
	return -1;
}

static void
parse_insn(char *insn, int next_token)
{
	asm_arg args[MAX_ASM_ARGS];
	int args_types[MAX_ASM_ARGS];
	int args_nr = 0;
	int mode = INSN_MODE_EXPECT_END;
	
	switch (next_token) {

	case '$':
		args_types[args_nr] = ASM_ARG_REG;
		args[args_nr].r = parse_register();
		if (args[args_nr].r == -1) {
			return;
		}
		++args_nr;
		break;

	case T_ID:
		args_types[args_nr] = ASM_ARG_LABEL;
		args[args_nr++].label = get_label(yylval.str);
		break;

	case T_INT:
		args_types[args_nr] = ASM_ARG_IMM64S;
		args[args_nr++].imm = yylval.num;
		break;

	case T_UINT:
		args_types[args_nr] = ASM_ARG_IMM64U;
		args[args_nr++].imm = yylval.num;
		break;

	case ',':
		if (args_nr >= MAX_ASM_ARGS) {
			error("Too many arguments to instruction");
			return;
		}
		mode = INSN_MODE_EXPECT_ARG;
		break;

	case ')':
		if (mode == INSN_MODE_EXPECT_UNDISPLACEMENT) {
			mode = INSN_MODE_EXPECT_END;
		}
		break;

	case '(':
		if (args_nr == MAX_ASM_ARGS) {
			error("Too many arguments to instruction");
			return;
		}

		if (mode == INSN_MODE_EXPECT_END && args_nr > 0) {
			mode = INSN_MODE_EXPECT_DISPLACEMENT;
		} else {
			error("Unexpected parenthesis");
		}
		break;

	case '\n':
		if (mode == INSN_MODE_EXPECT_END) {
			try_emit_insn(insn, args_nr, args_types, args);
		} else {
			error(describe_insn_mode(mode));
		}
		return;

	default:
		error("Parse error in assembly instruction `%s'", insn);
	}
}

static void
parse()
{
	int data_mode = 0; // Bulk data input expected
	int input;
	while ((input == yylex())) {
		switch (input) {
		case T_ID: {
			data_mode = 0;
			char *id = yylval.str;
			int next = yylex();
			if (next == ':') {
				add_label(id, current_offset());
			} else {
				if (!in_text_section) {
					error("Assembly instructions not supported in .data section");
					return;
				}
				parse_insn(id, next);
			}
			break;
		}

		case T_S_DATA:
			in_text_section = false;
			data_mode = 0;
			break;

		case T_S_TEXT:
			in_text_section = true;
			data_mode = 0;
			break;

		case T_S_BYTES:
		case T_S_WORDS:
		case T_S_ASCIIZ:
			data_mode = input;
			break;

		case '\n':
			break;
		case ',':
			if (!data_mode) {
				error("Stray comma");
			}
			break;

		case T_INT: {
			switch (data_mode) {
			case T_S_BYTES:
				((unsigned char *)alloc_data_in_section(1))[0] = yylval.num;
				break;
			case T_S_WORDS:
				memcpy(alloc_data_in_section(8), &yylval.num, 8);
				break;
			case T_S_ASCIIZ:
			default:
				error("Unexpected raw integer data");
				return;
			}
			break;
		}

		case T_STR: {
			switch (data_mode) {
			case T_S_ASCIIZ:
				memcpy(alloc_data_in_section(8), &yylval.str, strlen(yylval.str) + 1);
				break;
			case T_S_BYTES:
			case T_S_WORDS:
			default:
				error("Unexpected asciiz strings");
				return;
			}
			break;
		}

		default:
			error("Unexpected control character `%c'\n", input);
			return;
		}
	}
}

static void
parse_file(char *filename)
{
	yyin = fopen(filename, "r");
	if (!yyin) {
		perror(filename);
		return;
	}

	parse();
}



int
main(int argc, char **argv)
{
}
