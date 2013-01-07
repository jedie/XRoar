/*  Copyright 2003-2013 Ciaran Anscomb
 *
 *  This file is part of XRoar.
 *
 *  XRoar is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  XRoar is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XRoar.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "joystick.h"
#include "logging.h"
#include "machine.h"
#include "mc6821.h"

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
	int dac_value = PIA1.a.out_sink & 0xfc;
	if (joystick_axis[axis] >= dac_value) {
		PIA0.a.in_sink |= 0x80;
	} else {
		PIA0.a.in_sink &= 0x7f;
	}
}
