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

#ifndef _ATTOL_COMPILER_OPTIONS_H
#define _ATTOL_COMPILER_OPTIONS_H

#include <stdbool.h>
#include "ast.h"

struct compiler_options {
	bool no_bounds_checks; /*d Keine Arrayschrankenpruefung */ /*e no array-out-of-bounds checks */
	bool debug_dynamic_compilation; /*d Drucke Ausgaben aus, waehrend dynamische Uebersetzung aktiv ist */
	bool debug_assembly;
	bool debug_adaptive;
	bool debug_gc;
	bool no_adaptive_compilation;

	int array_storage_type;
	int method_call_param_type;
	int method_call_return_type;
	size_t heap_size; /*e available heap memory size */
};

extern struct compiler_options compiler_options;

#endif // !defined(_ATTOL_COMPILER_OPTIONS_H)
