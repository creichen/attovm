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
#include "assert.h"
#include "class.h"
#include "object.h"
#include "symbol-table.h"


class_t class_boxed_int = {
	.id = NULL,
	.table_mask = 0,
	.members = { { 0, NULL } }
};

class_t class_boxed_real = {
	.id = NULL,
	.table_mask = 0,
	.members = { { 0, NULL } }
};

class_t class_string = {
	.id = NULL,
	.table_mask = 1,
	.members = { { 0, NULL }, { 0, NULL } }
};

class_t class_array = {
	.id = NULL,
	.table_mask = 1,
	.members = { { 0, NULL }, { 0, NULL } }
};

/**
 * Initialisiert die Symboltabelle und installiert die eingebauten Operationen
 */
extern void
symtab_init();

static void
classes_init();

// Tabelle der eingebauten Operationen
// Einige dieser Operationen sind fest eingebaut (mit zur Parse-Zeit bekannten IDs),
// die anderen erhalten IDs bei ihrer Zuweisung.
//
// Feste IDs:
//  - Symboltabelleneintrag gegeben durch BUILTIN_OP_*
// Allozierte IDs:
//  - Symboltabelleneintrag gegeben durch BUILTIN_PRELINKED

#define BUILTIN_PRELINKED_THRESHOLD 0x100
#define BUILTIN_PRELINKED(x) ((x) + BUILTIN_PRELINKED_THRESHOLD)

#define BUILTIN_PRELINKED_CLASS_INT		BUILTIN_PRELINKED(0)
#define BUILTIN_PRELINKED_CLASS_REAL		BUILTIN_PRELINKED(1)
#define BUILTIN_PRELINKED_CLASS_STRING		BUILTIN_PRELINKED(2)
#define BUILTIN_PRELINKED_CLASS_ARRAY		BUILTIN_PRELINKED(3)
#define BUILTIN_PRELINKED_METHOD_STRING_SIZE	BUILTIN_PRELINKED(4)
#define BUILTIN_PRELINKED_METHOD_ARRAY_SIZE	BUILTIN_PRELINKED(5)

#define BUILTIN_PRELINKED_MAX			BUILTIN_PRELINKED_METHOD_ARRAY_SIZE

// Tabelle der Indizes für BUILTIN_PRELINKED (wird von symtab_add_builtins gefuellt)
static int
prelinked_index_table[BUILTIN_PRELINKED_MAX - BUILTIN_PRELINKED_THRESHOLD + 1];

/**
 * Bestimmt die symtab_id fuer die gegebene prelinked_id
 *
 * Kann erst nach dem korrespondierenden symtab_add_builtins() aufgerufen werden
 *
 * @param prelinked_id Aufzuloesende prelinked_id
 * @return Korrespondierende symtab_id
 */
#define RESOLVE_BUILTIN_PRELINKED_ID(i) prelinked_index_table[(i) - BUILTIN_PRELINKED_THRESHOLD]


struct builtin_ops {
	int index; // filled in with selector ID for selectors ONLY
	char *name;
	int symtab_flags;
	int ast_flags;
	int args_nr;
	unsigned short *args;
	void *function_pointer;
};

static unsigned short args_var_var[] = { AST_FLAG_VAR, AST_FLAG_VAR };
static unsigned short args_int_int[] = { AST_FLAG_INT, AST_FLAG_INT };
static unsigned short args_int[] = { AST_FLAG_INT };
static unsigned short args_obj[] = { AST_FLAG_OBJ };
static unsigned short args_null[] = { 0 };

void *builtin_op_print(object_t *arg);  // Nicht statisch: Wird vom Test-Code verwendet
static void *builtin_op_assert(long long int arg); 
static long long int builtin_op_string_size(object_t *arg); 
static long long int builtin_op_array_size(object_t *arg); 

