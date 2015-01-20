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
#include <string.h>

#include "ast.h"
#include "symbol-table.h"
#include "data-flow.h"
#include "bitvector.h"

#ifndef MIN
#  define MIN(a, b) (((a) < (b))? (a) : (b))
#endif
#ifndef MAX
#  define MAX(a, b) (((a) > (b))? (a) : (b))
#endif
//e negative `abs()':  guaranteed to avoid overflow
#define NABS(x) (((x) > 0) ? -(x) : (x))

//================================================================================
//e Classification data type and operations

#define INT_BOUND_NOVAR	-1	//e absolute bound
typedef struct {
	long int nr;		/*e Numeric bound; only used if var_nr == INT_BOUND_NOVAR */
	short var_nr;	/*e INT_BOUND_NOVAR or otherwise number of the local variable whose size we're describing */
} int_bound_t;

/*e
 * This int_bound_t describes the size of an array, notation `sizeof(a)', for some array variable `a'
 */
static bool
cla_int_bound_is_sizeof(int_bound_t bound)
{
	return bound.var_nr != INT_BOUND_NOVAR;
}

static void
cla_int_bound_print(FILE *file, symtab_entry_t **var_symbols, int_bound_t bound)
{
	if (cla_int_bound_is_sizeof(bound)) {
		fprintf(file, "size(%s)", var_symbols[bound.var_nr]->name);
	} else {
		fprintf(file, "%ld", bound.nr);
	}
}

static bool
cla_int_bound_less_than_or_equal(int_bound_t lhs, int_bound_t rhs)
{
	if (lhs.var_nr == INT_BOUND_NOVAR && rhs.var_nr == INT_BOUND_NOVAR) {
		return lhs.nr <= rhs.nr;
	}
	return lhs.var_nr == rhs.var_nr;
}

static int_bound_t
cla_int_bound_literal(int v)
{
	return (int_bound_t) { .var_nr = INT_BOUND_NOVAR, .nr = v };
}

static int_bound_t
cla_int_bound_sizeof(int varnr)
{
	return (int_bound_t) { .var_nr = varnr, .nr = 0 };
}

//e Classification
typedef struct {
	union { /*e union: use EITHER int_bounds OR array_info */
		struct { /*e with VARTYPE_INT only */
			int_bound_t min, max;
		} int_bounds;

		int array_size; /*e with VARTYPE_ARRAY only */
	} p;
	unsigned char vartype; /*e One of VARTYPE_* */
} classification_t;

#define VARTYPE_BOT	0
#define VARTYPE_INT	1
#define VARTYPE_ARRAY	2
#define VARTYPE_TOP	3

#define INT_TOP 0x80000000	/*e unknown array size */


static void
cla_array_bound_print(FILE *file, int bound)
{
	if (bound == INT_TOP) {
		fprintf(file, "T");
	} else {
		fprintf(file, "%d", bound);
	}
}

static void
cla_print(FILE *file, symtab_entry_t **var_symbols, classification_t *classification)
{
	switch (classification->vartype) {
	case VARTYPE_BOT:
		fprintf(file, "_");
		break;
	case VARTYPE_TOP:
		fprintf(file, "T");
		break;
	case VARTYPE_ARRAY:
		fprintf(file, "a[/");
		cla_array_bound_print(file, classification->p.array_size);
		fprintf(file, "]");
		break;
	case VARTYPE_INT:
		fprintf(file, "{");
		cla_int_bound_print(file, var_symbols, classification->p.int_bounds.min);
		fprintf(file, "..");
		cla_int_bound_print(file, var_symbols, classification->p.int_bounds.max);
		fprintf(file, "}");
		break;
	default:
		fprintf(file, "?(%d)ERROR\n", classification->vartype);
	}
}

