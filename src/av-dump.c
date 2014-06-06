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
#include "parser.h"

extern FILE *yyin; // lexer

static int do_print_builtins = 0;

void
print_help(char *fn)
{
	printf("Usage: %s [-f <filename>] [options] <action>", fn);
	printf("Where <action> is one of:\n"
	       "\t-a\tPrint AST\n"
	       "\t-n\tPrint name analysis results\n");
	printf("And [options] are any of:\n"
	       "\t-b\tWhen printing name analysis: include built-in operations\n"
	exit(0);
}

int
main(int argc, char **argv)
{
	char action = 0;
	yyin = stdin;
	int opt;

	while ((opt = getopt(argc, argv, "abnhf:")) != -1) {
		switch (opt) {

		case 'f':
			yyin = fopen(optarg, "r");
			if (yyin == NULL) {
				perror(optarg);
				return 1;
			}

		case 'b':
		case 'a':
		case 'n':
			action = opt;
			break;

		case 'h':
		default:
			print_help(argv[0]);
		}
	}

	ast_node_t *n = NULL;
	if (!parse_program(&n)) {
		fprintf(stderr, "Parse error\n");
		return 1;
	}

	switch (action) {
	case 'a':
		ast_node_dump(stdout, n, AST_NODE_DUMP_FLAGS | AST_NODE_DUMP_FORMATTED);
		ast_node_free(n, 1);
		break;
	case 'n':
		if (do_print_builtins) {
			for (int i = 1; i <= symtab_entries_builtin_nr; i++) {
				symtab_entry_dump(symtab_lookup(-i));
			}
		}
		for (int i = 1; i <= symtab_entries_nr; i++) {
			symtab_entry_dump(symtab_lookup(i));
		}
			     break;
	default:
		fprintf(stderr, "Unknown/missing action\n");
		print_help(argv[0]);
		return 1;
	}

	return 0;
}
