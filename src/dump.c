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
#include <unistd.h>

#include "ast.h"
#include "class.h"
#include "parser.h"
#include "symbol-table.h"
#include "analysis.h"

extern FILE *yyin; // lexer

static int do_print_builtins = 0;

void
print_help(char *fn)
{
	printf("Usage: %s [-f <filename>] [options] <action>", fn);
	printf("Where <action> is one of:\n"
	       "\t-r\tPrint raw AST (no name analysis)\n"
	       "\t-n\tPrint AST after name analysis\n"
	       "\t-a\tPrint AST after semantic (name + type) analysis\n"
	       "\t-s\tPrint symbol table after name analysis\n"
	       );
	printf("And [options] are any of:\n"
	       "\t-b\tWhen printing symbol table: include built-in operations\n");
	exit(0);
}

int
main(int argc, char **argv)
{
	char action = 0;
	yyin = stdin;
	int opt;

	while ((opt = getopt(argc, argv, "brnasf:")) != -1) {
		switch (opt) {

		case 'f':
			yyin = fopen(optarg, "r");
			if (yyin == NULL) {
				perror(optarg);
				return 1;
			}

		case 'b':
			do_print_builtins = 1;
			break;

		case 'r':
		case 'a':
		case 'n':
		case 's':
			action = opt;
			break;

		case 'h':
		default:
			print_help(argv[0]);
		}
	}

	ast_node_t *root = NULL;
	builtins_init();
	if (!parse_program(&root)) {
		fprintf(stderr, "Parse error\n");
		return 1;
	}

	switch (action) {
	case 'r':
		ast_node_dump(stdout, root, AST_NODE_DUMP_FLAGS | AST_NODE_DUMP_FORMATTED);
		break;

	case 'n':
	case 'a':
		name_analysis(root);
		if (action == 'a') {
			type_analysis(&root);
		}
		ast_node_dump(stdout, root, AST_NODE_DUMP_FLAGS | AST_NODE_DUMP_FORMATTED);
		break;

	case 's':
		name_analysis(root);
		if (do_print_builtins) {
			for (int i = 1; i <= symtab_entries_builtin_nr; i++) {
				symtab_entry_dump(stdout, symtab_lookup(-i));
				puts("");
			}
		}
		for (int i = 1; i <= symtab_entries_nr; i++) {
			symtab_entry_dump(stdout, symtab_lookup(i));
			puts("");
		}
			     break;
	default:
		fprintf(stderr, "Unknown/missing action\n");
		print_help(argv[0]);
		return 1;
	}

	ast_node_free(root, 1);
	return 0;
}
