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
#include <unistd.h>

#include <alsa/asoundlib.h>

#include "pl_glib.h"

#include "events.h"
#include "logging.h"
#include "machine.h"
#include "module.h"
#include "sound.h"
#include "xroar.h"

static _Bool init(void);
static void shutdown(void);
static void *write_buffer(void *buffer);

SoundModule sound_alsa_module = {
	.common = { .name = "alsa", .description = "ALSA audio",
		    .init = init, .shutdown = shutdown },
	.write_buffer = write_buffer,
};

static unsigned rate;
static snd_pcm_t *pcm_handle;
static snd_pcm_uframes_t fragment_nframes;
static void *audio_buffer;

static _Bool init(void) {
	const char *device = xroar_cfg.ao_device ? xroar_cfg.ao_device : "default";
	int err;
	snd_pcm_hw_params_t *hw_params;
	snd_pcm_format_t format = SND_PCM_FORMAT_S16;
	unsigned nchannels = xroar_cfg.ao_channels;
	if (nchannels < 1 || nchannels > 2)
		nchannels = 2;

	rate = (xroar_cfg.ao_rate > 0) ? xroar_cfg.ao_rate : 48000;

	if ((err = snd_pcm_open(&pcm_handle, device, SND_PCM_STREAM_PLAYBACK, 0)) < 0)
		goto failed;

	if ((err = snd_pcm_hw_params_malloc(&hw_params)) < 0)
		goto failed;

	if ((err = snd_pcm_hw_params_any(pcm_handle, hw_params)) < 0)
		goto failed;

	if ((err = snd_pcm_hw_params_set_access(pcm_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0)
		goto failed;

	if ((err = snd_pcm_hw_params_set_format(pcm_handle, hw_params, format)) < 0)
		goto failed;

	if ((err = snd_pcm_hw_params_set_rate_near(pcm_handle, hw_params, &rate, 0)) < 0)
		goto failed;

	if ((err = snd_pcm_hw_params_set_channels_near(pcm_handle, hw_params, &nchannels)) < 0)
		goto failed;

	snd_pcm_uframes_t buffer_nframes = 0;
	fragment_nframes = 0;
	if (xroar_cfg.ao_fragment_ms > 0) {
		fragment_nframes = (rate * xroar_cfg.ao_fragment_ms) / 1000;
	} else if (xroar_cfg.ao_fragment_nframes > 0) {
		fragment_nframes = xroar_cfg.ao_fragment_nframes;
	}
	if (fragment_nframes > 0) {
		if ((err = snd_pcm_hw_params_set_period_size_near(pcm_handle, hw_params, &fragment_nframes, NULL)) < 0) {
			LOG_ERROR("ALSA: snd_pcm_hw_params_set_period_size_near() failed\n");
			goto failed;
		}
	} else {
		buffer_nframes = 1024;
	}

	unsigned nfragments = 0;
	int nfragments_dir;
	if (xroar_cfg.ao_fragments > 0) {
		nfragments = xroar_cfg.ao_fragments;
	}
	if (nfragments > 0) {
		if ((err = snd_pcm_hw_params_set_periods_near(pcm_handle, hw_params, &nfragments, NULL)) < 0) {
			LOG_ERROR("ALSA: snd_pcm_hw_params_set_periods_near() failed\n");
			goto failed;
		}
	}

	if (xroar_cfg.ao_buffer_ms > 0) {
		buffer_nframes = (rate * xroar_cfg.ao_buffer_ms) / 1000;
	} else if (xroar_cfg.ao_buffer_nframes > 0) {
		buffer_nframes = xroar_cfg.ao_buffer_nframes;
	}
	if (buffer_nframes > 0) {
		if ((err = snd_pcm_hw_params_set_buffer_size_near(pcm_handle, hw_params, &buffer_nframes)) < 0) {
			LOG_ERROR("ALSA: snd_pcm_hw_params_set_buffer_size_near() failed\n");
			goto failed;
		}
	}

	if ((err = snd_pcm_hw_params(pcm_handle, hw_params)) < 0) {
		LOG_ERROR("ALSA: snd_pcm_hw_params() failed\n");
		goto failed;
	}

	if (nfragments == 0) {
		if ((err = snd_pcm_hw_params_get_periods(hw_params, &nfragments, &nfragments_dir)) < 0) {
			LOG_ERROR("ALSA: snd_pcm_hw_params_get_periods() failed\n");
			goto failed;
		}
	}

	if (fragment_nframes == 0) {
		if ((err = snd_pcm_hw_params_get_period_size(hw_params, &fragment_nframes, NULL)) < 0) {
			LOG_ERROR("ALSA: snd_pcm_hw_params_get_period_size() failed\n");
			goto failed;
		}
	}

	if (buffer_nframes == 0) {
		if ((err = snd_pcm_hw_params_get_buffer_size(hw_params, &buffer_nframes)) < 0) {
			LOG_ERROR("ALSA: snd_pcm_hw_params_get_buffer_size() failed\n");
			goto failed;
		}
	}

	snd_pcm_hw_params_free(hw_params);

	if ((err = snd_pcm_prepare(pcm_handle)) < 0) {
		LOG_ERROR("ALSA: snd_pcm_prepare() failed\n");
		goto failed;
	}

	enum sound_fmt buffer_fmt;
	unsigned sample_nbytes;
	switch (format) {
		case SND_PCM_FORMAT_S8:
			buffer_fmt = SOUND_FMT_S8;
			sample_nbytes = 1;
			break;
		case SND_PCM_FORMAT_U8:
			buffer_fmt = SOUND_FMT_U8;
			sample_nbytes = 1;
			break;
		case SND_PCM_FORMAT_S16_LE:
			buffer_fmt = SOUND_FMT_S16_LE;
			sample_nbytes = 2;
			break;
		case SND_PCM_FORMAT_S16_BE:
			buffer_fmt = SOUND_FMT_S16_BE;
			sample_nbytes = 2;
			break;
		default:
			LOG_ERROR("Unhandled audio format.\n");
			goto failed;
	}

	unsigned buffer_size = fragment_nframes * nchannels * sample_nbytes;
	audio_buffer = g_malloc(buffer_size);
	sound_init(audio_buffer, buffer_fmt, rate, nchannels, fragment_nframes);
	LOG_DEBUG(1, "\t%u frags * %ld frames/frag = %ld frames buffer (%ldms)\n", nfragments, fragment_nframes, buffer_nframes, (buffer_nframes * 1000) / rate);

	/* snd_pcm_writei(pcm_handle, buffer, fragment_nframes); */
	return 1;
failed:
	LOG_ERROR("Failed to initialise ALSA: %s\n", snd_strerror(err));
	return 0;
}

static void shutdown(void) {
	snd_pcm_close(pcm_handle);
	g_free(audio_buffer);
}

static void *write_buffer(void *buffer) {
	if (xroar_noratelimit)
		return buffer;
	if (snd_pcm_writei(pcm_handle, buffer, fragment_nframes) < 0) {
		snd_pcm_prepare(pcm_handle);
		snd_pcm_writei(pcm_handle, buffer, fragment_nframes);
	}
	return buffer;
}
