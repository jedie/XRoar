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

/* SDL processes audio in a separate thread, using a callback to request more
 * data.  When the configured number of audio fragments (nfragments) is 1,
 * write directly into the buffer provided by SDL.  When nfragments > 1,
 * maintain a queue of fragment buffers; the callback takes the next filled
 * buffer from the queue and copies its data into place. */

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

static void *callback_buffer;
static _Bool shutting_down;

static unsigned nfragments;
static unsigned fragment_nbytes;

static SDL_mutex *fragment_mutex;
static SDL_cond *fragment_cv;
static void **fragment_buffer;
static unsigned fragment_queue_length;
static unsigned write_fragment;
static unsigned play_fragment;

static unsigned timeout_ms;

static void callback(void *, Uint8 *, int);
static void callback_1(void *, Uint8 *, int);

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

	unsigned rate = 48000;
	unsigned nchannels = 2;
	unsigned fragment_nframes;
	unsigned buffer_nframes;
	unsigned sample_nbytes;
	enum sound_fmt sample_fmt;

	if (xroar_cfg.ao_rate > 0)
		rate = xroar_cfg.ao_rate;

	if (xroar_cfg.ao_channels >= 1 && xroar_cfg.ao_channels <= 2)
		nchannels = xroar_cfg.ao_channels;

	nfragments = 2;
	if (xroar_cfg.ao_fragments > 0 && xroar_cfg.ao_fragments <= 64)
		nfragments = xroar_cfg.ao_fragments;

	if (xroar_cfg.ao_fragment_ms > 0) {
		fragment_nframes = (rate * xroar_cfg.ao_fragment_ms) / 1000;
	} else if (xroar_cfg.ao_fragment_nframes > 0) {
		fragment_nframes = xroar_cfg.ao_fragment_nframes;
	} else {
		if (xroar_cfg.ao_buffer_ms > 0) {
			buffer_nframes = (rate * xroar_cfg.ao_buffer_ms) / 1000;
		} else if (xroar_cfg.ao_buffer_nframes > 0) {
			buffer_nframes = xroar_cfg.ao_buffer_nframes;
		} else {
			buffer_nframes = 1024;
		}
		fragment_nframes = buffer_nframes / nfragments;
	}

	desired.freq = rate;
	desired.format = AUDIO_S16;
	desired.channels = nchannels;
	desired.samples = fragment_nframes;
	desired.callback = (nfragments == 1) ? callback_1 : callback;
	desired.userdata = NULL;

	if (SDL_OpenAudio(&desired, &audiospec) < 0) {
		LOG_ERROR("Couldn't open audio: %s\n", SDL_GetError());
		SDL_QuitSubSystem(SDL_INIT_AUDIO);
		return 0;
	}
	rate = audiospec.freq;
	nchannels = audiospec.channels;
	fragment_nframes = audiospec.samples;

	switch (audiospec.format) {
		case AUDIO_U8: sample_fmt = SOUND_FMT_U8; sample_nbytes = 1; break;
		case AUDIO_S8: sample_fmt = SOUND_FMT_S8; sample_nbytes = 1; break;
		case AUDIO_S16LSB: sample_fmt = SOUND_FMT_S16_LE; sample_nbytes = 2; break;
		case AUDIO_S16MSB: sample_fmt = SOUND_FMT_S16_BE; sample_nbytes = 2; break;
		default:
			LOG_WARN("Unhandled audio format.");
			goto failed;
	}
	timeout_ms = (fragment_nframes * 1500) / rate;

	buffer_nframes = fragment_nframes * nfragments;
	fragment_nbytes = fragment_nframes * nchannels * sample_nbytes;

	fragment_mutex = SDL_CreateMutex();
	fragment_cv = SDL_CreateCond();

	shutting_down = 0;
	fragment_queue_length = 0;
	write_fragment = 0;
	play_fragment = 0;
	callback_buffer = NULL;

	// allocate fragment buffers
	fragment_buffer = g_malloc(nfragments * sizeof(void *));
	if (nfragments == 1) {
		fragment_buffer[0] = NULL;
	} else {
		for (unsigned i = 0; i < nfragments; i++) {
			fragment_buffer[i] = g_malloc0(fragment_nbytes);
		}
	}

	sound_init(fragment_buffer[0], sample_fmt, rate, nchannels, fragment_nframes);
	LOG_DEBUG(1, "\t%u frags * %d frames/frag = %d frames buffer (%.1fms)\n", nfragments, fragment_nframes, buffer_nframes, (float)(buffer_nframes * 1000) / rate);

	SDL_PauseAudio(0);
	return 1;

