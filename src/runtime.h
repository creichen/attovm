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

#ifndef _ATTOL_RUNTIME_H
#define _ATTOL_RUNTIME_H

//d Zentraler Sammelpunkt fuer Laufzeitsystem und Uebersetzer
//e central meeting point for run-time system and compiler

#include "ast.h"
#include "assembler-buffer.h"
#include "symbol-table.h"

typedef struct {
	//d Laufzeitinformationen
	//e run-time information

	buffer_t code_buffer;
	void *main_entry_point;	/*d Einsprungpunkt in das Text-Segment *//*e entry point into the text segment */

	int globals_nr;		/*e Number of global variables */
	int *globals;		/*e table of symbol IDs of global variables */
	void **static_memory;	/*d Statischer Speicher */ /*e static memory (containing global variables */

	int callables_nr;
	ast_node_t **callables;	/*d Funktionen und Konstruktoren *//*e functions and constructors */
	int classes_nr;
	ast_node_t **classes;

	storage_record_t storage;	/*e number of functions, variables, and temps (informs size of static_memory) */

	buffer_t dyncomp;	/*d Generischer Einsprungpunkt fuer den dynamischen Uebersetzer *//*e generic entrypoint for the dynamic compiler */
	buffer_t trampoline;	/*d Trampolin-Puffer: Hierher springen wir fuer eine nicht uebersetzte Funktion
	                         * In diesem Puffer findet sich nur ein kurzer Befehl, der die Funktionsnummer laed (nach $v0) und `dyncomp' aufruft. */

	ast_node_t *ast;	/*d Fertig analysierter abstrakter Syntaxbaum */
} runtime_image_t;


#define RUNTIME_ACTION_NONE			0 // only allocate the image and put in the AST
#define RUNTIME_ACTION_NAME_ANALYSIS		1
#define RUNTIME_ACTION_TYPE_ANALYSIS		2 // includes name analysis
#define RUNTIME_ACTION_SEMANTIC_ANALYSIS	3 // all semantic analysis passes
#define RUNTIME_ACTION_COMPILE			4 // do everything

/**
 * Bereitet ein runtime_image_t aus einem AST vor
 *
 * @param ast Der einzubettende abstrakte Syntaxbaum
 * @param action RUNTIME_ACTION_* um anzugeben, bis wohin die Vorbereitung durchgefuehrt werden soll
 * @return Ein runtime_image_t, oder NULL, falls bei der Programmanalyse ein Fehler entdeckt wurde.
 * Wenn RUNTIME_ACTION_COMPILE verwendet wurde, kann das image danach ausgefuhert werden.
 */
runtime_image_t *
runtime_prepare(ast_node_t *ast, unsigned int action);


/**
 * Fuehrt ein gegebenes runtime_image_t aus
 */
void
runtime_execute(runtime_image_t *);


/**
 * Dealloziert ein Runtime-Image
 */
void
runtime_free(runtime_image_t *);


/*d
 * Liefert das letztallozierte Laufzeit-Image
 */
/*e
 * Retrieves the most recently installed run-time image
 */
runtime_image_t *
runtime_current(void);

#endif // !defined(_ATTOL_RUNTIME_H)
