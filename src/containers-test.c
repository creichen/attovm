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

#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>
#include <stdarg.h>

#include "chash.h"
#include "cstack.h"
#include "bitvector.h"

jmp_buf fail_buf;
static int failures = 0;

void fail() {};

void
signal_failure()
{
	 ++failures;
	 printf("\033[1;31mFAILURE\033[0m\n");
}

void
signal_success()
{
	 printf("\033[1;32mOK\033[0m\n");
}

#define FAIL(msg)  { signal_failure(); if (msg) fprintf(stderr, "Failure at %s L%d: %s\n", __FILE__, __LINE__, msg); longjmp(fail_buf, 1); }
#define ASSERT(a) {if (!(a)) { signal_failure(); fprintf(stderr, "Failure at %s L%d: (%s) is false\n", __FILE__, __LINE__, # a); longjmp(fail_buf, 1); }}

#define TEST(test_name) { if (!setjmp(fail_buf)) { printf("Testing %s:\t", # test_name); test_name(); signal_success(); } }

static void
test_stack_basic(void)
{
	cstack_t *stack = stack_alloc(sizeof(int), 2);
	ASSERT(stack_size(stack) == 0);
	ASSERT(NULL == stack_pop(stack));
	ASSERT(stack_size(stack) == 0);
	int n = 1;
	stack_push(stack, &n);
	ASSERT(stack_size(stack) == 1);
	int *d = stack_pop(stack);
	ASSERT(d != NULL);
	ASSERT(*d == 1);
	ASSERT(stack_size(stack) == 0);
	ASSERT(NULL == stack_pop(stack));
	stack_free(stack, NULL);
}

static void
subfree(void **d)
{
	free(*d);
}

static void
test_stack_many(void)
{
	const int TEST_SIZE = 100;
	cstack_t *stack = stack_alloc(sizeof(long *), 10);
	for (int i = 0; i < TEST_SIZE; i++) {
		long *v = malloc(sizeof(long));
		*v = i;
		ASSERT(stack_size(stack) == i);
		stack_push(stack, &v);
		ASSERT(stack_size(stack) == i + 1);
	}
	for (int i = TEST_SIZE; i > 100; i--) {
		ASSERT(stack_size(stack) == i);
		long *v = stack_pop(stack);
		ASSERT(v != NULL);
		ASSERT(*v == (i - 1));
		ASSERT(stack_size(stack) == i - 1);
	}

	stack_free(stack, (void (*)(void *)) subfree);
}

static void
test_stack_big(void)
{
	const int TEST_SIZE = 100;
	cstack_t *stack = stack_alloc(64, 2);

	for (int i = 0; i < TEST_SIZE; i++) {
		unsigned char d[64];
		ASSERT(stack_size(stack) == i);
		for (int k = 0; k < 64; k++) {
			d[k] = i + k;
		}

		stack_push(stack, d);
	}

	for (int i = TEST_SIZE; i > 0; i--) {
		ASSERT(stack_size(stack) == i);
		unsigned char *d = stack_pop(stack);
		
		for (int k = 0; k < 64; k++) {
			ASSERT(d[k] == i + k - 1);
		}
		ASSERT(d != NULL);
	}
	ASSERT(stack_pop(stack) == NULL);
	ASSERT(stack_size(stack) == 0);

	stack_free(stack, NULL);
}

static void
test_lhash_basic()
{
	hashset_long_t *hl = hashset_long_alloc();
	ASSERT(hl);
	ASSERT(0 == hashset_long_size(hl));
	ASSERT(!hashset_long_contains(hl, 2));
	hashset_long_add(hl, 2);
	ASSERT(hashset_long_contains(hl, 2));
	
	hashset_long_add(hl, 2);
	ASSERT(1 == hashset_long_size(hl));
	ASSERT(hashset_long_contains(hl, 2));
	
	hashset_long_remove(hl, 1);
	ASSERT(1 == hashset_long_size(hl));
	hashset_long_remove(hl, 2);
	ASSERT(!hashset_long_contains(hl, 2));
	ASSERT(!hashset_long_contains(hl, 0));
	ASSERT(0 == hashset_long_size(hl));

	ASSERT(!hashset_long_contains(hl, 0));
	ASSERT(!hashset_long_contains(hl, -1));
	hashset_long_add(hl, 0);
	hashset_long_add(hl, -1);
	ASSERT(hashset_long_size(hl) == 2);
	ASSERT(hashset_long_contains(hl, 0));
	ASSERT(hashset_long_contains(hl, -1));
	
	hashset_long_free(hl);
}

