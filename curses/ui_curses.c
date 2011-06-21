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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curses.h>

#include "types.h"
#include "logging.h"
#include "module.h"

static int init(void);
static void shutdown(void);

extern VideoModule video_curses_module;
extern VideoModule video_null_module;
static VideoModule *curses_video_module_list[] = {
	&video_curses_module,
	&video_null_module,
	NULL
};

extern KeyboardModule keyboard_curses_module;
static KeyboardModule *curses_keyboard_module_list[] = {
	&keyboard_curses_module,
	NULL
};

UIModule ui_curses_module = {
	.common = { .name = "curses", .description = "Curses user-interface",
	            .init = init, .shutdown = shutdown },
	.video_module_list = curses_video_module_list,
	.keyboard_module_list = curses_keyboard_module_list,
};

static int init(void) {
	initscr();
	start_color();
	nodelay(stdscr, TRUE);
	keypad(stdscr, TRUE);
	nonl();
	raw();
	noecho();
	curs_set(0);
	return 0;
}

static void shutdown(void) {
	curs_set(1);
	endwin();
}
