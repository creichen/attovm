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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "registers.h"
#include "assembler-buffer.h"
#include "assembler.h"

void
fail(char *msg)
{
	fprintf(stderr, "FATAL: %s\n", msg);
	*((int *)NULL) = 0; // breakpointish
	exit(1);
}

int
main(int argc, char **argv)
{
	buffer_t buf = buffer_new(1024);
	emit_li(&buf, 0, 0);
	emit_add(&buf, 0, 7);
	emit_add(&buf, 0, 6);
	emit_jreturn(&buf);
	buffer_terminate(buf);
	long (*f)(long, long) = buffer_entrypoint(buf);
	buffer_disassemble(buf);
	printf("%ld\n", f(2, 7));
	return 0;
}

#include "assembler.c"
#include "assembler-buffer.c"
