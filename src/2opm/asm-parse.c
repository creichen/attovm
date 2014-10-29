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

#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "../registers.h"
#include "../address-store.h"
#include "../assembler.h"
#include "../chash.h"

#include "assembler-instructions.h"
#include "asm.h"

#define MAX_ASM_ARGS 5

int yylex();
yylval_t yylval;
extern int yy_line_nr;
extern FILE *yyin;

hashtable_t *valid_text_locations = NULL;

bool
text_instruction_location(byte *loc)
{
	return (valid_text_locations && hashtable_get(valid_text_locations, loc));
}

static int
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
		return 0;

	case ASM_ARG_IMM64U:
		if (is_signed) {
			warn("Number %lld is negative, can't be represented as unsigned 64 bit argument",
			     v);
		}
		return 0;

	default:
		error("Non-integral type expected");
		return -1;
	}

	if (v < lower || v > upper) {
		warn("%lld is out of bounds [%lld-%lld] for %s", v, lower, upper, mode);
	}
	return 0;
}

static char *
ty_name(int ty)
{
	switch (ty) {
	case ASM_ARG_REG:
		return "register";
	case ASM_ARG_LABEL:
		return "label";
	case ASM_ARG_IMM8U:
		return "unsigned 8 bit integer";
	case ASM_ARG_IMM32U:
		return "unsigned 32 bit integer";
	case ASM_ARG_IMM32S:
		return "signed 32 bit integer";
	case ASM_ARG_IMM64U:
		return "unsigned 64 bit integer";
	case ASM_ARG_IMM64S:
		return "signed 64 bit integer";
	default:
		return "?";
	}
}

void
do_emit_la(buffer_t *buf, asm_arg *args)
{
	emit_li(buf, args[0].r, 0);
}

struct pseudo_op {
	char *name;
	int args_nr;
	byte types[5];
	char offsets[5];
	bool gp_relative_relocation[5];
	void (*emit)(buffer_t *buf, asm_arg *args);
} pseudo_ops[] = {
	{ "la", 2, { ASM_ARG_REG, ASM_ARG_IMM64U }, { -1, 2 }, { false, false }, &do_emit_la },
	{ NULL, 0, {}, {}, {}, NULL }
};

struct pseudo_op *
get_pseudo_op(char *n)
{
	int offset = 0;
	while (pseudo_ops[offset].name) {
		if (!strcmp(n, pseudo_ops[offset].name)) {
			return pseudo_ops + offset;
		}
		++offset;
	}
	return NULL;
}

int
insn_get_args_nr(char *insn)
{
	struct pseudo_op *pseudo_op = get_pseudo_op(insn);
	if (pseudo_op) {
		return pseudo_op->args_nr;
	}
	return asm_insn_get_args_nr(insn);
}

int
insn_get_arg(char *insn, int arg)
{
	struct pseudo_op *pseudo_op = get_pseudo_op(insn);
	if (pseudo_op) {
		if (arg < 0 || arg >= pseudo_op->args_nr) {
			return -1;
		}
		return pseudo_op->types[arg];
	}
	return asm_insn_get_arg(insn, arg);
}

int
insn_arg_offset(char *insn, asm_arg *args, int arg)
{
	struct pseudo_op *pseudo_op = get_pseudo_op(insn);
	if (pseudo_op) {
		if (arg < 0 || arg >= pseudo_op->args_nr) {
			return -1;
		}
		return pseudo_op->offsets[arg];
	}
	return asm_arg_offset(insn, args, arg);
}

bool
insn_get_relocation_type(char *insn, int arg)
{
	struct pseudo_op *pseudo_op = get_pseudo_op(insn);
	if (pseudo_op) {
		if (arg < 0 || arg >= pseudo_op->args_nr) {
			return -1;
		}
		return pseudo_op->gp_relative_relocation[arg];
	}
	return true; // The other instructions use $gp-relative addressing
}

