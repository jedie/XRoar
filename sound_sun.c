/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2004  Ciaran Anscomb
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

#ifdef HAVE_SUN_AUDIO

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/audioio.h>
#include <sys/stropts.h>

#include "sound.h"
#include "pia.h"
#include "xroar.h"
#include "logging.h"
#include "types.h"

static int init(void);
static void shutdown(void);
static void reset(void);
static void update(void);

SoundModule sound_sun_module = {
	NULL,
	"sun",
	"Sun audio",
	init, shutdown, reset, update
};

typedef uint8_t Sample;  /* 8-bit mono */
typedef uint_fast8_t Sample_f;  /* Fastest for manipulating above */

#define SAMPLE_RATE 22050
#define CHANNELS 1
#define FRAME_SIZE 512
#define SAMPLE_CYCLES (OSCILLATOR_RATE / SAMPLE_RATE)
#define FRAME_CYCLES ((OSCILLATOR_RATE / SAMPLE_RATE) * FRAME_SIZE)

static int sound_fd;
static int channels = CHANNELS;
static uint_t samples_start;

static Cycle frame_cycle_base;
static Sample *buffer;
static Sample *wrptr;
static Sample_f lastsample;

static int init(void) {
	int rate = SAMPLE_RATE;
	audio_info_t device_info;
	char *device = "/dev/audio";

	LOG_DEBUG(2,"Initialising Sun audio driver\n");
	channels = CHANNELS;
	sound_fd = open(device, O_WRONLY);
	if (sound_fd == -1) {
		LOG_ERROR("Couldn't open audio device %s: %s!\n", device, strerror(errno));
		return 1;
	}
	AUDIO_INITINFO(&device_info);
	device_info.play.sample_rate = SAMPLE_RATE;
	device_info.play.channels = CHANNELS;
	device_info.play.precision = 8;
	device_info.play.encoding = AUDIO_ENCODING_LINEAR;
	if (ioctl(sound_fd, AUDIO_SETINFO, &device_info) < 0) {
		LOG_ERROR("Failed to configure audio device %s: %s",
				device, strerror(errno));
		return 1;
	}
	if (device_info.play.channels != channels) {
		LOG_ERROR("Couldn't set desired (%d) audio channels.  Got %d.\n", channels, device_info.play.channels);
		return 1;
	}
	if (device_info.play.encoding != AUDIO_ENCODING_LINEAR) {
		LOG_ERROR("Couldn't set desired audio format.\n");
		return 1;
	}
	if (device_info.play.sample_rate != rate) {
		LOG_ERROR("Couldn't set desired (%dHz) audio rate.  Got %dHz.\n", rate, device_info.play.sample_rate);
		return 1;
	}
	LOG_DEBUG(0,"Set up audio device at %dHz, %d channels.\n", rate, channels);
	buffer = (Sample *)malloc(FRAME_SIZE * sizeof(Sample));
	return 0;
}

static void shutdown(void) {
	LOG_DEBUG(2,"Shutting down Sun audio driver\n");
	close(sound_fd);
	free(buffer);
}

static void reset(void) {
	audio_info_t device_info;
	memset(buffer, 0x80, FRAME_SIZE);
	wrptr = buffer;
	frame_cycle_base = current_cycle;
	next_sound_update = frame_cycle_base + FRAME_CYCLES;
	lastsample = 0x00;
	ioctl(sound_fd, I_FLUSH, FLUSHW);
	ioctl(sound_fd, AUDIO_GETINFO, &device_info);
	samples_start = device_info.play.samples;
}

static void update(void) {
	Cycle elapsed_cycles = current_cycle - frame_cycle_base;
	Sample_f fill_with;
	Sample *fill_to;
	audio_info_t device_info;
	if (elapsed_cycles >= FRAME_CYCLES) {
		fill_to = buffer + FRAME_SIZE;
	} else {
		fill_to = buffer + (elapsed_cycles/(Cycle)SAMPLE_CYCLES);
	}
	if (!(PIA_1B.control_register & 0x08)) {
		/* Single-bit sound */
		fill_with = ((PIA_1B.port_output & 0x02) << 5);
		//fill_with = 0;
	} else  {
		if (PIA_0B.control_register & 0x08 || PIA_0A.control_register & 0x08) { 
			/* Sound disabled */
			fill_with = 0;
		} else {
			/* DAC output */
			fill_with = ((PIA_1A.port_output & 0xfc) >> 1);
			//fill_with = PIA_1A.port_output & 0xfc;
			//fill_with ^= 0x80;
		}
	}
	while (wrptr < fill_to)
		*(wrptr++) = fill_with;
	if ((fill_to - buffer) >= FRAME_SIZE) {
		frame_cycle_base += FRAME_CYCLES;
		next_sound_update = frame_cycle_base + FRAME_CYCLES;
		wrptr = buffer;
		do {
			ioctl(sound_fd, AUDIO_GETINFO, &device_info);
		} while ((int)(device_info.play.samples - samples_start) < 0);
		write(sound_fd, buffer, FRAME_SIZE);
		samples_start += FRAME_SIZE;
	}
	lastsample = fill_with;
	return;
}

#endif  /* HAVE_SUN_AUDIO */
