/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2009  Ciaran Anscomb
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
#include <alsa/asoundlib.h>

#include "types.h"
#include "events.h"
#include "logging.h"
#include "machine.h"
#include "module.h"
#include "xroar.h"

static int init(void);
static void shutdown(void);
static void update(int value);

SoundModule sound_alsa_module = {
	{ "alsa", "ALSA audio",
	  init, 0, shutdown },
	update
};

typedef int8_t Sample;  /* 8-bit mono */

#define FRAGMENTS 2
#define FRAME_SIZE 512
#define SAMPLE_CYCLES ((int)(OSCILLATOR_RATE / sample_rate))
#define FRAME_CYCLES (SAMPLE_CYCLES * FRAME_SIZE)

static unsigned int sample_rate;
unsigned int bytes_per_sample;

static snd_pcm_t *pcm_handle;

static Cycle frame_cycle_base;
static Sample *buffer;
static Sample *wrptr;
static Sample lastsample;

static void flush_frame(void);
static event_t *flush_event;

static int init(void) {
	int err;
	snd_pcm_hw_params_t *hw_params;
	snd_pcm_uframes_t buffer_size = FRAME_SIZE * FRAGMENTS;
	snd_pcm_uframes_t period_size = FRAME_SIZE / 2;
	snd_pcm_format_t format = SND_PCM_FORMAT_U8;
	unsigned int channels = 1;

	LOG_DEBUG(2,"Initialising ALSA audio driver\n");

	sample_rate = 44100;

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

	snd_pcm_hw_params_set_buffer_size_near(pcm_handle, hw_params, &buffer_size);
	snd_pcm_hw_params_set_period_size_near(pcm_handle, hw_params, &period_size, NULL);

	if ((err = snd_pcm_hw_params(pcm_handle, hw_params)) < 0)
		goto failed;

	snd_pcm_hw_params_free(hw_params);

	if ((err = snd_pcm_prepare(pcm_handle)) < 0)
		goto failed;

	/* TODO: Need to abstract this logging out */
	LOG_DEBUG(2, "\t");
	switch (format) {
		case SND_PCM_FORMAT_S8: LOG_DEBUG(2, "8-bit signed, "); break;
		case SND_PCM_FORMAT_U8: LOG_DEBUG(2, "8-bit unsigned, "); break;
		case SND_PCM_FORMAT_S16_LE: LOG_DEBUG(2, "16-bit signed little-endian, "); break;
		case SND_PCM_FORMAT_S16_BE: LOG_DEBUG(2, "16-bit signed big-endian, "); break;
		case SND_PCM_FORMAT_U16_LE: LOG_DEBUG(2, "16-bit unsigned little-endian, "); break;
		case SND_PCM_FORMAT_U16_BE: LOG_DEBUG(2, "16-bit unsigned big-endian, "); break;
		default: LOG_DEBUG(2, "unknown, "); break;
	}
	switch (channels) {
		case 1: LOG_DEBUG(2, "mono, "); break;
		case 2: LOG_DEBUG(2, "stereo, "); break;
		default: LOG_DEBUG(2, "%d channel, ", channels); break;
	}
	LOG_DEBUG(2, "%dHz\n", sample_rate);

	buffer = malloc(FRAME_SIZE * sizeof(Sample));
	flush_event = event_new();
	flush_event->dispatch = flush_frame;

	memset(buffer, 0, FRAME_SIZE * sizeof(Sample));
	wrptr = buffer;
	frame_cycle_base = current_cycle;
	flush_event->at_cycle = frame_cycle_base + FRAME_CYCLES;
	event_queue(&MACHINE_EVENT_LIST, flush_event);
	lastsample = 0;
	snd_pcm_writei(pcm_handle, buffer, FRAME_SIZE);
	return 0;
failed:
	LOG_ERROR("Failed to initialiase ALSA audio driver\n");
	return 1;
}

static void shutdown(void) {
	LOG_DEBUG(2,"Shutting down ALSA audio driver\n");
	event_free(flush_event);
	snd_pcm_close(pcm_handle);
	free(buffer);
}

static void update(int value) {
	Cycle elapsed_cycles = current_cycle - frame_cycle_base;
	Sample *fill_to;
	if (elapsed_cycles >= FRAME_CYCLES) {
		fill_to = buffer + FRAME_SIZE;
	} else {
		fill_to = buffer + (elapsed_cycles/(Cycle)SAMPLE_CYCLES);
	}
	while (wrptr < fill_to)
		*(wrptr++) = lastsample;
	lastsample = value;
}

static void flush_frame(void) {
	int err;
	Sample *fill_to = buffer + FRAME_SIZE;
	while (wrptr < fill_to)
		*(wrptr++) = lastsample;
	frame_cycle_base += FRAME_CYCLES;
	flush_event->at_cycle = frame_cycle_base + FRAME_CYCLES;
	event_queue(&MACHINE_EVENT_LIST, flush_event);
	wrptr = buffer;
	if (xroar_noratelimit)
		return;
	if ((err = snd_pcm_writei(pcm_handle, buffer, FRAME_SIZE)) < 0) {
		snd_pcm_prepare(pcm_handle);
		snd_pcm_writei(pcm_handle, buffer, FRAME_SIZE);
	}
}
