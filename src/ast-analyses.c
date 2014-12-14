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

#include "ast.h"

void
ast_analyses_run(ast_node_t *ast)
{
#ifdef ANALYSIS_CONTROL_FLOW_GRAPH
	control_flow_graph_build(ast);
#endif
}

void
ast_analyses_free(ast_node_t *ast)
{
#ifdef ANALYSIS_CONTROL_FLOW_GRAPH
	control_flow_graph_node_free(ast);
#endif
}

void
ast_analyses_print(FILE *file, ast_node_t *node, int flags)
{
#ifdef ANALYSIS_CONTROL_FLOW_GRAPH
	if (flags & AST_NODE_DUMP_CONTROL_FLOW_GRAPH) {
		control_flow_graph_node_print(file, node);
	}
#endif
}