static struct builtin_ops builtin_ops[] = {
	{ BUILTIN_OP_ADD, "+", (SYMTAB_TY_FUNCTION | SYMTAB_HIDDEN), AST_FLAG_INT, 2, args_int_int, NULL },
	{ BUILTIN_OP_MUL, "*", (SYMTAB_TY_FUNCTION | SYMTAB_HIDDEN), AST_FLAG_INT, 2, args_int_int, NULL },
	{ BUILTIN_OP_SUB, "-", (SYMTAB_TY_FUNCTION | SYMTAB_HIDDEN), AST_FLAG_INT, 2, args_int_int, NULL },
	{ BUILTIN_OP_DIV, "/", (SYMTAB_TY_FUNCTION | SYMTAB_HIDDEN), AST_FLAG_INT, 2, args_int_int, NULL },
	{ BUILTIN_OP_TEST_EQ, "==", (SYMTAB_TY_FUNCTION | SYMTAB_HIDDEN), AST_FLAG_VAR, 2, args_var_var, NULL },
	{ BUILTIN_OP_TEST_LE, "<=", (SYMTAB_TY_FUNCTION | SYMTAB_HIDDEN), AST_FLAG_INT, 2, args_int_int, NULL },
	{ BUILTIN_OP_TEST_LT, "<", (SYMTAB_TY_FUNCTION | SYMTAB_HIDDEN), AST_FLAG_INT, 2, args_int_int, NULL },
	{ BUILTIN_OP_CONVERT, "__convert", (SYMTAB_TY_FUNCTION | SYMTAB_HIDDEN), 0, 1, args_null, NULL },
	{ BUILTIN_OP_NOT, "not", (SYMTAB_TY_FUNCTION | SYMTAB_HIDDEN), AST_FLAG_INT, 1, args_int, NULL },
	// not with a fixed position
	{ 0, "print", SYMTAB_TY_FUNCTION, AST_FLAG_OBJ, 1, args_obj, &builtin_op_print },
	{ 0, "assert", SYMTAB_TY_FUNCTION, AST_FLAG_OBJ, 1, args_int, &builtin_op_assert }
};

static struct builtin_ops builtin_selectors[] = {
	{ 0, "size", SYMTAB_TY_FUNCTION | SYMTAB_SELECTOR, AST_FLAG_INT, 0, NULL, NULL }
};

static struct builtin_ops builtin_classes[] = {
	{ BUILTIN_PRELINKED_CLASS_INT, "Int", SYMTAB_TY_CLASS | SYMTAB_HIDDEN, 0, 0, NULL, NULL },
	{ BUILTIN_PRELINKED_CLASS_REAL, "Real", SYMTAB_TY_CLASS | SYMTAB_HIDDEN, 0, 0, NULL, NULL },
	{ BUILTIN_PRELINKED_CLASS_STRING, "String", SYMTAB_TY_CLASS | SYMTAB_HIDDEN, 0, 0, NULL, NULL },
	{ BUILTIN_PRELINKED_CLASS_ARRAY, "Array", SYMTAB_TY_CLASS | SYMTAB_HIDDEN, 0, 0, NULL, NULL },

	{ BUILTIN_PRELINKED_METHOD_STRING_SIZE, "size", SYMTAB_TY_FUNCTION | SYMTAB_MEMBER, AST_FLAG_INT, 0, NULL, &builtin_op_string_size },
	{ BUILTIN_PRELINKED_METHOD_ARRAY_SIZE, "size", SYMTAB_TY_FUNCTION | SYMTAB_MEMBER, AST_FLAG_INT, 0, NULL, &builtin_op_array_size }
};

// Die Namen für builtins, die hier verwendet werden, müssen auf die gleiche Speicherstelle zeigen
// wie die im AST verwendeten Namen.  Dazu wird diese Normalisierungsfunktion verwendet:
extern char* mk_unique_string(char *id);
extern int symtab_selectors_nr; // symbol-table.c

