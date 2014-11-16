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

#include <string.h>
#include "cstack.h"

#define MAX_GROWTH_BYTES (1024 * 1024 * 8) // grow by at most 8 MB at a time
#define DEFAULT_INITIAL_CAPACITY 16

struct stack {
	size_t element_size;
	size_t capacity;
	size_t tos;
	unsigned char *data;
};

stack_t *
stack_alloc(size_t element_size, size_t initial_capacity)
{
	stack_t *stack = malloc(sizeof(stack_t));
	if (!initial_capacity) {
		initial_capacity = DEFAULT_INITIAL_CAPACITY;
	}
	stack->data = malloc(initial_capacity * element_size);
	stack->tos = 0;
	stack->element_size = element_size;
	stack->capacity = initial_capacity;
	return stack;
}

size_t
stack_size(stack_t *stack)
{
	return stack->tos;
}

void *
stack_pop(stack_t *stack)
{
	if (stack->tos) {
		--stack->tos;
		void *datum = stack->data + stack->element_size * stack->tos;
		return datum;
	} else {
		return NULL;
	}
}

void
stack_push(stack_t *stack, void *element)
{
	++stack->tos;
	if (stack->tos == stack->capacity) {
		size_t old_capacity = stack->capacity;
		size_t growth = stack->capacity;
		if (growth * stack->element_size > MAX_GROWTH_BYTES) {
			growth = MAX_GROWTH_BYTES / stack->element_size;
			if (growth == 0) {
				growth = 1;
			}
		}
		stack->capacity += growth;
		unsigned char *olddata = stack->data;
		stack->data = malloc(stack->capacity * stack->element_size);
		memcpy(stack->data, olddata, old_capacity * stack->element_size);
		free(olddata);
	}
	memcpy(stack->data + stack->element_size * (stack->tos - 1), element, stack->element_size);
	return;
}

void
stack_free(stack_t *stack, void (*element_free)(void *))
{
	if (element_free) {
		void *d;
		while ((d = stack_pop(stack))) {
			element_free(d);
		}
	}
	free(stack->data);
	free(stack);
}
