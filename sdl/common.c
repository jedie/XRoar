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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL.h>

#include "config.h"

#include "events.h"
#include "logging.h"
#include "machine.h"
#include "module.h"
#include "sdl/common.h"
#include "vdg.h"
#include "xroar.h"

VideoModule *sdl_video_module_list[] = {
#ifdef HAVE_SDLGL
	&video_sdlgl_module,
#endif
#ifdef PREFER_NOYUV
	&video_sdl_module,
	&video_sdlyuv_module,
#else
	&video_sdlyuv_module,
	&video_sdl_module,
#endif
	&video_null_module,
	NULL
};

KeyboardModule *sdl_keyboard_module_list[] = {
	&keyboard_sdl_module,
	NULL
};

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

static void sdl_js_shutdown(void);

// If the SDL UI is active, more joystick interfaces are available

static struct joystick_interface *js_iflist[] = {
	&sdl_js_if_physical,
	&sdl_js_if_keyboard,
	NULL
};

JoystickModule sdl_js_internal = {
	.common = { .name = "sdl", .description = "SDL joystick input",
	            .shutdown = sdl_js_shutdown },
	.interface_list = js_iflist,
};

JoystickModule *sdl_js_modlist[] = {
	&sdl_js_internal,
	NULL
};

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void sdl_js_shutdown(void) {
	sdl_js_physical_shutdown();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void sdl_run(void) {
	while (1) {
		SDL_Event event;
		machine_run(VDG_LINE_DURATION * 16);
		while (SDL_PollEvent(&event) == 1) {
			switch(event.type) {
			case SDL_VIDEORESIZE:
				if (video_module->resize) {
					video_module->resize(event.resize.w, event.resize.h);
				}
				break;
			case SDL_QUIT:
				xroar_quit();
				break;
			case SDL_KEYDOWN:
				sdl_keypress(&event.key.keysym);
				break;
			case SDL_KEYUP:
				sdl_keyrelease(&event.key.keysym);
				break;
			default:
				break;
			}
		}
		/* XXX will this ever be needed? */
		event_run_queue(UI_EVENT_LIST);
	}
}
