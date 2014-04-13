/*  Copyright 2003-2014 Ciaran Anscomb
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
#include <SDL_syswm.h>

#include "events.h"
#include "keyboard.h"
#include "logging.h"
#include "machine.h"
#include "mc6847.h"
#include "module.h"
#include "sam.h"
#include "xroar.h"

#include "sdl/common.h"

#ifdef WINDOWS32
#include "windows32/common_windows32.h"
#endif

/* Note: prefer the default order for sound and joystick modules, which
 * will include the SDL options. */

static _Bool init(void);
static void shutdown(void);

UIModule ui_sdl_module = {
	.common = { .name = "sdl", .description = "SDL UI",
	            .init = init, .shutdown = shutdown },
	.video_module_list = sdl_video_module_list,
	.keyboard_module_list = sdl_keyboard_module_list,
	.joystick_module_list = sdl_js_modlist,
	.run = sdl_run,
};

static _Bool init(void) {
#ifdef WINDOWS32
	if (!getenv("SDL_VIDEODRIVER"))
		putenv("SDL_VIDEODRIVER=windib");
#endif

	if (!SDL_WasInit(SDL_INIT_NOPARACHUTE)) {
		if (SDL_Init(SDL_INIT_NOPARACHUTE) < 0) {
			LOG_ERROR("Failed to initialise SDL: %s\n", SDL_GetError());
			return 0;
		}
	}

	if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0) {
		LOG_ERROR("Failed to initialise SDL video: %s\n", SDL_GetError());
		return 0;
	}

#ifdef WINDOWS32
	{
		SDL_version sdlver;
		SDL_SysWMinfo sdlinfo;
		SDL_VERSION(&sdlver);
		sdlinfo.version = sdlver;
		SDL_GetWMInfo(&sdlinfo);
		windows32_main_hwnd = sdlinfo.window;
	}
#endif

	return 1;
}

static void shutdown(void) {
	SDL_QuitSubSystem(SDL_INIT_VIDEO);
}
