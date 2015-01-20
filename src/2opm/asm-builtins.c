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

#include "../assembler.h"
#include "../registers.h"
#include "../address-store.h"

#include "asm.h"


static void
print_string(char *s)
{
	printf("%s\n", s);
}

static void
print_int(long long i)
{
	printf("%lld\n", i);
}

static long long int
read_int()
{
	long long int i;
	scanf("%lld", &i);
	return i;
}

#define BUILTINS_NR 4

struct {
	char *name;
	void *ptr;
} builtins[BUILTINS_NR] = {
	{"print_string", &print_string},
	{"print_int", &print_int},
	{"read_int", &read_int},
	{"exit", &exit},
};

char* mk_unique_string(char *text);

void
init_builtins(buffer_t *buffer)
{
	in_text_section = true;
	for (int i = 0; i < BUILTINS_NR; i++) {
		void *addr = buffer_target(buffer);
		relocation_add_label(mk_unique_string(builtins[i].name), addr);
				     
		addrstore_put(addr, ADDRSTORE_KIND_BUILTIN, builtins[i].name);

		emit_push(buffer, REGISTER_T0); // align stack
		emit_li(buffer, REGISTER_V0, (unsigned long long) builtins[i].ptr);
		emit_jalr(buffer, REGISTER_V0);
		emit_pop(buffer, REGISTER_T0); // align stack
		emit_jreturn(buffer);
	}
}
