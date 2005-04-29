/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2005  Ciaran Anscomb
 *
 *  See COPYING for redistribution conditions. */

#ifndef __XROAR_H__
#define __XROAR_H__

#include "types.h"

#define RESET_SOFT 0
#define RESET_HARD 1

extern Cycle current_cycle;
extern Cycle next_sound_update, next_disk_interrupt;
extern int_fast8_t enable_disk_interrupt;

#ifdef TRACE
extern int trace;
#else
#define trace 0
#endif

void xroar_init(int argc, char **argv);
void xroar_shutdown(void);
void xroar_reset(int hard);
void xroar_mainloop(void);

#endif  /* __XROAR_H__ */
