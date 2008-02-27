/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2008  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef __CART_H__
#define __CART_H__

void cart_helptext(void);
void cart_getargs(int argc, char **argv);
void cart_init(void);
void cart_reset(void);
void cart_insert(char *filename, int autostart);
void cart_remove(void);

#endif  /* __CART_H__ */
