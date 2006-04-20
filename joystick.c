/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2006  Ciaran Anscomb
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "pia.h"
#include "joystick.h"
#include "logging.h"
#include "types.h"

#ifdef HAVE_SDL
extern JoystickModule joystick_sdl_module;
#endif

JoystickModule *joystick_module;

unsigned int joystick_leftx, joystick_lefty;
unsigned int joystick_rightx, joystick_righty;

int joystick_init(void) {
	if (joystick_module == NULL)
		return 1;
	return joystick_module->init();
}

void joystick_shutdown(void) {
	if (joystick_module)
		joystick_module->shutdown();
}

void joystick_reset(void) {
	joystick_leftx = joystick_lefty = 128;
	joystick_rightx = joystick_righty = 128;
}

void joystick_update(void) {
	int xcompare, ycompare;
	unsigned int octet = PIA_1A.port_output & 0xfc;
	if (PIA_0B.control_register & 0x08) {
		xcompare = (joystick_leftx >= octet);
		ycompare = (joystick_lefty >= octet);
	} else {
		xcompare = (joystick_rightx >= octet);
		ycompare = (joystick_righty >= octet);
	}
	if (PIA_0A.control_register & 0x08) {
		if (ycompare) {
			PIA_0A.port_input |= 0x80;
		} else {
			PIA_0A.port_input &= 0x7f;
		}
	} else {
		if (xcompare) {
			PIA_0A.port_input |= 0x80;
		} else {
			PIA_0A.port_input &= 0x7f;
		}
	}
}
