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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pulse/error.h>
#include <pulse/gccmacro.h>
#include <pulse/simple.h>

#include "pl_glib.h"

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

static unsigned int sample_rate;
static unsigned int nchannels;
static pa_simple *pa;
static void *audio_buffer;

static int fragment_bytes;

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

	sample_rate = (xroar_cfg.ao_rate > 0) ? xroar_cfg.ao_rate : 48000;
	nchannels = xroar_cfg.ao_channels;
	if (nchannels < 1 || nchannels > 2)
		nchannels = 2;
	ss.rate = sample_rate;
	ss.channels = nchannels;

	int fragment_size;
	if (xroar_cfg.ao_buffer_ms > 0) {
		fragment_size = (sample_rate * xroar_cfg.ao_buffer_ms) / 1000;
	} else if (xroar_cfg.ao_buffer_samples > 0) {
		fragment_size = xroar_cfg.ao_buffer_samples;
	} else {
		fragment_size = 512;
	}
	ba.tlength = fragment_size;
	fragment_bytes = fragment_size;

	pa = pa_simple_new(NULL, "XRoar", PA_STREAM_PLAYBACK, device,
	                   "output", &ss, NULL, &ba, &error);
	if (!pa) {
		LOG_ERROR("Failed to initialise: %s\n", pa_strerror(error));
		goto failed;
	}

	enum sound_fmt request_fmt;
	unsigned sample_size;
	if (ss.format == PA_SAMPLE_U8) {
		request_fmt = SOUND_FMT_U8;
		sample_size = 1;
	} else if (ss.format & PA_SAMPLE_S16LE) {
		request_fmt = SOUND_FMT_S16_LE;
		sample_size = 2;
	} else if (ss.format & PA_SAMPLE_S16BE) {
		request_fmt = SOUND_FMT_S16_BE;
		sample_size = 2;
	} else {
		LOG_WARN("Unhandled audio format.");
		goto failed;
	}
	unsigned buffer_size = fragment_size * sample_size * nchannels;
	audio_buffer = g_malloc(buffer_size);
	sound_init(audio_buffer, request_fmt, sample_rate, nchannels, fragment_size);
	LOG_DEBUG(2, "\t%dms (%d samples) buffer\n", (fragment_size * 1000) / sample_rate, fragment_size);
	return 1;
failed:
	return 0;
}

static void shutdown(void) {
	int error;
	pa_simple_flush(pa, &error);
	pa_simple_free(pa);
	g_free(audio_buffer);
}

static void *write_buffer(void *buffer) {
	int error;
	if (xroar_noratelimit)
		return buffer;
	pa_simple_write(pa, buffer, fragment_bytes, &error);
	return buffer;
}
