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

static object_t *
heap_allocate_object(class_t* type, size_t fields_nr)
{
	object_t *obj = calloc(1, sizeof(object_t) + fields_nr * sizeof(object_t *));
	obj->classref = type;
	return obj;
}

object_t *
new_object(class_t* type, unsigned long long fields_nr)
{
	return heap_allocate_object(type, fields_nr);
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
	size_t fields_nr_space = (len + sizeof(void*)) & ~(sizeof (void*) - 1);
	object_t *obj = heap_allocate_object(&class_string, fields_nr_space >> 3);
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


long long int
builtin_op_obj_test_eq(object_t *a0, object_t *a1)
{
	//	fprintf(stderr, "obj-compare(%p, %p)\n", a0, a1);
	if (a0 == a1) {
		return 1;
	}
	if (a0 == NULL || a1 == NULL) {
		return 0;
	}
	if (a0->classref != a1->classref) {
		return 0;
	}
	if (a0->classref == &class_boxed_int) {
		return a0->members[0].int_v == a1->members[0].int_v;
	}
	if (a0->classref == &class_boxed_real) {
		return a0->members[0].real_v == a1->members[0].real_v;
	}
	if (a0->classref == &class_string) {
		return !strcmp(&a0->members[0].string, &a1->members[0].string);
	}
	// Ansonsten immer ungleich
	return 0;
}

static void
object_print_internal(FILE *f, object_t *obj, bool debug, int depth, char *sep)
{
	if (!obj) {
		fprintf(f, "(null)");
		return;
	}

	class_t *classref = obj->classref;
	char loc[24] = "";
	if (debug) {
		sprintf(loc, "@%p", obj);
	}

	if (classref == &class_boxed_int) {
		fprintf(f, "%lld%s", obj->members[0].int_v, loc);
		return;
	} else if (classref == &class_boxed_real) {
		fprintf(f, "%f%s", obj->members[0].real_v, loc);
		return;
	} else if (classref == &class_string) {
		fprintf(f, "%s%s", (char *)&obj->members[0], loc);
		return;
	} else if (classref == &class_array) {
		fprintf(f, "[ ");
		for (int i = 0; i < obj->members[0].int_v; i++) {
			if (i > 0) {
				fprintf(f, ",");
			}
			object_print_internal(f, obj->members[i+1].object_v, debug, depth - 1, " ");
		}
		fprintf(f, "]%s\n", loc);
		return;
	}


	int printed = 0;
	symtab_entry_t *sym = classref->id;
	fprintf(f, "%s@%p {%s", sym->name, loc, sep);
	if (depth == 0) {
		fprintf(f, "... }");
		return;
	}

	if (debug) {
		fprintf(f, "_table_mask = 0x%llx,%s", classref->table_mask, sep);
	}
	// Felder
	for (int i = 0; i <= classref->table_mask; i++) {
		unsigned long long coding = classref->members[i].selector_encoding;
		symtab_entry_t *msym = classref->members[i].symbol;
		if (coding && !CLASS_MEMBER_IS_METHOD(CLASS_DECODE_SELECTOR_TYPE(coding))) {
			char *ty = "?";
			if (msym->ast_flags & TYPE_OBJ) {
				ty = "obj";
			}
			if (msym->ast_flags & TYPE_INT) {
				ty = "int";
			}
			fprintf(f, "%s %s = ", ty, msym->name);

			if (msym->ast_flags & TYPE_OBJ) {
				object_print_internal(f, obj->members[CLASS_DECODE_SELECTOR_OFFSET(coding)].object_v,
						      debug, depth - 1, " ");
			} else if (msym->ast_flags & TYPE_INT) {
				fprintf(f, "%lld", obj->members[CLASS_DECODE_SELECTOR_OFFSET(coding)].int_v);
			} else {
				fprintf(f, "?");
			}
			if (debug) {
				fprintf(f, "[%i: code=0x%llx, selector=0x%x]", i, coding, msym->selector);
			}
			fprintf(f, ", %s", sep);
		}
	}
	// Methoden
	if (debug) {
		for (int i = 0; i <= classref->table_mask; i++) {
			unsigned long long coding = classref->members[i].selector_encoding;
			symtab_entry_t *msym = classref->members[i].symbol;
			if (coding && CLASS_MEMBER_IS_METHOD(CLASS_DECODE_SELECTOR_TYPE(coding))) {
				fprintf(f, "method %s(%d args)", msym->name, msym->parameters_nr);
				fprintf(f, "[%i: code=0x%llx, selector=0x%x]", i, coding, msym->selector);
				fprintf(f, ", %s", sep);
			}
		}
	}
	if (printed) {
		fprintf(f, "%s", sep);
	}
	fprintf(f, "}");
}

void
object_print(FILE *f, object_t *obj, int depth, bool debug)
{
	object_print_internal(f, obj, debug, depth, "\n");
}

