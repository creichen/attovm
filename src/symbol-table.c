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
#include <stdlib.h>

#include "ast.h"
#include "chash.h"
#include "lexer-support.h"
#include "symbol-table.h"

#define INITIAL_SIZE 128
#define INCREMENT 128

typedef symtab_entry_t *symbol_table_t;

int symtab_entries_nr = 0;
static int symtab_entries_size = 0;
static symbol_table_t *symtab_user = NULL;

int symtab_entries_builtin_nr = 0;
static int symtab_entries_builtin_size = 0;
static symbol_table_t *symtab_builtin = NULL;

int symtab_selectors_nr = 1; // selector #0 ist reserviert (Kein Eintrag/Fehler)
hashtable_t *symtab_selectors_table;	// Bildet Selektor-namen auf EINEM der passenden Symboltabelleneinträge ab (nur für Aufrufe!)

#define BUILTIN_TABLE -1
#define USER_TABLE 1

static void
symtab_get(int *id, int **ids_nr, int **alloc_size, symbol_table_t **symtab)
{
	if (*id < 0) {
		*id = (-*id) - 1;
		*symtab = symtab_builtin;
		*alloc_size = &symtab_entries_builtin_size;
		*ids_nr = &symtab_entries_builtin_nr;
	} else if (*id > 0) {
		*id = *id - 1;
		*symtab = symtab_user;
		*alloc_size = &symtab_entries_size;
		*ids_nr = &symtab_entries_nr;
	} else {
		*symtab = NULL; // no such table;
	}
}

symtab_entry_t *
symtab_selector(char *name)
{
	if (!symtab_selectors_table) {
		symtab_selectors_table = hashtable_alloc(hashtable_pointer_hash, hashtable_pointer_compare, 5);
	}
	name = mk_unique_string(name);

	symtab_entry_t *lookup = hashtable_get(symtab_selectors_table, name);
	if (lookup) {
		return lookup;
	}

	// Alloziere neuen Selektor
	lookup = symtab_new(0, // Selektoren haben keine eindeutigen Attribute
			    SYMTAB_SELECTOR,
			    name,
			    NULL /* Selektoren werden nicht deklariert */);
	lookup->selector = symtab_selectors_nr;

	hashtable_put(symtab_selectors_table, name, lookup, NULL);
	++symtab_selectors_nr;
	return lookup;
}

symtab_entry_t *
symtab_lookup(int id)
{
	int *ids_nr, *alloc_size;
	symbol_table_t *symtab;
	if (!id) {
		return NULL;
	}
	symtab_get(&id, &ids_nr, &alloc_size, &symtab);

	if (id >= *ids_nr) {
		fprintf(stderr, "Looking up invalid symtab ID %d (post-convesion)", id);
		return NULL;
	}
	if (id < 0) {
		return NULL;
	}
	return symtab[id];
}


symtab_entry_t *
symtab_new(int ast_flags, int symtab_flags, char *name, ast_node_t *declaration)
{
	const int nr = symtab_entries_nr++;
	const int id = nr + 1;
	if (nr >= symtab_entries_size) {
		symtab_entries_size += INCREMENT;
		symtab_user = realloc(symtab_user, sizeof (symtab_entry_t *) * symtab_entries_size);
	}

	symtab_entry_t *entry = calloc(sizeof(symtab_entry_t), 1);
	symtab_user[nr] = entry;
	entry->id = id;
	entry->ast_flags = ast_flags;
	entry->symtab_flags = symtab_flags & ~SYMTAB_BUILTIN;
	entry->name = name;
	entry->astref = declaration;

	return entry;
}


symtab_entry_t *
symtab_builtin_new(int id, int ast_flags, int symtab_flags, char *name)
{
	const int nr = (id)? -id -1 : symtab_entries_builtin_nr;

	while (nr >= symtab_entries_builtin_size) {
		symtab_entries_builtin_size += INCREMENT;
		symtab_builtin = realloc(symtab_builtin, sizeof (symtab_entry_t *) * symtab_entries_builtin_size);
	}
	if (nr >= symtab_entries_builtin_nr) {
		symtab_entries_builtin_nr = nr + 1;
	}

	symtab_entry_t *entry = calloc(sizeof(symtab_entry_t), 1);
	symtab_builtin[nr] = entry;
	entry->id = -nr - 1;
	entry->ast_flags = ast_flags;
	entry->symtab_flags = symtab_flags | SYMTAB_BUILTIN;
	entry->name = name;

	return entry;
}


void
symtab_entry_name_dump(FILE *file, symtab_entry_t *entry)
{
	if (entry->parent) {
		symtab_entry_name_dump(file, entry->parent);
		fputs(".", file);
	}
	fputs(entry->name, file);
	if (entry->occurrence_count) {
		fprintf(file, "$%d", entry->occurrence_count);
	}
}