static bool
cla_less_than_or_equal(classification_t *lhs, classification_t *rhs)
{
	//e trivial top/bottom element occurrence?
	if (lhs->vartype == VARTYPE_BOT
	    || rhs->vartype == VARTYPE_TOP) {
		return true;
	}

	//e not comparable?
	if (lhs->vartype != rhs->vartype) {
		return false;
	}

	switch (lhs->vartype) {

	case VARTYPE_ARRAY:
		if (lhs->p.array_size == INT_TOP) {
			return true; //e unknown array size is less concrete than known
		}
		return lhs->p.array_size == rhs->p.array_size;

	case VARTYPE_INT:
		//e more precise if lhs interval is contained in rhs interval
		return cla_int_bound_less_than_or_equal(rhs->p.int_bounds.min, lhs->p.int_bounds.min)
		    && cla_int_bound_less_than_or_equal(lhs->p.int_bounds.max, rhs->p.int_bounds.max);

	default:
		fprintf(stderr, "%s: ?(%d)ERROR\n", __func__, lhs->vartype);
		exit(1);
	}
}

static classification_t
cla_top()
{
	return (classification_t) { .vartype = VARTYPE_TOP };
}

static classification_t
cla_bottom()
{
	return (classification_t) { .vartype = VARTYPE_BOT };
}

static classification_t
cla_int(int i)
{
	classification_t classification = { .vartype = VARTYPE_INT };
	classification.p.int_bounds.max = cla_int_bound_literal(i);
	classification.p.int_bounds.min = classification.p.int_bounds.max;
	return classification;
}

static classification_t
cla_array(int size)
{
	return (classification_t) { .vartype = VARTYPE_ARRAY, .p.array_size = size };
}

static classification_t
cla_sizeof(int var_nr)
{
	classification_t classification = { .vartype = VARTYPE_INT };
	classification.p.int_bounds.max = cla_int_bound_sizeof(var_nr);
	classification.p.int_bounds.min = classification.p.int_bounds.max;
	return classification;
}

static long int
int_add(long int a, long int b)
{
	return a + b;
}

static long int
int_sub(long int a, long int b)
{
	return a - b;
}

static long int
int_mul(long int a, long int b)
{
	return a * b;
}

static classification_t
cla_arithmetic(long int (*f)(long int a, long int b), classification_t lhs, classification_t rhs, long int safety_bound)
{
	//e integer arithmetic only makes sense with literal ints:
	if (lhs.vartype != VARTYPE_INT
	    || rhs.vartype != VARTYPE_INT
	    || cla_int_bound_is_sizeof(lhs.p.int_bounds.min)
	    || cla_int_bound_is_sizeof(lhs.p.int_bounds.max)
	    || cla_int_bound_is_sizeof(rhs.p.int_bounds.min)
	    || cla_int_bound_is_sizeof(rhs.p.int_bounds.max)
	    //e also guard against overflow:
	    //e We compute the negative abs because two's complement permits one more negative
	    //e value than positive values
	    || (NABS(lhs.p.int_bounds.min.nr) < -safety_bound)
	    || (NABS(lhs.p.int_bounds.max.nr) < -safety_bound)
	    || (NABS(rhs.p.int_bounds.min.nr) < -safety_bound)
	    || (NABS(rhs.p.int_bounds.max.nr) < -safety_bound)) {
		return cla_top();
	}
	//e compute all combinations
	long int extreme0 = f(lhs.p.int_bounds.min.nr, rhs.p.int_bounds.min.nr);
	long int extreme1 = f(lhs.p.int_bounds.max.nr, rhs.p.int_bounds.min.nr);
	long int extreme2 = f(lhs.p.int_bounds.min.nr, rhs.p.int_bounds.max.nr);
	long int extreme3 = f(lhs.p.int_bounds.max.nr, rhs.p.int_bounds.max.nr);
	lhs.p.int_bounds.min.nr = MIN(MIN(extreme0, extreme1), MIN(extreme2, extreme3));
	lhs.p.int_bounds.max.nr = MAX(MAX(extreme0, extreme1), MAX(extreme2, extreme3));
	return lhs;
}

