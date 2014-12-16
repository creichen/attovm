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

#ifndef _ATTOL_DEBUGGER_H
#define _ATTOL_DEBUGGER_H

#include <stdbool.h>

typedef struct {
	unsigned char *debug_region_start;
	unsigned char *debug_region_end;
	void *static_section;
	size_t static_section_size;
	void *(*name_lookup)(char *name); // may be NULL: maps name to address
	void (*error)(const char *fmt, ...); // for sending error messages
	bool (*is_instruction)(unsigned char *location); // check whether this location contains a known machine instruction
} debugger_config_t;

/*e
 * Debugs execution of the specified entry_point with interactive debugger.
 * Skips over any code that is not within asm_start and asm_end.
 */
void
debug(debugger_config_t *config, void (*entry_point)());

#endif // !defined(_ATTOL_DEBUGGER_H)
