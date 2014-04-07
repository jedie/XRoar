/*  Copyright 2003-2014 Ciaran Anscomb
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pulse/error.h>
#include <pulse/gccmacro.h>
#include <pulse/simple.h>

#include "xalloc.h"

#include "logging.h"
#include "machine.h"
#include "module.h"
#include "sound.h"
#include "xroar.h"

static _Bool init(void);
static void shutdown(void);
static void *write_buffer(void *buffer);

SoundModule sound_pulse_module = {
	.common = { .name = "pulse", .description = "Pulse audio",
		    .init = init, .shutdown = shutdown },
	.write_buffer = write_buffer,
};

static pa_simple *pa;
static void *audio_buffer;

static size_t fragment_nbytes;

static _Bool init(void) {
	const char *device = xroar_cfg.ao_device;
	pa_sample_spec ss = {
		.format = PA_SAMPLE_S16NE,
	};
	pa_buffer_attr ba = {
		.maxlength = -1,
		.minreq = -1,
		.prebuf = -1,
	};
	int error;

	unsigned rate = (xroar_cfg.ao_rate > 0) ? xroar_cfg.ao_rate : 48000;
	unsigned nchannels = xroar_cfg.ao_channels;
	if (nchannels < 1 || nchannels > 2)
		nchannels = 2;
	ss.rate = rate;
	ss.channels = nchannels;

	enum sound_fmt request_fmt;
	unsigned sample_nbytes;
	if (ss.format == PA_SAMPLE_U8) {
		request_fmt = SOUND_FMT_U8;
		sample_nbytes = 1;
	} else if (ss.format & PA_SAMPLE_S16LE) {
		request_fmt = SOUND_FMT_S16_LE;
		sample_nbytes = 2;
	} else if (ss.format & PA_SAMPLE_S16BE) {
		request_fmt = SOUND_FMT_S16_BE;
		sample_nbytes = 2;
	} else {
		LOG_WARN("Unhandled audio format.");
		goto failed;
	}
	unsigned frame_nbytes = sample_nbytes * nchannels;

	/* PulseAudio abstracts a bit further, so fragments don't really come
	 * into it.  Use any specified value as "tlength". */

	int fragment_nframes;
	if (xroar_cfg.ao_fragment_ms > 0) {
		fragment_nframes = (rate * xroar_cfg.ao_fragment_ms) / 1000;
	} else if (xroar_cfg.ao_fragment_nframes > 0) {
		fragment_nframes = xroar_cfg.ao_fragment_nframes;
	} else if (xroar_cfg.ao_buffer_ms > 0) {
		fragment_nframes = (rate * xroar_cfg.ao_buffer_ms) / 1000;
	} else if (xroar_cfg.ao_buffer_nframes > 0) {
		fragment_nframes = xroar_cfg.ao_buffer_nframes;
	} else {
		fragment_nframes = 512;
	}
	ba.tlength = fragment_nframes * frame_nbytes;

	pa = pa_simple_new(NULL, "XRoar", PA_STREAM_PLAYBACK, device,
	                   "output", &ss, NULL, &ba, &error);
	if (!pa) {
		LOG_ERROR("Failed to initialise: %s\n", pa_strerror(error));
		goto failed;
	}

	fragment_nbytes = fragment_nframes * sample_nbytes * nchannels;
	audio_buffer = xmalloc(fragment_nbytes);
	sound_init(audio_buffer, request_fmt, rate, nchannels, fragment_nframes);
	LOG_DEBUG(1, "\t%dms (%d samples) buffer\n", (fragment_nframes * 1000) / rate, fragment_nframes);
	return 1;
failed:
	return 0;
}

static void shutdown(void) {
	int error;
	pa_simple_flush(pa, &error);
	pa_simple_free(pa);
	free(audio_buffer);
}

static void *write_buffer(void *buffer) {
	int error;
	if (xroar_noratelimit)
		return buffer;
	pa_simple_write(pa, buffer, fragment_nbytes, &error);
	return buffer;
}
