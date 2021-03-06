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

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "analysis.h"
#include "ast.h"
#include "class.h"
#include "compiler-options.h"
#include "data-flow.h"
#include "parser.h"
#include "runtime.h"
#include "symbol-table.h"
#include "timer.h"
#include "version.h"

#define ACTION_RUN	1
#define ACTION_PRINT	2
#define ACTION_VERSION	3
#define ACTION_HELP	4

// Optionen zur Ausgabe
// positive Werte reserviert für RUNTIME_ACTION-Konstanten
#define PRINT_PARSED_AST	RUNTIME_ACTION_NONE
#define PRINT_NAMED_AST		RUNTIME_ACTION_NAME_ANALYSIS
#define PRINT_TYPED_AST		RUNTIME_ACTION_TYPE_ANALYSIS
#define PRINT_SEMANTIC_AST	RUNTIME_ACTION_SEMANTIC_ANALYSIS
#define PRINT_SYMTAB		-1
#define PRINT_SYMTAB_ALL	-2
#define PRINT_ASM		-3
#define PRINT_CFG_AST		-4
#define PRINT_CFG_DOT		-5

#define COMPOPT_NO_BOUNDS_CHECKS	1
#define COMPOPT_INT_ARRAYS		2
#define COMPOPT_DEBUG_DYNAMIC_COMPILER	3
#define COMPOPT_DEBUG_ASSEMBLY		4
#define COMPOPT_DEBUG_DATA_FLOW		5
#define COMPOPT_DEBUG_ADAPTIVE		6
#define COMPOPT_DEBUG_GC		7
#define COMPOPT_NO_ADAPTIVE		8

typedef struct {
	char *name;
	int option;
	char *description;
}  option_rec_t;

static const option_rec_t options_printing[] = {
	{ "parse",	PRINT_PARSED_AST,	"AST right after parsing" },
	{ "parsed",	PRINT_PARSED_AST,	NULL },
	{ "named",	PRINT_NAMED_AST,	"AST after name analysis but before type analysis" },
	{ "typed",	PRINT_TYPED_AST,	"AST after type analysis" },
	{ "semantic",	PRINT_SEMANTIC_AST,	"AST after all semantic analyses" },
	{ "ast",	PRINT_SEMANTIC_AST,	NULL },
	{ "asm",	PRINT_ASM,		"Assembly code for main entry point" },
	{ "symtab",	PRINT_SYMTAB,		"Symbol table with user-defined symbols" },
	{ "symtab-all",	PRINT_SYMTAB_ALL,	"Full symbol table, including built-in symbols" },
	{ "cfg-ast",	PRINT_CFG_AST,		"AST with control flow graph annotations" },
	{ "cfg",	PRINT_CFG_DOT,		"Control-flow graph in DOT format" },
	{ NULL, 0, NULL }
};

static const option_rec_t options_compiler[] = {
	{ "no-bounds-checks",		COMPOPT_NO_BOUNDS_CHECKS,	"Do not generate bounds-checking code for array accesses" },
	{ "no-adaptive",		COMPOPT_NO_ADAPTIVE,		"Do not perform adaptive compilation" },
	{ "int-arrays",			COMPOPT_INT_ARRAYS,		"Change the type of array elements to 'int'" },
	{ "debug-dynamic-compiler",	COMPOPT_DEBUG_DYNAMIC_COMPILER,	"Print out informative messages and disassembly during runtime compilation" },
	{ "debug-asm",			COMPOPT_DEBUG_ASSEMBLY,		"Use interactive assembly debugger to run" },
	{ "debug-adaptive",		COMPOPT_DEBUG_ADAPTIVE,		"Debug adaptive compilation" },
	{ "debug-gc",			COMPOPT_DEBUG_GC,		"Debug automatic memory management" },
	{ "debug-data-flow",		COMPOPT_DEBUG_DATA_FLOW,	"Debug the selected data flow analysis" },
	{ NULL, 0, NULL }
};

static void
print_options(const option_rec_t *options, char *indent)
{
	while (options->name) {
		if (options->description) {
			printf("%s%-30s%s\n", indent, options->name, options->description);
		}
		++options;
	}
}

static int
pick_option(const option_rec_t *options, char *msg, char *s)
{
	const option_rec_t *orig_options = options;
	while (options->name) {
		if (!strcmp(s, options->name)) {
			return options->option;
		}
		++options;
	}
	printf("`%s' is not a valid %s.  The following options are valid:\n", s, msg);
	print_options(orig_options, "\t");
	fflush(NULL);
	exit(1);
}

void
fail(char *msg)
{
	fprintf(stderr, "Critical failure: %s\n", msg);
	exit(1);
}

void
print_help(char *fn)
{
	printf("Usage: %s [<options>] <filename>\n", fn);
	printf("Where <options> can be any of the following:\n"
	       "\t-v\tPrint version information\n"
	       "\t-h\tPrint this help information\n"
	       "\t-x\tExecute program (default action)\n"
	       "\t-t\tTime program execution\n"
	       "\t-m <n>\tSet heap size to <n> kiB\n"
	       "\t-f <n>\tActivate various compiler options, with <n> from:\n");
	print_options(options_compiler, "\t\t\t");
	printf("\t-p <n>\tPrint intermediate representation, with <n> from:\n");
	print_options(options_printing, "\t\t\t");
	printf("\t-a <n>\tObserve data flow analysis (e.g., with -p cfg), with <n> from:\n");
	for (int i = 0; i < DATA_FLOW_ANALYSES_NR; ++i) {
		if (data_flow_analysis_by_index(i)) {
			printf("\t\t\t%s\n", data_flow_analysis_by_index(i)->name);
		}
	}
	exit(0);
}

