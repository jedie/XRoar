/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2010  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef XROAR_TAPE_H_
#define XROAR_TAPE_H_

#include "types.h"

void tape_init(void);
void tape_reset(void);
void tape_shutdown(void);

int tape_open_reading(const char *filename);
void tape_close_reading(void);
int tape_open_writing(const char *filename);
void tape_close_writing(void);

int tape_autorun(const char *filename);

void tape_update_motor(void);
void tape_update_output(void);
void tape_update_input(void);

/* Tapehack specific stuff: */
void tape_sync(void);
void tape_desync(int leader);
void tape_bit_out(int value);

#endif  /* XROAR_TAPE_H_ */
