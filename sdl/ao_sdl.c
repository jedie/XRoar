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
#include <SDL.h>
#include <SDL_thread.h>

#ifdef WINDOWS32
#include <windows.h>
#endif

#include "types.h"
#include "logging.h"
#include "machine.h"
#include "module.h"
#include "sound.h"
#include "xroar.h"

static int init(void);
static void _shutdown(void);
static void flush_frame(void);

SoundModule sound_sdl_module = {
	.common = { .name = "sdl", .description = "SDL ring-buffer audio",
		    .init = init, .shutdown = _shutdown },
	.flush_frame = flush_frame,
};

static int buffer_bytes;
static uint8_t *buffer;

static SDL_AudioSpec audiospec;
#ifndef WINDOWS32
 static SDL_mutex *halt_mutex;
 static SDL_cond *halt_cv;
#else
 HANDLE hEvent;
#endif
static int haltflag;

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
	desired.freq = (xroar_opt_ao_rate > 0) ? xroar_opt_ao_rate : 44100;
	desired.format = AUDIO_U8;
	if (xroar_opt_ao_buffer_ms > 0) {
		desired.samples = (desired.freq * xroar_opt_ao_buffer_ms) / 1000;
	} else if (xroar_opt_ao_buffer_samples > 0) {
		desired.samples = xroar_opt_ao_buffer_samples;
	} else {
		desired.samples = 1024;
	}
	desired.channels = 1;
	desired.callback = callback;
	desired.userdata = NULL;
	if (SDL_OpenAudio(&desired, &audiospec) < 0) {
		LOG_ERROR("Couldn't open audio: %s\n", SDL_GetError());
		SDL_QuitSubSystem(SDL_INIT_AUDIO);
		return 1;
	}

	int request_fmt;
	switch (audiospec.format) {
		case AUDIO_U8: request_fmt = SOUND_FMT_U8; break;
		case AUDIO_S8: request_fmt = SOUND_FMT_S8; break;
		case AUDIO_S16LSB: request_fmt = SOUND_FMT_S16_LE; break;
		case AUDIO_S16MSB: request_fmt = SOUND_FMT_S16_BE; break;
		default:
			LOG_WARN("Unhandled audio format.");
			goto failed;
	}
	int buffer_length = audiospec.samples;
	buffer_bytes = audiospec.size;
	buffer = sound_init(audiospec.freq, audiospec.channels, request_fmt, buffer_length);
	LOG_DEBUG(2, "\t%dms (%d samples) buffer\n", (buffer_length * 1000) / audiospec.freq, buffer_length);

#ifndef WINDOWS32
	halt_mutex = SDL_CreateMutex();
	halt_cv = SDL_CreateCond();
#else
	hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
#endif
	SDL_PauseAudio(0);
	return 0;
failed:
	SDL_CloseAudio();
	SDL_QuitSubSystem(SDL_INIT_AUDIO);
	return 1;
}

static void _shutdown(void) {
	LOG_DEBUG(2,"Shutting down SDL audio driver\n");
#ifndef WINDOWS32
	SDL_DestroyCond(halt_cv);
	SDL_DestroyMutex(halt_mutex);
#endif
	SDL_CloseAudio();
	SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

static void flush_frame(void) {
	if (xroar_noratelimit)
		return;
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

static void callback(void *userdata, Uint8 *stream, int len) {
	(void)userdata;  /* unused */
	if (len == buffer_bytes) {
		if (haltflag == 1) {
			/* Data is ready */
			memcpy(stream, buffer, buffer_bytes);
		} else {
			/* Not ready - provide a "padding" frame */
			sound_render_silence(buffer, len);
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
