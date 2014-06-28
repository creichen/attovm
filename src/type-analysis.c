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
#include <stdarg.h>

#include "analysis.h"
#include "symbol-table.h"
#include "compiler-options.h"
#include "class.h"

int array_storage_type = TYPE_OBJ;
int method_call_param_type = TYPE_OBJ;
int method_call_return_type = TYPE_OBJ;

// Hilfsdefinitionen, um neue AST-Objekte zu erzeugen:
#define CONSV(TY, VALUE) value_node_alloc_generic(AST_VALUE_ ## TY, (ast_value_union_t) { .VALUE })
#define CONS(TY, ...) ast_node_alloc_generic(AST_NODE_ ## TY, ARGS_NR(__VA_ARGS__), __VA_ARGS__)
#define BUILTIN(ID) builtin(BUILTIN_OP_ ## ID)

// Trick zum Berechnen der Laenge von VA_ARGS, basierend auf Ideen von Laurent Deniau
#define ARGS_NR_X(_9, _8, _7, _6, _5, _4, _3, _2, _1, N, ...) N
#define ARGS_NR(...) ARGS_NR_X(__VA_ARGS__, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)

static int error_count = 0;

typedef struct {
	ast_node_t **callables;
	ast_node_t **classes;
	int functions_nr; int classes_nr;
} context_t;

static ast_node_t *
builtin(int id)
{
	ast_node_t *retval = CONSV(ID, ident = id);
	symtab_entry_t *sym = symtab_lookup(id);
	retval->sym = sym;
	retval->type |= sym->ast_flags & TYPE_FLAGS;
	return retval;
}

static void
error(const ast_node_t *node, char *fmt, ...)
{
	// Variable Anzahl von Parametern
	va_list args;
	fprintf(stderr, "Type error in line %d: ", node->source_line);
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
	fprintf(stderr, "\n");
	++error_count;
}


static int
update_type(int old, int ty)
{
	return (old & AST_NODE_MASK) | (ty & ~AST_NODE_MASK);
}

static void
set_type(ast_node_t *node, int ty)
{
	node->type = update_type(node->type, ty);
}

static void
mutate_node(ast_node_t *node, int ty)
{
	node->type = ty;
}

static ast_node_t *
require_lvalue(ast_node_t *node, int const_assignment_permitted)
{
	if (node == NULL) {
		return NULL;
	}

	switch (NODE_TY(node)) {
	case AST_VALUE_ID:
		if (!const_assignment_permitted && (node->type & AST_FLAG_CONST)) {
			error(node, "attempted assignment to constant `%s'", node->sym->name);
		}
	case AST_NODE_MEMBER:
	case AST_NODE_ARRAYSUB:
		break;

	default:
		error(node, "attempted assignment to non-lvalue");
	}

	node->type |= AST_FLAG_LVALUE;
	return node;
}

#define DEFAULT_TYPE TYPE_OBJ

static ast_node_t *
require_type(ast_node_t *node, int ty)
{
	static symtab_entry_t *convert_sym = NULL;
	if (!convert_sym) {
		convert_sym = symtab_lookup(BUILTIN_OP_CONVERT);
	}

	if (node == NULL) {
		return NULL;
	}

	const int current_ty = node->type & TYPE_FLAGS;

	if (!current_ty) {
		// MEMBER-Zugriff: Passenden Typ beantragen
		if (!ty) {
			ty = DEFAULT_TYPE;
		}
		node->type |= ty;
		return node;
	}

	if (ty == 0
	     || current_ty == ty) {
		return node;
	}

	ast_node_t *fun = BUILTIN(CONVERT);
	fun->sym = convert_sym;
	ast_node_t *conversion = CONS(FUNAPP, fun,
				      CONS(ACTUALS, node));
	set_type(fun, ty);
	set_type(conversion, ty);
	return conversion;
}

