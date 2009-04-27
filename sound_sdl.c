/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2009  Ciaran Anscomb
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
#include <SDL.h>
#include <SDL_thread.h>

#include "types.h"
#include "events.h"
#include "logging.h"
#include "machine.h"
#include "module.h"
#include "xroar.h"

static int init(int argc, char **argv);
static void shutdown(void);
static void update(int value);

SoundModule sound_sdl_module = {
	{ "sdl", "SDL ring-buffer audio",
	  init, 0, shutdown },
	update
};

typedef Sint8 Sample;  /* 8-bit mono (SDL type) */

#ifdef WINDOWS32
# define SAMPLE_RATE 22050
#else
# define SAMPLE_RATE 44100
#endif
/* The lower the FRAME_SIZE, the better.  Windows32 seems to have problems
 * with very small frame sizes though. */
#ifdef WINDOWS32
# define FRAME_SIZE 1024
#else
# define FRAME_SIZE 512
#endif
#define SAMPLE_CYCLES ((int)(OSCILLATOR_RATE / SAMPLE_RATE))
#define FRAME_CYCLES (SAMPLE_CYCLES * FRAME_SIZE)

static SDL_AudioSpec audiospec;
static Cycle frame_cycle_base;
static Sample *buffer;
static Sample *wrptr;
static Sample lastsample;
static SDL_mutex *halt_mutex;
static SDL_cond *halt_cv;
static int haltflag;

static void flush_frame(void);
static event_t *flush_event;

static void callback(void *userdata, Uint8 *stream, int len);

static int init(int argc, char **argv) {
	(void)argc;
	(void)argv;
	LOG_DEBUG(2,"Initialising SDL audio driver\n");
#ifdef WINDOWS32
	//if (!getenv("SDL_AUDIODRIVER"))
		//putenv("SDL_AUDIODRIVER=windib");
	//if (!getenv("SDL_MIXERDRIVER"))
		//putenv("SDL_MIXERDRIVER=windib");
#endif
	if (!SDL_WasInit(SDL_INIT_NOPARACHUTE)) {
		if (SDL_Init(SDL_INIT_NOPARACHUTE) < 0) {
			LOG_ERROR("Failed to initialiase SDL\n");
			return 1;
		}
	}
	if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
		LOG_ERROR("Failed to initialiase SDL audio driver\n");
		return 1;
	}
	audiospec.freq = SAMPLE_RATE;
	audiospec.format = AUDIO_S8;
	audiospec.samples = FRAME_SIZE;
	audiospec.channels = 1;
	audiospec.callback = callback;
	audiospec.userdata = NULL;
	if (SDL_OpenAudio(&audiospec, NULL) < 0) {
		LOG_ERROR("Couldn't open audio: %s\n", SDL_GetError());
		SDL_QuitSubSystem(SDL_INIT_AUDIO);
		return 1;
	}
	buffer = (Sample *)malloc(FRAME_SIZE * sizeof(Sample));
	halt_mutex = SDL_CreateMutex();
	halt_cv = SDL_CreateCond();
	flush_event = event_new();
	flush_event->dispatch = flush_frame;

	memset(buffer, 0, FRAME_SIZE * sizeof(Sample));
	SDL_PauseAudio(0);
	wrptr = buffer;
	frame_cycle_base = current_cycle;
	flush_event->at_cycle = frame_cycle_base + FRAME_CYCLES;
	event_queue(&MACHINE_EVENT_LIST, flush_event);
	lastsample = 0;
	return 0;
}

static void shutdown(void) {
	LOG_DEBUG(2,"Shutting down SDL audio driver\n");
	event_free(flush_event);
	SDL_DestroyCond(halt_cv);
	SDL_DestroyMutex(halt_mutex);
	SDL_CloseAudio();
	SDL_QuitSubSystem(SDL_INIT_AUDIO);
	free(buffer);
}

static void update(int value) {
	Cycle elapsed_cycles = current_cycle - frame_cycle_base;
	Sample *fill_to;
	if (elapsed_cycles >= FRAME_CYCLES) {
		fill_to = buffer + FRAME_SIZE;
	} else {
		fill_to = buffer + (elapsed_cycles/(Cycle)SAMPLE_CYCLES);
	}
	while (wrptr < fill_to)
		*(wrptr++) = lastsample;
	lastsample = value;
}

static void flush_frame(void) {
	Sample *fill_to = buffer + FRAME_SIZE;
	while (wrptr < fill_to)
		*(wrptr++) = lastsample;
	frame_cycle_base += FRAME_CYCLES;
	flush_event->at_cycle = frame_cycle_base + FRAME_CYCLES;
	event_queue(&MACHINE_EVENT_LIST, flush_event);
	wrptr = buffer;
	if (!xroar_noratelimit) {
		SDL_LockMutex(halt_mutex);
		haltflag = 1;
		while (haltflag)
			SDL_CondWait(halt_cv, halt_mutex);
		SDL_UnlockMutex(halt_mutex);
	}
}

static void callback(void *userdata, Uint8 *stream, int len) {
	(void)userdata;  /* unused */
	if (len == FRAME_SIZE) {
		memcpy(stream, buffer, len);
		memset(buffer, 0, len);
	}
	SDL_LockMutex(halt_mutex);
	haltflag = 0;
	SDL_CondSignal(halt_cv);
	SDL_UnlockMutex(halt_mutex);
}