static void
symtab_add_builtins(struct builtin_ops *builtins, int nr)
{
	for (int i = 0; i < nr; i++) {
		int *id_p = NULL; // Hierher schreiben wir nachher die ID, falls noetig
		struct builtin_ops *b = builtins + i;
		if (b->index >= BUILTIN_PRELINKED_THRESHOLD) {
			id_p = &RESOLVE_BUILTIN_PRELINKED_ID(b->index);
			b->index = 0;
		}
		symtab_entry_t *e = symtab_builtin_new(b->index, b->ast_flags, b->symtab_flags,
						       mk_unique_string(b->name));
		b->index = e->id;
		if (id_p) {
			*id_p = e->id;
		}

		if (e->symtab_flags & SYMTAB_SELECTOR) {
			e->selector = b->index = symtab_selectors_nr++;
		}
		if (b->args != NULL) {
			e->parameters_nr = b->args_nr;
			e->parameter_types = b->args;
		}
	}
}


void
builtins_init()
{
	// Symbol table
	symtab_init();

	// Builtin operations
	symtab_add_builtins(builtin_ops, sizeof(builtin_ops) / sizeof(struct builtin_ops));
	symtab_add_builtins(builtin_selectors, sizeof(builtin_selectors) / sizeof(struct builtin_ops));
	symtab_add_builtins(builtin_classes, sizeof(builtin_classes) / sizeof(struct builtin_ops));

	// Classes
	classes_init();
}


static void
classes_init()
{
	class_initialise_and_link(&class_boxed_int, symtab_lookup(RESOLVE_BUILTIN_PRELINKED_ID(BUILTIN_PRELINKED_CLASS_INT)));
	class_initialise_and_link(&class_boxed_real, symtab_lookup(RESOLVE_BUILTIN_PRELINKED_ID(BUILTIN_PRELINKED_CLASS_REAL)));
	class_initialise_and_link(&class_string, symtab_lookup(RESOLVE_BUILTIN_PRELINKED_ID(BUILTIN_PRELINKED_CLASS_STRING)));
	class_initialise_and_link(&class_array, symtab_lookup(RESOLVE_BUILTIN_PRELINKED_ID(BUILTIN_PRELINKED_CLASS_ARRAY)));

	class_add_selector(&class_string, symtab_lookup(RESOLVE_BUILTIN_PRELINKED_ID(BUILTIN_PRELINKED_METHOD_STRING_SIZE)));
	class_add_selector(&class_array, symtab_lookup(RESOLVE_BUILTIN_PRELINKED_ID(BUILTIN_PRELINKED_METHOD_ARRAY_SIZE)));
}


// Alternativer Ausgabestrom (zum Testen)
FILE *
builtin_print_redirection = NULL;

void *
builtin_op_print(object_t *arg)
{
	FILE *output_stream = stdout;
	if (builtin_print_redirection) {
		output_stream = builtin_print_redirection;
	}

	if (arg->classref == &class_boxed_int) {
		fprintf(output_stream, "%lld\n", arg->members[0].int_v);
	} else if (arg->classref == &class_boxed_real) {
		fprintf(output_stream, "%f\n", arg->members[0].real_v);
	} else if (arg->classref == &class_string) {
		fprintf(output_stream, "%s\n", (char *)&arg->members[0]);
	} else {
		// fixme: pretty-printing support for arrays
		// fixme: pretty-printing support for arbitrary objects
		fprintf(output_stream, "%s\n", arg->classref->id->name);
	}
	return NULL;
}

static void *
builtin_op_assert(long long int arg)
{
	fprintf(stderr, "Not yet implemented: op assert");
	return NULL;
}

static long long int
builtin_op_string_size(object_t *arg)
{
	fprintf(stderr, "Not yet implemented: op string.size");
	return 0;
}

static long long int
builtin_op_array_size(object_t *arg)
{
	fprintf(stderr, "Not yet implemented: op array.size");
	return 0;
} 

