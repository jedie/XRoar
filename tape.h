/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2004  Ciaran Anscomb
 *
 *  See COPYING for redistribution conditions. */

#ifndef __TAPE_H__
#define __TAPE_H__

#include "types.h"

void tape_init(void);
void tape_reset(void);
int tape_attach(char *filename);
void tape_detach(void);
int tape_autorun(char *filename);
uint_fast8_t tape_read_bit(void);

#endif  /* __TAPE_H__ */
