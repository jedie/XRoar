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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pulse/simple.h>
#include <pulse/error.h>
#include <pulse/gccmacro.h>

#include "types.h"
#include "logging.h"
#include "machine.h"
#include "module.h"
#include "sound.h"
#include "xroar.h"

static int init(void);
static void shutdown(void);
static void flush_frame(void);

SoundModule sound_pulse_module = {
	.common = { .name = "pulse", .description = "Pulse audio",
		    .init = init, .shutdown = shutdown },
	.flush_frame = flush_frame,
};

static unsigned int sample_rate;
static pa_simple *pa;

static uint8_t *buffer;
static int fragment_bytes;

static int init(void) {
	pa_sample_spec ss = {
		.format = PA_SAMPLE_U8,
		.channels = 1
	};
	pa_buffer_attr ba = {
		.maxlength = -1,
		.minreq = -1,
		.prebuf = -1,
	};
	int error;

	LOG_DEBUG(2,"Initialising PulseAudio driver\n");

	sample_rate = (xroar_opt_ao_rate > 0) ? xroar_opt_ao_rate : 44100;
	ss.rate = sample_rate;

	int fragment_size;
	if (xroar_opt_ao_buffer_ms > 0) {
		fragment_size = (sample_rate * xroar_opt_ao_buffer_ms) / 1000;
	} else if (xroar_opt_ao_buffer_samples > 0) {
		fragment_size = xroar_opt_ao_buffer_samples;
	} else {
		fragment_size = 512;
	}
	ba.tlength = fragment_size;
	fragment_bytes = fragment_size;

	pa = pa_simple_new(NULL, "XRoar", PA_STREAM_PLAYBACK, NULL, "playback",
	                   &ss, NULL, &ba, &error);
	if (!pa) {
		LOG_ERROR("Failed to initialise: %s\n", pa_strerror(error));
		goto failed;
	}

	int request_fmt;
	if (ss.format == PA_SAMPLE_U8) request_fmt = SOUND_FMT_U8;
	else if (ss.format & PA_SAMPLE_S16LE) request_fmt = SOUND_FMT_S16_LE;
	else if (ss.format & PA_SAMPLE_S16BE) request_fmt = SOUND_FMT_S16_BE;
	else {
		LOG_WARN("Unhandled audio format.");
		goto failed;
	}
	buffer = sound_init(sample_rate, 1, request_fmt, fragment_size);
	LOG_DEBUG(2, "\t%dms (%d samples) buffer\n", (fragment_size * 1000) / sample_rate, fragment_size);
	return 0;
failed:
	LOG_ERROR("Failed to initialise PulseAudio driver\n");
	return 1;
}

static void shutdown(void) {
	LOG_DEBUG(2, "Shutting down PulseAudio driver\n");
	int error;
	pa_simple_flush(pa, &error);
	pa_simple_free(pa);
}

static void flush_frame(void) {
	int error;
	if (xroar_noratelimit)
		return;
	pa_simple_write(pa, buffer, fragment_bytes, &error);
}
