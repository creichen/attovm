/***************************************************************************
 dlexer.l Copyright (C) 2013 Christoph Reichenbach


 This program may be modified and copied freely according to the terms of
 the GNU general public license (GPL), as long as the above copyright
 notice and the licensing information contained herein are preserved.

 Please refer to www.gnu.org for licensing details.

 This work is provided AS IS, without warranty of any kind, expressed or
 implied, including but not limited to the warranties of merchantibility,
 noninfringement, and fitness for a specific purpose. The author will not
 be held liable for any damage caused by this work or derivatives of it.

 By using this source code, you agree to the licensing terms as stated
 above.


 Please contact the maintainer for bug reports or inquiries.

 Current Maintainer:

    Christoph Reichenbach (CR) <jameson@linuxgames.com>

***************************************************************************/

%{
#include <math.h>
#include <string.h>

#include "ast.h"
#include "parser.h"
#include "chash.h"
#include "lexer-support.h"

void yyerror(const char *s); // Fehler beim Lexen/Parsen

int yy_line_nr = 1;

int yy_xflag = 0; // Erweiterungsflag: TAG_AST_*
%}

DIGIT [0-9]
HEXDIGIT [0-9a-fA-F]
IDENTIFIER [a-zA-Z_][a-zA-Z0-9_]*

%option always-interactive
%%

$$RULES$$

[[:blank:]]+ // Eat whitespace
\n	{ // Eat whitespace
	++yy_line_nr;
}

<<EOF>> {
        yy_delete_buffer( YY_CURRENT_BUFFER );
        yyterminate();
}

"//"[^\n]*\n {
	++yy_line_nr;
}

<*>.|\n {
	fprintf(stderr, "Invalid input token: '%s'\n", yytext);
	yyerror("File format error");
}

%%

int
yywrap(void)
{
	return 1;
}
