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
#include <assert.h>
#include <stddef.h>

#include "ast.h"
#include "baseline-backend.h"
#include "assembler.h"
#include "registers.h"
#include "class.h"
#include "object.h"
#include "errors.h"


#define VAR_IS_OBJ

#define FAIL(...) { fprintf(stderr, "[baseline-backend] L%d: Compilation failed:", __LINE__); fprintf(stderr, __VA_ARGS__); exit(1); }

static void
baseline_compile_expr(buffer_t *buf, ast_node_t *ast, int dest_register, int temp_reg_base);

// Gibt Groesse des Stapelrahmens (in Bytes) zurueck
static int
baseline_compile_prepare_arguments(buffer_t *buf, int children_nr, ast_node_t **children, int temp_reg_base);


static void
fail_at_node(ast_node_t *node, char *msg)
{
	fprintf(stderr, "Fatal: %s", msg);
	if (node->source_line) {
		fprintf(stderr, " in line %d\n", node->source_line);
	} else {
		fprintf(stderr, "\n");
	}
	ast_node_dump(stderr, node, AST_NODE_DUMP_FORMATTED | AST_NODE_DUMP_ADDRESS | AST_NODE_DUMP_FLAGS);
	fprintf(stderr, "\n");
	fail("execution halted on error");
}


// Kann dieser Knoten ohne temporaere Register berechnet werden?
static int
is_simple(ast_node_t *n)
{
	return IS_VALUE_NODE(n);
}

static void
emit_optmove(buffer_t *buf, int dest, int src)
{
	if (dest != src) {
		emit_move(buf, dest, src);
	}
}

// Load Address
static void
emit_la(buffer_t *buf, int reg, void *p)
{
	emit_li(buf, reg, (long long) p);
}

static void
baseline_compile_builtin_convert(buffer_t *buf, ast_node_t *arg, int to_ty, int from_ty, int dest_register, int temp_reg_base)
{
	// Annahme: zu konvertierendes Objekt ist in a0
	const int a0 = registers_argument[0];
	const int a1 = registers_argument[1];

#ifdef VAR_IS_OBJ
	if (to_ty == TYPE_VAR) {
		to_ty = TYPE_OBJ;
	}
	if (from_ty == TYPE_VAR) {
		from_ty = TYPE_OBJ;
	}
#endif

	switch (from_ty) {
	case TYPE_INT:
		switch (to_ty) {
		case TYPE_INT:
			return;
		case TYPE_OBJ:
			emit_la(buf, REGISTER_V0, &new_int);
			emit_jalr(buf, REGISTER_V0);
			emit_optmove(buf, dest_register, REGISTER_V0);
			return;
		case TYPE_VAR:
			FAIL("VAR not supported");
			return;
		}
		break;
	case TYPE_OBJ:
		switch (to_ty) {
		case TYPE_INT: {
			emit_ld(buf, REGISTER_T0, a0, 0);
			emit_la(buf, REGISTER_V0, &class_boxed_int);
			relative_jump_label_t jump_label;
			// Falls Integer-Objekt: Springe zur Dekodierung
			emit_beq(buf, REGISTER_T0, REGISTER_V0, &jump_label);
			emit_la(buf, a0, arg);
			emit_la(buf, a1, "attempted to convert non-Integer object to int");
			emit_la(buf, REGISTER_V0, &fail_at_node);
			emit_jalr(buf, REGISTER_V0);
			// Erfolgreiche Dekodierung:
			buffer_setlabel2(&jump_label, *buf);
			emit_ld(buf, dest_register, a0,
				// Int-Wert im Objekt:
				offsetof(object_t, members[0].int_v));
			return;
		}
		case TYPE_OBJ:
			return;
		case TYPE_VAR:
			FAIL("VAR not supported");
			return;
		}
		break;
	case TYPE_VAR:
		switch (to_ty) {
		case TYPE_INT:
			FAIL("VAR not supported");
			return;
		case TYPE_OBJ:
			FAIL("VAR not supported");
			return;
		case TYPE_VAR:
			return;
		}
		break;
	}
	FAIL("Unsupported conversion: %x to %x\n", from_ty, to_ty);
}

