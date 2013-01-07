/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2013  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef XROAR_JOYSTICK_H_
#define XROAR_JOYSTICK_H_

extern int joystick_axis[4];

void joystick_init(void);
void joystick_shutdown(void);
void joystick_update(void);

#endif  /* XROAR_JOYSTICK_H_ */
