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

#include <windows.h>

#include "logging.h"
#include "module.h"
#include "sound.h"
#include "xroar.h"

static int init(void);
static void _shutdown(void);
static void flush_frame(void *buffer);

SoundModule sound_windows32_module = {
	.common = { .name = "windows32", .description = "Windows audio",
		    .init = init, .shutdown = _shutdown },
	.flush_frame = flush_frame,
};

#define NUM_BUFFERS (3)

static HWAVEOUT device;
static HGLOBAL wavehdr_alloc[NUM_BUFFERS];
static LPWAVEHDR wavehdr_p[NUM_BUFFERS];
static HGLOBAL data_alloc[NUM_BUFFERS];
static LPSTR data_p[NUM_BUFFERS];
static int buffer_samples, buffer_size;
static DWORD cursor;
static int buffer_num;
static int sample_rate;

static int init(void) {
	sample_rate = (xroar_opt_ao_rate > 0) ? xroar_opt_ao_rate : 44100;

	if (xroar_opt_ao_buffer_ms > 0) {
		buffer_samples = (sample_rate * xroar_opt_ao_buffer_ms) / 1000;
	} else if (xroar_opt_ao_buffer_samples > 0) {
		buffer_samples = xroar_opt_ao_buffer_samples;
	} else {
		buffer_samples = (sample_rate * 23) / 1000;
	}

	int channels = 1;
	int request_fmt = SOUND_FMT_U8;
	int bytes_per_sample = 1;
	buffer_size = channels * buffer_samples * bytes_per_sample;

	WAVEFORMATEX format;
	memset(&format, 0, sizeof(format));
	format.cbSize = sizeof(format);
	format.wFormatTag = WAVE_FORMAT_PCM;
	format.nChannels = channels;
	format.nSamplesPerSec = sample_rate;
	format.nAvgBytesPerSec = buffer_size;
	format.nBlockAlign = 1;
	format.wBitsPerSample = bytes_per_sample * 8;

	if (waveOutOpen(&device, WAVE_MAPPER, &format, 0, 0, WAVE_ALLOWSYNC) != MMSYSERR_NOERROR)
		return 1;

	int i;
	for (i = 0; i < NUM_BUFFERS; i++) {
		data_alloc[i] = GlobalAlloc(GMEM_MOVEABLE | GMEM_SHARE, buffer_size);
		if (!data_alloc[i])
			return 1;
		data_p[i] = (LPSTR)GlobalLock(data_alloc[i]);

		wavehdr_alloc[i] = GlobalAlloc(GMEM_MOVEABLE | GMEM_SHARE, sizeof(WAVEHDR));
		if (!wavehdr_alloc[i])
			return 1;
		wavehdr_p[i] = (WAVEHDR *)GlobalLock(wavehdr_alloc[i]);

		wavehdr_p[i]->lpData = data_p[i];
		wavehdr_p[i]->dwBufferLength = buffer_size;
		wavehdr_p[i]->dwFlags = 0;
		wavehdr_p[i]->dwLoops = 0;

		waveOutPrepareHeader(device, wavehdr_p[i], sizeof(WAVEHDR));
	}

	sound_init(sample_rate, channels, request_fmt, buffer_samples);
	LOG_DEBUG(2, "\t%dms (%d samples) buffer\n", (buffer_samples * 1000) / sample_rate, buffer_samples);

	cursor = 0;
	buffer_num = 0;

	return 0;
}

static void _shutdown(void) {
	waveOutClose(device);
	int i;
	for (i = 0; i < NUM_BUFFERS; i++) {
		GlobalUnlock(wavehdr_alloc[i]);
		GlobalFree(wavehdr_alloc[i]);
		GlobalUnlock(data_alloc[i]);
		GlobalFree(data_alloc[i]);
	}
}

static void flush_frame(void *buffer) {
	if (xroar_noratelimit)
		return;
	memcpy(data_p[buffer_num], buffer, buffer_size);
	MMTIME mmtime;
	mmtime.wType = TIME_SAMPLES;
	MMRESULT rc = waveOutGetPosition(device, &mmtime, sizeof(mmtime));
	if (rc == MMSYSERR_NOERROR) {
		int delta = cursor - mmtime.u.sample;
		if (delta > buffer_samples*2) {
			int sleep_ms = (delta - buffer_samples*2) * 1000 / sample_rate;
			if (sleep_ms > 0) {
				Sleep(sleep_ms);
			}
		}
	}
	waveOutWrite(device, wavehdr_p[buffer_num], sizeof(WAVEHDR));
	cursor += buffer_samples;
	buffer_num = (buffer_num + 1) % NUM_BUFFERS;
	return;
}
