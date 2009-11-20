/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2009  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef __CART_H__
#define __CART_H__

#include "types.h"

extern uint8_t cart_data[0x4000];
extern int cart_data_writable;

void cart_init(void);
void cart_reset(void);
int cart_insert(const char *filename, int autostart);
void cart_remove(void);

#endif  /* __CART_H__ */
