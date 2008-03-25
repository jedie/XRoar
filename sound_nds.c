/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2008  Ciaran Anscomb
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

#include <stdlib.h>
#include <string.h>
#include <nds.h>

#include "types.h"
#include "events.h"
#include "logging.h"
#include "machine.h"
#include "mc6821.h"
#include "module.h"
#include "xroar.h"

static int init(int argc, char **argv);
static void shutdown(void);
static void update(void);

SoundModule sound_nds_module = {
	{ "nds", "NDS audio",
	  init, 0, shutdown, NULL },
	update
};

typedef uint8_t Sample;  /* 8-bit mono (SDL type) */

#define SAMPLE_RATE 22050
#define FRAME_SIZE 256
#define SAMPLE_CYCLES ((int)(OSCILLATOR_RATE / SAMPLE_RATE))
#define FRAME_CYCLES (SAMPLE_CYCLES * FRAME_SIZE)

static Cycle frame_cycle_base;
#define buffer ((uint8_t *)0x02800000 - FRAME_SIZE*sizeof(Sample))
static Sample *wrptr;
static Sample lastsample;

static void flush_frame(void *context);
static event_t *flush_event;

static int init(int argc, char **argv) {
	(void)argc;
	(void)argv;
	flush_event = event_new();
	flush_event->dispatch = flush_frame;

	memset(buffer, 0, FRAME_SIZE * sizeof(Sample));
	wrptr = buffer;
	frame_cycle_base = current_cycle;
	flush_event->at_cycle = frame_cycle_base + FRAME_CYCLES;
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
	if (elapsed_cycles >= FRAME_CYCLES) {
		fill_to = buffer + FRAME_SIZE;
	} else {
		fill_to = buffer + (elapsed_cycles/(Cycle)SAMPLE_CYCLES);
	}
	while (wrptr < fill_to)
		*(wrptr++) = lastsample;
	if (!(PIA1.b.control_register & 0x08)) {
		/* Single-bit sound */
		lastsample = (PIA1.b.port_output & 0x02) ? 0x7c : 0;
	} else  {
		if (PIA0.b.control_register & 0x08) {
			/* Sound disabled */
			lastsample = 0;
		} else {
			/* DAC output */
			lastsample = (PIA1.a.port_output & 0xfc) >> 1;
		}
	}
}

static void flush_frame(void *context) {
	Sample *fill_to = buffer + FRAME_SIZE;
	(void)context;
	while (wrptr < fill_to)
		*(wrptr++) = lastsample;
	frame_cycle_base += FRAME_CYCLES;
	flush_event->at_cycle = frame_cycle_base + FRAME_CYCLES;
	event_queue(&event_list, flush_event);
	wrptr = buffer;
	/* wait here */
}
