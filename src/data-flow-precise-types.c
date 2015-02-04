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
#include "data-flow.h"
#include "class.h"

//#define DEBUG_PRECISE_CALLS

//================================================================================
//e Data Flow Anaysis

//e Data stored:
//e Array mapping local variables to their predicted class types, represented as symtab_entry_t*, or NULL for known non-objects

static symtab_entry_t _top;
static symtab_entry_t _bottom;

#define TOP (&_top)
#define BOTTOM (&_bottom)

static void *
init(symtab_entry_t *sym, ast_node_t *node)
{
	int count = data_flow_number_of_locals(sym);
	symtab_entry_t **specs = malloc(sizeof(symtab_entry_t *) * count);

	for (int i = 0; i < count; i++) {
		specs[i] = BOTTOM;
	}

	//e try to incorporate dynamic information, if available
	if (node == sym->astref && sym->dynamic_parameter_types) {
		//e method entry for which we've already collected dynamic data?
		ast_node_t *formals = sym->astref->children[1];
		
		for (int i = 0; i < sym->parameters_nr; i++) {
			ast_node_t *formal = formals->children[i]->children[0];
			int var = data_flow_is_local_var(sym, formal);
			assert(var >= 0);
			class_t *classref = sym->dynamic_parameter_types[i];
			if (classref == NULL) {
				specs[i] = NULL;
			} else if (classref == &class_top) {
				specs[i] = TOP;
			} else if (classref == &class_bottom) {
				specs[i] = BOTTOM;
			} else {
				specs[i] = classref->id;
			}
		}
	}
	return specs;
}

static void
print_type(FILE *f, symtab_entry_t *e)
{
	if (e == NULL) {
		fprintf(f, "-");
	} else if (e == TOP) {
		fprintf(f, "^T^");
	} else if (e == BOTTOM) {
		fprintf(f, "_|_");
	} else {
		symtab_entry_name_dump(f, e);
	}
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

	symtab_entry_t** types = (symtab_entry_t **) pfact;

	bool printed_before = false;
	if (types) {
		for (int i = 0; i < entries_nr; ++i) {
			if (types[i] != BOTTOM) {
				if (printed_before) {
					fprintf(file, "; ");
				} else {
					printed_before = true;
				}
				fprintf(file, "%s:", var_symbols[i]->name);
				print_type(file, types[i]);
			}
		}
	}
}

static void *
df_copy(symtab_entry_t *sym, void *fact)
{
	const int locals_nr = data_flow_number_of_locals(sym);
	size_t size = locals_nr * sizeof(symtab_entry_t *);
	symtab_entry_t **cloned_classifications = calloc(size, 1);
	memcpy(cloned_classifications, fact, size);
	return cloned_classifications;
}

static symtab_entry_t *
class_join(symtab_entry_t *lhs, symtab_entry_t *rhs)
{
	if (lhs == rhs) {
		return lhs;
	}
	if (lhs == BOTTOM) {
		return rhs;
	}
	if (rhs == BOTTOM) {
		return lhs;
	}
	return TOP;
}

static bool
class_less_than_or_equal(symtab_entry_t *lhs, symtab_entry_t *rhs)
{
	if (lhs == rhs) {
		return true;
	}
	if (lhs == BOTTOM) {
		return true;
	}
	if (rhs == TOP) {
		return true;
	}
	return false;
}

static void *
join(symtab_entry_t *sym, void *pin1, void *pin2)
{
	const int locals_nr = data_flow_number_of_locals(sym);
	symtab_entry_t **lhs = (symtab_entry_t **) pin1;
	symtab_entry_t **rhs = (symtab_entry_t **) pin2;
	symtab_entry_t **result = malloc(sizeof(symtab_entry_t **) * locals_nr);
	for (int i = 0; i < locals_nr; i++) {
		result[i] = class_join(lhs[i], rhs[i]);
	}
	return result;
}

