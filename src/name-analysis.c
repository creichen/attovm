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

#include "analysis.h"
#include "chash.h"
#include "symbol-table.h"

// Spezielle Flags bei der rekursiven Namenauflösung, die wir nicht an die Symboltabelle weiterreichen
#define NF_SPECIAL_CHILD_FLAGS	0xf000
#define NF_SELECTOR		0x1000

extern int symtab_selectors_nr; // from symbol-table.c
hashtable_t *selectors_table;	// Bildet Selektor-namen auf EINEM der passenden Symboltabelleneinträge ab (nur für Aufruge!)

static int error_count = 0;

int
name_analysis_errors()
{
	return error_count;
}

static void
error(const ast_node_t *node, char *message)
{
	fprintf(stderr, "Name error for `%s': %s\n", AV_NAME(node), message);
	++error_count;
}

static void
fixnames(ast_node_t *node, hashtable_t *orig_env, symtab_entry_t *parent, int child_flags);

static void
fixnames_recursive(ast_node_t *node, hashtable_t *env, symtab_entry_t *parent, int child_flags)
{
	if (!node || IS_VALUE_NODE(node)) {
		return;
	}
	for (int i = 0; i < node->children_nr; i++) {
		fixnames(node->children[i], env, parent, child_flags);
	}
}

static symtab_entry_t *
get_selector(ast_node_t *node, int child_flags)
{
	symtab_entry_t *lookup = hashtable_get(selectors_table, AV_NAME(node));
	if (lookup) {
		return lookup;
	}

	// Alloziere neuen Selektor
	lookup = symtab_new(node->type & ~AST_NODE_MASK,
			    SYMTAB_SELECTOR | (child_flags & ~NF_SPECIAL_CHILD_FLAGS),
			    AV_NAME(node),
			    NULL /* Selektoren werden nicht deklariert */);
	lookup->selector = symtab_selectors_nr;

	hashtable_put(selectors_table, AV_NAME(node), lookup, NULL);
	++symtab_selectors_nr;

	return lookup;
}

static void
resolve_node(ast_node_t *node, symtab_entry_t *symtab)
{
	node->type = (node->type & ~AST_NODE_MASK) // Flags erhalten
		| AST_VALUE_ID;
	AV_ID(node) = symtab->id;
	node->annotations = symtab;
}

// fuer CLASSDEF und FUNDEF:
static void
fix_with_parameters(ast_node_t *cnode, hashtable_t *env, int child_flags_params, int child_flags_body)
{
	symtab_entry_t *syminfo = cnode->children[0]->annotations;
	assert(syminfo);

	env = hashtable_clone(env, NULL, NULL);	

	// CLASSDEF und FUNDEF haben analoge Struktur für Parameter:
	ast_node_t *formals = cnode->children[1];
	fixnames_recursive(formals, env, syminfo, (child_flags_params & ~SYMTAB_MEMBER) | SYMTAB_PARAM);
	// Setzt implizit parameters_nr korrekt
	syminfo->parameter_types = malloc(sizeof(unsigned short) * syminfo->parameters_nr);
	for (int i = 0; i < syminfo->parameters_nr; i++) {
		ast_node_t *child = formals->children[i];
		if (child != NULL) {
			syminfo->parameter_types[i] = child->type & ~AST_NODE_MASK;
		}
	}

	fixnames(cnode->children[2], env, syminfo, child_flags_body);

	hashtable_free(env, NULL, NULL);
}


