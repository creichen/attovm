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

#ifndef _ATTOL_STACKMAP_H
#define _ATTOL_STACKMAP_H

#include <stdbool.h>
#include "bitvector.h"

/*e
 * Initialises the stack map registry
 */
void
stackmap_init(void);

/*e
 * Clears the stack map registry
 */
void
stackmap_clear(void);

/*e
 * Adds a binding of a memory address (stack return address) to a stack map to the registry
 */
void
stackmap_put(void *address, bitvector_t bitvector);

/*e
 * Requests a stack map from the registry
 *
 * @param address The address to look for
 * @param bitvector Pointer to a bitvector variable to write to
 * @return true iff the address was recognised
 */
bool
stackmap_get(void *address, bitvector_t *bitvector);


#endif // !defined(_ATTOL_STACKMAP_H)