static void
baseline_compile_builtin_op(buffer_t *buf, int result_ty, int op, ast_node_t **args, int dest_register, int temp_reg_base)
{
	const int a0 = registers_argument[0];
	const int a1 = registers_argument[1];
	const int a2 = registers_argument[2];

	// Bestimme Anzahl der Parameter
	int args_nr = 2;

	switch (op) {
	case BUILTIN_OP_CONVERT:
	case BUILTIN_OP_NOT:
	case BUILTIN_OP_ALLOCATE:
		args_nr = 1;
		break;

	case BUILTIN_OP_DIV:
		args_nr = 0;
		break;
	}

	// Parameter laden
	if (args_nr) {
		baseline_compile_prepare_arguments(buf, args_nr, args, temp_reg_base);
	}

	switch (op) {
	case BUILTIN_OP_ADD:
		if (dest_register == a0) {
			emit_add(buf, a0, a1);
		} else {
			emit_add(buf, a1, a0);
			emit_optmove(buf, dest_register, a1);
		}
		break;

	case BUILTIN_OP_SUB:
		emit_sub(buf, a0, a1);
		emit_optmove(buf, dest_register, a0);
		break;

	case BUILTIN_OP_DIV:
		baseline_compile_expr(buf, args[0], REGISTER_V0, temp_reg_base);
		baseline_compile_expr(buf, args[1], REGISTER_T0, temp_reg_base);
		emit_li(buf, a2, 0); // muss vor Division auf 0 gesetzt werden
		emit_div_a2v0(buf, registers_temp[0]);
		emit_optmove(buf, dest_register, REGISTER_V0); // Ergebnis in $v0
		break;

	case BUILTIN_OP_MUL:
		if (dest_register == a0) {
			emit_mul(buf, a0, a1);
		} else {
			emit_mul(buf, a1, a0);
			emit_optmove(buf, dest_register, a1);
		}
		break;

	case BUILTIN_OP_CONVERT:
		baseline_compile_builtin_convert(buf, args[0], result_ty, args[0]->type & TYPE_FLAGS, dest_register, temp_reg_base);
		break;

	default:
		FAIL("Unsupported builtin op: %d\n", op);
	}
}


// Gibt Groesse des Stapelrahmens (in Bytes) zurueck
static int
baseline_compile_prepare_arguments(buffer_t *buf, int children_nr, ast_node_t **children, int temp_reg_base)
{
	// Optimierung:  letzter nichtrivialer Parameter muss nicht zwischengesichert werden...
	int start_of_trailing_simple = children_nr;
	for (int i = children_nr - 1; i >= 0; i--) {
		if (is_simple(children[i])) {
			start_of_trailing_simple = i;
		} else break;
	}

	int last_nonsimple = start_of_trailing_simple - 1;
	if (last_nonsimple >= REGISTERS_ARGUMENT_NR) {
		// ... Ausnahme: wenn der letzte nichttriviale Parameter sowieso
		// nicht in Register gehoert, nutzt diese Optimierung nichts
		last_nonsimple = -1;
	}
	// Ende Vorbereitung der Optimierung [last_nonsimple]

	// Stapelrahmen vorbereiten, soweit noetig
	int stack_frame_size = 0;
	if (children_nr > REGISTERS_ARGUMENT_NR) {
		stack_frame_size = (children_nr - REGISTERS_ARGUMENT_NR);
		if (stack_frame_size & 1) {
			++stack_frame_size; // 16-Byte-Ausrichtung, gem. AMD64-ABI
		}

		emit_addi(buf, REGISTER_SP, -(stack_frame_size * 8));
	}

	// Nichttriviale Werte und Werte, die ohnehin auf den Stapel muessen, rekursiv ausrechnen
	for (int i = 0; i < children_nr; i++) {
		int reg = REGISTER_V0;
		
		if (i > REGISTERS_ARGUMENT_NR || !is_simple(children[i])) {
			if (i == last_nonsimple) { // Opt: [non_simple]
				reg = registers_argument[i];
			}
			baseline_compile_expr(buf, children[i], reg, temp_reg_base);

			if (i >=  REGISTERS_ARGUMENT_NR) {
				// In Ziel speichern
				emit_sd(buf, reg, REGISTER_SP, 8 * (i - REGISTERS_ARGUMENT_NR));
			} else if (i != last_nonsimple) {
				// Muss in temporaerem Speicher ablegen
				emit_sd(buf, reg, temp_reg_base, children[i]->storage * 8);
			}
		}
	}

	// Triviale Registerinhalte generieren, vorher berechnete Werte bei Bedarf laden
	for (int i = 0; i < children_nr; i++) {
		if (is_simple(children[i])) {
			if (i >= REGISTERS_ARGUMENT_NR) {
				baseline_compile_expr(buf, children[i], REGISTER_V0, temp_reg_base);
				emit_sd(buf, REGISTER_V0, REGISTER_SP, 8 * (i - REGISTERS_ARGUMENT_NR));
			} else {
				baseline_compile_expr(buf, children[i], registers_argument[i], temp_reg_base);
			}
		} else if (i < REGISTERS_ARGUMENT_NR && i != last_nonsimple) {
			emit_ld(buf, registers_argument[i], temp_reg_base, children[i]->storage * 8);
		}
	}

	return stack_frame_size * 8;
}

