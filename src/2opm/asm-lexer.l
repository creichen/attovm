/***************************************************************************
 asm-lexer.l Copyright (C) 2014 Christoph Reichenbach


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
#include "asm.h"
#include <math.h>
#include <string.h>

void yyerror(const char *s); // Fehler beim Lexen/Parsen

// Map `n' to `\n' etc.
char* unescape_string(char *text);

char* mk_unique_string(char *text);

int yy_line_nr = 1;
%}

DIGIT [0-9]
HEXDIGIT [0-9a-fA-F]
IDENTIFIER [a-zA-Z_][a-zA-Z0-9_]*

%option always-interactive
%%

".data" {
	return T_S_DATA;
}

".text" {
	return T_S_TEXT;
}

".byte" {
	return T_S_BYTE;
}

".word" {
	return T_S_WORD;
}

".asciiz" {
	return T_S_ASCIIZ;
}

":" {
	return ':';
}

"," {
	return ',';
}

"$" {
	return '$';
}

"(" {
	return '(';
}

")" {
	return ')';
}

\"(\\.|[^\"\\])*\" {
	yylval.str = unescape_string(yytext);
	return T_STR;
}


0x{HEXDIGIT}+ {
	yylval.num = strtoull(yytext + 2, NULL, 16);
	return T_UINT;
}

{DIGIT}+ {
	yylval.num = strtoll(yytext, NULL, 10);
	return T_UINT;
}

-{DIGIT}+ {
	yylval.num = strtoll(yytext, NULL, 10);
	return T_INT;
}

{IDENTIFIER} {
	yylval.str = mk_unique_string(yytext);
	return T_ID;
}



[[:blank:]]+ {} // Eat whitespace

\n	{ // Eat whitespace
	++yy_line_nr;
	return '\n';
}

<<EOF>> {
        yy_delete_buffer( YY_CURRENT_BUFFER );
        yyterminate();
}

";"[^\n]*\n {
	++yy_line_nr;
	return '\n';
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
