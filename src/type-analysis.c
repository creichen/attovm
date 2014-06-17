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
#include <stdarg.h>

#include "analysis.h"
#include "symbol-table.h"

int array_storage_type = TYPE_OBJ;
int method_call_param_type = TYPE_OBJ;
int method_call_return_type = TYPE_OBJ;

static int error_count = 0;

int
type_analysis_errors()
{
	return error_count;
}

static void
error(const ast_node_t *node, char *fmt, ...)
{
	// Variable Anzahl von Parametern
	va_list args;
	fprintf(stderr, "Type error for `%s': ", AV_NAME(node));
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
	fprintf(stderr, "\n");
	++error_count;
}


static void
set_type(ast_node_t *node, int ty)
{
	node->flags &= AST_NODE_MASK;
	node->flags |= ty & ~AST_NODE_MASK;
}

static ast_node_t *
require_lvalue(ast_node_t *node, int const_assignment_permitted)
{
	if (node == NULL) {
		return NULL;
	}

	switch (NODE_TY(node)) {
	case AST_VALUE_ID:
		if (!const_assignment_permitted && (node->type & AST_FLAG_CONST)) {
			error(node, "attempted assignment to constant `%s'", ((symtab_entry_t *)node->annotations)->name);
		}
	case AST_NODE_ARRAYSUB:
		break;

	default:
		error(node, "attempted assignment to non-lvalue");
	}

	node->type |= AST_FLAG_LVALUE;
}

static ast_node_t *
require_type(ast_node_t *node, int ty)
{
	static symtab_entry_t *convert_sym = NULL;
	if (!convert_sym) {
		symtab_lookup(BUILTIN_OP_CONVERT);
	}

	if (node == NULL) {
		return NULL;
	}

	if (ty == 0 // only used for the CONVERT operation itself
	    || (node->type & ~AST_NODE_MASK) == ty) {
		return node;
	}

	ast_node_t *fun = value_node_alloc_generic(AST_VALUE_ID, (ast_value_union_t) { .ident = BUILTIN_OP_CONVERT });
	fun->annotation = convert_sym;
	ast_node_t *conversion ast_node_alloc_generic(AST_NODE_FUNAPP,
						      2, fun,
						      ast_node_alloc_generic(AST_NODE_ACTUALS,
									     1,
									     node));
	set_type(fun, ty);
	set_type(conversion, ty);
	return conversion;
}

static ast_node_t *
analyse(ast_node_t *node)
{
	if (!node) {
		return NULL;
	}

	if (!IS_VALUE_NODE(node)) {
		for (int i = 0; i < node->children_nr; i++) {
			node->children[i] = type_analysis(node->children[i]);
		}
	}

	switch (NODE_TY(node)) {

	case AST_VALUE_REAL:
		error(node, "Floating point numbers are not presently supported");
		return TYPE_REAL;

	case AST_VALUE_ID:
		if (node->annotations) {
			// Typ aktualisieren
			set_type(node, ((symtab_entry_t *) node->annotations)->ast_flags);
		}
		break;

	case AST_NODE_FUNAPP:
		if (NODE_TY(node->children[0]) == AST_VALUE_ID) {
			// Funktionsaufruf
			symtab_entry_t *function = node->children[0]->annotations;
			node->annotations = function;

			int min_params = function->parameters_nr;
			ast_node_t *actuals = node->children[1];
			if (actuals->children_nr != min_params) {
				int actual_params_nr = node->children[1]->children_nr;
				if (actual_params_nr < min_params) {
					min_params = actual_params_nr;
				}
				error(node, "expected %d parameters, found %d", min_params, actual_params_nr);
			}

			for (int i = 0; i < function->parameters_nr; i++) {
				short expected_type = function->parameter_types[i];
				actuals->children[i] = require_type(actuals->children[i], expected_type);
			}

			set_type(node, function->ast_flags);

		} else if (NODE_TY(node->children[0]) == AST_NODE_MEMBER) {
			// Methoden-Aufruf
			ast_node_t *receiver = node->children[0]->children[0];
			ast_node_t *selector_node = node->children[0]->children[1];
			ast_node_t *actuals = node->children[1];
			symtab_entry_t *selector = selector_node->annotations;
			node->annotations = selector;

			for (int i = 0; i < actuals->children_nr; i++) {
				actuals->children[i] = require_type(actuals->children[i], method_call_param_type);
			}

			if (NODE_FLAGS(receiver) & (TYPE_INT | TYPE_REAL)) {
				error(node, "method received must be an object");
			}

			ast_node_free(node, 0);
			node = ast_node_alloc_generic(AST_NODE_METHODAPP | method_call_return_type,
						      require_type(receiver, TYPE_OBJ),
						      selector_node,
						      actuals);
			return node;
		} else {
			error(node, "calls only permitted on functions and methods!");
		}
		break;

	case AST_NODE_VARDECL:
	case AST_NODE_ASSIGN:
		node->children[1] = require_type(node->children[1], NODE_FLAGS(node->children[0]) & TYPE_FLAGS);
		node->children[0] = require_lvalue(node->children[0],
						   NODE_TY(node) == AST_NODE_VARDECL);
		break;

	case AST_NODE_ARRAYITEMS:
		for (int i = 0; i < node->children_nr; i++) {
			node->children[i] = require_type(node->children[i], array_storage_type);
		}
		break;

	case AST_NODE_ARRAYVAL:
		node->children[1] = require_type(node->children[1], array_storage_type);
		set_type(node, TYPE_OBJ);
		break;

	case AST_NODE_ISINSTANCE: {
		symtab_entry_t *classref = (symtab_entry_t *) node->children[1]->annotations;
		if (classref) {
			if (!(classref->symtab_flags & SYMTAB_TY_CLASS)) {
				error(node, "`isinstance' on non-class (%s)", classref->name);
			}
		} // ansonsten hat die Namensanalyse bereits einen Fehler gemeldet
	case AST_NODE_ISPRIMTY:
		set_type(node, TYPE_INT);
		break;

	case AST_NODE_ARRAYSUB:
		if (NODE_FLAGS(node->children[0]) & (TYPE_INT | TYPE_REAL)) {
			error(node, "array subscription must be on object, not number");
		}
		node->children[1] = require_type(node->children[1], TYPE_INT);
		set_type(node, array_storage_type);
		break;

	case AST_NODE_WHILE:
		node->children[0] = require_type(node->children[0], TYPE_INT);
		break;

	case AST_NODE_IF:
		node->children[0] = require_type(node->children[0], TYPE_INT);
		break;

	default:
	}

	return node;
}

void
type_analysis(ast_node_t *node)
{
	analyse(node);
}
