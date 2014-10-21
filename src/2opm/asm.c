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

#include <assert.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/ptrace.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/user.h>
#include <sys/types.h>
#include <unistd.h>

#include "../assembler.h"
#include "../registers.h"
#include "../assembler-buffer.h"
#include "../chash.h"

#include "assembler-instructions.h"
#include "asm.h"

#define MAX_ASM_ARGS 5
#define DATA_SECTION_SIZE (1024 * 1024)

int yylex();
yylval_t yylval;
extern int yy_line_nr;
extern FILE *yyin;

int errors_nr = 0;
int warnings_nr = 0;

void
error(const char *fmt, ...)
{
	va_list args;
	if (yy_line_nr < 0) {
		fprintf(stderr, "Error: ");
	} else {
		fprintf(stderr, "[line %d] error: ", yy_line_nr);
	}
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
	fprintf(stderr, "\n");
	++errors_nr;
}

void
warn(const char *fmt, ...)
{
	va_list args;
	if (yy_line_nr < 0) {
		fprintf(stderr, "Warning: ");
	} else {
		fprintf(stderr, "[line %d] warning: ", yy_line_nr);
	}
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
	fprintf(stderr, "\n");
	++warnings_nr;
}

void
yyerror(const char *str)
{
	error("%s", str);
}

buffer_t text_buffer;
char data_section[DATA_SECTION_SIZE];
bool in_text_section = false; // which section are we operating on?
int data_offset = 0;

void *
current_offset()
{
	if (in_text_section) {
		return buffer_target(&text_buffer);
	} else {
		return &(data_section[data_offset]);
	}
}

typedef struct {
	char *name;
	void *text_location;
	int int_offset;
	bool text_section;
	hashtable_t *label_references;	// contains mallocd label_t* s
	hashtable_t *gp_references;	// contains sint32* s
} relocation_list_t;
static hashtable_t *labels_table;

void init()
{
	data_offset = 0;
	text_buffer = buffer_new(1024);
	in_text_section = false;
	labels_table = hashtable_alloc(hashtable_pointer_hash, hashtable_pointer_compare, 4);
}

