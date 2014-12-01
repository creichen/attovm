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

#include <stdbool.h>
#include <assert.h>

#include "analysis.h"
#include "chash.h"
#include "symbol-table.h"

// Spezielle Flags bei der rekursiven Namenauflösung, die wir nicht an die Symboltabelle weiterreichen
#define NF_SPECIAL_CHILD_FLAGS	0xf0000000
#define NF_SELECTOR		0x80000000
#define NF_WITHIN_LOOP		0x40000000
#define NF_PART_OF_CLASSDECL	0x20000000
#define NF_NEED_STORAGE		0x10000000	/*e request temporary storage space */

extern int symtab_selectors_nr; // from symbol-table.c
extern hashtable_t *symtab_selectors_table;	// Bildet Selektor-namen auf EINEM der passenden Symboltabelleneinträge ab (nur für Aufrufe!)

static int error_count = 0;

static void
error(const ast_node_t *node, char *message)
{
	fprintf(stderr, "Name error in line %d for `%s': %s\n", node->source_line, AV_NAME(node), message);
	++error_count;
}

static void
fixnames(ast_node_t *node, hashtable_t *orig_env, symtab_entry_t *parent, int child_flags, storage_record_t *counters, int *classes_nr);

static void
fixnames_recursive(ast_node_t *node, hashtable_t *env, symtab_entry_t *parent, int child_flags, storage_record_t *counters, int *classes_nr)
{
	if (!node || IS_VALUE_NODE(node)) {
		return;
	}
	int base_temporaries = counters->temps_nr;
	int max_temporaries = base_temporaries;
	const int children_max = node->children_nr - 1;
	for (int i = 0; i <= children_max; ++i) {
		if (i == children_max) {
			//e we don't need storage for the very last call to a parameter list
			child_flags &= ~NF_NEED_STORAGE;
		}

		counters->temps_nr = base_temporaries;
		fixnames(node->children[i], env, parent, child_flags, counters, classes_nr);
		if (counters->temps_nr >= max_temporaries) {
			max_temporaries = counters->temps_nr;
		}

		if (child_flags & NF_NEED_STORAGE) {
			base_temporaries += 1;
		}
	}
	counters->temps_nr = max_temporaries;
}

static symtab_entry_t *
get_selector(ast_node_t *node)
{
	if (NODE_TY(node) != AST_VALUE_NAME) {
		*((int *)0) = 1;
	}
	return symtab_selector(AV_NAME(node));
}

static void
resolve_node(ast_node_t *node, symtab_entry_t *symtab)
{
	node->type = (node->type & ~AST_NODE_MASK) /*d Flags erhalten */ /*e preserve flags */
		| AST_VALUE_ID;
	AV_ID(node) = symtab->id;
	node->sym = symtab;
}

//d fuer CLASSDEF und FUNDEF:
//e for CLASSDEF and FUNDEF:
static void
fix_with_parameters(ast_node_t *cnode, hashtable_t *env, int child_flags_params, int child_flags_body, storage_record_t *counters, int *classes_nr)
{
	symtab_entry_t *syminfo = cnode->children[0]->sym;
	assert(syminfo);

	env = hashtable_clone(env, NULL, NULL);	

	//d CLASSDEF und FUNDEF haben analoge Struktur für Parameter:
	//e CLASSDEF and FUNDEF use an analogous structure for parameters:
	ast_node_t *formals = cnode->children[1];

	fixnames_recursive(formals, env, syminfo, (child_flags_params & ~SYMTAB_MEMBER) | SYMTAB_PARAM, &syminfo->storage, classes_nr);
	syminfo->parameters_nr = syminfo->storage.vars_nr;
	syminfo->storage.vars_nr = 0;

	syminfo->parameter_types = malloc(sizeof(unsigned short) * syminfo->parameters_nr);
	for (int i = 0; i < syminfo->parameters_nr; i++) {
		ast_node_t *child = formals->children[i];
		if (child != NULL) {
			syminfo->parameter_types[i] = child->type & ~AST_NODE_MASK;
		}
	}

	fixnames(cnode->children[2], env, syminfo, child_flags_body, &syminfo->storage, classes_nr);
#if 0	
	ast_node_dump(stderr, cnode, 6);
	fprintf(stderr, "params = %d, vars = %d, temps = %d, fields = %d, functions = %d\n", syminfo->parameters_nr, syminfo->storage.vars_nr, syminfo->storage.temps_nr, syminfo->storage.fields_nr, syminfo->storage.functions_nr);
#endif

	hashtable_free(env, NULL, NULL);
}