static void
fixnames(ast_node_t *node, hashtable_t *env, symtab_entry_t *parent, int child_flags)
{
	symtab_entry_t *lookup;
	if (!node) {
		return;
	}

	/* fprintf(stderr, ">> "); */
	/* ast_node_dump(stderr, node, 0); */
	/* fprintf(stderr, "\n"); */

	switch (NODE_TY(node)) {
	case AST_VALUE_NAME:
		if (child_flags & NF_SELECTOR) {
			lookup = get_selector(node, 0);
		} else {
			lookup = hashtable_get(env, AV_NAME(node));
		}
		if (!lookup) {
			error(node, "undefined name");
		} else {
			resolve_node(node, lookup);
		}
		break;

	case AST_NODE_FUNDEF:
		// Definitionen werden im umgebenden AST_NODE_BLOCK gemanaged
		fix_with_parameters(node, env, (child_flags & ~SYMTAB_MEMBER) | SYMTAB_PARAM, child_flags & ~SYMTAB_MEMBER);
		return;
	case AST_NODE_CLASSDEF:
		// Definitionen werden im umgebenden AST_NODE_BLOCK gemanaged
		fix_with_parameters(node, env, (child_flags & ~SYMTAB_MEMBER) | SYMTAB_PARAM, child_flags | SYMTAB_MEMBER);
		return;

	case AST_NODE_FORMALS:
		fixnames_recursive(node, env, parent, child_flags | SYMTAB_PARAM);
		break;

	case AST_NODE_VARDECL: {
		// Definition ist in der Initialisierung noch nicht sichtbar, also erst hierhin:
		fixnames(node->children[1], env, parent, child_flags);
		ast_node_t *name_node = node->children[0];
		int occurrence_count = 0;
		lookup = hashtable_get(env, AV_NAME(name_node));
		if (lookup) {
			occurrence_count = lookup->occurrence_count + 1;
		}

		lookup = symtab_new(node->type & ~AST_NODE_MASK,
				    SYMTAB_TY_VAR | (child_flags & ~NF_SPECIAL_CHILD_FLAGS),
				    AV_NAME(name_node),
				    node);
		lookup->occurrence_count = occurrence_count;
		lookup->parent = parent;
		// Speicherstelle in Klasse wählen
		if (child_flags & SYMTAB_MEMBER && parent) {
			lookup->offset = parent->fields_nr++;
		} else if (child_flags & SYMTAB_PARAM && parent) { // parent == NULL bei Namensfehlern möglich
			lookup->offset = parent->parameters_nr++;
		}
		char *name = AV_NAME(name_node);
		resolve_node(node->children[0], lookup);
		hashtable_put(env, name, lookup, NULL);
		assert(lookup == hashtable_get(env, name));
		return;
	}

	case AST_NODE_MEMBER:
		fixnames(node->children[0], env, parent, child_flags);
		fixnames(node->children[1], env, parent, child_flags | NF_SELECTOR);
		break;

	case AST_NODE_ASSIGN: // Als lvalue markieren
		fixnames(node->children[0], env, parent, child_flags | SYMTAB_LVALUE);
		fixnames(node->children[1], env, parent, child_flags);
		return;

	case AST_NODE_BLOCK: {
		// Separater erster Durchlauf, um gegenseitige Rekursion Klassen/Funktionen zu erlauben
		hashtable_t *orig_env = env;
		env = hashtable_clone(env, NULL, NULL);
		for (int i = 0; i < node->children_nr; i++) {
			ast_node_t *cnode = node->children[i];
			symtab_entry_t *syminfo = NULL;
			ast_node_t *name_node = NULL;

			switch (NODE_TY(cnode)) {
			case AST_NODE_FUNDEF: {
				name_node = cnode->children[0];
				if (parent && !(child_flags & SYMTAB_MEMBER)) {
					error(name_node, "nested functions are not permitted");
				}
				syminfo = symtab_new(node->type & ~AST_NODE_MASK,
						     SYMTAB_TY_FUNCTION | (child_flags & ~NF_SPECIAL_CHILD_FLAGS),
						     AV_NAME(name_node),
						     node);

				if (child_flags & SYMTAB_MEMBER) {
					syminfo->selector = get_selector(cnode->children[0], 0)->selector;
					syminfo->offset = parent->methods_nr++;
					syminfo->parent = parent;
				}
				break;
			}

			case AST_NODE_CLASSDEF: {
				name_node = cnode->children[0];
				if (parent) {
					error(name_node, "nested classes are not permitted");
				}
				syminfo = symtab_new(node->type & ~AST_NODE_MASK,
						     SYMTAB_TY_CLASS | (child_flags & ~NF_SPECIAL_CHILD_FLAGS),
						     AV_NAME(name_node),
						     node);
				break;
			}

			default:
				break;
			}

			if (syminfo) {
				lookup = hashtable_get(env, syminfo->name);
				if (lookup && lookup != hashtable_get(orig_env, syminfo->name)) {
					error(cnode->children[0], "multiple recursive definitions with identical name");
				}
				hashtable_put(env, syminfo->name, syminfo, NULL);
				resolve_node(name_node, syminfo);

				/* fprintf(stderr, "  res: "); */
				/* ast_node_dump(stderr, cnode, 0); */
				/* fprintf(stderr, "\n"); */

			}
		}
		fixnames_recursive(node, env, parent, child_flags);
		hashtable_free(env, NULL, NULL);
		return;
	}

	default:
		break;
	}

	fixnames_recursive(node, env, parent, child_flags);
}

// puts initial set of global names into env and selectors table
static void
populate_initial_env(hashtable_t *env)
{
	for (int i = 1; i <= symtab_entries_builtin_nr; i++) {
		symtab_entry_t *entry = symtab_lookup(-i);
		if (! (entry->symtab_flags & SYMTAB_HIDDEN)) {
			if (entry->symtab_flags & SYMTAB_SELECTOR) {
				hashtable_put(selectors_table, entry->name, entry, NULL);
			} else {
				hashtable_put(env, entry->name, entry, NULL);
			}
		}
	}
}

void
name_analysis(ast_node_t *node)
{
	if (!selectors_table) {
		selectors_table = hashtable_alloc(hashtable_pointer_hash, hashtable_pointer_compare, 5);
	}
	hashtable_t *initial_env = hashtable_alloc(hashtable_pointer_hash, hashtable_pointer_compare, 5);

	populate_initial_env(initial_env);

	fixnames(node, initial_env, NULL, 0);
	hashtable_free(initial_env, NULL, NULL);
}
