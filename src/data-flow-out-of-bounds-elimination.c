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
#include "symint.h"
#include "data-flow.h"
#include "bitvector.h"

//e negative `abs()':  guaranteed to avoid overflow
#define NABS(x) (((x) > 0) ? -(x) : (x))

//================================================================================
//e Classification lattice implementation

//e Classification
//e `vartype' designates the coarse type
typedef struct {
	union { /*e union: use EITHER interval OR array_info */
		struct { /*e with VARTYPE_INT only */
			symint_t min, max;
		} interval;

		int array_size; /*e with VARTYPE_ARRAY only */
	} p;
	unsigned char vartype; /*e One of VARTYPE_* */
	bool synthetic;
} classification_t;

#define VARTYPE_BOT	0
#define VARTYPE_INT	1
#define VARTYPE_ARRAY	2
#define VARTYPE_TOP	3

#define INT_TOP 0x80000000	/*e unknown array size */

static bool
cla_is_synthetic(symint_t arg1, symint_t arg2, symint_t result, bool synthetic1, bool synthetic2)
{
	if (symint_is_top(result)) {
		return false;
	}
	if (symint_equals(result, arg1) && !synthetic1) {
		return false;
	}
	if (symint_equals(result, arg2) && !synthetic2) {
		return false;
	}
	return synthetic1 | synthetic2;
}
			
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
cla_print(FILE *file, symtab_entry_t **var_symbols, classification_t classification)
{
	switch (classification.vartype) {
	case VARTYPE_BOT:
		fprintf(file, "_");
		break;
	case VARTYPE_TOP:
		fprintf(file, "T");
		break;
	case VARTYPE_ARRAY:
		fprintf(file, "a[/");
		cla_array_bound_print(file, classification.p.array_size);
		fprintf(file, "]");
		break;
	case VARTYPE_INT:
		fprintf(file, "{");
		symint_print(file, var_symbols, "sizeof(%s)", classification.p.interval.min);
		fprintf(file, "..");
		symint_print(file, var_symbols, "sizeof(%s)", classification.p.interval.max);
		fprintf(file, "}");
		if (classification.synthetic) {
			fprintf(file, ".s");
		}
		break;
	default:
		fprintf(file, "?(%d)ERROR\n", classification.vartype);
	}
}

/*e
 * Determines whether lhs <= rhs in the lattice, i.e., whether `lhs' describes a more precise element of the lattice
 *
 * @param lhs, rhs: The two classification lattice elements to compare
 * @return true iff lhs <= rhs
 */
static bool
cla_less_than_or_equal(classification_t lhs, classification_t rhs)
{
	//e trivial top/bottom element occurrence?
	if (lhs.vartype == VARTYPE_BOT
	    || rhs.vartype == VARTYPE_TOP) {
		return true;
	}

	//e not comparable?
	if (lhs.vartype != rhs.vartype) {
		return false;
	}

	switch (lhs.vartype) {

	case VARTYPE_ARRAY:
		if (lhs.p.array_size == INT_TOP) {
			return true; //e unknown array size is less concrete than known
		}
		return lhs.p.array_size == rhs.p.array_size;

	case VARTYPE_INT:
		//e more precise if lhs interval is contained in rhs interval
		return (symint_less_than_or_equal_numerically(rhs.p.interval.min, lhs.p.interval.min)
			|| symint_is_top(lhs.p.interval.min))
			&& ((symint_less_than_or_equal_numerically(lhs.p.interval.max, rhs.p.interval.max)
			     || symint_is_top(rhs.p.interval.max)));

	default:
		fprintf(stderr, "%s: ?(%d)ERROR\n", __func__, lhs.vartype);
		exit(1);
	}
}

/*e
 * Constructs a classification, given only its type.  May need further initialisation (for ARRAY and INT)
 */
static classification_t
cla_init(int type)
{
	return (classification_t) { .vartype = type, .synthetic = false };
}

/*e
 * Constructs a lattice TOP element
 */
