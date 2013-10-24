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

#ifndef _CMINOR_SRC_AST_H
#define _CMINOR_SRC_AST_H

#include <stdio.h>
#include <stdlib.h>

/* Abstrakter Syntaxbaum (AST).  Repräsentiert Programme nach dem Parsen. */

// AST-Knotentypen
#define TYPE_MASK		0xff	// Nur die ersten 8 Bits speichern AST-Knotentypen
#define AST_ILLEGAL		0x00
#define AST_VALUE_INT		0x01
#define AST_VALUE_UINT		0x02
#define AST_VALUE_BOOL		0x03
#define AST_VALUE_REAL		0x04
#define AST_VALUE_CHAR		0x05
#define AST_VALUE_STRING	0x06
#define AST_VALUE_NAME		0x07	// identifier stored as `s'
#define AST_VALUE_BUILTIN	0x08	// reference to builtin entity; encoded as `ident'

#define AST_VALUE_MAX		AST_VALUE_BUILTIN

#define AST_NODE_NAME		0x10	// qualified name, expressed as array of AST_VALUE_NAME entities
#define AST_NODE_FUNAPP		0x11	// [0] is function ref, [1] is argument
/* #define AST_NODE_PACKAGE	0x12	// [0] is name, [1] is SET of declarations */
#define AST_NODE_SET		0x20	// something in curly braces.  [0] is car, [1] is cdr (may be NULL).
#define AST_NODE_SEQ		0x21	// something in brackets.  [0] is car, [1] is cdr (may be NULL).
#define AST_NODE_TUPLE		0x22	// array-style
#define AST_NODE_VARDECL	0x30	// [0] is type or NULL, [1] is name, [2] is initialiser or NULL
#define AST_NODE_ASSIGN		0x40	// [0] is lvalue, [1] is accumulating builtin or NULL, [2] is rhs/rvalue
#define AST_NODE_PREFIXOP	0x41	// [0] is lvalue, [1] is accumulating builtin.  Other operand is implicitly 1.
#define AST_NODE_POSTFIXOP	0x42	// [0] is lvalue, [1] is accumulating builtin.  Other operand is implicitly 1.
#define AST_NODE_OR		0x43	// short-circuit `or'
#define AST_NODE_AND		0x44	// short-circuit `and'
#define AST_NODE_WHILE		0x45	// [0] is cond, [1] is body
#define AST_NODE_DOWHILE	0x46	// [0] is cond, [1] is body
#define AST_NODE_FOR		0x47	// [0,1,2] are inits [3] is body
#define AST_NODE_IF		0x48	// [0] is cond, [1] is true-branch, [2] is false-branch
/* #define AST_NODE_CASE		0x49	// [0] is match-value, [1] is body */
/* #define AST_NODE_SWITCH		0x4a	// [0] is cond, [1] is NODE_SET of NODE_CASE */
#define AST_NODE_JUMPLABEL	0x4b	// [0] is label name, [1] is stmt

#define IS_VALUE_NODE(n) ((n)->type <= AST_VALUE_MAX)

#define AV_STRING(n) (((ast_value_node_t *)(n))->v.s)
#define AV_INT(n) (((ast_value_node_t *)(n))->v.i)
#define AV_IDENT(n) (((ast_value_node_t *)(n))->v.ident)
#define AV_UINT(n) (((ast_value_node_t *)(n))->v.u)
#define AV_REAL(n) (((ast_value_node_t *)(n))->v.r)

// AST-Tags (Zusatzinformationen)
#define TAG_AST_EXPR_LHS_PAREN	(1 << 8)	// Infix-Ausdrücke: links geklammert
#define TAG_AST_EXPR_RHS_PAREN	(1 << 9)	// Infix-Ausdrücke: rechts geklammert
#define TAG_AST_HEX_REPR	(1 << 10)	// Literal ist in Hexadezimaldarstellung repräsentiert


typedef struct ast_node {
	unsigned short type;
	unsigned short children_nr;
	void *annotations; // Optional annotations (name analysis, type analysis etc.)
	struct ast_node * children[0]; // Kindknoten
} ast_node_t;

typedef union {
	int ident;
	long long i;
	unsigned long long u;
	char *s;
	double r;
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
ast_node_t *ast_node_alloc_generic(int type, int children_nr, ...);
ast_node_t *value_node_alloc_generic(int type, ast_value_union_t value);

/**
 * Frees AST nodes.
 *
 * @param node The node to free
 * @param recursive Whether to free recursively
 */
void ast_node_free(ast_node_t *node, int recursive);

/**
 * Pretty-prints AST nodes.
 *
 * @param file The output stream to print to
 * @param node The node to print out
 * @param recursive Whether to print recursively
 */
void ast_node_print(FILE *file, ast_node_t *node, int recursive);

/**
 * Dumps AST nodes (no pretty-printing, raw AST).
 *
 * @param file The output stream to print to
 * @param node The node to print out
 * @param recursive Whether to print recursively
 */
void ast_node_dump(FILE *file, ast_node_t *node, int recursive);

#endif // !defined(_CMINOR_SRC_AST_H)
