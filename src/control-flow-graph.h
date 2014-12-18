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

#define DATA_FLOW_ANALYSES_NR 3	/*e max number of permitted data flow analyses */

struct ast_node;
typedef struct ast_node ast_node_t;

typedef struct {
	struct cfg_node *node;	/*e guaranteed non-NULL */
} cfg_edge_t;

typedef struct cfg_data_flow_facts {
	void *inb, *outb;
} cfg_data_flow_facts_t;

typedef struct cfg_node {
	cstack_t *in; /*e contains cfg_edge_t */
	cstack_t *out;
	ast_node_t *ast;
	cfg_data_flow_facts_t analysis[DATA_FLOW_ANALYSES_NR];
} cfg_node_t;

struct symtab_entry;
struct data_flow_analysis;

//e preprocessor interface

/*e
 * Obtains the given node's return node, if it exists (function/method definitions and invocations only)
 */
#define CFG_RETURN(ast_node) (((ast_node)->analyses.cfg)? ((ast_node)->analyses.cfg->return_node) : NULL)

#define CFG_IN true
#define CFG_OUT false

/*e
 * Constructs a full control-flow graph for the given AST
 *
 * @param ast The abstract syntax tree 
 * @return The control flow graph's exit node
 */
cfg_node_t *
cfg_build(ast_node_t *ast);

/*e
 * Reads the number of control flow edges
 *
 * @param node The node to read from
 * @param edge_direction CFG_IN or CFG_OUT
 * @return Number of edges in the given edge direction
 */
int
cfg_edges_nr(cfg_node_t *node, bool edge_direction);

/*e
 * Reads the control flow graph's edges
 *
 * @param node The node to read from
 * @param edge_nr Number of the edge to read
 * @param edge_direction CFG_IN or CFG_OUT
 */
cfg_edge_t *
cfg_edge(cfg_node_t *node, int edge_nr, bool edge_direction);

/*e
 * deallocates cfg_node_t information from a single AST node
 *
 * @param node The node from which to deallocate
 */
void
cfg_ast_free(ast_node_t *node);

/*e
 * deallocates cfg_node_t information
 */
void
cfg_node_free(cfg_node_t *node);

/*e
 * Finds the child indices of all AST subnodes that show up in the control flow graph
 *
 * Usage example:
 *
 * int *subnodes;
 * int subnodes_nr = cfg_subnodes(node, &subnodes);
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
cfg_subnodes(ast_node_t *node, int **subs);

/*e
 * print node-specific AST information
 */
void
cfg_node_print(FILE *file, cfg_node_t *node);

/*e
 * print entire control-flow graph in DOT format
 *
 * @param file The file to print to
 * @param symbol The symbol to dottify
 * @param analysis Any program analysis results to print, or NULL
 */
void
cfg_dottify(FILE *file, struct symtab_entry *symbol, struct data_flow_analysis *analysis);

#define AST_NODE_DUMP_CFG	0x1000
#define AST_NODE_DUMP_DATA_FLOW	0x10000 /*e shift left by index of data flow analysis to print */
#define AST_NODE_DUMP_DATA_FLOW_SHIFT 16
/*e
 * Perform pretty printing of CFG and/or analysis results on the AST
 */
void
cfg_analyses_print(FILE *file, ast_node_t *ast, int flags);


#endif // !defined(_ATTOL_CFG_H)
