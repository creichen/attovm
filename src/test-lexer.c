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

#include <stdio.h>
#include <stdlib.h>

#include <check.h>	// http://check.sourceforge.net/

#include "ast.h"
#include "parser.h"


FILE *fmemopen(void *buf, size_t size, const char *mode);
int yylex();


extern FILE * yyin;  // Eingabedatei des Lexers

extern int yy_xflag; // Flags mit erweiterten Informationen über das Token

#define MAX_YYIN_BUFFER 4096
char yyin_buffer[MAX_YYIN_BUFFER];

void
setup_scan(const char *buffer)
{
	int buf_len = strlen(buffer);
	ck_assert_msg(!yyin, "string to scan was set up already");
	ck_assert_int_lt(buf_len, MAX_YYIN_BUFFER);
	strcpy(yyin_buffer, buffer);
	yyin = fmemopen(yyin_buffer, buf_len, "r");
}

#define TOKEN(t) ck_assert_int_eq(t, yylex())
#define XTOKEN(t, f) ck_assert_int_eq(t, yylex()); ck_assert_int_eq(f, yy_xflag)

START_TEST(empty)
{
	setup_scan(" ");
	mark_point();
	TOKEN(0);
}
END_TEST

START_TEST(zero)
{
	setup_scan("0");
	TOKEN(INT);
	ck_assert_int_eq(0, yylval.sll);
	TOKEN(0);
}
END_TEST


START_TEST(one)
{
	setup_scan("1");
	TOKEN(INT);
	ck_assert_int_eq(1, yylval.sll);
	TOKEN(0);
}
END_TEST


START_TEST(truefalse)
{
	setup_scan("true false");
	TOKEN(BOOL);
	ck_assert_int_eq(1, yylval.c);
	TOKEN(BOOL);
	ck_assert_int_eq(0, yylval.c);
	TOKEN(0);
}
END_TEST

START_TEST(uint)
{
	setup_scan("0x42 127u 1024U");
	XTOKEN(UINT, TAG_AST_HEX_REPR);
	ck_assert_int_eq(0x42ull, yylval.ull);
	XTOKEN(UINT, 0);
	ck_assert_int_eq(127ull, yylval.ull);
	XTOKEN(UINT, 0);
	ck_assert_int_eq(1024ull, yylval.ull);
	TOKEN(0);
}
END_TEST

START_TEST(character)
{
	setup_scan("'a' '\\n' '\\0x42'");
	XTOKEN(CHAR, 0);
	ck_assert_int_eq('a', yylval.c);
	XTOKEN(CHAR, 0);
	ck_assert_int_eq('\n', yylval.c);
	XTOKEN(CHAR, TAG_AST_HEX_REPR);
	ck_assert_int_eq('B', yylval.c);
	TOKEN(0);
}
END_TEST

START_TEST(string)
{
	setup_scan("\"\" \"foo\"  \"foo\\nbar\"  \"ab\\tcd\\tef\\tgh\"   \"ab\\\"\" \"X\\0x20Y\"  \"\\a\" \"abc\\0x41\\0x42\\0x43\"  \"abc\\0x41 \\0x42\\n\" \"\\\\\"");
	XTOKEN(STRING, 0);
	ck_assert_str_eq("", yylval.str);
	XTOKEN(STRING, 0);
	ck_assert_str_eq("foo", yylval.str);
	XTOKEN(STRING, 0);
	ck_assert_str_eq("foo\nbar", yylval.str);
	XTOKEN(STRING, 0);
	ck_assert_str_eq("ab\tcd\tef\tgh", yylval.str);
	XTOKEN(STRING, 0);
	ck_assert_str_eq("ab\"", yylval.str);
	XTOKEN(STRING, 0);
	ck_assert_str_eq("X Y", yylval.str);
	XTOKEN(STRING, 0);
	ck_assert_str_eq("\a", yylval.str);
	XTOKEN(STRING, 0);
	ck_assert_str_eq("abcABC", yylval.str);
	XTOKEN(STRING, 0);
	ck_assert_str_eq("abcA B\n", yylval.str);
	XTOKEN(STRING, 0);
	ck_assert_str_eq("\\", yylval.str);
	TOKEN(0);
}
END_TEST