void
emit_insn(buffer_t *buffer, char *insn, asm_arg *args, int args_nr)
{
	struct pseudo_op *pseudo_op = get_pseudo_op(insn);
	if (pseudo_op) {
		pseudo_op->emit(buffer, args);
		return;
	}
	asm_insn(buffer, insn, args, args_nr);
}

/**
 * args_types are preliminary types:
 *   - ASM_ARG_IMM64(U|S) marks literal ints
 *   - ASM_ARG_REG marks registers
 *   - ASM_ARG_LABEL marks uses of labels; the label pointer actually points to the label name
 *     (is handled and freed in this function)
 *
 * Type checking validates these.
 */
static void
try_emit_insn(buffer_t *buffer, char *insn, int args_nr, int *args_types, asm_arg *args)
{
	int expected_args_nr = insn_get_args_nr(insn);
	if (expected_args_nr < 0) {
		error("Unknown assembly instruction: `%s'", insn);
		return;
	}
	if (args_nr != expected_args_nr) {
		error("Assembly instruction `%s' expects %d arguments, has %d, ", insn, expected_args_nr, args_nr);
		return;
	}
	for (int i = 0; i < args_nr; i++) {
		int ty = args_types[i];
		int expected_ty = insn_get_arg(insn, i);
		if (expected_ty == ASM_ARG_REG) {
			if (ty != ASM_ARG_REG) {
				error("Assembly instruction `%s': expects register as parameter #%d, but received %s",
				      insn, i + 1, ty_name(ty));
				return;
			}
		} else if (expected_ty == ASM_ARG_LABEL) {
			char * label = (char *) args[i].label;
			if (ty != ASM_ARG_LABEL) {
				error("Assembly instruction `%s': expects label as parameter #%d, but received %s",
				      insn, i + 1, ty_name(ty));
				return;
			}
			args[i].label = relocation_add_jump_label(label);
		} else {
			if (ty == ASM_ARG_REG) {
				error("Assembly instruction `%s': expects number as parameter #%d, but received register",
				      insn, i + 1);
				return;
			} else if (ty == ASM_ARG_LABEL) {
				const int offset = insn_arg_offset(insn, args, i);
				char *label = (char *) args[i].label;
				if (offset == -1) {
					error("Assembly instruction `%s': Argument #%d cannot reference label (not relocable)",
					      insn, i + 1);
					free(label);
					return;
				}
				relocation_add_data_label(label, buffer_target(buffer), offset, insn_get_relocation_type(insn, i));
			} else {
				check_bounds(expected_ty, args[i].imm, ty == ASM_ARG_IMM64S);
			}
		}
	}

	if (!valid_text_locations) {
		valid_text_locations = hashtable_alloc(hashtable_pointer_hash, hashtable_pointer_compare, 10);
	}
	hashtable_put(valid_text_locations, current_location(), current_location(), NULL);

	emit_insn(buffer, insn, args, args_nr);
}

#define INSN_MODE_EXPECT_END		1
#define INSN_MODE_EXPECT_ARG		2
#define INSN_MODE_EXPECT_DISPLACEMENT	3
#define INSN_MODE_EXPECT_UNDISPLACEMENT	4

static char *
describe_insn_mode(int mode)
{
	switch (mode) {
	case INSN_MODE_EXPECT_END:
		return "expecting end of instruction or comma";
	case INSN_MODE_EXPECT_ARG:
		return "expecting argument after comma";
	case INSN_MODE_EXPECT_DISPLACEMENT:
		return "expecting argument after '('";
	case INSN_MODE_EXPECT_UNDISPLACEMENT:
		return "expecting closing ')'";
	default:
		return "?";
	}
}

static int
parse_register()
{
	switch (yylex()) {
	case T_ID:
		for (int i = 0; i < REGISTERS_NR; i++) {
			if (0 == strcasecmp(yylval.str, register_names[i].mips + 1)) {
				return i;
			}
		}
		error("Unknown register `$%s'", yylval.str);
		break;

	default:
		error("Expected register name after `$'");
	}
	return -1;
}