void
print_version()
{
	printf("AttoVM version " VERSION ".\n");
}

static data_flow_analysis_t *data_flow_analysis = NULL;

static void
dottify(symtab_entry_t *sym, void *_)
{
	cfg_dottify(stdout, sym, data_flow_analysis);
}

void
do_print(runtime_image_t *img, int mode)
{
	int dump_flags = AST_NODE_DUMP_FORMATTED | AST_NODE_DUMP_FLAGS | AST_NODE_DUMP_STORAGE;

	switch (mode) {

	case PRINT_ASM:
		buffer_disassemble(img->code_buffer);
		return;

	case PRINT_SYMTAB_ALL:
		for (int i = 1; i <= symtab_entries_builtin_nr; i++) {
			symtab_entry_dump(stdout, symtab_lookup(-i));
			puts("");
		}
	case PRINT_SYMTAB:
		for (int i = 1; i <= symtab_entries_nr; i++) {
			symtab_entry_dump(stdout, symtab_lookup(i));
			puts("");
		}
		return;

	case PRINT_CFG_DOT:
		fprintf(stdout, "digraph cfg {\n");
		runtime_foreach_callable(img, dottify, NULL);
		fprintf(stdout, "}\n");
		return;

	case PRINT_CFG_AST:
		dump_flags |= AST_NODE_DUMP_CFG;
		break;

	default:
		break;
	}
	ast_node_dump(stdout, img->ast, dump_flags);
	puts("");
}

int
main(int argc, char **argv)
{
	int action = ACTION_RUN;
	int print_mode = PRINT_ASM;
	int opt;
	bool debug_data_flow = false;
	bool time = false;
	wallclock_timer_t timer;
	timer_reset(&timer);
	timer_start(&timer);

	while ((opt = getopt(argc, argv, "htvxm:p:f:a:")) != -1) {
		switch (opt) {

		case 'v':
			action = ACTION_VERSION;
			break;

		case 'h':
			action = ACTION_HELP;
			break;

		case 'x':
			action = ACTION_RUN;
			break;

		case 't':
			time = true;
			break;

		case 'p':
			action = ACTION_PRINT;
			print_mode = pick_option(options_printing, "print option", optarg);
			break;

		case 'm':
			errno = 0;
			compiler_options.heap_size = 1024 * strtol(optarg, NULL, 0);
			if (errno) {
				perror("-m");
				exit(1);
			}
			break;

		case 'f':
			switch (pick_option(options_compiler, "compiler option", optarg)) {
			case COMPOPT_NO_BOUNDS_CHECKS:
				compiler_options.no_bounds_checks = true;
				break;

			case COMPOPT_NO_ADAPTIVE:
				compiler_options.no_adaptive_compilation = true;
				break;

			case COMPOPT_INT_ARRAYS:
				compiler_options.array_storage_type = TYPE_INT;
				break;

			case COMPOPT_DEBUG_DYNAMIC_COMPILER:
				compiler_options.debug_dynamic_compilation = true;
				break;

			case COMPOPT_DEBUG_ADAPTIVE:
				compiler_options.debug_adaptive = true;
				break;

			case COMPOPT_DEBUG_GC:
				compiler_options.debug_gc = true;
				break;

			case COMPOPT_DEBUG_ASSEMBLY:
				compiler_options.debug_assembly = true;
				break;

			case COMPOPT_DEBUG_DATA_FLOW:
				debug_data_flow = true;
				break;
			}
			break;

		case 'a':
			data_flow_analysis = data_flow_analysis_by_name(optarg);
			if (!data_flow_analysis) {
				fprintf(stderr, "No such analysis, `%s'\n", optarg);
				return 1;
			}
			break;

		default:;
		}
	}

	if (debug_data_flow) {
		if (data_flow_analysis) {
			data_flow_analysis->debug = true;
		} else {
			fprintf(stderr, "No data flow analysis selected, can't debug\n");
			return 1;
		}
	}

	switch (action) {
	case ACTION_HELP:
		print_help(argv[0]);
		return 0;

	case ACTION_VERSION:
		print_version();
		return 0;
	}

	if (optind >= argc) {
		fprintf(stderr, "Must specify at least one file to process!\n");
		return 1;
	}

	while (optind < argc) {
		FILE *input = NULL;

		if (!strcmp(argv[optind], "-")) {
			input = stdin;
		} else {
			input = fopen(argv[optind], "r");
			if (input == NULL) {
				perror(argv[optind]);
				return 1;
			}
		}
		++optind;

		parser_restart(input);

		ast_node_t *root = NULL;
		builtins_init();
		if (!parse_program(&root) || parser_get_errors_nr()) {
			fprintf(stderr, "Parse error\n");
			return 1;
		}

		switch (action) {

		case ACTION_RUN: {
			runtime_image_t *img = runtime_prepare(root, RUNTIME_ACTION_COMPILE);
			if (img) {
				runtime_execute(img);
				runtime_free(img);
			}
		}
			break;

		case ACTION_PRINT: {
			int preparation = RUNTIME_ACTION_COMPILE;
			if (print_mode >= 0) {
				preparation = print_mode;
			}
			runtime_image_t *img = runtime_prepare(root, preparation);
			if (img) {
				do_print(img, print_mode);
				runtime_free(img);
			} else {
				return 1;
			}
		}
			break;

		}

		if (input != stdin) {
			fclose(input);
		}
	}

	if (time) {
		fprintf(stderr, "Total\t");
		timer_print(stderr, &timer);
		fprintf(stderr, "\n");
	}
	return 0;
}
