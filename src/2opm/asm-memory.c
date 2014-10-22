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

#include "asm.h"

#define DATA_SECTION_SIZE (1024 * 1024)

buffer_t text_buffer;
byte data_section[DATA_SECTION_SIZE];
bool in_text_section = false; // which section are we operating on?
int data_offset;

byte *
end_of_data(void)
{
	byte *d = (byte *) data_section;
	d += data_offset;
	return d;
}

void *
current_location()
{
	if (in_text_section) {
		return buffer_target(&text_buffer);
	} else {
		return &(data_section[data_offset]);
	}
}

void
init_memory()
{
	data_offset = 16;  // reserve 16 bytes; this is a hack to help asm-label identify memory as allocated
	text_buffer = buffer_new(1024);
	in_text_section = false;
}

void *
alloc_data_in_section(size_t size)
{
	if (in_text_section) {
		return buffer_alloc(&text_buffer, size);
	} else {
		void *retval = &(data_section[data_offset]);
		data_offset += size;
		if (data_offset >= DATA_SECTION_SIZE) {
			error("Allocated too much data (maximum data section size is %d)", DATA_SECTION_SIZE);
			exit(1);
		}
		return retval;
	}
}
