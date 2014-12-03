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
#include <string.h>

#include "bitvector.h"

#define CHECK_BOUNDS

#ifdef CHECK_BOUNDS
#  define BOUNDS_CHECK(bv, i) assert((i) < bitvector_size(bv))
#else
#  define BOUNDS_CHECK(bv, i)
#endif

bitvector_t
bitvector_alloc(size_t size)
{
	bitvector_t v;
	if (size <= BITVECTOR_SMALL_MAX) {
		v.small = (size << BITVECTOR_SIZE_SHIFT) | 1ull;
	} else {
		v.large = calloc(1 + ((size + BITVECTOR_SIZE_MASK) >> 6), 8);
		v.large[0] = size;
	}
	return v;
}

bitvector_t
bitvector(size_t size, unsigned long long v)
{
	if (size < BITVECTOR_SMALL_MAX) {
		return (bitvector_t){ .small = (v << BITVECTOR_BODY_SHIFT) | (size << BITVECTOR_SIZE_SHIFT) | 1 };
	}
	
	bitvector_t bv = bitvector_alloc(size);
	for (int i = 0; i < size; i++) {
		if ((v >> i) & 1) {
			bv = BITVECTOR_SET(bv, i);
		}
	}
	return bv;
}

size_t
bitvector_size(bitvector_t bitvector)
{
	if (BITVECTOR_IS_SMALL(bitvector)) {
		return (bitvector.small >> BITVECTOR_SIZE_SHIFT) & BITVECTOR_SIZE_MASK;
	} else {
		return (bitvector.large[0]);
	}
}

void
bitvector_free(bitvector_t bitvector)
{
	if (BITVECTOR_IS_SMALL(bitvector)) {
	} else {
		free(bitvector.large);
	}
}

bitvector_t
bitvector_clone(bitvector_t bitvector)
{
	if (BITVECTOR_IS_SMALL(bitvector)) {
		return bitvector;
	} else {
		size_t size = bitvector_size(bitvector);
		bitvector_t clone = bitvector_alloc(size);
		memcpy(&(clone.large[1]), &(bitvector.large[1]), 8 * ((size + BITVECTOR_SIZE_MASK) >> 6));
		return clone;
	}
}

bitvector_t
bitvector_set_slow(bitvector_t bitvector, size_t pos)
{
	BOUNDS_CHECK(bitvector, pos);
	if (BITVECTOR_IS_SMALL(bitvector)) {
		bitvector.small |= 1ull << (BITVECTOR_BODY_SHIFT + pos);
		return bitvector;
	} else {
		(bitvector.large[1 + (pos >> 6)] |= 1ull << (pos & 63));
		return bitvector;
	}
}

bitvector_t
bitvector_clear_slow(bitvector_t bitvector, size_t pos)
{
	BOUNDS_CHECK(bitvector, pos);
	if (BITVECTOR_IS_SMALL(bitvector)) {
		bitvector.small &= ~(1ull << (BITVECTOR_BODY_SHIFT + pos));
		return bitvector;
	} else {
		(bitvector.large[1 + (pos >> 6)] &= ~(1ull << (pos & 63)));
		return bitvector;
	}
}

bool
bitvector_is_set_slow(bitvector_t bitvector, size_t pos)
{
	BOUNDS_CHECK(bitvector, pos);
	if (BITVECTOR_IS_SMALL(bitvector)) {
		return (bitvector.small >> (BITVECTOR_BODY_SHIFT + pos)) & 1ull;
	} else {
		return (bitvector.large[1 + (pos >> 6)] >> (pos & 63)) & 1ull;
	}
}

void
bitvector_print(FILE *f, bitvector_t bitvector)
{
	for (int i = 0; i < bitvector_size(bitvector); i++) {
		fprintf(f, "%llu", BITVECTOR_IS_SET(bitvector, i));
	}
}
