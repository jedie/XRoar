/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2013  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef XROAR_SOUND_H_
#define XROAR_SOUND_H_

enum sound_fmt {
	SOUND_FMT_NULL,
	SOUND_FMT_U8,
	SOUND_FMT_S8,
	SOUND_FMT_S16_BE,
	SOUND_FMT_S16_LE,
	SOUND_FMT_S16_HE,  // host-endian
	SOUND_FMT_S16_SE,  // swapped-endian
	SOUND_FMT_FLOAT,
};

void sound_init(void *buf, enum sound_fmt fmt, unsigned rate, unsigned nchannels, unsigned nframes);
void sound_set_volume(int v);
void sound_update(void);
void sound_silence(void);
void sound_render_silence(void *buf, int samples);

#endif  /* XROAR_SOUND_H_ */
