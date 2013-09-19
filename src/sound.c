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

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "pl_glib.h"

#include "events.h"
#include "logging.h"
#include "machine.h"
#include "module.h"
#include "sound.h"
#include "tape.h"
#include "xroar.h"

union sample_t {
	uint8_t as_int8[2];
	uint16_t as_int16[2];
	float as_float[2];
};

/* Describes the buffer: */
static enum sound_fmt buffer_fmt;
static int buffer_nchannels;
static unsigned buffer_nframes;
static void *buffer = NULL;
/* Current index into the buffer */
static unsigned buffer_frame = 0;

static union sample_t last_sample;
static event_ticks last_cycle;
static unsigned ticks_per_frame;
static unsigned ticks_per_buffer;

enum sound_source {
	SOURCE_DAC,
	SOURCE_TAPE,
	SOURCE_CART,
	SOURCE_NONE,
	SOURCE_SINGLE_BIT,
	NUM_SOURCES
};

/* These are the absolute measured voltages on a real Dragon for audio output
 * for each source, each indicated by a scale and offset.  Getting these right
 * should mean that any transition of single bit or mux enable will produce the
 * right effect.  Primary index indicates source, secondary index is by:
 *
 * Secondary index into each array is by:
 * 2 - Single bit output enabled and high
 * 1 - Single bit output enabled and low
 * 0 - Single bit output disabled
 */

// Maximum measured voltage:
static const float full_scale_v = 4.7;

// Source gains
static const float source_gain_v[NUM_SOURCES][3] = {
	{ 4.5, 2.84, 3.4 },  // DAC
	{ 0.5, 0.4, 0.5 },  // Tape
	{ 0.0, 0.0, 0.0 },  // Cart
	{ 0.0, 0.0, 0.0 },  // None
	{ 0.0, 0.0, 0.0 }  // Single-bit
};

// Source offsets
static const float source_offset_v[NUM_SOURCES][3] = {
	{ 0.2, 0.18, 1.3 },  // DAC
	{ 2.05, 1.6, 2.35 },  // Tape
	{ 0.0, 0.0, 3.9 },  // Cart
	{ 0.0, 0.0, 0.01 },  // None
	{ 0.0, 0.0, 3.9 }  // Single-bit
};

// Computed by set_volume().  Defaults to scale at full volume.
static unsigned scale = 6971;

static void flush_frame(void *);
static struct event flush_event;

static _Bool external_audio = 0;

static _Bool sbs_enabled = 0;
static _Bool sbs_level = 0;
static _Bool mux_enabled = 0;
static unsigned mux_source;
static float dac_level = 0.0;
static float tape_level = 0.0;
static float cart_level = 0.0;
static float external_level[2] = { 0.0, 0.0 };

// Feedback to the single-bit audio pin
sound_feedback_delegate sound_sbs_feedback = { NULL, NULL };

void sound_init(void *buf, enum sound_fmt fmt, unsigned rate, unsigned nchannels, unsigned nframes) {
	uint16_t test = 0x0123;
	int big_endian = (*(uint8_t *)(&test) == 0x01);
	int fmt_big_endian = 1;

	if (nchannels < 1 || nchannels > 2) {
		LOG_WARN("Invalid number of audio channels: disabling sound.");
		buffer = NULL;
		buffer_fmt = SOUND_FMT_NULL;
		return;
	}

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
		last_sample.as_int8[0] = last_sample.as_int8[1] = 0x80;
		LOG_DEBUG(2, "8-bit unsigned, ");
		break;
	case SOUND_FMT_S8:
		last_sample.as_int8[0] = last_sample.as_int8[1] = 0x00;
		LOG_DEBUG(2, "8-bit signed, ");
		break;
	case SOUND_FMT_S16_HE:
	case SOUND_FMT_S16_SE:
		last_sample.as_int16[0] = last_sample.as_int16[1] = 0x00;
		LOG_DEBUG(2, "16-bit signed %s-endian, ", fmt_big_endian ? "big" : "little" );
		break;
	case SOUND_FMT_FLOAT:
		last_sample.as_float[0] = last_sample.as_float[1] = 0.;
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
	buffer_nframes = nframes;
	buffer_fmt = fmt;
	buffer_nchannels = nchannels;
	ticks_per_frame = OSCILLATOR_RATE / rate;
	ticks_per_buffer = ticks_per_frame * nframes;
	last_cycle = event_current_tick;

	if (buffer)
		sound_render_silence(buffer, buffer_nframes);

	event_init(&flush_event, flush_frame, NULL);
	flush_event.at_tick = event_current_tick + ticks_per_buffer;
	event_queue(&MACHINE_EVENT_LIST, &flush_event);
}