static void
parse_insn(buffer_t *buffer, char *insn, int next_token)
{
	asm_arg args[MAX_ASM_ARGS];
	int args_types[MAX_ASM_ARGS];
	int args_nr = 0;
	int mode = INSN_MODE_EXPECT_END;

	while (true) {
	
	switch (next_token) {

	case '$':
		args_types[args_nr] = ASM_ARG_REG;
		args[args_nr].r = parse_register();
		if (args[args_nr].r == -1) {
			return;
		}
		++args_nr;
		mode = INSN_MODE_EXPECT_END;
		break;

	case T_ID:
		args_types[args_nr] = ASM_ARG_LABEL;
		args[args_nr++].label = (label_t *) yylval.str;
		mode = INSN_MODE_EXPECT_END;
		break;

	case T_INT:
		args_types[args_nr] = ASM_ARG_IMM64S;
		args[args_nr++].imm = yylval.num;
		mode = INSN_MODE_EXPECT_END;
		break;

	case T_UINT:
		args_types[args_nr] = ASM_ARG_IMM64U;
		args[args_nr++].imm = yylval.num;
		mode = INSN_MODE_EXPECT_END;
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

	case 0:
	case '\n':
		if (mode == INSN_MODE_EXPECT_END) {
			try_emit_insn(buffer, insn, args_nr, args_types, args);
		} else {
			error("encountered newline/end of line but %s", describe_insn_mode(mode));
		}
		return;

	default:
		error("Parse error in assembly instruction `%s': unexpected input", insn);
	}

	next_token = yylex();

	}
}

static void
parse(buffer_t *buffer)
{
	int data_mode = 0; // Bulk data input expected
	int input;
	while ((input = yylex())) {
		switch (input) {
		case T_ID: {
			error_line_nr = yy_line_nr;
			char *id = yylval.str;
			int next = yylex();
			if (next == ':') {
				addrstore_put(current_location(),
					      in_text_section ? ADDRSTORE_KIND_SPECIAL : ADDRSTORE_KIND_DATA, id);
				relocation_add_label(id, current_location());
			} else {
				if (in_text_section) {
					data_mode = 0;
					parse_insn(buffer, id, next);
				} else {
					if (data_mode == T_S_WORD) {
						relocation_add_data_label(id,
									  alloc_data_in_section(8),
									  0,
									  false);
					} else {
						error("Assembly instructions not supported in .data section");
						return;
					}
				}
			}
			break;
		}

		case T_S_DATA:
			error_line_nr = yy_line_nr;
			in_text_section = false;
			data_mode = 0;
			break;

		case T_S_TEXT:
			error_line_nr = yy_line_nr;
			in_text_section = true;
			data_mode = 0;
			break;

		case T_S_WORD:
			// align, if needed:
			if (((long long)current_location()) & 7ll) {
				alloc_data_in_section(8ll - ((long long)current_location() & 7ll));
			}
			// fall through
		case T_S_BYTE:
			// fall through
		case T_S_ASCIIZ:
			error_line_nr = yy_line_nr;
			data_mode = input;
			break;

		case '\n':
			break;
		case ',':
			if (!data_mode) {
				error("Stray comma");
			}
			break;

		case T_UINT:
		case T_INT: {
			switch (data_mode) {
			case T_S_BYTE:
				((unsigned char *)alloc_data_in_section(1))[0] = yylval.num;
				break;
			case T_S_WORD:
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
			int len = strlen(yylval.str) + 1;
			error_line_nr = yy_line_nr;
			switch (data_mode) {
			case T_S_ASCIIZ: {
				void * dest = alloc_data_in_section(len);
				memcpy(dest, yylval.str, len);
			}
				break;
			case T_S_BYTE:
			case T_S_WORD:
			default:
				error("Unexpected asciiz string");
				return;
			}
			break;
		}

		default:
			error("Unexpected control character `%c' (%x)\n", input, input);
			return;
		}
	}
}

void
parse_file(buffer_t *buf, char *filename)
{
	yy_line_nr = 1;
	yyin = fopen(filename, "r");
	if (!yyin) {
		perror(filename);
		error("Failed to load input file");
		return;
	}

	parse(buf);
	error_line_nr = -1;
}

