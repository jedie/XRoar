/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2007  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef __DELTADOS_H__
#define __DELTADOS_H__

void deltados_init(void);
void deltados_reset(void);

unsigned int deltados_read(unsigned int addr);
void deltados_write(unsigned int addr, unsigned int val);

#endif  /* __DELTADOS_H__ */
