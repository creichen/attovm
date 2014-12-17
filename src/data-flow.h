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

#ifndef _ATTOL_DATA_FLOW_H
#define _ATTOL_DATA_FLOW_H

#include <stdbool.h>

#include "control-flow-graph.h"
#include "ast.h"

/*e
 * Reports an error at a given AST node
 *
 * @param node The node to report the error for
 * @param fmt A format string (as for printf)
 * @param ... Any additional parameters needed for the format string (cf. `man printf')
 */
static void
data_flow_error(const ast_node_t *node, char *fmt, ...);

//e data flow analysis definitions

typedef struct data_flow_analysis {
	//e forward analysis?  otherwise backward
	bool forward;
	//e name of the analysis
	char *name;

	/*e initialise set of data flow facts for given AST node
	 *
	 * @param out Pointer to the variable to write the initial data flow fact to
	 * @param node AST node to initialise for
	 */
	void (*init)(void **out, ast_node_t *node);

	/*e
	 * Print out the given data flow fact, for debugging/pretty-printing
	 *
	 * @param file The output stream to print to
	 * @param data The fact to print
	 */
	void (*print)(FILE *file, void *data);

	/*e
	 * Joins two data flow facts
	 *
	 * @param in1, in2 The data flow fact to join
	 * @return The joined data flow fact
	 */
	void * (*join)(void *in1, void *in2);

	/*e
	 * Computes the effects of the transfer function for the given data flow node
	 *
	 * @param ast The AST node generating the transfer function
	 * @return The data flow fact generated by transferring `in' through `ast'
	 */
	void * (*transfer)(ast_node_t *ast, void *in);

	/*e
	 * Computes the effects of the transfer function for the given data flow node
	 *
	 * @param lhs, rhs Left-hand side and right-hand side of the comparison
	 * @return True iff rhs carries at least as much information as lhs
	 */
	bool (*is_less_than)(void *lhs, void *rhs);

	/*e
	 * Deallocates a data flow fact
	 *
	 * @param fact The fact to deallocate
	 */
	void (*free)(void *fact);
} data_flow_analysis_t;


/*e
 * Frees an array of data flow analysis results (from a CFG node)
 */
void
data_flow_free(void *data[]);

/*e
 * Prints out analysis results for the specified cfg node
 *
 * @param file File to print to
 * @param node CFG node containing the analysis results we want to print
 * @param flags Bit-flags to indicate which analysis results to print, with the least
 * significant bit printing analysis #0, the next bit enabling printing analysis #1 etc.
 */
void
data_flow_print(FILE *file, cfg_node_t *node, int flags);

#endif // !defined(_ATTOL_DATA_FLOW_H)
