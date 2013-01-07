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

#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <CoreAudio/AudioHardware.h>

#include "logging.h"
#include "machine.h"
#include "module.h"
#include "sound.h"
#include "xroar.h"

static int init(void);
static void shutdown(void);
static void flush_frame(void *buffer);

SoundModule sound_macosx_module = {
	.common = { .name = "macosx", .description = "Mac OS X audio",
		    .init = init, .shutdown = shutdown },
	.flush_frame = flush_frame,
};

static AudioObjectID device;
#ifdef MAC_OS_X_VERSION_10_5
static AudioDeviceIOProcID aprocid;
#endif
static float *buffer;
static int buffer_size;
static UInt32 buffer_bytes;
static pthread_mutex_t halt_mutex;
static pthread_cond_t halt_cv;
static int ready;

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

	int sample_rate = deviceFormat.mSampleRate;
	int channels = deviceFormat.mChannelsPerFrame;

	int frame_size;
	if (xroar_opt_ao_buffer_ms > 0) {
		frame_size = (sample_rate * xroar_opt_ao_buffer_ms) / 1000;
	} else if (xroar_opt_ao_buffer_samples > 0) {
		frame_size = xroar_opt_ao_buffer_samples;
	} else {
		frame_size = 1024;
	}

	buffer_size = frame_size * channels;
	buffer_bytes = buffer_size * sizeof(float);
	propertySize = sizeof(buffer_bytes);
	propertyAddress.mSelector = kAudioDevicePropertyBufferSize;
	if (AudioObjectSetPropertyData(device, &propertyAddress, 0, NULL, propertySize, &buffer_bytes) != kAudioHardwareNoError)
		goto failed;
	buffer_size = buffer_bytes / sizeof(float);
	frame_size = buffer_size / channels;
	buffer = sound_init(sample_rate, channels, SOUND_FMT_FLOAT, frame_size);
	LOG_DEBUG(2, "\t%dms (%d samples) buffer\n", (frame_size * 1000) / sample_rate, frame_size);

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

static void flush_frame(void *fbuffer) {
	(void)fbuffer;
	if (xroar_noratelimit)
		return;
	pthread_mutex_lock(&halt_mutex);
	ready = 1;
	while (ready) {
		pthread_cond_wait(&halt_cv, &halt_mutex);
	}
	pthread_mutex_unlock(&halt_mutex);
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
		memcpy(dest, buffer, buffer_bytes);
	} else {
		sound_render_silence(dest, buffer_size);
	}
	pthread_mutex_lock(&halt_mutex);
	ready = 0;
	pthread_cond_signal(&halt_cv);
	pthread_mutex_unlock(&halt_mutex);
	return kAudioHardwareNoError;
}
