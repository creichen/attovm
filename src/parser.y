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

    Christoph Reichenbach (CR) <jameson@linuxgames.com>

***************************************************************************/

%{
#include <stdlib.h>
#include <string.h>

#include "ast.h"

extern int yy_xflag; // extension flag
int yylex();
void yyerror(const char *);

%}

%union {
	ast_node_t *node;
	unsigned long long ull;
	signed long long sll;
	double real;
	char c;
	char *str;
	// node queue
}

%token <str> STRING NAME
%token <sll> UINT
%token <ull> INT
%token <real> REAL
%token <c> BOOL CHAR
%type <node> program name value


%%

program	: value
		{ $$ = $1; }
	| name
		{ $$ = $1; }
	;

value	: STRING
		{ $$ = value_node_alloc_generic(AST_VALUE_STRING, (ast_value_union_t) { .s = $1 }); }
	| INT
		{ $$ = value_node_alloc_generic(AST_VALUE_INT, (ast_value_union_t) { .i = $1 }); }
	| UINT
		{ $$ = value_node_alloc_generic(AST_VALUE_UINT | yy_xflag, (ast_value_union_t) { .u = $1 }); }
	| BOOL
		{ $$ = value_node_alloc_generic(AST_VALUE_BOOL, (ast_value_union_t) { .i = $1 }); }
	| CHAR
		{ $$ = value_node_alloc_generic(AST_VALUE_CHAR | yy_xflag, (ast_value_union_t) { .i = $1 }); }
	| REAL
		{ $$ = value_node_alloc_generic(AST_VALUE_REAL | yy_xflag, (ast_value_union_t) { .r = $1 }); }
	;

name	: NAME
		{ $$ = value_node_alloc_generic(AST_VALUE_NAME, (ast_value_union_t) { .s = $1 }); }
	;


%%

void
yyerror(const char *str)
{
	fprintf(stderr, "parse error: %s", str);
}
