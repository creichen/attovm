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
#include <stdarg.h>
#include <string.h>

#include "ast.h"
#include "cstack.h"
#include "chash.h"
#include "control-flow-graph.h"
#include "symbol-table.h"
#include "data-flow.h"

extern data_flow_analysis_t data_flow_analysis__reaching_definitions;
extern data_flow_analysis_t data_flow_analysis__definite_assignments;

data_flow_analysis_t *data_flow_analyses_correctness[] = {
	&data_flow_analysis__definite_assignments,
	NULL /*e terminator: must be final entry! */
};
data_flow_analysis_t *data_flow_analyses_optimisation[] = {
	&data_flow_analysis__reaching_definitions,
	NULL /*e terminator: must be final entry! */
};

static data_flow_analysis_t **analysis_sets[] = {
	data_flow_analyses_correctness,
	data_flow_analyses_optimisation,
	NULL
};


static int error_count;

void
data_flow_error(const ast_node_t *node, char *fmt, ...)
{
	// Variable Anzahl von Parametern
	va_list args;
	fprintf(stderr, "Error in line %d: ", node->source_line);
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
	fprintf(stderr, "\n");
	++error_count;
}

typedef struct {
	cfg_node_t *from;
	cfg_edge_t *to;
} edge_t;


typedef struct {
	cfg_node_t *init, *exit;
	cstack_t *worklist;
	symtab_entry_t *sym;
	int analysis_index;
	data_flow_analysis_t *analysis;
} context_t;

static void
fprintf_stderr(char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
	fprintf(stderr, "\n");
}

#define ADEBUG(s) if (ctx->analysis->debug) { fprintf(stderr, "[%s:", ctx->analysis->name); symtab_entry_name_dump(stderr, ctx->sym); fprintf(stderr, "] "); fprintf_stderr s; }

static void
dflow_init_node(context_t *ctx, bool edge_direction, cfg_node_t *cfg_node, hashset_ptr_t *set)
{
	//e avoid multiple iterations over the same node
	if (hashset_ptr_contains(set, cfg_node)) {
		return;
	}
	hashset_ptr_add(set, cfg_node);

	//e initialise both in and out edges
	void *datum = NULL;
	if (ctx->analysis->init) {
		datum = ctx->analysis->init(ctx->sym, cfg_node->ast);
	}
	cfg_node->analysis[ctx->analysis_index].inb = datum;
	if (cfg_node->ast) {
		cfg_node->analysis[ctx->analysis_index].outb = ctx->analysis->transfer(ctx->sym, cfg_node->ast, datum);
	} else {
		cfg_node->analysis[ctx->analysis_index].outb = ctx->analysis->copy(ctx->sym, cfg_node->analysis[ctx->analysis_index].inb);
	}
	if (ctx->analysis->debug) {
		ADEBUG(("Initialise node <%p> :", cfg_node));
		fprintf(stderr, "\t\t\t\tin:\t");
		ctx->analysis->print(stderr, ctx->sym, cfg_node->analysis[ctx->analysis_index].inb);
		fprintf(stderr, "\n\t\t\t\tbody:\t");
		if (cfg_node->ast) {
			ast_node_print(stderr, cfg_node->ast, true);
		}
		fprintf(stderr, "\n\t\t\t\tout:\t");
		ctx->analysis->print(stderr, ctx->sym, cfg_node->analysis[ctx->analysis_index].outb);
		fprintf(stderr, "\n");
	}

	for (int k = 0; k < 2; k++) {
		bool current_edge_direction = k > 0;
		for (int i = 0; i < cfg_edges_nr(cfg_node, current_edge_direction); i++) {
			cfg_edge_t *outedge = cfg_edge(cfg_node, i, current_edge_direction);
			if (edge_direction == current_edge_direction) {
				edge_t edge = { .from = cfg_node, .to = outedge };
				stack_push(ctx->worklist, &edge);
			}
			dflow_init_node(ctx, edge_direction, outedge->node, set);
		}
	}
}

static void
dflow_postprocess_node(context_t *ctx, cfg_node_t *cfg_node, void *data, hashset_ptr_t *set)
{
	//e avoid multiple iterations over the same node
	if (hashset_ptr_contains(set, cfg_node)) {
		return;
	}
	hashset_ptr_add(set, cfg_node);

	ctx->analysis->postprocessor->visit_node(ctx->sym,
						 cfg_node->analysis[ctx->analysis_index].inb,
						 data, cfg_node->ast);

	for (int k = 0; k < 2; k++) {
		bool edge_direction = k > 0;
		for (int i = 0; i < cfg_edges_nr(cfg_node, edge_direction); i++) {
			cfg_edge_t *outedge = cfg_edge(cfg_node, i, edge_direction);
			dflow_postprocess_node(ctx, outedge->node, data, set);
		}
	}
}


