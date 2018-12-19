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

//#define DEBUG
//#define INFO

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#include "bitvector.h"
#include "cstack.h"
#include "compiler-options.h"
#include "heap.h"
#include "runtime.h"
#include "stackmap.h"
#include "symbol-table.h"

#ifndef MAP_ANONYMOUS
#  ifndef MAP_ANON
#    error "need MAP_ANONYMOUS or MAP_ANON"
#  endif
#  define MAP_ANONYMOUS MAP_ANON
#endif

#ifdef DEBUG
#  define debug printf
#else
#  define debug if (0) printf
#endif

#define HEAP_START 0x10000000000 /*e default heap memory start address */
#define PAGE_SIZE 0x1000 /*e normal page size (FIXME: validate against system header) */

typedef struct {
	unsigned char *start;
	unsigned char *end;
} semispace_t;

void *heap_root_frame_pointer = NULL; /*e initialised by runtime_execute() */

static unsigned char *heap_base = NULL;
static size_t heap_size_total;
static unsigned char *heap_free_pointer = NULL;

static semispace_t to_space = { NULL, NULL };
static semispace_t from_space = { NULL, NULL };


void
heap_init(size_t requested_heap_size)
{
	//e we allocate the heap only once:
	assert(!heap_base);
	heap_size_total = requested_heap_size;

	//e Make sure that heap_size is a multiple of PAGE_SIZE, as most systems require that property
	if (heap_size_total & (PAGE_SIZE - 1)) {
		heap_size_total = (heap_size_total + PAGE_SIZE) & ~(PAGE_SIZE -1);
	}

	heap_base = mmap((void *) HEAP_START,
			 heap_size_total,
			 PROT_READ | PROT_WRITE,
			 MAP_PRIVATE | MAP_ANONYMOUS,
			 -1,
			 0);
	if (heap_base == MAP_FAILED) {
		perror("heap segment mmap");
		exit(1);
	}

	if (!heap_base) {
		// Out of memory
		fprintf(stderr, "Cannot allocate heap; out of memory\n");
		exit(1);
	}

	//e Copying GC: split heap into two semispaces
	to_space.start = heap_base;
	to_space.end = heap_base + (heap_size_total >> 1);
	from_space.start = to_space.end;
	from_space.end = heap_base + heap_size_total;

	heap_free_pointer = to_space.start;
}

void
heap_free()
{
	if (heap_base) {
		munmap(heap_base, heap_size_total);
		heap_base = heap_free_pointer = NULL;
	}
}

//e handle out-of-memory situations
static void
handle_out_of_memory(void *frame_pointer);