static void
fixnames(ast_node_t *node, hashtable_t *env, symtab_entry_t *parent, int child_flags, storage_record_t *counters, int *classes_nr)
{
	symtab_entry_t *lookup;
	if (!node) {
		return;
	}

	bool node_is_part_of_class_decl = child_flags & NF_PART_OF_CLASSDECL;

	if (child_flags & NF_NEED_STORAGE) {
		node->storage = counters->temps_nr;
	} else {
		node->storage = -1;
	}
	
	child_flags &= ~(NF_PART_OF_CLASSDECL | NF_NEED_STORAGE);

	/* fprintf(stderr, ">> "); */
	/* ast_node_dump(stderr, node, 8); */
	/* fprintf(stderr, "\n"); */

	switch (NODE_TY(node)) {

	case AST_VALUE_ID:
		//d Fest eingebaute Funktion
		//e built-in functions only at this stage
		node->sym = symtab_lookup(AV_ID(node));
		break;

	case AST_VALUE_NAME:
		if (child_flags & NF_SELECTOR) {
			lookup = get_selector(node);
		} else {
			lookup = hashtable_get(env, AV_NAME(node));
		}
		if (!lookup) {
			error(node, "undefined name");
		} else {
			if (SYMTAB_IS_CONS_ARG(lookup)
			    && SYMTAB_TY(parent) == SYMTAB_TY_FUNCTION) {
				error(node, "must not reference constructor parameter in method body");
				return;
			}

			resolve_node(node, lookup);
		}
		break;

	case AST_NODE_FUNDEF:
		++counters->functions_nr;
		//d Definitionen werden im umgebenden AST_NODE_BLOCK gemanaged
		//e surrounding AST_NODE_BLOCK handles definitions
		fix_with_parameters(node, env, (child_flags & ~SYMTAB_MEMBER) | SYMTAB_PARAM,
				    child_flags & ~SYMTAB_MEMBER, counters, classes_nr);
		node->children[0]->type |= AST_FLAG_DECL;
		return;
	case AST_NODE_CLASSDEF:
		++(*classes_nr);
		//d Definitionen werden im umgebenden AST_NODE_BLOCK gemanaged
		//e surrounding AST_NODE_BLOCK handles definitions
		fix_with_parameters(node, env, child_flags | SYMTAB_MEMBER | SYMTAB_PARAM,
				    child_flags | SYMTAB_MEMBER | NF_PART_OF_CLASSDECL, counters, classes_nr);
		node->children[0]->type |= AST_FLAG_DECL;
		return;

	case AST_NODE_FORMALS:
		fixnames_recursive(node, env, parent, child_flags | SYMTAB_PARAM, counters, classes_nr);
		return;

	case AST_NODE_VARDECL: {
		// Definition ist in der Initialisierung noch nicht sichtbar, also erst hierhin:
		fixnames(node->children[1], env, parent, child_flags, counters, classes_nr);
		ast_node_t *name_node = node->children[0];
		symtab_entry_t *selector_sym = NULL;
		if (child_flags & SYMTAB_MEMBER) {
			selector_sym = get_selector(name_node);
		}
		name_node->type |= AST_FLAG_DECL;

		// Wie oft haben wir diesen Namen schon definiert?
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
		/* if (child_flags & SYMTAB_MEMBER && parent) { */
		/* 	lookup->offset = parent->vars_nr++; */
		/* } else if (child_flags & SYMTAB_PARAM && parent) { // parent == NULL bei Namensfehlern möglich */
		/* 	lookup->offset = parent->parameters_nr++; */
		/* } */
		//e allocate memory offset
		if (child_flags & SYMTAB_MEMBER) {
			lookup->offset = counters->fields_nr++;
		} else {
			lookup->offset = counters->vars_nr++;
		}
		
		char *name = AV_NAME(name_node);
		resolve_node(node->children[0], lookup);
		hashtable_put(env, name, lookup, NULL);
		assert(lookup == hashtable_get(env, name));
		node->sym = lookup;


		if (selector_sym) {
			lookup->selector = selector_sym->selector;
		}
		return;
	}

	case AST_NODE_ACTUALS:
		fixnames_recursive(node, env, parent, child_flags | NF_NEED_STORAGE, counters, classes_nr); /*e must track temporaries */
		return;
		
	case AST_NODE_FUNAPP:
		if (NODE_TY(node->children[0]) == AST_NODE_MEMBER) {
			// METHODAPP (in type-analysis.c)
			if (node->storage < 0) {
				node->storage = counters->temps_nr++;
			}

			//e target
			fixnames(node->children[0], env, parent, child_flags | NF_NEED_STORAGE, counters, classes_nr);
			counters->temps_nr++;
			//e arguments
			fixnames_recursive(node->children[1], env, parent, child_flags | NF_NEED_STORAGE, counters, classes_nr);
			//e this ensures that the method target gets an extra spill slot
		} else {
			if (parent && node->storage < 0) {
				// Might be a method call; allocate spill space just in case
				node->storage = counters->temps_nr++;
			}
			fixnames_recursive(node, env, parent, child_flags, counters, classes_nr);
		}
		return;
		
	case AST_NODE_WHILE:
		child_flags |= NF_WITHIN_LOOP;
		break;

	case AST_NODE_CONTINUE:
		if (!(child_flags & NF_WITHIN_LOOP)) {
			error(node, "'continue' outside of loop");
		}
		break;

	case AST_NODE_BREAK:
		if (!(child_flags & NF_WITHIN_LOOP)) {
			error(node, "'break' outside of loop");
		}
		break;

	case AST_NODE_MEMBER:
		fixnames(node->children[0], env, parent, child_flags, counters, classes_nr);
		fixnames(node->children[1], env, parent, child_flags | NF_SELECTOR, counters, classes_nr);
		break;

	case AST_NODE_ASSIGN: {
		//d Als lvalue markieren
		/*e we compute the rhs first, so we mark it as possibly requiring storage */
		int base_temps = counters->temps_nr;
		fixnames(node->children[1], env, parent, child_flags | NF_NEED_STORAGE, counters, classes_nr);
		int max_temps = counters->temps_nr;
		counters->temps_nr = base_temps;
		fixnames(node->children[0], env, parent, child_flags, counters, classes_nr);
		if (max_temps > counters->temps_nr) {
			counters->temps_nr = max_temps;
		}
	}
		return;

	case AST_NODE_ARRAYSUB:
		fixnames(node->children[1], env, parent, child_flags, counters, classes_nr);
		fixnames(node->children[0], env, parent, child_flags, counters, classes_nr);
		return;

	case AST_NODE_ARRAYVAL:
		if (node->storage < 0) {
			node->storage = counters->temps_nr++;
		}
		fixnames(node->children[0], env, parent, child_flags, counters, classes_nr);
		fixnames(node->children[1], env, parent, child_flags, counters, classes_nr);
		return;

	case AST_NODE_BLOCK: {
		// Separater erster Durchlauf, um gegenseitige Rekursion Klassen/Funktionen zu erlauben
		hashtable_t *orig_env = env;
		int methods_nr = 0; // for class definitions only
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
				syminfo = symtab_new(cnode->type & ~AST_NODE_MASK,
						     SYMTAB_TY_FUNCTION | (child_flags & ~NF_SPECIAL_CHILD_FLAGS),
						     AV_NAME(name_node),
						     cnode);

				if (child_flags & SYMTAB_MEMBER) {
					syminfo->selector = get_selector(cnode->children[0])->selector;
					syminfo->offset = methods_nr++; //parent->storage.functions_nr++;
					syminfo->parent = parent;
				}
				break;
			}

			case AST_NODE_CLASSDEF: {
				name_node = cnode->children[0];
				if (parent) {
					error(name_node, "nested classes are not permitted");
				}
				syminfo = symtab_new(TYPE_OBJ,
						     SYMTAB_TY_CLASS | (child_flags & ~NF_SPECIAL_CHILD_FLAGS),
						     AV_NAME(name_node),
						     cnode);
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
		//d Verschachtelte Eintraege werden Teil des Konstruktors und somit keine Klassen-Elemente
		//e Nested entries become part of the constructor, so they're no class elements
		if (!node_is_part_of_class_decl) {
			child_flags &= ~SYMTAB_MEMBER;
		}
		
		fixnames_recursive(node, env, parent,
				   child_flags,
				   counters, classes_nr);
		hashtable_free(env, NULL, NULL);
		node->storage = counters->temps_nr;
		return;
	}

	default:
		break;
	}

	fixnames_recursive(node, env, parent, child_flags, counters, classes_nr);
}

// puts initial set of global names into env and selectors table
static void
populate_initial_env(hashtable_t *env)
{
	if (!symtab_selectors_table) {
		symtab_selectors_table = hashtable_alloc(hashtable_pointer_hash, hashtable_pointer_compare, 5);
	}

	for (int i = 1; i <= symtab_entries_builtin_nr; i++) {
		symtab_entry_t *entry = symtab_lookup(-i);
		if (! (entry->symtab_flags & SYMTAB_HIDDEN)) {
			if (entry->symtab_flags & SYMTAB_SELECTOR) {
				hashtable_put(symtab_selectors_table, entry->name, entry, NULL);
			} else {
				hashtable_put(env, entry->name, entry, NULL);
			}
		}
	}
}

int
name_analysis(ast_node_t *node, storage_record_t *storage, int *classes_nr)
{
	error_count = 0;
	hashtable_t *initial_env = hashtable_alloc(hashtable_pointer_hash, hashtable_pointer_compare, 5);

	populate_initial_env(initial_env);

	fixnames(node, initial_env, NULL, 0, storage, classes_nr);
	hashtable_free(initial_env, NULL, NULL);

	return error_count;
}
