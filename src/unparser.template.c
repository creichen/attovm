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
#include <stdarg.h>
#include <assert.h>

#include "ast.h"
#include "symbol-table.h"

void
ast_print_flags(FILE *file, int ty)
{
$$PRINT_FLAGS$$
}

static void
print_id_string(FILE *file, int id)
{
	switch (id) {
$$PRINT_IDS$$
	default: {
		symtab_entry_t *e = symtab_lookup(id);
		if (!e) {
			fprintf(file, "(UNKNOWN-SYM[%d])", id);
		} else {
			fprintf(file, "(SYM[%d] ", id);
			symtab_entry_name_dump(file, e);
			fprintf(file, ")");
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
		print_id_string(file, AV_ID(node));
		return;
$$PRINT_VNODES$$
	default:
		fprintf(file, "<unknown value node tpe 0x%x>", node->type & AST_NODE_MASK);
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
	*((int *)NULL) = 0;
	dump_recursively(file, 0, node, flags);
}


// pretty-printer
void
ast_node_print(FILE *file, ast_node_t *node, int recursive)
{
	// FIXME
}

