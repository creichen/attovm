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

#include "errors.h"
#include "class.h"
#include "symbol-table.h"
#include "address-store.h"

class_t class_top;
class_t class_bottom;

class_t *
class_new(symtab_entry_t *entry)
{
	assert(entry->symtab_flags & SYMTAB_KIND_CLASS);
	int size = class_selector_table_size(entry->storage.functions_nr, entry->storage.fields_nr);
	class_t *classref = calloc(1, sizeof(class_t)
				   + sizeof(class_member_t) * size
				   // Virtuelle Methodentabelle
				   + (entry->storage.functions_nr * sizeof(void *)));
	classref->table_mask = size - 1;
	classref->object_map = bitvector_alloc(entry->storage.fields_nr);

	return class_initialise_and_link(classref, entry);
}

// Findet das signifikanteste gesetzte Bit
int
find_last_set(int number)
{
	int count = 0;
#define TESTMASK(MASK, SHIFT)			\
	if (number & MASK) {			\
		count += SHIFT;			\
		number >>= SHIFT;		\
	}

	TESTMASK(0xffff0000, 16);
	TESTMASK(0xff00, 8);
	TESTMASK(0xf0, 4);
	TESTMASK(0xc, 2);
	TESTMASK(0x2, 1);

#undef TESTMASK
	return count;
}

void
class_print(FILE *file, class_t *classref)
{
	int vtable_size = 0;
	if (!classref) {
		fprintf(file, "<classref=NULL>\n");
		return;
	}
	
	void **vtable = CLASS_VTABLE(classref);
	
	fprintf(file, "class ");
	symtab_entry_name_dump(file, classref->id);
	fprintf(file, "[%d] at %p {\n", classref->id->id, classref);
	fprintf(file, "\tsymbol-table-mask = %llx (size = %lld)\n", classref->table_mask, classref->table_mask + 1);
	for (int i = 0; i <= classref->table_mask; i++) {
		class_member_t *member = classref->members + i;
		fprintf(file, "\t%d:\t", i);
		int selector = CLASS_DECODE_SELECTOR_ID(member->selector_encoding);
		int offset = CLASS_DECODE_SELECTOR_OFFSET(member->selector_encoding);
		int type = CLASS_DECODE_SELECTOR_TYPE(member->selector_encoding);
		if (selector) {
			fprintf(file, "sel#%d ", selector);
			if (CLASS_MEMBER_IS_METHOD(type)) {
				if (offset >= vtable_size) {
					vtable_size = offset + 1;
				}
				fprintf(file, "method@%d(%d args) -> %p  (%s %s)",
					offset,
					CLASS_MEMBER_METHOD_ARGS_NR(type),
					vtable[offset],
					addrstore_get_prefix(vtable[offset]),
					addrstore_get_suffix(vtable[offset])
					);
			} else {
				fprintf(file, "variable@%d : ", offset);
				if (type == CLASS_MEMBER_VAR_OBJ) {
					fprintf(file, "obj");
				} else if (type == CLASS_MEMBER_VAR_INT) {
					fprintf(file, "int");
				} else {
					fprintf(file, "?unknown-type");
				}
			}
		}
		if (member->symbol) {
			fprintf(file, "\t[");
			symtab_entry_name_dump(file, member->symbol);
			fprintf(file, "]");
		}
		fprintf(file, "\n");
	}
	fprintf(file, "\tvtable[%d] = {\n", vtable_size);
	for (int i = 0; i < vtable_size; i++) {
		fprintf(file, "\t\t%p (%s %s)\n", vtable[i],
			addrstore_get_prefix(vtable[i]),
			addrstore_get_suffix(vtable[i])
			);
	}
	fprintf(file, "\t}\n");
	fprintf(file, "}");
}

int
class_selector_table_size(int methods_nr, int fields_nr)
{
	return 4 << find_last_set(((methods_nr) + (fields_nr)));
}

void
class_add_selector(class_t *classref, symtab_entry_t *selector_impl)
{
	assert(selector_impl != NULL);

	size_t index = selector_impl->selector & classref->table_mask;
	while (classref->members[index].selector_encoding) {
		index = (index + 1) & classref->table_mask;
	}
	int type_encoding;
	if (SYMTAB_KIND(selector_impl) == SYMTAB_KIND_VAR) {
		if (selector_impl->ast_flags & TYPE_INT) {
			type_encoding = CLASS_MEMBER_VAR_INT;
		} else {
			type_encoding = CLASS_MEMBER_VAR_OBJ;
		}
	} else if (SYMTAB_KIND(selector_impl) == SYMTAB_KIND_FUNCTION) {
		type_encoding = CLASS_MEMBER_METHOD(selector_impl->parameters_nr);
	} else {
		symtab_entry_dump(stderr, selector_impl);
		fail("Unexpected symbol type in class");
	}

	classref->members[index].selector_encoding =
		CLASS_ENCODE_SELECTOR(selector_impl->selector, selector_impl->offset, type_encoding);
	classref->members[index].symbol = selector_impl;
}

class_t *
class_initialise_and_link(class_t *classref, symtab_entry_t *entry)
{
	classref->id = entry;
	entry->r_mem = classref;
	addrstore_put(classref, ADDRSTORE_KIND_TYPE, entry->name);

	int definitions = 0;
	if (entry->astref) {
		ast_node_t *defs = entry->astref->children[2]; // BLOCK
		definitions = defs->children_nr;

		for (int i = 0; i < definitions; i++) {
			ast_node_t *child = defs->children[i];
			assert(NODE_TY(child) == AST_NODE_FUNDEF
			       || NODE_TY(child) == AST_NODE_VARDECL);
			class_add_selector(classref, AST_CALLABLE_SYMREF(child));

			if (NODE_TY(child) == AST_NODE_VARDECL
			    && SYMTAB_TYPE(AST_CALLABLE_SYMREF(child)) == TYPE_OBJ) {
				classref->object_map = BITVECTOR_SET(classref->object_map, i);
			}
		}
	}
	return classref;
}

/*e
 * Prints out a class (including one of the fake classes)
 */
void
class_print_short(FILE *file, class_t *classref)
{
	if (!classref) {
		fprintf(file, "-");
	} else if (classref == &class_top) {
		fprintf(file, "T");
	} else if (classref == &class_bottom) {
		fprintf(file, "_");
	} else {
		symtab_entry_name_dump(file, classref->id);
	}
}


symtab_entry_t *
class_lookup_member(symtab_entry_t *sym, int selector)
{
	if (!sym) {
		//e fake class?
		return NULL;
	}
	if (!sym->astref) {
		//e broken call or built-in symbol?
		//e expansions: may want to revise how we record built-in operations internally
		static int size_selector = 0;
		if (!size_selector) {
			//e remains constant throughout run-time:
			size_selector = symtab_selector("size")->selector;
		}
		
		if (sym->id == symtab_builtin_class_array) {
			if (selector == size_selector) {
				return symtab_lookup(symtab_builtin_method_array_size);
			}
		} else if (sym->id == symtab_builtin_class_string) {
			if (selector == size_selector) {
				return symtab_lookup(symtab_builtin_method_string_size);
			}
		}
		
		//e migth be built in, but this was not a known method
		return NULL;
	}
	ast_node_t *body = sym->astref->children[2];

	for (int i = 0; i < body->children_nr; i++) {
		symtab_entry_t *child_sym = body->children[i]->children[0]->sym;
		if (child_sym && child_sym->selector == selector) {
			return child_sym;
		}
	}
	return NULL;
}
