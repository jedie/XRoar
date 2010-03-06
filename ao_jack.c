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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <jack/jack.h>
#include <pthread.h>

#include "types.h"
#include "events.h"
#include "logging.h"
#include "machine.h"
#include "module.h"
#include "types.h"
#include "xroar.h"

static int init(void);
static void shutdown(void);
static void update(int value);

static void flush_frame(void);
static int jack_callback(jack_nframes_t nframes, void *arg);

SoundModule sound_jack_module = {
	{ "jack", "JACK audio",
	  init, 0, shutdown },
	update
};

typedef jack_default_audio_sample_t Sample;

static jack_nframes_t sample_rate;
static jack_nframes_t frame_size;
static Cycle sample_cycles;
static Cycle frame_cycles;
static int frame_cycle;

static jack_client_t *client;
static int num_output_ports;  /* maximum of 2... */
static jack_port_t *output_port[2];

static Cycle frame_cycle_base;
static Sample *buffer = NULL;
static Sample *wrptr;
static Sample lastsample;
static pthread_mutex_t haltflag;

static event_t *flush_event;

static int init(void) {
	const char **ports;
	int i;

	LOG_DEBUG(2,"Initialising JACK audio driver\n");
	if ((client = jack_client_new("XRoar")) == 0) {
		LOG_ERROR("Initialisation failed: JACK server not running?\n");
		return 1;
	}
	jack_set_process_callback(client, jack_callback, 0);
	output_port[0] = jack_port_register(client, "output0", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
	output_port[1] = jack_port_register(client, "output1", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
	if (jack_activate(client)) {
		LOG_ERROR("Initialisation failed: Cannot activate client\n");
		jack_client_close(client);
		return 1;
	}
	if ((ports = jack_get_ports(client, NULL, NULL, JackPortIsPhysical|JackPortIsInput)) == NULL) {
		LOG_ERROR("Cannot find any physical playback ports\n");
		jack_client_close(client);
		return 1;
	}
	/* connect up to 2 ports (stereo output) */
	for (i = 0; ports[i] && i < 2; i++) {
		if (jack_connect(client, jack_port_name(output_port[i]), ports[i])) {
			LOG_ERROR("Cannot connect output ports\n");
			free(ports);
			jack_client_close(client);
			return 1;
		}
	}
	free(ports);
	num_output_ports = i;
	sample_rate = jack_get_sample_rate(client);
	sample_cycles = OSCILLATOR_RATE / sample_rate;
	frame_size = jack_get_buffer_size(client);
	frame_cycles = sample_cycles * frame_size;
	frame_cycle = 0;
	buffer = malloc(frame_size * sizeof(Sample));
	pthread_mutex_init(&haltflag, NULL);
	flush_event = event_new();
	flush_event->dispatch = flush_frame;

	memset(buffer, 0, frame_size * sizeof(Sample));
	wrptr = buffer;
	frame_cycle_base = current_cycle;
	flush_event->at_cycle = frame_cycle_base + frame_cycles;
	event_queue(&MACHINE_EVENT_LIST, flush_event);
	lastsample = 0.;
	return 0;
}

static void shutdown(void) {
	LOG_DEBUG(2,"Shutting down JACK audio driver\n");
	pthread_mutex_destroy(&haltflag);
	event_free(flush_event);
	if (client)
		jack_client_close(client);
	client = NULL;
	if (buffer)
		free(buffer);
	buffer = NULL;
}

static void update(int value) {
	int elapsed_cycles = current_cycle - frame_cycle_base;
	if (elapsed_cycles >= frame_cycles) {
		elapsed_cycles = frame_cycles;
	}
	while (frame_cycle < elapsed_cycles) {
		*(wrptr++) = lastsample;
		frame_cycle += sample_cycles;
	}
	lastsample = (Sample)(value - 64) / 150.;
}

static void flush_frame(void) {
	Sample *fill_to = buffer + frame_size;
	while (wrptr < fill_to)
		*(wrptr++) = lastsample;
	frame_cycle_base += frame_cycles;
	frame_cycle = 0;
	flush_event->at_cycle = frame_cycle_base + frame_cycles;
	event_queue(&MACHINE_EVENT_LIST, flush_event);
	wrptr = buffer;
	if (xroar_noratelimit)
		return;
	pthread_mutex_lock(&haltflag);
	pthread_mutex_lock(&haltflag);
	pthread_mutex_unlock(&haltflag);
}

static int jack_callback(jack_nframes_t nframes, void *arg) {
	int i;
	jack_default_audio_sample_t *out;
	(void)arg;  /* unused */
	for (i = 0; i < num_output_ports; i++) {
		out = (jack_default_audio_sample_t *)jack_port_get_buffer(output_port[i], nframes);
		memcpy(out, buffer, sizeof(Sample) * nframes);
	}
	pthread_mutex_unlock(&haltflag);
	return 0;
}
