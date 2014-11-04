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

#include <assert.h>

#include "errors.h"
#include "dynamic-compiler.h"
#include "runtime.h"
#include "baseline-backend.h"
#include "class.h"
#include "registers.h"
#include "assembler.h"
#include "symbol-table.h"
#include "address-store.h"
#include "compiler-options.h"

buffer_t
dyncomp_build_generic()
{
	buffer_t buf = buffer_new(64);
	// Argumentregister sichern
	emit_subi(&buf, REGISTER_SP, sizeof(void*) * REGISTERS_ARGUMENT_NR); // Alle Argumentregister
	for (int i = 0; i < REGISTERS_ARGUMENT_NR; i++) {
		emit_sd(&buf, registers_argument[i], i * sizeof(void*), REGISTER_SP);
	}
	// Der Uebersetzereinsprungpunkt liegt in C, wird entsprechend angesteuert
	emit_move(&buf, REGISTER_A0, REGISTER_V0);
	emit_move(&buf, REGISTER_A1, REGISTER_SP);
	emit_addi(&buf, REGISTER_A1, sizeof(void*) * REGISTERS_ARGUMENT_NR); // Alle Argumentregister
	emit_la(&buf, REGISTER_V0, dyncomp_compile_function);
	emit_jalr(&buf, REGISTER_V0);
	for (int i = 0; i < REGISTERS_ARGUMENT_NR; i++) {
		emit_ld(&buf, registers_argument[i], i * sizeof(void*), REGISTER_SP);
	}
	// Alte Argumentregister wiederherstellen
	emit_addi(&buf, REGISTER_SP, sizeof(void*) * REGISTERS_ARGUMENT_NR);
	emit_jreturn(&buf);
	buffer_terminate(buf);
	if (compiler_options.debug_dynamic_compilation) {
		fprintf(stderr, "Generic compiler entry point:");
		buffer_disassemble(buf);
	}
	return buf;
}


buffer_t
dyncomp_build_trampoline(void *dyncomp_entry, ast_node_t **functions, int functions_nr)
{
	if (!functions_nr) {
		return NULL;
	}
	buffer_t buf = buffer_new(16 * functions_nr);
	for (int i = 0; i < functions_nr; i++) {
		symtab_entry_t *sym = functions[i]->children[0]->sym;
		assert(sym);
		sym->r_trampoline = buffer_target(&buf);
		sym->r_mem = sym->r_trampoline;

		/* fprintf(stderr, "Adding to trampoline: \n"); */
		/* symtab_entry_dump(stderr, sym); */

		if (compiler_options.debug_dynamic_compilation) {
			addrstore_put(sym->r_trampoline, ADDRSTORE_KIND_TRAMPOLINE, sym->name);
		}

		emit_li(&buf, REGISTER_V0, sym->id);
		label_t label;
		emit_jal(&buf, &label);
		buffer_setlabel(&label, dyncomp_entry);
	}
	if (compiler_options.debug_dynamic_compilation) {
		fprintf(stderr, "Trampoline:");
		buffer_disassemble(buf);
	}
	buffer_terminate(buf);
	return buf;
}

void
dyncomp_compile_function(int symtab_entry, void **update_address_on_call_stack)
{
	symtab_entry_t *sym = symtab_lookup(symtab_entry);
	if (!sym || SYMTAB_TY(sym) != SYMTAB_TY_FUNCTION) {
		fprintf(stderr, "dyncomp_compile_funtion(%d, %p):", symtab_entry, update_address_on_call_stack);
		symtab_entry_dump(stderr, sym);
		fail("dynamic function compilation");
	}

	if (sym->symtab_flags & SYMTAB_CONSTRUCTOR) {
		// Klassenobjekt bei Konstruktoruebersetzung bauen
		symtab_entry_t *class_sym = sym->parent;
		class_t *class = class_new(class_sym);
		class_sym->r_mem = class;
		ast_node_t **method_defs = class_sym->astref->children[2]->children + class_sym->vars_nr;
		int method_defs_nr = class_sym->methods_nr;
		class_sym->r_trampoline = dyncomp_build_trampoline(buffer_entrypoint(runtime_current()->dyncomp),
								   method_defs, method_defs_nr);
		if (compiler_options.debug_dynamic_compilation) {
			fprintf(stderr, "Built trampoline:\n");
			buffer_disassemble((buffer_t) class_sym->r_trampoline);
		}
		for (int i = 0; i < method_defs_nr; i++) {
			CLASS_VTABLE(class)[i] = method_defs[i]->children[0]->sym->r_trampoline;
		}
	}

	if (compiler_options.debug_dynamic_compilation) {
		fprintf(stderr, "dyn-compiling `");
		symtab_entry_name_dump(stderr, sym);
		fprintf(stderr, "'\n");
		symtab_entry_dump(stderr, sym);
		fprintf(stderr, "Trampoline address at %p\n", sym->r_trampoline);
		ast_node_dump(stderr, sym->astref, 6 | 8);
	}

	buffer_t body_buf;
	if (sym->symtab_flags & SYMTAB_MEMBER) {
		body_buf = baseline_compile_method(sym);
	} else {
		body_buf = baseline_compile_static_callable(sym);
	}

	sym->r_mem = buffer_entrypoint(body_buf);
	if (compiler_options.debug_dynamic_compilation) {
		buffer_disassemble(body_buf);
	}

	if (update_address_on_call_stack) {
		*update_address_on_call_stack = sym->r_mem;
	}

	sym->symtab_flags |= SYMTAB_COMPILED;

	// Trampolin ueberschreiben
	pseudobuffer_t pbuf;
	buffer_t buf = buffer_pseudobuffer(&pbuf, sym->r_trampoline);
	label_t label;
	emit_j(&buf, &label);
	buffer_setlabel(&label, sym->r_mem);
	if (sym->parent && !(sym->symtab_flags & SYMTAB_CONSTRUCTOR)) {
		// Methode?
		symtab_entry_t *class_sym = sym->parent;
		class_t *class = (class_t *) class_sym->r_mem;
		CLASS_VTABLE(class)[sym->offset] = sym->r_mem;
	}
}
