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

#ifndef ASSEMBLER_BUFFER_H_
#define ASSEMBLER_BUFFER_H_

#include <stddef.h>

struct buffer_internal;
typedef struct buffer_internal* buffer_t;

typedef struct {
	void *label_position;
	void *base_position;
} relative_jump_label_t;

void
buffer_setlabel(relative_jump_label_t *label, void *target);

// Setzt das Label auf den naechsten Befehl
void
buffer_setlabel2(relative_jump_label_t *label, buffer_t target);

buffer_t
buffer_new(size_t expected_size);

void
buffer_free(buffer_t buf);

size_t
buffer_size(buffer_t buffer);

unsigned char *
buffer_alloc(buffer_t *, size_t bytes);

/**
 * Obtains the code entry point for the buffer
 */
void *
buffer_entrypoint(buffer_t buf);

/**
 * Notifies the buffer manager that writing for this buffer is complete
 */
void
buffer_terminate(buffer_t buf);

/**
 * Disassembles the buffer to stdout
 */
void
buffer_disassemble(buffer_t buf);

#endif // !defined(ASSEMBLER_BUFFER_H_)
