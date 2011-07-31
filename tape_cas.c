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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA  02110-1301, USA.
 */

#include <stdlib.h>
#include <stdio.h>

#include "fs.h"
#include "machine.h"
#include "misc.h"
#include "tape.h"

struct tape_cas {
	int fd;
	/* input-specific: */
	int byte;  /* current input byte */
	int bit;   /* current input bit */
	int bit_index;  /* 0-7, next bit to be read */
	int pulse_index;  /* 0-1, next pulse within bit */
	/* ascii input specific: */
	int is_ascii;
	int ascii_blocks;
	int ascii_last_block_size;
	int ascii_sum;
	/* output-specific: */
	int output_sense;  /* 0 = negative, 1 = positive, -1 = not known */
	int pulse_length;  /* current pulse */
	int last_pulse_length;  /* previous pulse */
	int leader_length;  /* cycles part of leader length */
	int leader_bits;  /* bits part of leader length */
	int output_byte;  /* current output byte */
	int output_bit_count;  /* 0-7, next bit to be written */
};

static void cas_close(struct tape *t);
static long cas_tell(struct tape *t);
static int cas_seek(struct tape *t, long offset, int whence);
static int cas_to_ms(struct tape *t, long pos);
static long cas_ms_to(struct tape *t, int ms);
static int cas_pulse_in(struct tape *t, int *pulse_width);
static int cas_sample_out(struct tape *t, uint8_t sample, int length);
static void cas_motor_off(struct tape *t);

struct tape_module tape_cas_module = {
	.close = cas_close, .tell = cas_tell, .seek = cas_seek,
	.to_ms = cas_to_ms, .ms_to = cas_ms_to,
	.pulse_in = cas_pulse_in, .sample_out = cas_sample_out,
	.motor_off = cas_motor_off,
};

static int read_byte(struct tape *t);
static void bit_out(struct tape_cas *cas, int bit);

#define ASCII_LEADER (192)
#define NAMEBLOCK_LENGTH (21)
#define EOFBLOCK_LENGTH (6)

static uint8_t ascii_name_block[NAMEBLOCK_LENGTH] = {
	0x55, 0x3c, 0x00, 0x0f,
	0x42, 0x41, 0x53, 0x49, 0x43, 0x20, 0x20, 0x20,  /* "BASIC   " */
	0x00, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00,
	0xcf, 0x55
};

static uint8_t ascii_eof_block[EOFBLOCK_LENGTH] = {
	0x55, 0x3c, 0xff, 0x00, 0xff, 0x55
};

static struct tape *do_tape_cas_open(const char *filename, int mode,
                                     int is_ascii) {
	struct tape *t;
	struct tape_cas *cas;
	int fd;
	fd = fs_open(filename, mode);
	if (fd == -1) return NULL;
	t = tape_new();
	t->module = &tape_cas_module;
	cas = xmalloc(sizeof(struct tape_cas));
	t->data = cas;
	/* initialise cas */
	cas->fd = fd;
	cas->is_ascii = is_ascii;
	cas->output_sense = -1;
	cas->pulse_length = cas->last_pulse_length = 0;
	cas->leader_length = cas->leader_bits = 0;
	cas->output_byte = cas->output_bit_count = 0;
	cas->bit_index = 0;  /* next read will fetch a new byte */
	cas->pulse_index = 0;
	/* find size */
	long size = fs_lseek(cas->fd, 0, FS_SEEK_END);
	if (is_ascii) {
		cas->ascii_blocks = size / 255;
		cas->ascii_last_block_size = size % 255;
		cas->ascii_sum = 0;
		size = ASCII_LEADER + NAMEBLOCK_LENGTH + cas->ascii_blocks * (ASCII_LEADER + 261) + ASCII_LEADER + EOFBLOCK_LENGTH;
		if (cas->ascii_last_block_size > 0) {
			size += ASCII_LEADER + cas->ascii_last_block_size + 6;
		}
	}
	if (size >= 0) {
		/* 3 bits offset into byte, 1 bit pulse index */
		t->size = size << 4;
	}
	/* rewind to start */
	fs_lseek(cas->fd, 0, FS_SEEK_SET);
	t->offset = 0;
	return t;
}

struct tape *tape_cas_open(const char *filename, int mode) {
	return do_tape_cas_open(filename, mode, 0);
}

struct tape *tape_asc_open(const char *filename, int mode) {
	return do_tape_cas_open(filename, mode, 1);
}

static void cas_close(struct tape *t) {
	struct tape_cas *cas = t->data;
	cas_motor_off(t);
	fs_close(cas->fd);
	free(cas);
	tape_free(t);
}

