/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2007  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef __DRAGONDOS_H__
#define __DRAGONDOS_H__

void dragondos_init(void);
void dragondos_reset(void);

unsigned int dragondos_read(unsigned int addr);
void dragondos_write(unsigned int addr, unsigned int val);

#endif  /* __DRAGONDOS_H__ */