void sound_set_volume(int v) {
	if (v < 0) v = 0;
	if (v > 100) v = 100;
	scale = (unsigned)((327.67 * (float)v) / full_scale_v);
}

static int fill_int8(int nframes) {
	if ((buffer_frame + nframes) > buffer_nframes)
		nframes = buffer_nframes - buffer_frame;
	uint8_t *ptr = (uint8_t *)buffer + buffer_frame * buffer_nchannels;
	if (buffer_nchannels == 1) {
		/* special case for single channel 8-bit */
		memset(ptr, last_sample.as_int8[0], nframes);
	} else {
		for (int i = 0; i < nframes; i++) {
			for (int j = 0; j < buffer_nchannels; j++) {
				*(ptr++) = last_sample.as_int8[j];
			}
		}
	}
	buffer_frame += nframes;
	if (buffer_frame >= buffer_nframes) {
		buffer = sound_module->write_buffer(buffer);
		buffer_frame = 0;
	}
	last_cycle += nframes * ticks_per_frame;
	return nframes;
}

static int fill_int16(int nframes) {
	if ((buffer_frame + nframes) > buffer_nframes)
		nframes = buffer_nframes - buffer_frame;
	uint16_t *ptr = (uint16_t *)buffer + buffer_frame * buffer_nchannels;
	for (int i = 0; i < nframes; i++) {
		for (int j = 0; j < buffer_nchannels; j++) {
			*(ptr++) = last_sample.as_int16[j];
		}
	}
	buffer_frame += nframes;
	if (buffer_frame >= buffer_nframes) {
		buffer = sound_module->write_buffer(buffer);
		buffer_frame = 0;
	}
	last_cycle += nframes * ticks_per_frame;
	return nframes;
}

static int fill_float(int nframes) {
	if ((buffer_frame + nframes) > buffer_nframes)
		nframes = buffer_nframes - buffer_frame;
	float *ptr = (float *)buffer + buffer_frame * buffer_nchannels;
	for (int i = 0; i < nframes; i++) {
		for (int j = 0; j < buffer_nchannels; j++) {
			*(ptr++) = last_sample.as_float[j];
		}
	}
	buffer_frame += nframes;
	if (buffer_frame >= buffer_nframes) {
		buffer = sound_module->write_buffer(buffer);
		buffer_frame = 0;
	}
	last_cycle += nframes * ticks_per_frame;
	return nframes;
}

/* Fill sound buffer to current point in time, call sound module's
 * update() function if buffer is full. */
static void sound_update(void) {
	unsigned elapsed = (event_current_tick - last_cycle);
	unsigned nframes = 0;
	if (elapsed <= (UINT_MAX/2)) {
		nframes = elapsed / ticks_per_frame;
	}
	if (buffer) {
		switch (buffer_fmt) {
		case SOUND_FMT_U8:
		case SOUND_FMT_S8:
			while (nframes)
				nframes -= fill_int8(nframes);
			break;
		case SOUND_FMT_S16_HE:
		case SOUND_FMT_S16_SE:
			while (nframes)
				nframes -= fill_int16(nframes);
			break;
		case SOUND_FMT_FLOAT:
			while (nframes)
				nframes -= fill_float(nframes);
			break;
		default:
			break;
		}
	} else {
		while (nframes >= buffer_nframes) {
			buffer = sound_module->write_buffer(buffer);
			last_cycle += ticks_per_buffer;
			nframes -= buffer_nframes;
		}
	}

	/* Mix internal sound sources to bus */
	float bus_level = 0.0;
	unsigned sindex = sbs_enabled ? (sbs_level ? 2 : 1) : 0;
	enum sound_source source;
	if (mux_enabled) {
		source = mux_source;
		switch (source) {
		case SOURCE_DAC:
			bus_level = dac_level;
			break;
		case SOURCE_TAPE:
			bus_level = tape_level;
			break;
		default:
		case SOURCE_CART:
		case SOURCE_NONE:
			bus_level = cart_level;
			break;
		}
	} else {
		source = SOURCE_SINGLE_BIT;
		bus_level = 0.0;
	}
	bus_level = (bus_level * source_gain_v[source][sindex])
			 + source_offset_v[source][sindex];

	/* Feed back bus level to single bit pin */
#ifndef FAST_SOUND
	if (sound_sbs_feedback.delegate)
		sound_sbs_feedback.delegate(sound_sbs_feedback.dptr,
		                            sbs_enabled || bus_level >= 1.414);
#endif

	/* Mix bus & external sound */
	float level[2];
	if (external_audio) {
		level[0] = (external_level[0]*full_scale_v + bus_level) / 2.0;
		level[1] = (external_level[1]*full_scale_v + bus_level) / 2.0;
	} else {
		level[0] = bus_level;
		level[1] = bus_level;
	}
	/* Downmix to mono */
	if (buffer_nchannels == 1)
		level[0] = (level[0] + level[1]) / 2.0;

	/* Update output samples */
	for (int i = 0; i < buffer_nchannels; i++) {
		unsigned output = level[i] * scale;
		switch (buffer_fmt) {
		case SOUND_FMT_U8:
			last_sample.as_int8[i] = (output >> 8) + 0x80;
			break;
		case SOUND_FMT_S8:
			last_sample.as_int8[i] = output >> 8;
			break;
		case SOUND_FMT_S16_HE:
			last_sample.as_int16[i] = output;
			break;
		case SOUND_FMT_S16_SE:
			last_sample.as_int16[i] = (output & 0xff) << 8 | ((output >> 8) & 0xff);
			break;
		case SOUND_FMT_FLOAT:
			last_sample.as_float[i] = (float)output / 32767.;
			break;
		default:
			break;
		}
	}
}

