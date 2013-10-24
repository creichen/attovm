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

void yyerror(const char *s); // Fehler beim Lexen/Parsen

// Bilde `n' auf `\n' etc.  ab. 
// Bei Fehler:  gibt '0' zurück.
static char unescape_char(char c);

int yy_xflag = 0; // Erweiterungsflag: TAG_AST_*
%}

DIGIT [0-9]
HEXDIGIT [0-9a-fA-F]
IDENTIFIER [a-zA-Z_][a-zA-Z0-9_]*

%option always-interactive
%%

((({DIGIT}*"."{DIGIT}+)|({DIGIT}+"."))([eE][+-]?{DIGIT}+)?)|({DIGIT}+[eE][+-]?{DIGIT}+) {
	yy_xflag = 0;
	yylval.real = strtod(yytext, NULL);
	return REAL;
}

0x(({HEXDIGIT}*"."{HEXDIGIT}+)|({HEXDIGIT}+"."))([pP][+-]?{DIGIT}+) {
	yy_xflag = TAG_AST_HEX_REPR;
	yylval.real = strtod(yytext, NULL);
	return REAL;
}

0x{HEXDIGIT}+[uU]? {
	yy_xflag = TAG_AST_HEX_REPR;
	yylval.ull = strtoull(yytext + 2, NULL, 16);
	return UINT;
}

{DIGIT}+[uU] {
	yy_xflag = 0;
	yylval.ull = strtoull(yytext, NULL, 10);
	return UINT;
}

{DIGIT}+ {
	yylval.sll = atoll(yytext);
	return INT;
}

true {
	yylval.c = 1;
	return BOOL;
}

false {
	yylval.c = 0;
	return BOOL;
}

'\\.' {
	yy_xflag = 0;
	yylval.c = unescape_char(yytext[2]);
	if (yylval.c == 0) {
		yyerror("Invalid escaped character");
	}
	return CHAR;
}

'.' {
	yy_xflag = 0;
	yylval.c = yytext[1];
	return CHAR;
}

'\\0x{HEXDIGIT}+' {
	yy_xflag = TAG_AST_HEX_REPR;
	yylval.c = strtoull(yytext + 4, NULL, 16);
	return CHAR;
}

\"(\\.|[^\"\\])*\" {
	char * result = yylval.str = strdup(yytext + 1);
	size_t length = strlen(result);

	result[--length] = 0; // entferne '"' am Ende

	// Wir scuhen jetzt nach Escape-Zeichen '\'.  Diese ersetzen wir passend.  Dabei müssen wir die jeweils
	// folgenden Speicherblöcke weiter nach vorne in den Speicher verschieben.
	char *escaped = strchr(result, '\\');
	char *dest = escaped; // Block-Verschiebeziel
	size_t aggregate_skip = 0; // 

	// Falls ein Zeichen mit '\' escaped wurde, wechseln wir in einen langsameren Übersetzungsmodus:

	while (escaped) {
		char replacement = unescape_char(escaped[1]);
		if (replacement) {
			*dest++ = replacement;
			aggregate_skip += 1;
			escaped += 2;
		} else if (escaped[1] == '0' && escaped[2] == 'x') {
			char *end;
			*dest++ = strtol(escaped + 3, &end, 16);
			aggregate_skip += end - escaped - 1;
			escaped = end;
		} else {
			yyerror("Illegal escape sequence in string");
			*dest++ = ' ';
			escaped += 2;
			aggregate_skip += 1;
		}

		// Fertig mit Übersetzen des Escape-Symbols:  Wir wissen nun, daß wir nach `dest' schreiben müssen,
		// und dabei um `aggregate_skip' Bytes nach vorne verschieben.  Die Anzahl der zu kopierenden Bytes ist
		// noch unklar:  wir kopieren entweder bis zum Stringende, oder bis zu nächsten Escape-Zeichen.

		char *end = strchr(escaped, '\\');
		if (!end) {
			// String am Ende?
			size_t string_length_up_to_dest = dest - result;
			size_t movelen = length - string_length_up_to_dest - aggregate_skip;
			memmove(dest, dest + aggregate_skip, movelen);
			dest[movelen] = 0;
		} else {
			size_t movelen = end - escaped;
			memmove(dest, dest + aggregate_skip, movelen);
			dest += movelen;
		}
		escaped = end;
	}
	// FIXME: Diese Implementierung ist für Strings entweder komplett ohne oder mit vielen Escape-Zeichen
	// optimiert.  Die Speicherkopieroperation ist ineffizient

	return STRING;
}

{IDENTIFIER} {
	yylval.str = strdup(yytext);
	return NAME;
}

[[:blank:]]+ // Eat whitespace

<<EOF>> {
        yy_delete_buffer( YY_CURRENT_BUFFER );
        yyterminate();
}
%%


/*
":=" {
	DLEX_PRINTTOKEN("Assignment operator");
	return ASSIGNMENT_TOKEN;
}

"<-" {
	DLEX_PRINTTOKEN("Strict assignment operator");
	return STRICT_ASSIGNMENT_TOKEN;
}


"==" {
	DLEX_PRINTTOKEN("Equality predicate");
	return EQUAL_TOKEN;
}


"<=" {
	DLEX_PRINTTOKEN("Less-Than predicate");
	return LTEQUAL_TOKEN;
}


">=" {
	DLEX_PRINTTOKEN("Greater-Than predicate");
	return GTEQUAL_TOKEN;
}


"->" {
	DLEX_PRINTTOKEN("Implication arrow");
}


"&&" {
	DLEX_PRINTTOKEN("Boolean AND");
	return AND_TOKEN;
}


"||" {
	DLEX_PRINTTOKEN("Boolean OR");
	return OR_TOKEN;
}


"^^" {
	DLEX_PRINTTOKEN("Boolean XOR");
}


">>" {
	DLEX_PRINTTOKEN("Shift-RIGHT");
}


"<<" {
	DLEX_PRINTTOKEN("Shift-LEFT");
}

"\\" {
	DLEX_PRINTTOKEN("Lambda");
	return LAMBDA_TOKEN;
}


"$" {
	DLEX_PRINTTOKEN("Call-by-name prefix operator");
}

"#".+$ {
	DLEX_PRINTTOKEN("Comment");
}





<<EOF>> {
        yy_delete_buffer( YY_CURRENT_BUFFER );
        yyterminate();
}


<*>.|\n {
	printf("Invalid input token: '%s'\n", yytext);
}
*/

static char
unescape_char(char c)
{
	switch (c) {
	case '\'': return '\'';
	case '\"': return '\"';
	case '\\': return '\\';
	case 'a': return '\a';
	case 'b': return '\b';
	case 'e': return '\e';
	case 'f': return '\f';
	case 'n': return '\n';
	case 'r': return '\r';
	case 't': return '\t';
	case 'v': return '\v';
	}
	return 0;
}


int
yywrap(void)
{
	return 1;
}
