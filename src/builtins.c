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

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "address-store.h"
#include "assert.h"
#include "class.h"
#include "errors.h"
#include "lexer-support.h"
#include "object.h"
#include "symbol-table.h"

class_t class_boxed_int = {
	.id = NULL,
	.object_map = BITVECTOR_MAKE_SMALL(1, 0),
	.table_mask = 0,
	.members = { { 0, NULL } }
};

class_t class_boxed_real = {
	.id = NULL,
	.object_map = BITVECTOR_MAKE_SMALL(1, 0),
	.table_mask = 0,
	.members = { { 0, NULL } }
};

class_t class_string = {
	.id = NULL,
	.object_map = BITVECTOR_MAKE_SMALL(0, 0),
	.table_mask = 1,
	.members = { { 0, NULL }, { 0, NULL },
		     { 0, NULL }} // Zusaetzlicher Platz fuer virtuelle Funktionstabelle
};

class_t class_array = {
	.id = NULL,
	.object_map = BITVECTOR_MAKE_SMALL(0, 0),
	.table_mask = 1,
	.members = { { 0, NULL }, { 0, NULL },
		     { 0, NULL }} // Zusaetzlicher Platz fuer virtuelle Funktionstabelle
};

/*d
 * Initialisiert die Symboltabelle und installiert die eingebauten Operationen
 */
extern void
symtab_init(void);

extern void
symtab_free(void);

static void
classes_init();

//d Tabelle der eingebauten Operationen
//d Einige dieser Operationen sind fest eingebaut (mit zur Parse-Zeit bekannten IDs),
//d die anderen erhalten IDs bei ihrer Zuweisung.
//d
//d Feste IDs:
//d  - Symboltabelleneintrag gegeben durch BUILTIN_OP_*
//d Allozierte IDs:
//d  - Symboltabelleneintrag gegeben durch BUILTIN_PRELINKED
//e Built-in operator table
//e Several of these operators are built in (with IDs known at parse time),
//e others have their IDs allocated when assigned.
//e
//e Fixed IDs:
//e  - Symbol table entry defined by BUILTIN_OP_*
//e Allocated IDs:
//e  - BUILTIN_PRELINKED

#define BUILTIN_PRELINKED_THRESHOLD 0x100
#define BUILTIN_PRELINKED(x) ((x) + BUILTIN_PRELINKED_THRESHOLD)

#define BUILTIN_PRELINKED_CLASS_INT		BUILTIN_PRELINKED(0)
#define BUILTIN_PRELINKED_CLASS_REAL		BUILTIN_PRELINKED(1)
#define BUILTIN_PRELINKED_CLASS_STRING		BUILTIN_PRELINKED(2)
#define BUILTIN_PRELINKED_CLASS_ARRAY		BUILTIN_PRELINKED(3)
#define BUILTIN_PRELINKED_METHOD_STRING_SIZE	BUILTIN_PRELINKED(4)
#define BUILTIN_PRELINKED_METHOD_ARRAY_SIZE	BUILTIN_PRELINKED(5)

#define BUILTIN_PRELINKED_MAX			BUILTIN_PRELINKED_METHOD_ARRAY_SIZE

//d Tabelle der Indizes für BUILTIN_PRELINKED (wird von symtab_add_builtins gefuellt)
//e Index table for BUILTIN_PRELINKED (filled by symtab_add_builtins)
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
	int index; /*e filled in with selector ID for selectors ONLY */ /*d Entweder 0 oder mit festem Index für eingebaute Operatoren */
	char *name;		/*d Name der Funktion/Operation */
	int symtab_flags;	/*d Attribut-Flags für die Symboltabelle, z.B. SYMTAB_KIND_FUNCTION für Funktionsdefinitionen */
	int ast_flags;		/*d Attribut-Flags für die Typanalyse, z.B. TYPE_OBJ für Operationen, die ein Objekt zurückliefern */
	int args_nr;		/*d Anzahl der formalen Parameter */
	unsigned short *args;	/*d Typen der formalen Parameter (Zeiger auf ein Array mit `args_nr' Einträgen) */
	void *function_pointer;	/*d Zeiger auf die Funktion, die aufgerufen werden soll */
};

static unsigned short args_int_int[] = { TYPE_INT, TYPE_INT };
static unsigned short args_int[] = { TYPE_INT };
static unsigned short args_obj[] = { TYPE_OBJ }; /*d nimmt ein Objekt als Parameter */
static unsigned short args_any[] = { TYPE_ANY };
static unsigned short args_any_any[] = { TYPE_ANY, TYPE_ANY };

void *builtin_op_print(object_t *arg);  /*d Nicht statisch: Wird vom Test-Code verwendet */
static object_t *builtin_op_magic(object_t *arg);
static object_t *builtin_op_assert(long long int arg);
static object_t *builtin_op_string_size(object_t *arg); 
static object_t *builtin_op_array_size(object_t *arg); 

