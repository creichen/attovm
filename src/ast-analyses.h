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

#ifndef _ATTOL_AST_ANALYSES_H
#define _ATTOL_AST_ANALYSES_H
//e optional analyses to run on the AST after name and type analysis

//e Analyses start below:
#define ANALYSIS_CONTROL_FLOW_GRAPH	/*e #define to build a control-flow graph */


#ifdef ANALYSIS_CONTROL_FLOW_GRAPH
#  include "control-flow-graph.h"
#endif


struct ast_analyses {
#ifdef ANALYSIS_CONTROL_FLOW_GRAPH
	control_flow_node_t *cfg;
#endif
};


/*e
 * Run all activated AST analyses
 */
void
ast_analyses_run(ast_node_t *ast);

/*e
 * Deallocate all analysis-specific allocated information from a single node
 *
 * @param ast The single node to deallocate from
 */
void
ast_analyses_free(ast_node_t *ast);

#define AST_NODE_DUMP_CONTROL_FLOW_GRAPH 0x1000
/*e
 * Perform pretty printing of analysis results on the AST
 */
void
ast_analyses_print(FILE *file, ast_node_t *ast, int flags);

#endif // !defined(_ATTOL_AST_ANALYSES_H)
