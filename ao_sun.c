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

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/audioio.h>
#include <sys/stropts.h>

#include "types.h"
#include "events.h"
#include "logging.h"
#include "machine.h"
#include "module.h"
#include "xroar.h"

static int init(void);
static void shutdown(void);
static void update(int value);

static void flush_frame(void);
static event_t *flush_event;

SoundModule sound_sun_module = {
	{ "sun", "Sun audio",
	  init, 0, shutdown },
	update
};

typedef uint8_t Sample;  /* 8-bit mono */

#define SAMPLE_RATE 22050
#define CHANNELS 1
#define FRAME_SIZE 512
#define SAMPLE_CYCLES (OSCILLATOR_RATE / SAMPLE_RATE)
#define FRAME_CYCLES ((OSCILLATOR_RATE / SAMPLE_RATE) * FRAME_SIZE)

static int sound_fd;
static unsigned int channels = CHANNELS;
static uint_t samples_written;

static Cycle frame_cycle_base;
static int frame_cycle;
static Sample *buffer;
static Sample *wrptr;
static Sample lastsample;

static int init(void) {
	unsigned int rate = SAMPLE_RATE;
	audio_info_t device_info;
	const char *device = "/dev/audio";

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
	LOG_DEBUG(2, "Set up audio device at %dHz, %d channels.\n", rate, channels);
	buffer = (Sample *)malloc(FRAME_SIZE * sizeof(Sample));
	flush_event = event_new();
	flush_event->dispatch = flush_frame;

	memset(buffer, 0x80, FRAME_SIZE);
	wrptr = buffer;
	frame_cycle_base = current_cycle;
	frame_cycle = 0;
	flush_event->at_cycle = frame_cycle_base + FRAME_CYCLES;
	event_queue(&MACHINE_EVENT_LIST, flush_event);
	lastsample = 0x00;
	ioctl(sound_fd, I_FLUSH, FLUSHW);
	ioctl(sound_fd, AUDIO_GETINFO, &device_info);
	samples_written = device_info.play.samples;
	return 0;
}

static void shutdown(void) {
	LOG_DEBUG(2,"Shutting down Sun audio driver\n");
	event_free(flush_event);
	ioctl(sound_fd, I_FLUSH, FLUSHW);
	close(sound_fd);
	free(buffer);
}

static void update(int value) {
	int elapsed_cycles = current_cycle - frame_cycle_base;
	if (elapsed_cycles >= FRAME_CYCLES) {
		elapsed_cycles = FRAME_CYCLES;
	}
	while (frame_cycle < elapsed_cycles) {
		*(wrptr++) = lastsample;
		frame_cycle += SAMPLE_CYCLES;
	}
	lastsample = value;
}

static void flush_frame(void) {
	audio_info_t device_info;
	int samples_left;
	Sample *fill_to = buffer + FRAME_SIZE;
	while (wrptr < fill_to)
		*(wrptr++) = lastsample;
	frame_cycle_base += FRAME_CYCLES;
	frame_cycle = 0;
	flush_event->at_cycle = frame_cycle_base + FRAME_CYCLES;
	event_queue(&MACHINE_EVENT_LIST, flush_event);
	wrptr = buffer;
	ioctl(sound_fd, AUDIO_GETINFO, &device_info);
	if (xroar_noratelimit) {
		ioctl(sound_fd, I_FLUSH, FLUSHW);
		samples_written = device_info.play.samples;
		return;
	}
	samples_left = samples_written - device_info.play.samples;
	if (samples_left > FRAME_SIZE) {
		int sleep_ms = ((samples_left - FRAME_SIZE) * 1000)/SAMPLE_RATE;
		struct timespec elapsed, tv;
		int errcode;
		sleep_ms -= 10;
		elapsed.tv_sec = sleep_ms / 1000;
		elapsed.tv_nsec = (sleep_ms % 1000) * 1000000;
		do {
			errno = 0;
			tv.tv_sec = elapsed.tv_sec;
			tv.tv_nsec = elapsed.tv_nsec;
			errcode = nanosleep(&tv, &elapsed);
		} while ( errcode && (errno == EINTR) );
	}
	write(sound_fd, buffer, FRAME_SIZE);
	samples_written += FRAME_SIZE;
}