static void
test_lhash_many()
{
	const long MAX_SIZE = 1000;
	hashset_long_t *hl = hashset_long_alloc();
	for (long i = 0; i < MAX_SIZE; ++i) {
		ASSERT(hashset_long_size(hl) == i);
		ASSERT(!hashset_long_contains(hl, i*i));
		hashset_long_add(hl, i*i);
		/* fprintf(stderr, "Added %ld: ", i*i); */
		/* hashset_long_print(stderr, hl); */
		/* fprintf(stderr, "\n"); */
		ASSERT(hashset_long_contains(hl, i*i));
	}
	for (long i = MAX_SIZE - 1; i >= 0; --i) {
		ASSERT(hashset_long_size(hl) == i + 1);
		/* fprintf(stderr, "Checking for %ld: ", i*i); */
		/* hashset_long_print(stderr, hl); */
		/* fprintf(stderr, "\n"); */
		ASSERT(hashset_long_contains(hl, i*i));
		hashset_long_remove(hl, i*i);
		ASSERT(!hashset_long_contains(hl, i*i));
	}
	hashset_long_free(hl);
}

static hashset_long_t *
lhash_create(int count, ...)
{
	hashset_long_t *hl = hashset_long_alloc();
	va_list vl;
	va_start(vl, count);
	while (count--) {
		int v = va_arg(vl, int);
		hashset_long_add(hl, v);
	}
	va_end(vl);
	return hl;
}

static void
test_lhash_interset(void)
{
	hashset_long_t *hl1 = lhash_create(3,
					   -9, 0, 2);
	hashset_long_t *hl3 = lhash_create(3,
					   1, 2, 3);

	hashset_long_t *hl;

	// intersection
	ASSERT(hashset_long_size(hl1) == 3);
	hl = hashset_long_clone(hl1);
	ASSERT(hashset_long_size(hl1) == 3);
	ASSERT(hashset_long_size(hl) == 3);
	ASSERT(hashset_long_contains(hl, 2));
	ASSERT(hashset_long_contains(hl, -9));
	ASSERT(hashset_long_contains(hl, 0));

	ASSERT(hashset_long_size(hl3) == 3);
	hashset_long_retain_common(hl, hl3);
	ASSERT(hashset_long_size(hl3) == 3);
	ASSERT(hashset_long_size(hl1) == 3);
	ASSERT(hashset_long_size(hl) == 1);
	ASSERT(hashset_long_contains(hl, 2));
	ASSERT(!hashset_long_contains(hl, -9));
	ASSERT(!hashset_long_contains(hl, 0));
	ASSERT(!hashset_long_contains(hl, 1));

	hashset_long_free(hl);

	// union
	ASSERT(hashset_long_size(hl1) == 3);
	hl = hashset_long_clone(hl1);
	ASSERT(hashset_long_size(hl1) == 3);
	ASSERT(hashset_long_size(hl) == 3);
	ASSERT(hashset_long_contains(hl, 2));
	ASSERT(hashset_long_contains(hl, -9));
	ASSERT(hashset_long_contains(hl, 0));

	ASSERT(hashset_long_size(hl3) == 3);
	hashset_long_add_all(hl, hl3);
	ASSERT(hashset_long_size(hl1) == 3);
	ASSERT(hashset_long_size(hl3) == 3);
	ASSERT(hashset_long_size(hl) == 5);
	ASSERT(hashset_long_contains(hl, -9));
	ASSERT(hashset_long_contains(hl, 0));
	ASSERT(hashset_long_contains(hl, 1));
	ASSERT(hashset_long_contains(hl, 2));
	ASSERT(hashset_long_contains(hl, 3));
	ASSERT(!hashset_long_contains(hl, 4));

	hashset_long_free(hl);

	// difference
	ASSERT(hashset_long_size(hl1) == 3);
	hl = hashset_long_clone(hl1);
	ASSERT(hashset_long_size(hl1) == 3);
	ASSERT(hashset_long_size(hl) == 3);
	ASSERT(hashset_long_contains(hl, 2));
	ASSERT(hashset_long_contains(hl, -9));
	ASSERT(hashset_long_contains(hl, 0));

	ASSERT(hashset_long_size(hl3) == 3);
	hashset_long_remove_all(hl, hl3);
	ASSERT(hashset_long_size(hl1) == 3);
	ASSERT(hashset_long_size(hl3) == 3);
	ASSERT(hashset_long_size(hl) == 2);
	ASSERT(hashset_long_contains(hl, -9));
	ASSERT(hashset_long_contains(hl, 0));
	ASSERT(!hashset_long_contains(hl, 1));
	ASSERT(!hashset_long_contains(hl, 2));
	ASSERT(!hashset_long_contains(hl, 3));
	ASSERT(!hashset_long_contains(hl, 4));

	hashset_long_free(hl);

	// end
	hashset_long_free(hl1);
	hashset_long_free(hl3);
}

