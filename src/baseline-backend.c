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

#include "baseline-backend.h"
#include "assembler.h"
#include "registers.h"
#include "class.h"
#include "object.h"


void
baseline_compile_expr(buffer_t *buf, int dest_register, int temp_reg_base)
{
	emit_li(buf, dest_register, 42);
}

void *builtin_op_print(object_t *arg);  // builtins.c

buffer_t
baseline_compile(ast_node_t *root,
		 void *static_memory)
{
	buffer_t buf = buffer_new(1024);
	// push...
	emit_push(&buf, REGISTER_GP);
	emit_li(&buf, REGISTER_GP, (long long int) static_memory);
	baseline_compile_expr(&buf, registers_argument_nr[0], REGISTER_GP);
	emit_li(&buf, REGISTER_V0, (long long int) new_int);
	emit_jalr(&buf, REGISTER_V0);
	emit_move(&buf, registers_argument_nr[0], REGISTER_V0);
	emit_li(&buf, REGISTER_V0, (long long int) builtin_op_print);
	emit_jalr(&buf, REGISTER_V0);
	emit_pop(&buf, REGISTER_GP);
	emit_jreturn(&buf);
	buffer_terminate(buf);
	return buf;
}
