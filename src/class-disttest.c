/***************************************************************************e
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

class_t *
class_new(symtab_entry_t *entry)
{
	assert(entry->symtab_flags & SYMTAB_TY_CLASS);
	int size = class_selector_table_size(entry->methods_nr, entry->vars_nr);
	class_t *classref = calloc(1, sizeof(class_t)
				   + sizeof(class_member_t) * size
				   // Virtuelle Methodentabelle
				   + (entry->methods_nr * sizeof(void *)));
	classref->table_mask = size - 1;

	return class_initialise_and_link(classref, entry);
}


//e german
//d Findet das signifikanteste gesetzte Bit
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
	if (SYMTAB_TY(selector_impl) == SYMTAB_TY_VAR) {
		if (selector_impl->ast_flags & TYPE_INT) {
			type_encoding = CLASS_MEMBER_VAR_INT;
		} else {
			type_encoding = CLASS_MEMBER_VAR_OBJ;
		}
	} else if (SYMTAB_TY(selector_impl) == SYMTAB_TY_FUNCTION) {
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
					   
		}
	}
	return classref;
}
