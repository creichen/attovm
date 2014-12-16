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
#include "control-flow-graph.h"
#include "symbol-table.h"

#define STACK(node, dir) (((dir) == CONTROL_FLOW_IN)? (cfg(node)->in) : (cfg(node)->out))

static control_flow_node_t *
cfg(ast_node_t *node)
{
	if (!node->analyses.cfg) {
		node->analyses.cfg = calloc(1, sizeof(control_flow_node_t));
	}
	return node->analyses.cfg;
}

//e Add one direction only
static void
cfg_add_unidirectional(ast_node_t *base, bool edge_direction, ast_node_t *node)
{
	cstack_t *stack = STACK(base, edge_direction);
	if (!stack) {
		stack = stack_alloc(sizeof(control_flow_edge_t), 2);
		if (edge_direction == CONTROL_FLOW_IN) {
			base->analyses.cfg->in = stack;
		} else {
			base->analyses.cfg->out = stack;
		}
	}
	control_flow_edge_t edge = { .node = node };
	stack_push(stack, &edge);
}

//e Add forward and backward edge
static void
cfg_add(ast_node_t *base, ast_node_t *node)
{
	cfg_add_unidirectional(base, CONTROL_FLOW_OUT, node);
	cfg_add_unidirectional(node, CONTROL_FLOW_IN, base);
}

int
control_flow_graph_edges_nr(ast_node_t *node, bool edge_direction)
{
	if (!node->analyses.cfg) {
		return 0;
	}
	cstack_t *stack = STACK(node, edge_direction);
	if (!stack) {
		return 0;
	}
	return stack_size(stack);
}

control_flow_edge_t *
control_flow_graph_edge(ast_node_t *node, int edge_nr, bool edge_direction)
{
	return (control_flow_edge_t *) stack_get(STACK(node, edge_direction), edge_nr);
}

void
control_flow_graph_node_free(ast_node_t *node)
{
	if (!node->analyses.cfg) {
		return;
	}
	cstack_t *stack = STACK(node, CONTROL_FLOW_IN);
	if (stack) {
		stack_free(stack, NULL);
	}

	stack = STACK(node, CONTROL_FLOW_OUT);
	if (stack) {
		stack_free(stack, NULL);
	}

	if (node->analyses.cfg->return_node) {
		ast_node_free(node->analyses.cfg->return_node, true);
		node->analyses.cfg->return_node = NULL;
	}
	free(node->analyses.cfg);
}

static void
cfg_print_node_addr(FILE *f, void *a)
{
	fprintf(f, "%p", (((control_flow_edge_t *)a))->node);
}

void
control_flow_graph_node_print(FILE *file, ast_node_t *node)
{
	if (!node->analyses.cfg) {
		return;
	}
	cstack_t *stack_in = STACK(node, CONTROL_FLOW_IN);
	cstack_t *stack_out = STACK(node, CONTROL_FLOW_OUT);
	fprintf(file, "{cfg:%p", node);
	if (stack_in) {
		fputs(":in=", file);
		stack_print(file, stack_in, cfg_print_node_addr);
	}
	if (stack_out) {
		fputs(":out=", file);
		stack_print(file, stack_out, cfg_print_node_addr);
	}
	fputs("}", file);
}

static int skip_offsets[2] = { 1, 2 };

int
control_flow_graph_subnodes(ast_node_t *node, int **subs)
{
	switch (NODE_TY(node)) {

	case AST_NODE_BLOCK:
		return -1;
		
	case AST_NODE_FUNDEF:
		*subs = skip_offsets;
		return 2;

	case AST_NODE_CLASSDEF:
		*subs = skip_offsets;
		return 2;

	case AST_NODE_WHILE:
		*subs = skip_offsets;
		return 1;
		
	case AST_NODE_IF:
		*subs = skip_offsets;
		return 2;
		
	default:
		return 0;
	}
}

static void
push_node(cstack_t* stack, ast_node_t *node)
{
	stack_push(stack, &node);
}

static void
move_stack(cstack_t *to_stack, cstack_t *from_stack)
{
	ast_node_t **transfer;
	while ((transfer = stack_pop(from_stack))) {
		stack_push(to_stack, transfer);
	}
}

typedef struct {
	cstack_t *before;	/*e set of nodes leading into the current node */
	cstack_t *loop_breaks;	/*e set of nodes that have issued `break' statements within the current loop */
	ast_node_t *fun_exit;	/*e function exit node (for `return') */
	ast_node_t *loop_continue;	/*e innermost loop node (for `continue') */
} cfg_context_t;

