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

#define _BSD_SOURCE // Um MAP_ANONYMOUS zu aktivieren
#define _DARWIN_C_SOURCE // Um MAP_ANON zu aktivieren

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

#include "assembler-buffer.h"
#include "errors.h"

#define BIN_PAGES_START 0xb000000000 // Startadresse des Binaercodes im Speicher
#define PAGE_SIZE 0x1000
#define INITIAL_SIZE (PAGE_SIZE * 64)
#define MIN_INCREMENT (PAGE_SIZE * 64)

#define MAX_ASM_WIDTH 14
#define DISASSEMBLE_PRINT_MACHINE_CODE

//#define DEBUG

typedef struct freelist {
	size_t size;  // excluding header
	struct freelist *next;
} freelist_t;

typedef struct buffer_internal {
	size_t allocd; // ohne header, 0 fuer Pseudobuffer
	size_t actual;
	unsigned char data[];
} buffer_internal_t;

#define IS_PSEUDOBUFFER(buf) (!(buf)->allocd)

static void *code_segment = NULL;
static size_t code_segment_size = 0;
static freelist_t *code_segment_free_list;

#define FREELIST

static buffer_internal_t *
code_alloc(size_t buf_size) // size does not include the header
{
	size_t size = buf_size + sizeof(buffer_internal_t);
	if (size < sizeof(freelist_t)) {
		// ensure that we can freelist this item later
		size = sizeof(freelist_t);
	}

	if (!code_segment) {
		size_t alloc_size = INITIAL_SIZE;
		if (alloc_size < size) {
			alloc_size = (size + MIN_INCREMENT) & (~(PAGE_SIZE-1));
		}

		// alloc executable memory
		code_segment = mmap((void *) BIN_PAGES_START,
				    alloc_size,
				    PROT_READ | PROT_WRITE | PROT_EXEC,
				    MAP_PRIVATE | MAP_ANONYMOUS,
				    -1,
				    0);
		if (code_segment == MAP_FAILED) {
			perror("code segment mmap");
			return NULL;
		}

		if (!code_segment) {
			// Out of memory
			return NULL;
		}
#ifdef DEBUG
		fprintf(stderr, "[ABUF] L%d: Alloc %zx at [%p]\n", __LINE__, alloc_size, code_segment);
#endif
		code_segment_size = alloc_size;
		code_segment_free_list = code_segment;
		code_segment_free_list->next = NULL;
		code_segment_free_list->size = alloc_size - sizeof(freelist_t);
#ifdef DEBUG
		fprintf(stderr, "[ABUF] L%d: Freelist at %p: next=%p, size=%zx\n", __LINE__, code_segment_free_list, code_segment_free_list->next, code_segment_free_list->size);
#endif
	}

	// NB: this will allocate the entire buffer on the first attempt, so
	// use of buffer_terminate() is strongly encouraged.
	// If we ever have more than one compilation thread, this will need revision.
	freelist_t **free = &code_segment_free_list;
	while (*free) {
		if ((*free)->size >= buf_size) { // pick the first hit
			freelist_t *buf_freelist = *free;
			buffer_internal_t *buf = (buffer_internal_t *) *free;
			// unchain
			(*free) = buf_freelist->next;
			buf->actual = 0;
#ifdef DEBUG
			fprintf(stderr, "[ABUF] L%d: Freelist at %p: ", __LINE__, code_segment_free_list);
			if (code_segment_free_list) {
				fprintf(stderr, "next=%p, size=%zx", code_segment_free_list->next, code_segment_free_list->size);
			}
			fprintf(stderr, "\n");
#endif
			return buf;
		}
		free = &((*free)->next);
	}

	// we ran out of space
	const size_t old_size = code_segment_size;
	size_t alloc_size = code_segment_size + MIN_INCREMENT;
	if (alloc_size + code_segment_size < size) {
		alloc_size = (code_segment_size + size + MIN_INCREMENT) & (~(PAGE_SIZE-1));
	}

	// alloc executable memory
	void *old_code_segment = code_segment; // error reporting

	// Dieser Code funktioniert nicht auf OS X:
	//void *mremap(void *old_address, size_t old_size, size_t new_size, int flags, ...);
	//code_segment = (buffer_internal_t *) mremap(code_segment, old_size, alloc_size, 0);
	// Daher verwenden wir diesen:
	void *code_segment2 = mmap(((char *) code_segment) + old_size,
				   alloc_size - old_size,
				   PROT_READ | PROT_WRITE | PROT_EXEC,
				   MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED,
				   -1,
				   0);
	exit(0);
	if (code_segment2 == MAP_FAILED) {
		perror("mmap");
		fprintf(stderr, "Failed: mmap(%p, %zx, ...)\n", ((char *) code_segment) + old_size, alloc_size);
		// Out of memory
		return NULL;
	}
	assert(old_code_segment == code_segment);

#ifdef DEBUG
	fprintf(stderr, "[ABUF] L%d: Freelist at %p: ", __LINE__, code_segment_free_list);
	if (code_segment_free_list) {
		fprintf(stderr, "next=%p, size=%zx", code_segment_free_list->next, code_segment_free_list->size);
	}
	fprintf(stderr, "\n");
#endif

	freelist_t *new_freelist = (freelist_t *) (((unsigned char *)code_segment) + old_size);
	code_segment_size = alloc_size;
	new_freelist->next = code_segment_free_list;
	new_freelist->size = alloc_size - old_size - sizeof(freelist_t);
	code_segment_free_list = new_freelist;
#ifdef DEBUG
		fprintf(stderr, "[ABUF] L%d: Freelist at %p: next=%p, size=%zx\n", __LINE__, code_segment_free_list, code_segment_free_list->next, code_segment_free_list->size);
#endif
	return code_alloc(buf_size);
}

