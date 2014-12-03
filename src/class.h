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

//d Klassenrepraesentierung
//e class representation

#ifndef _ATTOL_CLASS_H
#define _ATTOL_CLASS_H

#include <stdio.h>

#include "symbol-table.h"
#include "bitvector.h"

#define CLASS_ENCODE_SELECTOR_SHIFT 0
#define CLASS_ENCODE_TYPE_SHIFT 16
#define CLASS_ENCODE_OFFSET_SHIFT 32
#define CLASS_DECODE_MASK 0xffff

#define CLASS_MEMBER_VAR_OBJ 1
#define CLASS_MEMBER_VAR_INT 2
#define CLASS_MEMBER_METHOD_ARGS_SHIFT 2
#define CLASS_MEMBER_METHOD_ARGS_OFFSET 4
#define CLASS_MEMBER_IS_FIELD_MASK 0x3
#define CLASS_MEMBER_IS_METHOD(V) ((V) >= CLASS_MEMBER_METHOD_ARGS_OFFSET)
#define CLASS_MEMBER_METHOD(ARGS_NR) (CLASS_MEMBER_METHOD_ARGS_OFFSET + ((ARGS_NR) << CLASS_MEMBER_METHOD_ARGS_SHIFT))
#define CLASS_MEMBER_METHOD_ARGS_NR(TY) (((TY) - CLASS_MEMBER_METHOD_ARGS_OFFSET) >> CLASS_MEMBER_METHOD_ARGS_SHIFT)

#define CLASS_ENCODE_SELECTOR(SELECTOR, OFFSET, TYPE) ((((unsigned long long )SELECTOR) << CLASS_ENCODE_SELECTOR_SHIFT)	\
						       | (((unsigned long long) OFFSET) << CLASS_ENCODE_OFFSET_SHIFT)	\
						       | (((unsigned long long) TYPE) << CLASS_ENCODE_TYPE_SHIFT))

#define CLASS_DECODE_SELECTOR_ID(ENCODED) (((ENCODED) >> CLASS_ENCODE_SELECTOR_SHIFT) & CLASS_DECODE_MASK)
#define CLASS_DECODE_SELECTOR_OFFSET(ENCODED) (((ENCODED) >> CLASS_ENCODE_OFFSET_SHIFT) & CLASS_DECODE_MASK)
#define CLASS_DECODE_SELECTOR_TYPE(ENCODED) (((ENCODED) >> CLASS_ENCODE_TYPE_SHIFT) & CLASS_DECODE_MASK)

//d Zugriff auf die virtuelle Funktionstabelle
//e access to virtual function table
#define CLASS_VTABLE(CLASSREF) ((void **) (((class_member_t *) &((CLASSREF)->members)) + ((CLASSREF)->table_mask + 1)))

typedef struct {
	unsigned long long selector_encoding; /*d Kodiert per CLASS_ENCODE_SELECTOR() *//*e encoded via CLASS_ENCODE_SELECTOR() */
	symtab_entry_t *symbol;  /*d Nur zum Debugging *//*e debugging only */
} class_member_t;

