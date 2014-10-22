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

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "asm.h"

int error_line_nr = 0;
int errors_nr = 0;
int warnings_nr = 0;

void
error(const char *fmt, ...)
{
	va_list args;
	if (error_line_nr < 0) {
		fprintf(stderr, "Error: ");
	} else {
		fprintf(stderr, "[line %d] error: ", error_line_nr);
	}
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
	fprintf(stderr, "\n");
	++errors_nr;
}

void
warn(const char *fmt, ...)
{
	va_list args;
	if (error_line_nr < 0) {
		fprintf(stderr, "Warning: ");
	} else {
		fprintf(stderr, "[line %d] warning: ", error_line_nr);
	}
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
	fprintf(stderr, "\n");
	++warnings_nr;
}

void
yyerror(const char *str)
{
	error("%s", str);
}

void
fail(char *msg)
{
	fprintf(stderr, "Failure: %s\n", msg);
	exit(1);
}