static int
double_eq(double expected, double d)
{
	if (d != expected) {
		fprintf(stderr, "expected: %f, actual: %f\n", expected, d);
		return 0;
	}
	return 1;
}

START_TEST(real)
{
	setup_scan("0.5 .2 10e2 10e+3 25.e-2 75.0E-2 .1E+1    0xa.0p1 0xa.0p+2 0xa.0p-3");
	XTOKEN(REAL, 0);
	ck_assert_int_eq(1, double_eq(0.5, yylval.real));
	XTOKEN(REAL, 0);
	ck_assert_int_eq(1, double_eq(0.2, yylval.real));
	XTOKEN(REAL, 0);
	ck_assert_int_eq(1, double_eq(10e2, yylval.real));
	XTOKEN(REAL, 0);
	ck_assert_int_eq(1, double_eq(10e+3, yylval.real));
	XTOKEN(REAL, 0);
	ck_assert_int_eq(1, double_eq(25.e-2, yylval.real));
	XTOKEN(REAL, 0);
	ck_assert_int_eq(1, double_eq(75.0E-2, yylval.real));
	XTOKEN(REAL, 0);
	ck_assert_int_eq(1, double_eq(.1E+1, yylval.real));

	XTOKEN(REAL, TAG_AST_HEX_REPR);
	ck_assert_int_eq(1, double_eq(0xa.0p1, yylval.real));
	XTOKEN(REAL, TAG_AST_HEX_REPR);
	ck_assert_int_eq(1, double_eq(0xa.0p+2, yylval.real));
	XTOKEN(REAL, TAG_AST_HEX_REPR);
	ck_assert_int_eq(1, double_eq(0xa.0p-3, yylval.real));
	TOKEN(0);
}
END_TEST

START_TEST(identifier)
{
	setup_scan(" frob_nitz  foo _  ___ A  ");
	TOKEN(NAME);
	ck_assert_str_eq("frob_nitz", yylval.str);
	TOKEN(NAME);
	ck_assert_str_eq("foo", yylval.str);
	TOKEN(NAME);
	ck_assert_str_eq("_", yylval.str);
	TOKEN(NAME);
	ck_assert_str_eq("___", yylval.str);
	TOKEN(NAME);
	ck_assert_str_eq("A", yylval.str);
	TOKEN(0);
}
END_TEST



// Wird vor jedem Test ausgefuehrt
void
setup()
{
}

// Wird nach jedem Test ausgefuehrt
void
teardown()
{
	if (yyin) {
		fclose(yyin);
		yyin = NULL;
	}
}

Suite *
lexer_suite()
{
	Suite *s = suite_create("Lexer");

	TCase *tc_core = tcase_create("core");
	// Neue Tests werden hier eingetragen
	tcase_add_checked_fixture(tc_core, setup, teardown);
	tcase_add_test(tc_core, empty);
	tcase_add_test(tc_core, zero);
	tcase_add_test(tc_core, one);
	tcase_add_test(tc_core, truefalse);
	tcase_add_test(tc_core, uint);
	tcase_add_test(tc_core, character);
	tcase_add_test(tc_core, real);
	tcase_add_test(tc_core, string);
	tcase_add_test(tc_core, identifier);
	suite_add_tcase(s, tc_core);

	return s;
}

int
main (void)
{
	int number_failed;
	Suite *s = lexer_suite();
	SRunner *sr = srunner_create(s);
	yyin = NULL;
	srunner_run_all(sr, CK_NORMAL);
	number_failed = srunner_ntests_failed(sr);
	srunner_free(sr);
	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