static struct builtin_ops builtin_ops[] = {
	{ BUILTIN_OP_ADD, "+", (SYMTAB_KIND_FUNCTION | SYMTAB_HIDDEN), TYPE_INT, 2, args_int_int, NULL },
	{ BUILTIN_OP_MUL, "*", (SYMTAB_KIND_FUNCTION | SYMTAB_HIDDEN), TYPE_INT, 2, args_int_int, NULL },
	{ BUILTIN_OP_SUB, "-", (SYMTAB_KIND_FUNCTION | SYMTAB_HIDDEN), TYPE_INT, 2, args_int_int, NULL },
	{ BUILTIN_OP_DIV, "/", (SYMTAB_KIND_FUNCTION | SYMTAB_HIDDEN), TYPE_INT, 2, args_int_int, NULL },
	{ BUILTIN_OP_TEST_EQ, "==", (SYMTAB_KIND_FUNCTION | SYMTAB_HIDDEN), TYPE_INT, 2, args_any_any, NULL }, /*e but see object.c */
	{ BUILTIN_OP_TEST_LE, "<=", (SYMTAB_KIND_FUNCTION | SYMTAB_HIDDEN), TYPE_INT, 2, args_int_int, NULL },
	{ BUILTIN_OP_TEST_LT, "<", (SYMTAB_KIND_FUNCTION | SYMTAB_HIDDEN), TYPE_INT, 2, args_int_int, NULL },
	{ BUILTIN_OP_CONVERT, "*convert", (SYMTAB_KIND_FUNCTION | SYMTAB_HIDDEN), 0, 1, args_any, NULL },
	{ BUILTIN_OP_NOT, "not", (SYMTAB_KIND_FUNCTION | SYMTAB_HIDDEN), TYPE_INT, 1, args_int, NULL },
	{ BUILTIN_OP_ALLOCATE, "*allocate", (SYMTAB_KIND_FUNCTION | SYMTAB_HIDDEN), TYPE_OBJ, 1, args_int, NULL },
	{ BUILTIN_OP_SELF, "*self", (SYMTAB_KIND_VAR | SYMTAB_HIDDEN | SYMTAB_PARAM), TYPE_OBJ, 0, NULL, NULL },
	//e not with a fixed position
	//d keine feste Position; diese Funktionen können an beliebigen Stellen stehen und sind erweiterbar.
	//d Funktion `magic' ist ein 
	{ 0, "print", SYMTAB_KIND_FUNCTION, TYPE_OBJ, 1, args_obj, &builtin_op_print },
	{ 0, "assert", SYMTAB_KIND_FUNCTION, TYPE_OBJ, 1, args_int, &builtin_op_assert },
	{ .index=0, .name="magic", .symtab_flags=SYMTAB_KIND_FUNCTION, .ast_flags=TYPE_OBJ, .args_nr=1, .args=args_obj, .function_pointer=&builtin_op_magic }
};

static struct builtin_ops builtin_selectors[] = {
	{ 0, "size", SYMTAB_KIND_FUNCTION | SYMTAB_SELECTOR, TYPE_INT, 0, NULL, NULL }
};

static struct builtin_ops builtin_classes[] = {
	{ BUILTIN_PRELINKED_CLASS_INT, "int", SYMTAB_KIND_CLASS, 0, 0, NULL, NULL },
	{ BUILTIN_PRELINKED_CLASS_REAL, "real", SYMTAB_KIND_CLASS, 0, 0, NULL, NULL },
	{ BUILTIN_PRELINKED_CLASS_STRING, "string", SYMTAB_KIND_CLASS, 0, 0, NULL, NULL },
	{ BUILTIN_PRELINKED_CLASS_ARRAY, "Array", SYMTAB_KIND_CLASS, 0, 0, NULL, NULL },

	{ BUILTIN_PRELINKED_METHOD_STRING_SIZE, "size", SYMTAB_KIND_FUNCTION | SYMTAB_MEMBER, TYPE_INT, 0, NULL, &builtin_op_string_size },
	{ BUILTIN_PRELINKED_METHOD_ARRAY_SIZE, "size", SYMTAB_KIND_FUNCTION | SYMTAB_MEMBER, TYPE_INT, 0, NULL, &builtin_op_array_size }
};

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
		//d Die Namen für builtins, die hier verwendet werden, müssen auf die gleiche Speicherstelle zeigen
		//d wie die im AST verwendeten Namen.  Dazu wird diese Normalisierungsfunktion mk_unique_string() verwendet:
		symtab_entry_t *e = symtab_builtin_new(b->index, b->ast_flags, b->symtab_flags,
						       mk_unique_string(b->name));
		b->index = e->id;
		if (id_p) {
			*id_p = e->id;
		}
		e->storage.fields_nr = 1; /*e hack: currently, all our builtins are either special or have one field. */

		if (e->symtab_flags & SYMTAB_SELECTOR) {
			e->selector = b->index = symtab_selectors_nr++;
		}
		if (b->args != NULL) {
			e->parameters_nr = b->args_nr;
			e->parameter_types = b->args;
		}
		e->r_mem = b->function_pointer;
		if (e->r_mem) {
			addrstore_put(e->r_mem, ADDRSTORE_KIND_BUILTIN, e->name);
		}
	}
}


