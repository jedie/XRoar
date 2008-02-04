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
static void helptext(void);

JoystickModule joystick_sdl_module = {
	{ "sdl", "SDL joystick driver",
	  init, 0, shutdown, helptext }
};

static struct joy {
	int joy_num;
	SDL_Joystick *device;
	int num_axes;
	int num_buttons;
} joy[6];
static int num_joys = 0;

#define LEFTX     (0)
#define LEFTY     (1)
#define LEFTFIRE  (2)
#define RIGHTX    (3)
#define RIGHTY    (4)
#define RIGHTFIRE (5)

static struct {
	struct joy *joy;
	int control_num;
	int invert;
} control[6];

static struct {
	int joy_num;
	int control_num;
	int invert;
} control_config[6] = {
	{ 0, 0, 0 },
	{ 0, 1, 0 },
	{ 0, 0, 0 },
	{ 1, 0, 0 },
	{ 1, 1, 0 },
	{ 1, 0, 0 }
};

static event_t *poll_event;
static void do_poll(void *context);

static struct joy *find_joy(int joy_num) {
	SDL_Joystick *j;
	int i;
	if (joy_num >= SDL_NumJoysticks())
		return NULL;
	for (i = 0; i < num_joys; i++) {
		if (joy[i].joy_num == joy_num)
			return &joy[i];
	}
	i = num_joys;
	num_joys++;
	j = joy[i].device = SDL_JoystickOpen(joy_num);
	if (j == NULL)
		return NULL;
	joy[i].joy_num = joy_num;
	joy[i].num_axes = SDL_JoystickNumAxes(j);
	joy[i].num_buttons = SDL_JoystickNumButtons(j);
	LOG_DEBUG(1,"\t%s\n", SDL_JoystickName(joy_num));
	LOG_DEBUG(2,"\tNumber of Axes: %d\n", joy[i].num_axes);
	LOG_DEBUG(2,"\tNumber of Buttons: %d\n", joy[i].num_buttons);
	return &joy[i];
}

static void helptext(void) {
	puts("  -joy-left [XJ,][-]XA:[YJ,][-]YA:[FJ,]FB       [0,0:1:0]");
	puts("  -joy-right [XJ,][-]XA:[YJ,][-]YA:[FJ,]FB      [1,0:1:0]");
	puts("                        J = joystick number, A = axis number, B = button number");
	puts("                        a '-' before axis signifies inverted axis");
}

static void parse_joystick_def(char *def, int base) {
	int joy_num = control_config[base].joy_num;
	int i;
	if (def == NULL)
		return;
	for (i = 0; i < 3; i++) {
		char *tmp2 = strsep(&def, ":");
		char *tmp1 = strsep(&tmp2, ",");
		int control_num = i % 2;
		int invert = 0;
		if (tmp2) {
			joy_num = atoi(tmp1);
			if (*tmp2) {
				if (*tmp2 == '-') {
					invert = 0xff;
					tmp2++;
				}
				if (*tmp2) {
					control_num = atoi(tmp2);
				}
			}
		} else if (tmp1) {
			if (*tmp1 == '-') {
				invert = 0xff;
				tmp1++;
			}
			if (*tmp1) {
				control_num = atoi(tmp1);
			}
		}
		control_config[base+i].joy_num = joy_num;
		control_config[base+i].control_num = control_num;
		control_config[base+i].invert = invert;
	}
}

static int init(int argc, char **argv) {
	int valid, i;
	(void)argc;
	(void)argv;

	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-joy-left") && i+1<argc) {
			i++;
			parse_joystick_def(argv[i], 0);
		} else if (!strcmp(argv[i], "-joy-right") && i+1<argc) {
			i++;
			parse_joystick_def(argv[i], 3);
		}
	}

	poll_event = event_new();
	if (poll_event == NULL) {
		LOG_WARN("Couldn't create joystick polling event.\n");
		return 1;
	}
	poll_event->dispatch = do_poll;
	LOG_DEBUG(2,"Initialising SDL joystick driver\n");
	SDL_InitSubSystem(SDL_INIT_JOYSTICK);

	valid = 0;
	for (i = 0; i < 6; i++) {
		struct joy *j = find_joy(control_config[i].joy_num);
		control[i].joy = j;
		control[i].control_num = control_config[i].control_num;
		control[i].invert = control_config[i].invert;
		if (j && (i % 3) < 2) {
			if (control_config[i].control_num >= j->num_axes) {
				LOG_WARN("Axis %d not found on joystick %d\n", control_config[i].control_num, control_config[i].joy_num);
				control[i].joy = NULL;
			}
		} else if (j && (i % 3) == 2) {
			if (control_config[i].control_num >= j->num_buttons) {
				LOG_WARN("Button %d not found on joystick %d\n", control_config[i].control_num, control_config[i].joy_num);
				control[i].joy = NULL;
			}
		} else {
			LOG_WARN("Joystick %d not found\n", control_config[i].joy_num);
			control[i].joy = NULL;
		}
		if (control[i].joy)
			valid++;
	}

	/* No point scheduling joystick reads if we don't have any */
	if (valid) {
		poll_event->at_cycle = current_cycle + (OSCILLATOR_RATE / 100);
		event_queue(poll_event); 
	} else {
		LOG_WARN("No valid joystick mappings made.\n");
	}
	return 0;
}

static void shutdown(void) {
	int i;
	LOG_DEBUG(2,"Shutting down SDL joystick driver\n");
	for (i = 0; i < num_joys; i++) {
		SDL_JoystickClose(joy[i].device);
	}
	num_joys = 0;
	SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
}

static void do_poll(void *context) {
	(void)context;
	SDL_JoystickUpdate();
	if (control[LEFTX].joy) {
		joystick_leftx = ((SDL_JoystickGetAxis(control[LEFTX].joy->device, control[LEFTX].control_num)+32768) >> 8) ^ control[LEFTX].invert;
	}
	if (control[LEFTY].joy) {
		joystick_lefty = ((SDL_JoystickGetAxis(control[LEFTY].joy->device, control[LEFTY].control_num)+32768) >> 8) ^ control[LEFTY].invert;
	}
	if (control[RIGHTX].joy) {
		joystick_rightx = ((SDL_JoystickGetAxis(control[RIGHTX].joy->device, control[RIGHTX].control_num)+32768) >> 8) ^ control[RIGHTX].invert;
	}
	if (control[RIGHTY].joy) {
		joystick_righty = ((SDL_JoystickGetAxis(control[RIGHTY].joy->device, control[RIGHTY].control_num)+32768) >> 8) ^ control[RIGHTY].invert;
	}
	if (control[RIGHTFIRE].joy && SDL_JoystickGetButton(control[RIGHTFIRE].joy->device, control[RIGHTFIRE].control_num)) {
		PIA_0A.tied_low &= 0xfe;
	} else {
		PIA_0A.tied_low |= 0x01;
	}
	if (control[LEFTFIRE].joy && SDL_JoystickGetButton(control[LEFTFIRE].joy->device, control[LEFTFIRE].control_num)) {
		PIA_0A.tied_low &= 0xfd;
	} else {
		PIA_0A.tied_low |= 0x02;
	}
	joystick_update();
	poll_event->at_cycle += OSCILLATOR_RATE / 100;
	event_queue(poll_event);
}