static classification_t
cla_top()
{
	return cla_init(VARTYPE_TOP);
}

/*e
 * Constructs a lattice BOTTOM element
 */
static classification_t
cla_bottom()
{
	return cla_init(VARTYPE_BOT);
}

/*e
 * Constructs a literal integer element of the lattice
 */
static classification_t
cla_int(int i)
{
	classification_t classification = cla_init(VARTYPE_INT);
	classification.p.interval.max = symint_literal(i);
	classification.p.interval.min = classification.p.interval.max;
	return classification;
}

/*e
 * Constructs an array of given size for the lattice
 *
 * @param Array size, if known; otherwise INT_TOP
 * @return A suitable array classification
 */
static classification_t
cla_array(int size)
{
	classification_t classification = cla_init(VARTYPE_ARRAY);
	classification.p.array_size = size;
	return classification;
}

/*e
 * Constructs an integer representing `sizeof(v)' for some local variable `v'
 *
 * @param var_nr Local variable index (>= 0) to represent
 * @return A suitable classification representation
 */
static classification_t
cla_sizeof(int var_nr)
{
	classification_t classification = cla_init(VARTYPE_INT);
	classification.p.interval.max = symint_var(var_nr);
	classification.p.interval.min = classification.p.interval.max;
	return classification;
}

/*e
 * Negates a classification (computes -classification) arithmetically
 */
static classification_t
cla_negate(classification_t classification)
{
	if (classification.vartype == VARTYPE_INT) {
		symint_t min_backup = classification.p.interval.min;
		classification.p.interval.min = symint_negate(classification.p.interval.max);
		classification.p.interval.max = symint_negate(min_backup);
		return classification;
	}
	return cla_top();
}

/*e
 * Symbolically adds two classifications
 *
 * @param arg1, arg2: The two classifications to sum up
 * @return The sum of the two classifications
 */
static classification_t
cla_add(classification_t arg1, classification_t arg2)
{
	const long int safety_bound = 0x4000000000000000l;
	//e integer arithmetic only makes sense with literal ints:
	if (arg1.vartype != VARTYPE_INT
	    || arg2.vartype != VARTYPE_INT
	    || symint_is_var(arg1.p.interval.min)
	    || symint_is_var(arg1.p.interval.max)
	    || symint_is_var(arg2.p.interval.min)
	    || symint_is_var(arg2.p.interval.max)
	    //e also guard against overflow:
	    //e We compute the negative abs because two's complement permits one more negative
	    //e value than positive values
	    || (NABS(symint_literal_value(arg1.p.interval.min)) < -safety_bound)
	    || (NABS(symint_literal_value(arg1.p.interval.max)) < -safety_bound)
	    || (NABS(symint_literal_value(arg2.p.interval.min)) < -safety_bound)
	    || (NABS(symint_literal_value(arg2.p.interval.max)) < -safety_bound)) {
		return cla_top();
	}
	classification_t result = cla_init(VARTYPE_INT);
	result.synthetic = true;
	if (symint_is_top(arg1.p.interval.min)
	    || symint_is_top(arg2.p.interval.min)) {
		//e if either is unbounded, the result is unbounded:
		result.p.interval.min = symint_top();
	} else {
		result.p.interval.min = symint_literal(symint_literal_value(arg1.p.interval.min) + symint_literal_value(arg2.p.interval.min));
	}
	if (symint_is_top(arg1.p.interval.max)
	    || symint_is_top(arg2.p.interval.max)) {
		//e if either is unbounded, the result is unbounded:
		result.p.interval.max = symint_top();
	} else {
		result.p.interval.max = symint_literal(symint_literal_value(arg1.p.interval.max) + symint_literal_value(arg2.p.interval.max));
	}
	return result;
}

/*e
 * Lattice-join for two classifications
 */
