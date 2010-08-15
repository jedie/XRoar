/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2010  Ciaran Anscomb
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

#define REQUEST_SAMPLE_RATE 44100
#define REQUEST_FRAME_SIZE 512

static int sample_cycles;
static int frame_size;
static int frame_cycles;
static uint8_t sample_eor;

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
	static SDL_AudioSpec desired;
	LOG_DEBUG(2,"Initialising SDL audio driver\n");
	if (!SDL_WasInit(SDL_INIT_NOPARACHUTE)) {
		if (SDL_Init(SDL_INIT_NOPARACHUTE) < 0) {
			LOG_ERROR("Failed to initialise SDL\n");
			return 1;
		}
	}
	if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
		LOG_ERROR("Failed to initialise SDL audio driver\n");
		return 1;
	}
	desired.freq = REQUEST_SAMPLE_RATE;
	desired.format = AUDIO_U8;
	desired.samples = REQUEST_FRAME_SIZE;
	desired.channels = 1;
	desired.callback = callback;
	desired.userdata = NULL;
	if (SDL_OpenAudio(&desired, &audiospec) < 0) {
		LOG_ERROR("Couldn't open audio: %s\n", SDL_GetError());
		SDL_QuitSubSystem(SDL_INIT_AUDIO);
		return 1;
	}

	sample_eor = 0;
	/* TODO: Need to abstract this logging out */
	LOG_DEBUG(2, "\t");
	switch (audiospec.format) {
		case AUDIO_U8: LOG_DEBUG(2, "8-bit unsigned, "); sample_eor = 0x80; break;
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

	if ((audiospec.format != AUDIO_U8 && audiospec.format != AUDIO_S8)
	    || (audiospec.channels != 1)) {
		LOG_ERROR("Obtained unsupported audio format.\n");
		SDL_CloseAudio();
		SDL_QuitSubSystem(SDL_INIT_AUDIO);
		return 1;
	}

	sample_cycles = OSCILLATOR_RATE / audiospec.freq;
	frame_size = audiospec.samples;
	frame_cycles = sample_cycles * frame_size;

	buffer = (Sample *)malloc(frame_size * sizeof(Sample));
#ifndef WINDOWS32
	halt_mutex = SDL_CreateMutex();
	halt_cv = SDL_CreateCond();
#else
	hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
#endif
	flush_event = event_new();
	flush_event->dispatch = flush_frame;

	memset(buffer, sample_eor, frame_size * sizeof(Sample));
	SDL_PauseAudio(0);
	wrptr = buffer;
	frame_cycle_base = current_cycle;
	frame_cycle = 0;
	flush_event->at_cycle = frame_cycle_base + frame_cycles;
	event_queue(&MACHINE_EVENT_LIST, flush_event);
	lastsample = sample_eor;
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
	if (elapsed_cycles >= frame_cycles) {
		elapsed_cycles = frame_cycles;
	}
	while (frame_cycle < elapsed_cycles) {
		*(wrptr++) = lastsample;
		frame_cycle += sample_cycles;
	}
	lastsample = value ^ sample_eor;
}

static void flush_frame(void) {
	Sample *fill_to = buffer + frame_size;
	while (wrptr < fill_to)
		*(wrptr++) = lastsample;
	frame_cycle_base += frame_cycles;
	frame_cycle = 0;
	flush_event->at_cycle = frame_cycle_base + frame_cycles;
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
	if (len == frame_size) {
		if (haltflag == 1) {
			/* Data is ready */
			memcpy(stream, buffer, len * sizeof(Sample));
		} else {
			/* Not ready - provide a "padding" frame */
			memset(buffer, buffer[len-1], len * sizeof(Sample));
		}
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
