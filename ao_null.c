/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2011  Ciaran Anscomb
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <signal.h>

#if defined(HAVE_SDL)
#include <SDL.h>
#else
#include <errno.h>
#include <time.h>
#include <sys/time.h>
#endif

#include "types.h"
#include "logging.h"
#include "machine.h"
#include "module.h"
#include "sound.h"
#include "xroar.h"

static int init(void);
static void shutdown(void);
static void flush_frame(void);

SoundModule sound_null_module = {
	.common = { .name = "null", .description = "No audio",
		    .init = init, .shutdown = shutdown },
	.flush_frame = flush_frame,
};

#define CYCLES_PER_MS (OSCILLATOR_RATE / 1000)

static Cycle last_pause_cycle;
static unsigned int last_pause_ms;

static unsigned int current_time(void);
static void sleep_ms(unsigned int ms);

static int init(void) {
	LOG_DEBUG(2,"Initialising null audio driver\n");
	sound_init(44100, 1, SOUND_FMT_NULL, 1024);
	last_pause_cycle = current_cycle;
	last_pause_ms = current_time();
	return 0;
}

static void shutdown(void) {
}

static unsigned int current_time(void) {
#if defined(HAVE_SDL)
	return SDL_GetTicks();
#else
	struct timeval tp;
	gettimeofday(&tp, NULL);
	return (tp.tv_sec % 1000) * 1000 + (tp.tv_usec / 1000);
#endif
}

static void sleep_ms(unsigned int ms) {
#if defined(HAVE_SDL)
	SDL_Delay(ms);
#else
	struct timespec elapsed, tv;
	elapsed.tv_sec = (ms) / 1000;
	elapsed.tv_nsec = ((ms) % 1000) * 1000000;
	do {
		errno = 0;
		tv.tv_sec = elapsed.tv_sec;
		tv.tv_nsec = elapsed.tv_nsec;
	} while (nanosleep(&tv, &elapsed) && errno == EINTR);
#endif
}

static void flush_frame(void) {
	Cycle elapsed_cycles = current_cycle - last_pause_cycle;
	unsigned int expected_elapsed_ms = elapsed_cycles / CYCLES_PER_MS;
	unsigned int actual_elapsed_ms, difference_ms;
	actual_elapsed_ms = current_time() - last_pause_ms;
	difference_ms = expected_elapsed_ms - actual_elapsed_ms;
	if (difference_ms >= 10) {
		if (xroar_noratelimit || difference_ms > 1000) {
			last_pause_ms = current_time();
			last_pause_cycle = current_cycle;
		} else {
			sleep_ms(difference_ms);
			difference_ms = current_time() - last_pause_ms;
			last_pause_ms += difference_ms;
			last_pause_cycle += difference_ms * CYCLES_PER_MS;
		}
	}
}
