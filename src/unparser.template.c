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
#include <stdbool.h>
#include <stdio.h>
#include <stdarg.h>

#include "ast.h"
#include "symbol-table.h"

void
ast_print_flags(FILE *file, int ty)
{
$$PRINT_FLAGS$$
}

static void
print_id_string(FILE *file, int id, bool print_long)
{
	switch (id) {
$$PRINT_IDS$$
	default: {
		symtab_entry_t *e = symtab_lookup(id);
		if (!e) {
			fprintf(file, "(UNKNOWN-SYM[%d])", id);
		} else {
			if (print_long) {
				fprintf(file, "(SYM[%d] ", id);
			}
			symtab_entry_name_dump(file, e);
			if (print_long) {
				fprintf(file, ")");
			}
		}
	}
	}
}


static void
print_tag_string(FILE *file, int tag)
{
	switch (tag & AST_NODE_MASK) {
	case AST_VALUE_ID:
		return;
$$PRINT_TAGS$$
	default:
		fprintf(file, "<unknown tag: 0x%x>", tag & AST_NODE_MASK);
	}
}

static void
print_value_node(FILE *file, ast_value_node_t *node)
{
	switch (node->type & AST_NODE_MASK) {
	case AST_VALUE_ID:
		print_id_string(file, AV_ID(node), true);
		return;
$$PRINT_VNODES$$
	default:
		fprintf(file, "<unknown value node type 0x%x>", node->type & AST_NODE_MASK);
	}
}

static void
dump_recursively(FILE *file, int indent, ast_node_t *node, int flags)
{
	if (!node) {
		fputs("null", file);
		return;
	}

	int type = node->type & AST_NODE_MASK;
	if (type <= AST_VALUE_MAX) {
		print_value_node(file, (ast_value_node_t *) node);
		if (node->type != AST_VALUE_ID) {
			fputs(":", file);
		}
		print_tag_string(file, type);
		if (flags & AST_NODE_DUMP_ADDRESS) {
			fprintf(file, "@%p", node);
		}
		if ((flags & AST_NODE_DUMP_STORAGE) && (node->storage >= 0) ) {
			fprintf(file, "$_%i", node->storage);
		}
		if (flags & AST_NODE_DUMP_FLAGS) {
			ast_print_flags(file, node->type);
		}
		cfg_analyses_print(file, node, flags);
		return;
	}

	fputs("(", file);
	print_tag_string(file, type);
	if (flags & AST_NODE_DUMP_ADDRESS) {
		fprintf(file, "@%p", node);
	}
	if ((flags & AST_NODE_DUMP_STORAGE) && (node->storage >= 0) ) {
		fprintf(file, "$_%i", node->storage);
	}
	if (flags & AST_NODE_DUMP_FLAGS) {
		ast_print_flags(file, node->type);
	}
	cfg_analyses_print(file, node, flags);
	const int long_format = (flags & AST_NODE_DUMP_FORMATTED) && node->children_nr;
	if (long_format) {
		fputs("\n", file);
	}

	for (int i = 0; i < node->children_nr; i++) {
		if (!(flags & AST_NODE_DUMP_NONRECURSIVELY)) {
			if (long_format) {
				for (int d = 0; d <= indent; d++) {
					fputs("  ", file);
				}
			} else {
				fputs(" ", file);
			}
			dump_recursively(file, indent + 1, node->children[i], flags);
			if (long_format) {
				fputs("\n", file);
			}
		} else {
			fputs(" #", file);
		}
	}
	if (long_format) {
		for (int d = 0; d < indent; d++) { fputs("  ", file); }
	}
	fputs(")", file);
}

void
ast_node_dump(FILE *file, ast_node_t *node, int flags)
{
	dump_recursively(file, 0, node, flags);
}


static void
print_primty(FILE *file, int type)
{
	switch (type) {
	case TYPE_OBJ:
		fprintf(file, "obj");
		break;
	case TYPE_VAR:
		fprintf(file, "var");
		break;
	case TYPE_INT:
		fprintf(file, "int");
		break;
	default:
		fprintf(file, "?");
	}
}