object_t *
heap_allocate_object(class_t* type, size_t fields_nr)
{
	object_t *obj = (object_t *) heap_free_pointer;
	size_t requested_bytes = sizeof(object_t) + (fields_nr * sizeof(object_member_t));
	heap_free_pointer += requested_bytes;

	//	fprintf(stderr, "alloc(%s, %zu * %zu) : ", type->id->name, fields_nr, sizeof(object_member_t));
	
	if (heap_free_pointer >= to_space.end) {
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
	return to_space.end - heap_free_pointer;
}

size_t
heap_size(void)
{
	return to_space.end - to_space.start;
}

// ================================================================================
// garbage collector implementation

// Swaps contents of to_space and from_space.
static void
swap_semispaces(void)
{
	semispace_t buf = to_space;
	to_space = from_space;
	from_space = buf;
}

static bool
in_from_space(void *addr)
{
	return (((unsigned char *)addr) >= from_space.start
		&& ((unsigned char *)addr) < from_space.end);
}

static bool
in_to_space(void *addr)
{
	return (((unsigned char *)addr) >= to_space.start
		&& ((unsigned char *)addr) < to_space.end);
}

static void *
get_forwarding_pointer(void *addr)
{
	return *((void **)addr);
}

static void
set_forwarding_pointer(void *addr, void *reloc_addr)
{
	*((void **)addr) = reloc_addr;
}

static size_t
object_size(object_t *obj)
{
	const int BLOCKSIZE = sizeof(object_member_t);
	
	if (obj->classref == &class_string) {
		size_t strlen = obj->fields[0].int_v;
		size_t blocklen = (strlen + (BLOCKSIZE - 1)) & ~(BLOCKSIZE - 1);
		return BLOCKSIZE + blocklen + sizeof(object_t);
	} else if (obj->classref == &class_array) {
		return ((obj->fields[0].int_v + 1) * BLOCKSIZE) + sizeof(object_t);
	} else {
		return (obj->classref->id->storage.fields_nr * BLOCKSIZE) + sizeof(object_t);
	}
}

static void
gc_move(object_t **memref)
{
	if (!*memref) {
		return;
	}
	if (in_from_space(*memref)) {
		void *reloc = get_forwarding_pointer(*memref);
		if (in_to_space(reloc)) {
			// already relocated
			*memref = reloc;
			return;
		}

		reloc = heap_free_pointer;
		size_t obj_size = object_size(*memref);
		heap_free_pointer += obj_size;
		assert(heap_free_pointer < to_space.end);
		memcpy(reloc, *memref, obj_size);
		debug(" - [%p -> %p (%s, %zu bytes)]\n", *memref, reloc, (*memref)->classref->id->name, obj_size);
		set_forwarding_pointer(*memref, reloc);
	        *memref = reloc;
	} else {
		printf("Trying to relocate weird addr %p\n", *memref);
	}
}

static void
gc_init()
{
	swap_semispaces();
	heap_free_pointer = to_space.start;
}

static void
gc_rootset_static()
{
	runtime_image_t *img = runtime_current();
	debug(" <static memory: %d>\n", img->globals_nr);
	for (int i = 0; i < img->globals_nr; i++) {
		if (SYMTAB_TYPE(symtab_lookup(img->globals[i])) == TYPE_OBJ) {
			gc_move((object_t **) &(img->static_memory[i]));
		}
	}
}

static void
gc_rootset_stack(void *frame_pointer)
{
	void **seeker = (void **) frame_pointer;
	while (seeker != heap_root_frame_pointer) {
		void *return_addr = (void *)(seeker[1]);
		seeker = (void **) *seeker;
		debug(" <stack: %p @%p>: ", seeker, return_addr);
		symtab_entry_t *symtab_entry;
		bitvector_t stackmap;
		if (stackmap_get(return_addr, &stackmap, &symtab_entry)) {
			object_t **obj = (object_t **) seeker;
			size_t stackmap_size = bitvector_size(stackmap);
			int offset;

			if (symtab_entry) {
				offset = symtab_entry->stackframe_start / 8;
			} else {
				offset = -stackmap_size;
			}
#ifdef DEBUG
			if (symtab_entry) {
				symtab_entry_name_dump(stdout, symtab_entry);
			} else {
				debug("<main entry point>");
			}
			debug(": ");
			bitvector_print(stdout, stackmap);
			debug(" at offset %d\n", offset);
#endif

			obj += offset;
			for (int i = 0; i < stackmap_size; i++) {
				if (BITVECTOR_IS_SET(stackmap, i)) {
					gc_move(obj + i);
				}
			}
		} else {
			debug(" (unknown)\n");
		}
	}
}

static void
gc_do_scan()
{
	debug(" <scan>\n");
	unsigned char *scan = to_space.start;
	while ((unsigned char *) scan < heap_free_pointer) {
		object_t *obj = (object_t *) scan;
		size_t obj_size = object_size(obj);
		if (obj->classref == &class_array) {
			for (int i = 0; i < obj->fields[0].int_v; i++) {
				gc_move(&obj->fields[1 + i].object_v);
			}
		} else if (obj->classref != &class_string) {
			bitvector_t classmap = obj->classref->object_map;
			for (int i = 0; i < bitvector_size(classmap); i++) {
				if (BITVECTOR_IS_SET(classmap, i)) {
					gc_move(&obj->fields[i].object_v);
				}
			}
		}

		scan += obj_size;
	}
}

/*e
 * handle out-of-memory situations
 *
 * @param frame_pointer Frame pointer of the failed invocation to heap_allocate_object;
 * can be used to trace the parent frame pointers up to heap_root_frame_pointer
 */
static void
handle_out_of_memory(void *frame_pointer)
{
	//fprintf(stderr, "Out of memory! Seeking from %p to %p\n", frame_pointer, heap_root_frame_pointer);
	if (!heap_root_frame_pointer) {
		fprintf(stderr, "Error: ran out of memory before starting program.");
		exit(1);
	}

	size_t before = heap_available();

	gc_init();
	gc_rootset_static();
	gc_rootset_stack(frame_pointer);
	gc_do_scan();
	//e clear memory at end of stack frame
	memset(heap_free_pointer, 0, to_space.end - heap_free_pointer);

	size_t after = heap_available();
#if defined(INFO) || defined(DEBUG)
	{
#else
	if (compiler_options.debug_gc) {
#endif
		fprintf(stderr, "[GC: Reclaimed %zu bytes]\n", after - before);
	}
	if (after - before == 0) {
		fprintf(stderr, "Error: out of memory\n");
		exit(1);
	}
}
