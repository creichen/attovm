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

#include <unistd.h> // getopt
#include <stdlib.h>

#include "../registers.h"
#include "../assembler.h"
#include "../version.h"
#include "../debugger.h"

#include "asm.h"

#define MAIN_ENTRY_POINT "main"
#define START_ENTRY_POINT "__start"

#define ACTION_RUN	1
#define ACTION_VERSION	2
#define ACTION_HELP	3

static bool flag_print_asm = false;
static bool flag_print_aux_asm = false;
static bool flag_debug = false;

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
	emit_move(&builtins_buffer, REGISTER_FP, REGISTER_SP); // stack frame
	emit_li(&builtins_buffer, REGISTER_GP, (unsigned long long) &data_section[0]); // set up $gp
	emit_jal(&builtins_buffer, relocation_add_jump_label(MAIN_ENTRY_POINT));
	emit_pop(&builtins_buffer, REGISTER_FP); // re-align stack for return
	emit_jreturn(&builtins_buffer);
}

void
print_help(char *fn)
{
	printf("Usage: %s [<options>] <file.s>\n", fn);
	printf("Where <options> can select any of the following actions:\n"
	       "\t-v\tPrint version information\n"
	       "\t-h\tPrint this help information\n"
		"\t-x\tExecute program (default action)\n");
	printf("<options> can also toggle the following features:\n"
	       "\t-d\tEnable debugger\n"
	       "\t-p\tPrint assembly before executing\n"
	       "\t-P\tPrint assembly and auxiliary assembly code for built-in operations\n");
}

void
print_version()
{
	printf("2opm assembler version " VERSION ".\n");
}

void
asm_debug(buffer_t *buf, void (*f)())
{
	debugger_config_t conf = {
		.debug_region_start = (byte *) buffer_entrypoint(*buf),
		.debug_region_end = ((byte *) buffer_entrypoint(*buf)) + buffer_size(*buf),
		.static_section = data_section,
		.static_section_size = end_of_data() - data_section,
		.name_lookup = relocation_get_resolved_text_label,
		.error = error,
		.is_instruction = text_instruction_location
	};
	debug(&conf, f);
}

int
execute(char *filename)
{
	init_memory();
	parse_file(&text_buffer, filename);
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
	if (flag_print_asm) {
		buffer_disassemble(text_buffer);
	}
	if (flag_print_aux_asm) {
		buffer_disassemble(builtins_buffer);
	}
	fflush(NULL);
	if (flag_debug) {
		asm_debug(&text_buffer, entry_point);
	} else {
		entry_point();
	}
	return 0;
}

int
main(int argc, char **argv)
{
	int action = ACTION_RUN;
	char opt;

	while ((opt = getopt(argc, argv, "hvxpdP")) != -1) {
		switch (opt) {
		case 'h':
			action = ACTION_HELP;
			break;

		case 'v':
			action = ACTION_VERSION;
			break;

		case 'x':
			action = ACTION_RUN;
			break;

		case 'd':
			flag_debug = true;
			break;

		case 'P':
			flag_print_aux_asm = true;
			// fall through
		case 'p':
			flag_print_asm = true;
			break;

		default:
			;
		}
	}

	if (action == ACTION_HELP) {
		print_help(argv[0]);
	} else if (action == ACTION_VERSION) {
		print_version();
	} else {
		if (optind >= argc) {
			error("Must specify file to execute.  Run with `%s -h' for help.", argv[0]);
			return 1;
		}

		if (optind + 1 > argc) {
			error("Only one assembly file permitted.");
			// supporting multiple files would be easy-- it's mostly a question of making the error messages readable.
			return 1;
		}

		return execute(argv[optind]);
	}
	return 0;
}
