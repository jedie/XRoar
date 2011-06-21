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

#include <stdlib.h>
#include <string.h>
#include <nds.h>

#include "types.h"
#include "events.h"
#include "logging.h"
#include "machine.h"
#include "module.h"
#include "xroar.h"

static int init(void);
static void shutdown(void);
static void update(int value);

SoundModule sound_nds_module = {
	{ "nds", "NDS audio",
	  init, 0, shutdown },
	update
};

/* 32728 here as that's what it *actually* is */
#define SAMPLE_RATE 32728
#define FRAME_SIZE 256
#define SAMPLE_CYCLES ((int)(OSCILLATOR_RATE / SAMPLE_RATE))
#define FRAME_CYCLES (SAMPLE_CYCLES * FRAME_SIZE)

static Cycle frame_cycle_base;
static int frame_cycle;
static uint8_t buf[FRAME_SIZE * 2] __attribute__ ((aligned (32)));
static uint8_t *frame_base;
static uint8_t *wrptr;
static unsigned int lastsample;
static int writing_buf;

static void flush_frame(void);
static event_t *flush_event;

static int init(void) {
	LOG_DEBUG(2,"Initialising NDS audio (ARM9 side)\n");
	flush_event = event_new();
	flush_event->dispatch = flush_frame;

	memset(buf, 0x80, sizeof(buf));
	writing_buf = 0;
	frame_base = buf;
	wrptr = buf;

	frame_cycle_base = current_cycle;
	frame_cycle = 0;
	flush_event->at_cycle = frame_cycle_base + FRAME_CYCLES;
	event_queue(&MACHINE_EVENT_LIST, flush_event);
	lastsample = 0x80; //0;

	/* handshake with ARM7 to pass sound buffer address */
	REG_IPC_FIFO_CR = (1 << 3) | (1 << 15);  /* enable FIFO, clear sendq */
	REG_IPC_SYNC = (14 << 8);
	while ((REG_IPC_SYNC & 15) != 14);
	REG_IPC_FIFO_TX = (uint32_t)buf;
	REG_IPC_SYNC = (1 << 14) | (0 << 8);  /* IRQ on sync */
	irqEnable(IRQ_IPC_SYNC);
	
	/* now wait for ARM7 to be playing frame 1 */
	while ((REG_IPC_SYNC & 1) != 1) {
		swiIntrWait(1, IRQ_IPC_SYNC);
	}
	return 0;
}

static void shutdown(void) {
	event_free(flush_event);
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
	uint8_t *fill_to = frame_base + FRAME_SIZE;
	while (wrptr < fill_to)
		*(wrptr++) = lastsample;
	DC_FlushRange(frame_base, FRAME_SIZE);
	frame_cycle_base += FRAME_CYCLES;
	frame_cycle = 0;
	flush_event->at_cycle = frame_cycle_base + FRAME_CYCLES;
	event_queue(&MACHINE_EVENT_LIST, flush_event);
	writing_buf ^= 1;
	frame_base = buf + (writing_buf * FRAME_SIZE);
	wrptr = frame_base;
	/* wait here */
	if ((REG_IPC_SYNC & 1) == writing_buf) {
		swiIntrWait(1, IRQ_IPC_SYNC);
	}
}
