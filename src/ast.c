/***************************************************************************
  Copyright (C) 2013 Christoph Reichenbach


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

#include "ast.h"
#include <string.h>
#include <stdarg.h>
#include <assert.h>

ast_node_t *
ast_node_alloc_generic_without_init(int type, int children_nr)
{
	assert(type > AST_VALUE_MAX);

	ast_node_t *retval = (ast_node_t *) malloc(sizeof(ast_node_t)
						   // allocate extra space for children
						   + sizeof(ast_node_t *) * children_nr);
	retval->type = type;
	retval->annotations = NULL;
	retval->children_nr = children_nr;
	return retval;
}


ast_node_t *
ast_node_alloc_generic(int type, int children_nr, ...)
{
	ast_node_t *retval = ast_node_alloc_generic_without_init(type, children_nr);

	va_list va;
	va_start(va, children_nr);
	for (int i = 0; i < children_nr; i++) {
		retval->children[i] = va_arg(va, ast_node_t *);
	}
	va_end(va);

	return retval;
}

ast_node_t *
value_node_alloc_generic(int type, ast_value_union_t value)
{
	assert(type <= AST_VALUE_MAX && type > AST_ILLEGAL);

	ast_value_node_t *retval = (ast_value_node_t *) malloc(sizeof(ast_value_node_t));
	retval->type = type;
	retval->v = value;

	return (ast_node_t *) retval;
}

void
ast_node_free(ast_node_t *node, int recursive)
{
	int children_nr = 0;

	if (IS_VALUE_NODE(node)) {
		// Value-Knoten mit Strings/Bezeichnern zeigen auf separat allozierten Speicher
		if (node->type == AST_VALUE_STRING) {
			free(AV_STRING(node));
		}
	} else {
		children_nr = node->children_nr;
	}

	if (recursive) {
		while (children_nr--) {
			ast_node_free(node->children[children_nr], 1);
		}
	}

	free(node);
}

ast_node_t *
ast_node_clone(ast_node_t *node)
{
	if (node == NULL) {
		return NULL;
	}

	ast_node_t *retval;
	if (IS_VALUE_NODE(node)) {
		const ast_value_node_t *vn = (ast_value_node_t *) node;
		retval = value_node_alloc_generic(vn->type, vn->v);
		if (retval->type == AST_VALUE_STRING) {
			AV_STRING(retval) = strdup(AV_STRING(retval));
		}
	} else {
		retval = ast_node_alloc_generic(node->type, node->children_nr);
		for (int i = 0; i < node->children_nr; i++) {
			retval->children[i] = ast_node_clone(node->children[i]);
		}
	}
	return retval;
}
