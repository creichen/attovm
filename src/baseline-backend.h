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

#ifndef _ATTOL_BASELINE_BACKEND_H
#define _ATTOL_BASELINE_BACKEND_H

#include "ast.h"
#include "assembler-buffer.h"
#include "symbol-table.h"

/**
 * Uebersetzt ein AST-Fragment eines Haupteinsprungpunktes in ausfuehrbaren
 * Maschinencode
 *
 * @param node Der zu uebersetzende AST-Baumknoten
 * @param static_memory Der statische Speicher fuer Variableninhalte
 * @return Ein buffer_t mit dem Haupteinsprungpunkt
 */
buffer_t
baseline_compile_entrypoint(ast_node_t *node, void *static_memory);

/**
 * Uebersetzt ein AST-Fragment einer Funktion in ausfuehrbaren Maschinencode
 *
 * @param sym Symboltabelleneintrag
 * @return Ein buffer_t mit dem Funktionskoerper
 */
buffer_t
baseline_compile_static_callable(symtab_entry_t *sym);

/**
 * Uebersetzt ein AST-Fragment einer Methode in ausfuehrbaren Maschinencode
 *
 * @param sym Symboltabelleneintrag
 * @return Ein buffer_t mit dem Funktionskoerper
 */
buffer_t
baseline_compile_method(symtab_entry_t *sym);

#endif // defined(_ATTOL_BASELINE_BACKEND_H)
