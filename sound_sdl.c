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

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <SDL.h>
#include <SDL_thread.h>

#ifdef WINDOWS32
#include <windows.h>
#endif

#include "types.h"
#include "events.h"
#include "logging.h"
#include "machine.h"
#include "module.h"
#include "xroar.h"

static int init(void);
static void _shutdown(void);
static void update(int value);

SoundModule sound_sdl_module = {
	{ "sdl", "SDL ring-buffer audio",
	  init, 0, _shutdown },
	update
};

typedef Uint8 Sample;  /* 8-bit mono (SDL type) */

#define SAMPLE_RATE 44100
#define FRAME_SIZE 512
#define SAMPLE_CYCLES ((int)(OSCILLATOR_RATE / SAMPLE_RATE))
#define FRAME_CYCLES (SAMPLE_CYCLES * FRAME_SIZE)

static SDL_AudioSpec audiospec;
static Cycle frame_cycle_base;
static int frame_cycle;
static Sample *buffer;
static Sample *wrptr;
static Sample lastsample;
#ifndef WINDOWS32
 static SDL_mutex *halt_mutex;
 static SDL_cond *halt_cv;
#else
 HANDLE hEvent;
#endif
static int haltflag;

static void flush_frame(void);
static event_t *flush_event;

static void callback(void *userdata, Uint8 *stream, int len);

static int init(void) {
	LOG_DEBUG(2,"Initialising SDL audio driver\n");
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
	audiospec.format = AUDIO_U8;
	audiospec.samples = FRAME_SIZE;
	audiospec.channels = 1;
	audiospec.callback = callback;
	audiospec.userdata = NULL;
	if (SDL_OpenAudio(&audiospec, NULL) < 0) {
		LOG_ERROR("Couldn't open audio: %s\n", SDL_GetError());
		SDL_QuitSubSystem(SDL_INIT_AUDIO);
		return 1;
	}

	/* TODO: Need to abstract this logging out */
	LOG_DEBUG(2, "\t");
	switch (audiospec.format) {
		case AUDIO_U8: LOG_DEBUG(2, "8-bit unsigned, "); break;
		case AUDIO_S8: LOG_DEBUG(2, "8-bit signed, "); break;
		case AUDIO_U16LSB: LOG_DEBUG(2, "16-bit unsigned little-endian, "); break;
		case AUDIO_S16LSB: LOG_DEBUG(2, "16-bit signed little-endian, "); break;
		case AUDIO_U16MSB: LOG_DEBUG(2, "16-bit unsigned big-endian, "); break;
		case AUDIO_S16MSB: LOG_DEBUG(2, "16-bit signed big-endian, "); break;
		default: LOG_DEBUG(2, "unknown, "); break;
	}
	switch (audiospec.channels) {
		case 1: LOG_DEBUG(2, "mono, "); break;
		case 2: LOG_DEBUG(2, "stereo, "); break;
		default: LOG_DEBUG(2, "%d channel, ", audiospec.channels); break;
	}
	LOG_DEBUG(2, "%dHz\n", audiospec.freq);

	buffer = (Sample *)malloc(FRAME_SIZE * sizeof(Sample));
#ifndef WINDOWS32
	halt_mutex = SDL_CreateMutex();
	halt_cv = SDL_CreateCond();
#else
	hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
#endif
	flush_event = event_new();
	flush_event->dispatch = flush_frame;

	memset(buffer, 0x80, FRAME_SIZE * sizeof(Sample));
	SDL_PauseAudio(0);
	wrptr = buffer;
	frame_cycle_base = current_cycle;
	frame_cycle = 0;
	flush_event->at_cycle = frame_cycle_base + FRAME_CYCLES;
	event_queue(&MACHINE_EVENT_LIST, flush_event);
	lastsample = 0x80;
	return 0;
}

static void _shutdown(void) {
	LOG_DEBUG(2,"Shutting down SDL audio driver\n");
	event_free(flush_event);
#ifndef WINDOWS32
	SDL_DestroyCond(halt_cv);
	SDL_DestroyMutex(halt_mutex);
#endif
	SDL_CloseAudio();
	SDL_QuitSubSystem(SDL_INIT_AUDIO);
	free(buffer);
}

static void update(int value) {
	int elapsed_cycles = current_cycle - frame_cycle_base;
	if (elapsed_cycles >= FRAME_CYCLES) {
		elapsed_cycles = FRAME_CYCLES;
	}
	while (frame_cycle < elapsed_cycles) {
		*(wrptr++) = lastsample;
		frame_cycle += SAMPLE_CYCLES;
	}
	lastsample = value ^ 0x80;
}

static void flush_frame(void) {
	Sample *fill_to = buffer + FRAME_SIZE;
	while (wrptr < fill_to)
		*(wrptr++) = lastsample;
	frame_cycle_base += FRAME_CYCLES;
	frame_cycle = 0;
	flush_event->at_cycle = frame_cycle_base + FRAME_CYCLES;
	event_queue(&MACHINE_EVENT_LIST, flush_event);
	wrptr = buffer;
	if (!xroar_noratelimit) {
#ifndef WINDOWS32
		SDL_LockMutex(halt_mutex);
		haltflag = 1;
		while (haltflag)
			SDL_CondWait(halt_cv, halt_mutex);
		SDL_UnlockMutex(halt_mutex);
#else
		haltflag = 1;
		WaitForSingleObject(hEvent, INFINITE);
#endif
	}
}

static void callback(void *userdata, Uint8 *stream, int len) {
	(void)userdata;  /* unused */
	if (len == FRAME_SIZE) {
		memcpy(stream, buffer, len);
		memset(buffer, buffer[len-1], len);
	}
#ifndef WINDOWS32
	SDL_LockMutex(halt_mutex);
	haltflag = 0;
	SDL_CondSignal(halt_cv);
	SDL_UnlockMutex(halt_mutex);
#else
	haltflag = 0;
	SetEvent(hEvent);
#endif
}
