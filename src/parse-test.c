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
#include "ast.h"
#include "parser.h"

extern FILE *yyin; // lexer

int
main(int argc, char **argv)
{
	yyin = stdin;

	ast_node_t *n = NULL;
	if (!parse_expr(&n)) {
		fprintf(stderr, "Parse error\n");
		return 1;
	}

	ast_node_dump(stdout, n, AST_NODE_DUMP_FLAGS);
	ast_node_free(n, 1);

	return 0;
}
