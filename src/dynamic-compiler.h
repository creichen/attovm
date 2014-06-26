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

#ifndef _ATTOL_DYNAMIC_COMPILER_H
#define _ATTOL_DYNAMIC_COMPILER_H

#include "ast.h"
#include "assembler-buffer.h"

// Dynamischer (Zur-Laufzeit) Uebersetzer und Unterstuetzungsroutinen

/**
 * Erzeugt Sprung zum Uebersetzer
 *
 * Der erzeugte Code erwartet die Symbolnummer des zu uebersetzenden Eintrags in Register $v0.
 * Er sichert die Argumentregister und ruft danach dyncomp_compile_funtion($v0, ret_addr) auf.
 */
buffer_t
dyncomp_build_generic_compiler_entrypoint();

/**
 * Uebersetzt eine Funktion und aktualisiert ggf. einen Rücksprungadresseneintrag auf dem Stapel
 *
 * @param symtab_entry Die zu uebersetzende Funktion
 * @param update_address_on_call_stack Adresse einer Sprungadresse, die mit der neuen Speicheradresse des
 * Funktionscodes ueberschrieben werden soll (oder NULL).
 */
void
dyncomp_compile_function(int symtab_entry, void **update_address_on_call_stack);

/**
 * Erzeugt Trampolin-Code fuer dyncomp_build_generic und aktualisiert Symboltabelleinetraege der Funktionen
 *
 * (siehe auch dyncom_build_generic)
 *
 * Die Funktionen werden 
 *
 * @param dyncomp_entry Ein mit dyncomp_build_generic() gebauter generischer Uebersetzer
 * @param functions, functions_nr: Die gegen Trampolincode zu bindenden Funktionen
 */
buffer_t
dyncomp_build_trampoline(void *dyncomp_entry, ast_node_t **functions, int functions_nr);

/**
 * Erzeugt einen generischen Einsprungpunkt fuer dyncomp_compile_function
 *
 * (siehe auch dyncomp_build_trampoline, dyncomp_compile_function)
 *
 * Der erzeugte Einsprungpunkt sichert die Argumentregister (bzw. stellt sie nach dem Aufruf wieder
 * her) und ruft dyncomp_compile_function($v0, &retaddr) auf, so dass nach Rueckkehr von diesem
 * Einsprungpunkt automatisch die erzeugte Funktion angesprungen wird.
 */
buffer_t
dyncomp_build_generic();

#endif // !defined(_ATTOL_DYNAMIC_COMPILER_H)
