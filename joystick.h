/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2008  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef __JOYSTICK_H__
#define __JOYSTICK_H__

extern int joystick_axis[4];

void joystick_init(void);
void joystick_shutdown(void);
void joystick_update(void);

#endif  /* __JOYSTICK_H__ */
