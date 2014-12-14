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

/*e stack implementation */

#ifndef _ATTOL_CSTACK_H
#define _ATTOL_CSTACK_H

#include <stdlib.h>
#include <stdio.h>

struct cstack;
typedef struct cstack cstack_t;


/*e
 * Allocates a fresh cstack_t
 *
 * @param element_size Size of a single element
 * @param initial_capacity Initial stack capacity (tuning parameter)
 */
cstack_t *
stack_alloc(size_t element_size, size_t initial_capacity);

/*e
 * Determines the number of elements in a given stack
 *
 * @param stack The stack whose size we are to get
 * @return The stack's size
 */
size_t
stack_size(cstack_t *stack);

/*e
 * Pushes one element onto the stack
 *
 * @param stack The stack to append to
 * @param element Pointer to the element to push/copy onto the stack
 */
void
stack_push(cstack_t *stack, void *element);

/*e
 * Pops an element from the stack
 *
 * @param stack The stack to access
 * @param element Pointer to the removed element (valid until the next append operation), or NULL if the stack was empty
 */
void *
stack_pop(cstack_t *stack);

/*e
 * Directly accesses an element on the stack
 *
 * The returned pointer may become invalid on the next push or pop operation,
 * so it must no longer be used then.
 *
 * @param stack The stack to access
 * @param index Index of the element to read
 * @return Pointer to the element, or NULL if `index' was invalid
 */
void *
stack_get(cstack_t *stack, size_t index);

/*e
 * Duplicates an existing stack
 *
 * @param stack The stack to clone
 * @param clone A function to apply to the stack elements after copying (e.g., to duplicate dynamically
 *        allocated memory) or NULL to skip this step.
 * @return The duplicate
 */
cstack_t *
stack_clone(cstack_t *stack, void (*clone)(void *));

/*e
 * Removes all elements from a stack
 *
 * Afterwards, the stack will behave as if freshly allocated.
 *
 * @param stack The stack to clear
 * @param element_free A function to deallocate stack elements, or NULL if not needed
 */
void
stack_clear(cstack_t *stack, void (*free)(void *));

/*e
 * Deallocates a stack
 *
 * @param stack The stack to deallocate
 * @param element_free A function to deallocate stack elements, or NULL if not needed
 */
void
stack_free(cstack_t *stack, void (*element_free)(void *));

/*e
 * Prints the given stack
 *
 * @param file File to print to
 * @param stack
 * @param print A printing function for each stack element
 */
void
stack_print(FILE *file, cstack_t *stack, void (*print)(FILE *, void *));

#endif // !defined(_ATTOL_CSTACK_H)
