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

#include "cstack.h"
#include "control-flow-graph.h"
#include "data-flow.h"

typedef struct {
	control_flow_node_t *from;
	control_flow_edge_t *to;
} edge_t;

static void
dflow_init_node(cstack_t *worklist, ast_node_t *node, data_flow_analysis_t *analysis)
{
	...analysis->init(data, node);
	for (int i = 0; i < control_flow_graph_edges_nr(node, edge_direction); i++) {
		edge_t edge { .from = node, .to = control_flow_graph_edge(node, i, edge_direction) };
		stack_push(worklist, &edge);
	}
}

void
dataflow_analyse(runtime_image_t *image, data_flow_analysis_t *analysis)
{
	bool edge_direction = analysis->forward ? CONTROL_FLOW_OUT : CONTROL_FLOW_IN;

	cstack_t *worklist = stack_alloc(sizeof(edge_t), 128);

	dflow_init_node(worklist, node, analysis);

	while (stack_size(worklist)) {
		edge_t *edge = stack_pop(worklist);
		control_flow_node_t *to_node = edge->to->node;

		void *fact = analysis->transfer(edge->from->ast);
		if (!analysis->is_less_than(fact, ...)) {
			//e fact is meaningful, update
			... = analysis->join(..., fact);
			for (int i = 0; i < control_flow_graph_edges_nr(to_node, edge_direction); i++) {
				edge_t edge { .from = to_node, .to = control_flow_graph_edge(to_node, i, edge_direction) };
				stack_push(worklist, &edge);
			}
		}
		analysis->free(fact);
	}

    stack_free(worklist);
}