//d Klassenstruktur
//d ---------------
//d Wir identifizieren Felder/Methoden durch `Selektoren' (selector).  Ein Selektor ist
//d eindeutig durch seinen Namen (z.B. "size") bestimmt.  Der Uebersetzer verwendet Selektoren,
//d um anzuzeigen, welches Feld/welche Methode verwendet werden soll; Felder und Methoden
//d verwenden die gleichen Selektoren.
//d
//d Alle Methoden und Felder werden in der `open access'-Hashtabelle `members' abgelegt.
//d Initiale Position eines Eintrages ist (selector & table_mask).  Wenn diese Position schon
//d belegt ist, wird die naechste freie Position genommen.  Die Kodierung beinhaltet den Typ
//d und den tatsaechlichen Abstand, der entweder in die vtable zeigt (Methoden) oder in die
//d `fields'-Struktur von Objekten (Felder).
//d
//d Zugriff:
//d - object_{read,write}_member_field_{obj,int}	// (Felder)
//d - object_get_member_method			// (Methoden)
//d
//d Methodenadressen liegen unmittelbar hinter der Tabelle im Speicher (CLASS_VTABLE).
//d
//d Hinweis zur Kodierung:  Alle Methoden werden von der Typanalyse umgeschrieben, um Parameter
//d vom Typ compiler_options.method_call_param_type zu nehmen und Rueckgabewerte vom Typ
//d compiler_options.method_call_return_type zurueckzugeben (normalerweise TYPE_OBJ, der
//d intern einem object_t* entspricht).
//d
//d Felder unterscheiden weiterhin zwischen Typen.
//e class structure
//e ---------------
//e We identify fields/methods by selectors.  A selector is uniquely identified by its name (e.g., "size").
//e The compiler uses selectors to mark which field/method we should use; fields and methods draw from the
//e same pool of selectors.
//e
//e All methods/fields are stored in an `open access' hash table, represented by the field `members'.
//e Initial positions for the entries are (selector & table_mask).  If that position is already taken,
//e the next free location is selected instead.  Each `member' entry encodes the entry type (one of
//e `method with k parameters', `int-field', `obj-field') and offset.  For methods, the offset points into
//e the virtual function table, and for fields it points into the objects' `fields' structure.
//e
//e Access:
//e - object_{read,write}_member_field_{obj,int}	// (fields)
//e - object_get_member_method			// (methods)
//e
//e Method addresses reside in memory immediately after the `members' table (cf. CLASS_VTABLE).
//e
//e Note wrt encoding:  All members are re-written by type analysis to take parameters of type
//e compiler_options.method_call_param_type and to return values of type
//e compiler_options.method_call_return_type.  (Usually both are TYPE_OBJ, corresponding to object_t *)
typedef struct {
	//e WARNING: class_string and class_array have SPECIAL layouts:
	//e - class_array does not use object_map.
	//e   - fields[0] contains the number of array elements (int).
	//e   - fields[1] ... fields[fields[0]] contain the array elements (all objects).
	//e - class_string does not use object_map.
	//e   - fields[0] contains the string length (int).
	//e   - fields[1]ff etc. are filled with the character string.  The total object size is still block-aligned
	symtab_entry_t *id; /*d Symboltabelleneintrag (fuer den Uebersetzer/Debugging) *//*e symbol table entry */
	bitvector_t object_map; /*e bitvector marking the offsets of reference (object_t *) fields */
	unsigned long long table_mask; /*d Tabellengroesse - 1 *//* table size - 1 */

	class_member_t members[]; /* (table_mask + 1) Eintraege *//* (table_mask + 1) entries */
	//d Hinter den `members' liegt die virtuelle Funktionstabelle (vtable) (Adressen der tatsaechlichen Einsprungpunkte der Methoden)
	//e vtable (virtual function table) resides behind members
} class_t;

extern class_t class_boxed_int;	// Ein Eintrag: int_v
extern class_t class_boxed_real;// Ein Eintrag: real_v
extern class_t class_string;	// Zeichenkette beginnt ab member[0]
extern class_t class_array;	// len+1 Eintraege, mit member[0].int_v=len

/**
 * Erzeugt und bindet eine Klassenstruktur aus bzw. and einen Symboltabelleneintrag
 *
 * @param entry Der korrespondierende Symboltabelleneintrag
 * @return Die passende Klassenstruktur
 */
class_t*
class_new(symtab_entry_t *entry);

/*e
 * Prints the structure of a given class
 */
void
class_print(FILE *file, class_t *classref);

/**
 * Berechnet die Groesse der Selektortabelle fuer eine Klasse
 *
 * @param methods_nr Anzahl der Methoden der Klasse
 * @param fields_nr Anzahl der Felder der Klasse
 * @return Anzahl der zu verwendenden Eintraege in der Selektortabelle
 */
int
class_selector_table_size(int methods_nr, int fields_nr);

/**
 * Verbindet Klassenstruktur mit ihrem Symboltabelleneintrag und umgekehrt
 *
 * Nur für statisch allozierte Klassen nötig, wird implizit von class_new verwendet.
 */
class_t*
class_initialise_and_link(class_t *classref, symtab_entry_t *entry);

/**
 * Fuegt einen Selektor zu einer Klassenbeschreibung hinzu
 */
void
class_add_selector(class_t *classref, symtab_entry_t *selector_impl);


/*d
 * Initialisiert alle eingebauten Klassen und die Symboltabelle
 */
/*e
 * Initialises builtin classes and the symbol table
 */
void
builtins_init();

/*e
 * resets the symbol table; initialises built-in classes
 */
void
builtins_reset();

#endif // !defined(_ATTOL_CLASS_H)
