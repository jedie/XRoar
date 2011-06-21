/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2011  Ciaran Anscomb
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "types.h"
#include "logging.h"
#include "machine.h"
#include "mc6821.h"
#include "module.h"
#include "joystick.h"

/* Some dissonance: internally, the right joystick is considered "first", but
 * any interface presented to the user should probably consider the left
 * joystick to be "first". */

int joystick_axis[4];

void joystick_init(void) {
	joystick_axis[0] = joystick_axis[1]
		= joystick_axis[2] = joystick_axis[3] = 127;
}

void joystick_shutdown(void) {
}

void joystick_update(void) {
	int axis = ((PIA0.b.control_register & 0x08) >> 2)
		| ((PIA0.a.control_register & 0x08) >> 3);
	int dac_value = PIA1.a.port_output & 0xfc;
	if (joystick_axis[axis] >= dac_value) {
		PIA0.a.port_input |= 0x80;
	} else {
		PIA0.a.port_input &= 0x7f;
	}
}
