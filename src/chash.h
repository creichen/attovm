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

#ifndef _CHASH_H
#define _CHASH_H

// Anzahl der moeglichen linearen Folgeadressen zur `open address'-Aufloesung bei Hash-Konflikten
#define OPEN_ADDRESS_LINEAR_LENGTH 4

struct hashtable;

// Typ der Hashtabellen (Kurzform von `struct hashtable')
typedef struct hashtable hashtable_t;

// Typ der Hashfunktionen.  Diese Hashfunktionen liefern eine beliebig grosse `unsigned long'-Zahl.
typedef unsigned long (*hash_fn_t)(const void *);

// Typ der Vergleichsfunktion; liefert 0 bei Gleichheit
typedef int (*compare_fn_t)(const void *, const void *);

// Typ der Besuchsfunktion fuer die Funktion hashtable_foreach()
typedef void (*visit_fn_t)(void *key, void *value, void *state);

// Hashfunktion fuer Zeichenketten (char *)
hash_fn_t hashtable_string_hash;

hash_fn_t hashtable_pointer_hash;
compare_fn_t hashtable_pointer_compare;

/**
 * Alloziert eine neue leere Hash-Tabelle.
 *
 * Bei der Allozierung gibt der Benutzer explizit an, welche Hash- und Vergleichsfunktionen verwendet
 * werden sollen.
 *
 * @param hash_fn 	 Die zu verwendende Hash-Funktion
 * @param compare_fn	 Die zu verwendende Vergleichsfunktion fuer Schluessel
 * @param size_exponent Anfangs zu verwendender Groessenfaktor (log_2 der tatsaechlichen Groesse)
 */
hashtable_t *
hashtable_alloc(hash_fn_t hash_fn, compare_fn_t compare_fn, unsigned char size_exponent);

/**
 * Dealloziert eine Hash-Tabelle.
 *
 * Dabei werden die Schluessel- und Werteintraege in der Tabelle nach Bedarf durch eine vom Benutzer
 * angegebene Funktion dealloziert.
 *
 * @param tbl	     Tabelle
 * @param free_key   Optional (kann NULL sein): dealloziert Schluessel
 * @param free_value Optional (kann NULL sein): dealloziert Werte
 */
void
hashtable_free(hashtable_t *tbl, void (*free_key)(void *), void (*free_value)(void *));

/**
 * Greift auf die Tabelle zu und traegt evtl. einen neuen Eintrag ein.
 *
 * Falls ein Element mit dem Schluessel `key' vorhanden ist, wird ein Zeiger auf den zugeordneten
 * Wert zurueckgeliefert, sonst NULL.  NULL ist als Schluessel nicht erlaubt.
 *
 * Falls noch kein Element mit dem Schluessel `key' vorhanden ist UND `value' ungleich NULL ist, wird
 * `value' als neuer Wert fuer den betreffenden Schluessel eingetragen.
 *
 * Falls kein Platz fuer das betreffende Element in der linearen Folgenadresse vorhanden ist, wird
 * die Tabelle automatisch vergroessert.
 * 
 * @param tbl	 Tabelle
 * @param key	 Der nachzuschlagende Schluessel
 * @param value  Der einzutragende Wert, oder NULL, um das Vorhandensein eines Wertes zu pruefen
 * @return NULL falls das Element eingefuegt wurde, ansonsten ein Zeiger auf den existierenden Wert.
 * Wenn versucht wurde, einzufuegen, aber der Schluessel schon existierte, wird die Tabelle NICHT veraendert.
 */
void **
hashtable_access(hashtable_t *tbl, void *key, void *value);

void
hashtable_put(hashtable_t *tbl, void *key, void *value, void (*free_value)(void *));

void *
hashtable_get(hashtable_t *tbl, void *key);

/**
 * Iteriert ueber alle Eintraege der Tabelle.
 *
 * Angenommen, eine Tabelle t bildet "a" auf "b" ab und "foo" auf "bar", und wir rufen auf:
 *
 *   hashtable_foreach(t, f, &data);
 *
 * Dann soll diese Funktion die uebergebene Funktion `f' mit folgenden Parametern aufrufen:
 *
 *   f("a", "b", &data);
 *   f("foo", "bar", &data);
 *
 * @param tbl	 Tabelle
 * @param visit  Funktion wird auf allen eingetragenen Schluessel/Wert-Kombinationen ausgefuhert
 * @param state  Initialer Zustand
 */
void
hashtable_foreach(hashtable_t *tbl, visit_fn_t visit, void *state);

/**
 * Klont eine Hashtabelle
 *
 * @param tbl	Tabelle
 * @param clone_key	Funktion zum Klonen der Tabellenschlüssel, oder NULL falls unnötig
 * @param clone_vlaue	Funktion zum Klonen der Tabellenwerte, oder NULL falls unnötig
 */
hashtable_t *
hashtable_clone(hashtable_t *tbl, void *(*clone_key)(const void *), void *(*clone_value)(const void*));

#endif // !defined (_CHASH_H)
