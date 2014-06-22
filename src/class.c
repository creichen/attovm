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

#include "assert.h"
#include "class.h"
#include "symbol-table.h"
#include "address-store.h"

int ffs(int i);


class_t *
class_new(symtab_entry_t *entry)
{
	assert(entry->symtab_flags & SYMTAB_TY_CLASS);
	const int members = entry->methods_nr + entry->vars_nr;
	int size = 1 << ffs(members * 3);
	class_t *classref = calloc(1, sizeof(class_t) + sizeof(class_member_t) * size);
	classref->table_mask = size - 1;

	addrstore_put(classref, ADDRSTORE_KIND_TYPE, entry->name);

	return class_initialise_and_link(classref, entry);
}

void
class_add_selector(class_t *classref, symtab_entry_t *selector_impl)
{
	assert(selector_impl != NULL);
	size_t index = selector_impl->id & classref->table_mask;
	while (classref->members[index].selector_id) {
		index = (index + 1) & classref->table_mask;
	}
	classref->members[index].selector_id = selector_impl->selector;
	classref->members[index].symbol = selector_impl;
}

class_t *
class_initialise_and_link(class_t *classref, symtab_entry_t *entry)
{
	classref->id = entry;
	entry->r_mem = classref;

	if (entry->astref) {
		for (int i = 0; i < entry->astref->children_nr; i++) {
			assert(NODE_TY(entry->astref->children[i]) == AST_NODE_FUNDEF
			       || NODE_TY(entry->astref->children[i]) == AST_NODE_VARDECL);
			class_add_selector(classref,
					   (symtab_entry_t *) entry->astref->children[i]->children[1]->sym);
		}
	}
	return classref;
}
