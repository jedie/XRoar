/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2010  Ciaran Anscomb
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

#include "config.h"

#include <string.h>
#ifdef HAVE_SNDFILE
#include <stdlib.h>
#include <sndfile.h>
#endif

#include "types.h"
#include "events.h"
#include "fs.h"
#include "keyboard.h"
#include "logging.h"
#include "machine.h"
#include "mc6821.h"
#include "tape.h"
#include "xroar.h"

static int read_fd, write_fd;
static unsigned int motor;

static void waggle_bit(void);
static event_t waggle_event;

/* For reading */
static int input_type;
static int is_ascii;
static int fake_leader;
static int block_bytes = 0;
static int bits_remaining = 0;
static int current_byte;
static enum {
       FIRST_BLOCK,
       DATA_BLOCK,
       LAST_BLOCK,
} current_block = FIRST_BLOCK;
static uint8_t block[261];
/* WAV input support */
#ifdef HAVE_SNDFILE
static SNDFILE *wav_read_file;
static int wav_samples_remaining;
#define SF_BUF_LENGTH (512)
static short *wav_read_buf = NULL;
static short *wav_read_ptr;
static Cycle wav_last_sample_cycle;
static int wav_sample_rate;
static int wav_channels;
#define SAMPLE_CYCLES ((Cycle)(OSCILLATOR_RATE / wav_sample_rate))
#endif

/* For writing */
static int have_bytes = 0;
static int have_bits = 0;
static uint8_t write_buf[512];
static uint8_t *write_buf_ptr;
static unsigned int last_sample;
static Cycle last_read_time;

/* TAPEHACK: */
static int have_sync = 0;

static int bit_in(void);
static int byte_in(void);
static int block_in(void);

static void bit_out(int);
static void byte_out(int);
static void buffer_out(void);

#ifdef HAVE_SNDFILE
static void wav_buffer_in(void);
static short wav_sample_in(void);
#endif

void tape_init(void) {
#ifdef HAVE_SNDFILE
	wav_read_file = NULL;
#endif
	read_fd = write_fd = -1;
	bits_remaining = block_bytes = 0;
	fake_leader = 0;
	event_init(&waggle_event);
	waggle_event.dispatch = waggle_bit;
	write_buf_ptr = write_buf;
	have_bits = have_bytes = 0;
}

void tape_reset(void) {
	tape_close_writing();
	motor = 0;
	event_dequeue(&waggle_event);
}

void tape_shutdown(void) {
	tape_reset();
}

int tape_open_reading(const char *filename) {
#ifdef HAVE_SNDFILE
	SF_INFO info;
#endif
	tape_close_reading();
	input_type = xroar_filetype_by_ext(filename);
	is_ascii = (input_type == FILETYPE_ASC);
	switch (input_type) {
	case FILETYPE_CAS:
	case FILETYPE_ASC:
		if ((read_fd = fs_open(filename, FS_READ)) == -1) {
			LOG_WARN("Failed to open '%s'\n", filename);
			return -1;
		}
		bits_remaining = block_bytes = 0;
		/* If motor is on, enable the bit waggler */
		if (motor) {
			fake_leader = 64;
			waggle_event.at_cycle = current_cycle + (OSCILLATOR_RATE / 2);
			event_queue(&MACHINE_EVENT_LIST, &waggle_event);
		}
		LOG_DEBUG(2,"Attached virtual cassette%s %s\n", is_ascii ? " (ASCII)" : "", filename);
		break;
#ifdef HAVE_SNDFILE
	default:
		info.format = 0;
		wav_read_file = sf_open(filename, SFM_READ, &info);
		if (wav_read_file == NULL) {
			LOG_WARN("Failed to open '%s'\n", filename);
			return -1;
		}
		wav_channels = info.channels;
		wav_sample_rate = info.samplerate;
		if (wav_read_buf != NULL) {
			free(wav_read_buf);
			wav_read_buf = NULL;
		}
		if (wav_sample_rate == 0) {
			tape_close_reading();
			return -1;
		}
		wav_read_buf = malloc(SF_BUF_LENGTH * sizeof(short) * wav_channels);
		wav_samples_remaining = 0;
		LOG_DEBUG(2,"Attached audio file %s\n\t%dHz, %d channel%s\n", filename, wav_sample_rate, wav_channels, (wav_channels==1)?"":"s");
		break;
#endif
	}
	return 0;
}

