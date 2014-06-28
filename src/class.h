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

// Klassenrepraesentierung

#ifndef _ATTOL_CLASS_H
#define _ATTOL_CLASS_H

#include "symbol-table.h"

#define CLASS_ENCODE_SELECTOR_SHIFT 0
#define CLASS_ENCODE_OFFSET_SHIFT 16
#define CLASS_ENCODE_TYPE_SHIFT 32
#define CLASS_DECODE_MASK 0xffff

#define CLASS_TYPE_VAR_OBJ 1
#define CLASS_TYPE_VAR_INT 2
#define CLASS_TYPE_METHOD(ARGS_NR) (3 + (ARGS_NR))

#define CLASS_ENCODE_SELECTOR(SELECTOR, OFFSET, TYPE) ((((unsigned long long )SELECTOR) << CLASS_ENCODE_SELECTOR_SHIFT)	\
						       | (((unsigned long long) OFFSET) << CLASS_ENCODE_OFFSET_SHIFT)	\
						       | (((unsigned long long) TYPE) << CLASS_ENCODE_TYPE_SHIFT))

#define CLASS_DECODE_SELECTOR_ID(ENCODED) (((ENCODED) >> CLASS_ENCODE_SELECTOR_SHIFT) & CLASS_DECODE_MASK)
#define CLASS_DECODE_SELECTOR_OFFSET(ENCODED) (((ENCODED) >> CLASS_ENCODE_OFFSET_SHIFT) & CLASS_DECODE_MASK)
#define CLASS_DECODE_SELECTOR_TYPE(ENCODED) (((ENCODED) >> CLASS_ENCODE_TYPE_SHIFT) & CLASS_DECODE_MASK)

// Zugriff auf die virtuelle Funktionstabelle
#define CLASS_VTABLE(CLASSREF) ((void **) (&((CLASSREF)->members) + ((CLASSREF)->table_mask + 1));)

typedef struct {
	unsigned long long selector_encoding;
	symtab_entry_t *symbol;
} class_member_t;

// Klassenstruktur
typedef struct {
	symtab_entry_t *id;
	unsigned long long table_mask; // Tabellengroesse - 1
	// Repraesentierung ist relativ ineffizient, aber einfach
	class_member_t members[];
	// Hinter den `members' liegt die virtuelle Funktionstabelle (vtable) (Adressen der aktuellen Einsprungpunkte der Methoden)
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


/**
 * Extrahiert die 
 */
void **
class_vtable(class_t *classref);

/**
 * Initialisiert alle eingebauten Klassen und die Symboltabelle
 */
void
builtins_init();

#endif // !defined(_ATTOL_CLASS_H)
