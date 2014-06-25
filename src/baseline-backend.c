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
#include <stdbool.h>
#include <stdio.h>

#include "address-store.h"
#include "assembler.h"
#include "ast.h"
#include "baseline-backend.h"
#include "class.h"
#include "compiler-options.h"
#include "errors.h"
#include "object.h"
#include "registers.h"


#define VAR_IS_OBJ

#define FAIL(...) { fprintf(stderr, "[baseline-backend] L%d: Compilation failed: ", __LINE__); fprintf(stderr, __VA_ARGS__); exit(1); }

typedef struct relative_jump_label_list {
	label_t label;
	struct relative_jump_label_list *next;
} relative_jump_label_list_t;

// Uebersetzungskontext
typedef struct {
	// Fuer den aktuellen Ausfuehrungsrahmen:  Welches Register indiziert temporaere Variablen?
	int temp_reg_base;
	int stack_depth; // Wieviele Variablen auf dem Ablagestapel alloziert sind
	int variable_storage; // Speicherstelle fuer Variable

	// Falls verfuegbar/in Schleife: Sprungmarken 
	relative_jump_label_list_t *continue_labels, *break_labels;
} context_t;

// Makros fuer PUSH und POP, die automatisch die Stapeltiefe aktualisieren
// Wenn nur diese Makros zum Veraendern von SP verwendet werden, koennen wir sicherstellen,
//  dass stack_depth immer aktuell ist
#define PUSH(REG) emit_push(buf, (REG)); context->stack_depth++
#define POP(REG) emit_pop(buf, (REG)); context->stack_depth--
#define STACK_ALLOC(DSIZE); if (DSIZE) {emit_subiu(buf, REGISTER_SP, sizeof(void *) * (DSIZE)); context->stack_depth += (DSIZE); }
#define STACK_FREE(DSIZE); if (DSIZE) {emit_addiu(buf, REGISTER_SP, sizeof(void *) * (DSIZE)); context->stack_depth -= (DSIZE); }

#define MARK_LINE()  {emit_addiu(buf, REGISTER_FP, __LINE__);emit_subiu(buf, REGISTER_FP, __LINE__);}

static void
context_copy(context_t *dest, context_t *src)
{
	memcpy(dest, src, sizeof(context_t));
}

static label_t *
jll_add_label(relative_jump_label_list_t **list)
{
	relative_jump_label_list_t *node = (relative_jump_label_list_t *) malloc(sizeof (relative_jump_label_list_t));
	node->next = *list;
	*list = node;
	return &node->label;
}

static void
jll_labels_resolve(relative_jump_label_list_t **list, void *addr)
{
	while (*list) {
		relative_jump_label_list_t *node = *list;
		*list = node->next;
		buffer_setlabel(&node->label, addr);
		free(node);
	}
}

static void
baseline_compile_expr(buffer_t *buf, ast_node_t *ast, int dest_register, context_t *context);

long long int builtin_op_obj_test_eq(object_t *a0, object_t *a1);


// Gibt Groesse des Stapelrahmens (in Bytes) zurueck
static int
baseline_compile_prepare_arguments(buffer_t *buf, int children_nr, ast_node_t **children, context_t *context, bool mustalign);


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

static void
dump_ast(char *msg, ast_node_t *ast)
{
	fprintf(stderr, "[baseline-backend] %s\n", msg);
	ast_node_dump(stderr, ast, AST_NODE_DUMP_FORMATTED | AST_NODE_DUMP_FLAGS | AST_NODE_DUMP_ADDRESS);
	fprintf(stderr, "\n");
}

// Kann dieser Knoten ohne temporaere Register berechnet werden?
static int
is_simple(ast_node_t *n)
{
	return IS_VALUE_NODE(n) || NODE_TY(n) == AST_NODE_NULL;
}

static void
emit_optmove(buffer_t *buf, int dest, int src)
{
	if (dest != src) {
		emit_move(buf, dest, src);
	}
}

static void
emit_fail_at_node(buffer_t *buf, ast_node_t *node, char *msg)
{
	emit_la(buf, REGISTER_A0, node);
	emit_la(buf, REGISTER_A1, msg);
	emit_la(buf, REGISTER_V0, &fail_at_node);
	emit_jals(buf, REGISTER_V0);
}



