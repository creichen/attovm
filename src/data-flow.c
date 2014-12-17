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

#include <stdarg.h>

#include "ast.h"
#include "cstack.h"
#include "control-flow-graph.h"
#include "symbol-table.h"
#include "data-flow.h"

data_flow_analysis_t *data_flow_analyses_correctness[] = {
	NULL /*e terminator: must be final entry! */
};
data_flow_analysis_t *data_flow_analyses_optimisation[] = {
	NULL /*e terminator: must be final entry! */
};


static int error_count;

static void
error(const ast_node_t *node, char *fmt, ...)
{
	// Variable Anzahl von Parametern
	va_list args;
	fprintf(stderr, "Type error in line %d: ", node->source_line);
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

static void
dflow_init_node(cstack_t *worklist, ast_node_t *node, data_flow_analysis_t *analysis)
{
#if 0
	...analysis->init(data, node);
	for (int i = 0; i < cfg_edges_nr(node, edge_direction); i++) {
		edge_t edge { .from = node, .to = cfg_edge(node, i, edge_direction) };
		stack_push(worklist, &edge);
	}
#endif
}

static void
dflow_analyse(cfg_node_t *init, cfg_node_t *exit, data_flow_analysis_t *analysis)
{
#if 0
	bool edge_direction = analysis->forward ? CFG_OUT : CFG_IN;

	cstack_t *worklist = stack_alloc(sizeof(edge_t), 128);

	dflow_init_node(worklist, node, analysis);

	while (stack_size(worklist)) {
		edge_t *edge = stack_pop(worklist);
		cfg_node_t *to_node = edge->to->node;

		void *fact = analysis->transfer(edge->from->ast);
		if (!analysis->is_less_than(fact, ...)) {
			//e fact is meaningful, update
			... = analysis->join(..., fact);
			for (int i = 0; i < cfg_edges_nr(to_node, edge_direction); i++) {
				edge_t edge { .from = to_node, .to = cfg_edge(to_node, i, edge_direction) };
				stack_push(worklist, &edge);
			}
		}
		analysis->free(fact);
	}

    stack_free(worklist);
#endif
}


void
data_flow_analyses(symtab_entry_t *sym, data_flow_analysis_t *analyses[])
{
#if 0
	error_count = 0;
	int index = 0;
	while (analyses[index]) {
		...;
		++index;
	}
	return error_count;
#endif
}

void
data_flow_free(void *data[])
{
	// FIXME
}

void
data_flow_print(FILE *file, cfg_node_t *node, int flags)
{
	// FIXME
}