// Der Aufrufer speichert; der Aufgerufene haelt sich immer an dest_register
static void
baseline_compile_expr(buffer_t *buf, ast_node_t *ast, int dest_register, int temp_reg_base)
{
	switch (NODE_TY(ast)) {
	case AST_VALUE_INT:
		emit_li(buf, dest_register, AV_INT(ast));
		break;

	case AST_VALUE_STRING:
		emit_la(buf, dest_register, new_string(AV_STRING(ast), strlen(AV_STRING(ast))));
		break;

	case AST_VALUE_ID: {
		int reg;
		symtab_entry_t *sym = ast->sym;
		const int offset = 8 * sym->offset;
		if (SYMTAB_IS_STATIC(sym)) {
			reg = REGISTER_GP;
		} else if (SYMTAB_IS_STACK_DYNAMIC(sym)) {
			reg = REGISTER_FP;
		} else {
			fprintf(stderr, "Don't know how to load this variable:");
			symtab_entry_dump(stderr, sym);
			FAIL("Unable to load variable");
		}
		if (ast->type & AST_FLAG_LVALUE) {
			// Wir wollen nur die Adresse:
			emit_li(buf, dest_register, offset);
			emit_add(buf, dest_register, reg);
		} else {
			emit_ld(buf, dest_register, reg, offset);
		}
	}
		break;

	case AST_NODE_VARDECL:
		if (ast->children[1] == NULL) {
			// Keine Initialisierung?
			break;
		}
	case AST_NODE_ASSIGN:
		baseline_compile_expr(buf, ast->children[0], REGISTER_V0, temp_reg_base);
		emit_push(buf, REGISTER_V0);
		baseline_compile_expr(buf, ast->children[1], REGISTER_V0, temp_reg_base);
		emit_pop(buf, REGISTER_T0);
		emit_sd(buf, REGISTER_V0, REGISTER_T0, 0);
		break;


	case AST_NODE_FUNAPP: {
		// Annahme: Funktionsaufrufe (noch keine Unterstuetzung fuer Selektoren)
		symtab_entry_t *sym = ast->children[0]->sym;
		assert(sym);
		// Besondere eingebaute Operationen werden in einer separaten Funktion behandelt
		if (sym->id < 0 && sym->symtab_flags & SYMTAB_HIDDEN) {
			baseline_compile_builtin_op(buf, ast->type & TYPE_FLAGS, sym->id,
						    ast->children[1]->children, dest_register, temp_reg_base);
		} else {
			// Normaler Funktionsaufruf
			// Argumente laden
			int stack_frame_size = baseline_compile_prepare_arguments(buf, ast->children[1]->children_nr, ast->children[1]->children, temp_reg_base);

			if (!sym->r_mem) {
				fail_at_node(ast, "No code known");
			}
			assert(sym->r_mem); // Sprungadresse muss bekannt sein
			emit_la(buf, REGISTER_V0, sym->r_mem);
			emit_jalr(buf, REGISTER_V0);

			// Stapelrahmen nachbereiten, soweit noetig
			if (stack_frame_size) {
				emit_addi(buf, REGISTER_SP, stack_frame_size);
			}
		}
	}
		break;

	case AST_NODE_BLOCK:
		for (int i = 0; i < ast->children_nr; i++) {
			baseline_compile_expr(buf, ast->children[i], dest_register, temp_reg_base);
		}
		break;

	default:
		ast_node_dump(stderr, ast, 0);
		fail("Unsupported AST fragment");
	}
}

void *builtin_op_print(object_t *arg);  // builtins.c

buffer_t
baseline_compile(ast_node_t *root,
		 void *static_memory)
{
	buffer_t buf = buffer_new(1024);
	emit_push(&buf, REGISTER_GP);
	emit_la(&buf, REGISTER_GP, static_memory);
	baseline_compile_expr(&buf, root, REGISTER_V0, REGISTER_GP);
	/* emit_move(&buf, registers_argument_nr[0], REGISTER_V0); */
	emit_pop(&buf, REGISTER_GP);
	emit_jreturn(&buf);
	buffer_terminate(buf);
	return buf;
}
