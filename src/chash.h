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

#include <stdbool.h>
#include <stdio.h>

//e Number of linear follow-up addresses for `open address' resolution when handling hash conflicts
//d Anzahl der moeglichen linearen Folgeadressen zur `open address'-Aufloesung bei Hash-Konflikten
#define OPEN_ADDRESS_LINEAR_LENGTH 4

struct hashtable;

//e hash table type
//d Typ der Hashtabellen (Kurzform von `struct hashtable')
typedef struct hashtable hashtable_t;

//e type of hash functions, mapping table keys to unsigned longs
//d Typ der Hashfunktionen.  Diese Hashfunktionen liefern eine beliebig grosse `unsigned long'-Zahl.
typedef unsigned long (*hash_fn_t)(const void *);

//e type of comparison functions; yield 0 when equal
//d Typ der Vergleichsfunktion; liefert 0 bei Gleichheit
typedef int (*compare_fn_t)(const void *, const void *);

//e type of visitor functions for hashtable_foreach
//d Typ der Besuchsfunktion fuer die Funktion hashtable_foreach()
typedef void (*visit_fn_t)(void *key, void *value, void *state);

//e hash function for character strings
//d Hashfunktion fuer Zeichenketten (char *)
hash_fn_t hashtable_string_hash;

//e hash function for pointers
hash_fn_t hashtable_pointer_hash;
//e comparison function for pointers
compare_fn_t hashtable_pointer_compare;

//e hash function for long numbers
hash_fn_t hashtable_long_hash;
//e comparison function for longs
compare_fn_t hashtable_long_compare;

// ================================================================================
//d Hashtabellen
//e hash tables

/*d
 * Alloziert eine neue leere Hash-Tabelle.
 *
 * Bei der Allozierung gibt der Benutzer explizit an, welche Hash- und Vergleichsfunktionen verwendet
 * werden sollen.
 *
 * @param hash_fn 	 Die zu verwendende Hash-Funktion
 * @param compare_fn	 Die zu verwendende Vergleichsfunktion fuer Schluessel
 * @param size_exponent Anfangs zu verwendender Groessenfaktor (log_2 der tatsaechlichen Groesse)
 */
/*e
 * Allocates a new empty hash table
 *
 * During allocation, hashing and comparison functions must be supplied.
 *
 * @param hash_fn 	 hashing function to use 
 * @param compare_fn	 comparison function to use
 * @param size_exponent  initial size expontent (i.e., log_2 of initial table size)
 */
hashtable_t *
hashtable_alloc(hash_fn_t hash_fn, compare_fn_t compare_fn, unsigned char size_exponent);

/*d
 * Dealloziert eine Hash-Tabelle.
 *
 * Dabei werden die Schluessel- und Werteintraege in der Tabelle nach Bedarf durch eine vom Benutzer
 * angegebene Funktion dealloziert.
 *
 * @param tbl	     Tabelle
 * @param free_key   Optional (kann NULL sein): dealloziert Schluessel
 * @param free_value Optional (kann NULL sein): dealloziert Werte
 */
/*e
 * Deallocates a hash table.
 *
 * Keys and values may optionally be deallocated by user-supplied functions.
 *
 * @param tbl	     table to deallocate
 * @param free_key   either NULL or a function that deallocates hash keys up the 
 * @param free_value either NULL or a function that deallocates hash values up the 
 */
void
hashtable_free(hashtable_t *tbl, void (*free_key)(void *), void (*free_value)(void *));

/*d
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
hashtable_put(hashtable_t *tbl, void *key, void *value, void (*free_old_value)(void *));

void *
hashtable_get(hashtable_t *tbl, void *key);

/*d
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
/*e
 * Iterates over all table entries.
 *
 * Assume that table t maps "a" to "b" and "foo" to "bar".  Now, assume that we call
 *
 *   hashtable_foreach(t, f, &data);
 *
 * Then hashtable_foreach will invoke `f' as follows:
 *
 *   f("a", "b", &data);
 *   f("foo", "bar", &data);
 *
 * @param tbl	 table
 * @param visit  function to invoke on all key/value pairs (plus state)
 * @param state  initial state (optional; you can use this to provide an environment to `visit')
 */
void
hashtable_foreach(hashtable_t *tbl, visit_fn_t visit, void *state);

/*d
 * Klont eine Hashtabelle
 *
 * @param tbl	Tabelle
 * @param clone_key	Funktion zum Klonen der Tabellenschlüssel, oder NULL falls unnötig
 * @param clone_vlaue	Funktion zum Klonen der Tabellenwerte, oder NULL falls unnötig
 */
/*e
 * Clones a hash table
 *
 * @param tbl		table
 * @param clone_key	function for cloning keys, or NULL if keys are to be copied
 * @param clone_vlaue	function for cloning values, or NULL if values are to be copied directly
 */
hashtable_t *
hashtable_clone(hashtable_t *tbl, void *(*clone_key)(const void *), void *(*clone_value)(const void*));


// ================================================================================
//e hash sets for void *  (will not store NULL)

struct hashset_ptr;
typedef struct hashset_ptr hashset_ptr_t;

hashset_ptr_t *
hashset_ptr_alloc();

void
hashset_ptr_free(hashset_ptr_t *set);

void
hashset_ptr_add(hashset_ptr_t *set, void *ptr);

void
hashset_ptr_remove(hashset_ptr_t *set, void *ptr);

bool
hashset_ptr_contains(hashset_ptr_t *set, void *ptr);

size_t
hashset_ptr_size(hashset_ptr_t *set);

void
hashset_ptr_add_all(hashset_ptr_t *set, hashset_ptr_t *to_add);

void
hashset_ptr_remove_all(hashset_ptr_t *set, hashset_ptr_t *to_remove);

void
hashset_ptr_retain_common(hashset_ptr_t *set, hashset_ptr_t *other_set);

hashset_ptr_t *
hashset_ptr_clone(hashset_ptr_t *set);

void
hashset_ptr_foreach(hashset_ptr_t *set, void (*f)(void *ptr, void *state), void *state);

/*e
 * Prints all elements of the specified set
 *
 * @param set set to print
 * @param f output stream to print to
 * @param print_element print function to print single element, or NULL to print memory address
 */
void
hashset_ptr_print(FILE *f, hashset_ptr_t *set, void (*print_element)(FILE *, void *));


// ================================================================================
//e hash sets for `long' values (will not store 0x8000000000000000)

struct hashset_long;
typedef struct hashset_long hashset_long_t;

hashset_long_t *
hashset_long_alloc();

void
hashset_long_free(hashset_long_t *set);

void
hashset_long_add(hashset_long_t *set, long v);

void
hashset_long_remove(hashset_long_t *set, long v);

bool
hashset_long_contains(hashset_long_t *set, long v);

size_t
hashset_long_size(hashset_long_t *set);

void
hashset_long_add_all(hashset_long_t *set, hashset_long_t *to_add);

void
hashset_long_remove_all(hashset_long_t *set, hashset_long_t *to_remove);

void
hashset_long_retain_common(hashset_long_t *set, hashset_long_t *other_set);

hashset_long_t *
hashset_long_clone(hashset_long_t *set);

void
hashset_long_foreach(hashset_long_t *set, void (*f)(long value, void *state), void *state);

void
hashset_long_print(FILE *f, hashset_long_t *set);


#endif // !defined (_CHASH_H)