static classification_t
cla_join(classification_t arg1, classification_t arg2)
{
	if (arg1.vartype == VARTYPE_BOT) {
		return arg2;
	}
	if (arg2.vartype == VARTYPE_BOT) {
		return arg1;
	}
	if (arg1.vartype != VARTYPE_TOP
	    && arg2.vartype != VARTYPE_TOP
	    && arg1.vartype == arg2.vartype) {
		switch (arg1.vartype) {
		case VARTYPE_ARRAY:
			if (arg1.p.array_size == arg2.p.array_size) {
				return arg1;
			}
			//e unknown or inconsistent array size
			arg1.p.array_size = INT_TOP;
			return arg1;

		case VARTYPE_INT: {
			classification_t result = cla_init(VARTYPE_INT);
			result.p.interval.min = symint_min(arg1.p.interval.min,
							     arg2.p.interval.min);
			result.p.interval.max = symint_max(arg1.p.interval.max,
							     arg2.p.interval.max);

			result.synthetic = false;

			//e We now bound the analysis lattice height.
			//e The easiest solution would be to immediately discard arithmetic results, but this
			//e is very imprecise.  An alternative is to depth-bound joins with arithmetic results,
			//e which we here do, with a depth of 1 (indicated by `synthetic').

			//e If we're merging a synthetic result that extends our min bound, then
			//e assume that the min bound can grow infinitely low:
			if ((arg1.synthetic
			     && !symint_less_than_or_equal_numerically(arg2.p.interval.min, arg1.p.interval.min))
			    || (arg2.synthetic
				&& !symint_less_than_or_equal_numerically(arg1.p.interval.min, arg2.p.interval.min))) {

				result.p.interval.min = symint_top();
			}
			//e analogously for the max bound:
			if ((arg1.synthetic
			     && !symint_less_than_or_equal_numerically(arg1.p.interval.max, arg2.p.interval.max))
			    || (arg2.synthetic
				&& !symint_less_than_or_equal_numerically(arg2.p.interval.max, arg1.p.interval.max))) {

				result.p.interval.max = symint_top();
			}

			result.synthetic = cla_is_synthetic(arg1.p.interval.min,
							    arg2.p.interval.min,
							    result.p.interval.min,
							    arg1.synthetic, arg2.synthetic);
			result.synthetic |= cla_is_synthetic(arg1.p.interval.max,
							     arg2.p.interval.max,
							     result.p.interval.max,
							     arg1.synthetic, arg2.synthetic);

			//e Replace {T..T} by T for simplicity
			if (symint_is_top(result.p.interval.min)
			    && symint_is_top(result.p.interval.max)) {
				return cla_top();
			}
			return result;
		}


		default:
		fprintf(stderr, "%s: ?(%d)ERROR\n", __func__, arg1.vartype);
		exit(1);
		}
	}

	return cla_top();
}