static void
dflow_analyse(context_t *ctx)
{
	ADEBUG(("---------------------------------------- Debugging enabled ----------------------------------------"));

	bool edge_direction = ctx->analysis->forward ? CFG_OUT : CFG_IN;

	ctx->worklist = stack_alloc(sizeof(edge_t), 128);
	if (!ctx->init) {
		fprintf(stderr, "No init node for:\n");
		symtab_entry_dump(stderr, ctx->sym);
	}
	assert(ctx->init);

	hashset_ptr_t *set = hashset_ptr_alloc();
	dflow_init_node(ctx, edge_direction, ctx->init, set);
	hashset_ptr_free(set);

	ADEBUG(("---------------------------------------- Initialisation completed ----------------------------------------"));

	while (stack_size(ctx->worklist)) {
		edge_t *edge = stack_pop(ctx->worklist);
		cfg_node_t *from_node = edge->from;
		cfg_node_t *to_node = edge->to->node;
		void *from_outb = from_node->analysis[ctx->analysis_index].outb;

		cfg_data_flow_facts_t *to_node_facts = &(to_node->analysis[ctx->analysis_index]);
		ADEBUG(("----------------------------------------"));
		ADEBUG(("Examining edge: <%p> -> <%p>", edge->from, edge->to->node));

		if (!ctx->analysis->is_less_than_or_equal(ctx->sym, from_outb, to_node_facts->inb)) {
			ADEBUG(("\t => Updating <%p> :", edge->to->node));
			void *new_in_fact = ctx->analysis->join(ctx->sym, from_outb, to_node_facts->inb);
			if (ctx->analysis->debug) {
				fprintf(stderr, "\t\t\t\told in :\t");
				ctx->analysis->print(stderr, ctx->sym, to_node_facts->inb);
				fprintf(stderr, "\n");
				fprintf(stderr, "\t\t\t\tnew in :\t");
				ctx->analysis->print(stderr, ctx->sym, new_in_fact);
				fprintf(stderr, "\n");
			}
			
			ctx->analysis->free(to_node_facts->inb);
			to_node_facts->inb = new_in_fact;
			void *new_out_fact = ctx->analysis->transfer(ctx->sym, to_node->ast, new_in_fact);

			if (ctx->analysis->debug) {
				fprintf(stderr, "\t\t\t\told out:\t");
				ctx->analysis->print(stderr, ctx->sym, to_node_facts->outb);
				fprintf(stderr, "\n");
				fprintf(stderr, "\t\t\t\tnew out:\t");
				ctx->analysis->print(stderr, ctx->sym, new_out_fact);
				fprintf(stderr, "\n");
			}

			if (ctx->analysis->is_less_than_or_equal(ctx->sym, new_out_fact, to_node_facts->outb)) {
				//e didn't affect the result
				ADEBUG(("\t => not propagating (less-than-or-equal says that there is nothing new here)"));
				ctx->analysis->free(new_out_fact);
			} else {
				ctx->analysis->free(to_node_facts->outb);
				ADEBUG(("\t => propagating to all reachable nodes"));
				to_node_facts->outb = new_out_fact;
				//e fact is meaningful, update others
				for (int i = 0; i < cfg_edges_nr(to_node, edge_direction); i++) {
					edge_t edge = { .from = to_node, .to = cfg_edge(to_node, i, edge_direction) };
					ADEBUG(("\t<%p> -> <%p> added to worklist", edge.from, edge.to->node));
					stack_push(ctx->worklist, &edge);
				}
			}
		} else {
			ADEBUG(("\t => ignoring (less-than-or-equal says that there is nothing new here)"));
		}
	}

	stack_free(ctx->worklist, NULL);
	ctx->worklist = NULL;
	
	ADEBUG(("---------------------------------------- Worklist empty ----------------------------------------"));

	if (ctx->analysis->postprocessor) {
		void *data;
		
		if (ctx->analysis->postprocessor->init) {
			ctx->analysis->postprocessor->init(ctx->sym, &data);
		}
		
		if (ctx->analysis->postprocessor->visit_node) {
			set = hashset_ptr_alloc();
			dflow_postprocess_node(ctx, ctx->init, &data, set);
			hashset_ptr_free(set);
		}

		if (ctx->analysis->postprocessor->free) {
			ctx->analysis->postprocessor->free(ctx->sym, &data);
		}

	}
}


