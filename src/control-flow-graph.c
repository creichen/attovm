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
#include "chash.h"
#include "data-flow.h"
#include "symbol-table.h"

#define STACK(node, dir) (((dir) == CFG_IN)? ((node)->in) : ((node)->out))

static cfg_node_t *
cfg_new_node(ast_node_t *node)
{
	cfg_node_t *n = calloc(1, sizeof(cfg_node_t));
	n->ast = node;
	return n;
}

static cfg_node_t *
cfg(ast_node_t *node)
{
	if (!node->cfg) {
		node->cfg = cfg_new_node(node);
	}
	return node->cfg;
}


//e Add one direction only
static void
cfg_add_unidirectional(cfg_node_t *base, bool edge_direction, cfg_node_t *cfg)
{
	cstack_t *stack = STACK(base, edge_direction);
	if (!stack) {
		stack = stack_alloc(sizeof(cfg_edge_t), 2);
		if (edge_direction == CFG_IN) {
			base->in = stack;
		} else {
			base->out = stack;
		}
	}
	cfg_edge_t edge = { .node = cfg };
	stack_push(stack, &edge);
}

//e Add forward and backward edge
static void
cfg_add(cfg_node_t *base, cfg_node_t *node)
{
	cfg_add_unidirectional(base, CFG_OUT, node);
	cfg_add_unidirectional(node, CFG_IN, base);
}

int
cfg_edges_nr(cfg_node_t *cfg_node, bool edge_direction)
{
	if (!cfg_node) {
		return 0;
	}
	cstack_t *stack = STACK(cfg_node, edge_direction);
	if (!stack) {
		return 0;
	}
	return stack_size(stack);
}

cfg_edge_t *
cfg_edge(cfg_node_t *cfg_node, int edge_nr, bool edge_direction)
{
	return (cfg_edge_t *) stack_get(STACK(cfg_node, edge_direction), edge_nr);
}

void
cfg_ast_free(ast_node_t *node)
{
	cfg_node_free(node->cfg);
}

void
cfg_node_free(cfg_node_t *cfg)
{
	if (!cfg) {
		return;
	}

	cstack_t *stack = cfg->in;
	if (stack) {
		stack_free(stack, NULL);
	}

	stack = cfg->out;
	if (stack) {
		stack_free(stack, NULL);
	}

	data_flow_free(cfg->analysis);

	free(cfg);
}

static void
cfg_print_node_addr(FILE *f, void *a)
{
	fprintf(f, "%p", (((cfg_edge_t *)a))->node);
}