static void
cfg_build(ast_node_t *node, cfg_context_t context)
{
	if (NODE_TY(node) != AST_NODE_BLOCK) {
		while (stack_size(context.before)) {
			ast_node_t *before = *((ast_node_t **) stack_pop(context.before));
			cfg_add(before, node);
		}
	}
	switch (NODE_TY(node)) {

	case AST_NODE_CLASSDEF: {
		// only examine fundefs
		ast_node_t *defs = node->children[2];
		symtab_entry_t *sym = node->sym;
		const int methods_start = sym->storage.fields_nr;
		const int methods_nr = sym->storage.functions_nr;
		context.before = stack_clone(context.before, NULL);
		for (int i = methods_start; i < methods_start + methods_nr; ++i) {
			stack_clear(context.before, NULL);
			cfg_build(defs->children[i], context);
		}
		stack_free(context.before, NULL);
		return;
	}

	case AST_NODE_FUNDEF: {
		ast_node_t *exit_node = value_node_alloc_generic(AST_NODE_INTERNAL, (ast_value_union_t) { .num = 0 });
		cfg(node)->return_node = exit_node;
		
		context.before = stack_alloc(sizeof(ast_node_t *), 2);
		push_node(context.before, node);

		context.fun_exit = exit_node;
		cfg_build(node->children[2], context);
		stack_free(context.before, NULL);
		return;
	}
		
	case AST_NODE_RETURN:
		cfg_add(node, context.fun_exit);
		stack_clear(context.before, NULL);
		return;

	case AST_NODE_IF: {
		push_node(context.before, node);
		cstack_t *stack_backup = context.before;
		context.before = stack_clone(context.before, NULL);
		cfg_build(node->children[1], context);
		cstack_t *alt_paths = context.before;
		context.before = stack_backup;
		if (node->children[2]) {
			cfg_build(node->children[2], context);
		}
		// merge the two paths:
		move_stack(context.before, alt_paths);
		stack_free(alt_paths, NULL);
	}
		return;

	case AST_NODE_WHILE: {
		context.loop_continue = node;

		push_node(context.before, node);
		cstack_t *stack_backup = context.before;
		context.before = stack_clone(context.before, NULL);
		cfg_build(node->children[1], context);
		cstack_t *body_paths = context.before;
		for (int i = 0; i < stack_size(body_paths); i++) {
			cfg_add(*((ast_node_t **)stack_get(body_paths, i)), node);
		}
		context.before = stack_backup;
		// merge the two paths:
		move_stack(context.before, body_paths);
		move_stack(context.before, context.loop_breaks);
		stack_free(body_paths, NULL);
	}
		return;
		
	case AST_NODE_BREAK:
		push_node(context.loop_breaks, node);
		stack_clear(context.before, NULL);
		return;
		
	case AST_NODE_CONTINUE:
		cfg_add(node, context.loop_continue);
		stack_clear(context.before, NULL);
		return;

	case AST_NODE_BLOCK: {
		for (int i = 0; i < node->children_nr; ++i) {
			cfg_build(node->children[i], context);
		}
		return;
	}
	default:
		break;
	}
	push_node(context.before, node);
}

void
control_flow_graph_build(ast_node_t *ast)
{
	cfg_context_t context;
	context.before = stack_alloc(sizeof(ast_node_t *), 2);
	context.loop_breaks = stack_alloc(sizeof(ast_node_t *), 2);
	context.fun_exit = NULL;
	context.loop_continue = NULL;
	cfg_build(ast, context);
	stack_free(context.before, NULL);
	stack_free(context.loop_breaks, NULL);
}

#define MAX_EXPR_SIZE 80

static void
cfg_dottify(FILE *file, ast_node_t *node, ast_node_t *owner /*e only for function exit node */)
{
	if (node == NULL) {
		return;
	}

	if (node->analyses.cfg) {
		
		fprintf(file, "  n%p [label=\"", node);

		switch (NODE_TY(node)) {

		case AST_NODE_FUNDEF:
			symtab_entry_name_dump(file, node->children[0]->sym);
			fprintf(file, ":init");
			break;

		case AST_NODE_INTERNAL:
			symtab_entry_name_dump(file, owner->children[0]->sym);
			fprintf(file, ":exit");
			break;
			
		default: {
			char expr_string[MAX_EXPR_SIZE + 1];
			FILE *tempfile = fmemopen(expr_string, MAX_EXPR_SIZE, "w");

			//e We temporarily NULL child nodes that are handled by the graph, to avoid information duplication
			int *subs_indices;
			int subs_nr = control_flow_graph_subnodes(node, &subs_indices);
			assert(subs_nr >= 0);
			ast_node_t *subs[subs_nr];
			for (int i = 0; i < subs_nr; ++i) {
				subs[i] = node->children[subs_indices[i]];
				node->children[subs_indices[i]] = NULL;
			}
			ast_node_print(tempfile, node, 1);
			fclose(tempfile);
			
			//e And now we restore them again
			for (int i = 0; i < subs_nr; ++i) {
				node->children[subs_indices[i]] = subs[i];
			}

			// now print while escaping
			for (int i = 0; i < strlen(expr_string); ++i) {
				char c = expr_string[i];
				if (c == '\\' || c == '"') {
					fputc('\\', file);
				}
				fputc(c, file);
			}
		}
		}

		fprintf(file, "\"];\n");
		cstack_t *out_stack = STACK(node, CONTROL_FLOW_OUT);
		if (out_stack) {
			for (int i = 0; i < stack_size(out_stack); ++i) {
				control_flow_edge_t *edge = stack_get(out_stack, i);
				fprintf(file, "  n%p -> n%p;\n", node, edge->node);
			}
		}

		cfg_dottify(file, node->analyses.cfg->return_node, node);
	}
	if (!IS_VALUE_NODE(node)) {
		for (int i = 0; i < node->children_nr; ++i) {
			cfg_dottify(file, node->children[i], NULL);
		}
	}
}

void
control_flow_graph_dottify(FILE *file, ast_node_t *ast)
{
	fprintf(file, "digraph control_flow_graph {\n");
	cfg_dottify(file, ast, NULL);
	fprintf(file, "}\n");
}
