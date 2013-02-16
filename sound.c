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

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "portalib/glib.h"

#include "config.h"

#include "events.h"
#include "logging.h"
#include "machine.h"
#include "mc6821.h"
#include "module.h"
#include "sound.h"
#include "tape.h"
#include "xroar.h"

union sample_t {
	uint8_t as_int8;
	uint16_t as_int16;
	float as_float;
};

/* Describes the buffer: */
static enum sound_fmt buffer_fmt;
static int buffer_nchannels;
static int buffer_nsamples;
static void *buffer = NULL;
/* Current index into the buffer (in samples, not frames): */
static int buffer_sample;

static union sample_t last_sample;
static event_ticks last_cycle;
static unsigned ticks_per_sample;
static unsigned ticks_per_buffer;

/* All these gain values are computed by set_volume().  They're all relative to
 * full scale = 32767 = 4.7V */

// DAC output scale & offset:
static unsigned dac_gain = 124;
static unsigned dac_offset = 1394;
// DAC output when single bit out == 0:
static unsigned dacs0_gain = 78;
static unsigned dacs0_offset = 1254;
// DAC output when single bit out == 1:
static unsigned dacs1_gain = 94;
static unsigned dacs1_offset = 9063;
// Single bit out when MUX disabled:
static unsigned sbs1 = 27189;
// Tape value for tape_audio == 1:
static unsigned tape1 = 1742;

static void flush_frame(void *);
static struct event flush_event;

void sound_init(void *buf, enum sound_fmt fmt, unsigned rate, unsigned nchannels, unsigned nframes) {
	uint16_t test = 0x0123;
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
		last_sample.as_int8 = 0x80;
		LOG_DEBUG(2, "8-bit unsigned, ");
		break;
	case SOUND_FMT_S8:
		last_sample.as_int8 = 0x00;
		LOG_DEBUG(2, "8-bit signed, ");
		break;
	case SOUND_FMT_S16_HE:
	case SOUND_FMT_S16_SE:
		last_sample.as_int16 = 0x00;
		LOG_DEBUG(2, "16-bit signed %s-endian, ", fmt_big_endian ? "big" : "little" );
		break;
	case SOUND_FMT_FLOAT:
		last_sample.as_float = 0.;
		LOG_DEBUG(2, "Floating point, ");
		break;
	case SOUND_FMT_NULL:
	default:
		fmt = SOUND_FMT_NULL;
		LOG_DEBUG(2, "No audio\n");
		break;
	}
	if (fmt != SOUND_FMT_NULL) {
		switch (nchannels) {
		case 1: LOG_DEBUG(2, "mono, "); break;
		case 2: LOG_DEBUG(2, "stereo, "); break;
		default: LOG_DEBUG(2, "%d channel, ", nchannels); break;
		}
		LOG_DEBUG(2, "%dHz\n", rate);
	}

	buffer = buf;
	buffer_nsamples = nframes * nchannels;
	buffer_fmt = fmt;
	buffer_nchannels = nchannels;
	ticks_per_sample = OSCILLATOR_RATE / rate;
	ticks_per_buffer = ticks_per_sample * nframes;
	last_cycle = event_current_tick;

	if (buffer)
		sound_render_silence(buffer, buffer_nsamples);

	event_init(&flush_event, flush_frame, NULL);
	flush_event.at_tick = event_current_tick + ticks_per_buffer;
	event_queue(&MACHINE_EVENT_LIST, &flush_event);
}

void sound_set_volume(int v) {
	if (v < 0) v = 0;
	if (v > 100) v = 100;
	float scale = 32767. * ((float)v / 100);
	dac_gain = (scale * 4.5) / (4.7 * 252.);
	dacs0_gain = (scale * 2.84) / (4.7 * 252.);
	dacs1_gain = (scale * 3.4) / (4.7 * 252.);
	dac_offset = (scale * 0.2) / 4.7;
	dacs0_offset = (scale * 0.18) / 4.7;
	dacs1_offset = (scale * 1.3) / 4.7;
	sbs1 = (scale * 3.9) / 4.7;
	tape1 = (scale * .25) / 4.7;
}

