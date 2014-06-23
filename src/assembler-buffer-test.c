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
#include <assert.h>

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

void
test_0()
{
	buffer_t buf = buffer_new(1024);
	emit_li(&buf, 0, 0);
	emit_add(&buf, 0, 7);
	emit_add(&buf, 0, 6);
	emit_jreturn(&buf);
	buffer_terminate(buf);
	long (*f)(long, long) = buffer_entrypoint(buf);
	//	buffer_disassemble(buf);
	//	printf("%ld\n", f(2, 7));
	assert(9 == f(2, 7));
}

void
memtest_init_n(buffer_t *buffers, int buffers_nr)
{
	for (int i = 0; i < buffers_nr; i++) {
		buffer_t buf = buffer_new(100 + i);

		for (int j = 0; j <= i; j++) {
			int *iref = (int *) buffer_alloc(&buf, 4);
			*iref = i + (j*j);
		}
		buffer_terminate(buf);
		buffers[i] = buf;
	}
}

void
memtest_validate_i(buffer_t *buffers, int i)
{
	int *data = buffer_entrypoint(buffers[i]);

	for (int j = 0; j <= i; j++) {
		assert(data[j] == i + (j*j));
	}
}

void
memtest()
{
	const int buffers_nr = 300;
	buffer_t buffers[buffers_nr];

	memtest_init_n(buffers, buffers_nr);
	for (int i = 0; i < buffers_nr; i++) {
		memtest_validate_i(buffers, i);
	}

	for (int i = 0; i < buffers_nr; i++) {
		if (i & 1) {
			buffer_free(buffers[i]);
		}
	}

	const int auxbuffers_nr = 300;
	buffer_t auxbuffers[auxbuffers_nr];

	memtest_init_n(auxbuffers, auxbuffers_nr);
	for (int i = 0; i < auxbuffers_nr; i++) {
		memtest_validate_i(auxbuffers, i);
	}


	for (int i = 0; i < buffers_nr; i++) {
		if (!(i & 1)) {
			buffer_free(buffers[i]);
		}
	}
}

int
main(int argc, char **argv)
{
	fprintf(stderr, "START\n");
	test_0();
	memtest();
	fprintf(stderr, "DONE\n");
	return 0;
}
