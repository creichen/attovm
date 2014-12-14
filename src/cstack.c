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

struct cstack {
	size_t element_size;
	size_t capacity;
	size_t tos;
	unsigned char *data;
};

cstack_t *
stack_alloc(size_t element_size, size_t initial_capacity)
{
	cstack_t *stack = malloc(sizeof(cstack_t));
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
stack_size(cstack_t *stack)
{
	return stack->tos;
}

void *
stack_pop(cstack_t *stack)
{
	if (stack->tos) {
		--stack->tos;
		void *datum = stack->data + stack->element_size * stack->tos;
		return datum;
	} else {
		return NULL;
	}
}

void *
stack_get(cstack_t *stack, size_t index)
{
	if (index >= stack->tos) {
		return NULL;
	}
	return stack->data + (index * stack->element_size);
}

cstack_t *
stack_clone(cstack_t *stack, void (*clone)(void *))
{
	cstack_t *duplicate = stack_alloc(stack->element_size, stack->tos + 1);
	memcpy(duplicate->data, stack->data, stack->element_size * stack->tos);
	duplicate->tos = stack->tos;
	if (clone) {
		for (int i = 0; i < duplicate->tos; i++) {
			clone(stack_get(duplicate, i));
		}
	}
	return duplicate;
}

void
stack_push(cstack_t *stack, void *element)
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
stack_clear(cstack_t *stack, void (*element_free)(void *))
{
	if (element_free) {
		void *d;
		while ((d = stack_pop(stack))) {
			element_free(d);
		}
	}
	stack->tos = 0;
}

void
stack_free(cstack_t *stack, void (*element_free)(void *))
{
	stack_clear(stack, element_free);
	free(stack->data);
	free(stack);
}

void
stack_print(FILE *file, cstack_t *stack, void (*print)(FILE *, void *))
{
	fprintf(file, "[");
	for (int i = 0; i < stack_size(stack); i++) {
		if (i > 0) {
			fprintf(file, ", ");
		}
		print(file, stack_get(stack, i));
	}
	fprintf(file, "]");
}