/*e
 * Computes the classification for an expression
 *
 * This function effectively performs abstract interpretation over the expression.  It serves
 * as transfer function interpreter.
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
				return cla_add(args[0], args[1]);
			case BUILTIN_OP_SUB:
				return cla_add(args[0], cla_negate(args[1]));
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
				cla_print(file, var_symbols, classifications[i]);
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
		/*
		if (result[i].vartype) {
			fprintf(stderr, "[> JOIN <] ");
			cla_print(stderr, NULL, lhs[i]);
			fprintf(stderr, " \\/ ");
			cla_print(stderr, NULL, rhs[i]);
			fprintf(stderr, " => ");
			cla_print(stderr, NULL, result[i]);
			fprintf(stderr, "\n");
		}
		*/
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
		if (!cla_less_than_or_equal(lhs[var], rhs[var])) {
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

// --------------------------------------------------------------------------------
// actually removing nodes

static int type_checks_eliminated;
static int lower_bounds_checks_eliminated;
static int upper_bounds_checks_eliminated;
static int array_ops_count;

/*e
 * Record out-of-bounds access information
 */
static void
annotate_bounds(symtab_entry_t *sym, classification_t *locals, ast_node_t *node)
{
	if (!node || IS_VALUE_NODE(node)) {
		//e we only annotate array operations
		return;
	}

	//e CFG split magic:  The CFG splits up the AST into multiple CFG nodes.  Thus, we can't
	//e just iterate over the entire AST fragment that we get-- we might run into CFG sub-nodes.
	int *cfg_subnodes_indices;
	int cfg_subnodes_indices_nr = cfg_subnodes(node, &cfg_subnodes_indices);
	if (cfg_subnodes_indices_nr < 0) {
		return; //e This node is fully split up into multiple CFG nodes; we do nothing here
	}
	int cfg_subnodes_index_counter = 0;

	for (int i = 0; i < node->children_nr; i++) {
		if (cfg_subnodes_index_counter < cfg_subnodes_indices_nr
		    && i == cfg_subnodes_indices[cfg_subnodes_index_counter]) {
			//e Skip this one; it's the root of another CFG node
			++cfg_subnodes_index_counter;
		} else {
			//e recurse
			annotate_bounds(sym, locals, node->children[i]);
		}
	}

	switch (NODE_TY(node)) {
	case AST_NODE_ARRAYSUB: {
		++array_ops_count;
		const int array_var = data_flow_is_local_var(sym, node->children[0]);
		classification_t array_info = cla_expression(sym, locals, node->children[0]);
		classification_t index_info = cla_expression(sym, locals, node->children[1]);
		if (array_info.vartype == VARTYPE_ARRAY) {
			//e We can skip the `is-this-an-array' type check
			node->opt_flags |= OPT_FLAG_NO_TYPECHECK1;
			//fprintf(stderr, "type check eliminated on: "); AST_DUMP(node);
			++type_checks_eliminated;
		}
		if (index_info.vartype == VARTYPE_INT) {
			if (symint_less_than_or_equal_numerically(symint_literal(0),
								  index_info.p.interval.min)) {
				node->opt_flags |= OPT_FLAG_NO_LOWER;
				++lower_bounds_checks_eliminated;
				//fprintf(stderr, "lower bound check eliminated on: "); AST_DUMP(node);
			}

			bool eliminate_upper = false;

			//e There are two ways to eliminate the upper bound:
			//e (a) explicitly known array size
			//e (b) reference to array.size()

			//e Case (a):
			if (array_info.vartype == VARTYPE_ARRAY
			    && array_info.p.array_size != INT_TOP) {
				//e Know explicit array size!
				if (symint_less_than_or_equal_numerically(index_info.p.interval.max,
									  symint_literal(array_info.p.array_size - 1))) {
					eliminate_upper = true;
				}
			}

			//e Case (b):
			if (array_var > 0) {
				//e The upper bound for array size, size(array_var)
				symint_t upper_bound = symint_var(array_var);

				// FIXME:
				if (symint_less_than_or_equal_numerically(index_info.p.interval.max,
									  upper_bound)) {
					// FIXME: disabled:
					// eliminate_upper = true;
				}
			}

			if (eliminate_upper) {
				node->opt_flags |= OPT_FLAG_NO_UPPER;
				++upper_bounds_checks_eliminated;
				//fprintf(stderr, "upper bound check eliminated on: "); AST_DUMP(node);
			}

		}
	}
	}
}

static void
check_bounds_init(symtab_entry_t *sym, void **data)
{
	type_checks_eliminated = 0;
	lower_bounds_checks_eliminated = 0;
	upper_bounds_checks_eliminated = 0;
	array_ops_count = 0;
}

static void
check_bounds(symtab_entry_t *sym, void *pfact, void **context, ast_node_t *node)
{
	//const int locals_nr = data_flow_number_of_locals(sym);
	classification_t *classifications = (classification_t *) pfact;
	annotate_bounds(sym, classifications, node);
}

static void
check_bounds_free(symtab_entry_t *sym, void **data)
{
	/*
	symtab_entry_name_dump(stderr, sym);
	fprintf(stderr, ": eliminated checks:  %d array-type, %d lower, %d upper  (out of %d)\n",
		type_checks_eliminated, lower_bounds_checks_eliminated, upper_bounds_checks_eliminated, array_ops_count);
	*/
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