static void
padd(long v, void *acc)
{
	*((long *)acc) += v;
}

static void
test_lhash_iterate(void)
{
	hashset_long_t *hl = lhash_create(10,
					  1, 9, 10, 5, 25,
					  100, 200, 300, 400,
					  10000);
	ASSERT(hashset_long_size(hl) == 10);
	long count = 0l;
	hashset_long_foreach(hl, padd, &count);
	ASSERT(count = 11050);
}

static void
test_bitvec_basic(void)
{
	bitvector_t b3 = bitvector_alloc(3);
	bitvector_t b0 = bitvector_alloc(0);
	ASSERT(0 == bitvector_size(b0));
	ASSERT(3 == bitvector_size(b3));

	bitvector_free(b0);

	for (int i = 0; i < 3; i++) {
		ASSERT(!BITVECTOR_IS_SET(b3, i));
		b3 = BITVECTOR_SET(b3, i);
		ASSERT(BITVECTOR_IS_SET(b3, i));
	}

	for (int i = 0; i < 3; i++) {
		ASSERT(BITVECTOR_IS_SET(b3, i));
		b3 = BITVECTOR_CLEAR(b3, i);
		ASSERT(!BITVECTOR_IS_SET(b3, i));
	}

	b3 = BITVECTOR_SET(b3, 1);
	ASSERT(!BITVECTOR_IS_SET(b3, 0));
	ASSERT(BITVECTOR_IS_SET(b3, 1));
	ASSERT(!BITVECTOR_IS_SET(b3, 2));

	bitvector_free(b3);
}

static void
test_bitvec_scale(void)
{
	unsigned int z = 17;
	for (int size = 0; size < 1024; size++) {
		z = (z * 3) + 1 + (size * size);
		unsigned int off = 0;
		if (size > 0) {
			off = z % size;
		}
		
		bitvector_t b = bitvector_alloc(size);

		for (int i = size - 1; i >= 0; i--) {
			ASSERT(!BITVECTOR_IS_SET(b, i));
			b = BITVECTOR_SET(b, i);
			ASSERT(BITVECTOR_IS_SET(b, i));
		}

		bitvector_t b2 = bitvector_clone(b);

		ASSERT(bitvector_size(b2) == size);

		for (int i = size - 1; i >= 0; i--) {
			ASSERT(BITVECTOR_IS_SET(b, i));
			b = BITVECTOR_CLEAR(b, i);
			ASSERT(!BITVECTOR_IS_SET(b, i));
		}

		for (int i = size - 1; i >= 0; i--) {
			ASSERT(BITVECTOR_IS_SET(b2, i));
			b2 = BITVECTOR_CLEAR(b2, i);
			ASSERT(!BITVECTOR_IS_SET(b2, i));
		}

		if (size > 0) {
			b = BITVECTOR_SET(b, off);
			for (int i = 0; i < size; i++) {
				ASSERT(!BITVECTOR_IS_SET(b2, i));
				if (i == off) {
					ASSERT(BITVECTOR_IS_SET(b, i));
				} else {
					ASSERT(!BITVECTOR_IS_SET(b, i));
				}
			}
		}

		ASSERT(bitvector_size(b) == size);

		bitvector_free(b);
		bitvector_free(b2);
	}
}

int
main(int argc, char **args)
{
	TEST(test_stack_basic);
	TEST(test_stack_many);
	TEST(test_stack_big);
	
	TEST(test_lhash_basic);
	TEST(test_lhash_many);
	TEST(test_lhash_interset);
	TEST(test_lhash_iterate);

	TEST(test_bitvec_basic);
	TEST(test_bitvec_scale);
	
	return failures != 0;
}