static symtab_entry_t *
expression(symtab_entry_t *sym, symtab_entry_t **classifications, ast_node_t *node)
{
	switch (NODE_TY(node)) {
	case AST_VALUE_ID: {
		if (AV_ID(node) == BUILTIN_OP_SELF && sym->parent) {
			if (SYMTAB_KIND_CLASS == SYMTAB_KIND(sym->parent)) {
				symtab_entry_t *rv = sym->parent;
				assert(rv);
				return rv;
			}
		}
		int var = data_flow_is_local_var(sym, node);
		if (var >= 0) {
			return classifications[var];
		}
		return TOP;
	}

	case AST_VALUE_STRING:
		return class_string.id;

	case AST_VALUE_INT:
		return class_boxed_int.id;

	case AST_VALUE_REAL:
		return class_boxed_real.id;

	case AST_NODE_ARRAYVAL:
		return class_array.id;
			
	case AST_NODE_NEWINSTANCE: {
		symtab_entry_t *rv = node->children[0]->sym;
		assert(rv);
		return rv;
	}
		
	default:
		return TOP;
	}
}

static void *
transfer(symtab_entry_t *sym, ast_node_t *ast, void *pin)
{
	//const int locals_nr = data_flow_number_of_locals(sym);
	symtab_entry_t **classifications = (symtab_entry_t **) df_copy(sym, pin);
	
	switch (NODE_TY(ast)) {
	case AST_NODE_VARDECL:
	case AST_NODE_ASSIGN: {
		int var = data_flow_is_local_var(sym, ast->children[0]);
		if (var >= 0) {
			if (ast->children[1]) {
				//e obtain updated classification from assigned expression
				classifications[var] = expression(sym, classifications, ast->children[1]);
			} else {
				classifications[var] = TOP;
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
	symtab_entry_t **lhs = (symtab_entry_t **) plhs;
	symtab_entry_t **rhs = (symtab_entry_t **) prhs;

	for (int var = 0; var < locals_nr; var++) {
		if (!class_less_than_or_equal(lhs[var], rhs[var])) {
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
// Mark for optimisation

/*e
 * Record out-of-bounds access information
 */
static void
mark_types_recursively(symtab_entry_t *sym, symtab_entry_t **locals, ast_node_t *node)
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
			mark_types_recursively(sym, locals, node->children[i]);
		}
	}

	switch (NODE_TY(node)) {
	case AST_NODE_METHODAPP: {
		symtab_entry_t *receiver = expression(sym, locals, node->children[0]);
		if (receiver && receiver != TOP && receiver != BOTTOM) {
			//e found `real class'

			//e NB: running this ahead of time will not work too well, as methods won't generally have
			//e been compiled yet at that time.
			symtab_entry_t *sym = class_lookup_member(receiver, node->children[1]->sym->selector);
			if (sym && SYMTAB_KIND_FUNCTION == SYMTAB_KIND(sym)) {
#ifdef DEBUG_PRECISE_CALLS
				fprintf(stderr, "#opt replacing dynamic call by direct call to ");
				symtab_entry_name_dump(stderr, receiver);
				fprintf(stderr, ".");
				symtab_entry_name_dump(stderr, sym);
				fprintf(stderr, " at %p\n", sym->r_mem);
#endif
				node->children[1]->sym = sym;
			}
#ifdef DEBUG_PRECISE_CALLS
			else {
				fprintf(stderr, "#opt FAILED to replace dynamic call to ");
				symtab_entry_name_dump(stderr, receiver);
				fprintf(stderr, ".");
				symtab_entry_name_dump(stderr, node->children[1]->sym);
				fprintf(stderr, "[%d]", node->children[1]->sym->selector);
				fprintf(stderr, "because %p %x\n", sym, sym?sym->symtab_flags:0);
			}
#endif
		}
	}
	}
}

static void
mark_types(symtab_entry_t *sym, void *pfact, void **context, ast_node_t *node)
{
	//const int locals_nr = data_flow_number_of_locals(sym);
	symtab_entry_t **classifications = (symtab_entry_t **) pfact;
	mark_types_recursively(sym, classifications, node);
}

static data_flow_postprocessor_t postprocessor = {
	.init = NULL,
	.visit_node = mark_types,
	.free = NULL
};

data_flow_analysis_t data_flow_analysis__precise_types = {
	.forward = true,
	.name = "precise-types",
	.init = init,
	.print = print,
	.join = join,
	.transfer = transfer,
	.is_less_than_or_equal = is_less_than_or_equal,
	.free = df_free,
	.copy = df_copy,
	.postprocessor = &postprocessor
};