static int read_byte(struct tape *t) {
	struct tape_cas *cas = t->data;
	if (!cas->is_ascii) return fs_read_uint8(cas->fd);
	long byte_offset = t->offset >> 4;
	if (byte_offset < ASCII_LEADER)
		return 0x55;
	byte_offset -= ASCII_LEADER;
	if (byte_offset < NAMEBLOCK_LENGTH)
		return ascii_name_block[byte_offset];
	byte_offset -= NAMEBLOCK_LENGTH;
	byte_offset -= ASCII_LEADER;
	int block = byte_offset / (ASCII_LEADER + 261);
	int block_size = 255;
	if (block >= cas->ascii_blocks) {
		block = cas->ascii_blocks;
		block_size = cas->ascii_last_block_size;
	}
	byte_offset -= block * (ASCII_LEADER + 261);
	if (block == cas->ascii_blocks && cas->ascii_last_block_size == 0) {
		byte_offset += ASCII_LEADER + 6;
	}
	if (block == cas->ascii_blocks && byte_offset >= (block_size + ASCII_LEADER + 6)) {
		byte_offset -= (block_size + ASCII_LEADER + 6);
		if (byte_offset < ASCII_LEADER)
			return 0x55;
		byte_offset -= ASCII_LEADER;
		if (byte_offset >= EOFBLOCK_LENGTH)
			return -1;
		return ascii_eof_block[byte_offset];
	}
	if (byte_offset < ASCII_LEADER)
		return 0x55;
	byte_offset -= ASCII_LEADER;
	if (byte_offset == 0)
		return 0x55;
	if (byte_offset == 1)
		return 0x3c;
	if (byte_offset == 2) {
		cas->ascii_sum = 0x01;
		return 0x01;
	}
	if (byte_offset == 3) {
		cas->ascii_sum += block_size;
		return block_size;
	}
	byte_offset -= 4;
	if (byte_offset < block_size) {
		int byte = fs_read_uint8(cas->fd);
		if (byte == 0x0a) byte = 0x0d;
		cas->ascii_sum += byte;
		return byte;
	}
	byte_offset -= block_size;
	if (byte_offset == 0)
		return cas->ascii_sum & 0xff;
	return 0x55;
}

/* measures in number of pulses */
static long cas_tell(struct tape *t) {
	return t->offset;
}

static int cas_seek(struct tape *t, long offset, int whence) {
	struct tape_cas *cas = t->data;
	if (whence == FS_SEEK_CUR) {
		offset += t->offset;
	} else if (whence == FS_SEEK_END) {
		offset += t->size;
	}
	long seek_byte = offset >> 4;
	if (cas->is_ascii) {
		seek_byte -= ASCII_LEADER;
		seek_byte -= NAMEBLOCK_LENGTH;
		int block = seek_byte / (ASCII_LEADER + 261);
		int block_offset = seek_byte - (block * (ASCII_LEADER + 261));
		if (block_offset < 4) block_offset = 0;
		if (block >= cas->ascii_blocks) {
			if (block_offset > cas->ascii_last_block_size)
				block_offset = cas->ascii_last_block_size;
		} else {
			if (block_offset > 255)
				block_offset = 255;
		}
		seek_byte = (block * 255) + block_offset;
		if (seek_byte < 0) seek_byte = 0;
		if (seek_byte > (t->size >> 4)) seek_byte = (t->size >> 4);
	}
	if (fs_lseek(cas->fd, seek_byte, FS_SEEK_SET) == -1)
		return -1;
	t->offset = offset;
	cas->bit_index = (offset >> 1) & 7;
	cas->pulse_index = offset & 1;
	if (cas->bit_index != 0 || cas->pulse_index != 0) {
		cas->byte = read_byte(t);
		cas->bit = (cas->byte & (1 << cas->bit_index)) ? 1 : 0;
	}
	return 0;
}

/* approximate time based on av. pulse length */
static int cas_to_ms(struct tape *t, long pos) {
	(void)t;
	return (pos * 10) / 32;
}

static long cas_ms_to(struct tape *t, int ms) {
	(void)t;
	return (ms * 32) / 10;
}

/* Reading */

