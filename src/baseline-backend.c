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

#define _GNU_SOURCE

#include <string.h>
#include <assert.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>

#include "address-store.h"
#include "assembler.h"
#include "ast.h"
#include "baseline-backend.h"
#include "bitvector.h"
#include "class.h"
#include "compiler-options.h"
#include "dynamic-compiler.h"
#include "errors.h"
#include "object.h"
#include "registers.h"
#include "stackmap.h"


#define VAR_IS_OBJ

#define FAIL(...) { fprintf(stderr, "[baseline-backend] L%d: Compilation failed: ", __LINE__); fprintf(stderr, __VA_ARGS__); exit(1); }

const int WORD_SIZE = sizeof(void *);

typedef struct relative_jump_label_list {
	label_t label;
	struct relative_jump_label_list *next;
} relative_jump_label_list_t;

//d Uebersetzungskontext
//e translation context
typedef struct {
	symtab_entry_t *symtab_entry;
	bitvector_t stackmap;
	int self_stack_location; /*e memory offset of the `SELF' reference, relative to $fp */ /*d Speicherstelle der `SELF'-Referenz relativ zu $fp */

	//e the following values are normally NEGATIVE:
	int stack_offset_args; /*e $fp offset for $a0 ($a1 etc. right below; arg 6 are spilled separately in accordance to calling conventions) */
	int stack_offset_locals; /*e $fp offset for local variable at offset 0 (i.e., `offset = 0' in symbol table) */
	int stack_offset_temps; /*e $fp offset for temp offset 0 (i.e., `storage = 0' in ast node); always the LOWEST part of the stack */
	int stack_offset_base; /*e total number of bytes allocated for the stack */
	
	/*e if we're in a loop: jump labels */ /*d Falls verfuegbar/in Schleife: Sprungmarken */
	relative_jump_label_list_t *continue_labels, *break_labels;
} context_t;

#define STACK_ALLOCATE(DSIZE) if (DSIZE) {emit_subi(buf, REGISTER_SP, WORD_SIZE * (DSIZE)); }
#define STACK_DEALLOCATE(DSIZE) if (DSIZE) {emit_addi(buf, REGISTER_SP, WORD_SIZE * (DSIZE)); }

//e for debugging: embeds line number in generated code
#define MARK_LINE()  {emit_addi(buf, REGISTER_FP, __LINE__);emit_subi(buf, REGISTER_FP, __LINE__);}

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
save_stackmap(buffer_t *buf, context_t *context)
{
	/* fprintf(stderr, "---> ISSUE stackmap ["); */
	/* bitvector_print(stderr, context->stackmap); */
	/* fprintf(stderr, "] at %p\n", buffer_target(buf)); */
	stackmap_put(buffer_target(buf), bitvector_clone(context->stackmap), context->symtab_entry);
}

static void
baseline_compile_expr(buffer_t *buf, ast_node_t *ast, int dest_register, context_t *context);

long long int builtin_op_obj_test_eq(object_t *a0, object_t *a1);


#define PREPARE_ARGUMENTS_MUSTALIGN	0x0001	// Aufrufstapel muss ausgerichtet werden
#define PREPARE_ARGUMENTS_SKIP_A0	0x0002	// Parameter beginnen bei $a1; $a0 wird nach baseline_prepare_arguments() und vor emit_jal(r)() separat gesetzt

/*d
 * Laed eine Argumentliste in $a0...$a5 und bereitet (soweit noetig) den Stapel auf einen Aufruf vor
 *
 * @param buf
 * @param children_nr Anzahl der Parameter
 * @param children Parameter
 * @param context
 * @param flags: PREPARE_ARGUMENTS_*
 * @return Anzahl der allozierten Stapelelemente
 */
/*e
 * Loads argument list into $a0...$a5 and prepares call stack (if needed)
 *
 * @param buf
 * @param children_nr number of parameters
 * @param children parameters
 * @param context
 * @param flags: PREPARE_ARGUMENTS_*
 * @return number of stack entries allocated (0 if fewer than 7 args)
 */
static int
baseline_prepare_arguments(buffer_t *buf, int children_nr, ast_node_t **children, context_t *context, int flags);

void
fail_at_node(ast_node_t *node, char *msg)
{
	fprintf(stderr, "Fatal: %s", msg);
	if (node->source_line) {
		fprintf(stderr, " in line %d", node->source_line);
	}
#ifdef __GNUC__
	fprintf(stderr, " before %p", __builtin_return_address(0));
#endif
	fprintf(stderr, "\n");

	ast_node_dump(stderr, node, AST_NODE_DUMP_FORMATTED | AST_NODE_DUMP_ADDRESS | AST_NODE_DUMP_FLAGS);
	fprintf(stderr, "\n");
	fail("execution halted on error");
}

static void
dump_ast(char *msg, ast_node_t *ast)
{
	fprintf(stderr, "[baseline-backend] %s\n", msg);
	ast_node_dump(stderr, ast, AST_NODE_DUMP_FORMATTED | AST_NODE_DUMP_FLAGS | AST_NODE_DUMP_ADDRESS | AST_NODE_DUMP_STORAGE);
	fprintf(stderr, "\n");
}

#define IS_SELF_REF(node) ((NODE_TY(node) == AST_VALUE_ID) && ((node)->sym->id == BUILTIN_OP_SELF))

//d Kann dieser Knoten ohne temporaere Register berechnet werden?
//e Can we compute this node without using temporary registers?
static bool
is_simple(ast_node_t *n)
{
	//e Strings are presently allocated every time we encounter them
	return (IS_VALUE_NODE(n) && NODE_TY(n) != AST_VALUE_STRING)
		|| NODE_TY(n) == AST_NODE_NULL
		|| IS_SELF_REF(n);
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
	emit_jalr(buf, REGISTER_V0);
}


