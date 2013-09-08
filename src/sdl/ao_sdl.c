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

#ifdef WINDOWS32
#include <windows.h>
#endif

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

static int buffer_size;
static int buffer_nframes;
static Uint8 *audio_buffer;

static SDL_AudioSpec audiospec;
#ifndef WINDOWS32
 static SDL_mutex *halt_mutex;
 static SDL_cond *halt_cv;
#else
 HANDLE hEvent;
#endif
static int haltflag;

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
	desired.freq = (xroar_cfg.ao_rate > 0) ? xroar_cfg.ao_rate : 44100;
	desired.format = AUDIO_S16;
	if (xroar_cfg.ao_buffer_ms > 0) {
		desired.samples = (desired.freq * xroar_cfg.ao_buffer_ms) / 1000;
	} else if (xroar_cfg.ao_buffer_samples > 0) {
		desired.samples = xroar_cfg.ao_buffer_samples;
	} else {
		desired.samples = 1024;
	}
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
	buffer_nframes = audiospec.samples;
	buffer_size = audiospec.size;
	audio_buffer = g_malloc(buffer_size);
	sound_init(audio_buffer, buffer_fmt, audiospec.freq, audiospec.channels, buffer_nframes);
	LOG_DEBUG(2, "\t%dms (%d samples) buffer\n", (buffer_nframes * 1000) / audiospec.freq, buffer_nframes);

#ifndef WINDOWS32
	halt_mutex = SDL_CreateMutex();
	halt_cv = SDL_CreateCond();
#else
	hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
#endif
	SDL_PauseAudio(0);
	return 1;
failed:
	SDL_CloseAudio();
	SDL_QuitSubSystem(SDL_INIT_AUDIO);
	return 0;
}

static void _shutdown(void) {
#ifndef WINDOWS32
	SDL_DestroyCond(halt_cv);
	SDL_DestroyMutex(halt_mutex);
#endif
	SDL_CloseAudio();
	SDL_QuitSubSystem(SDL_INIT_AUDIO);
	g_free(audio_buffer);
}

static void *write_buffer(void *buffer) {
	if (xroar_noratelimit)
		return buffer;
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
	return buffer;
}

static void callback(void *userdata, Uint8 *stream, int len) {
	(void)userdata;  /* unused */
	if (len == buffer_size) {
		if (haltflag == 1) {
			/* Data is ready */
			memcpy(stream, audio_buffer, buffer_size);
		} else {
			/* Not ready - provide a "padding" frame */
			sound_render_silence(stream, buffer_nframes);
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
