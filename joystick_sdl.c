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

#include <SDL.h>

#include "types.h"
#include "joystick.h"
#include "pia.h"
#include "logging.h"

static int init(void);
static void shutdown(void);
static void poll(void);

JoystickModule joystick_sdl_module = {
	NULL,
	"sdl",
	"SDL joystick driver",
	init, shutdown, poll
};

static SDL_Joystick *joy = NULL;

static int init(void) {
	LOG_DEBUG(2,"Initialising SDL joystick driver\n");
	SDL_InitSubSystem(SDL_INIT_JOYSTICK);
	if (SDL_NumJoysticks() > 0) {
		LOG_DEBUG(2,"\tFound %d joysticks, using first\n", SDL_NumJoysticks());
		joy = SDL_JoystickOpen(0);
		if (joy) {
			LOG_DEBUG(1,"\t%s\n", SDL_JoystickName(0));
			LOG_DEBUG(2,"\tNumber of Axes: %d\n", SDL_JoystickNumAxes(joy));
			LOG_DEBUG(2,"\tNumber of Buttons: %d\n", SDL_JoystickNumButtons(joy));
			LOG_DEBUG(2,"\tNumber of Balls: %d\n", SDL_JoystickNumBalls(joy));
			return 0;
		}
	}
	return 1;
}

static void shutdown(void) {
	LOG_DEBUG(2,"Shutting down SDL joystick driver\n");
	if (joy) {
		SDL_JoystickClose(joy);
		joy = NULL;
	}
	SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
}

static void poll(void) {
	if (joy == NULL)
		return;
	SDL_JoystickUpdate();
	joystick_leftx = (SDL_JoystickGetAxis(joy, 3)+32768) >> 8;
	joystick_lefty = (SDL_JoystickGetAxis(joy, 2)+32768) >> 8;
	joystick_rightx = (SDL_JoystickGetAxis(joy, 0)+32768) >> 8;
	joystick_righty = (SDL_JoystickGetAxis(joy, 1)+32768) >> 8;
	if (SDL_JoystickGetButton(joy, 1)) {
		PIA_0A.tied_low &= 0xfe;
	} else {
		PIA_0A.tied_low |= 0x01;
	}
	if (SDL_JoystickGetButton(joy, 0)) {
		PIA_0A.tied_low &= 0xfd;
	} else {
		PIA_0A.tied_low |= 0x02;
	}
}