static void
code_free(buffer_internal_t *buf)
{
	freelist_t *new_freelist = (freelist_t*) buf;
	new_freelist->next = code_segment_free_list;
	code_segment_free_list = new_freelist;
#ifdef DEBUG
	fprintf(stderr, "[ABUF] L%d: Freelist at %p: ", __LINE__, code_segment_free_list);
	if (code_segment_free_list) {
		fprintf(stderr, "next=%p, size=%zx", code_segment_free_list->next, code_segment_free_list->size);
	}
	fprintf(stderr, "\n");
#endif
	// size remains unchanged
}

static buffer_internal_t * // only used if we _actually_ ran out of space
code_realloc(buffer_internal_t *old_buf, size_t size)
{
	buffer_internal_t *new_buf = code_alloc(size);
#ifdef DEBUG
	fprintf(stderr, "[ABUF] L%d: Realloc'd %p ->", __LINE__, old_buf);
	fprintf(stderr, "%p (copying %zx) for %zx, now has %zx\n", new_buf, old_buf->actual, size, new_buf->allocd);
#endif
	memcpy(new_buf->data, old_buf->data, old_buf->actual);
	new_buf->actual = old_buf->actual;
	assert(new_buf->actual <= size);
	code_free(old_buf);
#ifdef DEBUG
	fprintf(stderr, "[ABUF] L%d: New buf has %zx\n", __LINE__, new_buf->allocd);
#endif
	return new_buf;
}

