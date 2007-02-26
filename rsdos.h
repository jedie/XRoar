/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2007  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef __RSDOS_H__
#define __RSDOS_H__

void rsdos_init(void);
void rsdos_reset(void);
void rsdos_ff40_write(unsigned int octet);

#endif  /* __RSDOS_H__ */
