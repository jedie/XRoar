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

/* XXX
 *
 * This code is old and hasn't been tested for a while.  In particular, it
 * looks like some parts of it depend on single-channel audio - or at least
 * both channels being identical, which is not the case any more.
 *
 * Needs attention!
 */

#include "config.h"

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/audioio.h>
#include <sys/ioctl.h>
#include <sys/stropts.h>
#include <time.h>
#include <unistd.h>

#include "logging.h"
#include "machine.h"
#include "module.h"
#include "sound.h"
#include "xroar.h"

static _Bool init(void);
static void shutdown(void);
static void flush_frame(void *buffer);

SoundModule sound_sun_module = {
	.common = { .name = "sun", .description = "Sun audio",
		    .init = init, .shutdown = shutdown },
	.flush_frame = flush_frame,
};

typedef uint8_t Sample;  /* 8-bit mono */

static unsigned int rate;
static int sound_fd;
static uint_t samples_written;

static _Bool init(void) {
	const char *device = xroar_cfg.ao_device ? xroar_cfg.ao_device : "/dev/audio";
	audio_info_t device_info;

	sound_fd = open(device, O_WRONLY);
	if (sound_fd == -1) {
		LOG_ERROR("Couldn't open audio device %s: %s!\n", device, strerror(errno));
		return 0;
	}
	rate = (xroar_cfg.ao_rate > 0) ? xroar_cfg.ao_rate : 48000;
	AUDIO_INITINFO(&device_info);
	device_info.play.sample_rate = rate;
	device_info.play.channels = xroar_cfg.ao_channels;
	if (device_info.play.channels < 1 || device_info.play.channels > 2)
		device_info.play.channels = 2;
	device_info.play.precision = 8;
	device_info.play.encoding = AUDIO_ENCODING_LINEAR;
	if (ioctl(sound_fd, AUDIO_SETINFO, &device_info) < 0) {
		LOG_ERROR("Failed to configure audio device %s: %s",
				device, strerror(errno));
		return 0;
	}
	if (device_info.play.encoding != AUDIO_ENCODING_LINEAR) {
		LOG_ERROR("Couldn't set desired audio format.\n");
		return 0;
	}

	int frame_nbytes;
	if (xroar_cfg.ao_buffer_ms > 0) {
		frame_nbytes = (rate * xroar_cfg.ao_buffer_ms) / 1000;
	} else if (xroar_cfg.ao_buffer_nframes > 0) {
		frame_nbytes = xroar_cfg.ao_buffer_nframes;
	} else {
		frame_nbytes = 1024;
	}

	sound_init(device_info.play.sample_rate, device_info.play.channels, SOUND_FMT_U8, frame_nbytes);
	LOG_DEBUG(2, "\t%dms (%d frames) buffer\n", (frame_nbytes * 1000) / device_info.play.sample_rate, frame_nbytes);

	ioctl(sound_fd, I_FLUSH, FLUSHW);
	ioctl(sound_fd, AUDIO_GETINFO, &device_info);
	samples_written = device_info.play.samples;
	return 1;
}

static void shutdown(void) {
	ioctl(sound_fd, I_FLUSH, FLUSHW);
	close(sound_fd);
}

static void flush_frame(void *buffer) {
	audio_info_t device_info;
	int samples_left;
	ioctl(sound_fd, AUDIO_GETINFO, &device_info);
	if (xroar_noratelimit) {
		ioctl(sound_fd, I_FLUSH, FLUSHW);
		samples_written = device_info.play.samples;
		return;
	}
	samples_left = samples_written - device_info.play.samples;
	if (samples_left > frame_nbytes) {
		int sleep_ms = ((samples_left - frame_nbytes) * 1000) / rate;
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
	write(sound_fd, buffer, frame_nbytes);
	samples_written += frame_nbytes;
}
