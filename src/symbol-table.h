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


#ifndef _ATTOL_SYMBOL_TABLE_H
#define _ATTOL_SYMBOL_TABLE_H

#include "ast.h"

//e The symbol table stores mappins from `identifiers' to semantic information (bindings) for those identifiers

/*d
 * SELECTOR-Einträge sind impräzise:  Aus Sicht außerhalb einer Klasse ist nur die `selector'-Nummer
 * notwendigerweise korrekt.  Ein Zugriff auf ein SELECTOR-Symbol darf sich daher nur am Feld `selector' orientieren;
 * alle andere Informationen sind zunächst bedeutungslos.
 */
/*e
 * SELECTOR entries are imprecise: viewed from outside a class only the `selector' number is correct.
 * Accessing a SELECTOR symbol must thus only access that field; all other information is initially meaningless.
 */

#define SYMTAB_KIND_MASK	0x0003
#define SYMTAB_KIND_VAR		0x0001
#define SYMTAB_KIND_FUNCTION	0x0002
#define SYMTAB_KIND_CLASS	0x0003

#define SYMTAB_SELECTOR		0x0004	/*d Selektor in ein Objekt */ /*e selector for selector table */
#define SYMTAB_PARAM		0x0008	/*d nur mit TY_VAR */ /*e only with TY_VAR */
#define SYMTAB_MEMBER		0x0010	/*d Klassenelement, also Methode oder Feld (zusammen mit TY_FUNCTION oder TY_VAR) */ /*e class element: method (with TY_FUNCTION) or field (with TY_VAR) */
#define SYMTAB_BUILTIN		0x0020  /*d Eingebaute Funktion */ /* built-in function */
#define SYMTAB_HIDDEN		0x0040	/*d Name nicht explizit angegeben */ /* name not specified explicitly */
#define SYMTAB_CONSTRUCTOR	0x0080	/*d Konstruktor */ /*e constructor */
#define SYMTAB_COMPILED		0x0100	/*d Fertig uebersetzt */ /*e fully translated to machine code */
#define SYMTAB_OPT		0x0200	/*d Mit Optimierungen übersetzt */ /*e fully translated with optimisations */
#define SYMTAB_EXCESS_PARAM	0x0400	/*d PARAM, den wir auf dem Stapel uebergeben */ /*e PARAM passed on the stack */
#define SYMTAB_MAIN_ENTRY_POINT	0x0800	/*e special symbol for main entry point */

#define SYMTAB_TYPE(s)			(((s)->ast_flags) & TYPE_FLAGS)	/*e TYPE_INT, TYPE_OBJ etc */
#define SYMTAB_KIND(s)			(((s)->symtab_flags) & SYMTAB_KIND_MASK)
#define SYMTAB_HAS_SELF(s)		(((s)->symtab_flags) & (SYMTAB_CONSTRUCTOR | SYMTAB_MEMBER))
#define SYMTAB_IS_STATIC(s)		(!(s)->parent && (SYMTAB_KIND(s) == SYMTAB_KIND_VAR))					/*d static-alloziert */ /*e in static memory */
#define SYMTAB_IS_STACK_DYNAMIC(s)	(((s)->parent && (!((s)->symtab_flags & SYMTAB_MEMBER))) || s->id == BUILTIN_OP_SELF)	/*d Stapel/Register-Alloziert */ /*e stack or register */
#define SYMTAB_IS_CONS_ARG(s)		(((s)->parent && ((s)->symtab_flags & SYMTAB_PARAM) && (SYMTAB_KIND((s)->parent) == SYMTAB_KIND_CLASS)))

typedef struct {
	short functions_nr;		/*e methods */
	short vars_nr;			/*e locals (function) or fields (class) */
	short temps_nr;			/*e temporary variables (for computing expressions) */
	short fields_nr;		/*e fields (classes only) */
} storage_record_t;

struct cfg_node;
struct class_struct;
struct object;

