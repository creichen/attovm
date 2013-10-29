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

#include <assert.h>

#include "rd-parser.h"

typedef ast_node_t node_t;

int yylex();
yylval_t yylval;
extern int yy_xflag;

void
yyerror(const char *str)
{
	fprintf(stderr, "parse error: %s", str);
}

#define PUSHBACK_BUFFER_SIZE 3

static ast_node_t *pushed_back_nodes[PUSHBACK_BUFFER_SIZE]; // Temporaerer Knoten fuer peek
static int pushed_back_tokens[PUSHBACK_BUFFER_SIZE];
int pushed_back_tokens_nr = 0;

static void
push_back(int token, node_t *node)
{
	assert(pushed_back_tokens_nr < PUSHBACK_BUFFER_SIZE);
	pushed_back_nodes[pushed_back_tokens_nr] = node;
	pushed_back_tokens[pushed_back_tokens_nr] = token;
	++pushed_back_tokens_nr;
}

static int
obtain_next_node_from_lexer(ast_node_t **node_ptr)
{
	int token_t = yylex();

	switch (token_t) {
	case STRING:
		*node_ptr = value_node_alloc_generic(AST_VALUE_STRING, (ast_value_union_t) { .s = yylval.str });
		break;
	case INT:
		*node_ptr = value_node_alloc_generic(AST_VALUE_INT, (ast_value_union_t) { .i = yylval.sll });
		break;
	case UINT:
		*node_ptr = value_node_alloc_generic(AST_VALUE_UINT | yy_xflag, (ast_value_union_t) { .u = yylval.ull });
		break;
	case BOOL:
		*node_ptr = value_node_alloc_generic(AST_VALUE_BOOL, (ast_value_union_t) { .i = yylval.c });
		break;
	case CHAR:
		*node_ptr = value_node_alloc_generic(AST_VALUE_CHAR | yy_xflag, (ast_value_union_t) { .i = yylval.c });
		break;
	case REAL:
		*node_ptr = value_node_alloc_generic(AST_VALUE_REAL | yy_xflag, (ast_value_union_t) { .r = yylval.real });
		break;
	case NAME:
		*node_ptr = value_node_alloc_generic(AST_VALUE_NAME, (ast_value_union_t) { .s = yylval.str });
		break;
	default:
		*node_ptr = NULL; // syntax only
	}

	return token_t;
}

static int
get_next_token(node_t **node_ptr)
{
	if (pushed_back_tokens_nr) {
		--pushed_back_tokens_nr;
		*node_ptr = pushed_back_nodes[pushed_back_tokens_nr];
		return pushed_back_tokens[pushed_back_tokens_nr];
	}

	return obtain_next_node_from_lexer(node_ptr);
}


static int
peek()
{
	node_t *node;
	const int next_token = get_next_token(&node);
	push_back(next_token, node);
	return next_token;
}

static int
next()
{
	node_t *node;
	const int next_token = get_next_token(&node);
	assert(node == NULL);
	return next_token;
}

static int
next_n(node_t **node_ptr)
{
	const int next_token = get_next_token(node_ptr);
	assert(*node_ptr == NULL);
	return next_token;
}

static int
expect(int token_type)
{
	if (peek() != token_type) {
		return 0;
	}
	return next();
}

static int
expect_n(int token_type, node_t **node_ptr)
{
	if (peek() != token_type) {
		return 0;
	}
	return next_n(node_ptr);
}

// expr ::= add-expr
// add-expr ::= add-expr ADDOP mul-expr
//            | mul-expr
// mul-expr ::= mul-expr MULOP mul-expr
//            | constant