static classification_t
cla_join(classification_t lhs, classification_t rhs)
{
	if (lhs.vartype == VARTYPE_BOT) {
		return rhs;
	}
	if (rhs.vartype == VARTYPE_BOT) {
		return lhs;
	}
	if (lhs.vartype != VARTYPE_TOP
	    && rhs.vartype != VARTYPE_TOP
	    && lhs.vartype == rhs.vartype) {
		switch (lhs.vartype) {
		case VARTYPE_ARRAY:
			if (lhs.p.array_size == rhs.p.array_size) {
				return lhs;
			}
			//e unknown or inconsistent array size
			lhs.p.array_size = INT_TOP;
			return lhs;

		case VARTYPE_INT:
			if (lhs.p.int_bounds.min.var_nr != rhs.p.int_bounds.min.var_nr
			    || lhs.p.int_bounds.max.var_nr != rhs.p.int_bounds.max.var_nr) {
				//e  {size(a)..X} vs. {size(b)..Y} or {X..size(a)} vs. {Y..size(b)}
				//e Can't merge sizes of two different variables
				return cla_top();
			}
			//e If we are using variables, then the above is sufficient.
			//e If we aren't using variables, we need to update minimum and maximum:
			lhs.p.int_bounds.min.nr = MIN(lhs.p.int_bounds.min.nr,
						      rhs.p.int_bounds.min.nr);
			lhs.p.int_bounds.max.nr = MAX(lhs.p.int_bounds.max.nr,
						      rhs.p.int_bounds.max.nr);
			return lhs;


		default:
		fprintf(stderr, "%s: ?(%d)ERROR\n", __func__, lhs.vartype);
		exit(1);
		}
	}

	return cla_top();
}

/*e
 * Computes the classification for an expression
 *
 * @param sym The function within which we perform the analysis (needed to detect local variables)
 * @param locals Bindings for all local variables
 * @param node The node to analyse
 */
static classification_t
cla_expression(symtab_entry_t *sym, classification_t *locals, ast_node_t *node)
{
	switch (NODE_TY(node)) {
	case AST_VALUE_INT:
		return cla_int(AV_INT(node));

	case AST_VALUE_ID: {
		const int var = data_flow_is_local_var(sym, node);
		if (var >= 0) {
			return locals[var];
		}
		return cla_top();
	}

	case AST_NODE_ARRAYVAL: {
		int size = node->children[0]->children_nr;
		if (node->children[1]) {
			//e explicit size specification
			size = AV_INT(node->children[1]);
		}
		return cla_array(size);
	}

	case AST_NODE_METHODAPP: {
		//e are we observing array.size()?
		const int var = data_flow_is_local_var(sym, node->children[0]);
		if (var >= 0
		    && node->children[1]->sym->selector == symtab_selector_size) {
			return cla_sizeof(var);
		}
		return cla_top();
	}

	case AST_NODE_FUNAPP: {
		if (AST_CALLABLE_SYMREF(node)->symtab_flags & SYMTAB_BUILTIN) {
			const int args_nr = node->children[1]->children_nr;
			classification_t args[args_nr];
			//e prepare arguments
			for (int i = 0; i < args_nr; i++) {
				args[i] = cla_expression(sym, locals, node->children[1]->children[i]);
				//e Any bottom yields a bottom, ant top yields a top
				if (args[i].vartype == VARTYPE_BOT
				    || args[i].vartype == VARTYPE_TOP) {
					return args[i];
				}
			}

			//e symbolically interpret functions
			switch (AST_CALLABLE_SYMREF(node)->id) {
			case BUILTIN_OP_CONVERT:
				return args[0];
			case BUILTIN_OP_ADD:
				return cla_arithmetic(int_add, args[0], args[1],
						      /*e safety bound*/ 0x4000000000000000l);
			case BUILTIN_OP_SUB:
				return cla_arithmetic(int_sub, args[0], args[1],
						      /*e safety bound*/ 0x4000000000000000l);
			case BUILTIN_OP_MUL:
				return cla_arithmetic(int_mul, args[0], args[1],
						      0x40000000l);
			//e we don't support the others and fall through
			}
		}
	}
		// otherwise, fall through

	default:
		//e unknown/dangerous
		return cla_top();
	}
}

