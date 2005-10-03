/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2005  Ciaran Anscomb
 *
 *  See COPYING for redistribution conditions. */

#ifndef __VDG_H__
#define __VDG_H__

#include "types.h"

extern unsigned int vdg_alpha[768];
extern Cycle scanline_start;
extern int beam_pos;

void vdg_init(void);
void vdg_reset(void);
void vdg_set_mode(void);

#endif  /* __VDG_H__ */
