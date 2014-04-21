/*  Copyright 2003-2014 Ciaran Anscomb
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

#include "xalloc.h"

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
static int fragment_nbytes;
static void *audio_buffer;

static _Bool init(void) {
	const char *device = "/dev/dsp";
	if (xroar_cfg.ao_device)
		device = xroar_cfg.ao_device;

	sound_fd = open(device, O_WRONLY);
	if (sound_fd == -1) {
		LOG_ERROR("OSS: failed to open device\n");
		goto failed;
	}

	/* The order these ioctls are tried (format, channels, rate) is
	 * important: OSS docs say setting format can affect channels and
	 * sample rate, and setting channels can affect rate. */

	// Find a supported format
	int desired_format;
	switch (xroar_cfg.ao_format) {
	case SOUND_FMT_U8:
		desired_format = AFMT_U8;
		break;
	case SOUND_FMT_S8:
		desired_format = AFMT_S8;
		break;
	case SOUND_FMT_S16_BE:
		desired_format = AFMT_S16_BE;
		break;
	case SOUND_FMT_S16_LE:
		desired_format = AFMT_S16_LE;
		break;
	case SOUND_FMT_S16_HE:
	default:
		desired_format = AFMT_S16_NE;
		break;
	case SOUND_FMT_S16_SE:
		if (AFMT_S16_NE == AFMT_S16_LE)
			desired_format = AFMT_S16_BE;
		else
			desired_format = AFMT_S16_LE;
		break;
	}
	enum sound_fmt buffer_fmt;
	int format;
	int bytes_per_sample;
	if (ioctl(sound_fd, SNDCTL_DSP_GETFMTS, &format) == -1) {
		LOG_ERROR("OSS: SNDCTL_DSP_GETFMTS failed\n");
		goto failed;
	}
	if ((format & (AFMT_U8 | AFMT_S8 | AFMT_S16_LE | AFMT_S16_BE)) == 0) {
		LOG_ERROR("No desired audio formats supported by device\n");
		goto failed;
	}
	// if desired_format is one of those returned, use it:
	if (format & desired_format) {
		format = desired_format;
	}
	if (format & AFMT_S16_NE) {
		format = AFMT_S16_NE;
		buffer_fmt = SOUND_FMT_S16_HE;
		bytes_per_sample = 2;
	} else if (format & AFMT_S16_BE) {
		format = AFMT_S16_BE;
		buffer_fmt = SOUND_FMT_S16_BE;
		bytes_per_sample = 2;
	} else if (format & AFMT_S16_LE) {
		format = AFMT_S16_LE;
		buffer_fmt = SOUND_FMT_S16_LE;
		bytes_per_sample = 2;
	} else if (format & AFMT_S8) {
		format = AFMT_S8;
		buffer_fmt = SOUND_FMT_S8;
		bytes_per_sample = 1;
	} else {
		format = AFMT_U8;
		buffer_fmt = SOUND_FMT_U8;
		bytes_per_sample = 1;
	}
	if (ioctl(sound_fd, SNDCTL_DSP_SETFMT, &format) == -1) {
		LOG_ERROR("OSS: SNDCTL_DSP_SETFMT failed\n");
		goto failed;
	}

	// Set stereo if desired
	int nchannels = xroar_cfg.ao_channels - 1;
	if (nchannels < 0 || nchannels > 1)
		nchannels = 1;
	if (ioctl(sound_fd, SNDCTL_DSP_STEREO, &nchannels) == -1) {
		LOG_ERROR("OSS: SNDCTL_DSP_STEREO failed\n");
		goto failed;
	}
	nchannels++;

	// Set rate
	unsigned rate = 48000;
	if (xroar_cfg.ao_rate > 0)
		rate = xroar_cfg.ao_rate;
	if (ioctl(sound_fd, SNDCTL_DSP_SPEED, &rate) == -1) {
		LOG_ERROR("OSS: SNDCTL_DSP_SPEED failed\n");
		goto failed;
	}

	/* Now calculate and set the fragment parameter.  If fragment size args
	 * not specified, calculate based on buffer size args. */

	int nfragments = 2;
	int buffer_nframes = 0;
	int fragment_nframes = 0;

	if (xroar_cfg.ao_fragments >= 2 && xroar_cfg.ao_fragments < 0x8000) {
		nfragments = xroar_cfg.ao_fragments;
	}

	if (xroar_cfg.ao_fragment_ms > 0) {
		fragment_nframes = (rate * xroar_cfg.ao_fragment_ms) / 1000;
	} else if (xroar_cfg.ao_fragment_nframes > 0) {
		fragment_nframes = xroar_cfg.ao_fragment_nframes;
	} else {
		if (xroar_cfg.ao_buffer_ms > 0) {
			buffer_nframes = (rate * xroar_cfg.ao_buffer_ms) / 1000;
		} else if (xroar_cfg.ao_buffer_nframes > 0) {
			buffer_nframes = xroar_cfg.ao_buffer_nframes;
		} else {
			buffer_nframes = 1024;
		}
		fragment_nframes = buffer_nframes / nfragments;
	}

	// frag size selector is 2^N:
	int fbytes = fragment_nframes * bytes_per_sample * nchannels;
	fragment_nbytes = 16;
	int frag_size_sel = 4;
	while (fragment_nbytes < fbytes && frag_size_sel < 30) {
		frag_size_sel++;
		fragment_nbytes <<= 1;
	}

	// now piece together the ioctl:
	int frag = (nfragments << 16) | frag_size_sel;
	if (ioctl(sound_fd, SNDCTL_DSP_SETFRAGMENT, &frag) == -1) {
		LOG_ERROR("OSS: SNDCTL_DSP_SETFRAGMENT failed\n");
		goto failed;
	}
	// ioctl may have modified frag, so extract new values:
	nfragments = (frag >> 16) & 0x7fff;
	frag_size_sel = frag & 0xffff;
	if (frag_size_sel > 30) {
		LOG_ERROR("OSS: returned fragment size too large\n");
		goto failed;
	}
	fragment_nbytes = 1 << frag_size_sel;
	fragment_nframes = fragment_nbytes / (bytes_per_sample * nchannels);
	buffer_nframes = fragment_nframes * nfragments;

	audio_buffer = xmalloc(fragment_nbytes);
	sound_init(audio_buffer, buffer_fmt, rate, nchannels, fragment_nframes);
	LOG_DEBUG(1, "\t%d frags * %d frames/frag = %d frames buffer (%.1fms)\n", nfragments, fragment_nframes, buffer_nframes, (float)(buffer_nframes * 1000) / rate);

	ioctl(sound_fd, SNDCTL_DSP_RESET, 0);
	return 1;
failed:
	if (sound_fd != -1)
		close(sound_fd);
	return 0;
}

static void shutdown(void) {
	ioctl(sound_fd, SNDCTL_DSP_RESET, 0);
	close(sound_fd);
	free(audio_buffer);
}

static void *write_buffer(void *buffer) {
	if (xroar_noratelimit)
		return buffer;
	int r = write(sound_fd, buffer, fragment_nbytes);
	(void)r;
	return buffer;
}
