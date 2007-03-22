/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2007  Ciaran Anscomb
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
#include "events.h"
#include "joystick.h"
#include "logging.h"
#include "machine.h"
#include "module.h"
#include "pia.h"

static int init(int argc, char **argv);
static void shutdown(void);

JoystickModule joystick_sdl_module = {
	{ "sdl", "SDL joystick driver",
	  init, 0, shutdown, NULL }
};

static SDL_Joystick *joy = NULL;

static event_t *poll_event;
static void do_poll(void *context);

static int init(int argc, char **argv) {
	(void)argc;
	(void)argv;
	poll_event = event_new();
	if (poll_event == NULL) {
		LOG_WARN("Couldn't create joystick polling event.\n");
		return 1;
	}
	poll_event->dispatch = do_poll;
	LOG_DEBUG(2,"Initialising SDL joystick driver\n");
	SDL_InitSubSystem(SDL_INIT_JOYSTICK);
	if (SDL_NumJoysticks() <= 0)
		return 1;
	LOG_DEBUG(2,"\tFound %d joysticks, using first\n", SDL_NumJoysticks());
	joy = SDL_JoystickOpen(0);
	if (joy == NULL)
		return 1;
	LOG_DEBUG(1,"\t%s\n", SDL_JoystickName(0));
	LOG_DEBUG(2,"\tNumber of Axes: %d\n", SDL_JoystickNumAxes(joy));
	LOG_DEBUG(2,"\tNumber of Buttons: %d\n", SDL_JoystickNumButtons(joy));
	LOG_DEBUG(2,"\tNumber of Balls: %d\n", SDL_JoystickNumBalls(joy));
	poll_event->at_cycle = current_cycle + (OSCILLATOR_RATE / 100);
	event_queue(poll_event); 
	return 0;
}

static void shutdown(void) {
	LOG_DEBUG(2,"Shutting down SDL joystick driver\n");
	SDL_JoystickClose(joy);
	joy = NULL;
	SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
}

static void do_poll(void *context) {
	(void)context;
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
	joystick_update();
	poll_event->at_cycle += OSCILLATOR_RATE / 100;
	event_queue(poll_event);
}
