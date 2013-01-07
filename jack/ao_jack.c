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
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <jack/jack.h>

#include "logging.h"
#include "machine.h"
#include "module.h"
#include "sound.h"
#include "xroar.h"

static int init(void);
static void shutdown(void);
static void flush_frame(void *buffer);

static int jack_callback(jack_nframes_t nframes, void *arg);

SoundModule sound_jack_module = {
	.common = { .name = "jack", .description = "JACK audio",
		    .init = init, .shutdown = shutdown },
	.flush_frame = flush_frame,
};

static jack_client_t *client;
static int num_output_ports;  /* maximum of 2... */
static jack_port_t *output_port[2];
static float *buffer;

static pthread_mutex_t haltflag;

static int init(void) {
	const char **ports;
	int i;

	if ((client = jack_client_open("XRoar", 0, NULL)) == 0) {
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
	jack_nframes_t sample_rate = jack_get_sample_rate(client);
	jack_nframes_t buffer_size = jack_get_buffer_size(client);

	buffer = sound_init(sample_rate, 1, SOUND_FMT_FLOAT, buffer_size);
	LOG_DEBUG(2, "\t%dms (%d samples) buffer\n", (buffer_size * 1000) / sample_rate, buffer_size);

	pthread_mutex_init(&haltflag, NULL);

	return 0;
}

static void shutdown(void) {
	pthread_mutex_destroy(&haltflag);
	if (client)
		jack_client_close(client);
	client = NULL;
}

static void flush_frame(void *buffer) {
	(void)buffer;
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
		out = (float *)jack_port_get_buffer(output_port[i], nframes);
		memcpy(out, buffer, nframes * sizeof(float));
	}
	pthread_mutex_unlock(&haltflag);
	return 0;
}
