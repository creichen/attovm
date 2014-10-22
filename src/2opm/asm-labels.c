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

#include "../chash.h"
#include "../assembler-buffer.h"

typedef struct {
	char *name;
	void *text_location;
	int int_offset;
	bool text_section;
	hashtable_t *label_references;	// contains mallocd label_t* s
	hashtable_t *gp_references;	// contains sint32* s
} relocation_list_t;

static hashtable_t *labels_table = NULL;

char *
mk_unique_string(const char *str);

static void
init_labels_table()
{
	if (!labels_table) {
		labels_table = hashtable_alloc(hashtable_pointer_hash, hashtable_pointer_compare, 4);
	}
}

static relocation_list_t *
get_label_relocation_list(char *name)
{
	name = mk_unique_string(name);
	init_labels_table();
	relocation_list_t *rellist = (relocation_list_t *) hashtable_get(labels_table, name);
	if (!rellist) {
		rellist = calloc(1, sizeof(relocation_list_t));
		hashtable_put(labels_table, name, rellist, NULL);
		rellist->name = name;
		rellist->label_references = hashtable_alloc(hashtable_pointer_hash, hashtable_pointer_compare, 3);
		rellist->gp_references = hashtable_alloc(hashtable_pointer_hash, hashtable_pointer_compare, 3);
	}
	return rellist;
}

void
relocation_add_label(char *name, void *offset)
{
	relocation_list_t *rellist = get_label_relocation_list(name);
	if (rellist->text_location || rellist->int_offset) {
		error("Multiple definitions of label `%s'", name);
		return;
	}

	rellist->text_section = in_text_section;
	if (in_text_section) {
		rellist->text_location = offset;
	} else {
		rellist->int_offset = ((char *)offset) - ((char *)data_section);
	}
}


static void
relocate_label(void *key, void *value, void *state)
{
	label_t *label = (label_t *) value;
	relocation_list_t *rellist = (relocation_list_t *) state;
	if (!rellist->text_section) {
		error("Label %s defined for data but used in code", rellist->name);
	} else {
		buffer_setlabel(label, rellist->text_location);
		free(label);
	}
}

static void
relocate_displacement(void *key, void *value, void *state)
{
	relocation_list_t *rellist = (relocation_list_t *) state;
	if (rellist->text_section) {
		error("Label %s defined for code but used in data", rellist->name);
	} else {
		*((int *) key) = rellist->int_offset;
	}
}

// relocate relocation_list
static void
relocate_one(void *key, void *value, void *state)
{
	relocation_list_t *rellist = (relocation_list_t *) value;
	if (!rellist->text_location && !rellist->int_offset) {
		error("Label `%s' used but not defined!", rellist->name);
		return;
	}
	hashtable_foreach(rellist->label_references, relocate_label, rellist);
	hashtable_foreach(rellist->gp_references, relocate_displacement, rellist);
}

void
relocation_finish(void)
{
	init_labels_table();
	hashtable_foreach(labels_table, relocate_one, NULL);
}

label_t *
relocation_add_jump_label(char *label)
{
	relocation_list_t *t = get_label_relocation_list(label);
	label_t *retval = malloc(sizeof(label_t));
	hashtable_put(t->label_references, retval, retval, NULL);
	return retval;
}

void
relocation_add_gp_label(char *label, void *addr, int offset)
{
	relocation_list_t *t = get_label_relocation_list(label);
	hashtable_put(t->gp_references,
		      ((unsigned char *) addr) + offset,
		      (void *) ((signed long long) offset), NULL);
}

void *
relocation_get_resolved_text_label(char *name)
{
	relocation_list_t *list = get_label_relocation_list(name);
	if (list->text_section) {
		return list->text_location;
	}
	error("Required label `%s' not defined or not in text segment", name);
	return NULL;
}
