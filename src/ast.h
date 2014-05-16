/* ** AUTOMATICALLY GENERATED.  DO NOT MODIFY. ** */
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

#ifndef _ATTOL_AST_H
#define _ATTOL_AST_H

#include <stdio.h>
#include <stdlib.h>

/* Abstrakter Syntaxbaum (AST).  Repräsentiert Programme nach dem Parsen. */

// AST-Knotentypen
#define AST_ILLEGAL      0x00
#define AST_NODE_MASK    0xff
#define AST_VALUE_INT    0x01
#define AST_VALUE_STRING 0x02
#define AST_VALUE_REAL   0x03
#define AST_VALUE_NAME   0x04
#define AST_VALUE_ID     0x05
#define AST_VALUE_MAX    0x06
#define AST_NODE_FUNAPP  0x06

#define IS_VALUE_NODE(n) ((n)->type <= AST_VALUE_MAX)

#define AV_INT(n) (((ast_value_node_t *)(n))->v.num)
#define AV_REAL(n) (((ast_value_node_t *)(n))->v.real)
#define AV_NAME(n) (((ast_value_node_t *)(n))->v.str)
#define AV_ID(n) (((ast_value_node_t *)(n))->v.ident)
#define AV_STRING(n) (((ast_value_node_t *)(n))->v.str)

// AST-Tags (Zusatzinformationen)

// Ende der AST-Tags

// Eingebaute Bezeichner.  NAME: nicht aufgeloester Name, ID: aufgeloester Name
#define BUILTIN_OP_ADD     -1
#define BUILTIN_OP_DIV     -2
#define BUILTIN_OP_MUL     -3
#define BUILTIN_OP_SUB     -4
#define BUILTIN_OP_TEST_EQ -5
#define BUILTIN_OP_TEST_LE -6
#define BUILTIN_OP_TEST_LT -7

#define BUILTIN_OPS_NR 7


typedef struct ast_node {
	unsigned short type;
	unsigned short children_nr;
	void *annotations; // Optional annotations (name analysis, type analysis etc.)
	struct ast_node * children[0]; // Kindknoten
} ast_node_t;

typedef union {
	double real;
	int ident;
	signed long int num;
	char * str;
} ast_value_union_t;

// Wert-Knoten; hat gleiche Struktur wie AST-Knoten (bis auf Kinder)
typedef struct {
	unsigned short type;
	unsigned short _reserved;
	void *annotations;
	ast_value_union_t v;
} ast_value_node_t;

/**
 * Allocates a single AST node.
 *
 * @param type AST node type
 * @param children_nr Number of children that follow
 * @param ... Sequence of child_nr ast_node_t* objects
 * @return A freshly allocated AST node with the requisite settings
 */
ast_node_t *
ast_node_alloc_generic(int type, int children_nr, ...);
ast_node_t *
value_node_alloc_generic(int type, ast_value_union_t value);

/**
 * Frees AST nodes.
 *
 * @param node The node to free
 * @param recursive Whether to free recursively
 */
void
ast_node_free(ast_node_t *node, int recursive);


/**
 * Recursively duplicates a node structure
 *
 * Strings are also duplicated.
 *
 * @param node The node to clone
 * @return A clone
 */
ast_node_t *
ast_node_clone(ast_node_t *node);

/**
 * Pretty-prints AST nodes.
 *
 * @param file The output stream to print to
 * @param node The node to print out
 * @param recursive Whether to print recursively
 */
void
ast_node_print(FILE *file, ast_node_t *node, int recursive);

/**
 * Dumps AST nodes (no pretty-printing, raw AST).
 *
 * @param file The output stream to print to
 * @param node The node to print out
 * @param recursive Whether to print recursively
 */
void
ast_node_dump(FILE *file, ast_node_t *node, int recursive);

#endif // !defined(_CMINOR_SRC_AST_H)
