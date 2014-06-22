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


static void
storage_alloc(ast_node_t *node, unsigned short *var_counter);

static void
recurse(ast_node_t *node, unsigned short *var_counter)
{
	if (!IS_VALUE_NODE(node)) {
		for (int i = 0; i < node->children_nr; i++) {
			storage_alloc(node->children[i], var_counter);
		}
	}
}


static void
storage_alloc(ast_node_t *node, unsigned short *var_counter)
{
	if (!node) {
		return;
	}

	switch (NODE_TY(node)) {
	case AST_NODE_CLASSDEF: {
		// Felder in Klassen erhalten ihre Lokationen schon in der Namensanalyse
		// (Ebenso Parameter)
		recurse(node, NULL);
	}
		break;

	case AST_NODE_FUNDEF:
		// Parameter erhalten ihre Lokationen schon in der Namensanalyse
		recurse(node->children[2], &node->sym->vars_nr);
		break;

	case AST_NODE_BLOCK:
		if (!var_counter) {
			recurse(node, NULL);
		} else {
		unsigned short var_counter_base = *var_counter;
		unsigned short max_var_counter = var_counter_base;
		ast_node_t **elements = node->children;
		for (int i = 0; i < node->children_nr; i++) {
			*var_counter = var_counter_base;

			recurse(elements[i], var_counter);
			if (NODE_TY(elements[i]) == AST_NODE_VARDECL) {
				// Die einzige `echte' Variablenallozierung:  Alle anderen Allozierungen sind temporaer
				//ast_node_dump(stderr, elements[i], 6);
				elements[i]->sym->offset = var_counter_base++;
			}

			if (NODE_TY(elements[i]) == AST_NODE_BLOCK) {
				// Verschachtelter Block: Wir erben angemessen
				var_counter_base = *var_counter;
			}

			if (*var_counter > max_var_counter) {
				max_var_counter = *var_counter;
			}
		}
		*var_counter = max_var_counter;
		
	}
		break;

	case AST_NODE_VARDECL:
		if (var_counter) {
			node->sym->offset = (*var_counter)++;
		}
		break;


	case AST_NODE_FUNAPP: {
		ast_node_t **elements = node->children[1]->children;
		for (int i = 0; i < node->children[1]->children_nr; i++) {
			if (!IS_VALUE_NODE(elements[i])) {
				elements[i]->storage = (*var_counter)++;
			}
			storage_alloc(elements[i], var_counter);
		}
	}
		break;

	default:
		recurse(node, var_counter);
	}
}



int
storage_allocation(ast_node_t *n)
{
	unsigned short vars = 0;
	storage_alloc(n, &vars);
	return vars;
}