static void
baseline_compile_builtin_convert(buffer_t *buf, ast_node_t *arg, int to_ty, int from_ty, int dest_register, context_t *context)
{
	// Annahme: zu konvertierendes Objekt ist in a0

#ifdef VAR_IS_OBJ
	if (to_ty == TYPE_VAR) {
		to_ty = TYPE_OBJ;
	}
	if (from_ty == TYPE_VAR) {
		from_ty = TYPE_OBJ;
	}
#endif

	bool uses_function_call = false;
	if (to_ty != from_ty
	    && to_ty == TYPE_OBJ) {
		// Verpacken (boxing) immer mit Funktionsaufruf
		uses_function_call = true;
	}

	int prep_stack_frame_size = baseline_compile_prepare_arguments(buf, 1, &arg, context, uses_function_call);
	STACK_FREE(prep_stack_frame_size);


	switch (from_ty) {
	case TYPE_INT:
		switch (to_ty) {
		case TYPE_INT:
			return;
		case TYPE_OBJ:
			emit_la(buf, REGISTER_V0, &new_int);
			emit_jals(buf, REGISTER_V0);
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
			label_t null_label;
			emit_beqz(buf, REGISTER_A0, &null_label); // NULL-Parameter?
			emit_ld(buf, REGISTER_T0, 0, REGISTER_A0);
			emit_la(buf, REGISTER_V0, &class_boxed_int);
			label_t jump_label;
			// Falls Integer-Objekt: Springe zur Dekodierung
			buffer_setlabel2(&null_label, buf);
			emit_beq(buf, REGISTER_T0, REGISTER_V0, &jump_label);
			emit_fail_at_node(buf, arg, "attempted to convert non-Integer object to int");
			// Erfolgreiche Dekodierung:
			buffer_setlabel2(&jump_label, buf);
			emit_ld(buf, dest_register,
				// Int-Wert im Objekt:
				offsetof(object_t, members[0].int_v),
				REGISTER_A0);
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

	dump_ast("Trouble with AST during type conversion", arg);
	FAIL("Unsupported conversion: %x to %x\n", from_ty, to_ty);
}

static void
baseline_compile_builtin_eq(buffer_t *buf, int dest_register,
			    int a0_ty,
			    int a1_ty, context_t *context)
{

	//#ifdef VAR_IS_OBJ
	if (a0_ty == TYPE_VAR) {
		a0_ty = TYPE_OBJ;
	}
	if (a1_ty == TYPE_VAR) {
		a1_ty = TYPE_OBJ;
	}
	//#endif

	bool temp_object = false;
	if (a0_ty != a1_ty) {
		// Int -> Object
		if (a0_ty == TYPE_INT || a1_ty == TYPE_INT) {
			// Einer der Typen muss nach Obj konvertiert werden
			int conv_reg;
			if (a0_ty == TYPE_INT) {
				conv_reg = REGISTER_A0;
				a0_ty = TYPE_OBJ;
			} else {
				conv_reg = REGISTER_A1;
				a1_ty = TYPE_OBJ;
			}
			temp_object = true;
			// Temporaeres Objekt auf dem Stapel erzeugen
			PUSH(conv_reg);
			emit_la(buf, conv_reg, &class_boxed_int);
			PUSH(conv_reg);
			emit_move(buf, conv_reg, REGISTER_SP);
		}

		// Noch nicht implementiert
		assert(a0_ty != TYPE_VAR && a1_ty != TYPE_VAR);
	}

	// Beide Typen sind nun gleich
	switch (a0_ty) {

	case TYPE_INT:
		emit_seq(buf, dest_register, REGISTER_A0, REGISTER_A1);
		break;

	case TYPE_OBJ:
		emit_la(buf, REGISTER_V0, builtin_op_obj_test_eq);
		emit_jals(buf, REGISTER_V0);
		break;

	case TYPE_VAR:
	default:
		 FAIL("Unsupported type equality check: %x\n", a0_ty);
	}

	if (temp_object) {
		// Temporaeres Objekt vom Stapel deallozieren
		STACK_FREE(2);
	}

}


static void
baseline_compile_builtin_op(buffer_t *buf, int result_ty, int op, ast_node_t **args, int dest_register, context_t *context)
{
	// Bestimme Anzahl der Parameter
	int args_nr = 2;

	switch (op) {
	case BUILTIN_OP_CONVERT:
		args_nr = 0; // hier nicht vorbereitet, da wir nicht so leicht testen koennen, ob der Stapel ausgerichtet werden muss
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
		int prep_stack_frame_size = baseline_compile_prepare_arguments(buf, args_nr, args, context, 0);
		STACK_FREE(prep_stack_frame_size);
	}

	switch (op) {
	case BUILTIN_OP_ADD:
		if (dest_register == REGISTER_A0) {
			emit_add(buf, REGISTER_A0, REGISTER_A1);
		} else {
			emit_add(buf, REGISTER_A1, REGISTER_A0);
			emit_optmove(buf, dest_register, REGISTER_A1);
		}
		break;

	case BUILTIN_OP_SUB:
		emit_sub(buf, REGISTER_A0, REGISTER_A1);
		emit_optmove(buf, dest_register, REGISTER_A0);
		break;

	case BUILTIN_OP_NOT:
		emit_not(buf, dest_register, REGISTER_A0);
		break;

	case BUILTIN_OP_TEST_EQ:
		baseline_compile_builtin_eq(buf, dest_register,
					    args[0]->type & TYPE_FLAGS,
					    args[1]->type & TYPE_FLAGS,
					    context);
		break;

	case BUILTIN_OP_TEST_LE:
		emit_sle(buf, dest_register, REGISTER_A0, REGISTER_A1);
		break;

	case BUILTIN_OP_TEST_LT:
		emit_slt(buf, dest_register, REGISTER_A0, REGISTER_A1);
		break;

	case BUILTIN_OP_DIV:
		baseline_compile_expr(buf, args[0], REGISTER_V0, context);
		baseline_compile_expr(buf, args[1], REGISTER_T0, context);
		emit_li(buf, REGISTER_A2, 0); // muss vor Division auf 0 gesetzt werden
		emit_div_a2v0(buf, registers_temp[0]);
		emit_optmove(buf, dest_register, REGISTER_V0); // Ergebnis in $v0
		break;

	case BUILTIN_OP_MUL:
		if (dest_register == REGISTER_A0) {
			emit_mul(buf, REGISTER_A0, REGISTER_A1);
		} else {
			emit_mul(buf, REGISTER_A1, REGISTER_A0);
			emit_optmove(buf, dest_register, REGISTER_A1);
		}
		break;

	case BUILTIN_OP_CONVERT:
		baseline_compile_builtin_convert(buf, args[0], result_ty, args[0]->type & TYPE_FLAGS, dest_register, context);
		break;

	default:
		FAIL("Unsupported builtin op: %d\n", op);
	}
}


// Gibt Groesse des Stapelrahmens (in Bytes) zurueck
// Stellt sicher, dass der Stapelspeicher angemessen ausgerichtet ist
static int
baseline_compile_prepare_arguments(buffer_t *buf, int children_nr, ast_node_t **children, context_t *context, bool mustalign)
{
	/* Aufrufkonventionen gem. System V ABI fuer x86-64:
	 * $a0-$a5 haben Parameter 0-5
	 *
	 * |    X    |
         * +---------+
	 * | param 7 |
         * +---------+
	 * | param 6 |
         * +---------+  <- $sp
	 *
	 * Zusaeztlich muss $sp 16-Byte-ausgerichtet sein.
	 * Wir legen in X zusaetzlich noch Platz fuer Parameter 0-5 ab, falls noetig, ebenfalls in aufsteigender
	 * Reihenfolge.  Falls ein Ausrichtungseintrag (fuer 16-Byte-Ausrichtung) noetig ist, wird es wiederum
	 * davor abgelegt.
	 */
	const int stack_args_nr = children_nr < REGISTERS_ARGUMENT_NR ? 0 : children_nr - REGISTERS_ARGUMENT_NR;
	bool stack_frame_active_during_call =
		(mustalign && context->stack_depth & 1) // Ausrichtung extern aufgezwungen
		|| stack_args_nr;

	int backup_space_needed = 0; // Anzahl der $a-Register, die wir sichern muessen

	int last_nonsimple = -1; // letzte nichttriviale Berechnung
	for (int i = 0; i < children_nr; i++) {
		if (!is_simple(children[i])) {
			last_nonsimple = i;
			if (i < REGISTERS_ARGUMENT_NR) {
				++backup_space_needed;
			}
		}
	}

	if (backup_space_needed && last_nonsimple < REGISTERS_ARGUMENT_NR) {
		// kein Backup fuer den letzten nichttrivialen Eintrag noetig
		--backup_space_needed;
	}

static int zeta_c = 0;
 int zeta = zeta_c++;
 fprintf(stderr, "[%d] Spill space requested = %d (lastnonsimple = %d, argsnr=%d)\n", zeta, backup_space_needed, last_nonsimple, children_nr);

	// Adresse relativ zu $sp, in der bei Bedarf $a0..$a5 gesichert werden
	const int register_arg_spill_space = children_nr < REGISTERS_ARGUMENT_NR ? 0 : children_nr - REGISTERS_ARGUMENT_NR;

	int stack_frame_size = backup_space_needed
		+ ((children_nr < REGISTERS_ARGUMENT_NR)? 0 : children_nr - REGISTERS_ARGUMENT_NR);

	if (stack_frame_active_during_call
	    && (stack_frame_size + context->stack_depth) & 1) { // Ausrichtung noetig?
		++stack_frame_size;  // Ausrichtung
	}

	STACK_ALLOC(stack_frame_size);

	int spill_counter = 0;
	// Nichttriviale Werte und Werte, die ohnehin auf den Stapel muessen, rekursiv ausrechnen
	for (int i = 0; i < children_nr; i++) {
		int reg = REGISTER_V0;
		if (i < REGISTERS_ARGUMENT_NR){
			reg = registers_argument[i];
		}
		
		if (i >= REGISTERS_ARGUMENT_NR || !is_simple(children[i])) {
			baseline_compile_expr(buf, children[i], reg, context);

			int dest_stack_location;
			if (i >= REGISTERS_ARGUMENT_NR) {
				dest_stack_location = i - REGISTERS_ARGUMENT_NR;
			} else {
				dest_stack_location = register_arg_spill_space + spill_counter++;
			}

			if (i != last_nonsimple) {
				emit_sd(buf, reg,
					sizeof(void *) * dest_stack_location,
					REGISTER_SP);
			}
		}
	}

	spill_counter = 0;
	// Triviale Registerinhalte generieren, vorher berechnete Werte bei Bedarf laden
	const int max = children_nr < REGISTERS_ARGUMENT_NR ? children_nr : REGISTERS_ARGUMENT_NR;
	for (int i = 0; i < max; i++) {
		if (is_simple(children[i])) {
			if (i < REGISTERS_ARGUMENT_NR) {
				baseline_compile_expr(buf, children[i], registers_argument[i], context);
			}
		} else if (i != last_nonsimple) { // Wert der letzten nichttrivialen Berechnung ist noch frisch
			emit_ld(buf, registers_argument[i],
				sizeof(void *) * (register_arg_spill_space + spill_counter++),
				REGISTER_SP);
		}
	}

	fprintf(stderr, "[%d] Spill space used = %d / %d, sfsize = %d\n", zeta, spill_counter, backup_space_needed, stack_frame_size);

	if (!stack_frame_active_during_call) {
		STACK_FREE(stack_frame_size);
		return 0;
	}

	return stack_frame_size;
}

// Der Aufrufer speichert; der Aufgerufene haelt sich immer an dest_register
static void
baseline_compile_expr(buffer_t *buf, ast_node_t *ast, int dest_register, context_t *context)
{
	switch (NODE_TY(ast)) {
	case AST_VALUE_INT:
		emit_li(buf, dest_register, AV_INT(ast));
		break;

	case AST_VALUE_STRING: {
		void *addr = new_string(AV_STRING(ast), strlen(AV_STRING(ast)));
		emit_la(buf, dest_register, addr);
		addrstore_put(addr, ADDRSTORE_KIND_STRING_LITERAL, AV_STRING(ast));
	}
		break;

	case AST_VALUE_ID: {
		int reg;
		symtab_entry_t *sym = ast->sym;
		const int offset = sizeof(void *) * sym->offset;
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
			emit_ld(buf, dest_register, offset, reg);
		}
	}
		break;

	case AST_NODE_VARDECL:
		ast->sym->offset = context->variable_storage++;
		if (ast->children[1] == NULL) {
			// Keine Initialisierung?
			break;
		}
		// fall through
	case AST_NODE_ASSIGN:
		baseline_compile_expr(buf, ast->children[0], REGISTER_V0, context);
		PUSH(REGISTER_V0);
		baseline_compile_expr(buf, ast->children[1], REGISTER_V0, context);
		POP(REGISTER_T0);
		emit_sd(buf, REGISTER_V0, 0, REGISTER_T0);
		break;

	case AST_NODE_ARRAYVAL: {
		if (ast->children[1]) {
			// Laden mit expliziter Groessenangabe
			baseline_compile_expr(buf, ast->children[1], REGISTER_A0, context);
			if (!compiler_options.no_bounds_checks) {
				// Arraygrenzenpruefung
				emit_li(buf, REGISTER_T0, ast->children[0]->children_nr);
				label_t jl;
				emit_ble(buf, REGISTER_T0, REGISTER_A0, &jl);
				emit_fail_at_node(buf, ast, "Requested array size is smaller than number of array elements");
				buffer_setlabel2(&jl, buf);
			}
		} else {
			// Laden mit impliziter Groesse
			emit_li(buf, REGISTER_A0, ast->children[0]->children_nr);
		}
		emit_la(buf, REGISTER_V0, &new_array);
		emit_jals(buf, REGISTER_V0);
		// We now have the allocated array in REGISTER_V0
		PUSH(REGISTER_V0);
		for (int i = 0; i < ast->children[0]->children_nr; i++) {
			ast_node_t *child = ast->children[0]->children[i];
			baseline_compile_expr(buf, child, REGISTER_T0, context);
			if (!is_simple(child)) {
				emit_ld(buf, REGISTER_V0, 0, REGISTER_SP);
			}
			emit_sd(buf, REGISTER_T0,
				16 /* header + groesse */ + 8 * i,
				REGISTER_V0);
		}
		POP(dest_register);
	}
		break;

	case AST_NODE_ARRAYSUB: {
		baseline_compile_expr(buf, ast->children[0], REGISTER_V0, context);
		emit_la(buf, REGISTER_T1, &class_array);
		emit_ld(buf, REGISTER_T0, 0, REGISTER_V0);

		label_t jl;
		emit_beq(buf, REGISTER_T0, REGISTER_T1, &jl);
		emit_fail_at_node(buf, ast, "Attempted to index non-array");
		buffer_setlabel2(&jl, buf);

		PUSH( REGISTER_V0);
		baseline_compile_expr(buf, ast->children[1], REGISTER_T0, context);
		POP(REGISTER_V0);

		if (!compiler_options.no_bounds_checks) {
			// Arraygrenzenpruefung
			emit_ld(buf, REGISTER_T1, 8, REGISTER_V0);
			// v0: Array
			// t0: offset
			// t1: size
		
			emit_bgez(buf, REGISTER_T0, &jl);
			emit_fail_at_node(buf, ast, "Negative index into array");
			buffer_setlabel2(&jl, buf);

			emit_blt(buf, REGISTER_T0, REGISTER_T1, &jl);
			emit_fail_at_node(buf, ast, "Index into array out of bounds");
			buffer_setlabel2(&jl, buf);
		}

		emit_slli(buf, REGISTER_T0, REGISTER_T0, 3);
		emit_add(buf, REGISTER_V0, REGISTER_T0);

		// index is valid
		if (ast->type & AST_FLAG_LVALUE) {
			emit_addiu(buf, REGISTER_V0, 16); // +16 um ueber Typ-ID und Arraygroesse zu springen
			emit_optmove(buf, dest_register, REGISTER_V0);
		} else {
			emit_ld(buf, dest_register, 16, REGISTER_V0);
		}
	}
		break;

	case AST_NODE_IF: {
		baseline_compile_expr(buf, ast->children[0], REGISTER_V0, context);
		label_t false_label, end_label;
		emit_beqz(buf, REGISTER_V0, &false_label);
		baseline_compile_expr(buf, ast->children[1], REGISTER_V0, context);
		if (ast->children[2]) {
			emit_j(buf, &end_label);
			buffer_setlabel2(&false_label, buf);
			baseline_compile_expr(buf, ast->children[2], REGISTER_V0, context);
			buffer_setlabel2(&end_label, buf);
		} else {
			buffer_setlabel2(&false_label, buf);
		}
		break;
	}

	case AST_NODE_WHILE: {
		label_t loop_label, exit_label;
		void *loop_target = buffer_target(buf);

		// Schleife beendet?
		baseline_compile_expr(buf, ast->children[0], REGISTER_V0, context);
		emit_beqz(buf, REGISTER_V0, &exit_label);

		// Schleifenkoerper
		context_t context_backup;  // Kontext sichern (s.u.)
		context_copy(&context_backup, context);
		context->continue_labels = NULL;
		context->break_labels = NULL;
		baseline_compile_expr(buf, ast->children[1], REGISTER_V0, context);
		emit_j(buf, &loop_label);

		// Schleifenende, und Sprungmarken einsetzen
		void *exit_target = buffer_target(buf);
		buffer_setlabel(&loop_label, loop_target);
		buffer_setlabel(&exit_label, exit_target);
		// Break-Continue-Sprungmarken binden
		jll_labels_resolve(&context->continue_labels, loop_target);
		jll_labels_resolve(&context->break_labels, exit_target);

		// Kontext wiederherstellen, damit umgebende Schleifen wieder Zugriff auf ihre `continue_label' und `break_label' erhalten
		context_copy(context, &context_backup);
	}
		break;

	case AST_NODE_CONTINUE:
		// Sprungbefehl fuer spaeter merken
		emit_j(buf, jll_add_label(&context->continue_labels));
		break;

	case AST_NODE_BREAK:
		// Sprungbefehl fuer spaeter merken
		emit_j(buf, jll_add_label(&context->break_labels));
		break;

	case AST_NODE_FUNAPP: {
		// Annahme: Funktionsaufrufe (noch keine Unterstuetzung fuer Selektoren)
		symtab_entry_t *sym = ast->children[0]->sym;
		assert(sym);
		// Besondere eingebaute Operationen werden in einer separaten Funktion behandelt
		if (sym->id < 0 && sym->symtab_flags & SYMTAB_HIDDEN) {
			baseline_compile_builtin_op(buf, ast->type & TYPE_FLAGS, sym->id,
						    ast->children[1]->children, dest_register, context);
		} else {
			// Normaler Funktionsaufruf
			// Argumente laden
			int stack_frame_size = baseline_compile_prepare_arguments(buf, ast->children[1]->children_nr, ast->children[1]->children, context, 1);

			if (!sym->r_mem) {
				fail_at_node(ast, "No code known");
			}
			assert(sym->r_mem); // Sprungadresse muss bekannt sein
			emit_la(buf, REGISTER_V0, sym->r_mem);
			emit_jals(buf, REGISTER_V0);

			// Stapelrahmen nachbereiten, soweit noetig
			if (stack_frame_size) {
				STACK_FREE(stack_frame_size);
			}
			emit_optmove(buf, dest_register, REGISTER_V0);
		}
	}
		break;

	case AST_NODE_BLOCK:
		for (int i = 0; i < ast->children_nr; i++) {
			baseline_compile_expr(buf, ast->children[i], dest_register, context);
		}
		break;

	case AST_NODE_SKIP:
		// Nichts zu tun
		break;

	case AST_NODE_NULL:
		emit_la(buf, dest_register, NULL);
		break;

	case AST_NODE_ISINSTANCE: {
		label_t null_label;
		baseline_compile_expr(buf, ast->children[0], REGISTER_T0, context);
		emit_li(buf, dest_register, 0);
		emit_beqz(buf, REGISTER_T0, &null_label);
		emit_ld(buf, REGISTER_T1, 0, REGISTER_T0);
		emit_la(buf, REGISTER_T0, ast->children[1]->sym->r_mem);
		emit_seq(buf, dest_register, REGISTER_T0, REGISTER_T1);
		buffer_setlabel2(&null_label, buf);
	}
		break;

	default:
		dump_ast("Trouble with AST:", ast);
		fail("Unsupported AST fragment");
	}
}

void *builtin_op_print(object_t *arg);  // builtins.c

buffer_t
baseline_compile(ast_node_t *root,
		 void *static_memory)
{
	static bool initialised_address_store = false;
	if (!initialised_address_store) {
		initialised_address_store = true;
		ADDRSTORE_PUT(fail_at_node, SPECIAL);
		ADDRSTORE_PUT(new_int, SPECIAL);
		ADDRSTORE_PUT(new_array, SPECIAL);
		ADDRSTORE_PUT(new_real, SPECIAL);
		ADDRSTORE_PUT(new_string, SPECIAL);
		ADDRSTORE_PUT(builtin_op_obj_test_eq, SPECIAL);
	}

	if (static_memory) {
		addrstore_put(static_memory, ADDRSTORE_KIND_SPECIAL, ".static segment");
	}

	context_t mcontext;
	mcontext.temp_reg_base = REGISTER_GP;
	mcontext.continue_labels = NULL;
	mcontext.break_labels = NULL;
	mcontext.stack_depth = 1;
	mcontext.variable_storage = 0;
	context_t *context = &mcontext;

	buffer_t mbuf = buffer_new(1024);
	buffer_t *buf = &mbuf;
	PUSH(REGISTER_GP);
	emit_la(buf, REGISTER_GP, static_memory);
	baseline_compile_expr(buf, root, REGISTER_V0, context);
	/* emit_move(&buf, registers_argument_nr[0], REGISTER_V0); */
	POP(REGISTER_GP);
	emit_jreturn(buf);
	buffer_terminate(mbuf);
	return mbuf;
}