void
cfg_node_print(FILE *file, cfg_node_t *cfg_node)
{
	if (!cfg_node) {
		return;
	}
	cstack_t *stack_in = STACK(cfg_node, CFG_IN);
	cstack_t *stack_out = STACK(cfg_node, CFG_OUT);
	fprintf(file, "{cfg:%p", cfg_node);
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
cfg_subnodes(ast_node_t *node, int **subs)
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
push_node(cstack_t* stack, cfg_node_t *cfg_node)
{
	stack_push(stack, &cfg_node);
}

static void
move_stack(cstack_t *to_stack, cstack_t *from_stack)
{
	cfg_node_t **transfer;
	while ((transfer = stack_pop(from_stack))) {
		stack_push(to_stack, transfer);
	}
}

typedef struct {
	cstack_t *before;	/*e set of nodes leading into the current node */
	cstack_t *loop_breaks;	/*e set of nodes that have issued `break' statements within the current loop */
	cfg_node_t *fun_exit;	/*e function exit node (for `return') */
	cfg_node_t *loop_continue;	/*e innermost loop node (for `continue') */
} cfg_context_t;

static void
cfg_exit(cfg_context_t context)
{
	while (stack_size(context.before)) {
		cfg_node_t *before = *((cfg_node_t **) stack_pop(context.before));
		cfg_add(before, context.fun_exit);
	}
}

static void
cfg_make(ast_node_t *node, cfg_context_t context)
{
	int node_ty = NODE_TY(node);
	if (node_ty != AST_NODE_BLOCK
	    && node_ty != AST_NODE_FUNDEF
	    && node_ty != AST_NODE_CLASSDEF) {
		while (stack_size(context.before)) {
			cfg_node_t *before = *((cfg_node_t **) stack_pop(context.before));
			cfg_add(before, cfg(node));
		}
	}
	switch (NODE_TY(node)) {

	case AST_NODE_CLASSDEF: {
		//e only examine fundefs and constructor
		ast_node_t *defs = node->children[2];
		symtab_entry_t *sym = node->sym;
		const int methods_start = sym->storage.fields_nr;
		const int methods_nr = sym->storage.functions_nr;
		context.before = stack_clone(context.before, NULL);
		for (int i = methods_start; i < methods_start + methods_nr; ++i) {
			stack_clear(context.before, NULL);
			cfg_make(defs->children[i], context);
		}
		//e finally, check the constructor
		stack_clear(context.before, NULL);
		cfg_make(node->children[3], context);
		//e clean up
		stack_free(context.before, NULL);
		return;
	}

	case AST_NODE_FUNDEF: {
		cfg_node_t *exit_node = cfg_new_node(node);
		AST_CALLABLE_SYMREF(node)->cfg_exit = exit_node;
		
		context.before = stack_alloc(sizeof(cfg_node_t *), 2);
		push_node(context.before, cfg(node));

		context.fun_exit = exit_node;
		cfg_make(node->children[2], context);
		cfg_exit(context);
		return;
	}
		
	case AST_NODE_RETURN:
		cfg_add(cfg(node), context.fun_exit);
		stack_clear(context.before, NULL);
		return;

	case AST_NODE_IF: {
		push_node(context.before, cfg(node));
		cstack_t *stack_backup = context.before;
		context.before = stack_clone(context.before, NULL);
		cfg_make(node->children[1], context);
		cstack_t *alt_paths = context.before;
		context.before = stack_backup;
		if (node->children[2]) {
			cfg_make(node->children[2], context);
		}
		// merge the two paths:
		move_stack(context.before, alt_paths);
		stack_free(alt_paths, NULL);
	}
		return;

	case AST_NODE_WHILE: {
		context.loop_continue = cfg(node);

		push_node(context.before, cfg(node));
		cstack_t *stack_backup = context.before;
		context.before = stack_clone(context.before, NULL);
		cfg_make(node->children[1], context);
		cstack_t *body_paths = context.before;
		for (int i = 0; i < stack_size(body_paths); i++) {
			cfg_add(*((cfg_node_t **)stack_get(body_paths, i)), cfg(node));
		}
		context.before = stack_backup;
		// merge the two paths:
		move_stack(context.before, context.loop_breaks);
		stack_free(body_paths, NULL);
	}
		return;
		
	case AST_NODE_BREAK:
		push_node(context.loop_breaks, cfg(node));
		stack_clear(context.before, NULL);
		return;
		
	case AST_NODE_CONTINUE:
		cfg_add(cfg(node), context.loop_continue);
		stack_clear(context.before, NULL);
		return;

	case AST_NODE_BLOCK: {
		for (int i = 0; i < node->children_nr; ++i) {
			cfg_make(node->children[i], context);
		}
		return;
	}
	default:
		break;
	}
	push_node(context.before, cfg(node));
}

cfg_node_t *
cfg_build(ast_node_t *ast)
{
	cfg_context_t context;
	context.before = stack_alloc(sizeof(ast_node_t *), 2);
	if (NODE_TY(ast) == AST_NODE_BLOCK) {
		//e introduce init node, so that ast->cfg will always set for the main entry point
		cfg_node_t *init_node = cfg_new_node(ast);
		ast->cfg = init_node;
		push_node(context.before, init_node);
	}
	
	context.loop_breaks = stack_alloc(sizeof(ast_node_t *), 2);
	context.fun_exit = cfg_new_node(NULL);
	context.loop_continue = NULL;
	cfg_make(ast, context);
	cfg_exit(context);
	stack_free(context.before, NULL);
	stack_free(context.loop_breaks, NULL);
	
	return context.fun_exit;
}

// --------------------------------------------------------------------------------
//e Temporary writing buffer: used to escape characters that aren't permissible in DOT output format

#define MAX_EXPR_SIZE 80

struct temp_write_buf {
	char buf[MAX_EXPR_SIZE + 1];
	FILE *file;
};

static FILE *
temp_write_init(struct temp_write_buf *buf)
{
	FILE *tempfile = fmemopen(buf->buf, MAX_EXPR_SIZE, "w");
	buf->file = tempfile;
	return tempfile;
}

static void
temp_write_transfer(FILE *outfile, struct temp_write_buf *buf)
{
	fclose(buf->file);
	// now print while escaping
	for (int i = 0; i < strlen(buf->buf); ++i) {
		char c = buf->buf[i];
		if (c == '\\' || c == '"' || c == '|' || c == '{' || c == '}' || c == '<' || c == '>') {
			fputc('\\', outfile);
		}
		fputc(c, outfile);
	}

}

static void
cfg_subdottify(FILE *file, data_flow_analysis_t *analysis, symtab_entry_t *sym,
	       hashset_ptr_t *visited_nodes, cfg_node_t *cfg_node)
{
	if (!cfg_node || hashset_ptr_contains(visited_nodes, cfg_node)) {
		return;
	}
	hashset_ptr_add(visited_nodes, cfg_node);

	fprintf(file, "  n%p [shape=record, label=\"", cfg_node);

	if (analysis) {
		fprintf(file, "{ ");
		struct temp_write_buf buf;
		FILE *tempfile = temp_write_init(&buf);
		analysis->print(tempfile, sym, cfg_node->analysis[analysis->unique_global_index].inb);
		temp_write_transfer(file, &buf);
		fprintf(file, " | ");
	}

	if (!cfg_node->ast) {
		fprintf(file, "\\<exit\\>");
	} else {
		ast_node_t *ast_node = cfg_node->ast;
	
		switch (NODE_TY(ast_node)) {
		
		case AST_NODE_FUNDEF:
			symtab_entry_name_dump(file, AST_CALLABLE_SYMREF(ast_node));
			if (cfg_node == AST_CALLABLE_SYMREF(ast_node)->cfg_exit) {
				fprintf(file, ":exit");
			} else {
				fprintf(file, ":init");
			}
			break;

		case AST_NODE_BLOCK:
			fprintf(file, "\\<init\\>");
			break;

		default: {
			struct temp_write_buf buf;
			FILE *tempfile = temp_write_init(&buf);
			
			//e We temporarily NULL child nodes that are handled by the graph, to avoid information duplication
			int *subs_indices;
			int subs_nr = cfg_subnodes(ast_node, &subs_indices);
			assert(subs_nr >= 0);
			ast_node_t *subs[subs_nr];
			for (int i = 0; i < subs_nr; ++i) {
				subs[i] = ast_node->children[subs_indices[i]];
				ast_node->children[subs_indices[i]] = NULL;
			}
			ast_node_print(tempfile, ast_node, 1);

			//e And now we restore them again
			for (int i = 0; i < subs_nr; ++i) {
				ast_node->children[subs_indices[i]] = subs[i];
			}
			temp_write_transfer(file, &buf);
		}
		}
	}
	
	if (analysis) {
		fprintf(file, " | ");
		struct temp_write_buf buf;
		FILE *tempfile = temp_write_init(&buf);
		analysis->print(tempfile, sym, cfg_node->analysis[analysis->unique_global_index].outb);
		temp_write_transfer(file, &buf);
		fprintf(file, " }");
	}

	fprintf(file, "\"];\n");
	cstack_t *out_stack = STACK(cfg_node, CFG_OUT);
	if (out_stack) {
		for (int i = 0; i < stack_size(out_stack); ++i) {
			cfg_edge_t *edge = stack_get(out_stack, i);
			fprintf(file, "  n%p -> n%p;\n", cfg_node, edge->node);
		}
	}
	
	for (int i = 0; i < cfg_edges_nr(cfg_node, CFG_OUT); i++) {
		cfg_subdottify(file, analysis, sym, visited_nodes,
			       cfg_edge(cfg_node, i, CFG_OUT)->node);
	}
	//e The following is needed to find unreachable code:
	
	for (int i = 0; i < cfg_edges_nr(cfg_node, CFG_IN); i++) {
		cfg_subdottify(file, analysis, sym, visited_nodes,
			       cfg_edge(cfg_node, i, CFG_IN)->node);
	}
}

static void
cfg_subdottify_ast(FILE *file, data_flow_analysis_t *analysis, symtab_entry_t *sym,
		   hashset_ptr_t *visited_nodes, ast_node_t *node)
{
	if (!node) {
		return;
	}
	cfg_subdottify(file, analysis, sym, visited_nodes, node->cfg);
}

//#define DOTTIFY_CONSERVATIVELY /* this breaks analysis pretty-printing */

#ifdef DOTTIFY_CONSERVATIVELY
static void
cfg_subdottify_ast_recursively(FILE *file, data_flow_analysis_t *analysis, symtab_entry_t *sym,
			       hashset_ptr_t *visited_nodes, ast_node_t *node)

{
	cfg_subdottify_ast(file, analysis, sym, visited_nodes, node);
	if (node && !IS_VALUE_NODE(node)) {
		for (int i = 0; i < node->children_nr; i++) {
			cfg_subdottify_ast_recursively(file, analysis, sym, visited_nodes, node->children[i]);
		}
	}
}
#endif

void
cfg_dottify(FILE *file, symtab_entry_t *symtab, data_flow_analysis_t *analysis)
{
	hashset_ptr_t *visited_nodes = hashset_ptr_alloc();
#ifdef DOTTIFY_CONSERVATIVELY
	cfg_subdottify_ast_recursively(file, analysis, symtab, visited_nodes, symtab->astref);
#else
	cfg_subdottify_ast(file, analysis, symtab, visited_nodes, symtab->astref);

#endif
	hashset_ptr_free(visited_nodes);
}

void
cfg_analyses_print(FILE *file, ast_node_t *node, int flags)
{
	assert((1 << AST_NODE_DUMP_DATA_FLOW_SHIFT) == AST_NODE_DUMP_DATA_FLOW);
	cfg_node_t *cfg_node = node->cfg;
	if (!cfg_node) {
		return;
	}
	
	if (flags & AST_NODE_DUMP_CFG) {
		cfg_node_print(file, cfg_node);
	}
	data_flow_print(file, cfg_node, flags >> AST_NODE_DUMP_DATA_FLOW_SHIFT);
}
