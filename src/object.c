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

#include "errors.h"
#include "object.h"
#include "heap.h"

object_t *
new_object(class_t* type, unsigned long long fields_nr)
{
	object_t *obj = heap_allocate_object(type, fields_nr);
#if 0
	fprintf(stderr, "ALLOC: %p\n", obj);
	class_print(stderr, type);
#endif
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
	size_t fields_nr_space = (len + sizeof(void*)) & ~(sizeof (void*) - 1);
	object_t *obj = heap_allocate_object(&class_string, 1 + (fields_nr_space >> 3));
	obj->members[0].int_v = len;
	memcpy(OBJECT_STRING(obj), string, len + 1);
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
		return !strcmp(OBJECT_STRING(a0), OBJECT_STRING(a1));
	}
	// Ansonsten immer ungleich
	return 0;
}

static void
object_print_internal(FILE *f, object_t *obj, int depth, bool debug, char *sep)
{
	if (depth < 0) {
		fprintf(f, "...");
		return;
	}
	
	if (!obj) {
		fprintf(f, "NULL");
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
		fprintf(f, "%s%s", OBJECT_STRING(obj), loc);
		return;
	} else if (classref == &class_array) {
		fprintf(f, "[");
		for (int i = 0; i < obj->members[0].int_v; i++) {
			if (i > 0) {
				fprintf(f, ",");
			}
			object_print_internal(f, obj->members[i+1].object_v, depth - 1, debug, " ");
		}
		fprintf(f, "]%s", loc);
		return;
	}


	int printed = 0;
	symtab_entry_t *sym = classref->id;
	fprintf(f, "%s@%p {%s", sym->name, loc, sep);
	if (depth <= 0) {
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
						      depth - 1, debug, " ");
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
	object_print_internal(f, obj, depth, debug, "\n");
}

// ---------- Selektoren ---------- //

// Fehlerbehandlung fuer Selektorzugriff
static void
fail_selector_lookup(object_t *obj, ast_node_t *node, int actual_type, int expected_type) __attribute__ ((noreturn));
static void
fail_selector_lookup(object_t *obj, ast_node_t *node, int actual_type, int expected_type)
{
	char message[1024];
	if (!node) {
		fail("Selector lookup failed (no node information available)");
	}
	symtab_entry_t *sym = node->sym;
	if (actual_type) {
		// Typfehler
		if (CLASS_MEMBER_IS_METHOD(actual_type)) {
			sprintf(message, "Tried to access method `%s' as if it were a field", sym->name);
		} else {
			if (CLASS_MEMBER_IS_METHOD(actual_type)) {
				sprintf(message, "Tried to call method `%s' with %d parameters, but it expects %d", sym->name,
					CLASS_MEMBER_METHOD_ARGS_NR(expected_type),
					CLASS_MEMBER_METHOD_ARGS_NR(actual_type));
			} else {
				sprintf(message, "Tried to call field `%s' as if it were a method", sym->name);
			}
		}
	} else {
		sprintf(message, "Object at %p has no method or field `%s'", obj, sym->name);
fprintf(stderr, "Object at %p (class %p) has no method or field `%s'", obj, obj->classref, sym->name);
class_print(stderr, obj->classref);
	}

	fail_at_node(node, message);
}

// Gemeinsamer Code fuer alle Selektorzugriffe
#define LOAD_SELECTOR							\
									\
	if (!obj) {							\
		fail_at_node(node, "Null pointer object dereference");	\
	}								\
	class_t *classref = obj->classref;				\
	const int mask = classref->table_mask;				\
	int index = selector & mask;					\
	unsigned short type;						\
	long long int offset;						\
	while (true) {							\
		const unsigned long long coding = classref->members[index].selector_encoding; \
		if (!coding) {						\
			fail_selector_lookup(obj, node, 0, 0);		\
		}							\
		if (CLASS_DECODE_SELECTOR_ID(coding) != selector) {	\
			index = (index + 1) & mask;			\
			continue;					\
		}							\
		type = CLASS_DECODE_SELECTOR_TYPE(coding);		\
		offset = CLASS_DECODE_SELECTOR_OFFSET(coding);		\
		break;							\
	}


void *
object_get_member_method(object_t *obj, ast_node_t *node, int selector, int parameters_nr)
{
	LOAD_SELECTOR;
	// `type' und `offset' sind nun gesetzt
	if (type != CLASS_MEMBER_METHOD(parameters_nr)) {
		fail_selector_lookup(obj, node, type, CLASS_MEMBER_METHOD(parameters_nr));
	}
	return CLASS_VTABLE(classref)[offset];
}

void *
object_read_member_field_obj(object_t *obj, ast_node_t *node, int selector)
{
	LOAD_SELECTOR;
	// `type' und `offset' sind nun gesetzt

	/* fprintf(stderr, "Reading from field slc=0x%x of object:\n", selector); */
	/* object_print(stderr, obj, true, 3); */
	/* fprintf(stderr, "\nreading from %p\n", &(obj->members[offset].int_v)); */

	if (type == CLASS_MEMBER_VAR_OBJ) {
		return obj->members[offset].object_v;
	} else if (type == CLASS_MEMBER_VAR_INT) {
		return new_int(obj->members[offset].int_v);
	} else {
		fail_selector_lookup(obj, node, type, CLASS_MEMBER_VAR_OBJ);
	}
}

long long int
object_read_member_field_int(object_t *obj, ast_node_t *node, int selector)
{
	LOAD_SELECTOR;
	// `type' und `offset' sind nun gesetzt

	if (type == CLASS_MEMBER_VAR_OBJ) {
		object_t *elt = obj->members[offset].object_v;
		if (elt->classref == &class_boxed_int) {
			return elt->members[0].int_v;
		}
		fail_at_node(node, "attempted to convert non-int object to int value");
	} else if (type == CLASS_MEMBER_VAR_INT) {
		return obj->members[offset].int_v;
	} else {
		fail_selector_lookup(obj, node, type, CLASS_MEMBER_VAR_INT);
	}
}

void
object_write_member_field_int(object_t *obj, ast_node_t *node, int selector, long long int value)
{
	LOAD_SELECTOR;
	//d `type' und `offset' sind nun gesetzt
	//e `type' und `offset' are now set
	if (type == CLASS_MEMBER_VAR_OBJ) {
		obj->members[offset].object_v = new_int(value);
	} else if (type == CLASS_MEMBER_VAR_INT) {
		obj->members[offset].int_v = value;
	} else {
		fail_selector_lookup(obj, node, type, CLASS_MEMBER_VAR_INT);
	}
}

void
object_write_member_field_obj(object_t *obj, ast_node_t *node, int selector, object_t *value)
{
	LOAD_SELECTOR;
	// `type' und `offset' sind nun gesetzt

	if (type == CLASS_MEMBER_VAR_OBJ) {
		obj->members[offset].object_v = value;
	} else if (type == CLASS_MEMBER_VAR_INT) {
		if (!value) {
			fail_at_node(node, "attempted to assign NULL to int field");
		}
		if (value->classref == &class_boxed_int) {
			obj->members[offset].int_v = value->members[0].int_v;
		}
		fail_at_node(node, "attempted to convert non-int object to int value");
	} else {
		fail_selector_lookup(obj, node, type, CLASS_MEMBER_VAR_OBJ);
	}
}
