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
hash_fn_t hashtable_long_hash = (hash_fn_t) _string_hash;

compare_fn_t hashtable_pointer_compare = (compare_fn_t) _pointer_compare;
compare_fn_t hashtable_long_compare = (compare_fn_t) _pointer_compare;

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


// ================================================================================

struct hashset_ptr {
	hashtable_t *ht;
	size_t size;
};
	
hashset_ptr_t *
hashset_ptr_alloc()
{
	hashset_ptr_t *set = malloc(sizeof(hashset_ptr_t));
	set->ht = hashtable_alloc(hashtable_pointer_hash, hashtable_pointer_compare, 4);
	set->size = 0;
	return set;
}

void
hashset_ptr_add(hashset_ptr_t *set, void *ptr)
{
	void **access = hashtable_access(set->ht, ptr, (void *)1);
	if (!access) {
		++set->size;
	}
}

void
hashset_ptr_remove(hashset_ptr_t *set, void *ptr)
{
	void **access = hashtable_access(set->ht, ptr, NULL);
	if (!access || !*access) {
		return;
	}
	*access = NULL;
	--set->size;
}

bool
hashset_ptr_contains(hashset_ptr_t *set, void *ptr)
{
	return NULL != hashtable_get(set->ht, ptr);
}	

size_t
hashset_ptr_size(hashset_ptr_t *set)
{
	return set->size;
}

static void
_hashset_ptr_add_one(void *elt, hashset_ptr_t *set)
{
	hashset_ptr_add(set, elt);
}

void
hashset_ptr_add_all(hashset_ptr_t *set, hashset_ptr_t *to_add)
{
	hashset_ptr_foreach(to_add, (void (*)(void *ptr, void *state)) _hashset_ptr_add_one, set);
}

static void
_hashset_ptr_remove_one(void *elt, hashset_ptr_t *set)
{
	hashset_ptr_remove(set, elt);
}

void
hashset_ptr_remove_all(hashset_ptr_t *set, hashset_ptr_t *to_remove)
{
	hashset_ptr_foreach(to_remove, (void (*)(void *ptr, void *state)) _hashset_ptr_remove_one, set);
}

struct hashset_ptr_pair {
	hashset_ptr_t *l, *r;
};

static void
_hashset_ptr_remove_unless_in_r(void *elt, struct hashset_ptr_pair *pair)
{
	if (!hashset_ptr_contains(pair->r, elt)) {
		hashset_ptr_remove(pair->l, elt);
	}
}

void
hashset_ptr_retain_common(hashset_ptr_t *set, hashset_ptr_t *other_set)
{
	struct hashset_ptr_pair pair = { .l = set, .r = other_set };
	hashset_ptr_foreach(set, (void (*)(void *ptr, void *state)) _hashset_ptr_remove_unless_in_r, &pair);
}

hashset_ptr_t *
hashset_ptr_clone(hashset_ptr_t *set)
{
	hashset_ptr_t *cloneset = malloc(sizeof(hashset_ptr_t));
	cloneset->ht =  hashtable_clone(set->ht, NULL, NULL);
	cloneset->size = set->size;
	return cloneset;
}

struct hashset_visitor_state {
	void (*f)(void *ptr, void *state);
	void *fstate;
};

static void
hashset_ptr_table_visit(void *key, void *value, void *_state)
{
	struct hashset_visitor_state *state = (struct hashset_visitor_state *) _state;
	if (value) {
		state->f(key, state->fstate);
	}
}

void
hashset_ptr_foreach(hashset_ptr_t *set, void (*f)(void *ptr, void *state), void *state)
{
	struct hashset_visitor_state vstate = { .f = f, .fstate = state };
	hashtable_foreach(set->ht, hashset_ptr_table_visit, &vstate);
}

void
hashset_ptr_free(hashset_ptr_t *set)
{
	hashtable_free(set->ht, NULL, NULL);
	free(set);
}

struct print_state {
	FILE *file;
	void (*p)(FILE *, void *);
	bool is_first;
};

static void
print_one_ptr(void *ptr, void *_state)
{
	struct print_state *state = (struct print_state *)_state;
	if (!state->is_first) {
		fprintf(state->file, ", ");
	}
	state->p(state->file, ptr);
	state->is_first = false;
}

static void
print_ptr(FILE *f, void *v)
{
	fprintf(f, "%p", v);
}

void
hashset_ptr_print(FILE *f, hashset_ptr_t *set, void (*print_element)(FILE *, void *))
{
	fprintf(f, "{ ");
	if (print_element == NULL) {
		print_element = print_ptr;
	}
	struct print_state print_state = { .file = f, .p = print_element, .is_first = true };
	hashset_ptr_foreach(set, print_one_ptr, &print_state);
	if (!print_state.is_first) {
		fprintf(f, " ");
	}
	fprintf(f, "}");
}


// ================================================================================

struct hashset_long
{
	hashtable_t ht;
};

#define INVERT(x) (0x8000000000000000l ^ x)

hashset_long_t *
hashset_long_alloc()
{
	return (hashset_long_t *) hashset_ptr_alloc();
}

void
hashset_long_add(hashset_long_t *set, long v)
{
	hashset_ptr_add((hashset_ptr_t *) set, (void *) INVERT(v));
}

void
hashset_long_remove(hashset_long_t *set, long v)
{
	hashset_ptr_remove((hashset_ptr_t *) set, (void *) INVERT(v));
}

bool
hashset_long_contains(hashset_long_t *set, long v)
{
	return hashset_ptr_contains((hashset_ptr_t *) set, (void *) INVERT(v));
}

size_t
hashset_long_size(hashset_long_t *set)
{
	return hashset_ptr_size((hashset_ptr_t *) set);
}

void
hashset_long_add_all(hashset_long_t *set, hashset_long_t *to_add)
{
	hashset_ptr_add_all((hashset_ptr_t *) set, (hashset_ptr_t *) to_add);
}

void
hashset_long_remove_all(hashset_long_t *set, hashset_long_t *to_remove)
{
	hashset_ptr_remove_all((hashset_ptr_t *) set, (hashset_ptr_t *) to_remove);
}


void
hashset_long_retain_common(hashset_long_t *set, hashset_long_t *other_set)
{
	hashset_ptr_retain_common((hashset_ptr_t *) set, (hashset_ptr_t *) other_set);
}


hashset_long_t *
hashset_long_clone(hashset_long_t *set)
{
	return (hashset_long_t *) hashset_ptr_clone((hashset_ptr_t *) set);
}

struct hashset_long_visit_state {
	void (*f)(long, void *);
	void *fstate;
};

static void
_hashset_long_visit(void *ptr, void *_state)
{
	struct hashset_long_visit_state *state = (struct hashset_long_visit_state *) state;
	long inverted_v = (long) ptr;
	state->f(INVERT(inverted_v), state->fstate);
}

void
hashset_long_foreach(hashset_long_t *set, void (*f)(long value, void *state), void *state)
{
	struct hashset_long_visit_state fstate = { .f = f, .fstate = state };
	hashset_ptr_foreach((hashset_ptr_t *) set, (void (*)(void *, void *)) f, &fstate);
}

void
hashset_long_free(hashset_long_t *set)
{
	hashset_ptr_free((hashset_ptr_t *) set);
}

static void
print_long(FILE *f, long v)
{
	fprintf(f, "%ld", INVERT(v));
}

void
hashset_long_print(FILE *f, hashset_long_t *set)
{
	hashset_ptr_print(f, (hashset_ptr_t *) set, (void (*)(FILE *, void *)) print_long);
}
