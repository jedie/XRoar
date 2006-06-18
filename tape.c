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

static int read_fd, write_fd;
static unsigned int motor;

static void waggle_bit(void);
static event_t *waggle_event;

/* For reading */
static int fake_leader;
static int bytes_remaining = 0;
static int bits_remaining = 0;
static uint8_t read_buf[512];
static uint8_t *read_buf_ptr;

/* For writing */
static int have_bytes = 0;
static int have_bits = 0;
static uint8_t write_buf[512];
static uint8_t *write_buf_ptr;
static unsigned int last_sample;
static Cycle last_read_time;

static int bit_in(void);
static int byte_in(void);
static void buffer_in(void);

static void bit_out(int);
static void byte_out(int);
static void buffer_out(void);

void tape_init(void) {
	read_fd = write_fd = -1;
	read_buf_ptr = read_buf;
	bits_remaining = bytes_remaining = 0;
	fake_leader = 0;
	waggle_event = event_new();
	waggle_event->dispatch = waggle_bit;
	write_buf_ptr = write_buf;
	have_bits = have_bytes = 0;
}

void tape_reset(void) {
	if (read_fd != -1) tape_close_reading();
	if (write_fd != -1) tape_close_writing();
	motor = 0;
	event_dequeue(waggle_event);
}

void tape_shutdown(void) {
	tape_reset();
}

int tape_open_reading(char *filename) {
	tape_close_reading();
	if ((read_fd = fs_open(filename, FS_READ)) == -1)
		return -1;
	bits_remaining = bytes_remaining = 0;
	return 0;
}

void tape_close_reading(void) {
	if (read_fd != -1)
		fs_close(read_fd);
	read_fd = -1;
}

int tape_open_writing(char *filename) {
	tape_close_writing();
	if ((write_fd = fs_open(filename, FS_WRITE)) == -1)
		return -1;
	have_bits = have_bytes = 0;
	return 0;
}

void tape_close_writing(void) {
	while (have_bits)
		bit_out(0);
	if (have_bytes)
		buffer_out();
	if (write_fd != -1)
		fs_close(write_fd);
	write_fd = -1;
}

/* Close any currently-open tape file, open a new one and read the first
 * bufferful of data.  Tries to guess the filetype.  Returns -1 on error,
 * 0 for a BASIC program, 1 for data and 2 for M/C. */
int tape_autorun(char *filename) {
	int state, type, count = 0;
	if (filename == NULL)
		return -1;
	if (tape_open_reading(filename) == -1)
		return -1;
	/* Little state machine to try and determine type of first
	 * file on tape */
	state = 0;
	type = 1;  /* default to data - no autorun (if trying to) */
	buffer_in();
	while (read_fd != -1 && state >= 0) {
		uint8_t b = *(read_buf_ptr++);
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
		bytes_remaining--;
		if (bytes_remaining < 1)
			buffer_in();
	}
	tape_close_reading();
	if (tape_open_reading(filename) == -1)
		return -1;
	/* If motor is on, enable the bit waggler */
	if (motor) {
		waggle_event->at_cycle = current_cycle + (OSCILLATOR_RATE / 2);
		event_queue(waggle_event);
	}
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
	return type;
}

/* Called whenever PIA_1A control register is written to.
 * Detects changes in motor status. */
void tape_update_motor(void) {
	if ((PIA_1A.control_register & 0x08) != motor) {
		motor = PIA_1A.control_register & 0x08;
		if (motor && read_fd != -1) {
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

/* Called whenever PIA_1A data register is written to.
 * Detects change frequency for writing out bits. */
void tape_update_output(void) {
	unsigned int new_sample;
	if (!motor)
		return;
	new_sample = PIA_1A.port_output & 0xfc;
	if (!last_sample && new_sample > 0xf4) {
		last_sample = 1;
		last_read_time = current_cycle;
		return;
	}
	if (last_sample && new_sample < 0x04) {
		int delta = current_cycle - last_read_time;
		last_sample = 0;
		if (delta < (OSCILLATOR_RATE/3600)) {
			bit_out(1);
		} else {
			bit_out(0);
		}
	}
}

static int bit_in(void) {
	static int cur_byte;
	int ret;
	if (bits_remaining == 0) {
		if ((cur_byte = byte_in()) == -1)
			return -1;
		bits_remaining = 8;
	}
	ret = cur_byte & 1;
	cur_byte >>= 1;
	bits_remaining--;
	return ret;
}

static int byte_in(void) {
	if (fake_leader) {
		fake_leader--;
		return 0x55;
	}
	if (bytes_remaining == 0) {
		buffer_in();
		if (bytes_remaining <= 0)
			return -1;
	}
	bytes_remaining--;
	return *(read_buf_ptr++);
}

static void buffer_in(void) {
	read_buf_ptr = read_buf;
	bytes_remaining = 0;
	if (read_fd != -1) {
		bytes_remaining = fs_read(read_fd, read_buf, sizeof(read_buf));
		if (bytes_remaining < sizeof(read_buf))
			tape_close_reading();
	}
}

static void waggle_bit(void) {
	static int cur_bit = 0;
	static int waggle_state = 0;
	switch (waggle_state) {
		default:
		case 0:
			if (!motor || (cur_bit = bit_in()) == -1) {
				event_dequeue(waggle_event);
				return;
			}
			PIA_1A.port_input |= 0x01;
			waggle_state = 1;
			break;
		case 1:
			PIA_1A.port_input &= 0xfe;
			waggle_state = 0;
			break;
	}
	/* Single cycles of 1200 baud for 0s, and 2400 baud for 1s */
	if (cur_bit == 0)
		waggle_event->at_cycle += (OSCILLATOR_RATE / 2400);
	else
		waggle_event->at_cycle += (OSCILLATOR_RATE / 4800);
	event_queue(waggle_event);
}

static void bit_out(int value) {
	static int cur_byte = 0;
	cur_byte >>= 1;
	if (value)
		cur_byte |= 0x80;
	have_bits++;
	if (have_bits < 8)
		return;
	byte_out(cur_byte);
	cur_byte = 0;
	have_bits = 0;
}

static void byte_out(int value) {
	*(write_buf_ptr++) = value;
	have_bytes++;
	if (have_bytes < sizeof(write_buf))
		return;
	buffer_out();
}

static void buffer_out(void) {
	if (write_fd != -1) {
		fs_write(write_fd, write_buf, have_bytes);
	}
	have_bytes = 0;
	write_buf_ptr = write_buf;
}
