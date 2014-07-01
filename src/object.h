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

// Objektrepräsentierung

#ifndef _ATTOL_OBJECT_H
#define _ATTOL_OBJECT_H

#include <stdbool.h>
#include <stdio.h>

#include "symbol-table.h"
#include "class.h"

typedef struct object {
	class_t *classref; // Zeigt auf Klassenobjekt
	union {
		long long int int_v;
		double real_v;
		struct object *object_v;
		char string;
	} members[];
} object_t;


/**
 * Alloziert ein neues Objekt fuer eine beliebige Klasse
 *
 * Die Anzahl der allozierten Felder wird als Optimierung explizit angegeben.
 *
 * @param classref Die zu verwendende Klasse
 * @param members_nr Anzahl der Felder der Klasse
 * @return Ein alloziertes Objekt
 */
object_t *
new_object(class_t *classref, unsigned long long members_nr);

/**
 * Alloziert ein neues Int-Objekt, um eine Ganzzahl zu repraesentieren
 */
object_t *
new_int(long long int value);

/**
 * Alloziert ein neues Real-Objekt, um eine Fliesskommazahl zu repraesentieren
 */
object_t *
new_real(double value);

/**
 * Alloziert ein Zeichenketten-Objekt im AttoVM-Ablagespeicher
 *
 * @param string Zeiger auf die Zeichenkette (die im Ablagespeicher dupliziert wird)
 * @param len Länge der Zeichenkette (ohne abschließende Null)
 * @return Ein Zeiger auf das allozierte Objekt
 */
object_t *
new_string(char *value, size_t len);

/**
 * Alloziert ein Array-Objekt
 *
 * Die einzelnen Einträge werden mit dem NULL-Objekt initialisiert.
 *
 * @param len Anzahl der Elemente im Array
 * @return Ein Zeiger auf das allozierte Objekt
 */
object_t *
new_array(size_t len);


/**
 * Druckt ein gegebenes Objekt aus
 *
 * @param f Die Ausgabedatei
 * @param obj Das auszugebende Objekt
 * @param depth Rekursionstiefe
 * @param debug Ob Debug-Informationen mit ausgegeben werden sollen
 */
void
object_print(FILE *f, object_t *obj, int depth, bool debug);


// Selektor-Zugriff

/**
 * Liest einen Methoden-Zeiger aus einem Objekt, wenn der Eintrag die korrekte Anzahl an Parametern hat
 *
 * @param obj Das zu lesende Objekt
 * @param node AST-Knoten des Zugriffs; wird zur Ausgabe einer Fehlermeldung verwendet, wenn der betreffende
 *             Selektor nicht vorhanden ist oder einen unpassenden Typ hat
 * @param selector Die gewuenschte Selektornummer der Methode
 * @param parameters_nr Anzahl der erwarteten Methodenparameter (der `self'-Parameter wird hierbei nicht gezaehlt!)
 * @return Zeiger auf den Code der Methode
 */
void *
object_get_member_method(object_t *obj, ast_node_t *node, int selector, int parameters_nr);

/**
 * Laed ein Obj-Feld aus einem Objekt
 *
 * @param obj Das zu bearbeitende Objekt
 * @param node AST-Knoten, zur Fehlerbehandlung
 * @param selector Die gewuenschte Selektornummer des Feldes
 * @param desired_type Der erwartete Typ (Typkonvertierungen werden mit durchgefuehrt)
 */
void *
object_read_member_field_obj(object_t *obj, ast_node_t *node, int selector);

/**
 * Laed ein Int-Feld aus einem Objekt
 *
 * @param obj Das zu bearbeitende Objekt
 * @param node AST-Knoten, zur Fehlerbehandlung
 * @param selector Die gewuenschte Selektornummer des Feldes
 * @param desired_type Der erwartete Typ (Typkonvertierungen werden mit durchgefuehrt)
 */
long long int
object_read_member_field_int(object_t *obj, ast_node_t *node, int selector);

/**
 * Schreibt einen int-getyptes Feld in einem Objekt
 *
 * @param obj Das zu bearbeitende Objekt
 * @param node AST-Knoten, zur Fehlerbehandlung
 * @param selector Die gewuenschte Selektornummer
 * @param value
 */
void
object_write_member_field_int(object_t *obj, ast_node_t *node, int selector, long long int value);

/**
 * Schreibt einen obj-getyptes Feld in einem Objekt
 *
 * @param obj Das zu bearbeitende Objekt
 * @param node AST-Knoten, zur Fehlerbehandlung
 * @param selector Die gewuenschte Selektornummer
 * @param value
 */
void
object_write_member_field_obj(object_t *obj, ast_node_t *node, int selector, object_t *value);

#endif // !defined(_ATTOL_OBJECT_H)
