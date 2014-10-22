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

// Principal declarations and definitions for the assembler

#ifndef _ATTOL_2OPM_ASM_H
#define _ATTOL_2OPM_ASM_H

#include <stdbool.h>

#include "../assembler-buffer.h"

typedef union {
	signed long int num;
	char  *str;
} yylval_t;

extern yylval_t yylval;

// Lexer/Tokeniser outputs

#define T_ID		0x101
#define T_INT		0x102
#define T_UINT		0x103
#define T_STR		0x104

#define T_S_DATA	0x110
#define T_S_TEXT	0x111
#define T_S_BYTES	0x112
#define T_S_WORDS	0x113
#define T_S_ASCIIZ	0x114

extern int errors_nr; // total # of errors encountered

/**
 * Issue load/link-time error message (prevents execution)
 */
void
error(const char *fmt, ...);

/**
 * Issue load/link-time warning message (does not prevent execution)
 */
void
warn(const char *fmt, ...);

/* -- memory -- */

extern buffer_t text_buffer;
extern char data_section[];
extern bool in_text_section; // which section are we operating on?

void
init_memory(void);

/**
 * Determines the pointer of the current write position in text or static data.
 * Uses `in_text_section' to determine which section we are writing to.
 */
void *
current_offset();

/**
 * Allocates memory in the current section
 *
 * Uses `in_text_section' to determine which section we are writing to.
 *
 * @param size Number of bytes to allocate
 * @return pointer to the allocated region
 */
void *
alloc_data_in_section(size_t size);

/* -- labels -- */

/**
 * Perform relocation on all registered label uses
 */
void
relocation_finish(void);

/**
 * Adds a $gp-relative label reference
 *
 * @param label Name of the label to add
 * @param addr Address of the instruction that references the label
 * @param offset Byte address into the instruction
 */
void
relocation_add_gp_label(char *label, void *addr, int offset);

/**
 * Adds a new label reference as part of a branch/jump instruction
 *
 * @param label Name of the label to reference/add
 */
label_t *
relocation_add_jump_label(char *label);

/**
 * Adds a new label reference as part of a 
 *
 */
void *
relocation_get_resolved_text_label(char *label);

/**
 * Resolves the location of a label
 *
 * Resolved addresses are stored for later relocation (relocation_finish())
 *
 * @param name Name of the label to resolve
 * @param offset Memory address to resolve to
 */
void
relocation_add_label(char *name, void *offset);

/* -- miscellany -- */

/**
 *
 */
void
parse_file(buffer_t *dest_buffer, char *file_name);

/**
 * Initialise the builtin functions into the given buffer and install their labels into the label table
 */
void
init_builtins(buffer_t *dest_buffer);

/**
 * Execute f() with an interactive debugger
 *
 * @param buf The buffer that `f' is defined in.  Any program counter addresses outside of
 * this buffer will be silently executed.
 * @param f The function to execute interactively.
 */
void
debug(buffer_t *buf, void (*f)());

#endif // !defined(_ATTOL_2OPM_ASM_H)