static int cas_pulse_in(struct tape *t, int *pulse_width) {
	struct tape_cas *cas = t->data;
	if (cas->pulse_index == 0) {
		if (cas->bit_index == 0) {
			cas->byte = read_byte(t);
			if (cas->byte == -1) return -1;
		}
		cas->bit = (cas->byte >> cas->bit_index) & 1;
	}
	if (cas->bit == 0) {
		*pulse_width = TAPE_BIT0_LENGTH / 2;
	} else {
		*pulse_width = TAPE_BIT1_LENGTH / 2;
	}
	t->offset++;
	cas->pulse_index = (cas->pulse_index + 1) & 1;
	if (cas->pulse_index == 0) {
		cas->bit_index = (cas->bit_index + 1) & 7;
	}
	return !cas->pulse_index;
}

/* Writing */

static void bit_out(struct tape_cas *cas, int bit) {
	cas->output_byte = ((cas->output_byte >> 1) & 0x7f) | (bit ? 0x80 : 0);
	cas->output_bit_count++;
	if (cas->output_bit_count == 8) {
		cas->output_bit_count = 0;
		fs_write_uint8(cas->fd, cas->output_byte);
	}
}

static void add_to_leader(struct tape_cas *cas, int length) {
	int i = 0;
	cas->leader_length += length;
	while (cas->leader_length > TAPE_AV_BIT_LENGTH) {
		i++;
		cas->leader_bits++;
		cas->leader_length -= TAPE_AV_BIT_LENGTH;
	}
}

static void pulse_out(struct tape *t, int pulse_length) {
	struct tape_cas *cas = t->data;
	if (pulse_length < (TAPE_BIT1_LENGTH / 4)
	    || pulse_length > (TAPE_BIT0_LENGTH * 2)) {
		add_to_leader(cas, pulse_length);
		/* invalidate any preceeding pulse */
		if (cas->last_pulse_length > 0) {
			add_to_leader(cas, cas->last_pulse_length);
			cas->last_pulse_length = 0;
		}
		return;
	}
	if (cas->last_pulse_length == 0) {
		cas->last_pulse_length = pulse_length;
		return;
	}
	/* if pulses aren't similar enough lengths, ignore last_pulse */
	int delta = abs(pulse_length - cas->last_pulse_length);
	if (delta > (pulse_length / 2)
	    || delta > (cas->last_pulse_length / 2)) {
		add_to_leader(cas, cas->last_pulse_length);
		cas->last_pulse_length = pulse_length;
		return;
	}
	/* ok, looks like these make a bit! */
	int bit_length = pulse_length + cas->last_pulse_length;
	int bit = (bit_length > TAPE_AV_BIT_LENGTH) ? 0 : 1;
	/* if there is any leader, extend it up to the next byte boundary */
	if (cas->leader_bits > 0) {
		if (cas->output_bit_count > 0) {
			/* pad current output byte */
			cas->leader_bits &= ~7;
			cas->leader_bits += (8 - cas->output_bit_count);
		} else {
			cas->leader_bits = (cas->leader_bits + 7) & ~7;
		}
	}
	/* dump leader bits - ensure we finish with the opposite of the bit
	 * we're about to write */
	int lbit = (cas->leader_bits & 1) ? !bit : bit;
	while (cas->leader_bits > 0) {
		bit_out(cas, lbit);
		t->offset += 2;
		lbit = !lbit;
		cas->leader_bits--;
	}
	cas->leader_length = 0;
	/* now, our bit */
	bit_out(cas, bit);
	t->offset += 2;
	cas->last_pulse_length = 0;
	if (t->offset > t->size) t->size = t->offset;
}

static int cas_sample_out(struct tape *t, uint8_t sample, int length) {
	struct tape_cas *cas = t->data;
	int sense = (sample >= 0x80);
	if (cas->output_sense == -1) {
		cas->output_sense = sense;
	}
	if (sense != cas->output_sense) {
		pulse_out(t, cas->pulse_length);
		cas->pulse_length = length;
		cas->output_sense = sense;
		return 0;
	}
	cas->pulse_length += length;
	while (cas->pulse_length > (2 * TAPE_BIT0_LENGTH)) {
		add_to_leader(cas, TAPE_AV_BIT_LENGTH);
		cas->pulse_length -= TAPE_AV_BIT_LENGTH;
	}
	return 0;
}

static void cas_motor_off(struct tape *t) {
	struct tape_cas *cas = t->data;
	/* ensure last pulse is flushed */
	cas_sample_out(t, 0x00, 0);
	cas_sample_out(t, 0x80, 0);
	while (cas->output_bit_count > 0) {
		bit_out(cas, 0);
		t->offset += 2;
		if (t->offset > t->size) t->size = t->offset;
	}
	cas->output_sense = -1;
	cas->pulse_length = cas->last_pulse_length = 0;
	cas->leader_length = cas->leader_bits = 0;
}
