/* ** AUTOMATICALLY GENERATED.  DO NOT MODIFY. ** */
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

void yyerror(const char *s); // Fehler beim Lexen/Parsen

// Bilde `n' auf `\n' etc.  ab.
// Bei Fehler:  gibt '0' zurück.
static char unescape_char(char c);

static char* unescape_string(char *text);

static char* mk_unique_string(char *text);

int yy_line_nr = 1;

int yy_xflag = 0; // Erweiterungsflag: TAG_AST_*
%}

DIGIT [0-9]
HEXDIGIT [0-9a-fA-F]
IDENTIFIER [a-zA-Z_][a-zA-Z0-9_]*

%option always-interactive
%%

":=" {
	return T_L_COLONEQ;
}

"break" {
	return T_L_BREAK;
}

"if" {
	return T_L_IF;
}

";" {
	return ';';
}

"else" {
	return T_L_ELSE;
}

"-" {
	return '-';
}

"<=" {
	return T_L_LTEQ;
}

"const" {
	return T_L_CONST;
}

"int" {
	return T_L_INT;
}

"!=" {
	return T_L_BANGEQ;
}

"continue" {
	return T_L_CONTINUE;
}

"\]" {
	return ']';
}

"var" {
	return T_L_VAR;
}

"," {
	return ',';
}

"\(" {
	return '(';
}

"==" {
	return T_L_EQEQ;
}

"do" {
	return T_L_DO;
}

"\*" {
	return '*';
}

"=" {
	return '=';
}

"class" {
	return T_L_CLASS;
}

"is" {
	return T_L_IS;
}

">" {
	return '>';
}

"{" {
	return '{';
}

"obj" {
	return T_L_OBJ;
}

"while" {
	return T_L_WHILE;
}

"\." {
	return '.';
}

"real" {
	return T_L_REAL;
}

"/" {
	return '/';
}

"not" {
	return T_L_NOT;
}

"\[" {
	return '[';
}

"\)" {
	return ')';
}

"return" {
	return T_L_RETURN;
}

">=" {
	return T_L_GTEQ;
}

"}" {
	return '}';
}

"\+" {
	return '+';
}

"<" {
	return '<';
}

\"(\\.|[^\"\\])*\" {
	yylval.str = unescape_string(yytext);
	return T__STRING;
}

((({DIGIT}*"."{DIGIT}+)|({DIGIT}+"."))([eE][+-]?{DIGIT}+)?)|({DIGIT}+[eE][+-]?{DIGIT}+) {
	yylval.real = strtod(yytext, NULL);
	return T__REAL;
}

0x(({HEXDIGIT}*"."{HEXDIGIT}+)|({HEXDIGIT}+"."))([pP][+-]?{DIGIT}+) {
	yylval.real = strtod(yytext, NULL);
	return T__REAL;
}

0x{HEXDIGIT}+ {
	yylval.num = strtol(yytext + 2, NULL, 16);
	return T__INT;
}

{DIGIT}+ {
	yylval.num = strtol(yytext, NULL, 10);
	return T__INT;
}

{IDENTIFIER} {
	yylval.str = mk_unique_string(yytext);
	return T__NAME;
}



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


static char*
unescape_string(char *text)
{
	char * result = strdup(text + 1);
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
       return result;
}

int
yywrap(void)
{
	return 1;
}

static hashtable_t *name_map = NULL;

static char* mk_unique_string(char *id)
{
	if (!name_map) {
		name_map = hashtable_alloc(string_hash, (int (*)(const void *, const void *)) strcmp, 5);
	}
	char **retval_ptr = (char **) hashtable_access(name_map, id, NULL);

	if (retval_ptr) {
		// already know this one
		return *retval_ptr;
	}
	// otherwise:  add new entry
	char *unique_id = strdup(id);
	hashtable_access(name_map, id, unique_id);
	return unique_id;
}
