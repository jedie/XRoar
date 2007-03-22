/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2007  Ciaran Anscomb
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

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/soundcard.h>

#include "types.h"
#include "events.h"
#include "logging.h"
#include "machine.h"
#include "module.h"
#include "pia.h"
#include "xroar.h"

static int init(int argc, char **argv);
static void shutdown(void);
static void update(void);

SoundModule sound_oss_module = {
	{ "oss", "OSS audio",
	  init, 0, shutdown, NULL },
	update
};

typedef int8_t Sample;  /* 8-bit mono */

#define FRAGMENTS 2
#define FRAME_SIZE 512
#define SAMPLE_CYCLES ((int)(OSCILLATOR_RATE / sample_rate))
#define FRAME_CYCLES (SAMPLE_CYCLES * FRAME_SIZE)

static unsigned int sample_rate;
static int sound_fd;
static int channels;
static int format;
unsigned int bytes_per_sample;

static Cycle frame_cycle_base;
static Sample *buffer;
static Sample *wrptr;
static Sample lastsample;
static int8_t *convbuf;

static void flush_frame(void *context);
static event_t *flush_event;

static int init(int argc, char **argv) {
	const char *device = "/dev/dsp";
	int fragment_param, tmp;

	(void)argc;
	(void)argv;
	LOG_DEBUG(2,"Initialising OSS audio driver\n");
	sound_fd = open(device, O_WRONLY);
	if (sound_fd == -1)
		goto failed;
	/* The order these ioctls are tried (format, channels, sample rate)
	 * is important: OSS docs say setting format can affect channels and
	 * sample rate, and setting channels can affect sample rate. */
	/* Set audio format.  Only support AFMT_S8 (signed 8-bit) and
	 * AFMT_S16_NE (signed 16-bit, host-endian) */
	if (ioctl(sound_fd, SNDCTL_DSP_GETFMTS, &format) == -1)
		goto failed;
	if ((format & (AFMT_S8 | AFMT_S16_NE)) == 0) {
		LOG_ERROR("No desired audio formats supported by device\n");
		return 1;
	}
	if (format & AFMT_S8) {
		format = AFMT_S8;
		bytes_per_sample = 1;
	} else {
		format = AFMT_S16_NE;
		bytes_per_sample = 2;
	}
	if (ioctl(sound_fd, SNDCTL_DSP_SETFMT, &format) == -1)
		goto failed;
	/* Set device to mono if possible */
	channels = 0;
	if (ioctl(sound_fd, SNDCTL_DSP_STEREO, &channels) == -1)
		goto failed;
	channels++;
	/* Attempt to set sample_rate to 44.1kHz, but live with whatever
	 * we get */
	sample_rate = 44100;
	if (ioctl(sound_fd, SNDCTL_DSP_SPEED, &sample_rate) == -1)
		goto failed;
	/* Set number of fragments low */
	fragment_param = 0;
	tmp = FRAME_SIZE * bytes_per_sample * channels;
	while (tmp > 1) {
		tmp >>= 1;
		fragment_param++;
	}
	fragment_param |= (FRAGMENTS << 16);
	tmp = fragment_param;
	if (ioctl(sound_fd, SNDCTL_DSP_SETFRAGMENT, &fragment_param) == -1)
		goto failed;
	LOG_DEBUG(2, "\t%d channel%s, %dHz, %d bytes per sample\n", channels, channels > 1 ? "s" : "", sample_rate, bytes_per_sample);
	if (tmp != fragment_param)
		LOG_WARN("Couldn't set desired buffer parameters: sync to audio might not be ideal\n");
	buffer = malloc(FRAME_SIZE * sizeof(Sample));
	convbuf = malloc(FRAME_SIZE * bytes_per_sample * channels);
	flush_event = event_new();
	flush_event->dispatch = flush_frame;

	ioctl(sound_fd, SNDCTL_DSP_RESET, 0);
	memset(buffer, 0, FRAME_SIZE * sizeof(Sample));
	wrptr = buffer;
	frame_cycle_base = current_cycle;
	flush_event->at_cycle = frame_cycle_base + FRAME_CYCLES;
	event_queue(flush_event);
	lastsample = 0;
	return 0;
failed:
	LOG_ERROR("Failed to initialiase OSS audio driver\n");
	return 1;
}

static void shutdown(void) {
	LOG_DEBUG(2,"Shutting down OSS audio driver\n");
	event_free(flush_event);
	ioctl(sound_fd, SNDCTL_DSP_RESET, 0);
	close(sound_fd);
	free(buffer);
}

static void update(void) {
	Cycle elapsed_cycles = current_cycle - frame_cycle_base;
	Sample *fill_to;
	if (elapsed_cycles >= FRAME_CYCLES) {
		fill_to = buffer + FRAME_SIZE;
	} else {
		fill_to = buffer + (elapsed_cycles/(Cycle)SAMPLE_CYCLES);
	}
	while (wrptr < fill_to)
		*(wrptr++) = lastsample;
	if (!(PIA_1B.control_register & 0x08)) {
		/* Single-bit sound */
		lastsample = (PIA_1B.port_output & 0x02) ? 0x7c : 0;
	} else  {
		if (PIA_0B.control_register & 0x08) {
			/* Sound disabled */
			lastsample = 0;
		} else {
			/* DAC output */
			lastsample = (PIA_1A.port_output & 0xfc) >> 1;
		}
	}
}

static void flush_frame(void *context) {
	int8_t *source = buffer;
	Sample *fill_to = buffer + FRAME_SIZE;
	(void)context;
	while (wrptr < fill_to)
		*(wrptr++) = lastsample;
	frame_cycle_base += FRAME_CYCLES;
	flush_event->at_cycle = frame_cycle_base + FRAME_CYCLES;
	event_queue(flush_event);
	wrptr = buffer;
	if (noratelimit)
		return;
	/* Convert buffer and write to device */
	if (format == AFMT_S8) {
		int8_t *dest = convbuf;
		int8_t tmp;
		int i, j;
		for (i = FRAME_SIZE; i; i--) {
			tmp = *(source++);
			for (j = 0; j < channels; j++)
				*(dest++) = tmp;
		}
		write(sound_fd, convbuf, FRAME_SIZE * channels);
		return;
	}
	if (format == AFMT_S16_NE) {
		int16_t *dest = (int16_t *)convbuf;
		int16_t tmp;
		int i, j;
		for (i = FRAME_SIZE; i; i--) {
			tmp = *(source++) << 8;
			for (j = 0; j < channels; j++)
				*(dest++) = tmp;
		}
		write(sound_fd, convbuf, FRAME_SIZE * channels * 2);
		return;
	}
}
