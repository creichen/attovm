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


// Temp-Speicher:
//
// Um Zwischenwerte zu speichern, werden `Temp-Speicher' verwendet.  Diese sind Integer-Zahlen:
// >= 0:  Registernummer
// <= 0:  Index relativ zu $fp


/**
 * Erzeugt Maschinencode für einen Ausdruck
 * 
 * @param dest Zielpuffer zur Maschinencodegenerierung
 * @param node AST-Knoten zur Übersetzung
 * @param temp_stores NULL, oder ein Array der Speicher, in die Zwischenergebnisse geschrieben werden können (s.o.)
 * @param temp_stores_nr Anzahl der Temp-Speicher
 * @param dest_register Zielregister, in dem das Ergebnis gespeichert werden muß
 * @returns Eine Bitmaske mit AST_FLAG_* (für Typinformationen) und RESULT_FLAG_*
 */
int
baseline_compile_expr(buffer_t *dest, int dest_register, ast_node_t *node, int *temp_regs, int temp_regs_nr);


/* /\** */
/*  * Erzeugt Code, der von to_type nach from_type konvertiert, im angegebenen Register */
/*  * */
/*  * @param dest Zielpuffer zur Maschinencodegenerierung */
/*  * @param dest_type Zieltyp */
/*  * @param from_type Quelltyp */
/*  * @param reg Register, in dem der zu konvertierende Wert liegen */
/*  *\/ */
/* void */
/* baseline_compile_convert(buffer_t *dest, int dest_type, int from_type, int reg); */


#endif // defined(_ATTOL_BASELINE_BACKEND_H)
