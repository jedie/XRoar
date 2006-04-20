/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2006  Ciaran Anscomb
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

#include "types.h"
#include "events.h"
#include "fs.h"
#include "logging.h"
#include "pia.h"
#include "tape.h"
#include "xroar.h"

static FS_FILE tapefd;
static int isopen = 0;
static unsigned int motor;

static int waggle_state;
static void waggle_bit(void);
static event_t *waggle_event;

static uint8_t buf[512];
static uint8_t *buf_next;
static size_t buf_remain;
static unsigned int fake_leader;

static int tape_open(char *filename);
static void tape_close(void);
static void fetch_buffer(void);

void tape_init(void) {
	buf_next = buf;
	buf_remain = 0;
	fake_leader = 0;
	isopen = 0;
	waggle_event = event_new();
	waggle_event->dispatch = waggle_bit;
}

void tape_reset(void) {
	if (isopen) fs_close(tapefd);
	isopen = 0;
	motor = 0;
	event_dequeue(waggle_event);
}

void tape_detach(void) {
	tape_close();
}

/* Close any currently-open tape file, open a new one and read the first
 * bufferful of data.  Tries to guess the filetype.  Returns -1 on error,
 * 0 for a BASIC program, 1 for data and 2 for M/C. */
int tape_attach(char *filename) {
	int state, type, count = 0;
	if (filename == NULL)
		return -1;
	if (tape_open(filename) == -1)
		return -1;
	/* Little state machine to try and determine type of first
	 * file on tape */
	state = 0;
	type = 1;  /* default to data - no autorun (if trying to) */
	fetch_buffer();
	while (isopen && state >= 0) {
		uint8_t b = *(buf_next++);
		switch(state) {
			case 0:
				if (b != 0x55) state = -1;
				if (b == 0x3c) state = 1;
				break;
			case 1:
				state = 2;
				count = 10;
				if (b != 0x00) state = -1;
				break;
			case 2:
				count--;
				if (count == 0) {
					state = -1;
					type = b;
				}
				break;
			default:
				break;
		}
		buf_remain--;
		if (buf_remain < 1)
			fetch_buffer();
	}
	tape_close();
	if (tape_open(filename) == -1)
		return -1;
	/* If motor is on, enable the bit waggler */
	if (motor) {
		waggle_event->at_cycle = current_cycle + (OSCILLATOR_RATE / 2);
		event_queue(waggle_event);
	}
	return type;
}

int tape_autorun(char *filename) {
	int type = tape_attach(filename);
	if (type < 0)
		return type;
	switch (type) {
		/* BASIC programs don't autorun yet */ 
		case 0: keyboard_queue_string("CLOAD\r");
			return 0;
			break;
		case 2: keyboard_queue_string("CLOADM:EXEC\r");
			return 0;
			break;
		default:
			return 1;
			break;
	}
}

static int read_bit(void) {
	static uint8_t tape_byte = 0;
	static int count = 0;
	int ret;
	if (count == 0) {
		if (buf_remain == 0) {
			if (isopen) {
				fetch_buffer();
			}
		}
		if (buf_remain == 0)
			return -1;
		if (fake_leader) {
			tape_byte = 0x55;
			fake_leader--;
		} else {
			tape_byte = *(buf_next++);
			buf_remain--;
		}
		count = 8;
	}
	ret = tape_byte & 1;
	tape_byte >>= 1;
	count--;
	return ret;
}

static void waggle_bit(void) {
	static int cur_bit = 0;
	switch (waggle_state) {
		default:
		case 0:
			cur_bit = read_bit();
			PIA_1A.port_input |= 0x01;
			waggle_state = 1;
			break;
		case 1:
			PIA_1A.port_input &= 0xfe;
			waggle_state = 0;
			break;
	}
	/* Single cycles of 1200 baud for 0s, and 2400 baud for 1s */
	if (cur_bit >= 0) {
		if (cur_bit == 0)
			waggle_event->at_cycle += (OSCILLATOR_RATE / 2400);
		else
			waggle_event->at_cycle += (OSCILLATOR_RATE / 4800);
		event_queue(waggle_event);
	} else {
		event_dequeue(waggle_event);
	}
}

/* Called whenever PIA_1A is written to.  Detect changed is motor status. */
void tape_update(void) {
	if ((PIA_1A.control_register & 0x08) != motor) {
		motor = PIA_1A.control_register & 0x08;
		if (motor && isopen) {
			/* If motor turned on and tape file attached, enable
			 * the tape input bit waggler */
			fake_leader = 64;  /* Insert some faked leader bytes */
			waggle_event->at_cycle = current_cycle + (OSCILLATOR_RATE / 2);
			event_queue(waggle_event);
		} else {
			event_dequeue(waggle_event);
		}
	}
}

static int tape_open(char *filename) {
	if (isopen)
		tape_close();
	if ((tapefd = fs_open(filename, FS_READ)) == -1)
		return -1;
	buf_remain = 0;
	isopen = 1;
	return 0;
}

static void tape_close(void) {
	if (isopen)
		fs_close(tapefd);
	isopen = 0;
}

static void fetch_buffer(void) {
	buf_next = buf;
	buf_remain = sizeof(buf);
	if (isopen) {
		buf_remain = fs_read(tapefd, buf, sizeof(buf));
		if (buf_remain < sizeof(buf))
			tape_close();
	} else {
		memset(buf, 0, sizeof(buf));
	}
}
