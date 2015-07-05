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

#ifndef _ATTOL_HEAP_H
#define _ATTOL_HEAP_H

#include "class.h"
#include "object.h"

extern void *heap_root_frame_pointer; /*e points to the frame pointer of the loader stack frame */

/*e
 * Creates the heap
 *
 * Note that the heap is not cleared initially.  Call heap_clear() before allocating.
 *
 * @param size_t Maximum heap size
 */
void
heap_init(size_t heap_size);

/*e
 * Deallocates the heap
 */
void
heap_free(void);

/*e
 * Allocates an object on the heap
 *
 * This function always returns successfully _if it returns_.
 * If the system runs out of heap space, this function will trigger
 * garbage collection or abort.
 *
 * @param type Type descriptor
 * @param fields_nr number of 8-byte fields to allocate
 * @return A heap object
 */
/*d
 * Alloziert ein Objekt auf dem Ablagespeicher
 *
 * Diese Funktion ist wird immer erfolgreich zurückkehren oder das
 * Programm abbrechen, wenn nicht genug Speicher gefunden werden kann.  Sie loesst
 * die automatische Speicherbereinigung aus, soweit noetig.
 *
 * @param type Typdeskriptor
 * @param fields_nr Anzahl der Felder (8-Byte-Einträge), die zu allozieren sind
 * @return Ein Objekt im Ablagespeicher, das insgesamt (sizeof(class_t *) + fields_nr * 8) Bytes Platz belegt.
 */
object_t *
heap_allocate_object(class_t* type, size_t fields_nr);

/*e
 * Determines the amount of currently unallocated heap space, in bytes
 */
size_t
heap_available(void);

/*e
 * Determines the total size of the heap, both allocated and unallocated, in bytes
 */
size_t
heap_size(void);

#endif // !defined(_ATTOL_HEAP_H)
