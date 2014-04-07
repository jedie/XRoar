/*  Copyright 2003-2014 Ciaran Anscomb
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

#include "xalloc.h"

#include "logging.h"
#include "module.h"
#include "sound.h"
#include "xroar.h"

static _Bool init(void);
static void _shutdown(void);
static void *write_buffer(void *buffer);

SoundModule sound_windows32_module = {
	.common = { .name = "windows32", .description = "Windows audio",
		    .init = init, .shutdown = _shutdown },
	.write_buffer = write_buffer,
};

#define NUM_BUFFERS (3)

static HWAVEOUT device;
static HGLOBAL wavehdr_alloc[NUM_BUFFERS];
static LPWAVEHDR wavehdr_p[NUM_BUFFERS];
static HGLOBAL data_alloc[NUM_BUFFERS];
static LPSTR data_p[NUM_BUFFERS];
static int buffer_nframes, buffer_nbytes;
static DWORD cursor;
static int buffer_num;
static int rate;
static uint8_t *audio_buffer;

static _Bool init(void) {
	rate = (xroar_cfg.ao_rate > 0) ? xroar_cfg.ao_rate : 48000;

	if (xroar_cfg.ao_buffer_ms > 0) {
		buffer_nframes = (rate * xroar_cfg.ao_buffer_ms) / 1000;
	} else if (xroar_cfg.ao_buffer_nframes > 0) {
		buffer_nframes = xroar_cfg.ao_buffer_nframes;
	} else {
		buffer_nframes = (rate * 23) / 1000;
	}

	int nchannels = xroar_cfg.ao_channels;
	if (nchannels < 1 || nchannels > 2)
		nchannels = 2;
	int request_fmt = SOUND_FMT_U8;
	int sample_nbytes = 1;
	buffer_nbytes = nchannels * buffer_nframes * sample_nbytes;

	WAVEFORMATEX format;
	memset(&format, 0, sizeof(format));
	format.cbSize = sizeof(format);
	format.wFormatTag = WAVE_FORMAT_PCM;
	format.nChannels = nchannels;
	format.nSamplesPerSec = rate;
	format.nAvgBytesPerSec = rate * nchannels * sample_nbytes;
	format.nBlockAlign = 1;
	format.wBitsPerSample = sample_nbytes * 8;

	if (waveOutOpen(&device, WAVE_MAPPER, &format, 0, 0, WAVE_ALLOWSYNC) != MMSYSERR_NOERROR)
		return 0;

	for (unsigned i = 0; i < NUM_BUFFERS; i++) {
		data_alloc[i] = GlobalAlloc(GMEM_MOVEABLE | GMEM_SHARE, buffer_nbytes);
		if (!data_alloc[i])
			return 0;
		data_p[i] = (LPSTR)GlobalLock(data_alloc[i]);

		wavehdr_alloc[i] = GlobalAlloc(GMEM_MOVEABLE | GMEM_SHARE, sizeof(WAVEHDR));
		if (!wavehdr_alloc[i])
			return 0;
		wavehdr_p[i] = (WAVEHDR *)GlobalLock(wavehdr_alloc[i]);

		wavehdr_p[i]->lpData = data_p[i];
		wavehdr_p[i]->dwBufferLength = buffer_nbytes;
		wavehdr_p[i]->dwFlags = 0;
		wavehdr_p[i]->dwLoops = 0;

		waveOutPrepareHeader(device, wavehdr_p[i], sizeof(WAVEHDR));
	}

	audio_buffer = xmalloc(buffer_nbytes);
	sound_init(audio_buffer, request_fmt, rate, nchannels, buffer_nframes);
	LOG_DEBUG(1, "\t%dms (%d samples) buffer\n", (buffer_nframes * 1000) / rate, buffer_nframes);

	cursor = 0;
	buffer_num = 0;

	return 1;
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
	free(audio_buffer);
}

static void *write_buffer(void *buffer) {
	if (xroar_noratelimit)
		return buffer;
	memcpy(data_p[buffer_num], buffer, buffer_nbytes);
	MMTIME mmtime;
	mmtime.wType = TIME_SAMPLES;
	MMRESULT rc = waveOutGetPosition(device, &mmtime, sizeof(mmtime));
	if (rc == MMSYSERR_NOERROR) {
		int delta = cursor - mmtime.u.sample;
		if (delta > buffer_nframes*2) {
			int sleep_ms = (delta - buffer_nframes*2) * 1000 / rate;
			if (sleep_ms > 0) {
				Sleep(sleep_ms);
			}
		}
	}
	waveOutWrite(device, wavehdr_p[buffer_num], sizeof(WAVEHDR));
	cursor += buffer_nframes;
	buffer_num = (buffer_num + 1) % NUM_BUFFERS;
	return buffer;
}
