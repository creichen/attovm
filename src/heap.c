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

#define _BSD_SOURCE /*e activates MAP_ANONYMOUS */
#define _DARWIN_C_SOURCE /*e activates MAP_ANON */

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#ifndef MAP_ANONYMOUS
#  ifndef MAP_ANON
#    error "need MAP_ANONYMOUS or MAP_ANON"
#  endif
#  define MAP_ANONYMOUS MAP_ANON
#endif

#include "heap.h"
#include "cstack.h"

#define HEAP_START 0x10000000000 /*e Default heap memory start address */
#define PAGE_SIZE 0x1000

void *heap_root_frame_pointer = NULL; /*e initialised by runtime_execute() */

static unsigned char *heap_start = NULL;
static unsigned char *heap_end = NULL;
static unsigned char *heap_free_pointer = NULL;

void
heap_init(size_t heap_size)
{
	//e we allocate the heap only once:
	assert(!heap_start);

	//e Make sure that heap_size is a multiple of PAGE_SIZE, as most systems require that
	if (heap_size & (PAGE_SIZE - 1)) {
		heap_size = (heap_size + PAGE_SIZE) & ~(PAGE_SIZE -1);
	}

	heap_start = mmap((void *) HEAP_START,
			    heap_size,
			    PROT_READ | PROT_WRITE,
			    MAP_PRIVATE | MAP_ANONYMOUS,
			    -1,
			    0);
	if (heap_start == MAP_FAILED) {
		perror("heap segment mmap");
		exit(1);
	}

	if (!heap_start) {
		// Out of memory
		fprintf(stderr, "Cannot allocate heap; out of memory\n");
		exit(1);
	}

	heap_end = heap_start + heap_size;
	//e clear heap memory (set all bytes to zero)
	memset(heap_start, 0, heap_size);
	heap_free_pointer = heap_start;
}

void
heap_free()
{
	if (heap_start) {
		munmap(heap_start, heap_size());
		heap_start = heap_end = heap_free_pointer = NULL;
	}
}

//e handle out-of-memory situations; frame_pointer
static void
handle_out_of_memory(void *frame_pointer)
{
	void **seeker = (void **) frame_pointer;
	fprintf(stderr, "Out of memory! Seeking from %p to %p\n", seeker, heap_root_frame_pointer);
	if (!heap_root_frame_pointer) {
		fprintf(stderr, "Error: ran out of memory before starting program.");
		exit(1);
	}
	while (seeker != heap_root_frame_pointer) {
		seeker = (void **) *seeker;
		fprintf(stderr, "stack : %p\n", seeker);
	}
	fprintf(stderr, "Help!  I don't know how to handle out-of-memory situations yet!\n");
	exit(1);
}

object_t *
heap_allocate_object(class_t* type, size_t fields_nr)
{
	object_t *obj = (object_t *) heap_free_pointer;
	size_t requested_bytes = sizeof(object_t) + (fields_nr * sizeof(object_member_t));
	heap_free_pointer += requested_bytes;

	//	fprintf(stderr, "alloc(%s, %zu * %zu) : ", type->id->name, fields_nr, sizeof(object_member_t));
	
	if (heap_free_pointer >= heap_end) {
		heap_free_pointer -= requested_bytes;
		//e __builtin_frame_address(0) reads the $fp
		handle_out_of_memory(__builtin_frame_address(0));
		if (heap_available() < requested_bytes) {
			fprintf(stderr, "Out of memory: insufficient space for %zu bytes (%zu fields) (allocated: %zu of %zu bytes)\n", requested_bytes, fields_nr, heap_available(), heap_size());
			exit(1);
		}
		//e handled successfully; recurse to minimise risk of accidental bug
		return heap_allocate_object(type, fields_nr);
	}
	obj->classref = type;
	/* fprintf(stderr, "alloc(%s, %zu * %zu) -> %p\n", type->id->name, fields_nr, sizeof(object_member_t), obj); */
	return obj;
}

size_t
heap_available(void)
{
	return heap_end - heap_free_pointer;
}

size_t
heap_size(void)
{
	return heap_end - heap_start;
}
