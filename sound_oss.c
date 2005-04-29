/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2005  Ciaran Anscomb
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

#ifdef HAVE_OSS_AUDIO

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/soundcard.h>

#include "sound.h"
#include "pia.h"
#include "xroar.h"
#include "logging.h"
#include "types.h"

static int init(void);
static void shutdown(void);
static void reset(void);
static void update(void);

SoundModule sound_oss_module = {
	NULL,
	"oss",
	"OSS audio",
	init, shutdown, reset, update
};

typedef uint8_t Sample;  /* 8-bit mono */
typedef uint_fast8_t Sample_f;  /* Fastest for manipulating above */

#define SAMPLE_RATE 44100
#define CHANNELS 1
#define FORMAT AFMT_U8
#define FRAGMENTS 2
#define FRAME_SIZE 512
#define SAMPLE_CYCLES ((int)(OSCILLATOR_RATE / SAMPLE_RATE))
#define FRAME_CYCLES (SAMPLE_CYCLES * FRAME_SIZE)

static int sound_fd;
static int channels = CHANNELS;
static int format;

static Cycle frame_cycle_base;
static Sample *buffer;
static Sample *wrptr;
static Sample_f lastsample;

static uint8_t convbuf[FRAME_SIZE * 4];

static void sound_send_buffer(void);

static int init(void) {
	int rate = SAMPLE_RATE;
	int buffer_size = FRAME_SIZE;
	int num_fragments = FRAGMENTS;
	int tmp, fragment_param;
	const char *device = "/dev/dsp";

	LOG_DEBUG(2,"Initialising OSS audio driver\n");
	channels = CHANNELS;
	format = FORMAT;
	sound_fd = open(device, O_WRONLY);
	if (sound_fd == -1) {
		LOG_ERROR("Couldn't open audio device %s: %s!\n", device, strerror(errno));
		return 1;
	}
	fragment_param = 0;
	tmp = buffer_size;
	while (tmp > 1) {
		tmp >>= 1;
		fragment_param++;
	}
	fragment_param |= (num_fragments << 16);
	tmp = fragment_param;
	if (ioctl(sound_fd, SNDCTL_DSP_SETFRAGMENT, &fragment_param) == -1
			|| tmp != fragment_param) {
		LOG_ERROR("Couldn't set audio buffer size/fragmentation limit.\n");
		return 1;
	}
	channels--;
	tmp = channels;
	if (ioctl(sound_fd, SNDCTL_DSP_STEREO, &channels) == -1
			|| tmp != channels) {
		LOG_ERROR("Couldn't set desired (%d) audio channels.  Got %d.\n", tmp + 1, channels + 1);
		return 1;
	}
	channels++;
	if (ioctl(sound_fd, SNDCTL_DSP_SETFMT, &format) == -1) {
		LOG_ERROR("Couldn't set desired audio format.\n");
		return 1;
	}
	tmp = rate;
	if (ioctl(sound_fd, SNDCTL_DSP_SPEED, &rate) == -1
			|| tmp != rate) {
		LOG_ERROR("Couldn't set desired (%dHz) audio rate.  Got %dHz.\n", tmp, rate);
		return 1;
	}
	LOG_DEBUG(0,"Set up audio device at %dHz, %d channels.\nOSS buffering %d fragments of %d bytes.\n", rate, channels, num_fragments, buffer_size);
	if (format != FORMAT || channels != CHANNELS) {
		LOG_DEBUG(1,"Format needs conversion.\n");
	}
	buffer = (Sample *)malloc(FRAME_SIZE * sizeof(Sample));
	return 0;
}

static void shutdown(void) {
	LOG_DEBUG(2,"Shutting down OSS audio driver\n");
	close(sound_fd);
	free(buffer);
}

static void reset(void) {
	memset(buffer, 0x00, FRAME_SIZE * sizeof(Sample));
	frame_cycle_base = current_cycle;
	next_sound_update = frame_cycle_base + FRAME_CYCLES;
	wrptr = buffer;
	lastsample = 0;
}

static void update(void) {
	Cycle elapsed_cycles = current_cycle - frame_cycle_base;
	Sample_f fill_with;
	Sample *fill_to;
	if (!(PIA_1B.control_register & 0x08)) {
		/* Single-bit sound */
		fill_with = ((PIA_1B.port_output & 0x02) << 5) ^ 0x80;
	} else  {
		if (PIA_0B.control_register & 0x08) {
			/* Sound disabled */
			fill_with = 0x80;
		} else {
			/* DAC output */
			fill_with = ((PIA_1A.port_output & 0xfc) >> 1) ^ 0x80;
		}
	}
	if (elapsed_cycles >= (FRAME_CYCLES*2)) {
		LOG_ERROR("Eek!  Sound buffer overrun!\n");
		reset();
		return;
	} else {
		fill_to = buffer + (elapsed_cycles/(Cycle)SAMPLE_CYCLES);
	}
	while (wrptr < fill_to && wrptr < (buffer + FRAME_SIZE))
		*(wrptr++) = lastsample;
	if (elapsed_cycles >= FRAME_CYCLES) {
		sound_send_buffer();
		frame_cycle_base += FRAME_CYCLES;
		next_sound_update = frame_cycle_base + FRAME_CYCLES;
		wrptr -= FRAME_SIZE;
		fill_to -= FRAME_SIZE;
		while (wrptr < fill_to)
			*(wrptr++) = lastsample;
	}
	lastsample = fill_with;
	return;
}

static void sound_send_buffer(void) {
	uint8_t *source = buffer;
	uint8_t *dest = convbuf;
	uint8_t tmp;
	int i;
	if (format == AFMT_U8 && channels == 1) {
		write(sound_fd, buffer, FRAME_SIZE);
		return;
	}
	if (format == AFMT_U8 && channels == 2) {
		for (i = FRAME_SIZE; i; i--) {
			tmp = *(source++);
			*(dest++) = tmp;
			*(dest++) = tmp;
		}
		write(sound_fd, convbuf, FRAME_SIZE * 2);
		return;
	}
	if (format == AFMT_S16_LE) {
		for (i = FRAME_SIZE; i; i--) {
			tmp = *(source++)^0x80;
			*(dest++) = 0;
			*(dest++) = tmp;
			if (channels == 2) {
				*(dest++) = 0;
				*(dest++) = tmp;
			}
		}
		write(sound_fd, convbuf, FRAME_SIZE * channels * 2);
		return;
	}
	if (format == AFMT_S16_BE) {
		for (i = FRAME_SIZE; i; i--) {
			tmp = *(source++)^0x80;
			*(dest++) = tmp;
			*(dest++) = 0;
			if (channels == 2) {
				*(dest++) = tmp;
				*(dest++) = 0;
			}
		}
		write(sound_fd, convbuf, FRAME_SIZE * channels * 2);
		return;
	}
}

#endif  /* HAVE_OSS_AUDIO */
