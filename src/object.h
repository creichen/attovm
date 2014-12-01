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

//d Objektrepräsentierung
//e object representation

#ifndef _ATTOL_OBJECT_H
#define _ATTOL_OBJECT_H

#include <stdbool.h>
#include <stdio.h>

#include "symbol-table.h"
#include "class.h"

typedef	union {
	long long int int_v;
	double real_v;
	struct object *object_v;
	char string;
} object_member_t;

typedef struct object {
	class_t *classref; // Zeigt auf Klassenobjekt
	object_member_t members[];
} object_t;

//e gets the string pointer from a string object (performs no type check)
#define OBJECT_STRING(obj) ((char *)(&((obj)->members[1])))

/*d
 * Alloziert ein neues Objekt fuer eine beliebige Klasse
 *
 * Die Anzahl der allozierten Felder wird als Optimierung explizit angegeben.
 *
 * @param classref Die zu verwendende Klasse
 * @param members_nr Anzahl der Felder der Klasse
 * @return Ein alloziertes Objekt
 */
/*e
 * Allocates a new object for an arbitrary class
 *
 * The number of fields must be passed explicitly
 *
 * @param classref Pointer to the type descriptor
 * @param members_nr Number of fields for this class
 * @return pointer to an allocated object
 */
object_t *
new_object(class_t *classref, unsigned long long members_nr);

/*d
 * Alloziert ein neues `Int' Objekt, um eine Ganzzahl zu repraesentieren
 */
/*e
 * Allocates a new Int object
 */
object_t *
new_int(long long int value);

/*d
 * Alloziert ein neues Real-Objekt, um eine Fliesskommazahl zu repraesentieren
 */
/*e
 * Allocates a new `Real' object (for floating point numbers)
 */
object_t *
new_real(double value);

/*d
 * Alloziert ein Zeichenketten-Objekt im AttoVM-Ablagespeicher
 *
 * @param string Zeiger auf die Zeichenkette (die im Ablagespeicher dupliziert wird)
 * @param len Länge der Zeichenkette (ohne abschließende Null)
 * @return Ein Zeiger auf das allozierte Objekt
 */
/*e
 * Allocates a character string on the AttoVM heap
 *
 * @param string Pointer to the initial characters to copy
 * @param len Length of the character string (minus terminator byte)
 * @return Pointer to the allocated object
 */
object_t *
new_string(char *value, size_t len);

/*d
 * Alloziert ein Array-Objekt
 *
 * Die einzelnen Einträge werden mit dem NULL-Objekt initialisiert.
 *
 * @param len Anzahl der Elemente im Array
 * @return Ein Zeiger auf das allozierte Objekt
 */
/*e
 * Allocates an array object
 *
 * Individual entries are initialised to be NULL
 *
 * @param len Number of allocated array elements
 * @return Pointer to the allocated array
 */
object_t *
new_array(size_t len);


/*d
 * Druckt ein gegebenes Objekt aus
 *
 * @param f Die Ausgabedatei
 * @param obj Das auszugebende Objekt
 * @param depth Rekursionstiefe
 * @param debug Ob Debug-Informationen mit ausgegeben werden sollen
 */
/*e
 * Prints an arbitrary object
 *
 * @param f The output file stream
 * @param obj Object to print
 * @param depth Max recursion depth
 * @param debug Whether to print debug information
 */
void
object_print(FILE *f, object_t *obj, int depth, bool debug);


//d Selektor-Zugriff
//e Selector access

/*d
 * Liest einen Methoden-Zeiger aus einem Objekt, wenn der Eintrag die korrekte Anzahl an Parametern hat
 *
 * @param obj Das zu lesende Objekt
 * @param node AST-Knoten des Zugriffs; wird zur Ausgabe einer Fehlermeldung verwendet, wenn der betreffende
 *             Selektor nicht vorhanden ist oder einen unpassenden Typ hat
 * @param selector Die gewuenschte Selektornummer der Methode
 * @param parameters_nr Anzahl der erwarteten Methodenparameter (der `self'-Parameter wird hierbei nicht gezaehlt!)
 * @return Zeiger auf den Code der Methode
 */
/*e
 * Reads a method pointer from an object, if the entry has the expected number of parameters
 *
 * @param obj Object to read from
 * @param node AST node for the access; used to issue an error message/determine the line number if the selector
 *             is missing or has the wrong type
 * @param selector Selector number
 * @param parameters_nr Number of expected parameters (NOT counting the `self' parameter in $a0)
 * @return Pointer to the method entry point (assembly code)
 */
void *
object_get_member_method(object_t *obj, ast_node_t *node, int selector, int parameters_nr);

/*d
 * Laed ein Obj-Feld aus einem Objekt
 *
 * @param obj Das zu bearbeitende Objekt
 * @param node AST-Knoten, zur Fehlerbehandlung
 * @param selector Die gewuenschte Selektornummer des Feldes
 */
/*e
 * Loads an object field from an object
 *
 * @param obj Object to read from
 * @param node AST node, for error handling
 * @param selector Selector number
 */
void *
object_read_member_field_obj(object_t *obj, ast_node_t *node, int selector);

/*d
 * Laed ein Int-Feld aus einem Objekt
 *
 * @param obj Das zu bearbeitende Objekt
 * @param node AST-Knoten, zur Fehlerbehandlung
 * @param selector Die gewuenschte Selektornummer des Feldes
 */
/*e
 * Loads an int field from an object (unboxing as needed)
 *
 * @param obj Object to read from
 * @param node AST node, for error handling
 * @param selector Selector number
 */
long long int
object_read_member_field_int(object_t *obj, ast_node_t *node, int selector);

/*d
 * Schreibt einen int-getyptes Feld in einem Objekt
 *
 * @param obj Das zu bearbeitende Objekt
 * @param node AST-Knoten, zur Fehlerbehandlung
 * @param selector Die gewuenschte Selektornummer
 * @param value
 */
/*e
 * Stores a value to an int-typed field in an object (boxing as needed)
 *
 * @param obj Object to modify
 * @param node AST node for error handling
 * @param selector Selector number
 * @param value
 */
void
object_write_member_field_int(object_t *obj, ast_node_t *node, int selector, long long int value);

/*d
 * Schreibt einen obj-getyptes Feld in einem Objekt
 *
 * @param obj Das zu bearbeitende Objekt
 * @param node AST-Knoten, zur Fehlerbehandlung
 * @param selector Die gewuenschte Selektornummer
 * @param value
 */
/*e
 * Stores an object reference in an object
 *
 * @param obj Object to modify
 * @param node AST node for error handling
 * @param selector Selector number
 * @param value
 */
void
object_write_member_field_obj(object_t *obj, ast_node_t *node, int selector, object_t *value);

#endif // !defined(_ATTOL_OBJECT_H)
