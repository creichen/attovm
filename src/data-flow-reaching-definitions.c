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

#include <assert.h>
#include <string.h>

#include "ast.h"
#include "symbol-table.h"
#include "data-flow.h"

/*e
 * ----------------------------------------------------------------------
 * Reaching Definitions Analysis
 *
 * This implementation stores results as an array mapping variable offsets
 * to unique ast_node_t expressions that represent them, or NULL if no
 * assignment was found, or AST_TOP if conflicting assignments were found.
 * In other words, this is the lattice:
 *
 *                     &AST_TOP
 *                   /          \
 *             actual expressions in AST
 *                   \          /
 *                       NULL
 * ----------------------------------------------------------------------
 */

static ast_node_t AST_TOP; /*e `fake' AST node that serves as `top' element of AST node lattice */

static ast_node_t *TOP = &AST_TOP;
static ast_node_t *BOTTOM = NULL;


/*e
 * Determines the total number of stack-dynamic variables for the given function/constructor/method
 */
static int
dflow_number_of_locals(symtab_entry_t *sym)
{
	return sym->storage.vars_nr + sym->parameters_nr;
}

/*e
 * Checks if the AST represents a local variable or parameter.
 * Returns a local variable index in [0 .. number_of_locals - 1]
 * if so, or -1 otherwise.
 *
 * @param sym The symbol to examine
 * @param ast The AST node containing the presumed variable reference
 */
static int
dflow_is_local(symtab_entry_t *sym, ast_node_t *ast)
{
	if (sym->symtab_flags & SYMTAB_MAIN_ENTRY_POINT) {
		//e main function has no locals (all variables can be modified by subroutines,
		//e which would break intra-procedural analysis)
		return -1;
	}
	
	switch (NODE_TY(ast)) {
	case AST_VALUE_ID:
		if (SYMTAB_IS_STACK_DYNAMIC(ast->sym)) {
			if (ast->sym->symtab_flags & SYMTAB_PARAM) {
				return ast->sym->offset;
			} else {
				return sym->parameters_nr + ast->sym->offset;
			}
		}
	default:
		return -1;
	}
}


static void
dflow_get_all_locals_internal(symtab_entry_t *sym, symtab_entry_t **locals, ast_node_t *ast)
{
	if (!ast) {
		return;
	}
	
	int var_nr = dflow_is_local(sym, ast);
	if (var_nr >= 0) {
		locals[var_nr] = ast->sym;
	}

	if (!IS_VALUE_NODE(ast)) {
		for (int i = 0; i < ast->children_nr; ++i) {
			dflow_get_all_locals_internal(sym, locals, ast->children[i]);
		}
	}
}

/*e
 * Looks up all parameters and local variables defined in `sym' and places them in the array `locals'
 *
 * VERY inefficient; only use this for debugging/pretty-printing.
 *
 * @param sym The symbol to examine
 * @param locals Pointer to array of locals to write to
 */
static void
dflow_get_all_locals(symtab_entry_t *sym, symtab_entry_t **locals)
{
	assert(sym->astref); /*e only for callables (functions/methods/constructors) */
	dflow_get_all_locals_internal(sym, locals, sym->astref);
}



static void *
init(symtab_entry_t *sym, ast_node_t *node)
{
	//e Array with NULL (i.e., BOTTOM)
	return calloc(sizeof(ast_node_t *), dflow_number_of_locals(sym));
}

static void
print(FILE *file, symtab_entry_t *sym, void *data)
{
	if (!data) {
		fprintf(file, "NULL");
		return;
	}

	ast_node_t **vars = (ast_node_t **) data;

	int entries_nr = dflow_number_of_locals(sym);
	symtab_entry_t *var_symbols[entries_nr];
	
	dflow_get_all_locals(sym, var_symbols);

	bool printed_before = false;
	if (data) {
		for (int i = 0; i < entries_nr; ++i) {
			if (vars[i]) {
				if (printed_before) {
					fprintf(file, "; ");
				} else {
					printed_before = true;
				}
				fprintf(file, "%s:", var_symbols[i]->name);
				if (vars[i] == TOP) {
					fprintf(file, "T");
				} else {
					ast_node_print(file, vars[i], true);
				}
			}
		}
	}
}

static void *
join(symtab_entry_t *sym, void *in1, void *in2)
{
	ast_node_t **vars1 = (ast_node_t **) in1;
	ast_node_t **vars2 = (ast_node_t **) in2;
	int entries_nr = dflow_number_of_locals(sym);
	ast_node_t **results = calloc(sizeof(ast_node_t *), entries_nr);
	
	for (int i = 0; i < entries_nr; i++) {
		if (vars1[i]) {
			results[i] = vars1[i];
			if (vars2[i] && vars1[i] != vars2[i]) {
				results[i] = TOP; // disagreement
			}
		} else {
			results[i] = vars2[i];
		}
	}
	return results;
}

static void *
df_copy(symtab_entry_t *sym, void *data)
{
	int entries_nr = dflow_number_of_locals(sym);
	ast_node_t **results = malloc(sizeof(ast_node_t *) * entries_nr);
	memcpy(results, data, entries_nr * sizeof(ast_node_t *));
	return results;
}

static void *
transfer(symtab_entry_t *sym, ast_node_t *ast, void *in)
{
	//e copy `in' into results
	ast_node_t **results = df_copy(sym, in);

	int var = -1;
	ast_node_t *expression;
	
	switch (NODE_TY(ast)) {
	case AST_NODE_VARDECL:
	case AST_NODE_ASSIGN:
		var = dflow_is_local(sym, ast->children[0]);
		expression = ast->children[1];
	}

	if (var >= 0) {
		//e we observed an assignment: model assignment/update
		results[var] = expression;
	}

	return results;
}

static bool
is_less_than_or_equal(symtab_entry_t *sym, void *plhs, void *prhs)
{
	ast_node_t **lhs = (ast_node_t **) plhs;
	ast_node_t **rhs = (ast_node_t **) prhs;
	int entries_nr = dflow_number_of_locals(sym);
	
	for (int i = 0; i < entries_nr; i++) {
		if (lhs[i] == BOTTOM || lhs[i] == rhs[i] || rhs[i] == TOP) {
			continue;
		} else {
			return false;
		}
	}
	return true;
}

static void
df_free(void *data)
{
	free(data);
}

data_flow_analysis_t data_flow_analysis__reaching_definitions = {
	.forward = true,
	.name = "reaching-definitions",
	.init = init,
	.print = print,
	.join = join,
	.transfer = transfer,
	.is_less_than_or_equal = is_less_than_or_equal,
	.free = df_free,
	.copy = df_copy,
	.postprocessor = NULL
};
