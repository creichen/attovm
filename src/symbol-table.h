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

/**
 * SELECTOR-Einträge sind impräzise:  Aus Sicht außerhalb einer Klasse ist nur die `selector'-Nummer
 * notwendigerweise korrekt.  Ein Zugriff auf ein SELECTOR-Symbol darf sich daher nur am Feld `selector' orientieren;
 * alle andere Informationen sind zunächst bedeutungslos.
 */

#define SYMTAB_TY_MASK		0x0003
#define SYMTAB_TY_VAR		0x0001
#define SYMTAB_TY_FUNCTION	0x0002
#define SYMTAB_TY_CLASS		0x0003

#define SYMTAB_SELECTOR		0x0004	// Selektor in ein Objekt
#define SYMTAB_PARAM		0x0008	// nur mit TY_VAR
#define SYMTAB_MEMBER		0x0010	// Klassenelement, also Methode oder Feld (zusammen mit TY_FUNCTION oder TY_VAR)
#define SYMTAB_BUILTIN		0x0020  // Eingebaute Funktion
#define SYMTAB_HIDDEN		0x0040	// Name nicht explizit angegeben
#define SYMTAB_COMPILED		0x0080	// Fertig uebersetzt
#define SYMTAB_OPT		0x0100	// Mit Optimierungen übersetzt


#define SYMTAB_TY(s)			(((s)->symtab_flags) & SYMTAB_TY_MASK)
#define SYMTAB_IS_STATIC(s)		(!(s)->parent && (SYMTAB_TY(s) == SYMTAB_TY_VAR))	// static-alloziert
#define SYMTAB_IS_STACK_DYNAMIC(s)	((s)->parent && (!((s)->symtab_flags & SYMTAB_MEMBER)))	// Stack oder Register-alloziert

typedef struct symtab_entry {
	char *name;
	struct symtab_entry *parent;		// Struktureller Elterneintrag
	unsigned short occurrence_count;	// Wievielte Elemente mit diesem Namen wurden überschattet? (Nur zur eindeutigen Identifizierung)
	unsigned short ast_flags;
	unsigned short symtab_flags;
	unsigned short parameters_nr;
	void *r_mem;				// Zeiger auf Funktion / Klassenobjekt
	unsigned short *parameter_types;
	ast_node_t *astref;			// CLASSDEF, FUNDEF, oder VARDECL
	short id;				// Symboltabellennummer dieses Eintrags
	unsigned short selector;		// Globale ID für Felder und Methoden
	unsigned short vars_nr;			/* Klasse: Anzahl Felder
						 * Methode/Funktion: Anzahl lokale Variablen
						 */
	unsigned short methods_nr;
	unsigned short offset;			/* MEMBER | VAR: Offset in Speicher der Struktur
						 * MEMBER | FUNCTION: Eindeutige Funktionsnummer
						 * PARAM: Parameternummer
						 * VAR: Variablennummer, entweder auf dem Stapel oder im statischen Speicher
						 */
} symtab_entry_t;

// Symbol IDs:
// Symbol table entry 0 is always illegal, so regular entries start at 1 and builtin entries start at -1.

extern int symtab_entries_builtin_nr; // builtin symbol table entries (positive nuber, negate this to get the ID)
extern int symtab_entries_nr; // non-builtin symbol table entries

/**
 * Schlägt einen Eintrag in der Symboltabelle nach.
 */
symtab_entry_t *
symtab_lookup(int id);


/**
 * Alloziert einen neuen Eintrag in die benutzerdefinierten Symboltabelle.
 * 
 */
symtab_entry_t *
symtab_new(int ast_flags, int symtab_flags, char *name, ast_node_t *declaration);


/**
 * Alloziert einen neuen Eintrag in der Symboltabelle für eingebaute (built-in) Operationen.
 *
 * Sollte mit symtab_id = 0 erst aufgerufen werden, nachdem alle Einträge mit festen Nummern
 * vergeben wurden.
 * 
 * @param symtab_id Beantregte Nummer für den Eintrag, oder `0' für `beliebig'
 */
symtab_entry_t *
symtab_builtin_new(int symtab_id, int ast_flags, int symtab_flags, char *name);


/**
 * Druckt den kanonischen (eindeutigen) Namen des Eintrages aus, für Debug-Zwecke
 */
void
symtab_entry_name_dump(FILE *file, symtab_entry_t *entry);


/**
 * Ausgabe des Eintrags, für Debugzwecke
 */
void
symtab_entry_dump(FILE *file, symtab_entry_t *entry);


#endif // !defined(_ATTOL_SYMBOL_TABLE_H)
