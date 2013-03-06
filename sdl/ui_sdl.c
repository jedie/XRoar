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
#include "sdl/common.h"
#include "vdg.h"
#include "xroar.h"

/* Note: prefer the default order for sound and joystick modules, which
 * will include the SDL options. */

UIModule ui_sdl_module = {
	.common = { .name = "sdl", .description = "SDL UI" },
	.video_module_list = sdl_video_module_list,
	.keyboard_module_list = sdl_keyboard_module_list,
	.joystick_module_list = sdl_js_modlist,
	.run = sdl_run,
};