void
builtins_reset()
{
	// Symbol table
	symtab_free();
	symtab_init();

	
	static bool builtins_initialised = false;
	if (builtins_initialised) {
		return;
	}

	// Builtin operations
	symtab_add_builtins(builtin_ops, sizeof(builtin_ops) / sizeof(struct builtin_ops));
	symtab_add_builtins(builtin_selectors, sizeof(builtin_selectors) / sizeof(struct builtin_ops));
	symtab_add_builtins(builtin_classes, sizeof(builtin_classes) / sizeof(struct builtin_ops));

	builtins_initialised = true;
}


void
builtins_init()
{
	builtins_reset();
	// Classes
	classes_init();
}

int symtab_selector_size; /*e global: used by program analyses */

int symtab_builtin_class_array;
int symtab_builtin_class_string;
int symtab_builtin_method_array_size;
int symtab_builtin_method_string_size;

static void
classes_init()
{
	class_initialise_and_link(&class_boxed_int, symtab_lookup(RESOLVE_BUILTIN_PRELINKED_ID(BUILTIN_PRELINKED_CLASS_INT)));
	class_initialise_and_link(&class_boxed_real, symtab_lookup(RESOLVE_BUILTIN_PRELINKED_ID(BUILTIN_PRELINKED_CLASS_REAL)));
	class_initialise_and_link(&class_string, symtab_lookup(RESOLVE_BUILTIN_PRELINKED_ID(BUILTIN_PRELINKED_CLASS_STRING)));
	class_initialise_and_link(&class_array, symtab_lookup(RESOLVE_BUILTIN_PRELINKED_ID(BUILTIN_PRELINKED_CLASS_ARRAY)));

	symtab_selector_size = builtin_selectors[0].index;
	symtab_lookup(RESOLVE_BUILTIN_PRELINKED_ID(BUILTIN_PRELINKED_METHOD_STRING_SIZE))->selector = symtab_selector_size;
	symtab_lookup(RESOLVE_BUILTIN_PRELINKED_ID(BUILTIN_PRELINKED_METHOD_ARRAY_SIZE))->selector = symtab_selector_size;

	class_add_selector(&class_string, symtab_lookup(RESOLVE_BUILTIN_PRELINKED_ID(BUILTIN_PRELINKED_METHOD_STRING_SIZE)));
	CLASS_VTABLE(&class_string)[0] = builtin_op_string_size;
	class_add_selector(&class_array, symtab_lookup(RESOLVE_BUILTIN_PRELINKED_ID(BUILTIN_PRELINKED_METHOD_ARRAY_SIZE)));
	CLASS_VTABLE(&class_array)[0] = builtin_op_array_size;

	symtab_builtin_class_array = RESOLVE_BUILTIN_PRELINKED_ID(BUILTIN_PRELINKED_CLASS_ARRAY);
	symtab_builtin_class_string = RESOLVE_BUILTIN_PRELINKED_ID(BUILTIN_PRELINKED_CLASS_STRING);
	symtab_builtin_method_string_size = RESOLVE_BUILTIN_PRELINKED_ID(BUILTIN_PRELINKED_METHOD_STRING_SIZE);
	symtab_builtin_method_array_size = RESOLVE_BUILTIN_PRELINKED_ID(BUILTIN_PRELINKED_METHOD_ARRAY_SIZE);
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
	
#if 0
	object_print(stderr, arg, 3, true);
	fprintf(stderr, "(<- builtin-print)\n");
#endif

	object_print(output_stream, arg, 3, false);
	fprintf(output_stream, "\n");
	return NULL;
}

static object_t *
builtin_op_assert(long long int arg)
{
	if (!(arg)) {
		fail("Assertion failed");
	}
	return NULL;
}

static object_t *
builtin_op_string_size(object_t *arg)
{
	return new_int(arg->fields[0].int_v);
}

static object_t *
builtin_op_array_size(object_t *arg)
{
	return new_int(arg->fields[0].int_v);
} 

static object_t *
builtin_op_magic(object_t *arg)
{
	// ?
	return NULL;
}