void tape_close_reading(void) {
	if (read_fd != -1)
		fs_close(read_fd);
#ifdef HAVE_SNDFILE
	if (wav_read_file) {
		sf_close(wav_read_file);
		wav_read_file = NULL;
	}
#endif
	read_fd = -1;
}

int tape_open_writing(const char *filename) {
	tape_close_writing();
	if ((write_fd = fs_open(filename, FS_WRITE)) == -1)
		return -1;
	have_bits = have_bytes = 0;
	return 0;
}

void tape_close_writing(void) {
	while (have_bits)
		bit_out(0);
	if (xroar_tapehack) {
		byte_out(0x55);
		byte_out(0x55);
	}
	if (have_bytes)
		buffer_out();
	if (write_fd != -1)
		fs_close(write_fd);
	write_fd = -1;
}

/* Close any currently-open tape file, open a new one and read the first
 * bufferful of data.  Tries to guess the filetype.  Returns -1 on error,
 * 0 for a BASIC program, 1 for data and 2 for M/C. */
int tape_autorun(const char *filename) {
	int state, type, count = 0;
	if (filename == NULL)
		return -1;
	if (tape_open_reading(filename) == -1)
		return -1;
	/* Little state machine to try and determine type of first
	 * file on tape */
	state = 0;
	type = 1;  /* default to data - no autorun (if trying to) */
	/* ASCII files are always BASIC */
	if (is_ascii) {
		type = 0;    /* BASIC */
		state = -1;  /* skip state machine */
	}
	while ((block_bytes > 0 || read_fd != -1) && state >= 0) {
		uint8_t b = byte_in();
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
	}
	tape_close_reading();
	if (tape_open_reading(filename) == -1)
		return -1;
	switch (type) {
		/* BASIC programs don't autorun yet */
		case 0: keyboard_queue_string("CLOAD\r");
			break;
		case 2: keyboard_queue_string("CLOADM:EXEC\r");
			break;
		default:
			break;
	}
	return type;
}

/* Called whenever PIA1.a control register is written to.
 * Detects changes in motor status. */
void tape_update_motor(void) {
	if ((PIA1.a.control_register & 0x08) != motor) {
		motor = PIA1.a.control_register & 0x08;
		if (motor) {
			switch (input_type) {
			case FILETYPE_CAS:
			case FILETYPE_ASC:
				if (block_bytes > 0 || read_fd != -1) {
					/* If motor turned on and tape file
					 * attached, enable the tape input bit
					 * waggler */
					fake_leader = 64;
					waggle_event.at_cycle = current_cycle + (OSCILLATOR_RATE / 2);
					event_queue(&MACHINE_EVENT_LIST, &waggle_event);
				}
				break;
			default:
#ifdef HAVE_SNDFILE
				wav_last_sample_cycle = current_cycle;
#endif
				break;
			}
		} else {
			event_dequeue(&waggle_event);
			if (xroar_tapehack) {
				tape_desync(256);
			}
		}
	}
}

/* Called whenever PIA1.a data register is written to.
 * Detects change frequency for writing out bits. */
void tape_update_output(void) {
	unsigned int new_sample;
	if (!motor)
		return;
	new_sample = PIA1.a.port_output & 0xfc;
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

#ifdef HAVE_SNDFILE
void tape_update_input(void) {
	short sample;
	if (!motor || input_type == FILETYPE_CAS || input_type == FILETYPE_ASC)
		return;
	sample = wav_sample_in();
	if (sample >= 0)
		PIA1.a.port_input |= 0x01;
	else
		PIA1.a.port_input &= ~0x01;
}
#endif

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
	if (read_fd == -1)
		return -1;
	if (current_byte >= block_bytes) {
		if (block_in() == -1)
			return -1;
		current_byte = 0;
	}
	return block[current_byte++];
}