//================================================================================
//e Data Flow Anaysis

//e Data stored:
//e Array mapping local variables to their classifications

static void *
init(symtab_entry_t *sym, ast_node_t *node)
{
	return calloc(sizeof(classification_t), data_flow_number_of_locals(sym));
}

static void
print(FILE *file, symtab_entry_t *sym, void *pfact)
{
	if (!pfact) {
		fprintf(file, "NULL");
		return;
	}

	const int entries_nr = data_flow_number_of_locals(sym);
	symtab_entry_t *var_symbols[entries_nr];
	data_flow_get_all_locals(sym, var_symbols);

	classification_t* classifications = (classification_t *) pfact;

	bool printed_before = false;
	if (classifications) {
		for (int i = 0; i < entries_nr; ++i) {
			if (classifications[i].vartype != VARTYPE_BOT) {
				if (printed_before) {
					fprintf(file, "; ");
				} else {
					printed_before = true;
				}
				fprintf(file, "%s:", var_symbols[i]->name);
				cla_print(file, var_symbols, classifications + i);
			}
		}
	}
}

static void *
df_copy(symtab_entry_t *sym, void *fact)
{
	const int locals_nr = data_flow_number_of_locals(sym);
	size_t size = locals_nr * sizeof(classification_t);
	classification_t *cloned_classifications = calloc(size, 1);
	memcpy(cloned_classifications, fact, size);
	return cloned_classifications;
}

static void *
join(symtab_entry_t *sym, void *pin1, void *pin2)
{
	const int locals_nr = data_flow_number_of_locals(sym);
	classification_t *lhs = (classification_t *) pin1;
	classification_t *rhs = (classification_t *) pin2;
	classification_t *result = malloc(sizeof(classification_t) * locals_nr);
	for (int i = 0; i < locals_nr; i++) {
		result[i] = cla_join(lhs[i], rhs[i]);
	}
	return result;
}

static void *
transfer(symtab_entry_t *sym, ast_node_t *ast, void *pin)
{
	//const int locals_nr = data_flow_number_of_locals(sym);
	classification_t *classifications = (classification_t *) df_copy(sym, pin);
	
	switch (NODE_TY(ast)) {
	case AST_NODE_VARDECL:
	case AST_NODE_ASSIGN: {
		int var = data_flow_is_local_var(sym, ast->children[0]);
		if (var >= 0) {
			if (ast->children[1]) {
				//e obtain updated classification from assigned expression
				classifications[var] = cla_expression(sym, classifications, ast->children[1]);
			} else {
				classifications[var] = cla_top();
			}
		}
	}
	}

	return classifications;
}

static bool
is_less_than_or_equal(symtab_entry_t *sym, void *plhs, void *prhs)
{
	const int locals_nr = data_flow_number_of_locals(sym);
	classification_t *lhs = (classification_t *) plhs;
	classification_t *rhs = (classification_t *) prhs;

	for (int var = 0; var < locals_nr; var++) {
		if (!cla_less_than_or_equal(lhs + var, rhs + var)) {
			return false;
		}
	}
	return true;
}

static void
df_free(void *fact)
{
	free(fact);
}

static void
check_bounds_init(symtab_entry_t *sym, void **data)
{
}

static void
check_bounds_free(symtab_entry_t *sym, void **data)
{
}

static void
check_bounds(symtab_entry_t *sym, void *pfact, void **context, ast_node_t *node)
{
	const int locals_nr = data_flow_number_of_locals(sym);
	classification_t *classifiations = (classification_t *) pfact;
}

static data_flow_postprocessor_t postprocessor = {
	.init = check_bounds_init,
	.visit_node = check_bounds,
	.free = check_bounds_free
};

data_flow_analysis_t data_flow_analysis__out_of_bounds = {
	.forward = true,
	.name = "out-of-bounds-checking",
	.init = init,
	.print = print,
	.join = join,
	.transfer = transfer,
	.is_less_than_or_equal = is_less_than_or_equal,
	.free = df_free,
	.copy = df_copy,
	.postprocessor = &postprocessor
};
