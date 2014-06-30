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
#include <string.h>

#include "symbol-table.h"
#include "runtime.h"
#include "analysis.h"
#include "baseline-backend.h"
#include "compiler-options.h"
#include "dynamic-compiler.h"

struct compiler_options compiler_options = {
	.no_bounds_checks		= false,
	.debug_dynamic_compilation	= false,
	.array_storage_type		= TYPE_OBJ,
	.method_call_param_type		= TYPE_OBJ,
	.method_call_return_type	= TYPE_OBJ
};

runtime_image_t *last = NULL;

runtime_image_t *
runtime_prepare(ast_node_t *ast, unsigned int action)
{
	runtime_image_t *image = calloc(1, sizeof(runtime_image_t));
	image->ast = ast;
	if (action == RUNTIME_ACTION_NONE) {
		return image;
	}

	if (name_analysis(ast, &image->functions_nr, &image->classes_nr)) {
		free(image);
		return NULL;
	}

	image->callables_nr = image->functions_nr + image->classes_nr;

	if (action == RUNTIME_ACTION_NAME_ANALYSIS) {
		return image;
	}

	if (image->callables_nr) {
		image->callables = malloc(sizeof(ast_node_t *) * image->callables_nr);
	}

	if (image->classes_nr) {
		image->classes = malloc(sizeof(ast_node_t *) * image->classes_nr);
	}

	if (type_analysis(&image->ast, image->callables, image->classes)) {
		free(image);
		return NULL;
	}

	if (action == RUNTIME_ACTION_SEMANTIC_ANALYSIS) {
		return image;
	}

	image->ast = ast;
	image->static_memory_size = storage_size(ast);
	if (image->static_memory_size) {
		image->static_memory = malloc(sizeof(void*) * image->static_memory_size);
	} else {
		image->static_memory = NULL;
	}

	image->dyncomp = dyncomp_build_generic();
	image->trampoline = dyncomp_build_trampoline(buffer_entrypoint(image->dyncomp),
						     image->callables, image->functions_nr + image->classes_nr);

	image->code_buffer = baseline_compile_entrypoint(ast, image->static_memory);
	image->main_entry_point = buffer_entrypoint(image->code_buffer);

	last = image;

	return image;
}

// Helfer fuer gdb-breakpoints
void
start_dynamic() {};

void
runtime_execute(runtime_image_t *img)
{
	memset(img->static_memory, 0, sizeof(void*) * img->static_memory_size);
	void (*f)(void) = (void (*)(void)) img->main_entry_point;
	start_dynamic();
	(*f)();
}

void
runtime_free(runtime_image_t *img)
{
	if (img->static_memory_size) {
		free(img->static_memory);
		img->static_memory = NULL;
	}
	if (img->code_buffer) {
		buffer_free(img->code_buffer);
	}
	if (img->callables) {
		for (int i = 0; i < img->callables_nr; i++) {
			buffer_free(buffer_from_entrypoint(img->callables[i]->children[0]->sym->r_mem));
		}
		free(img->callables);
	}
	if (img->classes) {
		free(img->classes);
	}
	if (img->dyncomp) {
		buffer_free(img->dyncomp);
	}
	if (img->trampoline) {
		buffer_free(img->trampoline);
	}
	ast_node_free(img->ast, 1);
	free(img);
}

runtime_image_t *
runtime_current()
{
	return last;
}