failed:
	SDL_CloseAudio();
	SDL_QuitSubSystem(SDL_INIT_AUDIO);
	return 0;
}

static void _shutdown(void) {
	shutting_down = 1;

	// no more audio
	SDL_PauseAudio(1);

	// unblock audio thread
	SDL_LockMutex(fragment_mutex);
	fragment_queue_length = 1;
	SDL_CondSignal(fragment_cv);
	SDL_UnlockMutex(fragment_mutex);

	SDL_CloseAudio();
	SDL_QuitSubSystem(SDL_INIT_AUDIO);

	SDL_DestroyCond(fragment_cv);
	SDL_DestroyMutex(fragment_mutex);

	if (nfragments > 1) {
		for (unsigned i = 0; i < nfragments; i++) {
			g_free(fragment_buffer[i]);
		}
	}

	g_free(fragment_buffer);
}

static void *write_buffer(void *buffer) {
	(void)buffer;

	SDL_LockMutex(fragment_mutex);

	/* For nfragments == 1, a non-NULL buffer means we've finished writing
	 * to the buffer provided by the callback.  Otherwise, one fragment
	 * buffer is now full.  Either way, signal the callback in case it is
	 * waiting for data to be available. */

	if (buffer) {
		write_fragment = (write_fragment + 1) % nfragments;
		fragment_queue_length++;
		SDL_CondSignal(fragment_cv);
	}

	if (xroar_noratelimit) {
		SDL_UnlockMutex(fragment_mutex);
		return NULL;
	}

	if (nfragments == 1) {
		// for nfragments == 1, wait for callback to send buffer
		while (callback_buffer == NULL) {
			if (SDL_CondWaitTimeout(fragment_cv, fragment_mutex, timeout_ms) == SDL_MUTEX_TIMEDOUT) {
				SDL_UnlockMutex(fragment_mutex);
				return NULL;
			}
		}
		fragment_buffer[0] = callback_buffer;
		callback_buffer = NULL;
	} else {
		// for nfragments > 1, wait until a fragment buffer is available
		while (fragment_queue_length == nfragments) {
			if (SDL_CondWaitTimeout(fragment_cv, fragment_mutex, timeout_ms) == SDL_MUTEX_TIMEDOUT) {
				SDL_UnlockMutex(fragment_mutex);
				return NULL;
			}
		}
	}

	SDL_UnlockMutex(fragment_mutex);
	return fragment_buffer[write_fragment];
}

static void callback(void *userdata, Uint8 *stream, int len) {
	(void)userdata;  /* unused */
	if (shutting_down)
		return;
	SDL_LockMutex(fragment_mutex);

	// wait until at least one fragment buffer is filled
	while (fragment_queue_length == 0)
		SDL_CondWait(fragment_cv, fragment_mutex);

	// copy it to callback buffer
	memcpy(stream, fragment_buffer[play_fragment], fragment_nbytes);
	play_fragment = (play_fragment + 1) % nfragments;

	// signal main thread that a fragment buffer is available
	fragment_queue_length--;
	SDL_CondSignal(fragment_cv);

	SDL_UnlockMutex(fragment_mutex);
}

static void callback_1(void *userdata, Uint8 *stream, int len) {
	(void)userdata;  /* unused */
	if (shutting_down)
		return;
	SDL_LockMutex(fragment_mutex);

	// pass callback buffer to main thread
	callback_buffer = stream;
	SDL_CondSignal(fragment_cv);

	// wait until main thread signals filled buffer
	while (fragment_queue_length == 0)
		SDL_CondWait(fragment_cv, fragment_mutex);

	// set to 0 so next callback will wait
	fragment_queue_length = 0;

	SDL_UnlockMutex(fragment_mutex);
}
