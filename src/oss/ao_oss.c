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

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/soundcard.h>
#include <unistd.h>

#include "pl_glib.h"

#include "logging.h"
#include "machine.h"
#include "module.h"
#include "sound.h"
#include "xroar.h"

static _Bool init(void);
static void shutdown(void);
static void *write_buffer(void *buffer);

SoundModule sound_oss_module = {
	.common = { .name = "oss", .description = "OSS audio",
		    .init = init, .shutdown = shutdown },
	.write_buffer = write_buffer,
};

static int sound_fd;
static int fragment_size;
static void *audio_buffer;

static _Bool init(void) {
	const char *device = xroar_cfg.ao_device ? xroar_cfg.ao_device : "/dev/dsp";
	int fragment_param, tmp;

	sound_fd = open(device, O_WRONLY);
	if (sound_fd == -1)
		goto failed;
	/* The order these ioctls are tried (format, channels, sample rate)
	 * is important: OSS docs say setting format can affect channels and
	 * sample rate, and setting channels can affect sample rate. */
	/* Set audio format.  Only support AFMT_S8 (signed 8-bit) and
	 * AFMT_S16_NE (signed 16-bit, host-endian) */
	int format;
	int bytes_per_sample;
	if (ioctl(sound_fd, SNDCTL_DSP_GETFMTS, &format) == -1)
		goto failed;
	if ((format & (AFMT_S8 | AFMT_S16_NE)) == 0) {
		LOG_ERROR("No desired audio formats supported by device\n");
		goto failed;
	}
	if (format & AFMT_S16_NE) {
		format = AFMT_S16_NE;
		bytes_per_sample = 2;
	} else {
		format = AFMT_S8;
		bytes_per_sample = 1;
	}
	if (ioctl(sound_fd, SNDCTL_DSP_SETFMT, &format) == -1)
		goto failed;
	/* Set device to stereo if possible */
	int channels = 1;
	if (ioctl(sound_fd, SNDCTL_DSP_STEREO, &channels) == -1)
		goto failed;
	channels++;

	unsigned int sample_rate;
	/* Attempt to set sample_rate, but live with whatever we get */
	sample_rate = (xroar_cfg.ao_rate > 0) ? xroar_cfg.ao_rate : 44100;
	if (ioctl(sound_fd, SNDCTL_DSP_SPEED, &sample_rate) == -1)
		goto failed;

	int fragments = 2;
	int buffer_samples;
	if (xroar_cfg.ao_buffer_ms > 0) {
		buffer_samples = (sample_rate * xroar_cfg.ao_buffer_ms) / 1000;
	} else if (xroar_cfg.ao_buffer_samples > 0) {
		buffer_samples = xroar_cfg.ao_buffer_samples;
	} else {
		buffer_samples = 1024;
	}
	int fragment_samples = buffer_samples / fragments;
	fragment_size = fragment_samples * bytes_per_sample * channels;
	while (fragment_size > 65536) {
		fragment_size >>= 1;
		fragment_samples >>= 1;
		fragments *= 2;
		if (fragment_samples < 1 || fragments > 0x7fff) {
			LOG_ERROR("Couldn't determine buffer size\n");
			goto failed;
		}
	}

	tmp = fragment_size;
	fragment_param = 0;
	while (tmp > 1) {
		tmp >>= 1;
		fragment_param++;
	}
	fragment_param |= (fragments << 16);
	tmp = fragment_param;
	if (ioctl(sound_fd, SNDCTL_DSP_SETFRAGMENT, &fragment_param) == -1)
		goto failed;

	int delay = 0;
	if (ioctl(sound_fd, SNDCTL_DSP_GETODELAY, &delay) != -1) {
		delay /= (channels * bytes_per_sample);
	}
	if (delay <= 0) {
		audio_buf_info bi;
		if (ioctl(sound_fd, SNDCTL_DSP_GETOSPACE, &bi) != -1) {
			delay = bi.bytes / (channels * bytes_per_sample);
		}
	}

	enum sound_fmt buffer_fmt;
	if (format & AFMT_U8) buffer_fmt = SOUND_FMT_U8;
	else if (format & AFMT_S16_LE) buffer_fmt = SOUND_FMT_S16_LE;
	else if (format & AFMT_S16_BE) buffer_fmt = SOUND_FMT_S16_BE;
	else if (format & AFMT_S8) buffer_fmt = SOUND_FMT_S8;
	else {
		LOG_WARN("Unhandled audio format.");
		goto failed;
	}
	audio_buffer = g_malloc(fragment_size);
	sound_init(audio_buffer, buffer_fmt, sample_rate, channels, fragment_samples);
	LOG_DEBUG(2, "\t%dms (%d samples) buffer\n", (delay * 1000) / sample_rate, delay);

	if (tmp != fragment_param)
		LOG_WARN("Couldn't set desired buffer parameters: sync to audio might not be ideal\n");

	ioctl(sound_fd, SNDCTL_DSP_RESET, 0);
	return 1;
failed:
	return 0;
}

static void shutdown(void) {
	ioctl(sound_fd, SNDCTL_DSP_RESET, 0);
	close(sound_fd);
	g_free(audio_buffer);
}

static void *write_buffer(void *buffer) {
	if (xroar_noratelimit)
		return buffer;
	int r = write(sound_fd, buffer, fragment_size);
	(void)r;
	return buffer;
}
