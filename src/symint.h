/***************************************************************************
  Copyright (C) 2015 Christoph Reichenbach


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

#ifndef _ATTOL_SYMINT_H
#define _ATTOL_SYMINT_H

#include "symbol-table.h"

/*e
 * Basic support for a symbolic `int' type
 *
 * Supports:
 * - any literal integer
 * - variables (assumed to be represented by >= 0 variable IDs referencing a local variable, for pretty-printing)
 * - Top/constradiction as designated element
 */

typedef struct {
	long int nr;	/*e Numeric bound; only used if var_nr == INT_BOUND_ABSOLUTE */
	short kind;	/*e INT_BOUND_ABSOLUTE, INT_BOUND_NONE, or otherwise number of the local variable whose property we're describing */
} symint_t;


//e Constructing and testing for different kinds of sybolic integers:

//e Variable:
symint_t symint_var(int varnr);
bool symint_is_var(symint_t symint);

//e Literal:
symint_t symint_literal(long int v);
bool symint_is_literal(symint_t symint);
long int symint_literal_value(symint_t symint);

//e Top:
symint_t symint_top();
bool symint_is_top(symint_t bound);


//e Arithmetic operations:

// Negate symbolic integer (i.e., compute -symint)
symint_t
symint_negate(symint_t symint);

// Compute the smaller of two symbolic integers
symint_t
symint_min(symint_t lhs, symint_t rhs);

// Compute the larger of two symbolic integers
symint_t
symint_max(symint_t lhs, symint_t rhs);

/*
 * Test whether lhs <= rhs is guaranteed
 *
 * @param lhs, rhs: Symbolic integers to compare
 */
bool
symint_less_than_or_equal_numerically(symint_t lhs, symint_t rhs);

// Test whether lhs = rhs
bool
symint_equals(symint_t lhs, symint_t rhs);

/*e
 * Prints out a symint
 *
 * @param file The file to print to
 * @param var_symbols Array of local variable symbols to use for printing, or NULL
 * @param var_format Formatting string for the local variable name; must contain precisely one `%s'
 * @param symint Symbolic integer to print
 */
void
symint_print(FILE *file, symtab_entry_t **var_symbols, char *var_format, symint_t symint);


#endif // !defined(_ATTOL_SYMINT_H)
