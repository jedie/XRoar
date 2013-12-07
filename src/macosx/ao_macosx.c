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

/* Core Audio processes audio in a separate thread, using a callback to request
 * more data.  When the configured number of audio fragments (nfragments) is 1,
 * write directly into the buffer provided by Core Audio.  When nfragments > 1,
 * maintain a queue of fragment buffers; the callback takes the next filled
 * buffer from the queue and copies its data into place. */

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

static void *callback_buffer;
static _Bool shutting_down;

static unsigned nfragments;
static unsigned fragment_nbytes;

static pthread_mutex_t fragment_mutex;
static pthread_cond_t fragment_cv;
static void **fragment_buffer;
static unsigned fragment_queue_length;
static unsigned write_fragment;
static unsigned play_fragment;

static OSStatus callback(AudioDeviceID, const AudioTimeStamp *, const AudioBufferList *,
		const AudioTimeStamp *, AudioBufferList *, const AudioTimeStamp *, void *);
static OSStatus callback_1(AudioDeviceID, const AudioTimeStamp *, const AudioBufferList *,
		const AudioTimeStamp *, AudioBufferList *, const AudioTimeStamp *, void *);

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

	nfragments = 2;
	if (xroar_cfg.ao_fragments > 0 && xroar_cfg.ao_fragments <= 64)
		nfragments = xroar_cfg.ao_fragments;

	unsigned rate = deviceFormat.mSampleRate;
	unsigned nchannels = deviceFormat.mChannelsPerFrame;
	unsigned fragment_nframes;
	unsigned buffer_nframes;
	enum sound_fmt sample_fmt = SOUND_FMT_FLOAT;
	unsigned sample_nbytes = sizeof(float);
	unsigned frame_nbytes = nchannels * sample_nbytes;

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

	UInt32 prop_buf_size = fragment_nframes * frame_nbytes;
	propertySize = sizeof(prop_buf_size);
	propertyAddress.mSelector = kAudioDevicePropertyBufferSize;
	if (AudioObjectSetPropertyData(device, &propertyAddress, 0, NULL, propertySize, &prop_buf_size) != kAudioHardwareNoError)
		goto failed;
	fragment_nframes = prop_buf_size / frame_nbytes;

#ifdef MAC_OS_X_VERSION_10_5
	AudioDeviceCreateIOProcID(device, (nfragments == 1) ? callback_1 : callback, NULL, &aprocid);
#else
	AudioDeviceAddIOProc(device, (nfragments == 1) ? callback_1 : callback, NULL);
#endif

	buffer_nframes = fragment_nframes * nfragments;
	fragment_nbytes = fragment_nframes * nchannels * sample_nbytes;

	pthread_mutex_init(&fragment_mutex, NULL);
	pthread_cond_init(&fragment_cv, NULL);

	shutting_down = 0;
	fragment_queue_length = 0;
	write_fragment = 0;
	play_fragment = 0;
	callback_buffer = NULL;

	// allocate fragment buffers
	fragment_buffer = g_malloc(nfragments * sizeof(void *));
	if (nfragments > 1) {
		for (unsigned i = 0; i < nfragments; i++) {
			fragment_buffer[i] = g_malloc0(fragment_nbytes);
		}
	}

	AudioDeviceStart(device, (nfragments == 1) ? callback_1 : callback);

	if (nfragments == 1) {
		pthread_mutex_lock(&fragment_mutex);
		while (callback_buffer == NULL) {
			pthread_cond_wait(&fragment_cv, &fragment_mutex);
		}
		fragment_buffer[0] = callback_buffer;
		callback_buffer = NULL;
		pthread_mutex_unlock(&fragment_mutex);
	}

	sound_init(fragment_buffer[0], sample_fmt, rate, nchannels, fragment_nframes);
	LOG_DEBUG(1, "\t%u frags * %d frames/frag = %d frames buffer (%.1fms)\n", nfragments, fragment_nframes, buffer_nframes, (float)(buffer_nframes * 1000) / rate);

	return 1;
failed:
	return 0;
}

static void shutdown(void) {
	shutting_down = 1;

	// unblock audio thread
	pthread_mutex_lock(&fragment_mutex);
	fragment_queue_length = 1;
	pthread_cond_signal(&fragment_cv);
	pthread_mutex_unlock(&fragment_mutex);

	AudioDeviceStop(device, (nfragments == 1) ? callback_1 : callback);
#ifdef MAC_OS_X_VERSION_10_5
	AudioDeviceDestroyIOProcID(device, aprocid);
#else
	AudioDeviceRemoveIOProc(device, (nfragments == 1) ? callback_1 : callback);
#endif

	pthread_mutex_destroy(&fragment_mutex);
	pthread_cond_destroy(&fragment_cv);

	if (nfragments > 1) {
		for (unsigned i = 0; i < nfragments; i++) {
			g_free(fragment_buffer[i]);
		}
	}

	g_free(fragment_buffer);
}

static void *write_buffer(void *buffer) {
	(void)buffer;

	pthread_mutex_lock(&fragment_mutex);

	/* For nfragments == 1, a non-NULL buffer means we've finished writing
	 * to the buffer provided by the callback.  Otherwise, one fragment
	 * buffer is now full.  Either way, signal the callback in case it is
	 * waiting for data to be available. */

	if (buffer) {
		write_fragment = (write_fragment + 1) % nfragments;
		fragment_queue_length++;
		pthread_cond_signal(&fragment_cv);
	}

	if (xroar_noratelimit) {
		pthread_mutex_unlock(&fragment_mutex);
		return NULL;
	}

	if (nfragments == 1) {
		// for nfragments == 1, wait for callback to send buffer
		while (callback_buffer == NULL)
			pthread_cond_wait(&fragment_cv, &fragment_mutex);
		fragment_buffer[0] = callback_buffer;
		callback_buffer = NULL;
	} else {
		// for nfragments > 1, wait until a fragment buffer is available
		while (fragment_queue_length == nfragments)
			pthread_cond_wait(&fragment_cv, &fragment_mutex);
	}

	pthread_mutex_unlock(&fragment_mutex);
	return fragment_buffer[write_fragment];
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

	if (shutting_down)
		return kAudioHardwareNoError;
	pthread_mutex_lock(&fragment_mutex);

	// wait until at least one fragment buffer is filled
	while (fragment_queue_length == 0)
		pthread_cond_wait(&fragment_cv, &fragment_mutex);

	// copy it to callback buffer
	memcpy(outOutputData->mBuffers[0].mData, fragment_buffer[play_fragment], fragment_nbytes);
	play_fragment = (play_fragment + 1) % nfragments;

	// signal main thread that a fragment buffer is available
	fragment_queue_length--;
	pthread_cond_signal(&fragment_cv);

	pthread_mutex_unlock(&fragment_mutex);
	return kAudioHardwareNoError;
}

static OSStatus callback_1(AudioDeviceID inDevice, const AudioTimeStamp *inNow,
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

	if (shutting_down)
		return kAudioHardwareNoError;
	pthread_mutex_lock(&fragment_mutex);

	// pass callback buffer to main thread
	callback_buffer = outOutputData->mBuffers[0].mData;
	pthread_cond_signal(&fragment_cv);

	// wait until main thread signals filled buffer
	while (fragment_queue_length == 0)
		pthread_cond_wait(&fragment_cv, &fragment_mutex);

	// set to 0 so next callback will wait
	fragment_queue_length = 0;

	pthread_mutex_unlock(&fragment_mutex);
	return kAudioHardwareNoError;
}