static int block_in(void) {
	/* Normal binary mode: just read a block of data */
	if (!is_ascii) {
		block_bytes = fs_read(read_fd, block, sizeof(block));
		if (block_bytes < 0)
			return -1;
		return 0;
	}
	/* ASCII mode: construct a cassette block */
	block[0] = 0x55;
	block[1] = 0x3c;
	int bsize = 0;
	switch (current_block) {
	case FIRST_BLOCK:
		/* Header block */
		block[2] = 0x00;
		memcpy(&block[4], "BASIC   \000\377\377\000\000\000\000", 15);
		bsize = 15;
		current_block = DATA_BLOCK;
		break;
	case DATA_BLOCK:
		/* ASCII data block - read up to 255 bytes */
		block[2] = 0x01;
		bsize = fs_read(read_fd, &block[4], 255);
		if (bsize <= 0) {
			/* EOF block */
			block[2] = 0xff;
			bsize = 0;
			current_block = LAST_BLOCK;
		}
		break;
	case LAST_BLOCK:
		return -1;
	}
	/* Do line-ending conversion and calculate checksum */
	int sum = block[2], src = 0, dest = 0, cr = 0;
	for ( ; src < bsize; src++) {
		uint8_t sbyte = block[4+src];
		if (sbyte == 0x0d || sbyte == 0x0a) {
			if (!cr) {
				block[4+dest] = 0x0d;
				sum += 0x0d;
				dest++;
				cr = 1;
			}
		} else {
			block[4+dest] = sbyte;
			sum += sbyte;
			dest++;
			cr = 0;
		}
	}
	bsize = dest;
	block[3] = bsize;
	sum += bsize;
	block[bsize+4] = sum & 0xff;
	block[bsize+5] = 0x55;
	block_bytes = bsize+5;
	return 0;
}

static void waggle_bit(void) {
	static int cur_bit = 0;
	static int waggle_state = 0;
	switch (waggle_state) {
		default:
		case 0:
			PIA1.a.port_input |= 0x01;
			waggle_state = 1;
			if (!motor || (cur_bit = bit_in()) == -1) {
				event_dequeue(&waggle_event);
				return;
			}
			break;
		case 1:
			PIA1.a.port_input &= 0xfe;
			waggle_state = 0;
			break;
	}
	/* Single cycles of 1200 baud for 0s, and 2400 baud for 1s */
	if (cur_bit == 0)
		waggle_event.at_cycle += (OSCILLATOR_RATE / 2400);
	else
		waggle_event.at_cycle += (OSCILLATOR_RATE / 4800);
	event_queue(&MACHINE_EVENT_LIST, &waggle_event);
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
	if (have_bytes < (int)sizeof(write_buf))
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

#ifdef HAVE_SNDFILE
static void wav_buffer_in(void) {
	wav_read_ptr = wav_read_buf;
	wav_samples_remaining = 0;
	if (wav_read_file == NULL || wav_read_buf == NULL)
		return;
	wav_samples_remaining = sf_read_short(wav_read_file, wav_read_buf, SF_BUF_LENGTH);
	if (wav_samples_remaining < SF_BUF_LENGTH)
		tape_close_reading();
}

static short wav_sample_in(void) {
	Cycle elapsed_cycles;
	if (wav_sample_rate == 0)
		return 0;
	elapsed_cycles = current_cycle - wav_last_sample_cycle;
	while (elapsed_cycles >= SAMPLE_CYCLES) {
		wav_read_ptr++;
		if (wav_samples_remaining == 0) {
			wav_buffer_in();
			if (wav_samples_remaining <= 0)
				return 0;
		}
		wav_last_sample_cycle += SAMPLE_CYCLES;
		wav_samples_remaining--;
		elapsed_cycles -= SAMPLE_CYCLES;
	}
	if (wav_read_ptr == NULL)
		return 0;
	return *wav_read_ptr;
}
#endif

/* Tapehack-specific stuff: */

static int leader_count = 256;

void tape_sync(void) {
	int i;
	if (have_sync) return;
	for (i = 0; i < leader_count; i++)
		byte_out(0x55);
	byte_out(0x3c);
	have_sync = 1;
}

void tape_desync(int leader) {
	while (have_bits)
		bit_out(0);
	fake_leader = leader;
	have_sync = 0;
	leader_count = leader;
}

void tape_bit_out(int value) {
	if (have_sync) {
		bit_out(value);
	}
}