//e compute offset to $fp for temp storage node
static int
baseline_temp_get_fp_offset(ast_node_t *node, context_t *context)
{
	if (node->storage < 0) {
		fprintf(stderr, "FATAL CODEGEN ERROR: Trying to request temp for code fragment without temp storage\n");
		AST_DUMP(node);
		*((void **)0) = NULL;
		assert(node->storage >= 0);
	}
	int off = node->storage * WORD_SIZE;
	off += context->stack_offset_temps;
	if ((off >= context->stack_offset_locals)
		|| off < context->stack_offset_base) {
		fprintf(stderr, "FATAL CODEGEN ERROR: invalid temp offset on stack: locals at %d, off at %d, base at %d\n", context->stack_offset_locals, off, context->stack_offset_base);
		AST_DUMP(node);
		*((void **)0) = NULL;
		assert(off < context->stack_offset_locals);
		assert(off >= context->stack_offset_base);
	}
	return off;
}

static void
stackmap_mark(context_t *context, int fp_offset, bool is_reference)
{
	/* fprintf(stderr, "STACKMAP-MARK(%d)[%d(%d) with base %d(%d), len %zd] => ", */
	/* 	is_reference, */
	/* 	fp_offset / 8, */
	/* 	fp_offset, */
	/* 	context->stack_offset_base / 8, */
	/* 	context->stack_offset_base, */
	/* 	bitvector_size(context->stackmap)); */
	fp_offset -= context->stack_offset_base;
	fp_offset /= 8; /*e dear compiler: you better turn this into an arithmetic shift! (NB: it seems to do just that) */
	/* fprintf(stderr, "%d\n", fp_offset); */
	assert(fp_offset >= 0);
	assert(fp_offset < bitvector_size(context->stackmap));
	if (is_reference) {
		context->stackmap = BITVECTOR_SET(context->stackmap, fp_offset);
	} else {
		context->stackmap = BITVECTOR_CLEAR(context->stackmap, fp_offset);
	}
}

static void
baseline_store_temp(buffer_t *buf, int reg, ast_node_t *node, context_t *context)
{
	int offset = baseline_temp_get_fp_offset(node, context);
	emit_sd(buf, reg, offset, REGISTER_FP);
	stackmap_mark(context, offset, (node->type & TYPE_FLAGS) == TYPE_OBJ);
}

//e tells the stack map that this slot is no longer in use
static void
baseline_free_temp(ast_node_t *node, context_t *context)
{
	if (node->storage >= 0) {
		int offset = baseline_temp_get_fp_offset(node, context);
		stackmap_mark(context, offset, false);
	}
}

static void
baseline_load_temp(buffer_t *buf, int reg, ast_node_t *node, context_t *context)
{
	emit_ld(buf, reg, baseline_temp_get_fp_offset(node, context), REGISTER_FP);
}

//e may utilise register T0 when encountering fields
static void
baseline_id_get_location(buffer_t *buf, symtab_entry_t *sym, int *reg, int *offset, context_t *context)
{
	if (sym->id == BUILTIN_OP_SELF) {
		*offset = context->self_stack_location;
		*reg = REGISTER_FP;
		return;
	}

	int off = sym->offset * WORD_SIZE;

	//d Basisregister bestimmen
	//e determine base register
	if (SYMTAB_IS_STACK_DYNAMIC(sym)) {
		*reg = REGISTER_FP;
		if (sym->symtab_flags & SYMTAB_PARAM) {
			off += context->stack_offset_args;
		} else {
			off += context->stack_offset_locals;
		}
/*		
		fprintf(stderr, "[args=%d] [var=%d] [temp=%d] [base=%d] while off = %d\n",
			context->stack_offset_args,
			context->stack_offset_locals,
			context->stack_offset_temps,
			context->stack_offset_base,
			off);
*/
		assert(off < 0
		       || ((off >= (WORD_SIZE * 2)) && (sym->symtab_flags & SYMTAB_EXCESS_PARAM)));

		assert(off >= context->stack_offset_base);

	} else if (SYMTAB_IS_STATIC(sym)) {
		*reg = REGISTER_GP;
	} else if (sym->parent) {
		//d Lokales Feld
		//e local field
		emit_ld(buf, REGISTER_T0, context->self_stack_location, REGISTER_FP);
		*reg = REGISTER_T0;
		off += WORD_SIZE;
	} else {
		fprintf(stderr, "Don't know how to load this variable:");
		symtab_entry_dump(stderr, sym);
		FAIL("Unable to load variable");
	}
	*offset = off;
}

static void
baseline_store_type(buffer_t *buf, int reg, symtab_entry_t *sym, context_t *context, bool is_obj)
{
	int offset, base_reg;
	baseline_id_get_location(buf, sym, &base_reg, &offset, context);
	if (base_reg == REGISTER_FP) {
		stackmap_mark(context, offset, is_obj);
	}
	emit_sd(buf, reg, offset, base_reg);
}

static void
baseline_store(buffer_t *buf, int reg, symtab_entry_t *sym, context_t *context)
{
	baseline_store_type(buf, reg, sym, context, (sym->ast_flags & TYPE_FLAGS) == TYPE_OBJ);
}

static void
baseline_load(buffer_t *buf, int reg, symtab_entry_t *sym, context_t *context)
{
	int offset, base_reg;
	baseline_id_get_location(buf, sym, &base_reg, &offset, context);
	emit_ld(buf, reg, offset, base_reg);
}


