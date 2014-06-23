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

#include "runtime.h"
#include "analysis.h"
#include "baseline-backend.h"
#include "compiler-options.h"

struct compiler_options compiler_options;


runtime_image_t *
runtime_prepare(ast_node_t *ast, unsigned int action)
{
	runtime_image_t *image = calloc(1, sizeof(runtime_image_t));
	image->ast = ast;
	if (action == RUNTIME_ACTION_NONE) {
		return image;
	}

	if (name_analysis(ast)) {
		free(image);
		return NULL;
	}

	if (action == RUNTIME_ACTION_NAME_ANALYSIS) {
		return image;
	}

	if (type_analysis(&image->ast)) {
		free(image);
		return NULL;
	}

	if (action == RUNTIME_ACTION_SEMANTIC_ANALYSIS) {
		return image;
	}

	image->ast = ast;
	image->static_memory_size = storage_allocation(ast);
	if (image->static_memory_size) {
		image->static_memory = malloc(sizeof(void*) * image->static_memory_size);
	} else {
		image->static_memory = NULL;
	}
	image->code_buffer = baseline_compile(ast, image->static_memory);
	image->main_entry_point = buffer_entrypoint(image->code_buffer);

	return image;
}

// Helfer fuer gdb-breakpoints
void
start_dynamic() {};

void
runtime_execute(runtime_image_t *img)
{
	memset(img->static_memory, 0, sizeof(void**) * img->static_memory_size);
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
	ast_node_free(img->ast, 1);
	free(img);
}
