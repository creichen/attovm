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

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#include "ast.h"
#include "assembler.h"
#include "baseline-backend.h"
#include "class.h"
#include "analysis.h"
#include "object.h"
#include "parser.h"
#include "registers.h"
#include "runtime.h"
#include "symbol-table.h"
#include "compiler-options.h"

extern FILE *builtin_print_redirection; // builtins.c

static int failures = 0;
static int runs = 0;

#define DEBUG

void
fail(char *msg)
{
	fprintf(stderr, "Critical failure: %s\n", msg);
	exit(1);
}

static runtime_image_t *
compile(char *src, int line)
{
	FILE *memfile = fmemopen(src, strlen(src), "r");
	parser_restart(memfile);
	ast_node_t *root;
	if (!parse_program(&root)) {
		fprintf(stderr, "NOPARSE");
		fclose(memfile);
		return NULL;
	}
	fclose(memfile);
	runtime_image_t *img = runtime_prepare(root, RUNTIME_ACTION_COMPILE);
#ifdef DEBUG
	ast_node_dump(stderr, img->ast, AST_NODE_DUMP_FORMATTED | AST_NODE_DUMP_ADDRESS | AST_NODE_DUMP_FLAGS);
#endif
	return img;
}

void
signal_failure()
{
	 ++failures;
	 printf("\033[1;31mFAILURE\033[0m\n");
}

void
signal_success()
{
	 printf("\033[1;32mOK\033[0m\n");
}

void
test_program(char *source, char *expected_result, int line)
{
	++runs;
	builtins_init();
	printf("[L%d] Testing: \t", line);
	runtime_image_t *image = compile(source, line);
	if (!image) {
		signal_failure();
		return;
	}

	const int output_buf_len = 4096 + strlen(expected_result);
	char output_buf[output_buf_len + 1];
	memset(output_buf, 0, output_buf_len + 1);
	FILE *writefile = fmemopen(output_buf, output_buf_len, "w");

#ifdef DEBUG
	fflush(NULL);
	for (int i = 0; i < image->callables_nr; i++) {
		symtab_entry_t *sym = image->callables[i]->children[0]->sym;
		fprintf(stderr, "%s %s:\n",
			SYMTAB_TY(sym) == SYMTAB_TY_CLASS ? "CONSTRUCTOR" : "FUNCTION",
			sym->name);
		buffer_disassemble(buffer_from_entrypoint(sym->r_mem));
	}

	if (image->trampoline) {
		fprintf(stderr, "TRAMPOLINE:\n");
		buffer_disassemble(image->trampoline);
	}

	fprintf(stderr, "DYNCOMP:\n");
	buffer_disassemble(image->dyncomp);

	fprintf(stderr, "MAIN:\n");
	buffer_disassemble(image->code_buffer);
#endif

	builtin_print_redirection = writefile;
	runtime_execute(image); // Engage!
	builtin_print_redirection = NULL;

	fflush(NULL); // Outputs might not be written until we flush
	if (strcmp(expected_result, output_buf)) {
		signal_failure();
		fprintf(stderr, "[L%d] Result mismatch:\n----- Expected:\n%s\n----- Actual:\n%s\n-----\n", line, expected_result, output_buf);
		size_t expected_len = strlen(expected_result);
		size_t output_len = strlen(output_buf);
		if (expected_len != output_len) {
			fprintf(stderr, "Output size mismatch: Expected %zu, got %zu\n", expected_len, output_len);
		}
		size_t len = output_len < expected_len? output_len : expected_len;
		for (int i = 0; i < len; i++) {
			if (output_buf[i] != expected_result[i]) {
				fprintf(stderr, "First mismatch at offset %d: expected %02x/`%c', got %02x/`%c'\n",
					i,
					expected_result[i], expected_result[i], 
					output_buf[i], output_buf[i]);
				break;
			}
		}

		buffer_disassemble(image->code_buffer);
		fflush(NULL);
		fprintf(stderr, "[L%d] From program `%s'\n", line, source);
	} else {
		signal_success();
	}

	runtime_free(image);
	fclose(writefile);
}

