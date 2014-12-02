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

static hashtable_t *registry = NULL;

void
stackmap_init(void)
{
	if (registry) {
		stackmap_clear();
	}
	registry = hashtable_alloc(hashtable_pointer_hash, hashtable_pointer_compare, 10);
}

void
stackmap_clear(void)
{
	if (registry) {
		hashtable_free(registry, NULL, (void (*)(void *)) bitvector_free);
		registry = NULL;
	}
}

void
stackmap_put(void *address, bitvector_t bitvector)
{
	void *value = (void *) bitvector.large;
	hashtable_put(registry, address, value, (void (*)(void *)) bitvector_free);
}

bool
stackmap_get(void *address, bitvector_t *bitvector)
{
	void **ref = hashtable_access(registry, address, NULL);
	if (!ref) {
		return false;
	}
	*bitvector = (bitvector_t) (unsigned long long *) (*ref);
	return true;
}


