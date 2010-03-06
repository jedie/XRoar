/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2010  Ciaran Anscomb
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL.h>

#include "types.h"
#include "logging.h"
#include "module.h"

static int init(void);
static void shutdown(void);

extern VideoModule video_sdlgl_module;
extern VideoModule video_sdlyuv_module;
extern VideoModule video_sdl_module;
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
	NULL
};

extern KeyboardModule keyboard_sdl_module;
static KeyboardModule *sdl_keyboard_module_list[] = {
	&keyboard_sdl_module,
	NULL
};

/* Note: SDL sound and joystick modules not listed here as they can be used
 * outside of the usual SDL UI */

UIModule ui_sdl_module = {
	{ "sdl", "SDL user-interface",
	  init, 0, shutdown },
	NULL,  /* use default filereq module list */
	sdl_video_module_list,
	NULL,  /* use default sound module list */
	sdl_keyboard_module_list,
	NULL  /* use default joystick module list */
};

static int init(void) {
	return 0;
}

static void shutdown(void) {
}
