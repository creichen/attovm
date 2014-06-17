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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#include "assembler-buffer.h"
#include "errors.h"

#define PAGE_SIZE 0x1000
#define INITIAL_SIZE (PAGE_SIZE * 8)
#define MIN_INCREMENT (PAGE_SIZE * 8)

void *mremap(void *old_address, size_t old_size, size_t new_size, int flags, ...);

typedef struct freelist {
	size_t size;  // excluding header
	struct freelist *next;
} freelist_t;

typedef struct buffer_internal {
	size_t allocd; // excluding header
	size_t actual;
	unsigned char data[];
} buffer_internal_t;

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
		code_segment = mmap(NULL,
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
		code_segment_size = alloc_size;
		code_segment_free_list = code_segment;
		code_segment_free_list->next = NULL;
		code_segment_free_list->size = alloc_size - sizeof(freelist_t);
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
			(*free)->next = buf_freelist->next;
			buf->actual = 0;
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
	code_segment = (buffer_internal_t *)
		mremap(code_segment,
		       old_size,
		       alloc_size,
		       MAP_FIXED,
		       code_segment);
	if (code_segment == NULL) {
		// Out of memory
		return NULL;
	}

	freelist_t *new_freelist = (freelist_t *) ((unsigned char *)code_segment) + code_segment_size;
	code_segment_size = alloc_size;
	new_freelist->next = code_segment_free_list;
	new_freelist->size = alloc_size - old_size - sizeof(freelist_t);
	code_segment_free_list = new_freelist;
	return code_alloc(buf_size);
}

static void
code_free(buffer_internal_t *buf)
{
	freelist_t *new_freelist = (freelist_t*) buf;
	new_freelist->next = code_segment_free_list;
	// size remains unchanged
}

static buffer_internal_t * // only used if we _actually_ ran out of space
code_realloc(buffer_internal_t *old_buf, size_t size)
{
	buffer_internal_t *new_buf = code_alloc(size);
	memcpy(new_buf->data, old_buf->data, old_buf->actual);
	code_free(old_buf);
	return new_buf;
}

void
buffer_terminate(buffer_internal_t *buf)
{
	unsigned char *end = buf->data + buf->actual;
	end = (unsigned char *) ((((unsigned long) end) + sizeof(void *) - 1) & (~(sizeof(void *) - 1)));
	size_t left_over = (end - buf->data);
	if (left_over < sizeof(freelist_t) + 4) {
		// can't store a freelist entry? Just account it to the current entry
		left_over = 0;
	}
	buf->allocd -= left_over;
	if (left_over != 0) {
		// then we have a new freelist entry
		freelist_t *new_freelist = ((freelist_t *)end);
		new_freelist->next = code_segment_free_list;
		new_freelist->size = left_over - sizeof(freelist_t *);
	}
}

buffer_t
buffer_new(size_t expected_size)
{
	buffer_internal_t *buf = code_alloc(expected_size);
	if (buf == NULL) {
		fail("Out of code memory!");
	}
	buf->allocd = expected_size;
	buf->actual = 0;
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
	size_t required = buffer->actual + bytes;
	if (required > buffer->allocd) {
		size_t newsize = required + bytes; // some extra space
		buffer_t newbuf = code_realloc(buffer, newsize);
		if (!newbuf) {
			fail("Out of code memory!");
		}
		*buf = buffer = newbuf;
		newbuf->allocd = newsize;
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

int
disassemble_one(FILE *file, unsigned char *data, int max_len);

void
buffer_disassemble(buffer_t buf)
{
	int size = buf->actual;
	unsigned char *data = buffer_entrypoint(buf);

	while (size > 0) {
		int disasmd = disassemble_one(stdout, data, size);
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