void
buffer_terminate(buffer_internal_t *buf)
{
	unsigned char *end = buf->data + buf->actual;
	end = (unsigned char *) ((((unsigned long) end) + sizeof(void *) - 1) & (~(sizeof(void *) - 1)));
	size_t left_over = buf->allocd - (end - buf->data);
	if (left_over < sizeof(freelist_t) + 4) {
		// can't store a freelist entry? Just account it to the current entry
		left_over = 0;
	}
	buf->allocd -= left_over;
	if (left_over != 0) {
		// then we have a new freelist entry
		freelist_t *new_freelist = ((freelist_t *)end);
		new_freelist->next = code_segment_free_list;
		new_freelist->size = left_over - sizeof(freelist_t);
		code_segment_free_list = new_freelist;
	}
#ifdef DEBUG
	fprintf(stderr, "[ABUF] L%d: Freelist at %p: ", __LINE__, code_segment_free_list);
	if (code_segment_free_list) {
		fprintf(stderr, "next=%p, size=%zx", code_segment_free_list->next, code_segment_free_list->size);
	}
	fprintf(stderr, "\n");
#endif
}

buffer_t
buffer_new(size_t expected_size)
{
	assert(expected_size > 0);
	buffer_internal_t *buf = code_alloc(expected_size);
	if (buf == NULL) {
		fail("Out of code memory!");
	}
	buf->actual = 0;
#ifdef DEBUG
	fprintf(stderr, "[ABUF] L%d: New buffer %p starts at %p, max %x\n", __LINE__, buf, buf->data, buf->allocd);
#endif
	return buf;
}

void
buffer_free(buffer_t buf)
{
	code_free(buf);
}

size_t
buffer_size(buffer_t buffer)
{
	return buffer->actual;
}

unsigned char *
buffer_alloc(buffer_t *buf, size_t bytes)
{
	buffer_t buffer = *buf;
	if (IS_PSEUDOBUFFER(buffer)) {
		pseudobuffer_t *pb = (pseudobuffer_t *) buffer;
		unsigned char *offset = pb->dest;
		pb->dest += bytes;
		return offset;
	}

	size_t required = buffer->actual + bytes;
	if (required > buffer->allocd) {
		size_t newsize = required + bytes; // some extra space
		buffer_t newbuf = code_realloc(buffer, newsize);
		if (!newbuf) {
			fail("Out of code memory!");
		}
		*buf = buffer = newbuf;
	}
	unsigned char * retval = buffer->data + buffer->actual;
	buffer->actual += bytes;
	return retval;
}

void *
buffer_entrypoint(buffer_t buf)
{
	return &buf->data;
}

buffer_t
buffer_from_entrypoint(void *t)
{
	unsigned char *c = (unsigned char *) t;
	return (buffer_t) (c - offsetof(struct buffer_internal, data));
}

int
disassemble_one(FILE *file, unsigned char *data, int max_len);

void
buffer_disassemble(buffer_t buf)
{
	int size = buf->actual;
	unsigned char *data = buffer_entrypoint(buf);

	while (size > 0) {
		printf("[%p]\t", data);
		int disasmd = disassemble_one(NULL, data, size);

#ifdef DISASSEMBLE_PRINT_MACHINE_CODE
		int i;
		for (i = 0; i < disasmd; i++) {
			printf(" %02x", data[i]);
		}

		for (; i < MAX_ASM_WIDTH; i++) {
			printf("   ");
		}
		printf("\t");
#endif

		disassemble_one(stdout, data, size);
		putchar('\n');
		if (!disasmd) {
			puts("Dissassembly failed:");
			while (size > 0) {
				printf(" %02x", *data);
				++data;
				--size;
			}
			puts("\nFailed to decode");
			return;
		}
		data += disasmd;
		size -= disasmd;
	}
}

void
buffer_setlabel(label_t *label, void *target)
{
	int delta = (char *)target - (char*) label->base_position;
	memcpy(label->label_position, &delta, 4);
}

void *
buffer_target(buffer_t *target)
{
	return (*target)->data + (*target)->actual;
}

void
buffer_setlabel2(label_t *label, buffer_t *buffer)
{
	buffer_setlabel(label, buffer_target(buffer));
}

buffer_t
buffer_pseudobuffer(pseudobuffer_t *buf, void *dest)
{
	buf->dest = (unsigned char *) dest;
	buf->a = 0;
	buf->b = 0;
	return (buffer_t) buf;
}
