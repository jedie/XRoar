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

#include <string.h>
#include <gpdef.h>
#include <gpstdlib.h>
#include <gpgraphic.h>
#include <gpstdio.h>
#include <gpfont.h>

#include "types.h"
#include "events.h"
#include "machine.h"
#include "module.h"
#include "xroar.h"
#include "gp32/ao_gp32.h"
#include "gp32/gpsound.h"
#include "gp32/gp32.h"

static int init(void);
static void shutdown(void);
static void update(int value);

SoundModule sound_gp32_module = {
	{ "gp32", "GP32 audio",
	  init, 0, shutdown },
	update
};

typedef uint16_t Sample;  /* 8-bit stereo */

static uint_least32_t sample_rate;
static Cycle sample_cycles;
static int_least32_t frame_size;
static uint_least32_t frame_cycles;

static Cycle frame_cycle_base;
static int frame_cycle;
static unsigned int writing_frame;
static Sample **buffer;
static Sample *wrptr;
static Sample lastsample;

static void flush_frame(void);
static event_t *flush_event;

static int init(void) {
	sample_rate = 22050;
	gpsound_init(PCLK, &sample_rate);
	sample_cycles = OSCILLATOR_RATE / sample_rate;
	//frame_size = CYCLES_PER_FRAME / sample_cycles;
	frame_size = 512;
	frame_cycles = sample_cycles * frame_size;
	buffer = gpsound_buffers(frame_size);
	gpsound_start();
	flush_event = event_new();
	flush_event->dispatch = flush_frame;

	memset(buffer[0], 0, frame_size * sizeof(Sample));
	memset(buffer[1], 0, frame_size * sizeof(Sample));
	wrptr = buffer[1];
	writing_frame = 1;
	frame_cycle_base = current_cycle;
	frame_cycle = 0;
	flush_event->at_cycle = frame_cycle_base + frame_cycles;
	event_queue(&MACHINE_EVENT_LIST, flush_event);
	lastsample = 0;
	return 0;
}

static void shutdown(void) {
	event_free(flush_event);
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
	lastsample = (value << 8) | value;
}

static void flush_frame(void) {
	Sample *fill_to = buffer[writing_frame] + frame_size;
	while (wrptr < fill_to)
		*(wrptr++) = lastsample;
	frame_cycle_base += frame_cycles;
	frame_cycle = 0;
	flush_event->at_cycle = frame_cycle_base + frame_cycles;
	event_queue(&MACHINE_EVENT_LIST, flush_event);
	writing_frame ^= 1;
	wrptr = buffer[writing_frame];
	if (xroar_noratelimit)
		return;
	while ((rDCSRC2 >= (unsigned)buffer[1]) == writing_frame);
}

void sound_silence(void) {
	memset(buffer[0], 0, frame_size * sizeof(Sample));
	memset(buffer[1], 0, frame_size * sizeof(Sample));
}
