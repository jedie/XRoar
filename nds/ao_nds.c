/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2012  Ciaran Anscomb
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA  02110-1301, USA.
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <nds.h>

#include "types.h"

#include "events.h"
#include "logging.h"
#include "machine.h"
#include "module.h"
#include "sound.h"

static int init(void);
static void flush_frame(void *buffer);

SoundModule sound_nds_module = {
	.common = { .init = init },
	.flush_frame = flush_frame,
};

/* 32728 here as that's what it *actually* is */
#define SAMPLE_RATE 32728
#define FRAME_SIZE 256

static uint8_t *buf[2];

static int init(void) {
	buf[0] = sound_init_2(SAMPLE_RATE, 1, SOUND_FMT_U8, FRAME_SIZE);
	buf[1] = buf[0] + FRAME_SIZE;

	/* handshake with ARM7 to pass sound buffer address */
	REG_IPC_FIFO_CR = (1 << 3) | (1 << 15);  /* enable FIFO, clear sendq */
	REG_IPC_SYNC = (14 << 8);
	while ((REG_IPC_SYNC & 15) != 14);
	REG_IPC_FIFO_TX = (uint32_t)buf[0];
	REG_IPC_SYNC = (1 << 14) | (0 << 8);  /* IRQ on sync */
	irqEnable(IRQ_IPC_SYNC);

	/* now wait for ARM7 to be playing frame 1 */
	while ((REG_IPC_SYNC & 1) != 1) {
		swiIntrWait(1, IRQ_IPC_SYNC);
	}
	return 0;
}

static void flush_frame(void *buffer) {
	DC_FlushRange(buffer, FRAME_SIZE);
	int writing_buf = (buffer == buf[1]);
	/* wait here */
	if ((REG_IPC_SYNC & 1) == writing_buf) {
		swiIntrWait(1, IRQ_IPC_SYNC);
	}
}
