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
#include "bitvector.h"


static void *
init(symtab_entry_t *sym, ast_node_t *node)
{
	return BITVECTOR_TO_POINTER(bitvector_alloc_on(data_flow_number_of_locals(sym)));
}

static void
print(FILE *file, symtab_entry_t *sym, void *pfact)
{
	if (!pfact) {
		fprintf(file, "NULL");
		return;
	}
	bitvector_t fact = BITVECTOR_FROM_POINTER(pfact);

	int entries_nr = data_flow_number_of_locals(sym);
	symtab_entry_t *var_symbols[entries_nr];
	data_flow_get_all_locals(sym, var_symbols);

	bool printed_before = false;
	for (int i = 0; i < entries_nr; ++i) {
		if (BITVECTOR_IS_SET(fact, i)) {
			if (printed_before) {
				fprintf(file, ",");
			} else {
				printed_before = true;
			}
			if (var_symbols[i]) {
				fprintf(file, "%s", var_symbols[i]->name);
			} else {
				fprintf(file, "?%d", i);
			}
		}
	}
}

static void *
join(symtab_entry_t *sym, void *pin1, void *pin2)
{
	bitvector_t in1 = BITVECTOR_FROM_POINTER(pin1);
	bitvector_t in2 = BITVECTOR_FROM_POINTER(pin2);
	bitvector_t result = bitvector_and(in1, in2);
	return BITVECTOR_TO_POINTER(result);
}

static void *
df_copy(symtab_entry_t *sym, void *fact)
{
	return BITVECTOR_TO_POINTER(bitvector_clone(BITVECTOR_FROM_POINTER(fact)));
}

static void *
transfer(symtab_entry_t *sym, ast_node_t *ast, void *pin)
{
	bitvector_t in = BITVECTOR_FROM_POINTER(pin);
	bitvector_t result = bitvector_clone(in);
	
	switch (NODE_TY(ast)) {
	case AST_NODE_VARDECL:
	case AST_NODE_ASSIGN: {
		int var = data_flow_is_local_var(sym, ast->children[0]);
		if (var >= 0) {
			if (ast->children[1]) {
				result = BITVECTOR_SET(result, var);
			} else {
				result = BITVECTOR_CLEAR(result, var);
			}
		}
	}
	}

	return BITVECTOR_TO_POINTER(result);
}

static bool
is_less_than_or_equal(symtab_entry_t *sym, void *plhs, void *prhs)
{
	bitvector_t lhs = BITVECTOR_FROM_POINTER(plhs);
	bitvector_t rhs = BITVECTOR_FROM_POINTER(prhs);
	return bitvector_is_subset_eq(rhs, lhs);
}

static void
df_free(void *fact)
{
	BITVECTOR_FREE(BITVECTOR_FROM_POINTER(fact));
}

//e check that we are not reading from an unassigned variable
static void
recursive_check_definite_assignment(symtab_entry_t *sym, bitvector_t assigned_vars, bitvector_t *reported_vars, ast_node_t *node)
{
	if (!node) {
		return;
	}
	int var = data_flow_is_local_var(sym, node);
	if (var >= 0 && !BITVECTOR_IS_SET(assigned_vars, var)
	    && !BITVECTOR_IS_SET(*reported_vars, var)) { //e skip previously reported ones
		int entries_nr = data_flow_number_of_locals(sym);
		symtab_entry_t *var_symbols[entries_nr];
		data_flow_get_all_locals(sym, var_symbols);

		data_flow_error(node, "Variable may be uninitialised: `%s'\n", var_symbols[var]->name);
		*reported_vars = BITVECTOR_SET((*reported_vars), var);
	}
	
	if (!IS_VALUE_NODE(node)) {
		for (int i = 0; i < node->children_nr; i++) {
			recursive_check_definite_assignment(sym, assigned_vars, reported_vars, node->children[i]);
		}
	}
}

static void
check_assignment_init(symtab_entry_t *sym, void **data)
{
	//e we use this bitvector of reported variables to suppress multiple reports
	*data = BITVECTOR_TO_POINTER(bitvector_alloc(data_flow_number_of_locals(sym)));
}

static void
check_assignment_free(symtab_entry_t *sym, void **data)
{
	BITVECTOR_FREE(BITVECTOR_FROM_POINTER(*data));
}

static void
check_assignment(symtab_entry_t *sym, void *pfact, void **context, ast_node_t *node)
{
	bitvector_t fact = BITVECTOR_FROM_POINTER(pfact);
	bitvector_t *reported_variables = (bitvector_t *) context;
	
	if (!node) {
		return;
	}

	//e We temporarily NULL child nodes that are handled by the graph, to avoid recursion along graph edges
	//e (TODO: in the future, modify the AST permanently to make these easier to track!)
	int *subs_indices;
	int subs_nr = cfg_subnodes(node, &subs_indices);
	if (subs_nr < 0) {
		// `fake' node
		return;
	}
	ast_node_t *subs[subs_nr];
	for (int i = 0; i < subs_nr; ++i) {
		subs[i] = node->children[subs_indices[i]];
		node->children[subs_indices[i]] = NULL;
	}
	
	switch (NODE_TY(node)) {
	//e for assignments/initialisations: only check things that we are reading, not things that we're assigning
	case AST_NODE_FUNDEF:
		break;
	case AST_NODE_VARDECL:
	case AST_NODE_ASSIGN: {
		recursive_check_definite_assignment(sym, fact, reported_variables, node->children[1]);
		int var = data_flow_is_local_var(sym, node->children[0]);
		if (var < 0) {
			recursive_check_definite_assignment(sym, fact, reported_variables, node->children[0]);
		}
		break;
	}
	default:
		recursive_check_definite_assignment(sym, fact, reported_variables, node);
	}

	//e And now we restore them again
	for (int i = 0; i < subs_nr; ++i) {
		node->children[subs_indices[i]] = subs[i];
	}
}

static data_flow_postprocessor_t postprocessor = {
	.init = check_assignment_init,
	.visit_node = check_assignment,
	.free = check_assignment_free
};

data_flow_analysis_t data_flow_analysis__definite_assignments = {
	.forward = true,
	.name = "definite-assignments",
	.init = init,
	.print = print,
	.join = join,
	.transfer = transfer,
	.is_less_than_or_equal = is_less_than_or_equal,
	.free = df_free,
	.copy = df_copy,
	.postprocessor = &postprocessor
};
