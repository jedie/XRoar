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

#include <alsa/asoundlib.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "events.h"
#include "logging.h"
#include "machine.h"
#include "module.h"
#include "sound.h"
#include "xroar.h"

static int init(void);
static void shutdown(void);
static void flush_frame(void *buffer);

SoundModule sound_alsa_module = {
	.common = { .name = "alsa", .description = "ALSA audio",
		    .init = init, .shutdown = shutdown },
	.flush_frame = flush_frame,
};

static unsigned int sample_rate;
static snd_pcm_t *pcm_handle;
static snd_pcm_uframes_t frame_size;

static int init(void) {
	int err;
	snd_pcm_hw_params_t *hw_params;
	snd_pcm_format_t format = SND_PCM_FORMAT_U8;
	unsigned int channels = 1;

	sample_rate = (xroar_opt_ao_rate > 0) ? xroar_opt_ao_rate : 44100;

	if ((err = snd_pcm_open(&pcm_handle, "default", SND_PCM_STREAM_PLAYBACK, 0)) < 0)
		goto failed;

	if ((err = snd_pcm_hw_params_malloc(&hw_params)) < 0)
		goto failed;

	if ((err = snd_pcm_hw_params_any(pcm_handle, hw_params)) < 0)
		goto failed;

	if ((err = snd_pcm_hw_params_set_access(pcm_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0)
		goto failed;

	if ((err = snd_pcm_hw_params_set_format(pcm_handle, hw_params, format)) < 0)
		goto failed;

	if ((err = snd_pcm_hw_params_set_rate_near(pcm_handle, hw_params, &sample_rate, 0)) < 0)
		goto failed;

	if ((err = snd_pcm_hw_params_set_channels(pcm_handle, hw_params, channels)) < 0)
		goto failed;

	if ((err = snd_pcm_hw_params_get_period_size(hw_params, &frame_size, NULL)) < 0)
		goto failed;

	snd_pcm_uframes_t buffer_size;
	if (xroar_opt_ao_buffer_ms > 0) {
		buffer_size = (sample_rate * xroar_opt_ao_buffer_ms) / 1000;
	} else if (xroar_opt_ao_buffer_samples > 0) {
		buffer_size = xroar_opt_ao_buffer_samples;
	} else {
		buffer_size = frame_size * 2;
	}

	snd_pcm_hw_params_set_buffer_size_near(pcm_handle, hw_params, &buffer_size);

	if ((err = snd_pcm_hw_params(pcm_handle, hw_params)) < 0)
		goto failed;

	snd_pcm_hw_params_free(hw_params);

	if ((err = snd_pcm_prepare(pcm_handle)) < 0)
		goto failed;

	int request_fmt;
	switch (format) {
		case SND_PCM_FORMAT_S8: request_fmt = SOUND_FMT_S8; break;
		case SND_PCM_FORMAT_U8: request_fmt = SOUND_FMT_U8; break;
		case SND_PCM_FORMAT_S16_LE: request_fmt = SOUND_FMT_S16_LE; break;
		case SND_PCM_FORMAT_S16_BE: request_fmt = SOUND_FMT_S16_BE; break;
		default:
			LOG_WARN("Unhandled audio format.");
			goto failed;
	}
	sound_init(sample_rate, 1, request_fmt, frame_size);
	LOG_DEBUG(2, "\t%ldms (%ld samples) buffer\n", (buffer_size * 1000) / sample_rate, buffer_size);

	/* snd_pcm_writei(pcm_handle, buffer, frame_size); */
	return 0;
failed:
	LOG_ERROR("Failed to initialise ALSA: %s\n", snd_strerror(err));
	return 1;
}

static void shutdown(void) {
	snd_pcm_close(pcm_handle);
}

static void flush_frame(void *buffer) {
	if (xroar_noratelimit)
		return;
	if (snd_pcm_writei(pcm_handle, buffer, frame_size) < 0) {
		snd_pcm_prepare(pcm_handle);
		snd_pcm_writei(pcm_handle, buffer, frame_size);
	}
}
