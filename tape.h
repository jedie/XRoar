/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2007  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef __TAPE_H__
#define __TAPE_H__

#include "types.h"

void tape_init(void);
void tape_reset(void);
void tape_shutdown(void);

int tape_open_reading(char *filename);
void tape_close_reading(void);
int tape_open_writing(char *filename);
void tape_close_writing(void);

int tape_autorun(char *filename);

void tape_update_motor(void);
void tape_update_output(void);
void tape_update_input(void);

#endif  /* __TAPE_H__ */
