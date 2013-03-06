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

#include "input.h"
#include "joystick.h"
#include "keyboard.h"
#include "machine.h"
#include "mc6821.h"

void input_control_press(int command, unsigned int arg) {
	switch (command) {
		case INPUT_KEY:
			KEYBOARD_PRESS(arg);
			break;
		case INPUT_UNICODE_KEY:
			keyboard_unicode_press(arg);
			break;
		default:
			break;
	}
}

void input_control_release(int command, unsigned int arg) {
	switch (command) {
		case INPUT_KEY:
			KEYBOARD_RELEASE(arg);
			break;
		case INPUT_UNICODE_KEY:
			keyboard_unicode_release(arg);
			break;
		default:
			break;
	}
}