void
symtab_entry_dump(FILE *file, symtab_entry_t *entry)
{
	bool has_args = false;
	bool is_class = false;
	bool is_function = false;
	bool is_var = false;

	if (!entry) {
		fprintf(stderr, "Entry is (null)");
		return;
	}

	fprintf(file, "#%d:\t", entry->id);
	symtab_entry_name_dump(file, entry);
	fprintf(file, "\n\tFlags:\t");
#define PRINT_SYMVAR(s)				\
	if (entry->symtab_flags & SYMTAB_ ## s)	\
		fputs(" " # s, file);

	PRINT_SYMVAR(HIDDEN);
	PRINT_SYMVAR(BUILTIN);
	PRINT_SYMVAR(SELECTOR);
	PRINT_SYMVAR(OPT);
	PRINT_SYMVAR(COMPILED);
	PRINT_SYMVAR(MEMBER);
	PRINT_SYMVAR(PARAM);
	PRINT_SYMVAR(CONSTRUCTOR);

	switch (SYMTAB_TY(entry)) {
	case 0: // should only happen for selectors
		fputs(" UNKNOWN", file);
		break;
	case SYMTAB_TY_VAR:
		fputs(" VARIABLE", file);
		is_var = true;
		break;
	case SYMTAB_TY_FUNCTION:
		fputs(" FUNCTION", file);
		has_args = true;
		is_function = true;
		break;
	case SYMTAB_TY_CLASS:
		fputs(" CLASS", file);
		has_args = true;
		is_class = true;
		break;
	default:
		fprintf(file, " ?ty=%d", SYMTAB_TY(entry));
		has_args = true;
		is_class = true;
		break;
	}
#undef PRINT_SYMVAR
	fputs(" ", file);
	ast_print_flags(file, entry->ast_flags);
	fputs("\n", file);

	if (entry->astref) {
		fprintf(file, "\tAST:\t");
		ast_node_dump(file, entry->astref, /*AST_NODE_DUMP_ADDRESS |*/ AST_NODE_DUMP_NONRECURSIVELY);
		fputs("\n", file);
	}

	if (has_args) {
		fprintf(file, "\tArgs:\t%d: ", entry->parameters_nr);
		for (int i = 0; i < entry->parameters_nr; i++) {
			fputs(" (", file);
			ast_print_flags(file, entry->parameter_types[i]);
			fputs(")", file);
		}
		fputs("\n", file);
	}

	if (is_class) {
		fprintf(file, "\tMethNr:\t%d\n", entry->storage.functions_nr);
		fprintf(file, "\tFldsNr:\t%d\n", entry->storage.fields_nr);
	}

	if (is_function) {
		fprintf(file, "\tVarsNr:\t%d\n", entry->storage.vars_nr);
		fprintf(file, "\tTmpsNr:\t%d\n", entry->storage.temps_nr);
	}

	if (entry->selector) {
		fprintf(file, "\tSelect:\t%d\n", entry->selector);		
	}

	fprintf(file, "\tOffset:\t%d\n", entry->offset);
	
	if (is_var) {
		fprintf(file, "\tStorage:\t");
		//e normally, precisely one of those should trigger: 
		if (SYMTAB_IS_STATIC(entry)) {
			fprintf(file, "static ");
		}
		if (entry->symtab_flags & SYMTAB_MEMBER) {
			fprintf(file, "heap-dynamic ");
		}
		if (SYMTAB_IS_STACK_DYNAMIC(entry)) {
			fprintf(file, "stack-dynamic ");
		}
		fprintf(file, "\n");
	}
}

static void
symtab_entry_free(symtab_entry_t *e)
{
	if (e->parameters_nr && !(e->symtab_flags & SYMTAB_BUILTIN)) {
		free(e->parameter_types);
	}
	free(e);
}

void
symtab_free()
{
	if (symtab_selectors_table) {
		hashtable_free(symtab_selectors_table, NULL, NULL);
		symtab_selectors_table = NULL;
		symtab_selectors_nr = 1;
	}
	
	if (symtab_user) {
		for (int i = 0; i < symtab_entries_nr; i++) {
			symtab_entry_free(symtab_user[i]);
			symtab_user[i] = NULL;
		}
		free(symtab_user);
	
		symtab_entries_nr = 0;
		symtab_entries_size = 0;
	}

#if 0
	if (symtab_builtin) {
		for (int i = 0; i < symtab_entries_builtin_nr; i++) {
			symtab_entry_free(symtab_builtin[i]);
			symtab_builtin[i] = NULL;
		}

		free(symtab_builtin);
		symtab_entries_builtin_nr = 0;
		symtab_entries_builtin_size = 0;
	}
#endif
}

void
symtab_init()
{
	symtab_user = calloc(sizeof(symtab_entry_t *), INITIAL_SIZE);
	symtab_entries_size = INITIAL_SIZE;

	static bool initialised_builtin = false;
	if (!initialised_builtin) {
		symtab_builtin = calloc(sizeof(symtab_entry_t *), INITIAL_SIZE);
		symtab_entries_builtin_size = INITIAL_SIZE;
		initialised_builtin = true;
	}
}
