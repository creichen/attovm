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

#include <string.h>

#include "object.h"

object_t *
heap_allocate_object(class_t* type, size_t fields_nr)
{
	object_t *obj = calloc(1, sizeof(object_t) + fields_nr * sizeof(object_t *));
	obj->classref = type;
	return obj;
}

object_t *
new_int(long long int v)
{
	object_t *obj = heap_allocate_object(&class_boxed_int, 1);
	obj->members[0].int_v = v;
	return obj;
}

object_t *
new_real(double v)
{
	object_t *obj = heap_allocate_object(&class_boxed_real, 1);
	obj->members[0].real_v = v;
	return obj;
}


object_t *
new_string(char *string, size_t len)
{
	object_t *obj = heap_allocate_object(&class_string, (len + 1) & ~(sizeof (void*) - 1));
	memcpy(&obj->members[0], string, len + 1);
	return obj;
}


object_t *
new_array(size_t len)
{
	object_t *obj = heap_allocate_object(&class_array, len + 1);
	obj->members[0].int_v = len;
	return obj;
}
