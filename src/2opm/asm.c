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

#include <stdlib.h>

#include "../registers.h"
#include "../assembler.h"

#include "asm.h"

#define MAIN_ENTRY_POINT "main"
#define START_ENTRY_POINT "__start"

static void
forgot_to_return()
{
	fprintf(stderr, "Warning: Forgot to exit or return from main");
	exit(1);
}

static buffer_t builtins_buffer;

static void
init_builtins_and_start()
{
	builtins_buffer = buffer_new(128);
	in_text_section = true;
	init_builtins(&builtins_buffer);

	relocation_add_label(START_ENTRY_POINT, buffer_target(&builtins_buffer));

	// __start: loader
	emit_push(&builtins_buffer, REGISTER_FP); // align stack
	emit_li(&builtins_buffer, REGISTER_GP, (unsigned long long) &data_section[0]); // set up $gp
	emit_jal(&builtins_buffer, relocation_add_jump_label(MAIN_ENTRY_POINT));
	emit_pop(&builtins_buffer, REGISTER_FP); // re-align stack for return
	emit_jreturn(&builtins_buffer);
}

int
main(int argc, char **argv)
{
	init_memory();
	parse_file(&text_buffer, argv[1]);
	// helper code to catch simple bugs
	emit_li(&text_buffer, REGISTER_V0, (unsigned long long) &forgot_to_return);
	emit_jalr(&text_buffer, REGISTER_V0);

	init_builtins_and_start();
	relocation_finish();
	void (*entry_point)() = relocation_get_resolved_text_label(START_ENTRY_POINT);

	if (!entry_point) {
		error("No `%s' entry point set in text segment", START_ENTRY_POINT);
	}

	if (errors_nr) {
		fprintf(stderr, "%d error%s, aborting\n", errors_nr, errors_nr > 1? "s" : "");
		return 1;
	}
	buffer_disassemble(text_buffer);
	buffer_disassemble(builtins_buffer);
	fflush(NULL);
	debug(&text_buffer, entry_point);
	return 0;
}
