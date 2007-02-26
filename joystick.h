/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2007  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef __JOYSTICK_H__
#define __JOYSTICK_H__

extern unsigned int joystick_leftx, joystick_lefty;
extern unsigned int joystick_rightx, joystick_righty;

void joystick_init(void);
void joystick_shutdown(void);
void joystick_reset(void);
void joystick_poll(void);
void joystick_update(void);

#endif  /* __JOYSTICK_H__ */
