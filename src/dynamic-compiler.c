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

#include "address-store.h"
#include "assembler.h"
#include "baseline-backend.h"
#include "class.h"
#include "compiler-options.h"
#include "dynamic-compiler.h"
#include "analysis.h"
#include "errors.h"
#include "object.h"
#include "registers.h"
#include "runtime.h"
#include "symbol-table.h"

//#define DEBUG_DYNAMIC_COLLECTION
//#define DEBUG_DYNAMIC_OPTIMISATION

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
		symtab_entry_t *sym = AST_CALLABLE_SYMREF(functions[i]);
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

static void
dyncomp_compile_and_update(symtab_entry_t *sym)
{
	if (sym->symtab_flags & SYMTAB_CONSTRUCTOR) {
		//d Klassenobjekt bei Konstruktoruebersetzung bauen
		//e Build class object when compiling constructor
		symtab_entry_t *class_sym = sym->parent;
		class_t *class = class_new(class_sym);
		class_sym->r_mem = class;
		ast_node_t **method_defs = class_sym->astref->children[2]->children + class_sym->storage.fields_nr;
		int method_defs_nr = class_sym->storage.functions_nr;
		class_sym->r_trampoline = dyncomp_build_trampoline(buffer_entrypoint(runtime_current()->dyncomp),
								   method_defs, method_defs_nr);
		if (compiler_options.debug_dynamic_compilation) {
			fprintf(stderr, "Built trampoline:\n");
			buffer_disassemble((buffer_t) class_sym->r_trampoline);
		}
		for (int i = 0; i < method_defs_nr; i++) {
			CLASS_VTABLE(class)[i] = AST_CALLABLE_SYMREF(method_defs[i])->r_trampoline;
		}
	}

	if (compiler_options.debug_dynamic_compilation) {
		fflush(NULL);
		fprintf(stderr, "dyn-compiling `");
		symtab_entry_name_dump(stderr, sym);
		fprintf(stderr, "'\n");
		symtab_entry_dump(stderr, sym);
		fprintf(stderr, "Trampoline address at %p\n", sym->r_trampoline);
		AST_DUMP(sym->astref);
		fflush(NULL);
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

	sym->symtab_flags |= SYMTAB_COMPILED;

	//d Trampolin ueberschreiben
	//e re-write trampoline
	pseudobuffer_t pbuf;
	buffer_t buf = buffer_pseudobuffer(&pbuf, sym->r_trampoline);
	label_t label;
	emit_j(&buf, &label);
	buffer_setlabel(&label, sym->r_mem);

	if (sym->parent && !(sym->symtab_flags & SYMTAB_CONSTRUCTOR)) {
		//d Methode?
		//e method?  If so, update symbol table
		symtab_entry_t *class_sym = sym->parent;
		class_t *class = (class_t *) class_sym->r_mem;
		CLASS_VTABLE(class)[sym->offset] = sym->r_mem;
	}
}

void
dyncomp_compile_function(int symtab_entry, void **update_address_on_call_stack)
{
	symtab_entry_t *sym = symtab_lookup(symtab_entry);
	if (!sym || SYMTAB_KIND(sym) != SYMTAB_KIND_FUNCTION) {
		fprintf(stderr, "dyncomp_compile_funtion(%d, %p):", symtab_entry, update_address_on_call_stack);
		symtab_entry_dump(stderr, sym);
		fail("dynamic function compilation");
	}

	dyncomp_compile_and_update(sym);

	if (update_address_on_call_stack) {
		*update_address_on_call_stack = sym->r_mem;
	}
}

static void
dyncomp_print_current_dynamic_parameter_types(symtab_entry_t *sym)
{
	symtab_entry_name_dump(stderr, sym);
	fprintf(stderr, "(");
	for (int i = 0; i < sym->parameters_nr; i++) {
		if (i > 0) {
			fprintf(stderr, ",");
		}
		class_print_short(stderr, sym->dynamic_parameter_types[i]);
	}
	fprintf(stderr, ")\n");
}

static void
dyncomp_opt_compile(symtab_entry_t *sym)
{
	if (compiler_options.debug_dynamic_compilation || compiler_options.debug_adaptive) {
		fprintf(stderr, "opt-compiling `");
		symtab_entry_name_dump(stderr, sym);
		fprintf(stderr, "' for ");
		dyncomp_print_current_dynamic_parameter_types(sym);
	}
	//e run optimisations
	data_flow_analyses(sym, data_flow_analyses_optimisation);

	sym->symtab_flags |= SYMTAB_OPT;
	
	dyncomp_compile_and_update(sym);
}

void *
dyncomp_deoptimise(symtab_entry_t *sym)
{
	if (compiler_options.debug_dynamic_compilation || compiler_options.debug_adaptive) {
		fprintf(stderr, "de-optimising `");
		symtab_entry_name_dump(stderr, sym);
		fprintf(stderr, "'\n");
	}
	//e run optimisations
	data_flow_analyses(sym, data_flow_analyses_optimisation);

	sym->symtab_flags &= ~SYMTAB_OPT;

	dyncomp_compile_and_update(sym);
	return sym->r_mem;
}

void
dyncomp_init_unoptimised(symtab_entry_t *sym)
{
	sym->fast_hotness_counter = DYNCOMP_ADAPTIVE_SAMPLE_INTERVALS; /*e reset sample counter */
	sym->slow_hotness_counter = DYNCOMP_ADAPTIVE_OPT_THRESHOLD;
	if (sym->parameters_nr) {
		if (!sym->dynamic_parameter_types) {
			sym->dynamic_parameter_types = calloc(sym->parameters_nr, sizeof(class_t *));
		}
		for (int i = 0; i < sym->parameters_nr; i++) {
			if (sym->parameter_types[i] == TYPE_OBJ) {
				sym->dynamic_parameter_types[i] = &class_bottom;
			}
		}
	}
}

void
dyncomp_runtime_sample(symtab_entry_t *sym, object_t** low_args, object_t** high_args)
{
	for (int i = 0; i < sym->parameters_nr; i++) {
		/*e skip NULL entries in dynamic parameter list (those are value parameters) */
		if (sym->dynamic_parameter_types[i]) {
			class_t *current_classref = sym->dynamic_parameter_types[i];
			const int arg_index = i + (SYMTAB_HAS_SELF(sym) ? 1 : 0);
			object_t *obj;
			if (arg_index >= REGISTERS_ARGUMENT_NR) {
				obj = high_args[REGISTERS_ARGUMENT_NR - i];
			} else {
				obj = low_args[i];
			}

			if (!obj) {
				continue;
			}
			class_t *classref = obj->classref;

			if (classref == current_classref || current_classref == &class_bottom) {
				sym->dynamic_parameter_types[i] = classref;
			} else {
				sym->dynamic_parameter_types[i] = &class_top;
			}
		}
	}
#ifdef DEBUG_DYNAMIC_COLLECTION
	fprintf(stderr, "#dynamic ");
	dyncomp_print_current_dynamic_parameter_types(sym);
#endif
	if (--sym->slow_hotness_counter == 0) {
		/*e Let's optimise! */
#ifdef DEBUG_DYNAMIC_OPTIMISATION
		fprintf(stderr, "#optimise: ");
#endif
		dyncomp_opt_compile(sym);
	} else {
		/*e Let's sample some more! */
		sym->fast_hotness_counter = DYNCOMP_ADAPTIVE_SAMPLE_INTERVALS; /*e reset sample counter */
	}
}