static ast_node_t *
analyse(ast_node_t *node, symtab_entry_t *classref, symtab_entry_t *function, context_t *context)
{
	if (!node) {
		return NULL;
	}

	if (!IS_VALUE_NODE(node)) {
		if (NODE_TY(node) == AST_NODE_CLASSDEF) {
			classref = node->children[0]->sym;
		} else if (NODE_TY(node) == AST_NODE_FUNDEF) {
			function = node->children[0]->sym;
		}

		for (int i = 0; i < node->children_nr; i++) {
			node->children[i] = analyse(node->children[i], classref, function, context);
		}
	}

	switch (NODE_TY(node)) {

	case AST_VALUE_INT:
		set_type(node, TYPE_INT);
		break;

	case AST_VALUE_STRING:
		set_type(node, TYPE_OBJ);
		break;

	case AST_VALUE_REAL:
		error(node, "Floating point numbers are not presently supported");
		break;

	case AST_VALUE_ID:
		if (node->sym) {
			// Typ aktualisieren
			symtab_entry_t *sym = node->sym;
			set_type(node, sym->ast_flags | (node->type & AST_FLAG_DECL));

			if (function && ((sym->symtab_flags & (SYMTAB_MEMBER | SYMTAB_PARAM)) == (SYMTAB_MEMBER | SYMTAB_PARAM))) {
				error(node, "Method bodies must not reference class constructor arguments.");
			}

			// Der folgende Code wird relevant, wenn Vererbung eingebaut wird
			/*
			if (sym->symtab_flags & SYMTAB_MEMBER &&
			    !(node->type & AST_FLAG_DECL)) {
				// Impliziten Feldzugriff explizit machen
				return CONS(MEMBER,
					    BUILTIN(SELF),
					    node);
			}
			*/
		}
		break;

	case AST_NODE_NULL:
		set_type(node, TYPE_OBJ);
		break;

	case AST_NODE_FUNAPP:
		if (NODE_TY(node->children[0]) == AST_VALUE_ID) {
			// Funktionsaufruf
			function = node->children[0]->sym;
			if (!function) {
				// Sollte nur passieren, wenn Namensanalyse fehlschlug
				error(node, "(Internal) No function annotation on ID %d\n", AV_ID(node->children[0]));
				return node;
			}

			if (SYMTAB_TY(function) == SYMTAB_TY_CLASS) {
				mutate_node(node, AST_NODE_NEWINSTANCE | TYPE_OBJ);
			} else if (!(SYMTAB_TY(function) == SYMTAB_TY_FUNCTION)) {
				error(node, "Attempt to call non-function/non-class `%s'", function->name);
				return node;
			}
			node->sym = function;

			int min_params = function->parameters_nr;
			ast_node_t *actuals = node->children[1];
			if (actuals->children_nr != min_params) {
				int actual_params_nr = node->children[1]->children_nr;
				if (actual_params_nr < min_params) {
					min_params = actual_params_nr;
				}
				error(node, "expected %d parameters, found %d", min_params, actual_params_nr);
			}

			for (int i = 0; i < function->parameters_nr; i++) {
				short expected_type = function->parameter_types[i];
				actuals->children[i] = require_type(actuals->children[i], expected_type);
			}

			set_type(node, function->ast_flags);

		} else if (NODE_TY(node->children[0]) == AST_NODE_MEMBER) {
			// Methoden-Aufruf
			ast_node_t *receiver = node->children[0]->children[0];
			ast_node_t *selector_node = node->children[0]->children[1];
			ast_node_t *actuals = node->children[1];
			symtab_entry_t *selector = selector_node->sym;
			node->sym = selector;

			for (int i = 0; i < actuals->children_nr; i++) {
				actuals->children[i] = require_type(actuals->children[i], method_call_param_type);
			}

			if (NODE_FLAGS(receiver) & (TYPE_INT | TYPE_REAL)) {
				error(node, "method received must be an object");
			}

			ast_node_free(node, 0);
			node = CONS(METHODAPP,
				    require_type(receiver, TYPE_OBJ),
				    selector_node,
				    actuals);
			set_type(node, method_call_return_type);
			return node;
		} else {
			error(node, "calls only permitted on functions and methods!");
		}
		break;

	case AST_NODE_FUNDEF:
		if (classref) { // In Methode
			// Methodenparameter sind verpackt und muessen u.U. alle entpackt werden
			int method_args_to_unpack = 0;
			for (int i = 0; i < function->parameters_nr; i++) {
				if (function->parameter_types[i] != method_call_param_type) {
					++method_args_to_unpack;
				}
			}
			if (method_args_to_unpack == 0) {
				return node;
			}

			ast_node_t **formals = node->children[1]->children;
			ast_node_t *init_body = ast_node_alloc_generic_without_init
				(AST_NODE_BLOCK, method_args_to_unpack + 1);

			// Methodenkoerper austauschen
			init_body->children[method_args_to_unpack] = node->children[2];
			node->children[2] = init_body;

			int offset = 0;
			for (int i = 0; i < function->parameters_nr; i++) {
				if (function->parameter_types[i] != method_call_param_type) {
					ast_node_t *assignment = CONS(ASSIGN, 
								      ast_node_clone(formals[i]),
								      ast_node_clone(formals[i]));
					assignment = analyse(assignment, classref, function, context);
					// Erzwinge Konvertierung
					set_type(assignment->children[1], method_call_param_type);
					assignment->children[1] = require_type(assignment->children[1],
									       function->parameter_types[i]);
					init_body->children[offset++] = assignment;
				}
			}
		}
		context->callables[context->functions_nr++] = node;
		break;

	case AST_NODE_CLASSDEF: {
		context->classes[context->classes_nr++] = node;

		classref = node->children[0]->sym;
		node->sym = classref;
		int class_body_size = node->children[2]->children_nr;
		ast_node_t **class_body = node->children[2]->children;

		// Konstruktor-Funktionskoerper erstellen: Groesse berechnen
		int field_inits_nr = 0; // Anzahl der Feldinitialisierungen
		int cons_body_size =
			1	// initiale Allozierung
			+ 1;	// abschliessendes `return'

		for (int i = 0; i < class_body_size; i++) {
			switch (NODE_TY(class_body[i])) {
			case AST_NODE_FUNDEF:
				break;

			case AST_NODE_VARDECL:
				if (class_body[i]->children[1] == NULL) {
					break;
				}
				++field_inits_nr;
			default:
				++cons_body_size;
			}
		}

		// Konstruktorkoerper
		ast_node_t *cons_body_node = ast_node_alloc_generic_without_init(AST_NODE_BLOCK, cons_body_size);
		ast_node_t **cons_body = cons_body_node->children;
		int cons_body_offset = 0; // Schreibposition

		assert(classref);

		ast_node_t *self_update_ref = BUILTIN(SELF);
		self_update_ref->type |= AST_FLAG_LVALUE;
		cons_body[cons_body_offset++] = CONS(ASSIGN,
						     self_update_ref,
						     CONS(FUNAPP,
							  BUILTIN(ALLOCATE),
							  CONS(ACTUALS,
							       CONSV(INT, num = classref->id))));

		for (int i = 0; i < class_body_size; i++) {
			ast_node_t *write = NULL;

			switch (NODE_TY(class_body[i])) {
			case AST_NODE_FUNDEF:
				break;

			case AST_NODE_VARDECL:
				if (class_body[i]->children[1] != NULL) {
					// Initialisierung herausziehen
					write = CONS(ASSIGN,
						     CONS(MEMBER,
							  BUILTIN(SELF),
							  ast_node_clone(class_body[i]->children[0])),
						     class_body[i]->children[1]);
					class_body[i]->children[1] = NULL;
				}
				break;
			default:
				write = class_body[i];
				class_body[i] = NULL;
			};

			if (write) {
				cons_body[cons_body_offset++] = write;
			}
		}
		cons_body[cons_body_offset++] = CONS(RETURN, BUILTIN(SELF));

		// Konstruktor fertigstellen und in Symboltabelle eintragen
		ast_node_t *constructor = CONS(FUNDEF,
					       CONSV(ID, ident = classref->id),
					       ast_node_clone(node->children[1]), // Parameter
					       cons_body_node);
		node->children[3] = constructor;
		symtab_entry_t *constructor_sym = symtab_new(TYPE_OBJ, SYMTAB_TY_FUNCTION | SYMTAB_CONSTRUCTOR,
							     classref->name, constructor);
		constructor_sym->parent = classref;
		constructor->children[0]->sym = constructor_sym;
		context->callables[context->functions_nr++] = constructor;

		// Neuen Klassenkoerper erzeugen: Felder nach vorne, Methoden nach hinten
		ast_node_t *new_class_body_node =
			ast_node_alloc_generic_without_init(AST_NODE_BLOCK,
							    classref->vars_nr + classref->methods_nr);
		ast_node_t **new_class_body = new_class_body_node->children;
		int field_ref = 0;
		int method_ref = classref->vars_nr;

		for (int i = 0; i < class_body_size; i++) {
			ast_node_t *ref = class_body[i];
			class_body[i] = NULL;
			if (ref) {
				switch (NODE_TY(ref)) {
				case AST_NODE_FUNDEF:
					new_class_body[method_ref++] = ref;
					break;

				case AST_NODE_VARDECL:
					new_class_body[field_ref++] = ref;
					break;
				}
			}
		}

		// Neuen Klassenkoerper einsetzen
		ast_node_free(node->children[2], 0);
		node->children[2] = new_class_body_node;
	}
		break;

	case AST_NODE_RETURN:
		if (!function) {
			error(node, "`return' outside of a function body");
			return node;
		}
		if (classref) {
			function->ast_flags = update_type(function->ast_flags, method_call_return_type);
		}
		node->children[0] = require_type(node->children[0],
						 function->ast_flags);
		break;

	case AST_NODE_ISPRIMTY: { // Umwandeln nach ISINSTANCE oder vereinfachen
		ast_node_t *child = node->children[0];
		// Hack: der eingebaute Typ wird als Annotation an diesem Knoten mitangegeben
		int type = node->type & TYPE_FLAGS;

		switch (type) {
		case TYPE_OBJ:
		case TYPE_VAR: // immer wahr
			ast_node_free(node, 1);
			node = CONSV(INT, num = 1);
			set_type(node, TYPE_INT);
			return node;

		case TYPE_INT:
			ast_node_free(node, 0);
			ast_node_t *class_node = CONSV(ID, ident = class_boxed_int.id->id );
			class_node->sym = class_boxed_int.id;
			node = CONS(ISINSTANCE,
				    require_type(child, TYPE_OBJ),
				    class_node);
			set_type(node, TYPE_INT);
			return node;

		default:
			fprintf(stderr, "Unsupported builtin type in `is': %x", type);
			exit(1);
		}
	}

	case AST_NODE_ISINSTANCE: {
		symtab_entry_t *classref = node->children[1]->sym;
		if (classref) {
			if (!(SYMTAB_TY(classref) == SYMTAB_TY_CLASS)) {
				error(node, "`isinstance' on non-class (%s)", classref->name);
			}
		} // ansonsten hat die Namensanalyse bereits einen Fehler gemeldet

		node->children[0] = require_type(node->children[0],
						 TYPE_OBJ);
		set_type(node, TYPE_INT);
	}
		break;

	case AST_NODE_VARDECL:
	case AST_NODE_ASSIGN:
		node->children[1] = require_type(node->children[1], NODE_FLAGS(node->children[0]) & TYPE_FLAGS);
		node->children[0] = require_lvalue(node->children[0],
						   NODE_TY(node) == AST_NODE_VARDECL);
		break;

	case AST_NODE_ARRAYLIST:
		for (int i = 0; i < node->children_nr; i++) {
			node->children[i] = require_type(node->children[i], array_storage_type);
		}
		break;

	case AST_NODE_ARRAYVAL:
		node->children[1] = require_type(node->children[1], TYPE_INT /* Maximalgroesse */);
		set_type(node, TYPE_OBJ);
		break;

	case AST_NODE_ARRAYSUB:
		if (NODE_FLAGS(node->children[0]) & (TYPE_INT | TYPE_REAL)) {
			error(node, "array subscription must be on object, not number");
		}
		node->children[0] = require_type(node->children[0], TYPE_OBJ);
		node->children[1] = require_type(node->children[1], TYPE_INT);
		set_type(node, array_storage_type);
		break;

	case AST_NODE_WHILE:
		node->children[0] = require_type(node->children[0], TYPE_INT);
		break;

	/* case AST_NODE_DOWHILE: */
	/* 	node->children[1] = require_type(node->children[0], TYPE_INT); */
	/* 	break; */

	case AST_NODE_IF:
		node->children[0] = require_type(node->children[0], TYPE_INT);
		break;

	default: ;
	}

	return node;
}

int
type_analysis(ast_node_t **node, ast_node_t **callables, ast_node_t **classes)
{
	if (compiler_options.int_arrays) {
		array_storage_type = TYPE_INT;
	}

	context_t context;
	context.callables = callables;
	context.classes = classes;
	context.functions_nr = 0;
	context.classes_nr = 0;

	error_count = 0;
	*node = analyse(*node, NULL, NULL, &context);
	return error_count;
}
