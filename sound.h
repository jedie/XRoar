/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2005  Ciaran Anscomb
 *
 *  See COPYING for redistribution conditions. */

#ifndef __SOUND_H__
#define __SOUND_H__

#include "types.h"

typedef struct SoundModule SoundModule;
struct SoundModule {
	SoundModule *next;
	const char *name;
	const char *help;
	int (*init)(void);
	void (*shutdown)(void);
	void (*reset)(void);
	void (*update)(void);
};

extern SoundModule *sound_module;

void sound_helptext(void);
void sound_getargs(int argc, char **argv);
int sound_init(void);
void sound_shutdown(void);
void sound_next(void);

#ifdef HAVE_GP32
void sound_silence(void);
#endif

#endif  /* __SOUND_H__ */
