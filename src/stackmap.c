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

#include "chash.h"
#include "stackmap.h"

cstack_t *debug_stack = NULL;
static hashtable_t *bitvector_registry = NULL;
static hashtable_t *symtab_registry = NULL;

void
stackmap_debug(cstack_t *stack)
{
	debug_stack = stack;
}

void
stackmap_init(void)
{
	if (bitvector_registry) {
		stackmap_clear();
	}
	bitvector_registry = hashtable_alloc(hashtable_pointer_hash, hashtable_pointer_compare, 10);
	symtab_registry = hashtable_alloc(hashtable_pointer_hash, hashtable_pointer_compare, 10);
}

void
stackmap_clear(void)
{
	if (bitvector_registry) {
		hashtable_free(bitvector_registry, NULL, (void (*)(void *)) bitvector_free);
		hashtable_free(symtab_registry, NULL, NULL);
		bitvector_registry = NULL;
		symtab_registry = NULL;
	}
}

void
stackmap_put(void *address, bitvector_t bitvector, symtab_entry_t *entry)
{
	if (debug_stack) {
		stack_push(debug_stack, &address);
	}
	void *value = (void *) bitvector.large;
	hashtable_put(bitvector_registry, address, value, (void (*)(void *)) bitvector_free);
	hashtable_put(symtab_registry, address, (void *) entry, NULL);
}

bool
stackmap_get(void *address, bitvector_t *bitvector, symtab_entry_t **entry)
{
	void **ref = hashtable_access(bitvector_registry, address, NULL);
	if (!ref) {
		return false;
	}
	*bitvector = (bitvector_t) (unsigned long long *) (*ref);
	*entry = hashtable_get(symtab_registry, address);
	return true;
}


