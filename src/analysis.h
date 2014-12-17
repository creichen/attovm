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

#ifndef _ATTOL_ANALYSIS_H
#define _ATTOL_ANALYSIS_H

#include <stdbool.h>

#include "ast.h"
#include "symbol-table.h"

/*d
 * Ersetzt alle AST_VALUE_NAME-Knoten durch AST_VALUE_ID-Knoten
 *
 * Erste Analysephase
 *
 * @param globals_nr Variable, in die die Anzahl der beobachteten globalen Variablen geschrieben wird
 * @param functions_nr Variable, in die die Anzahl der definierten Funktionen geschrieben wird (keine Methoden)
 * @param classes_nr Variable, in die die Anzahl der definierten Klassen geschrieben wird
 * @return Anzahl der beobachteten Fehler
 */
int
name_analysis(ast_node_t *, storage_record_t *storage, int *classes_nr);


/*d
 * Fuehrt Typanalyse durch:
 *
 * - Fuegt *convert-Knoten ein, wo noetig
 * - Konvertiert Parameter vor und nach Methodenaufrufen, soweit noetig
 * - Fuegt METHODAPP-Knoten bei Methodenaufruf ein
 * - Stellt sicher, dass keine unerlaubten Variablenzugriffe stattfinden
 * - Erstellt den Konstruktor fuer jede Klass
 * - Markiert Fehler, wenn Fliesskommazahlen verwendet werden
 *
 * Zweite Analysephase
 *
 * @param callables Tabelle, in die alle Funktionen und Konstruktoren eingetragen werden
 * @param classes Tabelle, in die alle Klassen eingetragen werden
 * @param globals Tabelle, die die Symboltabelleneinträge der globalen Variablen beinhaltet (je nach Position im globalen Variablenblock)
 * @return Anzahl der beobachteten Fehler
 */
int
type_analysis(ast_node_t **, ast_node_t **callables, ast_node_t **classes, int *globals);

struct data_flow_analysis;
extern struct data_flow_analysis *data_flow_analyses_correctness[];
extern struct data_flow_analysis *data_flow_analyses_optimisation[];

/*e
 * Runs intra-procedural data flow analyses (and, possibly, AST-transformation based updates)
 *
 * These depend on control flow graphs (control-flow-graph.h) having been built.
 *
 * @param sym The symbol to analyse/optimise
 * @param recursive Whether to recursively analyse any encountered syb-
 * @return Number of observed errors
 */
int
data_flow_analyses(symtab_entry_t *sym, struct data_flow_analysis *analyses[]);

#endif 
