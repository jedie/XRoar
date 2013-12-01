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

#include "pl_glib.h"

#include "logging.h"
#include "machine.h"
#include "module.h"
#include "sound.h"
#include "xroar.h"

static _Bool init(void);
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
static _Bool halt;
static _Bool shutting_down;

static pthread_mutex_t audio_buffer_mutex;
static pthread_cond_t audio_buffer_cv;
static pthread_mutex_t halt_mutex;
static pthread_cond_t halt_cv;

static OSStatus callback(AudioDeviceID inDevice, const AudioTimeStamp *inNow,
		const AudioBufferList *inInputData,
		const AudioTimeStamp *inInputTime,
		AudioBufferList *outOutputData,
		const AudioTimeStamp *inOutputTime, void *defptr);

static _Bool init(void) {
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

	/* More than one fragment not currently supported. */

	unsigned rate = deviceFormat.mSampleRate;
	unsigned nchannels = deviceFormat.mChannelsPerFrame;
	unsigned fragment_nframes = 1024;

	if (xroar_cfg.ao_fragment_ms > 0) {
		fragment_nframes = (rate * xroar_cfg.ao_fragment_ms) / 1000;
	} else if (xroar_cfg.ao_fragment_nframes > 0) {
		fragment_nframes = xroar_cfg.ao_fragment_nframes;
	} else if (xroar_cfg.ao_buffer_ms > 0) {
		fragment_nframes = (rate * xroar_cfg.ao_buffer_ms) / 1000;
	} else if (xroar_cfg.ao_buffer_nframes > 0) {
		fragment_nframes = xroar_cfg.ao_buffer_nframes;
	} else {
		fragment_nframes = 1024;
	}

	unsigned buffer_nsamples = fragment_nframes * nchannels;
	UInt32 buffer_nbytes = buffer_nsamples * sizeof(float);
	propertySize = sizeof(buffer_nbytes);
	propertyAddress.mSelector = kAudioDevicePropertyBufferSize;
	if (AudioObjectSetPropertyData(device, &propertyAddress, 0, NULL, propertySize, &buffer_nbytes) != kAudioHardwareNoError)
		goto failed;
	buffer_nsamples = buffer_nbytes / sizeof(float);
	fragment_nframes = buffer_nsamples / nchannels;

#ifdef MAC_OS_X_VERSION_10_5
	AudioDeviceCreateIOProcID(device, callback, NULL, &aprocid);
#else
	AudioDeviceAddIOProc(device, callback, NULL);
#endif

	pthread_mutex_init(&audio_buffer_mutex, NULL);
	pthread_cond_init(&audio_buffer_cv, NULL);
	pthread_mutex_init(&halt_mutex, NULL);
	pthread_cond_init(&halt_cv, NULL);

	audio_buffer = NULL;
	halt = 1;
	shutting_down = 0;

	pthread_mutex_lock(&audio_buffer_mutex);
	AudioDeviceStart(device, callback);
	// wait for initial buffer from audio thread
	while (!audio_buffer)
		pthread_cond_wait(&audio_buffer_cv, &audio_buffer_mutex);

	sound_init(audio_buffer, SOUND_FMT_FLOAT, rate, nchannels, fragment_nframes);
	audio_buffer = NULL;
	pthread_mutex_unlock(&audio_buffer_mutex);
	LOG_DEBUG(1, "\t%dms (%d samples) buffer\n", (fragment_nframes * 1000) / rate, fragment_nframes);

	return 1;
failed:
	return 0;
}

static void shutdown(void) {
	pthread_mutex_lock(&halt_mutex);
	halt = 0;
	shutting_down = 1;
	pthread_cond_signal(&halt_cv);
	pthread_mutex_unlock(&halt_mutex);

	AudioDeviceStop(device, callback);
#ifdef MAC_OS_X_VERSION_10_5
	AudioDeviceDestroyIOProcID(device, aprocid);
#else
	AudioDeviceRemoveIOProc(device, callback);
#endif

	pthread_mutex_destroy(&audio_buffer_mutex);
	pthread_cond_destroy(&audio_buffer_cv);
	pthread_mutex_destroy(&halt_mutex);
	pthread_cond_destroy(&halt_cv);
}

static void *write_buffer(void *buffer) {
	(void)buffer;
	// unblock audio thread
	pthread_mutex_lock(&halt_mutex);
	halt = 0;
	pthread_cond_signal(&halt_cv);
	pthread_mutex_unlock(&halt_mutex);

	if (xroar_noratelimit)
		return NULL;

	// wait for audio thread to pass a buffer pointer
	pthread_mutex_lock(&audio_buffer_mutex);
	while (!audio_buffer)
		pthread_cond_wait(&audio_buffer_cv, &audio_buffer_mutex);
	void *r = audio_buffer;
	audio_buffer = NULL;
	pthread_mutex_unlock(&audio_buffer_mutex);
	return r;
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

	if (!shutting_down) {
		pthread_mutex_lock(&halt_mutex);
		halt = 1;
	}

	// pass buffer pointer back to main thread
	pthread_mutex_lock(&audio_buffer_mutex);
	audio_buffer = (float *)outOutputData->mBuffers[0].mData;
	pthread_cond_signal(&audio_buffer_cv);
	pthread_mutex_unlock(&audio_buffer_mutex);

	// wait for main thread to be done with buffer
	if (!shutting_down) {
		while (halt)
			pthread_cond_wait(&halt_cv, &halt_mutex);
		pthread_mutex_unlock(&halt_mutex);
	}
	return kAudioHardwareNoError;
}
