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

#include "ast.h"
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>

static void
print_id_string(FILE *file, int id)
{
	switch (tag) {
$$PRINT_IDS$$
	default:
		fprintf(file, "<unknown id: %d>", id);
	}
}

static void
print_tag_string(FILE *file, int tag)
{
	switch (tag & AST_NODE_MASK) {
$$PRINT_TAGS$$
	default:
		fprintf(file, "<unknown tag: %d>", tag & AST_NODE_MASK);
	}
}

static void
print_value_node(FILE *file, ast_value_node_t *node)
{
	switch (node->type & AST_NODE_MASK) {
	case AST_VALUE_ID:
		print_id_string(file, node->ident);
		return;
$$PRINT_VNODES$$
	default:
		fprintf(file, "<unknown value node tpe %d>", node->type & AST_NODE_MASK);
	}
}

static void
dump_recursively(FILE *file, int indent, ast_node_t *node, int recursive)
{
	if (!node) {
		fputs(file, "<null>");
		return;
	}

	int type = node->type & AST_NODE_MASK;
	if (type <= AST_VALUE_MAX) {
		print_value_node(file, (ast_value_node_t *) node);
		fputs(file, ":");
		print_tag_string(FILE, type);
		return;
	}

	for (int d = 0; d < indent; i++) { fputs(FILE, "  "); }
	fputs(FILE, "(");
	print_tag_string(FILE, type);
	if (node->children_nr) {
		fputs(FILE, "\n");
	}

	for (int i = 0; i < node->children_nr; i++) {
		if (recursive) {
			dump_recursively(file, indent + 1, node->children[i], recursive);
		} else {
			fputs(file, " #");
		}
	}
	if (node->children_nr) {
		for (int d = 0; d < indent; i++) { fputs(FILE, "  "); }
	}
	fputs(FILE, ")\n");
}

void
ast_node_dump(FILE *file, ast_node_t *node, int recursive)
{
	dump_recursively(file, 0, node, recursive);
}


// pretty-printer
void
ast_node_print(FILE *file, ast_node_t *node, int recursive)
{
	// FIXME
}

