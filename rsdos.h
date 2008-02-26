/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2007  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef __RSDOS_H__
#define __RSDOS_H__

void rsdos_init(void);
void rsdos_reset(void);

unsigned int rsdos_read(unsigned int addr);
void rsdos_write(unsigned int addr, unsigned int val);

#endif  /* __RSDOS_H__ */
