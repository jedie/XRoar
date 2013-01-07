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

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL.h>

#include "events.h"
#include "keyboard.h"
#include "logging.h"
#include "machine.h"
#include "module.h"
#include "sam.h"
#include "sdl/ui_sdl.h"
#include "vdg.h"
#include "xroar.h"

static VideoModule *sdl_video_module_list[] = {
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

static KeyboardModule *sdl_keyboard_module_list[] = {
	&keyboard_sdl_module,
	NULL
};

/* Note: prefer the default order for sound and joystick modules, which
 * will include the SDL options. */

UIModule ui_sdl_module = {
	.common = { .name = "sdl", .description = "SDL UI" },
	.video_module_list = sdl_video_module_list,
	.keyboard_module_list = sdl_keyboard_module_list,
	.run = sdl_run,
};

void sdl_run(void) {
	while (1) {
		SDL_Event event;
		machine_run(VDG_LINE_DURATION * 8);
		while (SDL_PollEvent(&event) == 1) {
			switch(event.type) {
			case SDL_VIDEORESIZE:
				if (video_module->resize) {
					video_module->resize(event.resize.w, event.resize.h);
				}
				break;
			case SDL_QUIT:
				exit(EXIT_SUCCESS);
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
