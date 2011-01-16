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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pulse/simple.h>
#include <pulse/error.h>
#include <pulse/gccmacro.h>

#include "types.h"
#include "events.h"
#include "logging.h"
#include "machine.h"
#include "module.h"
#include "xroar.h"

static int init(void);
static void shutdown(void);
static void update(int value);

SoundModule sound_pulse_module = {
	{ "pulse", "Pulse audio",
	  init, 0, shutdown },
	update
};

typedef uint8_t Sample;  /* 8-bit mono */

#define FRAGMENTS 4
#define FRAME_SIZE 512
#define SAMPLE_CYCLES ((int)(OSCILLATOR_RATE / sample_rate))
#define FRAME_CYCLES (SAMPLE_CYCLES * FRAME_SIZE)

static unsigned int sample_rate;
static pa_simple *pa;

static Cycle frame_cycle_base;
static int frame_cycle;
static Sample *buffer;
static Sample *wrptr;
static Sample lastsample;

static void flush_frame(void);
static event_t *flush_event;

static int init(void) {
	pa_sample_spec ss = {
		.format = PA_SAMPLE_U8,
		.channels = 1
	};
	pa_buffer_attr ba = {
		.maxlength = FRAME_SIZE * FRAGMENTS,
		.minreq = -1,
		.prebuf = -1,
		.tlength = FRAME_SIZE
	};
	int error;

	LOG_DEBUG(2,"Initialising PulseAudio driver\n");

	sample_rate = (xroar_opt_ao_rate > 0) ? xroar_opt_ao_rate : 44100;
	ss.rate = sample_rate;
	pa = pa_simple_new(NULL, "XRoar", PA_STREAM_PLAYBACK, NULL, "playback",
	                   &ss, NULL, &ba, &error);
	if (!pa) {
		LOG_ERROR("Failed to initialise: %s\n", pa_strerror(error));
		goto failed;
	}

	/* TODO: Need to abstract this logging out */
	LOG_DEBUG(2, "\t");
	if (ss.format == PA_SAMPLE_U8) LOG_DEBUG(2, "8-bit unsigned, ");
	else if (ss.format & PA_SAMPLE_S16LE) LOG_DEBUG(2, "16-bit signed little-endian, ");
	else if (ss.format & PA_SAMPLE_S16BE) LOG_DEBUG(2, "16-bit signed big-endian, ");
	else LOG_DEBUG(2, "Unknown format, ");
	switch (ss.channels) {
		case 1: LOG_DEBUG(2, "mono, "); break;
		case 2: LOG_DEBUG(2, "stereo, "); break;
		default: LOG_DEBUG(2, "%d channel, ", ss.channels); break;
	}
	LOG_DEBUG(2, "%dHz\n", ss.rate);

	buffer = malloc(FRAME_SIZE * sizeof(Sample));
	flush_event = event_new();
	flush_event->dispatch = flush_frame;

	memset(buffer, 0x80, FRAME_SIZE * sizeof(Sample));
	wrptr = buffer;
	frame_cycle_base = current_cycle;
	frame_cycle = 0;
	flush_event->at_cycle = frame_cycle_base + FRAME_CYCLES;
	event_queue(&MACHINE_EVENT_LIST, flush_event);
	lastsample = 0x80;
	return 0;
failed:
	LOG_ERROR("Failed to initialise PulseAudio driver\n");
	return 1;
}

static void shutdown(void) {
	LOG_DEBUG(2, "Shutting down PulseAudio driver\n");
	int error;
	event_free(flush_event);
	pa_simple_flush(pa, &error);
	pa_simple_free(pa);
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
	int error;
	Sample *fill_to = buffer + FRAME_SIZE;
	while (wrptr < fill_to)
		*(wrptr++) = lastsample;
	frame_cycle_base += FRAME_CYCLES;
	frame_cycle = 0;
	flush_event->at_cycle = frame_cycle_base + FRAME_CYCLES;
	event_queue(&MACHINE_EVENT_LIST, flush_event);
	wrptr = buffer;
	if (xroar_noratelimit)
		return;
	pa_simple_write(pa, buffer, FRAME_SIZE, &error);
}