void sound_enable_external(void) {
	external_audio = 1;
}

void sound_disable_external(void) {
	external_audio = 0;
}

void sound_set_sbs(_Bool enabled, _Bool level) {
	if (sbs_enabled == enabled && sbs_level == level)
		return;
	sbs_enabled = enabled;
	sbs_level = level;
	sound_update();
}

void sound_set_mux_enabled(_Bool enabled) {
	if (mux_enabled == enabled)
		return;
	mux_enabled = enabled;
#ifndef FAST_SOUND
	if (xroar_cfg.fast_sound)
		return;
#endif
	sound_update();
}

void sound_set_mux_source(unsigned source) {
	if (mux_source == source)
		return;
	mux_source = source;
	if (!mux_enabled)
		return;
#ifndef FAST_SOUND
	if (xroar_cfg.fast_sound)
		return;
#endif
	sound_update();
}

void sound_set_dac_level(float level) {
	dac_level = level;
	if (mux_enabled && mux_source == SOURCE_DAC)
		sound_update();
}

void sound_set_tape_level(float level) {
	tape_level = level;
	if (mux_enabled && mux_source == SOURCE_TAPE)
		sound_update();
}

void sound_set_cart_level(float level) {
	cart_level = level;
	if (mux_enabled && mux_source == SOURCE_CART)
		sound_update();
}

void sound_set_external_left(float level) {
	external_level[0] = level;
	if (external_audio)
		sound_update();
}

void sound_set_external_right(float level) {
	external_level[1] = level;
	if (external_audio)
		sound_update();
}

void sound_render_silence(void *buf, unsigned nframes) {
	switch (buffer_fmt) {
	case SOUND_FMT_U8:
	case SOUND_FMT_S8:
		/* special case for single channel 8-bit */
		if (buffer_nchannels == 1) {
			memset(buf, last_sample.as_int8[0], nframes);
		} else {
			uint8_t *ptr = buf;
			for (; nframes; nframes--) {
				*(ptr++) = last_sample.as_int8[0];
				*(ptr++) = last_sample.as_int8[1];
			}
		}
		break;
	case SOUND_FMT_S16_HE:
	case SOUND_FMT_S16_SE: {
		uint16_t *ptr = buf;
		for (; nframes; nframes--)
			for (int j = 0; j < buffer_nchannels; j++)
				*(ptr++) = last_sample.as_int16[j];
	} break;
	case SOUND_FMT_FLOAT: {
		float *ptr = buf;
		for (; nframes; nframes--)
			for (int j = 0; j < buffer_nchannels; j++)
				*(ptr++) = last_sample.as_float[j];
	} break;
	default:
		break;
	}
}

static void flush_frame(void *data) {
	(void)data;
	sound_update();
	flush_event.at_tick += ticks_per_buffer;
	event_queue(&MACHINE_EVENT_LIST, &flush_event);
}
