/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2013  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef XROAR_SOUND_H_
#define XROAR_SOUND_H_

#include <inttypes.h>

#define SOUND_FMT_NULL (-1)
#define SOUND_FMT_U8 (0)
#define SOUND_FMT_S8 (1)
#define SOUND_FMT_S16_BE (2)
#define SOUND_FMT_S16_LE (4)
#define SOUND_FMT_S16_HE (6)  /* host-endian */
#define SOUND_FMT_S16_SE (8)  /* swapped-endian */
#define SOUND_FMT_FLOAT (10)

void *sound_init(int sample_rate, int channels, int fmt, int frame_size);
void sound_set_volume(int v);
void sound_update(void);
void sound_silence(void);
void sound_render_silence(void *buf, int samples);

#endif  /* XROAR_SOUND_H_ */
