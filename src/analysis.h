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

#include "ast.h"

/**
 * Ersetzt alle AST_VALUE_NAME-Knoten durch AST_VALUE_ID-Knoten
 *
 * Erste Analysephase
 *
 * @return Anzahl der beobachteten Fehler
 */
int
name_analysis(ast_node_t *);


/**
 * Fuehrt Typanalyse durch:
 *
 * - Fuegt __convert-Knoten ein, wo noetig
 * - Konvertiert Parameter vor und nach Methodenaufrufen, soweit noetig
 * - Fuegt Let-Knoten bei Methodenaufruf ein
 * - Fuegt __self-Knoten bei lokalen Methodenaufrufen und Feldzugriffen ein
 * - Stellt sicher, dass keine unerlaubten Variablenzugriffe stattfinden
 * - Erstellt den Konstruktor fuer jede Klass
 * - Markiert Fehler, wenn Fliesskommazahlen verwendet werden
 *
 * Zweite Analysephase
 *
 * @return Anzahl der beobachteten Fehler
 */
int
type_analysis(ast_node_t **);


/**
 * Alloziert Speicherstellen fuer alle temporaeren Ergebnisse und globalen Variablen
 *
 * Dritte Analysephase
 *
 * @return Liefert die Anzahl der globalen Variableneintraege zurueck
 */
int
storage_allocation(ast_node_t *);

#endif 
