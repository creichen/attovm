/***************************************************************************
  Copyright (C) 2015 Christoph Reichenbach


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

#ifndef _ATTOL_TIMER_H
#define _ATTOL_TIMER_H

#include <stdbool.h>
#include <stdio.h>
#include <time.h>

typedef struct {
	unsigned long seconds;
	unsigned long nanoseconds;
} time_delta_t;

typedef struct timer {
	struct timespec start_time;
	bool running;
	time_delta_t aggregate;
} wallclock_timer_t;

void
timer_reset(wallclock_timer_t *timer);

void
timer_start(wallclock_timer_t *timer);

/*e
 * Pauses the timer; can be resumed with timer_start
 */
void
timer_stop(wallclock_timer_t *timer);

void
timer_print(FILE *file, wallclock_timer_t *timer);

#endif // !defined(_ATTOL_TIMER_H)