#define TEST(program, expected) test_program(program, expected, __LINE__);

char* mk_unique_string(char *id); // lexer

int
find_last_set(int number);


int
main(int argc, char **argv)
{
#ifdef DEBUG
	compiler_options.debug_dynamic_compilation = true;
#endif

	TEST("print(1);", "1\n");
#if 0
	TEST("print(3+4);", "7\n");
	TEST("print(3+4+1);", "8\n");
	TEST("print(4-1);", "3\n");
	TEST("print(4*6);", "24\n");
	TEST("print(7/2);", "3\n");
	TEST("print(((2*3)+(1+1))*((3+2)-(2+1)));", "16\n");
	TEST("{print(1);print(2);}", "1\n2\n");

	TEST("{ int x = 17; print(x); }", "17\n");
	TEST("{ int x = 17; print(x); x := 3; print(x*x+1); }", "17\n10\n");
	TEST("{ obj x = 17; print(x); }", "17\n");
	TEST("{ obj x = 17; print(x); x := 3; print(x*x+1); }", "17\n10\n");
	TEST("{ var x = 17; print(x); }", "17\n");
	TEST("{ var x = 17; print(x); x := 3; print(x*x+1); }", "17\n10\n");

	TEST("{ if (1) print(\"1\"); print(2);}", "1\n2\n");
	TEST("{ if (0) {print(\"0\");} print(2);}", "2\n");
	TEST("{ if (0) {print(\"0\");} else {print(\"1\");} print(2);}", "1\n2\n");

	TEST("{ if (1) print(\"0\"); else print(\"1\"); print(2);}", "0\n2\n");
	TEST("{ if (not 0) print(\"0\"); else print(\"1\"); print(2);}", "0\n2\n");
	TEST("{ if (not 1) print(\"0\"); else print(\"1\"); print(2);}", "1\n2\n");

	TEST("{ if (1 > 2) print(\"0\"); else print(\"1\"); print(2);}", "1\n2\n");
	TEST("{ if (1 >= 2) print(\"0\"); else print(\"1\"); print(2);}", "1\n2\n");
	TEST("{ if (2 > 2) print(\"0\"); else print(\"1\"); print(2);}", "1\n2\n");
	TEST("{ if (2 >= 2) print(\"0\"); else print(\"1\"); print(2);}", "0\n2\n");
	TEST("{ if (3 > 2) print(\"0\"); else print(\"1\"); print(2);}", "0\n2\n");
	TEST("{ if (3 >= 2) print(\"0\"); else print(\"1\"); print(2);}", "0\n2\n");

	TEST("{ if (1 < 2) print(\"0\"); else print(\"1\"); print(2);}", "0\n2\n");
	TEST("{ if (1 <= 2) print(\"0\"); else print(\"1\"); print(2);}", "0\n2\n");
	TEST("{ if (2 < 2) print(\"0\"); else print(\"1\"); print(2);}", "1\n2\n");
	TEST("{ if (2 <= 2) print(\"0\"); else print(\"1\"); print(2);}", "0\n2\n");
	TEST("{ if (3 < 2) print(\"0\"); else print(\"1\"); print(2);}", "1\n2\n");
	TEST("{ if (3 <= 2) print(\"0\"); else print(\"1\"); print(2);}", "1\n2\n");
	TEST("{ if (3 != 2) print(\"0\"); else print(\"1\"); print(2);}", "0\n2\n");
	TEST("{ if (3 != 3) print(\"0\"); else print(\"1\"); print(2);}", "1\n2\n");

	TEST("{ if (0 == \"foo\") print(\"0\"); print(2);}", "2\n");
	TEST("{ if (\"bar\" == \"foo\") print(\"0\"); print(2);}", "2\n");
	TEST("{ if (\"bar\" == \"bar\") print(\"0\"); print(2);}", "0\n2\n");
	TEST("{ obj v = 0; if (0 == v) print(\"0\"); print(2);}", "0\n2\n");
	TEST("{ obj v = 0; if (v == 0) print(\"0\"); print(2);}", "0\n2\n");
	TEST("{ obj v = 0; if (1 == v) print(\"0\"); print(2);}", "2\n");
	TEST("{ obj v = 0; if (v == 1) print(\"0\"); print(2);}", "2\n");
	TEST("{ obj v = 0; obj v2 = 0; if (v2 == v) print(\"0\"); print(2);}", "0\n2\n");
	TEST("{ obj v = 0; obj v2 = 1; if (v2 == v) print(\"0\"); print(2);}", "2\n");

	TEST("{ int x = 0; while(x < 3) { print(x); x := x + 1; } } ", "0\n1\n2\n");
	//TEST("{ int x = 0; do { print(x); x := x + 1; } while(x < 3);} ", "0\n1\n2\n");
	TEST("{ int x = 0; while (x < 0) { print(x); x := x + 1; } } ", "");
	//TEST("{ int x = 0; do { print(x); x := x + 1; } while(x < 0);} ", "0\n");

	TEST("{ int x = 10; while(x > 4) { x := x - 1; if (x == 7) continue; print(x); } } ", "9\n8\n6\n5\n4\n");
	//TEST("{ int x = 10; do { x := x - 1; if (x == 7) continue; print(x); } while(x > 4);} ", "9\n8\n6\n5\n4\n");
	TEST("{ int x = 10; while(x > 4) { x := x - 1; if (x == 7) break; print(x); } } ", "9\n8\n");
	//TEST("{ int x = 10; do { x := x - 1; if (x == 7) break; print(x); } while(x > 4);} ", "9\n8");

	TEST("{ int x = 0; int y; while(x < 5) { y := x; x := x + 1; while (y < 4) { y := y + 1; if (y == 2) continue; print(y); } if (x == 3) break; } } ", "1\n3\n4\n3\n4\n3\n4\n");
	TEST("{ int x = 0; int y; while(x < 3) { y := x; x := x + 1; while (y < 5) { if (y == 3) break; print(y); y := y + 1; } if (x == 2) continue; print(\"+\");} } ", "0\n1\n2\n+\n1\n2\n2\n+\n");

	TEST("{ obj a = [1]; print(a[0]); }", "1\n");
	TEST("{ obj a = [1,7]; print(a[1]); print(a[0]);}", "7\n1\n");
	TEST("{ obj a = [1,[3,7]]; print(a[1][0]); print(a[0]);}", "3\n1\n");
	TEST("{ obj a = [1,7]; print(a[1]); a[1] := 2; print(a[1]); print(a[0]); }", "7\n2\n1\n");
	TEST("{ obj a = [1,\"foo\", /5]; print(a[1]); a[4] := 2; print(a[0]); print(a[4]); }", "foo\n1\n2\n");

	// next: NULL literal
	TEST("if (NULL == NULL) { print(\"null\"); }", "null\n");
	TEST("if (NULL == \"\") { print(\"null\"); }", "");
	TEST("if (NULL == 1) { print(\"null\"); }", "");

	// next: `is'
	TEST("if (1 is int) { print(\"1\"); }", "1\n");
	TEST("if (\"x\" is int) { print(\"1\"); }", "");
	TEST("if (\"x\" is string) { print(\"1\"); }", "1\n");
	TEST("if (\"x\" is int) { print(\"1\"); }", "");
	TEST("if (NULL is int) { print(\"1\"); }", "");
	TEST("if (NULL is string) { print(\"1\"); }", "");

	// skip
	TEST("print(1);;;;;print(2);", "1\n2\n");

	// functions
	TEST("int f(int x) { return x + 1; } print(f(1));", "2\n");
	TEST("obj f(obj x) { return x + 1; } print(f(1));", "2\n");
	TEST("obj f(obj x) { print(x); } f(1); ", "1\n");
	TEST("int f(int a, int b) { return a + (2*b); } print(f(1, 2));", "5\n");
	TEST("int f(int a, int b) { print(a); return a + (2*b); } print(f(1, 2));", "1\n5\n");
	TEST("int f(int a0, int a1, int a2, obj a3, obj a4, obj a5, obj a6, obj a7) { print(a0); print(a1); print(a2); print(a3); print(a4); print(a5); print(a6); print (a7);  } f(1, 2, 3, 4, 5, 6, 7, 8);", "1\n2\n3\n4\n5\n6\n7\n8\n");
	TEST("int f(int a0, int a1, int a2, obj a3, obj a4, obj a5, obj a6, obj a7) { print(a0); print(a1); print(a2); print(a3); print(a4); print(a5); print(a6); print (a7);  } f(1, 2, 3, 4, 5, 3+3, 3+4, 4+4);", "1\n2\n3\n4\n5\n6\n7\n8\n");
	TEST("int fact(int a) { if (a == 0) return 1; return a * fact(a - 1); } print(fact(5));", "120\n");
	TEST("int x = 0; int f(int a) { x := x + a; } print(x); f(3); print(x); f(2); print(x); ", "0\n3\n5\n");
#endif

	// Hashtabellen fuer Klassen sind hinreichend gross:
	for (int i = 0; i < 1000; i++) {
		assert(class_selector_table_size(i, 0) >= i*2);
		assert(class_selector_table_size(0, i) >= i*2);
		assert(class_selector_table_size(i, i) >= i*3);
	}

	const int two_element_class_hashtable_mask = class_selector_table_size(0, 2) - 1;
	const int three_element_class_hashtable_mask = class_selector_table_size(0, 3) - 1;
	// Erzeuge drei Namen, die in einer zwei/drei-Elemente-Klasse konfliktierende Tabelleneintraege haben
	symtab_entry_t *sym0 = NULL, *sym1 = NULL, *sym2 = NULL;
	char sym_buf[10] = "m_";
	sym0 = symtab_selector(mk_unique_string(sym_buf));
	for (int i = 'a'; !sym2; i++) {
		assert(i <= 'z'); // Tabelle sollte nicht zu gross sein

		symtab_entry_t **symp = &sym1;
		if (sym1) {
			symp = &sym2;
		}
		sym_buf[1] = i;

		symtab_entry_t *proposed_sym = symtab_selector(mk_unique_string(sym_buf));
		if (((proposed_sym->selector & two_element_class_hashtable_mask)
		     == (sym0->selector & two_element_class_hashtable_mask))
		    && ((proposed_sym->selector & two_element_class_hashtable_mask)
			== (sym0->selector & three_element_class_hashtable_mask))) {
			*symp = proposed_sym;
		}
	}
	// sym0, sym1, sym2 haben nun konfliktierende Symboltabelleneintraege

	// classes as structures
	TEST("class C(){}; obj a = C(); if (a != NULL) print(1);", "1\n");
	TEST("class C(){}; obj a = C(); obj b = C(); if (a != b) print(1);", "1\n");
	TEST("class C(){ int x; }; obj a = C(); a.x := 2; print(a.x);", "2\n");
	TEST("class C(){ obj x; }; obj a = C(); a.x := 2; print(a.x);", "2\n");
	TEST("class C(){ obj x; }; obj a = C(); a.x := 2; a.x := a.x + 1; print(a.x);", "3\n");
	TEST("class C(){ int x; }; obj a = C(); obj b = C(); a.x := 3; b.x := 2; print(a.x); print(b.x);", "3\n2\n");
	TEST("class C(){ obj x; int y; }; obj a = C(); a.x := 2; a.y := 3; print(a.x); print(a.y); ", "2\n3\n");

	// nontrivial constructor (field init)
	TEST("class C(){ int x = 17; }; obj a = C(); print(a.x); ", "17\n");
	TEST("class C(int a){ int x = a; }; obj a = C(1); obj b = C(2); print(a.x); print(b.x);", "1\n2\n");
	TEST("class C(int a){ int x = a; int y = a*3;}; obj a = C(1); print(a.x); print(a.y);", "1\n3\n");
	TEST("class C(int a){ int x = a; print(a); int y = a*3;}; obj a = C(1); print(a.x + 1); print(a.y);", "1\n2\n3\n");
	TEST("int z = 0; class C(int a) { z := z + 1; }; print(z); obj a = C(1);print(z); a := C(1); print(z);", "0\n1\n2\n");

	char conflict_str[1024];
	sprintf(conflict_str, "class C() { int %s; int %s; } obj a = C(); a.%s := 1; a.%s := 2; print(a.%s); print (a.%s); a.%s := 3; print(a.%s); print (a.%s);",
		sym0->name, sym1->name, sym0->name, sym1->name, sym0->name, sym1->name, sym0->name, sym0->name, sym1->name);
	TEST(conflict_str, "1\n2\n3\n2\n");

	sprintf(conflict_str, "class C() { int %s; int %s; int %s; } obj a = C(); a.%s := 1; a.%s := 2; a.%s := 3; print(a.%s); print (a.%s); print(a.%s); a.%s := 4; print(a.%s); print (a.%s); print (a.%s);",
		sym0->name, sym1->name, sym2->name,
		sym0->name, sym1->name, sym2->name,
		sym0->name, sym1->name, sym2->name,
		sym0->name,
		sym0->name, sym1->name, sym2->name);
	TEST(conflict_str, "1\n2\n3\n4\n2\n3\n");

	// next: method call (including nontrivial/conflicting method selector lookup)
	TEST("class C() { obj p() { print(\"foo\"); } } obj a = C(); a.p();", "foo\n");
	TEST("class C() { obj p(obj x) { print(x); } } obj a = C(); a.p(1);", "1\n");
	TEST("class C() { obj p(obj x) { print(x); } } obj a = C(); a.p(1);", "1\n");
	TEST("class C() { obj p(obj x) { print(x+2); } } obj a = C(); a.p(1);", "3\n");
	TEST("class C() { obj p(int x) { print(x+2); } } obj a = C(); a.p(1);", "3\n");

	TEST("class C() { obj p(obj x, obj y) { print(x+y); } } obj a = C(); a.p(1, 2);", "3\n");

	TEST("class C() { obj p(obj x, obj y) { print(x); return y+1; } } obj a = C(); print(a.p(1, 2));", "1\n3\n");
	TEST("class C() { obj p(obj a1, obj a2, obj a3, obj a4, obj a5, obj a6, obj a7) { print(a6 * a6 + a1); return a7 + 1; } } obj a = C(); print(a.p(1, 2, 3, 4, 5, 6, 7));", "37\n8\n");

	TEST("class C() { int z = 9; obj p(obj x) { z := z + 1; return x + z; } } obj a = C(); print(a.p(1)); print(a.p(1)); ", "11\n12\n");
	TEST("class C(int k) { int z = k; obj p(obj x) { z := z + 1; return x + z; } } obj a = C(5); print(a.p(1)); print(a.p(1)); ", "7\n8\n");
	TEST("class C(int k) { int z = k; obj inc() { z := z + 1; return z; } obj dec() {z := z - 1; return z; } } obj a = C(5); print(a.inc()); print(a.inc()); print(a.dec()); ", "6\n7\n6\n");

	sprintf(conflict_str, "class C() { int %s() {print(1);}; int %s() {print(2);}; int %s() {print(3);}; } obj a = C(); a.%s(); a.%s(); a.%s(); ",
		sym0->name, sym1->name, sym2->name,
		sym0->name, sym1->name, sym2->name);
	TEST(conflict_str, "1\n2\n3\n");

	// next: method call within class
	TEST("class C() { obj p() { print(1); } obj q() { p(); }} obj a = C(); a.q();", "1\n");
	TEST("class C() { obj p() { print(1); return 2; } obj q() { return 1 + p(); }} obj a = C(); print(a.q());", "1\n3\n");
	TEST("class C() { int p() { print(1); return 2; } obj q() { return 1 + p(); }} obj a = C(); print(a.q());", "1\n3\n");
	TEST("class C() { int p() { print(1); return 2; } int q() { return 1 + p(); }} obj a = C(); print(a.q());", "1\n3\n");
	TEST("class C() { obj p(obj x, int y) { print(x+y); } q() { p(1, 2); } } obj a = C(); a.q();", "3\n");

	if (!failures) {
		printf("All %d tests succeeded\n", runs);
	} else {
		printf("%d of %d tests failed\n", failures, runs);
	}
	return failures > 0;
}

