/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2004  Ciaran Anscomb
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

#ifdef HAVE_SDL_AUDIO

#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <SDL.h>
#include <SDL_thread.h>

#include "sound.h"
#include "pia.h"
#include "xroar.h"
#include "logging.h"
#include "types.h"

static int init(void);
static void shutdown(void);
static void reset(void);
static void update(void);

SoundModule sound_sdl_module = {
	NULL,
	"sdl",
	"SDL ring-buffer audio",
	init, shutdown, reset, update
};

typedef Uint8 Sample;  /* 8-bit mono (SDL type) */
typedef uint_fast8_t Sample_f;  /* Fastest for manipulating above */

#define SAMPLE_RATE 22050
#define CHANNELS 1
#define FORMAT AFMT_U8
#define FRAGMENTS 2
#define FRAME_SIZE 512
#define SAMPLE_CYCLES ((uint32_t)(OSCILLATOR_RATE / SAMPLE_RATE))
#define FRAME_CYCLES (SAMPLE_CYCLES * FRAME_SIZE)

static SDL_AudioSpec audiospec;
static Cycle frame_cycle_base;
static Sample *buffer;
static Sample *wrptr;
static Sample_f lastsample;
static SDL_mutex *halt_mutex;
static SDL_cond *halt_cv;
static int haltflag;

static void callback(void *userdata, Uint8 *stream, int len);

extern JoystickModule joystick_sdl_module;

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
	audiospec.channels = CHANNELS;
	audiospec.callback = callback;
	audiospec.userdata = NULL;
	if (SDL_OpenAudio(&audiospec, NULL) < 0) {
		LOG_ERROR("Couldn't open audio: %s\n", SDL_GetError());
		SDL_QuitSubSystem(SDL_INIT_AUDIO);
		return 1;
	}
	buffer = (Sample *)malloc(FRAME_SIZE * sizeof(Sample));
	/* Set preferred joystick driver */
	joystick_module = &joystick_sdl_module;
	halt_mutex = SDL_CreateMutex();
	halt_cv = SDL_CreateCond();
	return 0;
}

static void shutdown(void) {
	LOG_DEBUG(2,"Shutting down SDL audio driver\n");
	//SDL_PauseAudio(1);
	SDL_DestroyCond(halt_cv);
	SDL_DestroyMutex(halt_mutex);
	SDL_CloseAudio();
	SDL_QuitSubSystem(SDL_INIT_AUDIO);
	free(buffer);
}

static void reset(void) {
	memset(buffer, 0x80, FRAME_SIZE * sizeof(Sample));
	SDL_PauseAudio(0);
	frame_cycle_base = current_cycle;
	next_sound_update = frame_cycle_base + FRAME_CYCLES;
	wrptr = buffer;
	lastsample = 0;
}

static void update(void) {
	Cycle elapsed_cycles = current_cycle - frame_cycle_base;
	Sample_f fill_with;
	Sample *fill_to;
	if (!(PIA_1B.control_register & 0x08)) {
		/* Single-bit sound */
		fill_with = ((PIA_1B.port_output & 0x02) << 5) ^ 0x80;
	} else  {
		if (PIA_0B.control_register & 0x08) {
			/* Sound disabled */
			fill_with = 0x80;
		} else {
			/* DAC output */
			fill_with = ((PIA_1A.port_output & 0xfc) >> 1) ^ 0x80;
		}
	}
	if (elapsed_cycles >= (FRAME_CYCLES*2)) {
		LOG_ERROR("Eek!  Sound buffer overrun!\n");
		frame_cycle_base = current_cycle;
		next_sound_update = frame_cycle_base + FRAME_CYCLES;
		wrptr = buffer;
		lastsample = fill_with;
		return;
	} else {
		fill_to = buffer + (elapsed_cycles/(Cycle)SAMPLE_CYCLES);
	}
	while (wrptr < fill_to && wrptr < (buffer + FRAME_SIZE))
		*(wrptr++) = lastsample;
	if (elapsed_cycles >= FRAME_CYCLES) {
		SDL_LockMutex(halt_mutex);
		haltflag = 1;
		while (haltflag) {
			SDL_CondWait(halt_cv, halt_mutex);
		}
		SDL_UnlockMutex(halt_mutex);
		frame_cycle_base += FRAME_CYCLES;
		next_sound_update = frame_cycle_base + FRAME_CYCLES;
		wrptr -= FRAME_SIZE;
		fill_to -= FRAME_SIZE;
		if (wrptr < fill_to) {
			LOG_WARN("Buffer overrun\n");
		}
		while (wrptr < fill_to)
			*(wrptr++) = lastsample;
	}
	lastsample = fill_with;
	return;
}

static void callback(void *userdata, Uint8 *stream, int len) {
	if (len == FRAME_SIZE) {
		memcpy(stream, buffer, len);
		memset(buffer, 0x80, len);
	}
	SDL_LockMutex(halt_mutex);
	haltflag = 0;
	SDL_CondSignal(halt_cv);
	SDL_UnlockMutex(halt_mutex);
}

#endif  /* HAVE_SDL_AUDIO */
