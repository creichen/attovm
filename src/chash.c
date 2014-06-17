/***************************************************************************
  Copyright (C) 2013 Christoph Reichenbach


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
#include <strings.h>
#include <string.h>
#include <stdio.h>

int ffs(int i);


#include "chash.h"

typedef struct {
	void *key, *value;
	unsigned long hashv;
} entry_t;

typedef struct hashtable {
	hash_fn_t hash_fn;
	compare_fn_t compare_fn;
	size_t used_size;
	size_t allocd_size_minus_1;  // Garantiert 0xffffffff >> x fuer irgendein x
	entry_t *entries;
} hashtable_t;


static void
alloc_at_size(hashtable_t *tbl, int size)
{
	tbl->allocd_size_minus_1 = size - 1;
	tbl->entries = calloc(sizeof(entry_t), size);
}

static void
resize(hashtable_t *tbl)
{
	entry_t *old_entries = tbl->entries;
	const size_t old_size = tbl->allocd_size_minus_1;
	alloc_at_size(tbl, (old_size+1) << 2); // size *= 4
	for (size_t i = 0 ; i <= old_size; i++) {
		const entry_t *entry = old_entries + i;
		if (entry->key) {
			hashtable_access(tbl, entry->key, entry->value);
		}
	}
	free(old_entries);
}

hashtable_t *
hashtable_alloc(hash_fn_t hash_fn, compare_fn_t compare_fn, unsigned char size_exponent)
{
	hashtable_t *tbl = malloc(sizeof(hashtable_t));
	alloc_at_size(tbl, 1 << size_exponent);
	tbl->hash_fn = hash_fn;
	tbl->compare_fn = compare_fn;
	tbl->used_size = 0;
	return tbl;
}

void
hashtable_free(hashtable_t *tbl, void (*free_key)(void *), void (*free_value)(void *))
{
	entry_t *entries = tbl->entries;

	if (free_key != NULL || free_value != NULL){
		for (int i = 0; i <= tbl->allocd_size_minus_1; i++) {
			const entry_t *entry = entries + i;
			if (entry->key) {
				if (free_key) {
					free_key(entry->key);
				}
				if (free_value) {
					free_value(entry->value);
				}
			}
		}
	}

	tbl->allocd_size_minus_1 = 0;
	tbl->entries = NULL;
	free(tbl);
	free(entries);
}

// djb2, by Dan Bernstein
static unsigned long
_string_hash(const char *str_)
{
	char *str = (char *) str_;
	unsigned long hash = 5381;
	int c;

	while ((c = *str++)) {
		hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
	}

	return hash;
}

static unsigned long
_pointer_hash(void *ptr)
{
	return (long) ptr;
}

static int
_pointer_compare(const void *a, const void *b)
{
	return ((long) a) - ((long) b);
}

hash_fn_t hashtable_string_hash = (hash_fn_t) _string_hash;
hash_fn_t hashtable_pointer_hash = (hash_fn_t) _pointer_hash;
compare_fn_t hashtable_pointer_compare = (compare_fn_t) _pointer_compare;

void **
hashtable_access(hashtable_t *tbl, void *key, void *value)
{
	const size_t mask = tbl->allocd_size_minus_1;
	unsigned long hash = tbl->hash_fn(key);
	size_t offset = hash & mask;
	entry_t *entries = tbl->entries;

	int retries = OPEN_ADDRESS_LINEAR_LENGTH;

	do {
		entry_t *entry = entries + offset;
		if (!entry->key) {
			// since we aren't allowed to delete keys, this means that the entry doesn't exist
			if (value) {
				entry->hashv = hash;
				entry->key = key;
				entry->value = value;
			}
			return NULL;
		}
		if (entry->hashv == hash && 0 == tbl->compare_fn(entry->key, key)) {
			// match
			return &entry->value;
		}
		offset = (offset + 1) % mask;
	} while (retries--);
	// no match or no space

	if (value) {
		// wanted to insert?  Need to re-size
		resize(tbl);
		return hashtable_access(tbl, key, value);
	}

	return NULL; // no match
}

void
hashtable_foreach(hashtable_t *tbl, void (*visit)(void *key, void *value, void *state), void *state)
{
	const entry_t *entries = tbl->entries;
	const size_t size = tbl->allocd_size_minus_1;
	for (size_t i = 0; i <= size; i++) {
		const entry_t *entry = entries + i;
		if (entry->key) {
			(*visit)(entry->key, entry->value, state);
		}
	}
}

void
hashtable_put(hashtable_t *tbl, void *key, void *value, void (*free_value)(void *))
{
	void **result = hashtable_access(tbl, key, value);

	if (result) {
		if (free_value) {
			free_value(*result);
		}
		*result = value;
	}
}

void *
hashtable_get(hashtable_t *tbl, void *key)
{
	void **result = hashtable_access(tbl, key, NULL);
	if (result) {
		return *result;
	}
	return NULL;
}

struct clone_state {
	void *(*clone_key)(const void *);
	void *(*clone_value)(const void*);
	hashtable_t *tbl;
};


void hashtable_clone_helper(void *key, void *value, void *state)
{
	struct clone_state *cstate = (struct clone_state *)state;

	if (cstate->clone_key) {
		key = cstate->clone_key(key);
	}
	if (cstate->clone_value) {
		value = cstate->clone_value(value);
	}
	hashtable_access(cstate->tbl, key, value);
}

hashtable_t *
hashtable_clone(hashtable_t *tbl, void *(*clone_key)(const void *), void *(*clone_value)(const void*))
{
	struct clone_state cstate;
	cstate.clone_key = clone_key;
	cstate.clone_value = clone_value;
	int size = ffs(tbl->used_size);
	if (size < 4) {
		size = 4;
	}
	cstate.tbl = hashtable_alloc(tbl->hash_fn, tbl->compare_fn, size);
	hashtable_foreach(tbl, hashtable_clone_helper, &cstate);
	return cstate.tbl;
}