static data_flow_analysis_t *all_analyses[DATA_FLOW_ANALYSES_NR];

/*e
 * Assign globally unique IDs to the analyses
 */
static void
init_analyses(void)
{
	static bool initialised = false;
	if (initialised) {
		return;
	}

	int global_index = 0;

	int i = 0;
	while (analysis_sets[i]) {
		data_flow_analysis_t **analyses = analysis_sets[i++];
		int k = 0;
		while (analyses[k]) {
			data_flow_analysis_t *analysis = analyses[k++];
			assert(global_index < DATA_FLOW_ANALYSES_NR);
			analysis->unique_global_index = global_index++;
			all_analyses[analysis->unique_global_index] = analysis;
		}
	}

	//e if this fails: increase DATA_FLOW_ANALYSES_NR
	initialised = true;
}

data_flow_analysis_t *
data_flow_analysis_by_index(int index)
{
	init_analyses();
	if (index < 0 || index >= DATA_FLOW_ANALYSES_NR) {
		return NULL;
	}
	return all_analyses[index];
}

data_flow_analysis_t *
data_flow_analysis_by_name(char *name)
{
	init_analyses();
	for (int i = 0; i < DATA_FLOW_ANALYSES_NR; ++i) {
		if (all_analyses[i] && !strcmp(all_analyses[i]->name, name)) {
			return all_analyses[i];
		}
	}
	return NULL;
}

int
data_flow_analyses(symtab_entry_t *sym, data_flow_analysis_t *analyses[])
{
	init_analyses();
	
	context_t context = {
		.init = sym->astref->cfg,
		.exit = sym->cfg_exit,
		.worklist = NULL,
		.sym = sym,
		.analysis_index = 0,
		.analysis = NULL
	};

	error_count = 0;
	int index = 0;
	while (analyses[index]) {
		context.analysis_index = analyses[index]->unique_global_index;
		context.analysis = analyses[index];
		++index;
		dflow_analyse(&context);
	}
	return error_count;
}

void
data_flow_free(cfg_data_flow_facts_t *data)
{
	int i = 0;
	while (analysis_sets[i]) {
		data_flow_analysis_t **analyses = analysis_sets[i++];
		int k = 0;
		while (analyses[k]) {
			data_flow_analysis_t *analysis = analyses[k++];
			cfg_data_flow_facts_t *facts = data + analysis->unique_global_index;
			analysis->free(facts->inb);
			analysis->free(facts->outb);
		}
	}
}

int
data_flow_number_of_locals(symtab_entry_t *sym)
{
	if (sym->symtab_flags & SYMTAB_MAIN_ENTRY_POINT) {
		return 0;
	}
	int count = sym->storage.vars_nr + sym->parameters_nr;
	/* if (sym->symtab_flags & (SYMTAB_MEMBER | SYMTAB_CONSTRUCTOR)) { */
	/* 	++count;  // `self' */
	/* } */
	return count;
}

int
data_flow_is_local_var(symtab_entry_t *sym, ast_node_t *ast)
{
	if (sym->symtab_flags & SYMTAB_MAIN_ENTRY_POINT) {
		//e main function has no locals (all variables can be modified by subroutines,
		//e which would break intra-procedural analysis)
		return -1;
	}
	
	switch (NODE_TY(ast)) {
	case AST_VALUE_ID:
		if (ast->sym->id == BUILTIN_OP_SELF) {
			return -1;
		}
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
data_flow_get_all_locals_internal(symtab_entry_t *sym, symtab_entry_t **locals, ast_node_t *ast)
{
	if (!ast) {
		return;
	}
	
	int var_nr = data_flow_is_local_var(sym, ast);
	if (var_nr >= 0) {
		locals[var_nr] = ast->sym;
	}

	if (!IS_VALUE_NODE(ast)) {
		for (int i = 0; i < ast->children_nr; ++i) {
			data_flow_get_all_locals_internal(sym, locals, ast->children[i]);
		}
	}
}

void
data_flow_get_all_locals(symtab_entry_t *sym, symtab_entry_t **locals)
{
	assert(sym->astref); /*e only for callables (functions/methods/constructors) */
	memset(locals, 0, sizeof(symtab_entry_t *) * data_flow_number_of_locals(sym));
	data_flow_get_all_locals_internal(sym, locals, sym->astref);
}


void
data_flow_print(FILE *file, cfg_node_t *node, int flags)
{
	// FIXME
}
