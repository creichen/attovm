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

#include "timer.h"

#define SECNANO 1000000000ul /*e nanoseconds per second */

static time_delta_t
timespec_difference(struct timespec ts1, struct timespec ts2)
{
	unsigned long sec = ts2.tv_sec - ts1.tv_sec;
	signed long nano = (signed)ts2.tv_nsec - (signed)ts1.tv_nsec;

	if (nano < 0) {
		--sec;
		nano += SECNANO;
	}

	time_delta_t d = (time_delta_t) {
		.seconds	= sec,
		.nanoseconds	= nano
	};
	return d;
}

static time_delta_t
delta_sum(time_delta_t delta1, time_delta_t delta2)
{
	delta1.seconds += delta2.seconds;
	delta1.nanoseconds += delta2.nanoseconds;
	if (delta1.nanoseconds >= SECNANO) {
		delta1.nanoseconds -= SECNANO;
		++delta1.seconds;
	}
	return delta1;
}

void
timer_reset(wallclock_timer_t *timer)
{
	timer->running = false;
	timer->aggregate.seconds = 0ul;
	timer->aggregate.nanoseconds = 0ul;
}

void
timer_start(wallclock_timer_t *timer)
{
	if (timer->running) {
		return;
	}
	clock_gettime(CLOCK_MONOTONIC, &timer->start_time);

	timer->running = true;;
}

static void
timer_update_aggregate(wallclock_timer_t *timer)
{
	struct timespec tmp;
	clock_gettime(CLOCK_MONOTONIC, &tmp);

	timer->aggregate = delta_sum(timer->aggregate,
				     timespec_difference(timer->start_time, tmp));
	timer->start_time = tmp;
}

void
timer_pause(wallclock_timer_t *timer)
{
	timer->running = false;
	timer_update_aggregate(timer);
}

void
timer_print(FILE *file, wallclock_timer_t *timer)
{
	if (timer->running) {
		timer_update_aggregate(timer);
	}
	fprintf(file, "%lu.%09lu",
		timer->aggregate.seconds,
		timer->aggregate.nanoseconds);
}

