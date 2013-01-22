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

#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <CoreAudio/AudioHardware.h>
#include "portalib/glib.h"

#include "logging.h"
#include "machine.h"
#include "module.h"
#include "sound.h"
#include "xroar.h"

static int init(void);
static void shutdown(void);
static void *write_buffer(void *buffer);

SoundModule sound_macosx_module = {
	.common = { .name = "macosx", .description = "Mac OS X audio",
		    .init = init, .shutdown = shutdown },
	.write_buffer = write_buffer,
};

static AudioObjectID device;
#ifdef MAC_OS_X_VERSION_10_5
static AudioDeviceIOProcID aprocid;
#endif
static float *audio_buffer;
static unsigned buffer_nsamples;
static UInt32 buffer_size;
static pthread_mutex_t halt_mutex;
static pthread_cond_t halt_cv;
static _Bool ready;

static OSStatus callback(AudioDeviceID inDevice, const AudioTimeStamp *inNow,
		const AudioBufferList *inInputData,
		const AudioTimeStamp *inInputTime,
		AudioBufferList *outOutputData,
		const AudioTimeStamp *inOutputTime, void *defptr);

static int init(void) {
	AudioObjectPropertyAddress propertyAddress;
	AudioStreamBasicDescription deviceFormat;
	UInt32 propertySize;

	propertyAddress.mSelector = kAudioHardwarePropertyDefaultOutputDevice;
	propertyAddress.mScope = kAudioObjectPropertyScopeGlobal;
	propertyAddress.mElement = kAudioObjectPropertyElementMaster;

	propertySize = sizeof(device);
	if (AudioObjectGetPropertyData(kAudioObjectSystemObject, &propertyAddress, 0, NULL, &propertySize, &device) != noErr)
		goto failed;

	propertySize = sizeof(deviceFormat);
	propertyAddress.mSelector = kAudioDevicePropertyStreamFormat;
	propertyAddress.mScope = kAudioDevicePropertyScopeOutput;
	propertyAddress.mElement = 0;
	if (AudioObjectGetPropertyData(device, &propertyAddress, 0, NULL, &propertySize, &deviceFormat) != noErr)
		goto failed;

	if (deviceFormat.mFormatID != kAudioFormatLinearPCM)
		goto failed;
	if (!(deviceFormat.mFormatFlags & kLinearPCMFormatFlagIsFloat))
		goto failed;

	unsigned int sample_rate = deviceFormat.mSampleRate;
	unsigned int buffer_nchannels = deviceFormat.mChannelsPerFrame;

	unsigned int buffer_nframes;
	if (xroar_opt_ao_buffer_ms > 0) {
		buffer_nframes = (sample_rate * xroar_opt_ao_buffer_ms) / 1000;
	} else if (xroar_opt_ao_buffer_samples > 0) {
		buffer_nframes = xroar_opt_ao_buffer_samples;
	} else {
		buffer_nframes = 1024;
	}

	buffer_nsamples = buffer_nframes * buffer_nchannels;
	buffer_size = buffer_nsamples * sizeof(float);
	propertySize = sizeof(buffer_size);
	propertyAddress.mSelector = kAudioDevicePropertyBufferSize;
	if (AudioObjectSetPropertyData(device, &propertyAddress, 0, NULL, propertySize, &buffer_size) != kAudioHardwareNoError)
		goto failed;
	buffer_nsamples = buffer_size / sizeof(float);
	buffer_nframes = buffer_nsamples / buffer_nchannels;
	audio_buffer = g_malloc(buffer_size);
	sound_init(audio_buffer, SOUND_FMT_FLOAT, sample_rate, buffer_nchannels, buffer_nframes);
	LOG_DEBUG(2, "\t%dms (%d samples) buffer\n", (buffer_nframes * 1000) / sample_rate, buffer_nframes);

	pthread_mutex_init(&halt_mutex, NULL);
	pthread_cond_init(&halt_cv, NULL);
	ready = 0;
#ifdef MAC_OS_X_VERSION_10_5
	AudioDeviceCreateIOProcID(device, callback, NULL, &aprocid);
#else
	AudioDeviceAddIOProc(device, callback, NULL);
#endif
	AudioDeviceStart(device, callback);

	return 0;
failed:
	return 1;
}

static void shutdown(void) {
#ifdef MAC_OS_X_VERSION_10_5
	AudioDeviceDestroyIOProcID(device, aprocid);
#else
	AudioDeviceRemoveIOProc(device, callback);
#endif
	AudioDeviceStop(device, callback);
	pthread_mutex_destroy(&halt_mutex);
	pthread_cond_destroy(&halt_cv);
}

static void *write_buffer(void *buffer) {
	(void)buffer;
	if (xroar_noratelimit)
		return audio_buffer;
	pthread_mutex_lock(&halt_mutex);
	ready = 1;
	while (ready) {
		pthread_cond_wait(&halt_cv, &halt_mutex);
	}
	pthread_mutex_unlock(&halt_mutex);
	return audio_buffer;
}

static OSStatus callback(AudioDeviceID inDevice, const AudioTimeStamp *inNow,
		const AudioBufferList *inInputData,
		const AudioTimeStamp *inInputTime,
		AudioBufferList *outOutputData,
		const AudioTimeStamp *inOutputTime, void *defptr) {
	(void)inDevice;      /* unused */
	(void)inNow;         /* unused */
	(void)inInputData;   /* unused */
	(void)inInputTime;   /* unused */
	(void)inOutputTime;  /* unused */
	(void)defptr;        /* unused */
	float *dest = (float *)outOutputData->mBuffers[0].mData;
	if (ready) {
		memcpy(dest, audio_buffer, buffer_size);
	} else {
		sound_render_silence(dest, buffer_nsamples);
	}
	pthread_mutex_lock(&halt_mutex);
	ready = 0;
	pthread_cond_signal(&halt_cv);
	pthread_mutex_unlock(&halt_mutex);
	return kAudioHardwareNoError;
}