typedef struct symtab_entry {
	char *name;
	struct symtab_entry *parent;		/*d Struktureller Elterneintrag */ /*e structural parent entry */
	signed short id;			/*d Symboltabellennummer dieses Eintrags */ /*e symbol table entry number */
	unsigned short occurrence_count;	/*d Wievielte Elemente mit diesem Namen wurden überschattet? (Nur zur eindeutigen Identifizierung) */ /*e How many elements of this name are shadowed? (used for unambiguous pretty-printing) */
	unsigned short ast_flags;
	unsigned short symtab_flags;
	ast_node_t *astref;			/* CLASSDEF, FUNDEF, VARDECL */
	struct cfg_node *cfg_exit;		/*e control flow graph exit node (for SYMTAB_KIND_FUNCTION) */
	void *r_trampoline;			/*d Zeiger auf Trampolin-Code, falls vorhanden */ /*e pointer to trampoline code, if present */
	void *r_mem;				/*d Zeiger auf Funktion / Klassenobjekt */ /*e pointer to function or class object */
	unsigned short *parameter_types;	/*e for constructors, parameter_types and parameters_nr are 0.  Refer to the class to access them. */
	struct class_struct **dynamic_parameter_types;	/*e dynamically detected parameter types, using class_top, class_bottom as lattice, and NULL to indicate non-object parameters */
	long fast_hotness_counter;		/*e outer hotness counter (decreased by generated `cold' code, triggers sampling) */
	unsigned short slow_hotness_counter;	/*e inner hotness counter (decreased by ) */
	unsigned short parameters_nr;
	unsigned short selector;		/*d Globale ID für Felder und Methoden */ /* Global ID for fields and methods */
	signed short offset;			/*d MEMBER | VAR: Offset in Speicher der Struktur
						 * MEMBER | FUNCTION: Eindeutige Funktionsnummer
						 * PARAM: Parameternummer
						 * VAR: Variablennummer, entweder auf dem Stapel oder im statischen Speicher
						 */
						/*e (MEMBER | VAR): offset into data structure memory
						 * (MEMBER | FUNCTION): unique function number
						 * PARAM: parameter number
						 * VAR: variable number, either on stack or in static memory
						 */
	signed short stackframe_start;		/*e functions/methods: stack frame start, relative to $fp (filled in by code generator) */
	storage_record_t storage;
} symtab_entry_t;

//e Symbol IDs:
//e Symbol table entry 0 is always illegal, so regular entries start at 1 and builtin entries start at -1.

extern int symtab_entries_builtin_nr; //e builtin symbol table entries (positive nuber, negate this to get the ID)
extern int symtab_entries_nr; //e non-builtin symbol table entries

extern int symtab_selector_size; /*e selector number for the `size' selector */

//e various built-in classes and operators for easy lookup
extern int symtab_builtin_class_array;
extern int symtab_builtin_class_string;
extern int symtab_builtin_method_array_size;
extern int symtab_builtin_method_string_size;


/*d
 * Schlägt einen Eintrag in der Symboltabelle nach.
 */
/*e
 * Looks up an entry in the symbol table
 */
symtab_entry_t *
symtab_lookup(int id);

/*d
 * Liefert ein (eindeutiges) Selektor-Symbol fuer den gegebenen Namen
 */
/*e
 * Returns a unique selector symbol for the specified name
 */
symtab_entry_t *
symtab_selector(char *name);

/*d
 * Alloziert einen neuen Eintrag in die benutzerdefinierten Symboltabelle.
 */
/*e
 * Allocates a new entry in the table of user-defined symbols
 */
symtab_entry_t *
symtab_new(int ast_flags, int symtab_flags, char *name, ast_node_t *declaration);


/*d
 * Alloziert einen neuen Eintrag in der Symboltabelle für eingebaute (built-in) Operationen.
 *
 * Sollte mit symtab_id = 0 erst aufgerufen werden, nachdem alle Einträge mit festen Nummern
 * vergeben wurden.
 * 
 * @param symtab_id Beantragte Nummer für den Eintrag, oder `0' für `beliebig'
 */
/*e
 * Allocates a new entry in the symbol table for built-in operations
 *
 * Must be called with symtab_id = 0 (`any') only _after_ all entries with predefined numbers
 * have been set.
 * 
 * @param symtab_id Requested number for entry, or `0' to allow the system to pick one.
 */
symtab_entry_t *
symtab_builtin_new(int symtab_id, int ast_flags, int symtab_flags, char *name);


/*d
 * Druckt den kanonischen (eindeutigen) Namen des Eintrages aus, für Debug-Zwecke
 */
/*e
 * Prints a canonical (unique) name of the entry, for debugging
 */
void
symtab_entry_name_dump(FILE *file, symtab_entry_t *entry);


/*d
 * Ausgabe des Eintrags, für Debugzwecke
 */
/*e
 * Prints out the symbol table entry, for debugging
 */
void
symtab_entry_dump(FILE *file, symtab_entry_t *entry);


#endif // !defined(_ATTOL_SYMBOL_TABLE_H)
