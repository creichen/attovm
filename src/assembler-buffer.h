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

#define ASSEMBLER_BIN_PAGES_START 0xb000000000 // Startadresse des Binaercodes im Speicher

typedef struct {
	size_t a, b;
	unsigned char *dest;
} pseudobuffer_t;

struct buffer_internal;
typedef struct buffer_internal* buffer_t;

// Relative Sprungmarke in einem Sprungbefehl.  Kann verzoegert gesetzt werden.
typedef struct relative_jump_label {
	void *label_position; // Speicherstelle, an der die relative Sprungadresse stehen soll
	void *base_position; // Position, relativ zu der der Sprung angegeben werden muss
} label_t;

//e Sets the label to a specific memory address
void
buffer_setlabel(label_t *label, void *target);

//d Setzt das Label auf den naechsten Befehl
//e Sets the label to the next instruction
void
buffer_setlabel2(label_t *label, buffer_t *target);

//d Sprungziel: naechster Befehl
//e Extract memory address that will be used for next instruction: for use with buffer_setlabel()
void *
buffer_target(buffer_t *target);

buffer_t
buffer_new(size_t expected_size);

/**
 * Erzeugt einen `Pseudobuffer'
 */
buffer_t
buffer_pseudobuffer(pseudobuffer_t *buf, void *dest);

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
 * Rekonstruiert Buffer von Einsprungpunkt.
 *
 * Fuehrt keine Fehleruebepruefung durch-- Speicherkorrpution, wenn der Zeiger
 * nicht durch buffer_entrypoint() erzeugt wurde!
 */
buffer_t
buffer_from_entrypoint(void *);


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

// Load Address
#define emit_la(buf, reg, p) emit_li(buf, reg, (long long) p)

#endif // !defined(ASSEMBLER_BUFFER_H_)
