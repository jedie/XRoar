/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2005  Ciaran Anscomb
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

#ifdef HAVE_MACOSX_AUDIO

#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <CoreAudio/AudioHardware.h>
#include <pthread.h>

#include "types.h"
#include "events.h"
#include "sound.h"
#include "pia.h"
#include "xroar.h"
#include "logging.h"

static int init(void);
static void shutdown(void);
static void reset(void);
static void update(void);

SoundModule sound_macosx_module = {
	NULL,
	"macosx",
	"Mac OS X CoreAudio",
	init, shutdown, reset, update
};

typedef float Sample;

#define FRAME_SIZE 512
#define SAMPLE_CYCLES ((int)(OSCILLATOR_RATE / sample_rate))
#define FRAME_CYCLES (SAMPLE_CYCLES * FRAME_SIZE)

static AudioDeviceID device;
static int sample_rate;
static int channels;
static Cycle frame_cycle_base;
static Sample *buffer;
static Sample *wrptr;
static Sample lastsample;
static pthread_mutex_t haltflag;

static void flush_frame(void);
static event_t flush_event = {0, flush_frame, 0, NULL };

static OSStatus callback(AudioDeviceID inDevice, const AudioTimeStamp *inNow,
		const AudioBufferList *inInputData, 
		const AudioTimeStamp *inInputTime,
		AudioBufferList *outOutputData,
		const AudioTimeStamp *inOutputTime, void *defptr);

static int init(void) {
	AudioStreamBasicDescription deviceFormat;
	UInt32 buffer_bytes = FRAME_SIZE * sizeof(Sample);
	UInt32 count;
	LOG_DEBUG(2,"Initialising Mac OS X CoreAudio driver\n");
	count = sizeof(device);
	if (AudioHardwareGetProperty(
				kAudioHardwarePropertyDefaultOutputDevice,
				&count, (void *)&device
				) != kAudioHardwareNoError)
		goto failed;
	count = sizeof(deviceFormat);
	if (AudioDeviceGetProperty(device, 0, false,
				kAudioDevicePropertyStreamFormat, &count,
				&deviceFormat) != kAudioHardwareNoError)
		goto failed;
	if (deviceFormat.mFormatID != kAudioFormatLinearPCM)
		goto failed;
	if (!(deviceFormat.mFormatFlags & kLinearPCMFormatFlagIsFloat))
		goto failed;
	sample_rate = deviceFormat.mSampleRate;
	channels = deviceFormat.mChannelsPerFrame;
	buffer_bytes = FRAME_SIZE * sizeof(Sample) * channels;
	count = sizeof(buffer_bytes);
	if (AudioDeviceSetProperty(device, NULL, 0, false,
				kAudioDevicePropertyBufferSize, count,
				&buffer_bytes) != kAudioHardwareNoError)
		goto failed;
	buffer = malloc(FRAME_SIZE * sizeof(Sample) * channels);
	memset(buffer, 0, FRAME_SIZE * sizeof(Sample) * channels);
	pthread_mutex_init(&haltflag, NULL);
	AudioDeviceAddIOProc(device, callback, NULL);
	AudioDeviceStart(device, callback);
	flush_event.scheduled = 0;
	flush_event.next = NULL;
	return 0;
failed:
	LOG_ERROR("Failed to initialiase Mac OS X CoreAudio driver\n");
	return 1;
}

static void shutdown(void) {
	LOG_DEBUG(2,"Shutting down Mac OS X CoreAudio driver\n");
	pthread_mutex_destroy(&haltflag);
	free(buffer);
}

static void reset(void) {
	memset(buffer, 0, FRAME_SIZE * sizeof(Sample) * channels);
	wrptr = buffer;
	frame_cycle_base = current_cycle;
	flush_event.at_cycle = frame_cycle_base + FRAME_CYCLES;
	event_schedule(&flush_event);
	lastsample = 0;
}

static void update(void) {
	Cycle elapsed_cycles = current_cycle - frame_cycle_base;
	Sample fill_with;
	Sample *fill_to;
	if (elapsed_cycles >= FRAME_CYCLES) {
		fill_to = buffer + FRAME_SIZE;
		LOG_WARN("sound_macosx.c: Got to end of buffer\n");
	} else {
		fill_to = buffer + (elapsed_cycles/(Cycle)SAMPLE_CYCLES);
	}
	if (!(PIA_1B.control_register & 0x08)) {
		/* Single-bit sound */
		fill_with = ((Sample)((PIA_1B.port_output & 0x02) << 4) / 300.) - 0.5;
	} else  {
		if (PIA_0B.control_register & 0x08) {
			/* Sound disabled */
			fill_with = 0.;
		} else {
			/* DAC output */
			fill_with = ((Sample)(PIA_1A.port_output & 0xfc) / 300.) - 0.5;
		}
	}
	while (wrptr < fill_to)
		*(wrptr++) = lastsample;
	lastsample = fill_with;
}

void flush_frame(void) {
	Sample *fill_to = buffer + FRAME_SIZE;
	while (wrptr < fill_to)
		*(wrptr++) = lastsample;
	frame_cycle_base += FRAME_CYCLES;
	flush_event.at_cycle = frame_cycle_base + FRAME_CYCLES;
	event_schedule(&flush_event);
	wrptr = buffer;
	pthread_mutex_lock(&haltflag);
	pthread_mutex_lock(&haltflag);
	pthread_mutex_unlock(&haltflag);
}

static OSStatus callback(AudioDeviceID inDevice, const AudioTimeStamp *inNow,
		const AudioBufferList *inInputData, 
		const AudioTimeStamp *inInputTime,
		AudioBufferList *outOutputData,
		const AudioTimeStamp *inOutputTime, void *defptr) {
	int i, j;
	Sample *src, *dest;
	(void)inDevice;      /* unused */
	(void)inNow;         /* unused */
	(void)inInputData;   /* unused */
	(void)inInputTime;   /* unused */
	(void)inOutputTime;  /* unused */
	(void)defptr;        /* unused */
	src = buffer;
	dest = (Sample *)outOutputData->mBuffers[0].mData;
	for (i = 0; i < FRAME_SIZE; i++) {
		for (j = 0; j < channels; j++) {
			*(dest++) = *src;
		}
		src++;
	}
	pthread_mutex_unlock(&haltflag);
	return kAudioHardwareNoError;
}

#endif  /* HAVE_MACOSX_AUDIO */