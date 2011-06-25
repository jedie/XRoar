/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2011  Ciaran Anscomb
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA  02110-1301, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL.h>

#include "types.h"
#include "keyboard.h"
#include "logging.h"
#include "machine.h"
#include "module.h"
#include "sam.h"
#include "vdg.h"
#include "xroar.h"

void sdl_run(void);

extern VideoModule video_sdlgl_module;
extern VideoModule video_sdlyuv_module;
extern VideoModule video_sdl_module;
extern VideoModule video_null_module;
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

extern KeyboardModule keyboard_sdl_module;
static KeyboardModule *sdl_keyboard_module_list[] = {
	&keyboard_sdl_module,
	NULL
};

/* Note: prefer the default order for sound and joystick modules, which
 * will include the SDL options. */

UIModule ui_sdl_module = {
	.common = { .name = "sdl", .description = "SDL user-interface" },
	.video_module_list = sdl_video_module_list,
	.keyboard_module_list = sdl_keyboard_module_list,
	.run = sdl_run,
};

void sdl_keypress(SDL_keysym *keysym);
void sdl_keyrelease(SDL_keysym *keysym);

void sdl_run(void) {
	while (1) {
		SDL_Event event;
		sam_run(VDG_LINE_DURATION * 8);
		while (SDL_PollEvent(&event) == 1) {
			switch(event.type) {
			case SDL_VIDEORESIZE:
				if (video_module->resize) {
					video_module->resize(event.resize.w, event.resize.h);
				}
				break;
			case SDL_QUIT:
				exit(0); break;
			case SDL_KEYDOWN:
				sdl_keypress(&event.key.keysym);
				keyboard_column_update();
				keyboard_row_update();
				break;
			case SDL_KEYUP:
				sdl_keyrelease(&event.key.keysym);
				keyboard_column_update();
				keyboard_row_update();
				break;
			default:
				break;
			}
		}
		/* XXX will this ever be needed? */
		while (EVENT_PENDING(UI_EVENT_LIST))
			DISPATCH_NEXT_EVENT(UI_EVENT_LIST);
	}
}
