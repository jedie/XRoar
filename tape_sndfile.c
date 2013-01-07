/*  Copyright 2003-2013 Ciaran Anscomb
 *
 *  This file is part of XRoar.
 *
 *  XRoar is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  XRoar is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XRoar.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <inttypes.h>
#include <stdlib.h>
#include <sndfile.h>
#include "portalib/glib.h"

#include "fs.h"
#include "logging.h"
#include "machine.h"
#include "tape.h"

#define BLOCK_LENGTH (512)

struct tape_sndfile {
	SF_INFO info;
	SNDFILE *fd;
	_Bool writing;
	int cycles_per_frame;
	short *block;
	sf_count_t block_length;
	sf_count_t cursor;
	int cycles_to_write;
};

static void sndfile_close(struct tape *t);
static long sndfile_tell(struct tape *t);
static int sndfile_seek(struct tape *t, long offset, int whence);
static int sndfile_to_ms(struct tape *t, long pos);
static long sndfile_ms_to(struct tape *t, int ms);
static int sndfile_pulse_in(struct tape *t, int *pulse_width);
static int sndfile_sample_out(struct tape *t, uint8_t sample, int length);
static void sndfile_motor_off(struct tape *t);

struct tape_module tape_sndfile_module = {
	.close = sndfile_close, .tell = sndfile_tell, .seek = sndfile_seek,
	.to_ms = sndfile_to_ms, .ms_to = sndfile_ms_to,
	.pulse_in = sndfile_pulse_in, .sample_out = sndfile_sample_out,
	.motor_off = sndfile_motor_off,
};

struct tape *tape_sndfile_open(const char *filename, const char *mode) {
	struct tape *t;
	struct tape_sndfile *sndfile;
	t = tape_new();
	t->module = &tape_sndfile_module;
	sndfile = g_malloc(sizeof(struct tape_sndfile));
	t->data = sndfile;
	/* initialise sndfile */
	sndfile->info.format = 0;
	if (mode[0] == 'w') {
		sndfile->writing = 1;
		sndfile->info.samplerate = 22050;
		sndfile->info.channels = 1;
		sndfile->info.format = SF_FORMAT_WAV | SF_FORMAT_PCM_U8;
		sndfile->fd = sf_open(filename, SFM_WRITE, &sndfile->info);
	} else {
		sndfile->writing = 0;
		sndfile->fd = sf_open(filename, SFM_READ, &sndfile->info);
	}
	if (!sndfile->fd) {
		LOG_WARN("libsndfile error: %s\n", sf_strerror(NULL));
		g_free(sndfile);
		tape_free(t);
		return NULL;
	}
	if (sndfile->info.samplerate == 0 || sndfile->info.channels < 1) {
		sndfile_close(t);
		return NULL;
	}
	sndfile->cycles_per_frame = OSCILLATOR_RATE / sndfile->info.samplerate;
	sndfile->block = g_malloc(BLOCK_LENGTH * sizeof(short) * sndfile->info.channels);
	sndfile->block_length = 0;
	sndfile->cursor = 0;
	/* find size */
	long size = sf_seek(sndfile->fd, 0, SEEK_END);
	if (size >= 0) {
		t->size = size;
	}
	/* rewind to start */
	sf_seek(sndfile->fd, 0, SEEK_SET);
	t->offset = 0;
	return t;
}

static void sndfile_close(struct tape *t) {
	struct tape_sndfile *sndfile = t->data;
	sndfile_motor_off(t);
	g_free(sndfile->block);
	sf_close(sndfile->fd);
	g_free(sndfile);
	tape_free(t);
}

static long sndfile_tell(struct tape *t) {
	return t->offset;
}

static int sndfile_seek(struct tape *t, long offset, int whence) {
	struct tape_sndfile *sndfile = t->data;
	sf_count_t new_offset = sf_seek(sndfile->fd, offset, whence);
	if (new_offset == -1)
		return -1;
	t->offset = new_offset;
	sndfile->block_length = 0;
	sndfile->cursor = 0;
	return 0;
}

static int sndfile_to_ms(struct tape *t, long pos) {
	struct tape_sndfile *sndfile = t->data;
	float ms = (float)pos * 1000. / (float)sndfile->info.samplerate;
	return (int)ms;
}

static long sndfile_ms_to(struct tape *t, int ms) {
	struct tape_sndfile *sndfile = t->data;
	float pos = (float)ms * (float)sndfile->info.samplerate / 1000.;
	return (long)pos;
}

static sf_count_t read_sample(struct tape_sndfile *sndfile, short *s) {
	if (sndfile->cursor >= sndfile->block_length) {
		sndfile->block_length = sf_readf_short(sndfile->fd, sndfile->block, BLOCK_LENGTH);
		sndfile->cursor = 0;
	}
	if (sndfile->cursor >= sndfile->block_length) {
		return 0;
	}
	*s = sndfile->block[sndfile->cursor * sndfile->info.channels];
	sndfile->cursor++;
	return 1;
}

static int sndfile_pulse_in(struct tape *t, int *pulse_width) {
	struct tape_sndfile *sndfile = t->data;
	short sample;
	if (!read_sample(sndfile, &sample))
		return -1;
	t->offset++;
	int sign = (sample < 0);
	int length = sndfile->cycles_per_frame;
	while (read_sample(sndfile, &sample)) {
		if ((sample < 0) != sign) {
			sndfile->cursor--;
			break;
		}
		t->offset++;
		length += sndfile->cycles_per_frame;
		if (length > (OSCILLATOR_RATE / 2))
			break;
	}
	*pulse_width = length;
	return sign;
}

/* Writing */

static int write_sample(struct tape *t, short s) {
	struct tape_sndfile *sndfile = t->data;
	short *dest = sndfile->block + (sndfile->block_length * sndfile->info.channels);
	int i;
	/* write frame */
	for (i = 0; i < sndfile->info.channels; i++) {
		*(dest++) = s;
	}
	sndfile->block_length++;
	/* write full blocks */
	if (sndfile->block_length >= BLOCK_LENGTH) {
		sf_count_t written = sf_writef_short(sndfile->fd, sndfile->block, sndfile->block_length);
		if (written >= 0)
			t->offset += written;
		sndfile->block_length = 0;
		if (written < sndfile->block_length) {
			return 0;
		}
	}
	sndfile->cursor = sndfile->block_length;
	return 1;
}

static int sndfile_sample_out(struct tape *t, uint8_t sample, int length) {
	struct tape_sndfile *sndfile = t->data;
	short sample_out = ((int)sample - 0x80) * 256;
	sndfile->cycles_to_write += length;
	while (sndfile->cycles_to_write > sndfile->cycles_per_frame) {
		sndfile->cycles_to_write -= sndfile->cycles_per_frame;
		if (write_sample(t, sample_out) < 1)
			return -1;
	}
	return 0;
}

static void sndfile_motor_off(struct tape *t) {
	struct tape_sndfile *sndfile = t->data;
	if (!sndfile->writing) return;
	/* flush output block */
	if (sndfile->block_length > 0) {
		sf_count_t written = sf_writef_short(sndfile->fd, sndfile->block, sndfile->block_length);
		if (written >= 0)
			t->offset += written;
		sndfile->block_length = 0;
		sndfile->cursor = 0;
	}
}
