/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2004  Ciaran Anscomb
 *
 *  See COPYING for redistribution conditions. */

#ifndef __JOYSTICK_H__
#define __JOYSTICK_H__

#include "types.h"

typedef struct JoystickModule JoystickModule;
struct JoystickModule {
	JoystickModule *next;
	char *name;
	char *help;
	int (*init)(void);
	void (*shutdown)(void);
	void (*poll)(void);
};

/* Video & sound modules will probably have a joystick module preference,
 * so will set this: */
extern JoystickModule *joystick_module;

extern uint_fast8_t joystick_leftx, joystick_lefty;
extern uint_fast8_t joystick_rightx, joystick_righty;

int joystick_init(void);
void joystick_shutdown(void);
void joystick_reset(void);
void joystick_poll(void);
void joystick_update(void);

#endif  /* __JOYSTICK_H__ */