/* within sound_update(), this loop is included for each sample format */
#define fill_buffer(type,member) do { \
		while ((event_current_tick - last_cycle) <= (UINT_MAX/2)) { \
			for (i = buffer_nchannels; i; i--) \
				((type *)buffer)[buffer_sample++] = last_sample.member; \
			last_cycle += ticks_per_sample; \
			if (buffer_sample >= buffer_nsamples) { \
				buffer = sound_module->write_buffer(buffer); \
				buffer_sample = 0; \
			} \
		} \
	} while (0)

/* Fill sound buffer to current point in time, call sound modules
 * update() function if buffer is full. */
void sound_update(void) {
	int i;
	if (buffer) {
		switch (buffer_fmt) {
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
		default:
			break;
		}
	} else {
		while ((event_current_tick - last_cycle) <= (UINT_MAX/2)) {
			last_cycle += ticks_per_buffer;
			buffer = sound_module->write_buffer(buffer);
		}
	}

	_Bool mux_enabled = PIA1.b.control_register & 0x08;
	_Bool sbs_enabled = !((PIA1.b.out_source ^ PIA1.b.out_sink) & (1<<1));
	_Bool sbs = PIA1.b.out_source & PIA1.b.out_sink & (1<<1);
#ifndef FAST_SOUND
	PIA1.b.in_source |= (1<<1);
	PIA1.b.in_sink |= (1<<1);
#endif
	unsigned value;
	if (mux_enabled) {
		int source = ((PIA0.b.control_register & (1<<3)) >> 2)
		             | ((PIA0.a.control_register & (1<<3)) >> 3);
		switch (source) {
		case 0:
			value = PIA_VALUE_A(PIA1) & 0xfc;
			if (sbs_enabled) {
				if (sbs) {
					value = (value * dacs1_gain) + dacs1_offset;
				} else {
					value = (value * dacs0_gain) + dacs0_offset;
				}
			} else {
#ifndef FAST_SOUND
				if (value <= 0x80) {
					PIA1.b.in_source &= ~(1<<1);
					PIA1.b.in_sink &= ~(1<<1);
				}
#endif
				value = (value * dac_gain) + dac_offset;
			}
			break;
		case 1:
			value = tape_audio ? tape1 : 0;
			break;
		default:
			value = 0;
			break;
		}
	} else {
		value = (sbs_enabled && sbs) ? sbs1 : 0;
	}

	switch (buffer_fmt) {
	case SOUND_FMT_U8:
		last_sample.as_int8 = (value >> 8) + 0x80;
		break;
	case SOUND_FMT_S8:
		last_sample.as_int8 = value >> 8;
		break;
	case SOUND_FMT_S16_HE:
		last_sample.as_int16 = value;
		break;
	case SOUND_FMT_S16_SE:
		last_sample.as_int16 = (value & 0xff) << 8 | ((value >> 8) & 0xff);
		break;
	case SOUND_FMT_FLOAT:
		last_sample.as_float = (float)value / 32767.;
		break;
	default:
		break;
	}
}

void sound_render_silence(void *buf, int samples) {
	int i;
	for (i = 0; i < samples; i++) {
		switch (buffer_fmt) {
		case SOUND_FMT_U8:
		case SOUND_FMT_S8:
			((uint8_t *)buf)[i] = last_sample.as_int8;
			break;
		case SOUND_FMT_S16_HE:
		case SOUND_FMT_S16_SE:
			((uint16_t *)buf)[i] = last_sample.as_int16;
			break;
		case SOUND_FMT_FLOAT:
			((float *)buf)[i] = last_sample.as_float;
			break;
		default:
			break;
		}
	}
}

static void flush_frame(void *data) {
	(void)data;
	sound_update();
	flush_event.at_tick += ticks_per_buffer;
	event_queue(&MACHINE_EVENT_LIST, &flush_event);
}
