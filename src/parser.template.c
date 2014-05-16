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

#include "parser.h"
#include "ast.h"

typedef ast_node_t node_t;

int yylex();
yylval_t yylval;
extern int yy_xflag;

void
yyerror(const char *str)
{
	fprintf(stderr, "parse error: %s", str);
}

typedef struct {
	int nodes_nr;
	int size;
	ast_node_t **nodes;
} node_vector_t;

static node_vector_t
make_vector()
{
	node_vector_t nv;
	const int initial_size = 8;
	nv.nodes = malloc(sizeof(node_vector_t *) + sizeof(ast_node_t *) * initial_size);
	nv.size = initial_size;
	nv.nodes_nr = 0;
	return nv;
}

static void
add_to_vector(node_vector_t *nv, ast_node_t *n)
{
	if (nv->nodes_nr >= nv->size) {
		nv->size <<= 1;
		nv->nodes = realloc(nv->nodes, sizeof(node_vector_t *) + sizeof(ast_node_t *) * nv->size);
	}
	nv->nodes[nv->nodes_nr++] = n;
}


static void
free_vector(node_vector_t *nv)
{
	free(nv);
}


// Stapel für backtracking
typedef struct {
	int ty;
	ast_node_t *node;
} node_entry_t;

static node_entry_t *node_stack = NULL;
static int node_stack_pos = 0;
static int node_stack_size = 0;


static int error_status = 0;
#define PARSE_ERROR -1

static int
next_token(ast_node_t **node_ptr)
{
	if (error_status) {
		return PARSE_ERROR;
	}
	if (node_stack_pos > 0) {
		node_entry_t *e = node_stack + --node_stack_pos;
		*node_ptr = e->node;
		return e->ty;
	}
	int token_t = yylex();

	switch (token_t) {
$$VALUE_TOKEN_DECODING$$
	default:
		*node_ptr = NULL;
	}
	return token_t;
}

static void
push_back(int ty, ast_node_t * node)
{
	if (node_stack_pos >= node_stack_size) {
		if (!node_stack) {
			node_stack_size = 16;
			node_stack = malloc(sizeof(node_entry_t) * node_stack_size);
		} else {
			node_stack = realloc(node_stack, sizeof(node_entry_t) * (node_stack_size <<= 1));
		}
	}
	node_stack[node_stack_pos].ty = ty;
	node_stack[node_stack_pos].node = node;
}

static int
accept(int expected, ast_node_t **node_ptr)
{
	int ty = next_token(node_ptr);
	if (expected == ty) {
		return 1;
	} else {
		push_back(ty, *node_ptr);
		*node_ptr = NULL;
		return 0;
	}
}

static int
peek()
{
	ast_node_t *ptr;
	int t = next_token(&ptr);
	push_back(t, ptr);
	return t;
}

static void
parse_error(char *message)
{
	yyerror(message);
	error_status = 1;
}

static void
clear_parse_error(int until_token)
{
	ast_node_t *node;
	error_status = 0;
	int token;
	do {
		node = NULL;
		token = next_token(&node);
		if (node) {
			ast_node_free(node, 1);
		}
	} while (token && token != until_token);
	// until EOF or `terminator token'
}

$$PARSING$$
