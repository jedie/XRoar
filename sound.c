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

#include "config.h"

#include <stdlib.h>
#include <string.h>

#include "types.h"
#include "events.h"
#include "logging.h"
#include "machine.h"
#include "mc6821.h"
#include "misc.h"
#include "module.h"
#include "sound.h"
#include "tape.h"
#include "xroar.h"

union sample_t {
	uint8_t as_int8;
	uint16_t as_int16;
	float as_float;
};

int format;
int num_channels;
void *buffer = NULL;
int buffer_index;
int buffer_size;
static union sample_t last_sample;
static Cycle last_cycle;
static int cycles_per_sample;
static int cycles_per_frame;
int volume;

static void flush_frame(void);
static event_t flush_event;

void *sound_init(int sample_rate, int channels, int fmt, int frame_size) {
	uint16_t test = 0x0123;
	int size;
	int big_endian = (*(uint8_t *)(&test) == 0x01);
	int fmt_big_endian = 1;

	if (fmt == SOUND_FMT_S16_BE) {
		fmt_big_endian = 1;
		if (big_endian) {
			fmt = SOUND_FMT_S16_HE;
		} else {
			fmt = SOUND_FMT_S16_SE;
		}
	} else if (fmt == SOUND_FMT_S16_LE) {
		fmt_big_endian = 0;
		if (big_endian) {
			fmt = SOUND_FMT_S16_SE;
		} else {
			fmt = SOUND_FMT_S16_HE;
		}
	} else if (fmt == SOUND_FMT_S16_HE) {
		fmt_big_endian = big_endian;
	} else if (fmt == SOUND_FMT_S16_SE) {
		fmt_big_endian = !big_endian;
	}

	LOG_DEBUG(2, "\t");
	switch (fmt) {
	case SOUND_FMT_U8:
		size = 1;
		last_sample.as_int8 = 0x80;
		LOG_DEBUG(2, "8-bit unsigned, ");
		break;
	case SOUND_FMT_S8:
		size = 1;
		last_sample.as_int8 = 0x00;
		LOG_DEBUG(2, "8-bit signed, ");
		break;
	case SOUND_FMT_S16_HE:
	case SOUND_FMT_S16_SE:
		size = 2;
		last_sample.as_int16 = 0x00;
		LOG_DEBUG(2, "16-bit signed %s-endian, ", fmt_big_endian ? "big" : "little" );
		break;
	case SOUND_FMT_FLOAT:
		size = sizeof(float);
		last_sample.as_float = (float)(-64 * volume) / 65536.;
		LOG_DEBUG(2, "Floating point, ");
		break;
	case SOUND_FMT_NULL:
		LOG_DEBUG(2, "No audio\n");
		break;
	default:
		return NULL;
	}
	if (fmt != SOUND_FMT_NULL) {
		switch (channels) {
		case 1: LOG_DEBUG(2, "mono, "); break;
		case 2: LOG_DEBUG(2, "stereo, "); break;
		default: LOG_DEBUG(2, "%d channel, ", channels); break;
		}
		LOG_DEBUG(2, "%dHz\n", sample_rate);
		buffer = xrealloc(buffer, frame_size * channels * size);
	}

	buffer_size = frame_size * channels;
	format = fmt;
	num_channels = channels;
	cycles_per_sample = OSCILLATOR_RATE / sample_rate;
	cycles_per_frame = cycles_per_sample * frame_size;
	last_cycle = current_cycle;

	sound_silence();

	event_init(&flush_event);
	flush_event.dispatch = flush_frame;
	flush_event.at_cycle = current_cycle + cycles_per_frame;
	event_queue(&MACHINE_EVENT_LIST, &flush_event);

	return buffer;
}

void sound_set_volume(int v) {
	if (v < 0) v = 0;
	if (v > 100) v = 100;
	volume = (256 * v) / 100;
}

/* within sound_update(), this loop is included for each sample format */
#define fill_buffer(type,member) do { \
		while ((int)(current_cycle - last_cycle) > 0) { \
			for (i = num_channels; i; i--) \
				((type *)buffer)[buffer_index++] = last_sample.member; \
			last_cycle += cycles_per_sample; \
			if (buffer_index >= buffer_size) { \
				sound_module->flush_frame(); \
				buffer_index = 0; \
			} \
		} \
	} while (0)

/* Fill sound buffer to current point in time, call sound modules
 * update() function if buffer is full. */
void sound_update(void) {
	int i;
	switch (format) {
		case SOUND_FMT_U8:
		case SOUND_FMT_S8:
			fill_buffer(uint8_t,as_int8);
			break;
		case SOUND_FMT_S16_HE:
		case SOUND_FMT_S16_SE:
			fill_buffer(uint16_t,as_int16);
			break;
		case SOUND_FMT_FLOAT:
			fill_buffer(float,as_float);
			break;
		case SOUND_FMT_NULL:
			while ((int)(current_cycle - last_cycle) <= 0) {
				last_cycle += cycles_per_frame;
				sound_module->flush_frame();
			}
			break;
		default:
			break;
	}

	int value;
	if (!(PIA1.b.control_register & 0x08)) {
		/* Single-bit sound */
		value = (PIA1.b.port_output & 0x02) ? 0x3f : 0;
	} else {
		int source = ((PIA0.b.control_register & 0x08) >> 2)
		             | ((PIA0.a.control_register & 0x08) >> 3);
		switch (source) {
			case 0:
				/* DAC output */
				value = (PIA1.a.port_output & 0xfc) >> 1;
				break;
			case 1:
				/* Tape input */
				value = tape_audio;
				break;
			default:
				/* CART input or disabled */
				value = 0;
				break;
		}
	}
#ifndef FAST_SOUND
	if (value >= 0x4c)
		PIA1.b.port_input |= 0x02;
	else
		PIA1.b.port_input &= 0xfd;
#endif


	switch (format) {
	case SOUND_FMT_U8:
		last_sample.as_int8 = ((value * volume) >> 8) ^ 0x80;
		break;
	case SOUND_FMT_S8:
		last_sample.as_int8 = (value * volume) >> 8;
		break;
	case SOUND_FMT_S16_HE:
		last_sample.as_int16 = value * volume;
		break;
	case SOUND_FMT_S16_SE:
		value *= volume;
		last_sample.as_int16 = (value & 0xff) << 8 | ((value >> 8) & 0xff);
		break;
	case SOUND_FMT_FLOAT:
		last_sample.as_float = (float)((value-64) * volume) / 65536.;
		break;
	default:
		break;
	}
}

void sound_silence(void) {
	sound_render_silence(buffer, buffer_size);
}

void sound_render_silence(void *buf, int samples) {
	int i;
	for (i = 0; i < samples; i++) {
		switch (format) {
		case SOUND_FMT_U8:
		case SOUND_FMT_S8:
			*((uint8_t *)buf++) = last_sample.as_int8;
			break;
		case SOUND_FMT_S16_HE:
		case SOUND_FMT_S16_SE:
			*((uint16_t *)buf++) = last_sample.as_int16;
			break;
		case SOUND_FMT_FLOAT:
			*((float *)buf++) = last_sample.as_float;
			break;
		default:
			break;
		}
	}
}

static void flush_frame(void) {
	sound_update();
	flush_event.at_cycle += cycles_per_frame;
	event_queue(&MACHINE_EVENT_LIST, &flush_event);
}
