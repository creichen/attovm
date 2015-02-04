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
#include "symbol-table.h"

#define DYNCOMP_ADAPTIVE_SAMPLE_INTERVALS	5	/*e number of method calls until we sample parameter types */
#define DYNCOMP_ADAPTIVE_OPT_THRESHOLD		5	/*e number samples until we invoke adaptive compiler */

//d Dynamischer (Zur-Laufzeit) Uebersetzer und Unterstuetzungsroutinen
//e dynamic (at-runtime) compiler and support operations

/*d
 * Erzeugt Sprung zum Uebersetzer
 *
 * Der erzeugte Code erwartet die Symbolnummer des zu uebersetzenden Eintrags in Register $v0.
 * Er sichert die Argumentregister und ruft danach dyncomp_compile_funtion($v0, ret_addr) auf.
 */
/*e
 * Generates trampoline for invoking compiler at run-time
 *
 * The generated code expects the symbol number of the
 * function/method/constructor to translate in register $v0.  It saves
 * all argument registers and then calls dyncomp_compile_function($v0,
 * ret_addr).
 */
buffer_t
dyncomp_build_generic_compiler_entrypoint();

/*d
 * Uebersetzt eine Funktion und aktualisiert ggf. einen Rücksprungadresseneintrag auf dem Stapel
 *
 * @param symtab_entry Die zu uebersetzende Funktion
 * @param update_address_on_call_stack Adresse einer Sprungadresse, die mit der neuen Speicheradresse des
 * Funktionscodes ueberschrieben werden soll (oder NULL).
 */
/*e
 * Translates a function and optionally updates the stack return address.
 *
 * @param symtab_entry Symbol of the function to translate
 * @param update_address_on_call_stack If non-NULL: write the entry point of the generated function to
 * this address.  (Used by dyncomp_build_generic_compiler_entrypoint to `return' into the function.)
 */
void
dyncomp_compile_function(int symtab_entry, void **update_address_on_call_stack);

/*d
 * Erzeugt Trampolin-Code fuer dyncomp_build_generic und aktualisiert Symboltabelleinetraege der Funktionen
 *
 * (siehe auch dyncom_build_generic)
 *
 * @param dyncomp_entry Ein mit dyncomp_build_generic() gebauter generischer Uebersetzer
 * @param functions, functions_nr: Die gegen Trampolincode zu bindenden Funktionen
 */
/*e
 * Generates trampoline code for dyncomp_build_generic() and updates function symbol table entry 
 *
 * (cf. dyncomp_build_generic)
 *
 * @param dyncomp_entry A compiler constructed via dyncomp_build_generic()
 * @param functions, functions_nr The functions to bind against this trampoline code 
 */
buffer_t
dyncomp_build_trampoline(void *dyncomp_entry, ast_node_t **functions, int functions_nr);

/*d
 * Erzeugt einen generischen Einsprungpunkt fuer dyncomp_compile_function
 *
 * (siehe auch dyncomp_build_trampoline, dyncomp_compile_function)
 *
 * Der erzeugte Einsprungpunkt sichert die Argumentregister (bzw. stellt sie nach dem Aufruf wieder
 * her) und ruft dyncomp_compile_function($v0, &retaddr) auf, so dass nach Rueckkehr von diesem
 * Einsprungpunkt automatisch die erzeugte Funktion angesprungen wird.
 */
/*e
 * Builds a generic entry point for dyncom_conmpile_function
 *
 * (cf. dyncomp_build_trampoline(), dyncomp_compile_function())
 *
 * The entry point generated here saves the argument registers (and recovers them after the call)
 * and then calls dyncomp_compile_function($v0, &retaddr), so that the generated function will be
 * invoked audomatically after this call.
 */
buffer_t
dyncomp_build_generic();


/*e
 * Sets up a symbol table entry for sampling
 *
 * @param sym Symbol table entry to initialise
 */
void
dyncomp_init_unoptimised(symtab_entry_t *sym);

/*e
 * Performs runtime sampling of unoptimised code
 *
 * This function may invoke opt compilation.
 *
 * @param sym Symbol table entry of the function/method/constructor to optimise
 * @param low_args Pointer (on the stack) of arguments 0 through 5
 * @param high_args Pointer (on the stack) to arguments 6.  Note that argument 7 is at high_args[-1] etc.
 */
void
dyncomp_runtime_sample(symtab_entry_t *sym, struct object** low_args, struct object** high_args);

/*e
 * Deoptimises the specified function
 *
 * @param sym The function to deoptimise
 *
 * @return Entry point for that function (i.e., sym->r_mem);
 */
void *
dyncomp_deoptimise(symtab_entry_t *sym);

#endif // !defined(_ATTOL_DYNAMIC_COMPILER_H)