static void
baseline_compile_builtin_convert(buffer_t *buf, ast_node_t *arg, int to_ty, int from_ty, int dest_register, context_t *context)
{
	//d Annahme: zu konvertierendes Objekt ist in a0
	//e Assumption: object to convert is in $a0

#ifdef VAR_IS_OBJ
	if (to_ty == TYPE_VAR) {
		to_ty = TYPE_OBJ;
	}
	if (from_ty == TYPE_VAR) {
		from_ty = TYPE_OBJ;
	}
#endif

	int arguments_flags = 0;
	if (to_ty != from_ty
	    && to_ty == TYPE_OBJ) {
		//e always use function call when boxing
		//d Verpacken (boxing) immer mit Funktionsaufruf
		arguments_flags = PREPARE_ARGUMENTS_MUSTALIGN;
	}
	STACK_DEALLOCATE(baseline_prepare_arguments(buf, 1, &arg, context, arguments_flags));

	switch (from_ty) {
	case TYPE_INT:
		switch (to_ty) {
		case TYPE_INT:
			return;
		case TYPE_OBJ:
			emit_la(buf, REGISTER_V0, &new_int);
			emit_jalr(buf, REGISTER_V0);
			save_stackmap(buf, context);
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
			emit_beqz(buf, REGISTER_A0, &null_label); // NULL?
			emit_ld(buf, REGISTER_T0, 0, REGISTER_A0);
			emit_la(buf, REGISTER_V0, &class_boxed_int);
			label_t jump_label;
			//d Falls Integer-Objekt: Springe zur Dekodierung
			//e if integer object: jump to decoding
			buffer_setlabel2(&null_label, buf);
			emit_beq(buf, REGISTER_T0, REGISTER_V0, &jump_label);
			emit_fail_at_node(buf, arg, "attempted to convert non-int object to int value");
			//e successfully decoded:
			//d Erfolgreiche Dekodierung:
			buffer_setlabel2(&jump_label, buf);
			emit_ld(buf, dest_register,
				//d Int-Wert im Objekt:
				//e int value in object:
				offsetof(object_t, fields[0].int_v),
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
			    ast_node_t **args,
			    context_t *context)
{
	int a0_ty = args[0]->type & TYPE_FLAGS;
	int a1_ty = args[1]->type & TYPE_FLAGS;

#ifdef VAR_IS_OBJ
	if (a0_ty == TYPE_VAR) {
		a0_ty = TYPE_OBJ;
	}
	if (a1_ty == TYPE_VAR) {
		a1_ty = TYPE_OBJ;
	}
#endif

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
			//d Temporaeres Objekt auf dem Stapel erzeugen
			//e allocate temporary object on stack
			emit_push(buf, conv_reg);
			emit_la(buf, conv_reg, &class_boxed_int);
			emit_push(buf, conv_reg);
			emit_move(buf, conv_reg, REGISTER_SP);
			//e two pushes: stack remains suitably aligned
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
		emit_jalr(buf, REGISTER_V0);
		save_stackmap(buf, context);
		emit_optmove(buf, dest_register, REGISTER_V0);
		break;

	case TYPE_VAR:
	default:
		 FAIL("Unsupported type equality check: %x\n", a0_ty);
	}

	if (temp_object) {
		//d Temporaeres Objekt vom Stapel deallozieren
		//e deallocate temporary object from stack
		STACK_DEALLOCATE(2);
	}
}


/**e
 * Compiles the execution of a built-in operator (op)
 *
 * @param buf The buffer to write to
 * @param ty_and_node_flags Flags for the AST node (used to extract the result type)
 * @param op The operator to translate
 * @param args Pointer to the AST node arguments to the operation
 * @param dest_register The register we should store the result in
 * @param context The translation context
 */
static void
baseline_compile_builtin_op(buffer_t *buf, int ty_and_node_flags, int op, ast_node_t **args, int dest_register, context_t *context)
{
	const int result_ty = ty_and_node_flags & TYPE_FLAGS;

	//d Bestimme Anzahl der Parameter
	//e Compute the number of arguments
	int args_nr;

	switch (op) {
	case BUILTIN_OP_NOT:
		args_nr = 1;
		break;

	case BUILTIN_OP_ADD:
	case BUILTIN_OP_MUL:
	case BUILTIN_OP_SUB:
	case BUILTIN_OP_TEST_EQ:
	case BUILTIN_OP_TEST_LE:
	case BUILTIN_OP_TEST_LT:
		args_nr = 2;
		break;

	//e CONVERT, ALLOCATE: don't precompute arguments, since we might need a call and can't predict here whether the stack needs to be aligned
	//d CONVERT, ALLOCATE: hier nicht vorbereitet, da wir nicht so leicht testen koennen, ob der Stapel ausgerichtet werden muss
	case BUILTIN_OP_CONVERT:
	case BUILTIN_OP_ALLOCATE:
	case BUILTIN_OP_DIV:
	default:
		args_nr = 0;
		break;
	}

	/*e load parameters into $a0, $a1, ... */ /*d Parameter laden */
	if (args_nr) {
		assert(0 == baseline_prepare_arguments(buf, args_nr, args, context, 0));
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
					    args,
					    context);
		break;

	case BUILTIN_OP_TEST_LE:
		emit_sle(buf, dest_register, REGISTER_A0, REGISTER_A1);
		break;

	case BUILTIN_OP_TEST_LT:
		emit_slt(buf, dest_register, REGISTER_A0, REGISTER_A1);
		break;

	case BUILTIN_OP_DIV:
		//e Parameters haven't been computed/loaded yet
		//e Compute lhs parameter
		baseline_compile_expr(buf, args[0], REGISTER_V0, context);
		if (!is_simple(args[1])) {
			baseline_store_temp(buf, REGISTER_V0, args[0], context);
		}
		baseline_compile_expr(buf, args[1], REGISTER_T0, context);
		if (!is_simple(args[1])) {
			baseline_load_temp(buf, REGISTER_V0, args[0], context);
		}
		emit_li(buf, REGISTER_A2, 0); /*e must be nulled prior to division */ /*d muss vor Division auf 0 gesetzt werden */
		emit_div_a2v0(buf, registers_temp[0]);
		emit_optmove(buf, dest_register, REGISTER_V0); /*e result in $v0 */ /*d Ergebnis in $v0 */
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

	case BUILTIN_OP_ALLOCATE: {
		assert(0 == baseline_prepare_arguments(buf, 0, args, context, PREPARE_ARGUMENTS_MUSTALIGN));
		symtab_entry_t *sym = symtab_lookup(AV_INT(args[0]));
		emit_la(buf, REGISTER_V0, new_object);
		emit_la(buf, REGISTER_A0, sym->r_mem);
		emit_li(buf, REGISTER_A1, sym->storage.fields_nr);
		emit_jalr(buf, REGISTER_V0);
		save_stackmap(buf, context);
	}
		break;

	default:
		FAIL("Unsupported builtin op: %d\n", op);
	}
}


// Gibt Groesse des Stapelrahmens (in Bytes) zurueck
// Stellt sicher, dass der Stapelspeicher angemessen ausgerichtet ist
static int
baseline_prepare_arguments(buffer_t *buf, int children_nr, ast_node_t **children, context_t *context, int flags)
{
	/*d Aufrufkonventionen gem. System V ABI fuer x86-64:
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
	/*e calling conventions according to System V ABI for x86-64:
	 * $a0-$a5 have parameters 0-5 (ignoring floating point etc.)
	 *
	 * |    X    |
         * +---------+
	 * | param 7 |
         * +---------+
	 * | param 6 |
         * +---------+  <- $sp
	 *
	 * In addition, $sp must be 16 byte-aligned, so we add one entry between X and the final param if the
	 * number of excess arguments is odd.
	 */
	const int first_arg = (flags & PREPARE_ARGUMENTS_SKIP_A0)? 1 : 0;
	int stack_args_nr = children_nr < REGISTERS_ARGUMENT_NR ? 0 : children_nr - REGISTERS_ARGUMENT_NR;

	int last_nonsimple = -1; // letzte nichttriviale Berechnung
	for (int i = 0; i < children_nr; i++) {
		if (!is_simple(children[i])) {
			last_nonsimple = i;
#if 0			
			if (i < REGISTERS_ARGUMENT_NR) {
				++backup_space_needed;
			}
#endif			
		}
	}
	if (stack_args_nr & 1) { /*d Ausrichtung noetig? */ /*e alignment needed? */
		++stack_args_nr;  /*d Ausrichtung */ /*e align */
	}

	STACK_ALLOCATE(stack_args_nr);

	//e recursively compute nontrivial values
	//d Nichttriviale Werte rekursiv ausrechnen
	for (int i = 0; i < children_nr; i++) {
		int arg_nr = i + first_arg;
		int reg = REGISTER_V0;
		if (arg_nr < REGISTERS_ARGUMENT_NR){
			reg = registers_argument[arg_nr];
		}
		
		if (!is_simple(children[i])) {
			baseline_compile_expr(buf, children[i], reg, context);

			if (last_nonsimple > i || arg_nr >= REGISTERS_ARGUMENT_NR) {
				/*e we don't have to store the last non-simple argument, since there are no other
				 * operations that can interfere with the register contents */
				int dest_stack_location;
				int base_reg;
				if (arg_nr >= REGISTERS_ARGUMENT_NR) {
					if (last_nonsimple > i) {
						//e GC is still possible:
						//e We still need to store it so the GC can find it
						baseline_store_temp(buf, reg, children[i], context);
					}
					dest_stack_location = WORD_SIZE * (arg_nr - REGISTERS_ARGUMENT_NR);
					base_reg = REGISTER_SP;
				} else {
					dest_stack_location = baseline_temp_get_fp_offset(children[i], context);
					base_reg = REGISTER_FP;
				}

				if (i != last_nonsimple || arg_nr >= REGISTERS_ARGUMENT_NR) {
					emit_sd(buf, reg,
						dest_stack_location,
						base_reg);
				}
			}
		}
	}
	//e from here on GC is impossible until the actual function call

	//e compute trivial values that have to go on the stack (but can't trigger GC)
	//d Berechne triviale Werte, die unbedingt auf den Stapel müssen (aber kein GC auslösen können)
	for (int i = REGISTERS_ARGUMENT_NR; i < children_nr; i++) {
		int reg = REGISTER_V0;
		
		if (is_simple(children[i])) {
			baseline_compile_expr(buf, children[i], reg, context);

			int dest_stack_location;
			int base_reg;
			dest_stack_location = WORD_SIZE * (i + first_arg - REGISTERS_ARGUMENT_NR);
			base_reg = REGISTER_SP;
			emit_sd(buf, reg,
				dest_stack_location,
				base_reg);
		}
	}
	
	//d Triviale Registerinhalte generieren, vorher berechnete Werte bei Bedarf laden
	//e trivial register contents, or load previously computed values
	const int max = children_nr < REGISTERS_ARGUMENT_NR ? children_nr : REGISTERS_ARGUMENT_NR;
	for (int i = 0; i < max; i++) {
		int arg_nr = i + first_arg;
		if (is_simple(children[i])) {
			if (arg_nr < REGISTERS_ARGUMENT_NR) {
				baseline_compile_expr(buf, children[i], registers_argument[arg_nr], context);
			}
		} else if (i != last_nonsimple) { /*d Wert der letzten nichttrivialen Berechnung ist noch frisch */ /*e value of last nontrivial computation is still fresh */
			emit_ld(buf, registers_argument[arg_nr],
				//WORD_SIZE * (register_arg_spill_space + spill_counter++),
				baseline_temp_get_fp_offset(children[i], context),
				REGISTER_FP);
		}
		//e deallocate in stack map
		baseline_free_temp(children[i], context);
	}
	return stack_args_nr;
}

// Der Aufrufer speichert; der Aufgerufene haelt sich immer an dest_register
static void
baseline_compile_expr(buffer_t *buf, ast_node_t *ast, int dest_register, context_t *context)
{
	/* ast_node_dump(stderr, ast, 6 | 8); */

	switch (NODE_TY(ast)) {
	case AST_VALUE_INT:
		emit_li(buf, dest_register, AV_INT(ast));
		break;

	case AST_VALUE_STRING: {
		/* object_t *addr = new_string(AV_STRING(ast), strlen(AV_STRING(ast))); */
		/* emit_la(buf, dest_register, (void *) addr); */
		/* addrstore_put(addr, ADDRSTORE_KIND_STRING_LITERAL, AV_STRING(ast)); */
		char *string = AV_STRING(ast);
		addrstore_put(string, ADDRSTORE_KIND_STRING_LITERAL, string);
		emit_la(buf, REGISTER_A0, string);
		emit_li(buf, REGISTER_A1, strlen(string));
		emit_la(buf, REGISTER_V0, &new_string);
		emit_jalr(buf, REGISTER_V0);
		save_stackmap(buf, context);
		emit_optmove(buf, dest_register, REGISTER_V0);
	}
		break;

	case AST_VALUE_ID: {
		//e determine register and offset
		//d Basis-Register und Abstand bestimmen
		int reg, offset;

		baseline_id_get_location(buf, ast->sym, &reg, &offset, context);
		
		//d Adresse oder Wert?
		//e address or value?
		if (ast->type & AST_FLAG_LVALUE) {
			//d Wir wollen nur die Adresse:
			//e we only want the address:
			emit_li(buf, dest_register, offset);
			emit_add(buf, dest_register, reg);
			//e This of course means that a store is imminent, so we update the stack map.
			//e Note that this is the right time to do so, as the assignment's rhs is generated first
			//e and the lhs cannot trigger memory allocation.
			if (reg == REGISTER_FP) {
				stackmap_mark(context, offset, (ast->type & TYPE_FLAGS) == TYPE_OBJ);
			}
		} else {
			emit_ld(buf, dest_register, offset, reg);
		}
	}
		break;

	case AST_NODE_VARDECL:
		if (ast->children[1] == NULL) {
			// Keine Initialisierung?
			break;
		}
		// fall through
	case AST_NODE_ASSIGN:
		if (NODE_TY(ast->children[0]) == AST_NODE_MEMBER) {
			//e field
			ast_node_t *member = ast->children[0];
			ast_node_t *selector_node = member->children[1];
			const int selector = selector_node->sym->selector;

			if (!is_simple(ast->children[1])) {
				baseline_compile_expr(buf, ast->children[1], REGISTER_A3, context);

				if (!is_simple(member->children[0])) {
					baseline_store_temp(buf, REGISTER_A3, ast->children[0], context);
				}
			}

			baseline_compile_expr(buf, member->children[0], REGISTER_A0, context);

			if (is_simple(ast->children[1])) {
				baseline_compile_expr(buf, ast->children[1], REGISTER_A3, context);
			} else if (!is_simple(member->children[0])) {
				baseline_load_temp(buf, REGISTER_A3, ast->children[0], context);
			}
			
			emit_la(buf, REGISTER_A1, selector_node);
			emit_li(buf, REGISTER_A2, selector);

			const int ty = ast->children[1]->type;
			if (ty & TYPE_INT) {
				emit_la(buf, REGISTER_V0, object_write_member_field_int);
			} else { //if (ty & TYPE_OBJ) {
				emit_la(buf, REGISTER_V0, object_write_member_field_obj);
			}
			/* } else */
			/* } else { */
			/* 	AST_DUMP(ast); */
			/* 	fprintf(stderr, "Offending type: %x\n", ty); */
			/* 	fail("baseline-backend.AST_NODE_ASSIGN(AST_NODE_MEMBER): unsupported type in AST_NODE_MEMBER value"); */
			/* } */

			assert(0 == baseline_prepare_arguments(buf, 0, NULL, context,
							       PREPARE_ARGUMENTS_MUSTALIGN));
			emit_jalr(buf, REGISTER_V0);
			save_stackmap(buf, context);
		} else {
			//e local or global variable
			baseline_compile_expr(buf, ast->children[1], REGISTER_V0, context);
			if (NODE_TY(ast->children[0]) == AST_VALUE_ID) {
				baseline_store_type(buf, REGISTER_V0, AST_CALLABLE_SYMREF(ast), context,
						    AST_TYPE(ast->children[1]) == TYPE_OBJ);
			} else {
				if (!is_simple(ast->children[0])) {
					baseline_store_temp(buf, REGISTER_V0, ast->children[1], context);
				}
				baseline_compile_expr(buf, ast->children[0], REGISTER_T0, context);
				if (!is_simple(ast->children[0])) {
					baseline_load_temp(buf, REGISTER_V0, ast->children[1], context);
				}
				emit_sd(buf, REGISTER_V0, 0, REGISTER_T0);
			}
		}
		break;

	case AST_NODE_ARRAYVAL: {
		if (ast->children[1]) {
			//d Laden mit expliziter Groessenangabe
			//e load with explicit size specification
			baseline_compile_expr(buf, ast->children[1], REGISTER_A0, context);
			if (!compiler_options.no_bounds_checks) {
				//d Arraygrenzenpruefung
				//e array out-of-bounds check
				emit_li(buf, REGISTER_T0, ast->children[0]->children_nr);
				label_t jl;
				emit_ble(buf, REGISTER_T0, REGISTER_A0, &jl);
				emit_fail_at_node(buf, ast, "Requested array size is smaller than number of array elements");
				buffer_setlabel2(&jl, buf);
			}
		} else {
			//d Laden mit impliziter Groesse
			//e load with implicit size
			emit_li(buf, REGISTER_A0, ast->children[0]->children_nr);
		}
		emit_la(buf, REGISTER_V0, &new_array);
		emit_jalr(buf, REGISTER_V0);
		save_stackmap(buf, context);
		//e We now have the allocated array in REGISTER_V0
		baseline_store_temp(buf, REGISTER_V0, ast, context);
		for (int i = 0; i < ast->children[0]->children_nr; i++) {
			ast_node_t *child = ast->children[0]->children[i];
			baseline_compile_expr(buf, child, REGISTER_T0, context);
			if (!is_simple(child)) {
				baseline_load_temp(buf, REGISTER_V0, ast, context);
			}
			emit_sd(buf, REGISTER_T0,
				(2 * WORD_SIZE) /*e header + size */ /*d header + groesse */ + WORD_SIZE * i,
				REGISTER_V0);
		}
		emit_optmove(buf, dest_register, REGISTER_V0);
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

		if (!is_simple(ast->children[1])) {
			baseline_store_temp(buf, REGISTER_V0, ast->children[0], context);
		}
		baseline_compile_expr(buf, ast->children[1], REGISTER_T0, context);
		if (!is_simple(ast->children[1])) {
			baseline_load_temp(buf, REGISTER_V0, ast->children[0], context);
		}

		if (!compiler_options.no_bounds_checks) {
			//d Arraygrenzenpruefung
			//e array bounds checking
			emit_ld(buf, REGISTER_T1, WORD_SIZE, REGISTER_V0);
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
			emit_addi(buf, REGISTER_V0, WORD_SIZE * 2); /*d zwei Worte, um ueber Typ-ID und Arraygroesse zu springen */ /*e two words to jump over type ID and array size */
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

	case AST_NODE_RETURN:
		if (ast->children[0]) {
			baseline_compile_expr(buf, ast->children[0], REGISTER_V0, context);
		}
		emit_move(buf, REGISTER_SP, REGISTER_FP);
		emit_pop(buf, REGISTER_FP);
		emit_jreturn(buf);
		break;

	case AST_NODE_METHODAPP: {
		baseline_compile_expr(buf, ast->children[0], REGISTER_A0, context);
		if (!(IS_SELF_REF(ast->children[0]))) {
			//e don't need to backup self ref (it's already in a secure stack slot)
			baseline_store_temp(buf, REGISTER_A0, ast->children[0], context);
		}

		//d Berechne Sprungadresse
		//e compute jump address
		ast_node_t *selector_node = ast->children[1];
		const int selector = selector_node->sym->selector;
		emit_la(buf, REGISTER_A1, selector_node);
		emit_li(buf, REGISTER_A2, selector);
		emit_li(buf, REGISTER_A3, ast->children[2]->children_nr);
		emit_la(buf, REGISTER_V0, object_get_member_method);

		assert(0 == baseline_prepare_arguments(buf, 0, NULL, context,
						       PREPARE_ARGUMENTS_MUSTALIGN));
		emit_jalr(buf, REGISTER_V0);
		save_stackmap(buf, context);
		
		//d Speichere Sprungadresse
		//e save jump address
		baseline_store_temp(buf, REGISTER_V0, ast, context);
		baseline_free_temp(ast, context);
		int stack_frame_size =
			baseline_prepare_arguments(buf,
						   ast->children[2]->children_nr,
						   ast->children[2]->children,
						   context,
						   PREPARE_ARGUMENTS_MUSTALIGN
						   | PREPARE_ARGUMENTS_SKIP_A0);

		if (IS_SELF_REF(ast->children[0])) {
			emit_ld(buf, REGISTER_A0, context->self_stack_location, REGISTER_FP);
		} else {
			baseline_load_temp(buf, REGISTER_A0, ast->children[0], context);
		}
		baseline_load_temp(buf, REGISTER_V0, ast, context);
		emit_jalr(buf, REGISTER_V0);
		save_stackmap(buf, context);

		STACK_DEALLOCATE(stack_frame_size);

		emit_optmove(buf, dest_register, REGISTER_V0);
	}
		break;

	case AST_NODE_MEMBER: {
		assert(!(ast->type & AST_FLAG_LVALUE)); // Sollte direkt von ASSIGN gehandhabt werden
		baseline_compile_expr(buf, ast->children[0], REGISTER_A0, context);
		ast_node_t *selector_node = ast->children[1];
		const int selector = selector_node->sym->selector;

		emit_la(buf, REGISTER_A1, selector_node);
		emit_li(buf, REGISTER_A2, selector);

		if (ast->type & TYPE_INT) {
			emit_la(buf, REGISTER_V0, object_read_member_field_int);
		} else { //if (ast->type & TYPE_OBJ) {
			emit_la(buf, REGISTER_V0, object_read_member_field_obj);
		}
		/* } else { */
		/* 	AST_DUMP(ast); */
		/* 	fprintf(stderr, "Offending type: %x\n", ast->type); */
		/* 	fail("baseline-backend.AST_NODE_MEMBER: unsupported type in AST_NODE_MEMBER value"); */
		/* } */
		assert(0 == baseline_prepare_arguments(buf, 0, NULL, context,
						       PREPARE_ARGUMENTS_MUSTALIGN));
		emit_jalr(buf, REGISTER_V0);
		save_stackmap(buf, context);
		emit_optmove(buf, dest_register, REGISTER_V0);
	}
		break;


	case AST_NODE_NEWINSTANCE:
	case AST_NODE_FUNAPP: {
		// Annahme: Funktionsaufrufe (noch keine Unterstuetzung fuer Selektoren)
		symtab_entry_t *sym = AST_CALLABLE_SYMREF(ast);
		if (NODE_TY(ast) == AST_NODE_NEWINSTANCE) {
			//d Konstruktor-Symbol
			//e constructor symbol
			sym = AST_CALLABLE_SYMREF(sym->astref->children[3]);
		}
		assert(sym);
		// Besondere eingebaute Operationen werden in einer separaten Funktion behandelt
		if (sym->id < 0 && sym->symtab_flags & SYMTAB_HIDDEN) {
			baseline_compile_builtin_op(buf, ast->type & ~AST_NODE_MASK, sym->id,
						    ast->children[1]->children, dest_register, context);
		} else {
			//d Normaler Funktionsaufruf
			//d Argumente laden
			//e load arguments for regular function call
			int stack_frame_size =
				baseline_prepare_arguments(buf, ast->children[1]->children_nr, ast->children[1]->children, context,
							   PREPARE_ARGUMENTS_MUSTALIGN);

			if (!sym->r_mem) {
				symtab_entry_dump(stderr, sym);
				fail_at_node(ast, "No call target address for function");
			}
			long long distance = ((unsigned char *) sym->r_mem) - ((unsigned char *) buffer_target(buf));
			if (llabs(distance) < 0x7ffffff0) {
				label_t lab;
				emit_jal(buf, &lab);
				save_stackmap(buf, context);
				buffer_setlabel(&lab, sym->r_mem);
			} else {
				emit_la(buf, REGISTER_V0, sym->r_mem);
				emit_jalr(buf, REGISTER_V0);
				save_stackmap(buf, context);
			}

			// Stapelrahmen nachbereiten, soweit noetig
			STACK_DEALLOCATE(stack_frame_size);

			emit_optmove(buf, dest_register, REGISTER_V0);
		}
	}
		break;

	case AST_NODE_BLOCK:
		for (int i = 0; i < ast->children_nr; i++) {
			baseline_compile_expr(buf, ast->children[i], dest_register, context);
		}
		break;

	case AST_NODE_FUNDEF:
	case AST_NODE_CLASSDEF:
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
		assert(ast->children[1]->sym->r_mem);
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

static void
init_address_store()
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
		ADDRSTORE_PUT(new_object, SPECIAL);
		ADDRSTORE_PUT(dyncomp_compile_function, SPECIAL);
		ADDRSTORE_PUT(object_write_member_field_obj, SPECIAL);
		ADDRSTORE_PUT(object_write_member_field_int, SPECIAL);
		ADDRSTORE_PUT(object_read_member_field_obj, SPECIAL);
		ADDRSTORE_PUT(object_read_member_field_int, SPECIAL);
		ADDRSTORE_PUT(object_get_member_method, SPECIAL);
	}

}


#define	MCONTEXT_KIND_DEFAULT		0
#define	MCONTEXT_KIND_CONSTRUCTOR	1
#define	MCONTEXT_KIND_METHOD		2

int
setup_mcontext(context_t *context, symtab_entry_t *sym,
	       storage_record_t *storage, int parameters_nr, int kind, int additional_words)
{
	int words = 0;
	if ((kind == MCONTEXT_KIND_CONSTRUCTOR) || (kind == MCONTEXT_KIND_METHOD)) {
		words = 1;
		context->self_stack_location = -WORD_SIZE;
	} else {
		context->self_stack_location = 0;
	}
	int excess_parameters_bound = REGISTERS_ARGUMENT_NR;
	if (kind == MCONTEXT_KIND_METHOD) {
		--excess_parameters_bound; /*e method also must pass self */
	}

	const int excess_parameters = (parameters_nr > excess_parameters_bound) ? parameters_nr - excess_parameters_bound : 0;
	words += (parameters_nr > excess_parameters_bound) ? excess_parameters_bound : parameters_nr;
	context->stack_offset_args = -WORD_SIZE * (words);
	words += storage->vars_nr;
	context->stack_offset_locals = -WORD_SIZE * (words);
	words += storage->temps_nr;
	context->stack_offset_temps = -WORD_SIZE * (words);

	words += additional_words;
	if (words & 1) { /*e align stack if needed */ /*d Aufrufstapel ausrichten, sofern nötig */ 
		++words;
	}
	context->stack_offset_base = -WORD_SIZE * (words);

	int stackmap_extra_bits = 0;
	if (excess_parameters) {
		stackmap_extra_bits += 2 /* $fp, jreturn addr. */ + excess_parameters;
	}
	if (sym) {
		sym->stackframe_start = context->stack_offset_base;
	}

	context->stackmap = bitvector_alloc(words + stackmap_extra_bits);
	context->symtab_entry = sym;

	/* fprintf(stderr, "[mcontext: params=%d, vars=%d, temps=%d, extra=%d, cons|method=%d, excess-args=%d]\n", */
	/* 	parameters_nr, storage->vars_nr, storage->temps_nr, additional_words, kind, excess_parameters); */
	/* fprintf(stderr, "           params@%d, vars@%d, temps@%d,     self@%d  (total words = %d)\n", */
	/* 	context->stack_offset_args, */
	/* 	context->stack_offset_locals, */
	/* 	context->stack_offset_temps, */
	/* 	context->self_stack_location, */
	/* 	words); */

	return words;
}

void
free_mcontext(context_t *context)
{
	bitvector_free(context->stackmap);
}

buffer_t
baseline_compile_entrypoint(ast_node_t *root,
			    storage_record_t *storage,
			    void *static_memory)
{
	init_address_store();
	if (static_memory) {
		addrstore_put(static_memory, ADDRSTORE_KIND_SPECIAL, ".static segment");
	}

	context_t mcontext;
	mcontext.continue_labels = NULL;
	mcontext.break_labels = NULL;
	context_t *context = &mcontext;
	int stack_entries_nr = setup_mcontext(&mcontext, NULL, storage, 0, MCONTEXT_KIND_DEFAULT, 1);
	int gp_offset = -WORD_SIZE * stack_entries_nr;

	buffer_t mbuf = buffer_new(1024);
	buffer_t *buf = &mbuf;
	
	emit_push(buf, REGISTER_FP);
	emit_move(buf, REGISTER_FP, REGISTER_SP);
	STACK_ALLOCATE(stack_entries_nr);
	emit_sd(buf, REGISTER_GP, gp_offset, REGISTER_FP);

	//e set up pointer to static memory (might be NULL)
	emit_la(buf, REGISTER_GP, static_memory);
	
	baseline_compile_expr(buf, root, REGISTER_V0, context);
	
	emit_ld(buf, REGISTER_GP, gp_offset, REGISTER_FP);
	emit_move(buf, REGISTER_SP, REGISTER_FP);
	emit_pop(buf, REGISTER_FP);
	emit_jreturn(buf);
	buffer_terminate(mbuf);
	free_mcontext(&mcontext);
	return mbuf;
}

buffer_t
baseline_compile_static_callable(symtab_entry_t *sym)
{
	/*d Funktions-Aufrufrahmen
	 *
	 *    ....
	 * | param 8 |
         * +---------+
	 * | param 7 |
         * +---------+
	 * |   ret   |
         * +---------+
	 * | old-$fp |
         * +---------+  <- $fp
	 * |   $a5   |  // Mit Konstruktor ist hier noch die Konstruktor-Referenz eingeschoben
         * +---------+
	 * |   $a4   |
	 *    .....
	 */
	/*e Function stack frame
	 *
	 *    ....
	 * | param 8 |
         * +---------+
	 * | param 7 |
         * +---------+
	 * |   ret   |
         * +---------+
	 * | old-$fp |
         * +---------+  <- $fp
	 * |   $a5   |  // When calling a constructor, we reserve this as `future' SELF argument
         * +---------+
	 * |   $a4   |
	 *    .....
	 */

	init_address_store();
	ast_node_t *node = sym->astref;

	context_t mcontext;
	mcontext.continue_labels = NULL;
	mcontext.break_labels = NULL;
	int parameters_nr = sym->parameters_nr;
	bool is_constructor = sym->symtab_flags & SYMTAB_CONSTRUCTOR;
	if (is_constructor) {
		parameters_nr = sym->parent->parameters_nr;
	}
	int stack_entries_nr = setup_mcontext(&mcontext, sym, &sym->storage, parameters_nr,
					      is_constructor ? MCONTEXT_KIND_CONSTRUCTOR : MCONTEXT_KIND_DEFAULT, 0);
	context_t *context = &mcontext;

	buffer_t mbuf = buffer_new(1024);
	buffer_t *buf = &mbuf;
	emit_push(buf, REGISTER_FP);
	emit_move(buf, REGISTER_FP, REGISTER_SP);
	
	assert(NODE_TY(node) == AST_NODE_FUNDEF);
	STACK_ALLOCATE(stack_entries_nr);
	
	ast_node_t **args = node->children[1]->children;
	const int args_nr = node->children[1]->children_nr;
	ast_node_t *body = node->children[2];

	//d Parameter in Argumentregistern auf Stapel
	//e Move parameters in argument registers onto the stack
	const int spilled_args = args_nr > REGISTERS_ARGUMENT_NR ? REGISTERS_ARGUMENT_NR : args_nr;

	for (int i = 0; i < spilled_args; i++) {
		baseline_store(buf, registers_argument[i], args[i]->sym, context);
	}
	for (int i = spilled_args; i < args_nr; i++) {
		args[i]->sym->offset = i + 2 + (is_constructor? 1 : 0); // post $fp and return address
		args[i]->sym->symtab_flags |= SYMTAB_EXCESS_PARAM;
	}
	if (is_constructor) {
		//e Zero the `self' reference so the GC can tell if it hasn't been assigned yet
		//d `self'-Referenz auf NULL setzen, damit der GC sie nicht fälschlich sucht
		emit_li(buf, REGISTER_T0, 0);
		emit_sd(buf, REGISTER_T0, context->self_stack_location, REGISTER_FP);
		stackmap_mark(context, context->self_stack_location, true);
	}

	baseline_compile_expr(buf, body, REGISTER_V0, context);

	emit_move(buf, REGISTER_SP, REGISTER_FP);
	emit_pop(buf, REGISTER_FP);
	emit_jreturn(buf);
	buffer_terminate(mbuf);
	free_mcontext(&mcontext);
	return mbuf;
}

buffer_t
baseline_compile_method(symtab_entry_t *sym)
{
	ast_node_t *node = sym->astref;

	context_t mcontext;
	mcontext.continue_labels = NULL;
	mcontext.break_labels = NULL;
	int stack_entries_nr = setup_mcontext(&mcontext, sym, &sym->storage, sym->parameters_nr,
					      MCONTEXT_KIND_METHOD, 0);
	context_t *context = &mcontext;

	buffer_t mbuf = buffer_new(1024);
	buffer_t *buf = &mbuf;
	emit_push(buf, REGISTER_FP);
	emit_move(buf, REGISTER_FP, REGISTER_SP);

	STACK_ALLOCATE(stack_entries_nr);

	assert(NODE_TY(node) == AST_NODE_FUNDEF);
	ast_node_t **args = node->children[1]->children;
	const int full_args_nr = node->children[1]->children_nr + 1; /*d implizites SELF-Argument */ /*e implicit SELF reference */
	ast_node_t *body = node->children[2];

	//d Parameter in Argumentregistern auf Stapel
	//e Move parameters in argument registers onto the stack
	//e subtract one since we're handling the `self' parameter separately
	const int excess_args = full_args_nr > (REGISTERS_ARGUMENT_NR) ? (REGISTERS_ARGUMENT_NR) : full_args_nr;

	emit_sd(buf, REGISTER_A0, context->self_stack_location, REGISTER_FP);
	stackmap_mark(context, context->self_stack_location, true);
	
	for (int i = 1; i < excess_args; i++) {
		baseline_store(buf, registers_argument[i], args[i - 1]->sym, context);
	}
	for (int i = excess_args; i < full_args_nr; i++) {
		args[i - 1]->sym->offset = i + 2; // post $fp, return address
		args[i - 1]->sym->symtab_flags |= SYMTAB_EXCESS_PARAM;
	}

	baseline_compile_expr(buf, body, REGISTER_V0, context);

	emit_move(buf, REGISTER_SP, REGISTER_FP);
	emit_pop(buf, REGISTER_FP);
	emit_jreturn(buf);
	buffer_terminate(mbuf);
	free_mcontext(&mcontext);
	return mbuf;
}
