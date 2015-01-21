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

#include <assert.h>
#include <limits.h>

#include "symint.h"

#ifndef MIN
#  define MIN(a, b) (((a) < (b))? (a) : (b))
#endif
#ifndef MAX
#  define MAX(a, b) (((a) > (b))? (a) : (b))
#endif

//================================================================================
//e Classification data type and operations

#define INT_BOUND_VARIABLE_MIN	0	/*e Lowest index used for var_nr */
#define INT_BOUND_ABSOLUTE	-1	/*e Absolute bound */
#define INT_BOUND_NONE		-2	/*e TOP element: unknown bound */

/*e
 * This symint_t describes a variable
 */
bool
symint_is_var(symint_t bound)
{
	return bound.kind >= INT_BOUND_VARIABLE_MIN;
}

/*e
 * This symint_t is not a bound (top element, unbounded)
 */
bool
symint_is_top(symint_t bound)
{
	return bound.kind == INT_BOUND_NONE;
}

/*e
 * This symint_t is a concrete numeric bound
 */
bool
symint_is_literal(symint_t bound)
{
	return bound.kind == INT_BOUND_ABSOLUTE;
}

void
symint_print(FILE *file, symtab_entry_t **var_symbols, char *format_string, symint_t bound)
{
	if (symint_is_var(bound)) {
		fprintf(file, format_string, var_symbols ? (var_symbols[bound.kind]->name) : "?");
	} else if (bound.kind == INT_BOUND_NONE) {
		fprintf(file, "T");
	} else {
		fprintf(file, "%ld", bound.nr);
	}
}

bool
symint_less_than_or_equal_numerically(symint_t lhs, symint_t rhs)
{
	if (lhs.kind == INT_BOUND_NONE || rhs.kind == INT_BOUND_NONE) {
		return false;
	}
	if (lhs.kind == INT_BOUND_ABSOLUTE && rhs.kind == INT_BOUND_ABSOLUTE) {
		return lhs.nr <= rhs.nr;
	}
	return lhs.kind == rhs.kind;
}

bool
symint_equals(symint_t lhs, symint_t rhs)
{
	return symint_less_than_or_equal_numerically(lhs, rhs)
		&&  symint_less_than_or_equal_numerically(rhs, lhs);
}

symint_t
symint_literal(long int v)
{
	return (symint_t) { .kind = INT_BOUND_ABSOLUTE, .nr = v };
}

symint_t
symint_var(int varnr)
{
	return (symint_t) { .kind = varnr, .nr = 0 };
}

symint_t
symint_top()
{
	return (symint_t) { .kind = INT_BOUND_NONE, .nr = 0 };
}

symint_t
symint_negate(symint_t bound)
{
	if (symint_is_literal(bound)
	    && bound.nr != LONG_MIN) { /*e can't negate LONG_MIN, as -LONG_MIN = LONG_MAX + 1 */
		bound.nr = -bound.nr;
		return bound;
	}
	return symint_top();
}

symint_t
symint_join(symint_t lhs, symint_t rhs, bool min)
{
	if (lhs.kind != rhs.kind) {
		return symint_top();
	}
	if (lhs.kind == INT_BOUND_ABSOLUTE) {
		if (min) {
			lhs.nr = MIN(lhs.nr, rhs.nr);
		} else {
			lhs.nr = MAX(lhs.nr, rhs.nr);
		}
	}
	return lhs;
}

symint_t
symint_min(symint_t lhs, symint_t rhs)
{
	return symint_join(lhs, rhs, true);
}


symint_t
symint_max(symint_t lhs, symint_t rhs)
{
	return symint_join(lhs, rhs, true);
}


long int
symint_literal_value(symint_t sym)
{
	return sym.nr;
}