// pretty-printer
void
ast_node_print(FILE *file, ast_node_t *node, int recursive)
{
	if (node == NULL) {
		return;
	}
	const int node_ty = NODE_TY(node);

	if (node_ty == AST_VALUE_ID) {
		print_id_string(file, AV_ID(node), false);
	} else if (node_ty <= AST_VALUE_MAX) {
		print_value_node(file, (ast_value_node_t *) node);
	} else {
		switch (node_ty) {

		case AST_NODE_METHODAPP:
		case AST_NODE_MEMBER:
			ast_node_print(file, node->children[0], recursive);
			fprintf(file, ".");
			for (int i = 1; i < node->children_nr; ++i) {
				ast_node_print(file,node->children[i], recursive);
			}
			break;

		case AST_NODE_BREAK:
			fprintf(file, "break");
			break;

		case AST_NODE_CONTINUE:
			fprintf(file, "continue");
			break;

		case AST_NODE_ISPRIMTY: {
			int type = node->type & TYPE_FLAGS;
			ast_node_print(file, node->children[0], recursive);
			fprintf(file, " is ");
			print_primty(file, type);
			break;
		}

		case AST_NODE_SKIP:
			fprintf(file, ";");
			break;

		case AST_NODE_ACTUALS:
			//e fall through
		case AST_NODE_FORMALS: {
			fprintf(file, "(");
			for (int i = 0; i < node->children_nr; ++i) {
				if (i > 0) {
					fprintf(file, ", ");
				}
				ast_node_print(file, node->children[i], recursive);
			}
			fprintf(file, ")");
			break;
		}

		case AST_NODE_ARRAYSUB:
			ast_node_print(file, node->children[0], recursive);
			fprintf(file, "[");
			ast_node_print(file, node->children[1], recursive);
			fprintf(file, "]");
			break;

		case AST_NODE_VARDECL:
			print_primty(file, node->type & TYPE_FLAGS);
			fprintf(file, " ");
			ast_node_print(file, node->children[0], recursive);
			if (node->children_nr > 1) {
				fprintf(file, " = ");
				ast_node_print(file, node->children[1], recursive);
			}
			break;

		case AST_NODE_NULL:
			fprintf(file, "NULL");
			break;

		case AST_NODE_RETURN:
			fprintf(file, "return");
			if (node->children_nr > 0) {
				fprintf(file, " ");
				ast_node_print(file, node->children[0], recursive);
			}
			break;

		case AST_NODE_ASSIGN:
			ast_node_print(file, node->children[0], recursive);
			fprintf(file, " := ");
			ast_node_print(file, node->children[1], recursive);
			break;

		case AST_NODE_NEWINSTANCE:
		case AST_NODE_FUNAPP:
			ast_node_print(file, node->children[0], recursive);
			ast_node_print(file, node->children[1], recursive);
			break;

		case AST_NODE_FUNDEF:
			//e fall through
		case AST_NODE_CLASSDEF:
			print_primty(file, node->type & TYPE_FLAGS);
			fprintf(file, " ");
			for (int i = 0; i < node->children_nr; ++i) {
				ast_node_print(file, node->children[i], recursive);
			}
			break;

		case AST_NODE_ISINSTANCE:
			ast_node_print(file, node->children[0], recursive);
			fprintf(file, " is ");
			ast_node_print(file, node->children[1], recursive);
			break;

		case AST_NODE_ARRAYLIST:
			for (int i = 0; i < node->children_nr; ++i) {
				if (i > 0) {
					fprintf(file, ", ");
				}
				ast_node_print(file, node->children[i], recursive);
			}
			break;

		case AST_NODE_ARRAYVAL:
			fprintf(file, "[");
			ast_node_print(file, node->children[0], recursive);
			if (node->children[0] && node->children[0]->children_nr) {
				fprintf(file, ",");
			}
			if (node->children_nr > 1) {
				fprintf(file, "/");
				ast_node_print(file, node->children[1], recursive);
			}
			fprintf(file, "]");
			break;

		case AST_NODE_IF:
			fprintf(file, "if ");
			ast_node_print(file, node->children[0], recursive);
			if (node->children[1]) {
				ast_node_print(file, node->children[1], recursive);
			}
			if (node->children[2]) {
				fprintf(file, " else ");
				ast_node_print(file, node->children[2], recursive);
			}
			break;

		case AST_NODE_WHILE:
			fprintf(file, "while (");
			ast_node_print(file, node->children[0], recursive);
			fprintf(file, ") ");
			ast_node_print(file, node->children[1], recursive);
			break;

		case AST_NODE_BLOCK:
			fprintf(file, "{ ");
			for (int i = 0; i < node->children_nr; ++i) {
				ast_node_print(file, node->children[i], recursive);
				fprintf(file, "; ");
			}
			fprintf(file, "}");
		default:
			ast_node_dump(file, node, 0);
		}
	}
}

