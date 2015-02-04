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

#include <stdlib.h>
#include <stdio.h>
#include "address-store.h"
#include "chash.h"

#define KIND_MASK 0x7

static char *kind_names[] = {
	[ADDRSTORE_KIND_TYPE] = "type ",
	[ADDRSTORE_KIND_FUNCTION] = "&",
	[ADDRSTORE_KIND_BUILTIN] = "",
	[ADDRSTORE_KIND_SPECIAL] = "",
	[ADDRSTORE_KIND_DATA] = "",
	[ADDRSTORE_KIND_STRING_LITERAL] = "string ",
	[ADDRSTORE_KIND_TRAMPOLINE] = "",
	[ADDRSTORE_KIND_COUNTER] = "counter:"
};

static hashtable_t *name_table = NULL;;
static hashtable_t *kind_table = NULL;;

void
addrstore_put(void *addr, int kind, char *description)
{
	if (!name_table) {
		name_table = hashtable_alloc(hashtable_pointer_hash, hashtable_pointer_compare, 5);
		kind_table = hashtable_alloc(hashtable_pointer_hash, hashtable_pointer_compare, 5);
	}
	hashtable_put(name_table, addr, description, NULL);
	hashtable_put(kind_table, addr, kind_names[kind], NULL);
}

char *
addrstore_get_prefix(void *addr)
{
	if (!name_table) {
		return NULL;
	}
	return (char *) hashtable_get(kind_table, addr);
}

char *
addrstore_get_suffix(void *addr)
{
	if (!name_table) {
		return NULL;
	}
	return (char *) hashtable_get(name_table, addr);
}

