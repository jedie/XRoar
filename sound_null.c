/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2007  Ciaran Anscomb
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include "config.h"
#if (0)
#include <errno.h>
#include <time.h>
#include <sys/time.h>
#elif defined(HAVE_SDL)
#include <SDL.h>
#endif

#include "types.h"
#include "logging.h"
#include "events.h"
#include "machine.h"
#include "module.h"
#include "xroar.h"

static int init(int argc, char **argv);
static void shutdown(void);
static void update(void);

SoundModule sound_null_module = {
	{ "null", "No audio",
	  init, 0, shutdown, NULL },
	update
};

#define CYCLES_PER_MS (OSCILLATOR_RATE / 1000)

static Cycle last_pause_cycle;
static unsigned int last_pause_ms;

static void flush_frame(void *context);
static event_t *flush_event;

static unsigned int current_time(void);
static void sleep_ms(unsigned int ms);

static int init(int argc, char **argv) {
	(void)argc;
	(void)argv;
	LOG_DEBUG(2,"Initialising null audio driver\n");
	last_pause_cycle = current_cycle;
	last_pause_ms = current_time();
	flush_event = event_new();
	flush_event->dispatch = flush_frame;
	flush_event->at_cycle = current_cycle + (10 * CYCLES_PER_MS);
	event_queue(flush_event);
	return 0;
}

static void shutdown(void) {
}

static void update(void) {
}

static unsigned int current_time(void) {
#if (0)
	struct timeval tp;
	gettimeofday(&tp, NULL);
	return (tp.tv_sec % 1000) * 1000 + (tp.tv_usec / 1000);
#elif defined(HAVE_SDL)
	return SDL_GetTicks();
#else
	return 0;
#endif
}

static void sleep_ms(unsigned int ms) {
#if (0)
	struct timespec elapsed, tv;
	elapsed.tv_sec = (ms) / 1000;
	elapsed.tv_nsec = ((ms) % 1000) * 1000000;
	do {
		errno = 0;
		tv.tv_sec = elapsed.tv_sec;
		tv.tv_nsec = elapsed.tv_nsec;
	} while (nanosleep(&tv, &elapsed) && errno == EINTR);
#elif defined(HAVE_SDL)
	SDL_Delay(ms);
#else
	(void)ms;
#endif
}

static void flush_frame(void *context) {
	Cycle elapsed_cycles = current_cycle - last_pause_cycle;
	unsigned int expected_elapsed_ms = elapsed_cycles / CYCLES_PER_MS;
	unsigned int actual_elapsed_ms, difference_ms;
	(void)context;
	actual_elapsed_ms = current_time() - last_pause_ms;
	difference_ms = expected_elapsed_ms - actual_elapsed_ms;
	if (difference_ms >= 10) {
		if (noratelimit || difference_ms > 1000) {
			last_pause_ms = current_time();
			last_pause_cycle = current_cycle;
		} else {
			sleep_ms(difference_ms);
			difference_ms = current_time() - last_pause_ms;
			last_pause_ms += difference_ms;
			last_pause_cycle += difference_ms * CYCLES_PER_MS;
		}
	}
	flush_event->at_cycle = current_cycle + (10 * CYCLES_PER_MS);
	event_queue(flush_event);
}
