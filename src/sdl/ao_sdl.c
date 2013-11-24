/*  Copyright 2003-2013 Ciaran Anscomb
 *
 *  This file is part of XRoar.
 *
 *  XRoar is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  XRoar is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XRoar.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <signal.h>
#include <stdlib.h>
#include <string.h>

#include <SDL.h>
#include <SDL_thread.h>

#include "pl_glib.h"

#include "logging.h"
#include "machine.h"
#include "module.h"
#include "sound.h"
#include "xroar.h"

static _Bool init(void);
static void _shutdown(void);
static void *write_buffer(void *buffer);

SoundModule sound_sdl_module = {
	.common = { .name = "sdl", .description = "SDL audio",
		    .init = init, .shutdown = _shutdown },
	.write_buffer = write_buffer,
};

static SDL_AudioSpec audiospec;

static Uint8 *audio_buffer;
static _Bool halt;
static _Bool shutting_down;

static SDL_mutex *audio_buffer_mutex;
static SDL_cond *audio_buffer_cv;
static SDL_mutex *halt_mutex;
static SDL_cond *halt_cv;

static void callback(void *userdata, Uint8 *stream, int len);

static _Bool init(void) {
	static SDL_AudioSpec desired;
	if (!SDL_WasInit(SDL_INIT_NOPARACHUTE)) {
		if (SDL_Init(SDL_INIT_NOPARACHUTE) < 0) {
			LOG_ERROR("Failed to initialise SDL\n");
			return 0;
		}
	}
	if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
		LOG_ERROR("Failed to initialise SDL audio\n");
		return 0;
	}
	desired.freq = (xroar_cfg.ao_rate > 0) ? xroar_cfg.ao_rate : 48000;
	desired.format = AUDIO_S16;

	/* More than one fragment not currently supported. */

	if (xroar_cfg.ao_fragment_ms > 0) {
		desired.samples = (desired.freq * xroar_cfg.ao_fragment_ms) / 1000;
	} else if (xroar_cfg.ao_fragment_nframes > 0) {
		desired.samples = xroar_cfg.ao_fragment_nframes;
	} else if (xroar_cfg.ao_buffer_ms > 0) {
		desired.samples = (desired.freq * xroar_cfg.ao_buffer_ms) / 1000;
	} else if (xroar_cfg.ao_buffer_nframes > 0) {
		desired.samples = xroar_cfg.ao_buffer_nframes;
	} else {
		desired.samples = 1024;
	}
	desired.channels = xroar_cfg.ao_channels;
	if (desired.channels < 1 || desired.channels > 2)
		desired.channels = 2;
	desired.callback = callback;
	desired.userdata = NULL;
	if (SDL_OpenAudio(&desired, &audiospec) < 0) {
		LOG_ERROR("Couldn't open audio: %s\n", SDL_GetError());
		SDL_QuitSubSystem(SDL_INIT_AUDIO);
		return 0;
	}

	int buffer_fmt;
	switch (audiospec.format) {
		case AUDIO_U8: buffer_fmt = SOUND_FMT_U8; break;
		case AUDIO_S8: buffer_fmt = SOUND_FMT_S8; break;
		case AUDIO_S16LSB: buffer_fmt = SOUND_FMT_S16_LE; break;
		case AUDIO_S16MSB: buffer_fmt = SOUND_FMT_S16_BE; break;
		default:
			LOG_WARN("Unhandled audio format.");
			goto failed;
	}
	int buffer_nframes = audiospec.samples;

	audio_buffer_mutex = SDL_CreateMutex();
	audio_buffer_cv = SDL_CreateCond();
	halt_mutex = SDL_CreateMutex();
	halt_cv = SDL_CreateCond();

	audio_buffer = NULL;
	halt = 1;
	shutting_down = 0;

	SDL_LockMutex(audio_buffer_mutex);
	SDL_PauseAudio(0);
	while (!audio_buffer)
		SDL_CondWait(audio_buffer_cv, audio_buffer_mutex);
	sound_init(audio_buffer, buffer_fmt, audiospec.freq, audiospec.channels, buffer_nframes);
	audio_buffer = NULL;
	SDL_UnlockMutex(audio_buffer_mutex);
	LOG_DEBUG(2, "\t%dms (%d samples) buffer\n", (buffer_nframes * 1000) / audiospec.freq, buffer_nframes);

	return 1;
failed:
	SDL_CloseAudio();
	SDL_QuitSubSystem(SDL_INIT_AUDIO);
	return 0;
}

static void _shutdown(void) {
	// no more audio
	SDL_PauseAudio(1);

	// unblock audio thread
	SDL_LockMutex(halt_mutex);
	halt = 0;
	shutting_down = 1;
	SDL_CondSignal(halt_cv);
	SDL_UnlockMutex(halt_mutex);

	SDL_CloseAudio();
	SDL_QuitSubSystem(SDL_INIT_AUDIO);

	SDL_DestroyCond(audio_buffer_cv);
	SDL_DestroyMutex(audio_buffer_mutex);
	SDL_DestroyCond(halt_cv);
	SDL_DestroyMutex(halt_mutex);
}

static void *write_buffer(void *buffer) {
	(void)buffer;
	// unblock audio thread
	SDL_LockMutex(halt_mutex);
	halt = 0;
	SDL_CondSignal(halt_cv);
	SDL_UnlockMutex(halt_mutex);

	if (xroar_noratelimit)
		return NULL;

	// wait for audio thread to pass a buffer pointer
	SDL_LockMutex(audio_buffer_mutex);
	while (!audio_buffer)
		SDL_CondWait(audio_buffer_cv, audio_buffer_mutex);
	void *r = audio_buffer;
	audio_buffer = NULL;
	SDL_UnlockMutex(audio_buffer_mutex);
	return r;
}

static void callback(void *userdata, Uint8 *stream, int len) {
	(void)userdata;  /* unused */

	// pass buffer pointer back to main thread
	SDL_LockMutex(audio_buffer_mutex);
	audio_buffer = stream;
	SDL_CondSignal(audio_buffer_cv);
	SDL_UnlockMutex(audio_buffer_mutex);

	// wait for main thread to be done with buffer
	if (!shutting_down) {
		SDL_LockMutex(halt_mutex);
		halt = 1;
		while (halt)
			SDL_CondWait(halt_cv, halt_mutex);
		SDL_UnlockMutex(halt_mutex);
	}
}
