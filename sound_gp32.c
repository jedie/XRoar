/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2007  Ciaran Anscomb
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
#include "pia.h"
#include "sound_gp32.h"
#include "xroar.h"
#include "gp32/gpsound.h"
#include "gp32/gp32.h"

static int init(int argc, char **argv);
static void shutdown(void);
static void update(void);

SoundModule sound_gp32_module = {
	{ "gp32", "GP32 audio",
	  init, 0, shutdown, NULL },
	update
};

typedef uint16_t Sample;  /* 8-bit stereo */

static uint_least32_t sample_rate;
static Cycle sample_cycles;
static int_least32_t frame_size;
static uint_least32_t frame_cycles;

static Cycle frame_cycle_base;
static unsigned int writing_frame;
static Sample **buffer;
static Sample *wrptr;
static Sample lastsample;

static void flush_frame(void *context);
static event_t *flush_event;

static int init(int argc, char **argv) {
	(void)argc;
	(void)argv;
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
	flush_event->at_cycle = frame_cycle_base + frame_cycles;
	event_queue(&event_list, flush_event);
	lastsample = 0;
	return 0;
}

static void shutdown(void) {
	event_free(flush_event);
}

static void update(void) {
	Cycle elapsed_cycles = current_cycle - frame_cycle_base;
	Sample *fill_to;
	if (elapsed_cycles >= frame_cycles) {
		fill_to = buffer[writing_frame] + frame_size;
	} else {
		fill_to = buffer[writing_frame] + (elapsed_cycles/(Cycle)sample_cycles);
	}
	while (wrptr < fill_to)
		*(wrptr++) = lastsample;
	if (!(PIA_1B.control_register & 0x08)) {
		/* Single-bit sound */
		lastsample = (PIA_1B.port_output & 0x02) ? 0x7c7c : 0;
	} else  {
		if (PIA_0B.control_register & 0x08) {
			/* Sound disabled */
			lastsample = 0;
		} else {
			/* DAC output */
			lastsample = (PIA_1A.port_output & 0xfc) >> 1;
			lastsample |= (lastsample << 8);
		}
	}
}

static void flush_frame(void *context) {
	Sample *fill_to = buffer[writing_frame] + frame_size;
	(void)context;
	while (wrptr < fill_to)
		*(wrptr++) = lastsample;
	frame_cycle_base += frame_cycles;
	flush_event->at_cycle = frame_cycle_base + frame_cycles;
	event_queue(&event_list, flush_event);
	writing_frame ^= 1;
	wrptr = buffer[writing_frame];
	if (noratelimit)
		return;
	while ((rDCSRC2 >= (unsigned)buffer[1]) == writing_frame);
}

void sound_silence(void) {
	memset(buffer[0], 0, frame_size * sizeof(Sample));
	memset(buffer[1], 0, frame_size * sizeof(Sample));
}
