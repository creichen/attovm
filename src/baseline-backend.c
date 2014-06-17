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

#include "baseline-backend.h"
#include "assembler.h"

int
baseline_compile_expr(buffer_t *buf, int dest_register, ast_node_t *node, int *temp_regs, int temp_regs_nr)
{
	emit_li(buf, dest_register, 42);
	return TYPE_INT;
}

/* void */
/* baseline_compile_convert(buffer_t *dest, int dest_type, int from_type, int reg) */
/* { */
/* 	if (dest_type == from_type) { */
/* 		return; */
/* 	} */

/* 	if (dest_type & TYPE_OBJ) { */
/* 		if (from_type & TYPE_INT) { */
			
/* 			return; */
/* 		} else if (from_type & TYPE_REAL) { */
/* 		} if (from_type & TYPE_VAR) { */
/* 		} */
/* 	} else if (dest_type & TYPE_INT) { */
/* 		if (from_type & TYPE_OBJ) { */
/* 		} else if (from_type & TYPE_REAL) { */
/* 		} if (from_type & TYPE_VAR) { */
/* 		} */
/* 	} else if (dest_type & TYPE_REAL) { */
/* 		if (from_type & TYPE_INT) { */
/* 		} else if (from_type & TYPE_OBJ) { */
/* 		} if (from_type & TYPE_VAR) { */
/* 		} */
/* 	} else if (dest_type & TYPE_VAR) { */
/* 		if (from_type & TYPE_INT) { */
/* 		} else if (from_type & TYPE_REAL) { */
/* 		} if (from_type & TYPE_VAR) { */
/* 		} */
/* 	} */
/* 	fprintf(stderr, "Baseline conversion failed: %x <- %x", dest_type, from_type); */
/* } */
