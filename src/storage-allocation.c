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

#include "analysis.h"
#include "symbol-table.h"


static int
storage_alloc(ast_node_t *node);

static int
update_node(ast_node_t *node)
{
	if (!node) {
		return 0;
	}
	int count = storage_alloc(node);
	if (!IS_VALUE_NODE(node)) {
		node->storage = count;
	}

	/* ast_node_dump(stderr, node, 6); */
	/* fprintf(stderr, "allloc=> %d\n", count); */

	return count;
}

static int
recurse(ast_node_t *node)
{
	int total = 0;
	if (!IS_VALUE_NODE(node)) {
		for (int i = 0; i < node->children_nr; i++) {
			total += update_node(node->children[i]);
		}
	}
	return total;
}


static int
storage_alloc(ast_node_t *node)
{
	if (!node) {
		return 0;
	}

	switch (NODE_TY(node)) {
	case AST_NODE_CLASSDEF: {
		// Felder in Klassen erhalten ihre Lokationen schon in der Namensanalyse
		// (Ebenso Parameter)
		recurse(node);
		return 0;
	}
		break;

	case AST_NODE_FUNDEF:
		// Parameter erhalten ihre Lokationen schon in der Namensanalyse
		recurse(node);
		return 0;
		break;

	case AST_NODE_BLOCK: {
		int locals = 0; // Anzahl der lokalen Variablen
		int max_subblock_size = 0; // Anzal der Variablen, die fuer den groessen Sub-Block benoetigt werden
		ast_node_t **elements = node->children;

		for (int i = 0; i < node->children_nr; i++) {
			int size = update_node(elements[i]);
			if (NODE_TY(elements[i]) == AST_NODE_VARDECL) {
				locals += size;
			} else if (size > max_subblock_size) {
				max_subblock_size = size;
			}
		}
		return locals + max_subblock_size;
	}

	case AST_NODE_VARDECL:
		return 1;

	default:
		return recurse(node);
	}
}

int
storage_size(ast_node_t *n)
{
	return update_node(n);
}