static void *
alloc_data_in_section(size_t size)
{
	if (in_text_section) {
		return buffer_alloc(&text_buffer, size);
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

static relocation_list_t *
get_label_relocation_list(char *name)
{
	relocation_list_t *rellist = (relocation_list_t *) hashtable_get(labels_table, name);
	if (!rellist) {
		rellist = calloc(1, sizeof(relocation_list_t));
		hashtable_put(labels_table, name, rellist, NULL);
		rellist->name = name;
		rellist->label_references = hashtable_alloc(hashtable_pointer_hash, hashtable_pointer_compare, 3);
		rellist->gp_references = hashtable_alloc(hashtable_pointer_hash, hashtable_pointer_compare, 3);
	}
	return rellist;
}

static void
add_label(char *name, void *offset)
{
	relocation_list_t *rellist = get_label_relocation_list(name);
	if (rellist->text_location || rellist->int_offset) {
		error("Multiple definitions of label `%s'", name);
		return;
	}

	rellist->text_section = in_text_section;
	if (in_text_section) {
		rellist->text_location = offset;
	} else {
		rellist->int_offset = ((char *)offset) - ((char *)data_section);
	}
}


static void
relocate_label(void *key, void *value, void *state)
{
	label_t *label = (label_t *) value;
	relocation_list_t *rellist = (relocation_list_t *) state;
	if (!rellist->text_section) {
		error("Label %s defined for data but used in code", rellist->name);
	} else {
		buffer_setlabel(label, rellist->text_location);
		free(label);
	}
}

static void
relocate_displacement(void *key, void *value, void *state)
{
	int offset = (signed long long) value;
	relocation_list_t *rellist = (relocation_list_t *) state;
	if (rellist->text_section) {
		error("Label %s defined for code but used in data", rellist->name);
	} else {
		*((int *) (((char *) key) + offset)) = rellist->int_offset;
	}
}

// relocate relocation_list
static void
relocate_one(void *key, void *value, void *state)
{
	relocation_list_t *rellist = (relocation_list_t *) value;
	if (!rellist->text_location && !rellist->int_offset) {
		error("Label `%s' used but not defined!", rellist->name);
		return;
	}
	hashtable_foreach(rellist->label_references, relocate_label, rellist);
	hashtable_foreach(rellist->gp_references, relocate_displacement, rellist);
}

static void
relocate()
{
	hashtable_foreach(labels_table, relocate_one, NULL);
}

static label_t *
relocation_add_jump_label(char *label)
{
	relocation_list_t *t = get_label_relocation_list(label);
	label_t *retval = malloc(sizeof(label_t));
	hashtable_put(t->label_references, retval, retval, NULL);
	return retval;
}

static void
relocation_add_gp_label(char *label, void *addr, int offset)
{
	relocation_list_t *t = get_label_relocation_list(label);
	hashtable_put(t->gp_references, addr, (void *) ((signed long long) offset), NULL);
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
try_emit_insn(char *insn, int args_nr, int *args_types, asm_arg *args)
{
	int expected_args_nr = asm_insn_get_args_nr(insn);
	if (expected_args_nr < 0) {
		error("Unknown assembly instruction: `%s'", insn);
		return;
	}
	if (args_nr != expected_args_nr) {
		error("Assembly instruction `%s' expects %d arguments, has %d, ", insn, args_nr, expected_args_nr);
		return;
	}
	for (int i = 0; i < args_nr; i++) {
		int ty = args_types[i];
		int expected_ty = asm_insn_get_arg(insn, i);
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
				const int offset = asm_arg_offset(insn, args, i);
				char *label = (char *) args[i].label;
				if (offset == -1) {
					error("Assembly instruction `%s': Argument #%d cannot reference label (not relocable)", i);
					free(label);
					return;
				}
				relocation_add_gp_label(label, buffer_target(&text_buffer), offset);
			} else {
				check_bounds(expected_ty, args[i].imm, ty == ASM_ARG_IMM64S);
			}
		}
	}
	asm_insn(&text_buffer, insn, args, args_nr);
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
parse_insn(char *insn, int next_token)
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
			try_emit_insn(insn, args_nr, args_types, args);
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
parse()
{
	int data_mode = 0; // Bulk data input expected
	int input;
	while ((input = yylex())) {
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

static void
print_string(char *s)
{
	printf("%s\n", s);
}

static void
print_int(int i)
{
	printf("%d\n", i);
}

static int
read_int()
{
	int i;
	scanf("%d", &i);
	return i;
}

static void
forgot_to_return()
{
	fprintf(stderr, "Warning: Forgot to exit or return from main");
	exit(1);
}

#define BUILTINS_NR 4

struct {
	char *name;
	void *ptr;
} builtins[BUILTINS_NR] = {
	{"print_string", &print_string},
	{"print_int", &print_int},
	{"read_int", &read_int},
	{"exit", &exit},
};

char* mk_unique_string(char *text);

void
init_builtins()
{
	in_text_section = true;
	for (int i = 0; i < BUILTINS_NR; i++) {
		add_label(mk_unique_string(builtins[i].name), current_offset());
		emit_push(&text_buffer, REGISTER_T0); // align stack
		emit_li(&text_buffer, REGISTER_V0, (unsigned long long) builtins[i].ptr);
		emit_jalr(&text_buffer, REGISTER_V0);
		emit_pop(&text_buffer, REGISTER_T0); // align stack
		emit_jreturn(&text_buffer);
	}
}

void
fail(char *msg)
{
	fprintf(stderr, "Failure: %s\n", msg);
	exit(1);
}

static void
decode_regs(unsigned long long *regs, void **pc, struct user_regs_struct *uregs)
{
	*pc = (void *) uregs->rip;
	regs[0] = uregs->rax;
	regs[1] = uregs->rcx;
	regs[2] = uregs->rdx;
	regs[3] = uregs->rbx;
	regs[4] = uregs->rsp;
	regs[5] = uregs->rbp;
	regs[6] = uregs->rsi;
	regs[7] = uregs->rdi;
	regs[8] = uregs->r8;
	regs[9] = uregs->r9;
	regs[10] = uregs->r10;
	regs[11] = uregs->r11;
	regs[12] = uregs->r12;
	regs[13] = uregs->r13;
	regs[14] = uregs->r14;
	regs[15] = uregs->r15;
}

// putdata/getdata by Pradeep Padala
void getdata(pid_t child, unsigned long long addr, char *str, int len)
{
	char *laddr;
	int i, j;
	union u {
		long val;
		char chars[sizeof(long)];
	}data;
	i = 0;
	j = len / sizeof(long);
	laddr = str;
	while(i < j) {
		data.val = ptrace(PTRACE_PEEKDATA, child,
				  addr + i * 4, NULL);
		memcpy(laddr, data.chars, sizeof(long));
		++i;
		laddr += sizeof(long);
	}
	j = len % sizeof(long);
	if(j != 0) {
		data.val = ptrace(PTRACE_PEEKDATA, child,
                          addr + i * 4, NULL);
		memcpy(laddr, data.chars, j);
	}
	str[len] = '\0';
}

void putdata(pid_t child, unsigned long long addr, char *str, int len)
{
	char *laddr;
	int i, j;
	union u {
		long val;
		char chars[sizeof(long)];
	}data;
	i = 0;
	j = len / sizeof(long);
	laddr = str;
	while(i < j) {
		memcpy(data.chars, laddr, sizeof(long));
		ptrace(PTRACE_POKEDATA, child,
		       addr + i * 4, data.val);
		++i;
		laddr += sizeof(long);
	}
	j = len % sizeof(long);
	if(j != 0) {
		memcpy(data.chars, laddr, j);
		ptrace(PTRACE_POKEDATA, child,
		       addr + i * 4, data.val);
	}
}

int wait_for_me = 0;

#define PTRACE(a, b, c, d) if (-1 == ptrace(a, b, c, d)) { fprintf(stderr, "ptrace failure in %d\n", __LINE__); perror(#a); return; }

static void
debug(void (*entry_point)())
{
	unsigned long long debug_region_start = (unsigned long long) buffer_entrypoint(text_buffer);
	unsigned long long debug_region_end = debug_region_start + buffer_size(text_buffer);
	
	int child;
	//	signal(SIGSTOP, &sighandler);

	if ((child = fork())) {
		struct user_regs_struct regs_struct;
		unsigned long long regs[16];
		void *pc;
		int status;

		// parent
		PTRACE(PTRACE_ATTACH, child, NULL, NULL);
		wait(&status);
		ptrace(PTRACE_SETOPTIONS, child, NULL, PTRACE_O_TRACEEXIT);
		wait_for_me = 1;
		putdata(child, (unsigned long long) &wait_for_me, (char *) &wait_for_me, sizeof(wait_for_me));
		do {
			PTRACE(PTRACE_GETREGS, child, &regs_struct, &regs_struct);
			if (((void *)regs_struct.rip) != entry_point) {
				break;
			}
			PTRACE(PTRACE_SINGLESTEP, child, NULL, NULL);
			if (status >> 8 == (SIGTRAP | (PTRACE_EVENT_EXIT<<8))) {
				printf("Early exit.");
				return;
			}
			wait(&status);
		} while (true);

		fprintf(stderr, "===>> found start\n");

		while (true) {
			if (WIFEXITED(status)) {// >> 8 == (SIGTRAP | (PTRACE_EVENT_EXIT<<8))) {
				printf("===>> Normal exit: %x.\n", status);
				return;
			} else if (WIFSIGNALED(status)) {// >> 8 == (SIGTRAP | (PTRACE_EVENT_EXIT<<8))) {
				PTRACE(PTRACE_DETACH, child, NULL, NULL);
				printf("===>> SIGNAL received: %d.\n", WTERMSIG(status));
				return;
			} else if (WIFSTOPPED(status)
				   && WSTOPSIG(status) != SIGSTOP
				   && WSTOPSIG(status) != SIGTRAP) {// >> 8 == (SIGTRAP | (PTRACE_EVENT_EXIT<<8))) {
				PTRACE(PTRACE_DETACH, child, NULL, NULL);
				printf("===>> SIGNAL received: %d, %s.\n", WSTOPSIG(status), strsignal(WSTOPSIG(status)));
				return;
			}
			PTRACE(PTRACE_GETREGS, child, &regs_struct, &regs_struct);
			decode_regs(regs, &pc, &regs_struct);
			
			if ((unsigned long long) pc >= debug_region_start
			    && (unsigned long long) pc <= debug_region_end) {
				printf("%p : %llu\n", pc, regs[REGISTER_A0]);
			}
			PTRACE(PTRACE_SINGLESTEP, child, NULL, NULL);
			wait(&status);
		}
	} else {
		// child
		while (!wait_for_me) {sleep(1); fprintf(stderr, "wfm=%d\n", wait_for_me);};
		entry_point();
	}
}


int
main(int argc, char **argv)
{
	init();
	parse_file(argv[1]);
	in_text_section = true;
	emit_li(&text_buffer, REGISTER_V0, (unsigned long long) &forgot_to_return);
	emit_jalr(&text_buffer, REGISTER_V0);
	init_builtins();
	relocate();
	relocation_list_t *list = get_label_relocation_list(mk_unique_string("main"));
	void (*entry_point)() = list->text_location;
	yy_line_nr = -1;
	if (!entry_point) {
		error("No `main' entry point set in text segment");
	}

	if (errors_nr) {
		fprintf(stderr, "%d error%s, aborting\n", errors_nr, errors_nr > 1? "s" : "");
		return 1;
	}
	buffer_disassemble(text_buffer);
	fprintf(stderr, "main at %p\n", entry_point);
	fflush(NULL);

	debug(entry_point);
	return 0;
}
