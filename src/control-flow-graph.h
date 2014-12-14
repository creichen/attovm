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

#ifndef _ATTOL_CONTROL_FLOW_GRAPH_H
#define _ATTOL_CONTROL_FLOW_GRAPH_H

#include "cstack.h"

#include <stdbool.h>

struct ast_node;
typedef struct ast_node ast_node_t;

typedef struct {
	struct ast_node *node;	/*e guaranteed non-NULL */
} control_flow_edge_t;

typedef struct control_flow_node {
	cstack_t *in; /*e contains control_flow_edge_t */
	cstack_t *out;
	struct ast_node *return_node; /*e function/method definitions/uses only: exit/return node */
} control_flow_node_t;

//e preprocessor interface

/*e
 * Obtains the given node's return node, if it exists (function/method definitions and invocations only)
 */
#define CONTROL_FLOW_RETURN(ast_node) (((ast_node)->analyses.cfg)? ((ast_node)->analyses.cfg->return_node) : NULL)

#define CONTROL_FLOW_IN true
#define CONTROL_FLOW_OUT false

/*e
 * Constructs a full control-flow graph for the given AST
 */
void
control_flow_graph_build(ast_node_t *ast);

/*e
 * Reads the number of control flow edges
 *
 * @param node The node to read from
 * @param edge_direction CONTROL_FLOW_IN or CONTROL_FLOW_OUT
 * @return Number of edges in the given edge direction
 */
int
control_flow_graph_edges_nr(ast_node_t *node, bool edge_direction);

/*e
 * Reads the control flow graph's edges
 *
 * @param node The node to read from
 * @param edge_nr Number of the edge to read
 * @param edge_direction CONTROL_FLOW_IN or CONTROL_FLOW_OUT
 */
control_flow_edge_t *
control_flow_graph_edge(ast_node_t *node, int edge_nr, bool edge_direction);

/*e
 * deallocates control_flow_node_t information from a single AST node
 *
 * @param node The node from which to deallocate
 */
void
control_flow_graph_node_free(ast_node_t *node);

/*e
 * Finds the child indices of all subnodes that show up in the control flow graph
 *
 * Usage example:
 *
 * int *subnodes;
 * int subnodes_nr = control_flow_graph_subnodes(node, &subnodes);
 * if (subnodes_nr == -1) {
 *    subnodes_n
 * }
 * for (int i = 0; i < subnodes_nr; i++) {
 *    int index = i;
 *    if (subnodes_nr != -1
 *    ast_node_t *subnode = node->children[i];
 *    ...
 * }
 *
 * @param node The node to analyse
 * @param subs Pointer into which the array of sub-node indices is stored
 * @return Number of entries in *subs, or -1 when called on a BLOCK (for which ALL
 * subnodes show up on the control flow graph)
 */
int
control_flow_graph_subnodes(ast_node_t *node, int **subs);

/*e
 * print node-specific AST information
 */
void
control_flow_graph_node_print(FILE *file, ast_node_t *node);

/*e
 * print entire control-flow graph in DOT format
 */
void
control_flow_graph_dottify(FILE *file, ast_node_t *node);

#endif // !defined(_ATTOL_CONTROL_FLOW_GRAPH_H)
